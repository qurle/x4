// Step 4: touch screen. Draw a dot wherever you touch; long-press the top bar to clear.
// Wiring: Core on USB.
// Pass: dots appear under your finger (corners too), coordinates match roughly 0..319 x 0..239.
#include "core_common.h"

void setup() {
  beginCoreSafely();
  screenTitle("step 4: touch (hold title = clear)");
}

void loop() {
  M5.update();
  auto t = M5.Touch.getDetail();
  if (t.isPressed()) {
    if (t.y < 36 && t.isHolding()) {
      screenTitle("step 4: touch (hold title = clear)");
    } else if (t.y >= 36) {
      M5.Display.fillCircle(t.x, t.y, 4, TFT_RED);
    }
    screenLine(9, TFT_WHITE, "x=%d y=%d", t.x, t.y);
  }
  if (t.wasPressed()) Serial.printf("touch x=%d y=%d\n", t.x, t.y);
  delay(10);
}
