#include "spi_lock.h"

namespace x4core {

SemaphoreHandle_t spiMutex() {
  static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
  return m;
}

}  // namespace x4core
