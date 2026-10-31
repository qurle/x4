// Camera-side system services: identity, config, logging sinks, boot banner.
#pragma once
#include <stdint.h>

#include "x4/config.h"
#include "x4/ringlog.h"

namespace x4cam {

using Ring = x4::RingLog<64, 200>;

namespace sys {
void beginLog();          // serial + ring sinks, device name "camN"
Ring& ring();
void enableForward(bool on);  // WARN+ lines -> Core over UDP (netcam drains the queue)
bool popForward(char* out, size_t cap);
uint8_t camId();          // 1..4 (0 = unknown / hello)
void setCamId(uint8_t id);  // stored in NVS, overrides the build flag after reboot
const char* resetName();
bool lastResetBrownout();
uint32_t bootCount();
void banner();
float tempC();
}  // namespace sys

namespace conf {
void begin();
x4::cfg::Config& get();
const x4::cfg::Config& base();
// From /register response or /settings. persist=true writes NVS.
bool set(const char* name, int32_t v, bool persist);
}  // namespace conf

}  // namespace x4cam
