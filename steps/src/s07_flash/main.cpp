// Step 7: one guarded flash. The Flashlight Unit's LED is driven hard, so the firmware caps the
// pulse at 50 ms and refuses another flash for 8 s (M5 recommends 8-15 s between flashes).
// Wiring: step 6 wiring, plus Flashlight red/black -> rail and Flashlight yellow (EN) -> Port A
//         yellow (G2). Flashlight slide switch on FLASH. Only after step 2 measured G2 at ~0 V.
// Use: tap the screen to ARM (auto-disarms after 60 s), press the Mini Button to fire.
// Pass: one short flash per press; presses inside 8 s are refused on screen; no reset; the
//       Core's own 5 V reading stays above ~4.6 V during the flash ("dip" line on screen).
//
// The dip: while the flash is on (and 100 ms after), the Core reads its 5 V input (AXP2101 VBUS
// ADC over I2C) as often as it can and keeps the lowest value. How often that ADC updates is
// Not sure, so it also counts how often the value changed: 0 changes = too slow to tell.
// The pulse is ended by a one-shot esp_timer, not by this loop, so slow reads can't stretch it.
#include <esp_timer.h>

#include "core_common.h"

constexpr uint32_t kPulseMs = 50;          // well under the 150 ms hard cap of the real firmware
constexpr uint32_t kAfterMs = 100;         // keep reading this long after the pulse (recovery)
constexpr uint32_t kCooldownMs = 8000;
constexpr uint32_t kArmTimeoutMs = 60000;

bool g_armed = false;
uint32_t g_armedAt = 0;
uint32_t g_lastFlash = 0;
bool g_everFired = false;
uint32_t g_count = 0;
esp_timer_handle_t g_pulseEnd = nullptr;

struct Dip {
  int beforeMv = 0, minMv = 0, afterMv = 0;
  uint32_t reads = 0, changes = 0;
  bool measured = false;
} g_dip;

void endPulse(void*) { digitalWrite(pins::kFlashEn, LOW); }  // runs in the esp_timer task

void fire() {
  Dip d;
  d.beforeMv = M5.Power.getVBUSVoltage();
  digitalWrite(pins::kFlashEn, HIGH);
  if (g_pulseEnd && esp_timer_start_once(g_pulseEnd, (uint64_t)kPulseMs * 1000) == ESP_OK) {
    int64_t t0 = esp_timer_get_time();
    int last = d.beforeMv;
    d.minMv = d.beforeMv;
    while (esp_timer_get_time() - t0 < (int64_t)(kPulseMs + kAfterMs) * 1000) {
      int v = M5.Power.getVBUSVoltage();
      d.reads++;
      if (v != last) d.changes++;
      last = v;
      if (v > 0 && v < d.minMv) d.minMv = v;
    }
    d.afterMv = last;
    d.measured = true;
  } else {
    // No timer: the old way, blocking, nothing between HIGH and LOW. No dip reading.
    delay(kPulseMs);
  }
  digitalWrite(pins::kFlashEn, LOW);  // already low via the timer; second guard
  g_lastFlash = millis();
  g_everFired = true;
  g_count++;
  g_dip = d;
  Serial.printf("flash fired pulse_ms=%lu count=%lu dip_measured=%d vbus_before_mv=%d vbus_min_mv=%d "
                "vbus_after_mv=%d reads=%lu changes=%lu window_ms=%lu\n",
                (unsigned long)kPulseMs, (unsigned long)g_count, d.measured, d.beforeMv, d.minMv, d.afterMv,
                (unsigned long)d.reads, (unsigned long)d.changes, (unsigned long)(kPulseMs + kAfterMs));
}

void setup() {
  beginCoreSafely();          // also forces G2 LOW (core_common.h)
  pinMode(pins::kFlashEn, OUTPUT);
  digitalWrite(pins::kFlashEn, LOW);
  pinMode(pins::kButton, INPUT_PULLUP);
  esp_timer_create_args_t a = {};
  a.callback = endPulse;
  a.dispatch_method = ESP_TIMER_TASK;
  a.name = "flash_end";
  if (esp_timer_create(&a, &g_pulseEnd) != ESP_OK) {
    g_pulseEnd = nullptr;
    Serial.println("flash timer create failed: dip reading off");
  }
  screenTitle("step 7: guarded flash");
}

void loop() {
  M5.update();
  uint32_t now = millis();
  if (M5.Touch.getDetail().wasPressed()) {
    g_armed = !g_armed;
    g_armedAt = now;
    Serial.printf("flash %s\n", g_armed ? "armed" : "disarmed");
  }
  if (g_armed && now - g_armedAt > kArmTimeoutMs) {
    g_armed = false;
    Serial.println("flash disarmed reason=timeout");
  }

  static bool wasDown = false;
  bool down = digitalRead(pins::kButton) == LOW;
  if (down && !wasDown) {
    uint32_t since = now - g_lastFlash;
    if (!g_armed) {
      Serial.println("flash refused reason=not_armed");
      screenLine(5, TFT_ORANGE, "refused: tap to arm first");
    } else if (g_everFired && since < kCooldownMs) {
      Serial.printf("flash refused reason=cooldown wait_ms=%lu\n", (unsigned long)(kCooldownMs - since));
      screenLine(5, TFT_ORANGE, "refused: cooling down");
    } else {
      fire();
      screenLine(5, TFT_GREEN, "fired #%lu", (unsigned long)g_count);
    }
  }
  wasDown = down;
  delay(down ? 30 : 5);  // crude debounce

  static uint32_t lastDraw = 0;
  if (now - lastDraw < 200) return;  // redraw 5x a second, not every loop
  lastDraw = now;
  uint32_t wait = (g_everFired && now - g_lastFlash < kCooldownMs) ? kCooldownMs - (now - g_lastFlash) : 0;
  screenLine(0, g_armed ? TFT_RED : TFT_WHITE, g_armed ? "ARMED (tap to disarm)" : "safe (tap to arm)");
  screenLine(1, wait ? TFT_ORANGE : TFT_GREEN, wait ? "next flash in %lu.%lu s" : "ready", (unsigned long)(wait / 1000),
             (unsigned long)(wait % 1000 / 100));
  screenLine(2, TFT_WHITE, "pulse %lu ms, cooldown %lu s", (unsigned long)kPulseMs, (unsigned long)(kCooldownMs / 1000));
  static uint32_t lastPoll = 0;
  if (now - lastPoll > 2000) {
    lastPoll = now;
    PowerBits b = readPowerBits();
    if (!b.safe()) b = forcePowerOutputsOff();
    screenLine(3, TFT_WHITE, "5V input: %d mV", b.vbusMv);
  }
  if (!g_dip.measured) return;
  if (!g_dip.changes)
    screenLine(4, TFT_ORANGE, "dip: reading never changed (too slow)");
  else
    screenLine(4, g_dip.minMv >= 4600 ? TFT_GREEN : TFT_RED, "dip: min %d mV (before %d), %lu/%lu",
               g_dip.minMv, g_dip.beforeMv, (unsigned long)g_dip.changes, (unsigned long)g_dip.reads);
}
