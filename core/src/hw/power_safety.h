// Core 5 V power-direction safety (docs/POWER.md section "Firmware rules").
//
// The x4 Core is powered FROM the rail through Port A's 5 V pin (BUS_OUT node). It must never
// drive that node itself: BUS_OUT_EN (AW9523B P0_1) and USB_OTG_EN (P0_5) stay 0 at all times.
// M5.begin() is called with cfg.output_power = false; this module then reads the AW9523B back,
// forces both bits off if they are set, logs the result in the boot banner, and re-checks every
// couple of seconds in case any library flips them later.
#pragma once
#include <stdint.h>

namespace x4core {
namespace power {

struct State {
  bool readOk = false;
  bool busOutEn = false;  // Core drives 5 V onto Port A / M-Bus (MUST be false)
  bool usbOtgEn = false;  // Core drives 5 V onto USB-C (MUST be false)
  bool boostEn = false;   // SY7088 boost running (harmless while BUS_OUT_EN=0, but we keep it off)
  uint8_t p0 = 0, p1 = 0;
  int32_t vbusMv = -1;    // AXP2101 VBUS measurement = the 5 V rail as seen by the Core
};

// Reads the current state (no changes).
State read();
// Reads, forces BUS_OUT_EN/USB_OTG_EN/BOOST_EN off if needed, logs, returns the final state.
State enforce(const char* when);
// Call from the loop; re-checks every periodMs.
void poll(uint32_t nowMs, uint32_t periodMs = 2000);
const State& last();
bool safe();  // readOk && !busOutEn && !usbOtgEn

}  // namespace power
}  // namespace x4core
