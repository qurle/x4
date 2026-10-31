// Physical inputs -> logical actions (shared/x4/input.h). Polled from the UI loop (~200 Hz).
//
// Drivers today: shutter button (Port A G1), touch swipes, web "virtual" button.
// Stubs for later: extra feature buttons (M5-Bus G8/G9) and a scroll wheel / encoder
// (quadrature on G5/G6, or an I2C encoder unit). To add a driver: implement Driver, register it
// in inputs.cpp, give it a Source, and add default bindings in shared/src/input.cpp. No UI change.
#pragma once
#include <stdint.h>

#include "x4/config.h"
#include "x4/input.h"

namespace x4core {
namespace inputs {

class Driver {
 public:
  virtual ~Driver() = default;
  virtual const char* name() const = 0;
  virtual bool begin() = 0;
  // Emit gestures for this poll via emit(Source, Gesture, value).
  virtual void poll(uint32_t nowMs) = 0;
};

void begin(const x4::cfg::Config& c);
void applyConfig(const x4::cfg::Config& c);
void poll(uint32_t nowMs);
// Pops the next logical action (false if none).
bool next(x4::input::Event* out);
// Inject from other sources (web page, UI widgets). Thread-safe.
void inject(x4::input::Source s, x4::input::Gesture g, int16_t value = 0);
x4::input::InputMap& map();
bool shutterPressed();  // raw debounced level, for the bring-up screen

}  // namespace inputs
}  // namespace x4core
