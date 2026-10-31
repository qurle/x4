// Unit CamS3-5MP pin map. VERIFIED: M5 docs "Unit CamS3-5MP" pin table and schematic
// SCH_UNIT_CAMS3_V1.0 (M5Stack knowledge base, read 2026-10-31).
#pragma once

namespace x4cam {
namespace pins {

// Camera DVP (sensor module: MEGA_CCM at SCCB 0x1F; M5 docs call it "PY260")
constexpr int kSiod = 17;   // SCCB data
constexpr int kSioc = 41;   // SCCB clock
constexpr int kXclk = 11;
constexpr int kVsync = 42;
constexpr int kHref = 18;
constexpr int kPclk = 12;
constexpr int kY2 = 6, kY3 = 15, kY4 = 16, kY5 = 7, kY6 = 5, kY7 = 10, kY8 = 4, kY9 = 13;
constexpr int kReset = 21;
constexpr int kPwdn = -1;   // PWDN is tied to GND through R5 10k on the board

// Blue LED: GPIO14 -> R7 1k -> LED -> +3V, so LOW = on.
constexpr int kLed = 14;
constexpr bool kLedActiveLow = true;

// Grove HY2.0-4P: yellow = G20 (USB D+), white = G19 (USB D-), red = 5 V, black = GND.
// These are the ESP32-S3 native USB pins: never wire them to the rail. Cap/tape their ends.
constexpr int kUsbDp = 20;
constexpr int kUsbDm = 19;

// Download mode: short G0 (header P2 pin 3) to GND before power-up (M5 docs).
constexpr int kBoot = 0;

}  // namespace pins
}  // namespace x4cam
