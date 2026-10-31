// Camera networking: WiFi STA (Core AP or, for cam-hello, home WiFi), registration, heartbeat +
// clock sync, trigger reception, shot ACKs, log forwarding, HTTP API (port 80) and MJPEG (81).
#pragma once
#include <stdint.h>

#include "x4/protocol.h"

namespace x4cam {
namespace net {

bool joinWifi();               // blocking with timeout; retries handled by poll()
void begin();                  // UDP + HTTP + tasks
void poll(uint32_t nowMs);     // reconnect, re-register, expire held shots, OTA
bool registered();
const x4::proto::ClockSync& clock();
int8_t rssi();

}  // namespace net
}  // namespace x4cam
