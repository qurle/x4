#include "camsys.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "x4/log.h"
#include "x4/version.h"

#ifndef X4_CAM_ID
#define X4_CAM_ID 0
#endif

namespace x4cam {
namespace {

Ring g_ring;
QueueHandle_t g_fwdQ = nullptr;
volatile bool g_fwd = false;
uint8_t g_id = X4_CAM_ID;
uint32_t g_boots = 0;
Preferences g_sys;
Preferences g_nvs;
x4::cfg::Config g_base, g_cfg;
constexpr size_t kLine = 200;

uint32_t clockMs() { return (uint32_t)millis(); }
void serialSink(const char* line, size_t len, x4::log::Level, void*) {
  Serial.write((const uint8_t*)line, len);
  Serial.write('\n');
}
void ringSink(const char* line, size_t len, x4::log::Level, void*) { g_ring.append(line, len); }
void fwdSink(const char* line, size_t len, x4::log::Level lvl, void*) {
  if (!g_fwd || !g_fwdQ) return;
  if ((int)lvl > g_cfg.log_fwd_level) return;
  char buf[kLine];
  size_t n = len < kLine - 1 ? len : kLine - 1;
  memcpy(buf, line, n);
  buf[n] = 0;
  xQueueSend(g_fwdQ, buf, 0);
}

const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "other_wdt";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_USB: return "usb";
    case ESP_RST_EXT: return "external";
    default: return "unknown";
  }
}

int idfVprintf(const char* fmt, va_list ap) {
  char raw[180];
  int n = vsnprintf(raw, sizeof(raw), fmt, ap);
  if (n <= 0) return n;
  char* s = raw;
  if (s[0] == '\x1b') {
    char* m = strchr(s, 'm');
    if (m) s = m + 1;
  }
  x4::log::Level lvl = s[0] == 'E' ? x4::log::Level::Error : s[0] == 'W' ? x4::log::Level::Warn : x4::log::Level::Debug;
  size_t l = strlen(s);
  while (l && (s[l - 1] == '\n' || s[l - 1] == '\r')) s[--l] = 0;
  char* esc = strchr(s, '\x1b');
  if (esc) *esc = 0;
  char safe[160];
  x4::log::sanitizeValue(safe, sizeof(safe), s);
  if (X4_LOG_ENABLED(lvl)) x4::log::write(lvl, "idf", "log", "msg=%s", safe);
  return n;
}

}  // namespace

namespace sys {

void beginLog() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
  Serial.setTxTimeoutMs(0);
#endif
  g_sys.begin("x4sys", false);
  uint8_t stored = g_sys.getUChar("cam_id", 0);
  if (stored >= 1 && stored <= 4) g_id = stored;
  g_boots = g_sys.getUInt("boots", 0) + 1;
  g_sys.putUInt("boots", g_boots);
  char dev[8];
  if (g_id) snprintf(dev, sizeof(dev), "cam%u", g_id);
  else snprintf(dev, sizeof(dev), "cam?");
  x4::log::setDevice(dev);
  x4::log::setClock(clockMs);
  x4::log::addSink(serialSink, nullptr, x4::log::Level::Trace);
  x4::log::addSink(ringSink, nullptr, x4::log::Level::Trace);
  g_fwdQ = xQueueCreate(16, kLine);
  x4::log::addSink(fwdSink, nullptr, x4::log::Level::Warn);
  esp_log_set_vprintf(idfVprintf);
}

Ring& ring() { return g_ring; }
void enableForward(bool on) { g_fwd = on; }
bool popForward(char* out, size_t cap) { return g_fwdQ && cap >= kLine && xQueueReceive(g_fwdQ, out, 0) == pdTRUE; }
uint8_t camId() { return g_id; }
void setCamId(uint8_t id) {
  g_sys.putUChar("cam_id", id);
  X4_LOGW("sys", "cam_id", "stored=%u reboot_to_apply=1", id);
}
const char* resetName() { return resetReasonName(esp_reset_reason()); }
bool lastResetBrownout() { return esp_reset_reason() == ESP_RST_BROWNOUT; }
uint32_t bootCount() { return g_boots; }
float tempC() { return temperatureRead(); }

void banner() {
  esp_chip_info_t ci;
  esp_chip_info(&ci);
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  X4_LOGI("boot", "banner", "app=camera fw=%s git=%s built=\"%s\" proto=%d cam_id=%u hello=%d", X4_FW_VERSION, X4_GIT_HASH,
          X4_BUILD_TIME, X4_PROTOCOL_VERSION, g_id,
#if X4_CAM_HELLO
          1
#else
          0
#endif
  );
  X4_LOGI("boot", "chip", "model=esp32s3 rev=%d cores=%d cpu_mhz=%lu flash_mb=%lu psram_mb=%lu psram_free=%lu mac=%02x:%02x:%02x:%02x:%02x:%02x",
          ci.revision, ci.cores, (unsigned long)getCpuFrequencyMhz(), (unsigned long)(ESP.getFlashChipSize() >> 20),
          (unsigned long)(ESP.getPsramSize() >> 20), (unsigned long)ESP.getFreePsram(), mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);
  X4_LOGI("boot", "reset", "reason=%s boot_count=%lu", resetName(), (unsigned long)g_boots);
  if (lastResetBrownout()) X4_LOG_ERR(Error, "boot", "brownout", x4::Err::BROWNOUT_RESET, "boots=%lu", (unsigned long)g_boots);
  if (ESP.getPsramSize() < 4 * 1024 * 1024)
    X4_LOG_ERR(Error, "boot", "psram", x4::Err::CAM_NO_PSRAM, "psram=%lu need=8MB_OPI", (unsigned long)ESP.getPsramSize());
}

}  // namespace sys

namespace conf {

void begin() {
  g_base = x4::cfg::defaults();
  g_cfg = g_base;
  g_nvs.begin("x4cfg", false);
  for (size_t i = 0; i < x4::cfg::optionCount(); i++) {
    const auto& o = x4::cfg::option(i);
    if ((o.scope & x4::cfg::CAM) && g_nvs.isKey(o.name)) x4::cfg::put(g_cfg, o, g_nvs.getInt(o.name, o.def));
  }
  x4::cfg::Issue is[6];
  size_t n = x4::cfg::validate(g_cfg, g_base, true, is, 6);
  for (size_t i = 0; i < n && i < 6; i++)
    X4_LOG_ERR(Warn, "cfg", "clamped", x4::Err::CFG_CLAMPED, "key=%s requested=%ld applied=%ld reason=%s", is[i].name,
               (long)is[i].requested, (long)is[i].applied, is[i].reason);
  x4::log::setLevel((x4::log::Level)g_cfg.log_level);
  char buf[900];
  x4::cfg::summary(g_cfg, x4::cfg::CAM, buf, sizeof(buf));
  X4_LOGI("cfg", "summary", "%s", buf);
}

x4::cfg::Config& get() { return g_cfg; }
const x4::cfg::Config& base() { return g_base; }

bool set(const char* name, int32_t v, bool persist) {
  x4::cfg::Issue is;
  if (!x4::cfg::set(g_cfg, g_base, name, v, &is)) return false;
  const auto* o = x4::cfg::find(name);
  int32_t applied = x4::cfg::get(g_cfg, *o);
  if (is.name)
    X4_LOG_ERR(Warn, "cfg", "clamped", x4::Err::CFG_CLAMPED, "key=%s requested=%ld applied=%ld reason=%s", is.name,
               (long)is.requested, (long)is.applied, is.reason);
  if (persist) g_nvs.putInt(name, applied);
  if (strcmp(name, "log_level") == 0) x4::log::setLevel((x4::log::Level)applied);
  return true;
}

}  // namespace conf
}  // namespace x4cam
