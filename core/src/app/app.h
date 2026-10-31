// The Core application: boot sequence, main loop and the glue between the shooting state machine
// (shared/x4/shoot.h), the network, the SD card, the flash and the UI. main.cpp calls only these.
#pragma once
#include <stdint.h>

namespace x4core {
namespace app {

enum class Mode : uint8_t { Full, Hello };

void setup(Mode m);
void loop();

// Requests from the UI (on the UI loop) or the web task (thread-safe: queued).
enum class Request : uint8_t {
  Shutter,
  FlashArm,
  FlashDisarm,
  FlashTest,      // bring-up: one 50 ms pulse (requires FlashArm first)
  SdBench,
  PreviewOn,
  PreviewOff,
  Reboot,
};
void request(Request r);

}  // namespace app
}  // namespace x4core
