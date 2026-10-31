// LVGL 9 <-> M5GFX glue for the CoreS3 (320x240 ILI9342C over SPI, FT6336U touch).
// Pattern from earlier planning (UNVERIFIED on hardware until the board arrives):
//   flush:  M5.Display.setAddrWindow + writePixels (under SpiLock: the SD shares the bus)
//   touch:  M5.Touch.getDetail() -> LVGL pointer indev
//   tick:   lv_tick_set_cb(millis)
// Partial render buffers (2 x 320x40 px RGB565 = 50 KB) live in internal DMA-capable RAM.
#pragma once
#include <stdint.h>

#include "lvgl.h"

namespace x4core {
namespace display {

void begin(uint8_t brightness);
void setBrightness(uint8_t b);
// Run LVGL. Call from the UI loop only. Returns ms until LVGL wants to run again.
uint32_t handle();

struct Stats {
  float fps = 0;           // frames rendered per second (last 1 s window)
  uint32_t lastFrameUs = 0;
  uint32_t maxFrameUs = 0; // worst frame in the window
  uint32_t flushes = 0;
};
const Stats& stats();
lv_display_t* disp();
lv_indev_t* touch();

// Raw touch state as seen this loop (for swipe detection outside LVGL).
struct TouchSample {
  bool pressed;
  int16_t x, y;
};
TouchSample touchSample();

}  // namespace display
}  // namespace x4core
