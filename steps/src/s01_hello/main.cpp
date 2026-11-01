// Step 1: the Core boots, talks over USB serial and draws on its screen.
// Wiring: nothing. Only USB-C from the computer. Port A empty.
// Pass: the screen shows "step 1: hello" and a counter; the serial monitor prints a line a second.
#include "core_common.h"

void setup() {
  beginCoreSafely();  // M5.begin with the 5 V output OFF (step 2 checks this properly)
  Serial.printf("hello chip=%s rev=%d cores=%d flash_mb=%u psram_mb=%u\n", ESP.getChipModel(),
                ESP.getChipRevision(), ESP.getChipCores(), (unsigned)(ESP.getFlashChipSize() >> 20),
                (unsigned)(ESP.getPsramSize() >> 20));
  screenTitle("step 1: hello");
  screenLine(0, TFT_WHITE, "%s, flash %u MB, PSRAM %u MB", ESP.getChipModel(),
             (unsigned)(ESP.getFlashChipSize() >> 20), (unsigned)(ESP.getPsramSize() >> 20));
}

void loop() {
  static uint32_t n = 0;
  Serial.printf("hello n=%lu uptime_ms=%lu\n", (unsigned long)n, (unsigned long)millis());
  screenLine(2, TFT_GREEN, "alive: %lu", (unsigned long)n++);
  delay(1000);
}
