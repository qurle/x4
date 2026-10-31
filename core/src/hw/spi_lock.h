// The microSD and the LCD share one SPI bus on the CoreS3 (VERIFIED: M5 docs, G35/G36/G37).
// LVGL flushes (UI task) and SD reads/writes (transfer + web tasks) must never overlap, so every
// bus user takes this recursive mutex. Keep each hold short (one flush, one <=16 KB chunk).
#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace x4core {

SemaphoreHandle_t spiMutex();

class SpiLock {
 public:
  SpiLock() { xSemaphoreTakeRecursive(spiMutex(), portMAX_DELAY); }
  ~SpiLock() { xSemaphoreGiveRecursive(spiMutex()); }
  SpiLock(const SpiLock&) = delete;
  SpiLock& operator=(const SpiLock&) = delete;
};

}  // namespace x4core
