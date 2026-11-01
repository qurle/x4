// Step 2: prove the Core's 5 V OUTPUT is off. This matters because the rig powers the Core
// backwards through Port A; if the Core also pushed 5 V out, two supplies would fight.
// Wiring: only USB-C from the computer. Port A empty.
// Pass: screen says SAFE, and a multimeter on Port A red (+) / black (-) reads about 0 V.
//       Port A yellow (G2, the future flash pin) to black also reads about 0 V.
#include "core_common.h"

void show(const PowerBits& b) {
  screenLine(0, b.readOk ? TFT_WHITE : TFT_RED, "I2C read of AW9523B: %s", b.readOk ? "ok" : "FAILED");
  screenLine(1, b.busOut ? TFT_RED : TFT_WHITE, "BUS_OUT_EN (5V to Port A): %d", b.busOut);
  screenLine(2, b.usbOtg ? TFT_RED : TFT_WHITE, "USB_OTG_EN (5V out of USB): %d", b.usbOtg);
  screenLine(3, TFT_WHITE, "BOOST_EN: %d", b.boost);
  screenLine(4, TFT_WHITE, "5V input seen: %d mV", b.vbusMv);
  if (b.safe()) {
    screenLine(6, TFT_GREEN, "SAFE. Now measure Port A: expect ~0 V");
  } else {
    screenLine(6, TFT_RED, "STOP. Do not connect the rail.");
  }
}

void setup() {
  PowerBits b = beginCoreSafely();
  screenTitle("step 2: 5 V output off?");
  show(b);
}

void loop() {
  // Re-check every 2 s, the same as the full firmware does. If something turned the output
  // back on, it is switched off again and logged.
  PowerBits b = readPowerBits();
  if (!b.safe()) {
    printPowerBits("found_on", b);
    b = forcePowerOutputsOff();
  }
  printPowerBits("poll", b);
  show(b);
  delay(2000);
}
