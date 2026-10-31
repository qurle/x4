#include "flash.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <esp_timer.h>

#include <atomic>

#include "board_pins.h"
#include "x4/log.h"

namespace x4core {
namespace flash {
namespace {

using x4::safety::FlashGuard;

constexpr gpio_num_t kPin = (gpio_num_t)pins::kFlashEn;

FlashGuard g_guard(x4::safety::kFlashPulseAbsMaxMs, x4::safety::kFlashCooldownAbsMinMs);
esp_timer_handle_t g_offTimer = nullptr;
std::atomic<bool> g_pulseEnded{false};
std::atomic<uint32_t> g_endMs{0};
uint32_t g_pulses = 0;
bool g_begun = false;

void IRAM_ATTR pinLow() { gpio_set_level(kPin, 0); }

void offTimerCb(void*) {
  pinLow();
  g_endMs.store((uint32_t)(esp_timer_get_time() / 1000));
  g_pulseEnded.store(true);
}

void shutdownHandler() { pinLow(); }

// (1) Earliest possible: runs during C++ static initialisation, before setup().
struct EarlyInit {
  EarlyInit() { earlyOff(); }
} g_early __attribute__((init_priority(101)));

}  // namespace

void earlyOff() {
  gpio_reset_pin(kPin);
  // INPUT_OUTPUT so poll() can read back the real pad level.
  gpio_set_direction(kPin, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_pull_mode(kPin, GPIO_PULLDOWN_ONLY);
  gpio_set_level(kPin, 0);
}

void applyConfig(const x4::cfg::Config& c) {
  // Keeps the armed state and the cooldown timer; flash_enabled only sets the state at boot.
  g_guard.setLimits((uint32_t)c.flash_pulse_ms, (uint32_t)c.flash_cool_ms);
}

void begin(const x4::cfg::Config& c) {
  earlyOff();
  if (!g_offTimer) {
    esp_timer_create_args_t a = {};
    a.callback = offTimerCb;
    a.name = "flash_off";
    a.dispatch_method = ESP_TIMER_TASK;
    esp_timer_create(&a, &g_offTimer);
    esp_register_shutdown_handler(shutdownHandler);
  }
  g_guard = FlashGuard((uint32_t)c.flash_pulse_ms, (uint32_t)c.flash_cool_ms);
  g_guard.setEnabled(c.flash_enabled != 0);
  g_begun = true;
  X4_LOGI("flash", "init", "pin=%d enabled=%d cap_ms=%lu cooldown_ms=%lu abs_cap_ms=%lu abs_cooldown_ms=%lu",
          (int)kPin, g_guard.enabled(), (unsigned long)g_guard.cap(), (unsigned long)g_guard.cooldown(),
          (unsigned long)x4::safety::kFlashPulseAbsMaxMs,
          (unsigned long)x4::safety::kFlashCooldownAbsMinMs);
}

FlashGuard::Verdict fire(uint32_t wantMs, const char* reason, uint32_t* grantedMs) {
  uint32_t now = millis();
  uint32_t granted = 0, wait = 0;
  if (!g_begun) return FlashGuard::Verdict::Disabled;
  FlashGuard::Verdict v = g_guard.request(now, wantMs, &granted, &wait);
  if (grantedMs) *grantedMs = granted;
  if (v != FlashGuard::Verdict::Ok) {
    X4_LOG_ERR(Info, "flash", "refused", x4::Err::FLASH_REFUSED, "verdict=%s wait_ms=%lu reason=%s",
               FlashGuard::verdictName(v), (unsigned long)wait, reason ? reason : "-");
    return v;
  }
  g_pulseEnded.store(false);
  g_guard.fired(now, granted);
  gpio_set_level(kPin, 1);
  esp_timer_start_once(g_offTimer, (uint64_t)granted * 1000ULL);
  g_pulses++;
  X4_LOGI("flash", "fire", "pulse_ms=%lu reason=%s n=%lu", (unsigned long)granted,
          reason ? reason : "-", (unsigned long)g_pulses);
  return v;
}

void poll() {
  uint32_t now = millis();
  if (g_pulseEnded.exchange(false)) g_guard.ended(g_endMs.load());
  bool high = gpio_get_level(kPin) != 0;
  bool overdue = g_guard.overdue(now);
  if ((high && !g_guard.active()) || overdue) {
    // (4) The pin is high without a running pulse, or the pulse outlived its grant.
    esp_timer_stop(g_offTimer);
    pinLow();
    g_guard.ended(now);
    X4_LOG_ERR(Error, "flash", "watchdog", x4::Err::FLASH_STUCK_ON, "overdue=%d high=%d", overdue,
               high);
  }
}

void setTooHot(bool hot) { g_guard.setTooHot(hot); }
void setEnabled(bool en) {
  g_guard.setEnabled(en);
  X4_LOGI("flash", "armed", "enabled=%d", en);
}
bool enabled() { return g_guard.enabled(); }
bool ready() {
  uint32_t g, w;
  return g_guard.request(millis(), 0, &g, &w) == FlashGuard::Verdict::Ok;
}
uint32_t readyInMs() { return g_guard.readyInMs(millis()); }
uint32_t pulseCount() { return g_pulses; }
int pin() { return (int)kPin; }

}  // namespace flash
}  // namespace x4core
