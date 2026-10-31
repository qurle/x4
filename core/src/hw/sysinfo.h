// Boot-time and runtime system facts: reset reason, boot counter (NVS), chip/PSRAM/flash info,
// temperature, heap. Shared shape with the camera firmware (camera/src/sysinfo.cpp).
#pragma once
#include <esp_system.h>
#include <stdint.h>

namespace x4core {
namespace sys {

struct Info {
  esp_reset_reason_t reset;
  const char* resetName;
  uint32_t bootCount;
  uint32_t brownouts;  // total brown-out resets seen (NVS)
  uint32_t chipRev;
  uint32_t cpuMhz;
  uint32_t flashBytes;
  uint32_t psramBytes;
  uint8_t mac[6];
};

void begin();  // reads reset reason, bumps counters in NVS
const Info& info();
const char* resetReasonName(esp_reset_reason_t r);
bool lastResetWasBrownout();
float chipTempC();  // ESP32-S3 internal sensor (die temperature)
uint32_t freeHeap();
uint32_t freePsram();
uint32_t minFreeHeap();
// Persistent shot counter (NVS). next() increments and stores.
uint32_t nextShotId();
uint32_t lastShotId();

}  // namespace sys
}  // namespace x4core
