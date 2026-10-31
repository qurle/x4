#include "lvgl_port.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include "../hw/spi_lock.h"
#include "x4/log.h"

namespace x4core {
namespace display {
namespace {

constexpr int32_t kW = 320;
constexpr int32_t kH = 240;
constexpr int32_t kBufLines = 40;

lv_display_t* g_disp = nullptr;
lv_indev_t* g_touch = nullptr;
Stats g_stats;
uint32_t g_frames = 0;
uint32_t g_windowStart = 0;
int64_t g_renderStart = 0;
uint32_t g_windowMax = 0;
TouchSample g_sample{false, 0, 0};

uint32_t tickCb() { return (uint32_t)millis(); }

void flushCb(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  const int32_t w = lv_area_get_width(a);
  const int32_t h = lv_area_get_height(a);
  {
    SpiLock lock;
    M5.Display.startWrite();
    M5.Display.setAddrWindow(a->x1, a->y1, w, h);
    // LVGL renders native little-endian RGB565; writePixels(uint16_t*, len, swap=true) converts
    // to the panel's big-endian order.
    M5.Display.writePixels(reinterpret_cast<const uint16_t*>(px), w * h, true);
    M5.Display.endWrite();
  }
  g_stats.flushes++;
  lv_display_flush_ready(d);
}

void touchCb(lv_indev_t*, lv_indev_data_t* data) {
  data->state = g_sample.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->point.x = g_sample.x;
  data->point.y = g_sample.y;
}

void renderStart(lv_event_t*) { g_renderStart = esp_timer_get_time(); }

void renderReady(lv_event_t*) {
  uint32_t us = (uint32_t)(esp_timer_get_time() - g_renderStart);
  g_stats.lastFrameUs = us;
  if (us > g_windowMax) g_windowMax = us;
  g_frames++;
  uint32_t now = millis();
  if (now - g_windowStart >= 1000) {
    g_stats.fps = g_frames * 1000.0f / (float)(now - g_windowStart);
    g_stats.maxFrameUs = g_windowMax;
    g_frames = 0;
    g_windowMax = 0;
    g_windowStart = now;
  }
}

}  // namespace

void begin(uint8_t brightness) {
  lv_init();
  lv_tick_set_cb(tickCb);
  g_disp = lv_display_create(kW, kH);
  const size_t bytes = kW * kBufLines * sizeof(uint16_t);
  void* b1 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* b2 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!b1 || !b2) {
    X4_LOG_ERR(Error, "ui", "buffers", x4::Err::INTERNAL, "bytes=%u", (unsigned)bytes);
  }
  lv_display_set_buffers(g_disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(g_disp, flushCb);
  lv_display_add_event_cb(g_disp, renderStart, LV_EVENT_RENDER_START, nullptr);
  lv_display_add_event_cb(g_disp, renderReady, LV_EVENT_RENDER_READY, nullptr);

  g_touch = lv_indev_create();
  lv_indev_set_type(g_touch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(g_touch, touchCb);
  setBrightness(brightness);
  g_windowStart = millis();
  X4_LOGI("ui", "lvgl_init", "lvgl=%d.%d.%d buf_lines=%ld buf_bytes=%u", LVGL_VERSION_MAJOR,
          LVGL_VERSION_MINOR, LVGL_VERSION_PATCH, (long)kBufLines, (unsigned)bytes);
}

void setBrightness(uint8_t b) { M5.Display.setBrightness(b); }

uint32_t handle() {
  // Sample touch once per loop (M5.update() must have run).
  auto t = M5.Touch.getDetail();
  g_sample.pressed = t.isPressed();
  if (g_sample.pressed) {
    g_sample.x = t.x;
    g_sample.y = t.y;
  }
  return lv_timer_handler();
}

const Stats& stats() { return g_stats; }
lv_display_t* disp() { return g_disp; }
lv_indev_t* touch() { return g_touch; }
TouchSample touchSample() { return g_sample; }

}  // namespace display
}  // namespace x4core
