// Absolute safety limits. These are compile-time constants on purpose: no config, NVS entry or
// HTTP request can move past them. Config values (config_schema.def) live inside these bounds and
// can only be TIGHTENED at runtime. Rationale and sources: docs/POWER.md, docs/HARDWARE.md.
#pragma once
#include <stdint.h>

namespace x4 {
namespace safety {

// ---- Flashlight Unit (AW3641EDNR, flash mode) -------------------------------------------------
// Owner-confirmed peak ~1 A for ~100 ms. AW3641 has its own flash timeout (~220 ms typ. for 1..8
// EN rising edges, datasheet DS_AW3641E_EN_V1.4) which is the hardware backstop; ours is lower.
constexpr uint32_t kFlashPulseAbsMaxMs = 150;
// M5Stack docs (Unit FlashLight, "operation"): "interval of 8~15 seconds between flashes is
// recommended" to avoid overheating. The brief asked for >= 1 s; we use the stricter M5 number.
constexpr uint32_t kFlashCooldownAbsMinMs = 8000;
// Minimum spacing between the falling edge and a new rising edge inside one pulse train.
constexpr uint32_t kFlashTestPulseMs = 50;  // Stage 0 bring-up test pulse.

// ---- Thermal (ESP32-S3 internal sensor, die temperature, not ambient) -----------------------
// ESP32-S3-WROOM-1-N16R8 (octal PSRAM) is rated to 65 C *ambient* (Espressif module datasheet,
// ordering table). The die runs hotter than ambient; these die thresholds are ASSUMPTIONS to be
// replaced with measurements (docs/MEASUREMENTS.md).
constexpr int32_t kThermalWarnAbsMaxC = 75;      // config may only lower it
constexpr int32_t kThermalThrottleAbsMaxC = 85;  // config may only lower it

// ---- SoftAP ----------------------------------------------------------------------------------
// 4 cameras + 1 phone + 1 spare. ESP-IDF caps SoftAP stations at ESP_WIFI_MAX_CONN_NUM (10 here).
constexpr uint8_t kApMinClients = 6;
constexpr uint8_t kApMaxClients = 10;

// ---- Bursts ----------------------------------------------------------------------------------
constexpr uint8_t kBurstAbsMaxFrames = 5;

// Pure flash guard: decides whether a flash pulse may fire now and for how long.
// Hardware code calls request() before driving the pin and fired() right after; the watchdog
// calls overdue() to catch a pin left high.
class FlashGuard {
 public:
  enum class Verdict : uint8_t { Ok, Disabled, Cooldown, TooHot, Busy };

  // pulseCapMs and cooldownMs are clamped to the absolute limits above.
  FlashGuard(uint32_t pulseCapMs, uint32_t cooldownMs);

  // Changes limits WITHOUT resetting timing state (a config change must not skip a cooldown).
  void setLimits(uint32_t pulseCapMs, uint32_t cooldownMs);
  void setEnabled(bool en) { enabled_ = en; }
  bool enabled() const { return enabled_; }
  void setTooHot(bool hot) { tooHot_ = hot; }

  // wantMs is clamped to the cap. On Ok, *grantedMs is the pulse to emit. On Cooldown,
  // *waitMs is how long until the next flash is allowed.
  Verdict request(uint32_t nowMs, uint32_t wantMs, uint32_t* grantedMs, uint32_t* waitMs) const;
  void fired(uint32_t nowMs, uint32_t pulseMs);   // rising edge happened
  void ended(uint32_t nowMs);                     // falling edge happened
  // True if the pin should already be low (pulse running longer than granted + slack).
  bool overdue(uint32_t nowMs, uint32_t slackMs = 10) const;
  bool active() const { return active_; }
  uint32_t cap() const { return cap_; }
  uint32_t cooldown() const { return cooldown_; }
  uint32_t readyInMs(uint32_t nowMs) const;

  static const char* verdictName(Verdict v);

 private:
  uint32_t cap_;
  uint32_t cooldown_;
  bool enabled_ = false;  // off by default: the bring-up screen or config must enable it
  bool tooHot_ = false;
  bool active_ = false;
  bool everFired_ = false;
  uint32_t lastStart_ = 0;
  uint32_t lastEnd_ = 0;
  uint32_t granted_ = 0;
};

}  // namespace safety
}  // namespace x4
