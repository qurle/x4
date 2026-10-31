// Core networking: SoftAP, UDP (heartbeats, clock sync, trigger fan-out, ACKs, camera logs),
// photo transfer workers and the HTTP server (web debug page + API + /register).
// Every network job runs in its own FreeRTOS task; results reach the app through queues so the
// UI loop never blocks on the network. Protocol: docs/PROTOCOL.md.
#pragma once
#include <stdint.h>

#include "x4/config.h"
#include "x4/protocol.h"

namespace x4core {
namespace net {

// ---- SoftAP -------------------------------------------------------------------------------------
bool beginAp(const x4::cfg::Config& c);
int apStations();

// ---- UDP ----------------------------------------------------------------------------------------
bool beginUdp(const x4::cfg::Config& c);
// Fan-out: broadcast + unicast to every known camera, `repeat` times, from a high-priority task.
void sendShoot(const x4::proto::ShootCmd& cmd, uint8_t repeat);
void sendCmd(uint8_t cam, const char* cmd, const char* arg);  // cam 0 = broadcast
// App-side queue of incoming ACKs.
bool nextAck(x4::proto::ShotAck* out);

// ---- transfer -------------------------------------------------------------------------------------
struct XferResult {
  uint8_t cam;
  uint32_t shot;
  bool ok;
  uint32_t bytes;
  uint32_t ms;
  int httpCode;
};
void beginTransfer(const x4::cfg::Config& c);
void applyTransferConfig(const x4::cfg::Config& c);
bool startTransfer(uint8_t cam, uint32_t shot);  // false if no worker is free
bool nextXferResult(XferResult* out);
struct XferProgress {
  uint8_t cam;
  uint32_t shot;
  uint32_t bytes;
  uint32_t total;  // from Content-Length, 0 if unknown
  bool active;
};
void progress(XferProgress out[x4::proto::kMaxCams]);

// ---- preview (Stage 4) ---------------------------------------------------------------------------
// Polls GET http://cam/capture?res=0 (QVGA JPEG) from a background task while enabled.
void setPreview(bool on, uint8_t cam, uint8_t fps);
// Latest JPEG for the UI to decode: returns size (0 if none newer than *seq). Copies into buf.
uint32_t takePreview(uint8_t* buf, uint32_t cap, uint32_t* seq);
float previewFps();

// ---- web -----------------------------------------------------------------------------------------
bool beginWeb();

// ---- OTA (Stage 5) -------------------------------------------------------------------------------
void beginOta();
void pollOta();

}  // namespace net
}  // namespace x4core
