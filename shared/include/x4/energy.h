// Battery estimate by energy counting. The CoreS3-SE cannot measure current or read the power
// bank, so x4 integrates an ESTIMATED power draw over time (a power model per device state, from
// docs/MEASUREMENTS.md once measured) and subtracts it from the bank's usable energy.
// Accuracy is only as good as the model; the UI shows it as "~NN%". Reset when the bank is charged.
#pragma once
#include <stdint.h>

namespace x4 {
namespace energy {

struct PowerModel {
  uint32_t coreMw = 900;        // Core with screen on, AP up
  uint32_t camIdleMw = 750;     // camera online, idle
  uint32_t camStreamMw = 1000;  // camera streaming preview
  uint32_t camBusyMw = 1500;    // camera capturing / holding / uploading
  uint32_t flashMw = 5000;      // flash while EN is high (5 V x ~1 A)
};

struct Load {
  int camsIdle = 0;
  int camsStream = 0;
  int camsBusy = 0;
};

uint32_t estimateMw(const PowerModel& m, const Load& l);
// Energy of one flash pulse in mWh.
float flashPulseMwh(const PowerModel& m, uint32_t pulseMs);

class Meter {
 public:
  // capacityMwh: bank rating (5000 mAh x 3.7 V = 18500 mWh); effPct: conversion efficiency.
  void setCapacity(uint32_t capacityMwh, uint8_t effPct);
  void setUsed(float usedMwh) { used_ = usedMwh < 0 ? 0 : usedMwh; }
  void add(uint32_t dtMs, uint32_t powerMw);  // integrates power over dt
  void addEnergy(float mWh) { used_ += mWh; }
  void reset() { used_ = 0; }
  float usedMwh() const { return used_; }
  float usableMwh() const { return usable_; }
  int percent() const;                            // 0..100
  float hoursLeft(uint32_t powerMw) const;        // at the given draw

 private:
  float usable_ = 18500.0f * 0.8f;
  float used_ = 0;
};

}  // namespace energy
}  // namespace x4
