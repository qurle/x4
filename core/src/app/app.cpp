#include "app.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "../display/lvgl_port.h"
#include "../hw/flash.h"
#include "../hw/power_safety.h"
#include "../hw/sysinfo.h"
#include "../input/inputs.h"
#include "../log/log_setup.h"
#include "../net/cams.h"
#include "../net/net.h"
#include "../storage/sd_store.h"
#include "../ui/ui.h"
#include "config_store.h"
#include "status.h"
#include "x4/log.h"
#include "x4/shoot.h"
#include "x4/storage_layout.h"
#include "x4/version.h"

namespace x4core {

// ---- status snapshot ---------------------------------------------------------------------------
namespace status {
namespace {
SemaphoreHandle_t g_mu = nullptr;
Status g_s;
}  // namespace
void publish(const Status& s) {
  if (!g_mu) g_mu = xSemaphoreCreateMutex();
  xSemaphoreTake(g_mu, portMAX_DELAY);
  g_s = s;
  xSemaphoreGive(g_mu);
}
Status get() {
  if (!g_mu) return Status{};
  xSemaphoreTake(g_mu, portMAX_DELAY);
  Status s = g_s;
  xSemaphoreGive(g_mu);
  return s;
}
}  // namespace status

namespace app {
namespace {

using x4::shoot::Controller;
using x4::shoot::Fault;
using x4::shoot::State;
namespace sh = x4::shoot;

Mode g_mode = Mode::Full;
Controller g_ctl;
QueueHandle_t g_req = nullptr;
bool g_flashArmed = false;     // user armed the flash for upcoming shots
volatile bool g_cfgDirty = false;
bool g_previewOn = false;
uint32_t g_lastSweep = 0, g_lastThermal = 0, g_lastSd = 0, g_lastPreviewPull = 0;
float g_temp = 0;
bool g_hot = false, g_overheat = false;
esp_timer_handle_t g_flashTimer = nullptr;
uint8_t* g_previewBuf = nullptr;
uint32_t g_previewSeq = 0;
uint32_t g_sdFreeMb = 0, g_sdTotalMb = 0;

sh::Params paramsFrom(const x4::cfg::Config& c) {
  sh::Params p;
  p.ackTimeoutMs = (uint32_t)c.trig_ack_ms;
  p.leadMs = (uint32_t)c.trig_lead_ms;
  p.budgetMs = (uint32_t)c.shot_budget_ms;
  p.parallel = (uint8_t)c.xfer_parallel;
  p.retries = (uint8_t)c.xfer_retries;
  p.minCams = (uint8_t)c.shoot_min_cams;
  p.camDelayMs = (uint32_t)c.cam_delay_ms;
  return p;
}

void onCfgChange(const x4::cfg::OptionInfo&, int32_t) { g_cfgDirty = true; }

void applyLiveConfig() {
  const auto& c = conf::get();
  g_ctl.setParams(paramsFrom(c));
  flash::applyConfig(c);
  inputs::applyConfig(c);
  display::setBrightness((uint8_t)c.ui_brightness);
  M5.Speaker.setVolume((uint8_t)c.ui_volume);
  if (g_mode == Mode::Full) net::applyTransferConfig(c);
  X4_LOGI("cfg", "applied", "live=1");
}

// Preview camera: the configured one if online, otherwise the first online camera (so a rig with
// any single camera still has a viewfinder).
uint8_t choosePreviewCam() {
  bool on[x4::proto::kMaxCams];
  cams::onlineMask(on);
  uint8_t want = (uint8_t)conf::get().prev_cam;
  if (want >= 1 && want <= x4::proto::kMaxCams && on[want - 1]) return want;
  for (uint8_t i = 0; i < x4::proto::kMaxCams; i++)
    if (on[i]) return (uint8_t)(i + 1);
  return want;
}

void startPreview() {
  const auto& c = conf::get();
  if (c.prev_enabled) net::setPreview(true, choosePreviewCam(), (uint8_t)c.prev_fps, (uint8_t)c.prev_res);
}

// The preview camera runs at QVGA/VGA and must switch to the shot size before the scheduled
// capture time. Cameras report their slowest switch (sw= in heartbeats); stretch the lead so that
// camera is not late. Other cameras never leave the shot size.
uint32_t effectiveLeadMs() {
  uint32_t lead = (uint32_t)conf::get().trig_lead_ms;
  if (!g_previewOn) return lead;
  uint8_t pc = net::previewCam();
  if (pc < 1 || pc > x4::proto::kMaxCams) return lead;
  uint32_t sw = cams::snapshot().cam[pc - 1].hb.switchMs;
  uint32_t need = sw ? sw + 40 : 0;  // 40 ms margin for the dropped first frame + scheduling
  return need > lead ? (need > 1000 ? 1000 : need) : lead;
}

void flashTimerCb(void*) {
  uint32_t granted = 0;
  flash::fire((uint32_t)conf::get().flash_pulse_ms, "shot", &granted);
}

void banner() {
  const auto& i = sys::info();
  X4_LOGI("boot", "banner", "app=core name=" X4_PROJECT_NAME " mode=%s fw=%s git=%s built=\"%s\" proto=%d", g_mode == Mode::Full ? "full" : "hello",
          X4_FW_VERSION, X4_GIT_HASH, X4_BUILD_TIME, X4_PROTOCOL_VERSION);
  X4_LOGI("boot", "chip", "model=esp32s3 rev=%lu cpu_mhz=%lu flash_mb=%lu psram_mb=%lu mac=%02x:%02x:%02x:%02x:%02x:%02x board=%d",
          (unsigned long)i.chipRev, (unsigned long)i.cpuMhz, (unsigned long)(i.flashBytes >> 20),
          (unsigned long)(i.psramBytes >> 20), i.mac[0], i.mac[1], i.mac[2], i.mac[3], i.mac[4], i.mac[5],
          (int)M5.getBoard());
  X4_LOGI("boot", "reset", "reason=%s boot_count=%lu brownouts_total=%lu", i.resetName,
          (unsigned long)i.bootCount, (unsigned long)i.brownouts);
  if (i.reset == ESP_RST_BROWNOUT)
    X4_LOG_ERR(Error, "boot", "brownout", x4::Err::BROWNOUT_RESET, "count=%lu", (unsigned long)i.brownouts);
  if (i.reset == ESP_RST_TASK_WDT || i.reset == ESP_RST_INT_WDT || i.reset == ESP_RST_WDT)
    X4_LOG_ERR(Error, "boot", "watchdog", x4::Err::WATCHDOG_RESET, "reason=%s", i.resetName);
  if (i.reset == ESP_RST_PANIC) X4_LOG_ERR(Error, "boot", "panic", x4::Err::PANIC_RESET, "reason=%s", i.resetName);
  X4_LOGI("boot", "memory", "heap_free=%lu heap_min=%lu psram_free=%lu", (unsigned long)sys::freeHeap(),
          (unsigned long)sys::minFreeHeap(), (unsigned long)sys::freePsram());
  if (M5.getBoard() != m5::board_t::board_M5StackCoreS3SE && M5.getBoard() != m5::board_t::board_M5StackCoreS3)
    X4_LOGW("boot", "board", "unexpected_board=%d expected=CoreS3SE", (int)M5.getBoard());
}

void startShot() {
  uint32_t now = millis();
  int64_t nowUs = esp_timer_get_time();
  // Flash only if armed, enabled and ready; otherwise shoot without it (and tell the user).
  bool flashOk = g_flashArmed && flash::ready() && !g_overheat;
  g_ctl.setFlashArmed(flashOk);
  sh::Params p = paramsFrom(conf::get());
  p.leadMs = effectiveLeadMs();
  g_ctl.setParams(p);
  uint32_t shotId = sys::lastShotId() + 1;
  uint32_t eff = g_ctl.shutter(now, nowUs, shotId);
  if (eff & sh::kShutterRefused) {
    const char* why = g_ctl.state() == State::Shooting || g_ctl.state() == State::Transferring ? "busy"
                      : g_ctl.state() == State::Error                                          ? "error"
                                                                                               : "not_ready";
    X4_LOGW("shoot", "refused", "state=%s why=%s", sh::stateName(g_ctl.state()), why);
    x4ui::shutterRefused(why);
    return;
  }
  sys::nextShotId();
  const auto& s = g_ctl.session();
  X4_LOGI("shoot", "start", "shot=%lu at_us=%lld lead_ms=%lu cfg_lead_ms=%ld cams=%d flash=%d delay_ms=%ld",
          (unsigned long)s.id, (long long)s.atUs, (unsigned long)p.leadMs, (long)conf::get().trig_lead_ms, s.expected(),
          s.flash, (long)conf::get().cam_delay_ms);
  if (eff & sh::kSendTrigger) {
    x4::proto::ShootCmd cmd{s.id, s.atUs, (uint8_t)s.flash, (uint8_t)conf::get().cam_res,
                            (uint32_t)conf::get().cam_delay_ms * 1000u};
    net::sendShoot(cmd, (uint8_t)conf::get().trig_repeat);
  }
  if (eff & sh::kFireFlash) {
    int64_t fireAt = s.atUs - (int64_t)conf::get().flash_lead_ms * 1000;
    int64_t delay = fireAt - esp_timer_get_time();
    esp_timer_start_once(g_flashTimer, delay > 0 ? (uint64_t)delay : 1);
  }
  if (eff & sh::kPausePreview) net::setPreview(false, 0, 0);
  x4ui::shutterMoment(s.flash);
  if (g_flashArmed && !flashOk) x4ui::toast("flash cooling down", 0xFFB020);
}

void writeSidecar() {
  const auto& s = g_ctl.session();
  char* buf = (char*)heap_caps_malloc(2048, MALLOC_CAP_SPIRAM);
  if (!buf) return;
  const auto& c = conf::get();
  x4::storage::SidecarInfo info{&s, X4_FW_VERSION, X4_GIT_HASH, "", c.cam_res, c.cam_quality, s.flash, (int32_t)g_temp,
                                c.ui_orient ? "portrait" : "horizontal", c.ui_effect ? "dithered" : "normal",
                                c.cam_delay_ms};
  size_t n = x4::storage::sidecarJson(buf, 2048, info);
  char path[48];
  x4::storage::sidecarFile(path, sizeof(path), s.id);
  if (n && sd::ensureShotDir(s.id)) sd::writeText(path, buf, n);
  heap_caps_free(buf);
}

void handleEffects(uint32_t eff) {
  if (!eff) return;
  const auto& s = g_ctl.session();
  if (eff & sh::kStateChanged) X4_LOGI("app", "state", "state=%s faults=0x%02x", sh::stateName(g_ctl.state()), g_ctl.faults());
  if (eff & sh::kShotCaptured) {
    X4_LOGI("shoot", "captured", "shot=%lu ok=%d of=%d skew_us=%lld ms=%lu", (unsigned long)s.id, s.ok(), s.expected(),
            (long long)s.skewUs(), (unsigned long)(s.capturedAtMs - s.t0Ms));
    x4ui::shotCaptured();
  }
  if (eff & sh::kStartTransfer) {
    for (uint8_t cam = g_ctl.nextToFetch(); cam; cam = g_ctl.nextToFetch()) net::startTransfer(cam, s.id);
  }
  if (eff & sh::kShotStored) {
    writeSidecar();
    bool partial = eff & sh::kShotPartial;
    bool over = eff & sh::kOverBudget;
    X4_LOGI("shoot", "stored", "shot=%lu stored=%d of=%d total_ms=%lu budget_ms=%ld partial=%d", (unsigned long)s.id,
            s.stored(), s.expected(), (unsigned long)s.totalMs(), (long)conf::get().shot_budget_ms, partial);
    if (over)
      X4_LOG_ERR(Warn, "shoot", "slow", x4::Err::NET_TRANSFER_SLOW, "shot=%lu total_ms=%lu", (unsigned long)s.id,
                 (unsigned long)s.totalMs());
    x4ui::shotStored(partial, s.totalMs(), over);
    g_lastSd = 0;  // re-check free space now
  }
  if (eff & sh::kResumePreview) startPreview();
  if (eff & sh::kPausePreview) net::setPreview(false, 0, 0);
}

void processRequests() {
  Request r;
  while (g_req && xQueueReceive(g_req, &r, 0) == pdTRUE) {
    switch (r) {
      case Request::Shutter:
        if (g_mode == Mode::Full) startShot();
        else x4ui::toast("hello build: no cameras", 0x8C877E);
        break;
      case Request::FlashArm:
        flash::setEnabled(true);
        g_flashArmed = true;
        break;
      case Request::FlashDisarm:
        g_flashArmed = false;
        if (!conf::get().flash_enabled) flash::setEnabled(false);
        break;
      case Request::FlashTest: {
        uint32_t granted = 0;
        auto v = flash::fire(x4::safety::kFlashTestPulseMs, "bringup_test", &granted);
        if (v != x4::safety::FlashGuard::Verdict::Ok)
          x4ui::toast(x4::safety::FlashGuard::verdictName(v), 0xFFB020);
        break;
      }
      case Request::SdBench:
        sd::benchmark(1024);
        break;
      case Request::PreviewOn:
        conf::set("prev_enabled", 1, nullptr);
        handleEffects(g_ctl.setPreviewWanted(true, millis()));
        break;
      case Request::PreviewOff:
        conf::set("prev_enabled", 0, nullptr);
        handleEffects(g_ctl.setPreviewWanted(false, millis()));
        net::setPreview(false, 0, 0);
        break;
      case Request::Reboot:
        X4_LOGW("app", "reboot", "by=request");
        delay(100);
        ESP.restart();
        break;
    }
  }
}

void processInputs() {
  x4::input::Event e;
  while (inputs::next(&e)) {
    switch (e.action) {
      case x4::input::Action::Shutter: request(Request::Shutter); break;
      case x4::input::Action::FlashToggle: request(g_flashArmed ? Request::FlashDisarm : Request::FlashArm); break;
      case x4::input::Action::PreviewToggle: request(g_previewOn ? Request::PreviewOff : Request::PreviewOn); break;
      default: x4ui::onAction(e); break;
    }
  }
}

void thermal(uint32_t now) {
  if (now - g_lastThermal < 1000) return;
  g_lastThermal = now;
  const auto& c = conf::get();
  g_temp = sys::chipTempC();
  bool hot = g_temp >= c.therm_warn_c;
  // 3 C hysteresis so the state does not flap.
  bool over = g_overheat ? g_temp >= c.therm_thr_c - 3 : g_temp >= c.therm_thr_c;
  if (hot != g_hot) X4_LOGW("therm", hot ? "warn" : "ok", "temp_c=%.1f warn_c=%ld", g_temp, (long)c.therm_warn_c);
  if (over != g_overheat) {
    if (over) X4_LOG_ERR(Error, "therm", "throttle", x4::Err::OVERHEAT, "temp_c=%.1f thr_c=%ld", g_temp, (long)c.therm_thr_c);
    else X4_LOGI("therm", "recovered", "temp_c=%.1f", g_temp);
    flash::setTooHot(over);
    handleEffects(g_ctl.setFault(Fault::Overheat, over, now));
    if (over) net::setPreview(false, 0, 0);
  }
  g_hot = hot;
  g_overheat = over;
}

void sdCheck(uint32_t now) {
  if (g_lastSd && now - g_lastSd < 10000) return;
  g_lastSd = now;
  bool mounted = sd::mounted();
  if (!mounted && g_mode == Mode::Full) mounted = sd::begin();  // card inserted later
  g_sdFreeMb = (uint32_t)(sd::freeBytes() >> 20);
  g_sdTotalMb = (uint32_t)(sd::totalBytes() >> 20);
  // One full shot needs up to 4 x jpeg buffer (2 MB) + sidecar: keep 12 MB headroom.
  bool full = mounted && g_sdFreeMb < 12;
  if (full) X4_LOG_EVERY(60000, x4::log::Level::Error, "sd", "full", "free_mb=%lu", (unsigned long)g_sdFreeMb);
  handleEffects(g_ctl.setFault(Fault::SdMissing, !mounted, now));
  handleEffects(g_ctl.setFault(Fault::SdFull, full, now));
}

void publishStatus(uint32_t now) {
  Status st{};
  const auto& s = g_ctl.session();
  st.state = g_ctl.state();
  st.faults = g_ctl.faults();
  st.shotId = s.id;
  st.shotsTaken = sys::lastShotId();
  st.shooting = st.state == State::Shooting || st.state == State::Transferring;
  auto snap = cams::snapshot();
  st.camsOnline = snap.online();
  net::XferProgress xp[x4::proto::kMaxCams];
  if (g_mode == Mode::Full) net::progress(xp);
  for (int i = 0; i < x4::proto::kMaxCams; i++) {
    const auto& c = snap.cam[i];
    CamView& v = st.cam[i];
    v.registered = c.registered;
    v.online = c.online;
    IPAddress ip(c.ip);
    snprintf(v.ip, sizeof(v.ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    strncpy(v.fw, c.fw, sizeof(v.fw) - 1);
    v.sensorPid = c.sensorPid;
    v.hwRev = c.hwRev;
    v.tempC = c.hb.tempC;
    v.rssi = c.hb.rssi;
    v.heap = c.hb.heap;
    v.psram = c.hb.psram;
    v.state = c.hb.state;
    v.lastHbAgeMs = c.lastHbMs ? now - c.lastHbMs : 0;
    const auto& cs = s.cam[i];
    v.expected = cs.expected;
    v.acked = cs.acked;
    v.ok = cs.ok;
    v.stored = cs.stored;
    v.failed = cs.failed;
    v.bytes = cs.bytes;
    v.capMs = cs.capMs;
    v.capOffsetUs = cs.ok && s.atUs ? cs.capCoreUs - s.atUs : 0;
    if (g_mode == Mode::Full && xp[i].active && xp[i].shot == s.id) {
      v.xferBytes = xp[i].bytes;
      v.xferTotal = xp[i].total;
    } else {
      v.xferBytes = cs.stored ? cs.bytes : 0;
      v.xferTotal = cs.bytes;
    }
  }
  if (s.doneAtMs) {
    st.lastShotId = s.id;
    st.lastTotalMs = s.totalMs();
    st.lastCapturedMs = s.capturedAtMs ? s.capturedAtMs - s.t0Ms : 0;
    st.lastSkewUs = s.skewUs();
    st.lastStored = s.stored();
    st.lastExpected = s.expected();
  }
  st.flashEnabled = flash::enabled();
  st.flashArmed = g_flashArmed;
  st.flashReady = flash::ready();
  st.flashReadyInMs = flash::readyInMs();
  st.flashPulses = flash::pulseCount();
  {
    uint32_t cool = (uint32_t)conf::get().flash_cool_ms;
    uint32_t left = st.flashReadyInMs > cool ? cool : st.flashReadyInMs;
    st.flashChargePct = (st.flashReady || cool == 0) ? 100 : (uint8_t)(100 - left * 100 / cool);
  }
  st.videoMode = false;
  st.recording = false;
  st.recordMs = 0;
  const auto& p = power::last();
  st.powerSafe = power::safe();
  st.busOutEn = p.busOutEn;
  st.usbOtgEn = p.usbOtgEn;
  st.vbusMv = p.vbusMv;
  st.tempC = g_temp;
  st.hot = g_hot;
  st.overheat = g_overheat;
  st.heap = sys::freeHeap();
  st.heapMin = sys::minFreeHeap();
  st.psram = sys::freePsram();
  st.resetReason = sys::info().resetName;
  st.bootCount = sys::info().bootCount;
  st.brownouts = sys::info().brownouts;
  st.uptimeMs = now;
  st.sdMounted = sd::mounted();
  st.sdFreeMb = g_sdFreeMb;
  st.sdTotalMb = g_sdTotalMb;
  st.sdWriteKBps = sd::lastBench().writeKBps;
  st.sdReadKBps = sd::lastBench().readKBps;
  st.fps = display::stats().fps;
  st.frameMaxUs = display::stats().maxFrameUs;
  st.previewFps = g_mode == Mode::Full ? net::previewFps() : 0;
  st.previewOn = g_previewOn;
  st.apStations = g_mode == Mode::Full ? net::apStations() : 0;
  st.buttonDown = inputs::shutterPressed();
  const auto& cf = conf::get();
  st.uiOrient = (uint8_t)cf.ui_orient;
  st.uiEffect = (uint8_t)cf.ui_effect;
  st.camDelayMs = (uint16_t)cf.cam_delay_ms;
  status::publish(st);
  x4ui::update(st, now);
}

void pullPreview(uint32_t now) {
  bool want = g_ctl.state() == State::Preview;
  if (want != g_previewOn) {
    g_previewOn = want;
    if (!want) x4ui::clearPreview();
  }
  if (!want || !g_previewBuf || now - g_lastPreviewPull < 30) return;
  g_lastPreviewPull = now;
  uint32_t n = net::takePreview(g_previewBuf, 128 * 1024, &g_previewSeq);
  if (n) x4ui::setPreviewJpeg(g_previewBuf, n, conf::get().prev_res ? 0.5f : 1.0f);
}

}  // namespace

void request(Request r) {
  if (g_req) xQueueSend(g_req, &r, 0);
}

bool setOption(const char* name, int32_t value) { return conf::set(name, value, nullptr); }

void setup(Mode m) {
  g_mode = m;
  flash::earlyOff();  // (again) first thing: flash EN low
  logsys::begin("core");
  X4_LOGI("boot", "start", "fw=%s git=%s", X4_FW_VERSION, X4_GIT_HASH);

  auto cfg = M5.config();
  cfg.output_power = false;        // NEVER drive 5 V out: the rail powers the Core (docs/POWER.md)
  cfg.internal_imu = false;        // the SE has no IMU
  cfg.internal_mic = false;
  cfg.internal_spk = true;
  cfg.external_display_value = 0;  // do not probe Port A: G1/G2 carry the button and the flash EN
  cfg.external_speaker_value = 0;
  cfg.led_brightness = 0;
  M5.begin(cfg);
  power::enforce("boot");

  sys::begin();
  conf::begin();
  conf::onChange(onCfgChange);
  banner();
  conf::logSummary();
  const auto& c = conf::get();

  flash::begin(c);
  esp_timer_create_args_t ta = {};
  ta.callback = flashTimerCb;
  ta.name = "flash_sched";
  ta.dispatch_method = ESP_TIMER_TASK;
  esp_timer_create(&ta, &g_flashTimer);

  g_req = xQueueCreate(16, sizeof(Request));
  inputs::begin(c);
  M5.Speaker.setVolume((uint8_t)c.ui_volume);

  display::begin((uint8_t)c.ui_brightness);
  x4ui::begin(m == Mode::Hello);
  display::handle();

  bool sdOk = sd::begin();
  logsys::enableSdQueue(sdOk && c.log_sd);
  if (sdOk && m == Mode::Hello) sd::benchmark(1024);

  cams::begin();
  if (m == Mode::Full) {
    net::beginAp(c);
    net::beginUdp(c);
    net::beginTransfer(c);
    net::beginWeb();
    net::beginOta();
    g_previewBuf = (uint8_t*)heap_caps_malloc(128 * 1024, MALLOC_CAP_SPIRAM);
  }
  g_ctl.setParams(paramsFrom(c));
  handleEffects(g_ctl.setPreviewWanted(c.prev_enabled != 0, millis()));
  handleEffects(g_ctl.bootDone(millis()));
  sdCheck(millis());
  enableLoopWDT();  // a hung loop resets the chip (flash EN returns to its pulled-down reset state)
  X4_LOGI("boot", "ready", "ms=%lu heap_free=%lu", (unsigned long)millis(), (unsigned long)sys::freeHeap());
}

void loop() {
  uint32_t now = millis();
  M5.update();
  inputs::poll(now);
  processInputs();
  processRequests();

  if (g_mode == Mode::Full) {
    x4::proto::ShotAck a;
    while (net::nextAck(&a)) {
      X4_LOGI("shoot", "ack", "cam=%u shot=%lu ok=%d bytes=%lu cap_ms=%lu cap_vs_at_us=%lld", a.cam, (unsigned long)a.shot,
              a.ok, (unsigned long)a.bytes, (unsigned long)a.capMs,
              (long long)(a.capCoreUs - g_ctl.session().atUs));
      handleEffects(g_ctl.onAck(a, now));
    }
    net::XferResult r;
    while (net::nextXferResult(&r)) {
      if (r.shot != g_ctl.session().id) continue;
      handleEffects(r.ok ? g_ctl.onStored(r.cam, true, now) : g_ctl.onTransferRetry(r.cam));
    }
    if (now - g_lastSweep >= 250) {
      g_lastSweep = now;
      cams::sweep(now, (uint32_t)conf::get().hb_offline_ms);
      bool on[x4::proto::kMaxCams];
      cams::onlineMask(on);
      handleEffects(g_ctl.setCameras(on, now));
      if (g_ctl.state() == State::Preview && choosePreviewCam() != net::previewCam()) startPreview();
    }
    handleEffects(g_ctl.tick(now));
    pullPreview(now);
    net::pollOta();
  }

  if (g_cfgDirty) {
    g_cfgDirty = false;
    applyLiveConfig();
  }
  thermal(now);
  sdCheck(now);
  power::poll(now);
  flash::poll();
  if (conf::get().log_sd) sd::pumpLogs((uint32_t)conf::get().log_sd_max_kb);

  publishStatus(now);
  uint32_t wait = display::handle();
  vTaskDelay(pdMS_TO_TICKS(wait > 5 ? 5 : (wait ? wait : 1)));
}

}  // namespace app
}  // namespace x4core
