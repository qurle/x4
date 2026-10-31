// Camera sensor control for the Unit CamS3-5MP: init, identity checks, capture, shot holding.
// All sensor access goes through one mutex (shooter task, HTTP handlers and the bench share it).
#pragma once
#include <esp_camera.h>
#include <stdint.h>

#include "x4/config.h"
#include "x4/protocol.h"

namespace x4cam {
namespace cam {

// MEGA_CCM product id (esp32-camera sensors/mega_ccm.c). The capture path refuses other sensors.
constexpr uint16_t kExpectedPid = 0x039E;

struct Identity {
  bool initOk = false;
  uint16_t pid = 0;
  uint8_t hwRev = 0;      // register 0x0200 read before init: 0x01 new, 0xFF old (M5 guide)
  uint8_t fwVer = 0;      // register 0x0002 (FIRMWARE_VER)
  const char* sensorName = "?";
  bool sensorOk = false;  // pid matches
  int initErr = 0;        // esp_err_t
};

// Probes the module's hardware revision over SCCB before esp_camera_init (M5's method).
uint8_t probeHwRev();
bool begin(const x4::cfg::Config& c);
const Identity& identity();

// Maps cfg cam_res (0..5) to framesize_t and a name.
framesize_t frameSizeFor(int32_t res);
const char* resName(int32_t res);

// Applies sensor options (quality, flip, mirror). Locks.
void applyConfig(const x4::cfg::Config& c);

// One frame at `res`, switching the sensor size if needed. Caller must release().
struct Frame {
  camera_fb_t* fb = nullptr;
  int64_t startUs = 0;  // frame start (driver timestamp, local esp_timer)
  uint32_t grabMs = 0;  // time spent waiting in fb_get
  bool jpegOk = false;  // starts FFD8 and ends FFD9
};
bool grab(int32_t res, Frame* out);
void release(Frame& f);

// Scheduled capture for a shot: waits until localAtUs, takes the frame whose start is closest
// to it (latest-before vs first-after), keeps it HELD for GET /shot.
struct ShotResult {
  bool ok;
  uint32_t shot;
  int64_t rxLocalUs;
  int64_t capLocalUs;
  uint32_t bytes;
  uint32_t capMs;
  uint16_t err;
};
ShotResult shoot(uint32_t shot, int64_t localAtUs, int32_t res, int64_t rxLocalUs);

// Held shot access (HTTP task). take() returns the fb and keeps it locked until done().
camera_fb_t* takeHeld(uint32_t shot);
void doneHeld(bool sent);
uint32_t heldShot();
void expireHeld(uint32_t nowMs, uint32_t holdMs);

// Stage 0 bench: captures at every supported size, logs a table.
void bench();

x4::proto::CamState state();
uint16_t switchMs();  // slowest resolution switch measured so far (ms)
void setState(x4::proto::CamState s);

}  // namespace cam
}  // namespace x4cam
