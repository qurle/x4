// Step 7: one guarded flash. The Flashlight Unit's LED is driven hard, so the firmware caps the
// pulse at 50 ms and refuses another flash for 8 s (M5 recommends 8-15 s between flashes).
// Wiring: step 6 wiring, plus Flashlight red/black -> rail and Flashlight yellow (EN) -> Port A
//         yellow (G2). Flashlight slide switch on FLASH. Only after step 2 measured G2 at ~0 V.
// Use: tap the screen to ARM (auto-disarms after 60 s), press the Mini Button to fire.
// Pass: one short flash per press; presses inside 8 s are refused on screen; no reset; the rail
//       (USB meter / multimeter min) stays above ~4.6 V during the flash.
#include "core_common.h"

constexpr uint32_t kPulseMs = 50;          // well under the 150 ms hard cap of the real firmware
constexpr uint32_t kCooldownMs = 8000;
constexpr uint32_t kArmTimeoutMs = 60000;

bool g_armed = false;
uint32_t g_armedAt = 0;
uint32_t g_lastFlash = 0;
bool g_everFired = false;
uint32_t g_count = 0;

void fire() {
  // Blocking on purpose: nothing can run between HIGH and LOW, so the pin cannot stay high
  // because some other code got stuck.
  digitalWrite(pins::kFlashEn, HIGH);
  delay(kPulseMs);
  digitalWrite(pins::kFlashEn, LOW);
  g_lastFlash = millis();
  g_everFired = true;
  g_count++;
  Serial.printf("flash fired pulse_ms=%lu count=%lu\n", (unsigned long)kPulseMs, (unsigned long)g_count);
}

void setup() {
  beginCoreSafely();          // also forces G2 LOW (core_common.h)
  pinMode(pins::kFlashEn, OUTPUT);
  digitalWrite(pins::kFlashEn, LOW);
  pinMode(pins::kButton, INPUT_PULLUP);
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
}
