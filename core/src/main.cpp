// x4 Core firmware entry point. setup()/loop() only; everything else lives in app/.
// Build flavours (core/platformio.ini): X4_APP_FULL (env core) and X4_APP_HELLO (env core-hello).
#include <Arduino.h>

#include "app/app.h"
#include "hw/flash.h"

void setup() {
  x4core::flash::earlyOff();  // the very first thing: flash EN low (also done in a static ctor)
#if defined(X4_APP_HELLO)
  x4core::app::setup(x4core::app::Mode::Hello);
#else
  x4core::app::setup(x4core::app::Mode::Full);
#endif
}

void loop() { x4core::app::loop(); }
