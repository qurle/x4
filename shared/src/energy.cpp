#include "x4/energy.h"

namespace x4 {
namespace energy {

uint32_t estimateMw(const PowerModel& m, const Load& l) {
  return m.coreMw + (uint32_t)l.camsIdle * m.camIdleMw + (uint32_t)l.camsStream * m.camStreamMw +
         (uint32_t)l.camsBusy * m.camBusyMw;
}

float flashPulseMwh(const PowerModel& m, uint32_t pulseMs) {
  return (float)m.flashMw * (float)pulseMs / 3600000.0f;
}

void Meter::setCapacity(uint32_t capacityMwh, uint8_t effPct) {
  if (effPct > 100) effPct = 100;
  usable_ = (float)capacityMwh * (float)effPct / 100.0f;
}

void Meter::add(uint32_t dtMs, uint32_t powerMw) { used_ += (float)powerMw * (float)dtMs / 3600000.0f; }

int Meter::percent() const {
  if (usable_ <= 0) return 0;
  float left = 1.0f - used_ / usable_;
  if (left < 0) left = 0;
  if (left > 1) left = 1;
  return (int)(left * 100.0f + 0.5f);
}

float Meter::hoursLeft(uint32_t powerMw) const {
  if (powerMw == 0) return 0;
  float left = usable_ - used_;
  return left <= 0 ? 0 : left / (float)powerMw;
}

}  // namespace energy
}  // namespace x4
