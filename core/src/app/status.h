// One snapshot of everything the UI, the web API and the diagnostics screen show. The app loop
// fills it every iteration; other tasks read a copy under a mutex (status::get()).
#pragma once
#include <stdint.h>

#include "x4/protocol.h"
#include "x4/shoot.h"

namespace x4core {

struct CamView {
  bool registered;
  bool online;
  char ip[16];
  char fw[16];
  uint16_t sensorPid;
  uint8_t hwRev;
  int16_t tempC;
  int8_t rssi;
  uint32_t heap;
  uint32_t psram;
  uint8_t state;        // x4::proto::CamState
  uint32_t lastHbAgeMs;
  int64_t clockRttUs;
  // current/last shot
  bool expected, acked, ok, stored, failed;
  uint32_t bytes, xferBytes, xferTotal;
  int64_t capOffsetUs;  // capture time minus scheduled time
  uint32_t capMs;
};

struct Status {
  // app
  x4::shoot::State state;
  uint8_t faults;
  uint32_t shotId;      // current/last
  uint32_t shotsTaken;  // persistent counter
  bool shooting;
  int camsOnline;
  CamView cam[x4::proto::kMaxCams];
  // last finished shot
  uint32_t lastShotId;
  uint32_t lastTotalMs;
  uint32_t lastCapturedMs;  // shutter -> all captured
  int64_t lastSkewUs;
  int lastStored, lastExpected;
  // flash
  bool flashEnabled, flashArmed, flashReady;
  uint32_t flashReadyInMs, flashPulses;
  uint8_t flashChargePct;  // 0..100 through the cooldown, 100 = ready (footer animates it)
  // video (not implemented yet: always false; the footer already shows the state)
  bool recording;
  uint32_t recordMs;
  // power / system
  bool powerSafe, busOutEn, usbOtgEn;
  int32_t vbusMv;
  float tempC;
  bool hot, overheat;
  uint32_t heap, heapMin, psram;
  const char* resetReason;
  uint32_t bootCount, brownouts;
  uint32_t uptimeMs;
  // sd
  bool sdMounted;
  uint32_t sdFreeMb, sdTotalMb;
  float sdWriteKBps, sdReadKBps;
  // ui / net
  float fps;
  uint32_t frameMaxUs;
  float previewFps;
  bool previewOn;
  int apStations;
  bool buttonDown;
  // settings tiles (config mirror for the UI)
  uint8_t uiOrient;    // 0 horizontal, 1 portrait
  uint8_t uiEffect;    // 0 normal, 1 dithered
  uint16_t camDelayMs;
};

namespace status {
void publish(const Status& s);
Status get();
}  // namespace status

}  // namespace x4core
