// Shared by every Core step. Three things only: pin numbers, keeping the flash pin LOW, and
// switching the Core's 5 V outputs off (and checking they really are off).
// Facts and sources: core/include/board_pins.h, docs/POWER.md, docs/HARDWARE.md.
#pragma once
#include <M5Unified.h>
#include <driver/gpio.h>

namespace pins {
constexpr int kButton = 1;   // Port A white, Mini Button, LOW when pressed
constexpr int kFlashEn = 2;  // Port A yellow, Flashlight EN, HIGH = light. Must idle LOW.
constexpr int kSdCs = 4;     // microSD chip select (shares the LCD's SPI bus)
constexpr int kSpiSck = 36, kSpiMiso = 35, kSpiMosi = 37;
}  // namespace pins

// The flash pin goes LOW before setup() even runs (C++ static init), so a Flashlight that is
// already wired can never light up while the Core boots. Every Core step gets this for free.
// Uses the IDF gpio calls because Arduino's pinMode is not ready that early.
inline void flashPinLow() {
  gpio_reset_pin((gpio_num_t)pins::kFlashEn);
  gpio_set_direction((gpio_num_t)pins::kFlashEn, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode((gpio_num_t)pins::kFlashEn, GPIO_PULLDOWN_ONLY);
  gpio_set_level((gpio_num_t)pins::kFlashEn, 0);
}
struct FlashPinLowAtBoot {
  FlashPinLowAtBoot() { flashPinLow(); }
};
inline FlashPinLowAtBoot g_flashPinLowAtBoot __attribute__((init_priority(101)));

// AW9523B I/O expander (I2C 0x58) bits that route 5 V. 1 = the Core pushes 5 V out.
//   P0 bit 1 BUS_OUT_EN: 5 V out to Port A / M-Bus   P0 bit 5 USB_OTG_EN: 5 V out of the USB-C
//   P1 bit 7 BOOST_EN: the boost converter that makes that 5 V
struct PowerBits {
  bool readOk = false;
  bool busOut = false, usbOtg = false, boost = false;
  int vbusMv = 0;  // voltage the Core sees on its 5 V input (USB or Port A)
  bool safe() const { return readOk && !busOut && !usbOtg; }
};

inline PowerBits readPowerBits() {
  PowerBits b;
  uint8_t p0 = 0, p1 = 0;
  b.readOk = M5.In_I2C.readRegister(0x58, 0x02, &p0, 1, 400000) &&
             M5.In_I2C.readRegister(0x58, 0x03, &p1, 1, 400000);
  b.busOut = p0 & (1u << 1);
  b.usbOtg = p0 & (1u << 5);
  b.boost = p1 & (1u << 7);
  b.vbusMv = M5.Power.getVBUSVoltage();
  return b;
}

// Turn both 5 V outputs off, then read back. Call after M5.begin and every couple of seconds.
inline PowerBits forcePowerOutputsOff() {
  M5.Power.setExtOutput(false);  // BUS_OUT_EN = 0 (and BOOST_EN = 0)
  M5.Power.setUsbOutput(false);  // USB_OTG_EN = 0
  return readPowerBits();
}

inline void printPowerBits(const char* when, const PowerBits& b) {
  Serial.printf("power when=%s read_ok=%d bus_out_en=%d usb_otg_en=%d boost_en=%d vbus_mv=%d safe=%d\n",
                when, b.readOk, b.busOut, b.usbOtg, b.boost, b.vbusMv, b.safe());
}

// M5.begin with the one setting x4cam must never get wrong: output_power = false.
// M5Unified's default is true, which on the CoreS3 turns the 5 V output on. The rig feeds the
// Core THROUGH Port A, so that output must stay off (docs/POWER.md).
inline PowerBits beginCoreSafely() {
  auto cfg = M5.config();
  cfg.output_power = false;
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  flashPinLow();  // again, in case M5.begin touched Port A
  PowerBits b = forcePowerOutputsOff();
  printPowerBits("boot", b);
  return b;
}

// Small screen helpers so the step sketches stay about the step.
inline void screenTitle(const char* title) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setFont(&fonts::FreeSans12pt7b);
  M5.Display.drawString(title, 8, 8);
  M5.Display.setFont(&fonts::Font2);
}

inline void screenLine(int row, uint16_t color, const char* fmt, ...) {
  char buf[64];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  int y = 44 + row * 20;
  M5.Display.fillRect(0, y, 320, 20, TFT_BLACK);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.drawString(buf, 8, y);
}
