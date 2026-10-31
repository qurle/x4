// Registry of the four cameras as the Core sees them. Written by the UDP and web tasks, read by
// the app/UI loop; guarded by one mutex. Snapshot() copies everything out for lock-free reading.
#pragma once
#include <IPAddress.h>
#include <stdint.h>

#include "x4/protocol.h"

namespace x4core {
namespace cams {

struct Cam {
  bool registered = false;  // POST /register received since boot
  bool online = false;      // heartbeat within hb_offline_ms
  uint32_t ip = 0;          // IPv4, network order as IPAddress uses
  char fw[16] = "";
  char git[16] = "";
  char mac[18] = "";
  uint16_t sensorPid = 0;
  uint8_t hwRev = 0;        // 0x0200 register: 1 = new hardware revision
  uint32_t regAtMs = 0;
  uint32_t lastHbMs = 0;
  uint32_t hbCount = 0;
  x4::proto::Heartbeat hb{};
  int64_t clockRttUs = -1;  // reported by the camera (its sync quality)
  uint32_t offlineEvents = 0;
};

struct Snapshot {
  Cam cam[x4::proto::kMaxCams];
  int online() const;
};

void begin();
// From POST /register. Returns false if id is out of range.
bool registerCam(uint8_t id, uint32_t ip, const char* fw, const char* git, const char* mac,
                 uint16_t pid, uint8_t hwRev, uint32_t nowMs);
void onHeartbeat(const x4::proto::Heartbeat& hb, uint32_t ip, uint32_t nowMs);
// Marks cameras offline after timeoutMs. Returns true if anything changed.
bool sweep(uint32_t nowMs, uint32_t timeoutMs);
Snapshot snapshot();
uint32_t ipOf(uint8_t id);  // 0 if unknown
void onlineMask(bool out[x4::proto::kMaxCams]);

}  // namespace cams
}  // namespace x4core
