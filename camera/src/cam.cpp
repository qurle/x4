#include "cam.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "cam_pins.h"
#include "x4/log.h"

namespace x4cam {
namespace cam {
namespace {

SemaphoreHandle_t g_mu = nullptr;  // sensor + frame buffers
Identity g_id;
int32_t g_curRes = -1;
volatile uint16_t g_switchMs = 0;  // slowest switch in the current window (decays on reboot only)
volatile x4::proto::CamState g_state = x4::proto::CamState::Booting;

// Held shot
camera_fb_t* g_held = nullptr;
uint32_t g_heldShot = 0;
uint32_t g_heldAtMs = 0;
SemaphoreHandle_t g_heldMu = nullptr;

struct Lock {
  Lock() { xSemaphoreTake(g_mu, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(g_mu); }
};

int64_t fbStartUs(const camera_fb_t* fb) {
  return (int64_t)fb->timestamp.tv_sec * 1000000LL + fb->timestamp.tv_usec;
}

bool jpegValid(const camera_fb_t* fb) {
  return fb && fb->format == PIXFORMAT_JPEG && fb->len > 4 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8 &&
         fb->buf[fb->len - 2] == 0xFF && fb->buf[fb->len - 1] == 0xD9;
}

// Must hold g_mu.
bool setResLocked(int32_t res) {
  if (res == g_curRes) return true;
  sensor_t* s = esp_camera_sensor_get();
  if (!s) return false;
  uint32_t t0 = millis();
  bool ok = s->set_framesize(s, frameSizeFor(res)) == 0;
  // The first frame after a size change can be partial; drop one.
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
  if (ok) g_curRes = res;
  uint32_t ms = millis() - t0;
  if (ms > g_switchMs) g_switchMs = (uint16_t)(ms > 65535 ? 65535 : ms);
  X4_LOGI("cam", "res", "res=%s ok=%d switch_ms=%lu", resName(res), ok, (unsigned long)ms);
  return ok;
}

uint8_t readReg16(uint8_t addr, uint16_t reg) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return 0;
  return Wire.read();
}

}  // namespace

framesize_t frameSizeFor(int32_t res) {
  switch (res) {
    case 0: return FRAMESIZE_QVGA;
    case 1: return FRAMESIZE_VGA;
    case 2: return FRAMESIZE_HD;
    case 3: return FRAMESIZE_UXGA;
    case 4: return FRAMESIZE_FHD;
    default: return FRAMESIZE_5MP;
  }
}

const char* resName(int32_t res) {
  static const char* n[] = {"qvga", "vga", "hd", "uxga", "fhd", "5mp"};
  return n[res < 0 || res > 5 ? 5 : res];
}

uint8_t probeHwRev() {
  // M5 "Unit CamS3-5MP Arduino" guide: read 16-bit register 0x0200 at SCCB 0x1F.
  Wire.begin(pins::kSiod, pins::kSioc, 100000);
  uint8_t v = readReg16(0x1F, 0x0200);
  g_id.fwVer = readReg16(0x1F, 0x0002);
  Wire.end();
  g_id.hwRev = v;
  return v;
}

bool begin(const x4::cfg::Config& c) {
  g_mu = xSemaphoreCreateMutex();
  g_heldMu = xSemaphoreCreateMutex();
  camera_config_t cc = {};
  cc.pin_pwdn = pins::kPwdn;
  cc.pin_reset = pins::kReset;
  cc.pin_xclk = pins::kXclk;
  cc.pin_sccb_sda = pins::kSiod;
  cc.pin_sccb_scl = pins::kSioc;
  cc.pin_d7 = pins::kY9;
  cc.pin_d6 = pins::kY8;
  cc.pin_d5 = pins::kY7;
  cc.pin_d4 = pins::kY6;
  cc.pin_d3 = pins::kY5;
  cc.pin_d2 = pins::kY4;
  cc.pin_d1 = pins::kY3;
  cc.pin_d0 = pins::kY2;
  cc.pin_vsync = pins::kVsync;
  cc.pin_href = pins::kHref;
  cc.pin_pclk = pins::kPclk;
  cc.xclk_freq_hz = c.cam_xclk_mhz * 1000000;
  cc.ledc_timer = LEDC_TIMER_0;
  cc.ledc_channel = LEDC_CHANNEL_0;
  cc.pixel_format = PIXFORMAT_JPEG;
  cc.frame_size = frameSizeFor(c.cam_res);
  cc.jpeg_quality = c.cam_quality;
  cc.fb_count = (size_t)c.cam_fb_count;
  cc.fb_location = CAMERA_FB_IN_PSRAM;
  cc.grab_mode = CAMERA_GRAB_LATEST;
  cc.jpeg_buffer_size = (size_t)c.cam_jpeg_buf_kb * 1024;
  uint32_t t0 = millis();
  esp_err_t err = esp_camera_init(&cc);
  g_id.initErr = err;
  g_id.initOk = err == ESP_OK;
  if (!g_id.initOk) {
    X4_LOG_ERR(Error, "cam", "init", x4::Err::CAM_INIT_FAIL, "esp_err=0x%x", (unsigned)err);
    g_state = x4::proto::CamState::Error;
    return false;
  }
  g_curRes = c.cam_res;
  sensor_t* s = esp_camera_sensor_get();
  g_id.pid = s ? s->id.PID : 0;
  camera_sensor_info_t* info = esp_camera_sensor_get_info(&s->id);
  g_id.sensorName = info ? info->name : "?";
  g_id.sensorOk = g_id.pid == kExpectedPid;
  X4_LOGI("cam", "sensor", "name=%s pid=0x%04x expected=0x%04x hw_rev=0x%02x module_fw=0x%02x init_ms=%lu max_size=%s",
          g_id.sensorName, g_id.pid, kExpectedPid, g_id.hwRev, g_id.fwVer, (unsigned long)(millis() - t0),
          info ? (info->max_size == FRAMESIZE_5MP ? "5mp" : "other") : "?");
  if (!g_id.sensorOk) {
    X4_LOG_ERR(Error, "cam", "sensor", x4::Err::CAM_SENSOR_MISMATCH, "pid=0x%04x name=%s", g_id.pid, g_id.sensorName);
    g_state = x4::proto::CamState::Error;
    return false;
  }
  if (g_id.hwRev == 0xFF)
    X4_LOG_ERR(Warn, "cam", "hw_rev", x4::Err::CAM_OLD_HW, "rev=0x%02x see=docs/TROUBLESHOOTING.md#old-cams3-hardware", g_id.hwRev);
  applyConfig(c);
  g_state = x4::proto::CamState::Idle;
  return true;
}

const Identity& identity() { return g_id; }

void applyConfig(const x4::cfg::Config& c) {
  if (!g_id.initOk) return;
  Lock l;
  sensor_t* s = esp_camera_sensor_get();
  if (!s) return;
  if (s->set_quality) s->set_quality(s, c.cam_quality);
  if (s->set_vflip) s->set_vflip(s, c.cam_vflip);
  if (s->set_hmirror) s->set_hmirror(s, c.cam_hmirror);
}

bool grab(int32_t res, Frame* out) {
  if (!g_id.sensorOk) return false;
  Lock l;
  if (!setResLocked(res)) return false;
  uint32_t t0 = millis();
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return false;
  out->fb = fb;
  out->startUs = fbStartUs(fb);
  out->grabMs = millis() - t0;
  out->jpegOk = jpegValid(fb);
  return true;
}

void release(Frame& f) {
  if (f.fb) esp_camera_fb_return(f.fb);
  f.fb = nullptr;
}

ShotResult shoot(uint32_t shot, int64_t localAtUs, int32_t res, int64_t rxLocalUs) {
  ShotResult r{false, shot, rxLocalUs, 0, 0, 0, 0};
  if (!g_id.sensorOk) {
    r.err = (uint16_t)x4::Err::CAM_SENSOR_MISMATCH;
    return r;
  }
  // A new shot replaces any shot the Core did not fetch.
  xSemaphoreTake(g_heldMu, portMAX_DELAY);
  if (g_held) {
    X4_LOG_ERR(Warn, "cam", "held_dropped", x4::Err::CAM_SHOT_EXPIRED, "shot=%lu by=%lu", (unsigned long)g_heldShot,
               (unsigned long)shot);
    esp_camera_fb_return(g_held);
    g_held = nullptr;
    g_heldShot = 0;
  }
  xSemaphoreGive(g_heldMu);

  setState(x4::proto::CamState::Armed);
  {
    Lock l;
    setResLocked(res);
  }
  // Sleep until ~3 ms before the target, then spin: vTaskDelay has 1 tick (1 ms) granularity.
  int64_t now = esp_timer_get_time();
  if (localAtUs > now + 4000) vTaskDelay(pdMS_TO_TICKS((localAtUs - now - 3000) / 1000));
  while (esp_timer_get_time() < localAtUs) {
  }
  setState(x4::proto::CamState::Capturing);
  uint32_t t0 = millis();
  camera_fb_t* chosen = nullptr;
  {
    Lock l;
    // A = latest complete frame (started before the target). B = the next one (started after).
    // Keep whichever started closer to the target; the error is at most half a frame period.
    camera_fb_t* a = esp_camera_fb_get();
    camera_fb_t* b = a ? esp_camera_fb_get() : nullptr;
    int64_t target = localAtUs ? localAtUs : rxLocalUs;
    if (a && b) {
      int64_t da = llabs(fbStartUs(a) - target), db = llabs(fbStartUs(b) - target);
      chosen = da <= db ? a : b;
      esp_camera_fb_return(chosen == a ? b : a);
    } else {
      chosen = a ? a : b;
    }
  }
  r.capMs = millis() - t0;
  if (!chosen) {
    r.err = (uint16_t)x4::Err::CAM_CAPTURE_FAIL;
    setState(x4::proto::CamState::Error);
    return r;
  }
  if (!jpegValid(chosen)) {
    bool startsJpeg = chosen->len > 2 && chosen->buf[0] == 0xFF && chosen->buf[1] == 0xD8;
    r.err = (uint16_t)(startsJpeg ? x4::Err::CAM_FRAME_TRUNCATED : x4::Err::CAM_CAPTURE_FAIL);
    X4_LOG_ERR(Error, "cam", "frame_bad", (x4::Err)r.err, "shot=%lu bytes=%u", (unsigned long)shot, (unsigned)chosen->len);
    esp_camera_fb_return(chosen);
    setState(x4::proto::CamState::Idle);
    return r;
  }
  r.ok = true;
  r.capLocalUs = fbStartUs(chosen);
  r.bytes = chosen->len;
  xSemaphoreTake(g_heldMu, portMAX_DELAY);
  g_held = chosen;
  g_heldShot = shot;
  g_heldAtMs = millis();
  xSemaphoreGive(g_heldMu);
  setState(x4::proto::CamState::Holding);
  return r;
}

camera_fb_t* takeHeld(uint32_t shot) {
  xSemaphoreTake(g_heldMu, portMAX_DELAY);
  if (g_held && g_heldShot == shot) return g_held;  // keeps g_heldMu until doneHeld()
  xSemaphoreGive(g_heldMu);
  return nullptr;
}

void doneHeld(bool sent) {
  if (sent && g_held) {
    esp_camera_fb_return(g_held);
    g_held = nullptr;
    g_heldShot = 0;
    setState(x4::proto::CamState::Idle);
  }
  xSemaphoreGive(g_heldMu);
}

uint32_t heldShot() { return g_heldShot; }

void expireHeld(uint32_t nowMs, uint32_t holdMs) {
  if (!g_held) return;
  xSemaphoreTake(g_heldMu, portMAX_DELAY);
  if (g_held && nowMs - g_heldAtMs > holdMs) {
    X4_LOG_ERR(Warn, "cam", "held_expired", x4::Err::CAM_SHOT_EXPIRED, "shot=%lu age_ms=%lu", (unsigned long)g_heldShot,
               (unsigned long)(nowMs - g_heldAtMs));
    esp_camera_fb_return(g_held);
    g_held = nullptr;
    g_heldShot = 0;
    setState(x4::proto::CamState::Idle);
  }
  xSemaphoreGive(g_heldMu);
}

void bench() {
  if (!g_id.sensorOk) return;
  X4_LOGI("bench", "start", "sizes=qvga,vga,hd,uxga,fhd,5mp frames_each=3");
  for (int32_t res = 0; res <= 5; res++) {
    uint32_t minMs = UINT32_MAX, maxMs = 0, sumBytes = 0;
    int ok = 0, bad = 0;
    uint32_t sw0 = millis();
    {
      Lock l;
      setResLocked(res);
    }
    uint32_t switchMs = millis() - sw0;
    int64_t lastStart = 0, period = 0;
    for (int i = 0; i < 3; i++) {
      Frame f;
      uint32_t t0 = millis();
      if (!grab(res, &f)) {
        bad++;
        continue;
      }
      uint32_t ms = millis() - t0;
      if (lastStart) period = f.startUs - lastStart;
      lastStart = f.startUs;
      if (f.jpegOk) {
        ok++;
        sumBytes += f.fb->len;
      } else {
        bad++;
      }
      if (ms < minMs) minMs = ms;
      if (ms > maxMs) maxMs = ms;
      X4_LOGD("bench", "frame", "res=%s i=%d w=%u h=%u bytes=%u grab_ms=%lu jpeg_ok=%d", resName(res), i,
              f.fb->width, f.fb->height, (unsigned)f.fb->len, (unsigned long)ms, f.jpegOk);
      release(f);
    }
    X4_LOG(bad ? x4::log::Level::Warn : x4::log::Level::Info, "bench", "size",
           "res=%s ok=%d bad=%d avg_bytes=%lu grab_ms_min=%lu grab_ms_max=%lu frame_period_ms=%.1f switch_ms=%lu", resName(res),
           ok, bad, (unsigned long)(ok ? sumBytes / ok : 0), (unsigned long)(minMs == UINT32_MAX ? 0 : minMs),
           (unsigned long)maxMs, period / 1000.0f, (unsigned long)switchMs);
  }
  X4_LOGI("bench", "done", "heap=%lu psram=%lu", (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());
}

x4::proto::CamState state() { return g_state; }
uint16_t switchMs() { return g_switchMs; }
void setState(x4::proto::CamState s) { g_state = s; }

}  // namespace cam
}  // namespace x4cam
