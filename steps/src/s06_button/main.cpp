// Step 6: the Mini Button on G1.
// Before wiring: power the button alone from the rail and measure white to GND: ~3.3 V released,
// ~0 V pressed. If it reads ~5 V, do NOT connect it to the Core (docs/HARDWARE.md#mini-button-unit).
// Wiring: Core on the rail (step 5, no USB). Button red/black -> rail, white -> Port A white (G1).
// Pass: pressing shows DOWN and counts presses; releasing shows up. No phantom presses.
#include "core_common.h"

constexpr uint32_t kDebounceMs = 25;

void setup() {
  beginCoreSafely();
  pinMode(pins::kButton, INPUT_PULLUP);  // the button pulls G1 to GND when pressed
  screenTitle("step 6: button on G1");
}

void loop() {
  static bool stable = false;  // true = pressed
  static bool lastRaw = false;
  static uint32_t changedAt = 0;
  static uint32_t presses = 0;

  bool raw = digitalRead(pins::kButton) == LOW;
  if (raw != lastRaw) {
    lastRaw = raw;
    changedAt = millis();
  }
  if (raw != stable && millis() - changedAt >= kDebounceMs) {
    stable = raw;
    if (stable) presses++;
    Serial.printf("button state=%s presses=%lu t=%lu\n", stable ? "down" : "up", (unsigned long)presses,
                  (unsigned long)millis());
    screenLine(0, stable ? TFT_GREEN : TFT_WHITE, "button: %s", stable ? "DOWN" : "up");
    screenLine(1, TFT_WHITE, "presses: %lu", (unsigned long)presses);
  }
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll > 2000) {  // keep watching the 5 V output in every step
    lastPoll = millis();
    if (!readPowerBits().safe()) printPowerBits("found_on", forcePowerOutputsOff());
  }
  delay(2);
}
