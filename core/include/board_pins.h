// CoreS3-SE pin and bus facts used by the x4 Core firmware.
// Every line is tagged VERIFIED (with source) or ASSUMPTION. Full tables: docs/HARDWARE.md.
#pragma once
#include <stdint.h>

namespace x4core {
namespace pins {

// ---- Port A (the SE's only Grove port, red, on the side) --------------------------------------
// VERIFIED: M5 docs CoreS3-SE pin map "HY2.0-4P PORT.A: Black GND, Red 5V, Yellow G2, White G1"
// and SE schematic J3 GH2.0-4P "1:BUS_PA_SCL 2:BUS_PA_SDA 3:BUS_OUT 4:GND" (docs.m5stack.com,
// M5Stack knowledge base, read 2026-10-31).
// x4 wiring (ASSUMPTION, owner to confirm, docs/OPEN_QUESTIONS.md Q1): one split Grove cable:
//   red/black  -> +5V/GND rail (Core power input, see docs/POWER.md)
//   white  G1  -> Mini Button signal
//   yellow G2  -> Flashlight Unit EN (yellow)
constexpr int kShutterButton = 1;  // G1, input, active LOW (button shorts to GND)
constexpr int kFlashEn = 2;        // G2, output, active HIGH, must idle LOW
constexpr bool kShutterActiveLow = true;

// Fallback pins on the M5-Bus header (2.54 mm, back of the Core) if Port A G1/G2 turn out to have
// pull-ups that keep the flash EN high (bring-up step 3 measures this):
// VERIFIED M5 docs M5-Bus map: pin 22 = G7 (GPIO), pin 4 = G8 (PB_IN), pin 10 = G9 (PB_OUT).
constexpr int kFlashEnAlt = 7;
constexpr int kExtraButton1 = 8;  // future feature button (free on the SE: no Port B base)
constexpr int kExtraButton2 = 9;
constexpr int kEncoderA = 5;      // future scroll wheel (quadrature) on M5-Bus pin 8 = G5
constexpr int kEncoderB = 6;      // M5-Bus pin 21 = G6

// ---- internal buses (do not use for x4 wiring) ------------------------------------------------
// VERIFIED M5 docs: SPI MOSI G37, SCK G36, MISO/LCD-DC G35, LCD CS G3, TF CS G4.
// The microSD shares the LCD SPI bus -> SD writes and LVGL flushes must not overlap (spi_lock.h).
constexpr int kSdCs = 4;
constexpr int kSpiSck = 36;
constexpr int kSpiMosi = 37;
constexpr int kSpiMiso = 35;
// VERIFIED: internal I2C G12 SDA / G11 SCL: AXP2101 0x34, AW9523B 0x58, FT6336U 0x38, BM8563 0x51.
constexpr uint8_t kAw9523Addr = 0x58;
constexpr uint8_t kAxp2101Addr = 0x34;

// ---- AW9523B bits that control 5 V power direction --------------------------------------------
// VERIFIED: SE schematic "AW9523B P0_1=BUS_OUT_EN, P0_5=USB_OTG_EN, P1_7=BOOST_EN" and M5Unified
// 0.2.29 Power_Class.inl (_core_s3_bus_en = 0b00000010, _core_s3_usb_en = 0b00100000,
// BOOST_EN = port1 bit 7). Output registers: 0x02 (P0), 0x03 (P1).
constexpr uint8_t kAwRegOutP0 = 0x02;
constexpr uint8_t kAwRegOutP1 = 0x03;
constexpr uint8_t kAwRegCfgP0 = 0x04;  // 1 = input
constexpr uint8_t kAwBitBusOutEn = 1u << 1;
constexpr uint8_t kAwBitUsbOtgEn = 1u << 5;
constexpr uint8_t kAwBitBoostEn = 1u << 7;  // in P1

}  // namespace pins
}  // namespace x4core
