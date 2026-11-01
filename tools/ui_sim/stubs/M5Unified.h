// UI simulator stand-in for M5Unified: only what core/src/ui/ui.cpp uses. Not device code.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <initializer_list>

uint32_t millis();
uint32_t micros();

struct SimSpeaker {
  bool isEnabled() const { return false; }  // the simulator is silent
  void tone(uint16_t, uint32_t, int = -1, bool = true) {}
};
struct SimDisplay {};
struct SimM5 {
  SimSpeaker Speaker;
  SimDisplay Display;
};
extern SimM5 M5;

// The device decodes preview JPEGs into a 16-bit sprite. The simulator hands ui.cpp raw
// big-endian RGB565 frames instead (same layout as the decoded sprite), so drawJpg() copies.
class M5Canvas {
 public:
  explicit M5Canvas(SimDisplay*) {}
  ~M5Canvas() { free(buf_); }
  void setPsram(bool) {}
  void setColorDepth(int) {}
  bool createSprite(int w, int h) {
    buf_ = (uint16_t*)calloc((size_t)w * h, 2);
    w_ = w;
    h_ = h;
    return buf_ != nullptr;
  }
  void* getBuffer() { return buf_; }
  bool drawJpg(const uint8_t* data, uint32_t len, int, int, int, int, int, int, float, float) {
    if (!buf_ || len != (uint32_t)w_ * h_ * 2) return false;
    memcpy(buf_, data, len);
    return true;
  }

 private:
  uint16_t* buf_ = nullptr;
  int w_ = 0, h_ = 0;
};
