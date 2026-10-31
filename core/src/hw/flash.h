// Flashlight Unit driver (AW3641EDNR in flash mode, EN on the yellow wire).
//
// Safety layers, outermost first:
//  1. earlyOff(): pin driven LOW from a global constructor, before setup() runs.
//  2. FlashGuard (shared/safety.h): disabled by default, pulse cap <= 150 ms, cooldown >= 8 s,
//     refuses while too hot.
//  3. Pulse end is an esp_timer one-shot (not the loop), so a slow loop cannot stretch it.
//  4. poll() watchdog: if the pin reads HIGH past cap+10 ms it is forced LOW (err=104).
//  5. Shutdown handler drives LOW on esp_restart(); panics/watchdog resets put the pin in its reset
//     state (input) and the unit's own 10k pull-down on EN turns it off (VERIFIED: U152 schematic R1).
//  6. AW3641 hardware flash timeout (~220 ms typ.) if all of the above failed.
#pragma once
#include <stdint.h>

#include "x4/config.h"
#include "x4/safety.h"

namespace x4core {
namespace flash {

void earlyOff();                       // safe to call any time, even before Serial
void begin(const x4::cfg::Config& c);  // configures the guard from config
void applyConfig(const x4::cfg::Config& c);
// Fire one pulse. Returns the verdict; *grantedMs = actual pulse.
x4::safety::FlashGuard::Verdict fire(uint32_t wantMs, const char* reason, uint32_t* grantedMs);
void poll();                           // watchdog + bookkeeping, call from the loop
void setTooHot(bool hot);
void setEnabled(bool en);              // runtime arm/disarm (bring-up screen, config)
bool enabled();
bool ready();                          // enabled, not hot, not cooling down
uint32_t readyInMs();
uint32_t pulseCount();
int pin();

}  // namespace flash
}  // namespace x4core
