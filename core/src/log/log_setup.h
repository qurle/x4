// Core log sinks: serial (USB CDC), in-RAM ring (diagnostics screen + GET /logs) and an optional
// rotating file on the SD card. Format and policy: docs/LOGGING.md.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "x4/log.h"
#include "x4/ringlog.h"

namespace x4core {
namespace logsys {

using Ring = x4::RingLog<128, 200>;

void begin(const char* device);
Ring& ring();
// Lines arriving from cameras (already formatted by them) go to the same sinks unchanged.
void injectRemote(const char* line, size_t len, x4::log::Level lvl);
// SD sink: lines are queued here and written by the storage task (never blocks the caller).
void enableSdQueue(bool on);
bool popSdLine(char* out, size_t cap);  // storage task drains this

}  // namespace logsys
}  // namespace x4core
