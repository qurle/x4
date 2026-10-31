#include "power_safety.h"

#include <M5Unified.h>

#include "board_pins.h"
#include "x4/log.h"

namespace x4core {
namespace power {
namespace {

State g_last;
uint32_t g_lastPoll = 0;
constexpr uint32_t kI2cFreq = 400000;

}  // namespace

State read() {
  State s;
  uint8_t p0 = 0, p1 = 0;
  bool ok0 = M5.In_I2C.readRegister(pins::kAw9523Addr, pins::kAwRegOutP0, &p0, 1, kI2cFreq);
  bool ok1 = M5.In_I2C.readRegister(pins::kAw9523Addr, pins::kAwRegOutP1, &p1, 1, kI2cFreq);
  s.readOk = ok0 && ok1;
  s.p0 = p0;
  s.p1 = p1;
  s.busOutEn = p0 & pins::kAwBitBusOutEn;
  s.usbOtgEn = p0 & pins::kAwBitUsbOtgEn;
  s.boostEn = p1 & pins::kAwBitBoostEn;
  s.vbusMv = M5.Power.getVBUSVoltage();
  return s;
}

State enforce(const char* when) {
  State s = read();
  if (!s.readOk) {
    X4_LOG_ERR(Error, "power", "assert", x4::Err::POWER_STATE_UNKNOWN, "when=%s", when);
    g_last = s;
    return s;
  }
  bool fixed = false;
  if (s.busOutEn) {
    X4_LOG_ERR(Error, "power", "assert", x4::Err::POWER_BUS_OUT_ON, "when=%s p0=0x%02x", when, s.p0);
    M5.Power.setExtOutput(false);
    fixed = true;
  }
  if (s.usbOtgEn) {
    X4_LOG_ERR(Error, "power", "assert", x4::Err::POWER_OTG_ON, "when=%s p0=0x%02x", when, s.p0);
    M5.Power.setUsbOutput(false);
    fixed = true;
  }
  if (s.boostEn && !s.busOutEn) {
    // M5Unified's setExtOutput(false) also clears BOOST_EN; clear it directly if it is still set.
    M5.In_I2C.bitOff(pins::kAw9523Addr, pins::kAwRegOutP1, pins::kAwBitBoostEn, kI2cFreq);
    fixed = true;
  }
  if (fixed) s = read();
  g_last = s;
  X4_LOG(safe() ? x4::log::Level::Info : x4::log::Level::Error, "power", "state",
         "when=%s bus_out_en=%d usb_otg_en=%d boost_en=%d vbus_mv=%ld p0=0x%02x p1=0x%02x safe=%d",
         when, s.busOutEn, s.usbOtgEn, s.boostEn, (long)s.vbusMv, s.p0, s.p1, safe());
  return s;
}

void poll(uint32_t nowMs, uint32_t periodMs) {
  if ((uint32_t)(nowMs - g_lastPoll) < periodMs) return;
  g_lastPoll = nowMs;
  State s = read();
  if (!s.readOk || s.busOutEn || s.usbOtgEn) {
    enforce("poll");
  } else {
    g_last = s;
  }
}

const State& last() { return g_last; }
bool safe() { return g_last.readOk && !g_last.busOutEn && !g_last.usbOtgEn; }

}  // namespace power
}  // namespace x4core
