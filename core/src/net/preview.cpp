// Stage 4 live preview: a background task polls one camera for small JPEGs and keeps the newest
// in a PSRAM buffer. The UI decodes it (software TJpgDec via M5GFX) into an LVGL image.
// Paused automatically while shooting (app: kPausePreview). Fallback if the Core cannot keep up:
// lower prev_fps, or disable preview (docs/UI.md "Live preview").
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "cams.h"
#include "net.h"
#include "x4/log.h"

namespace x4core {
namespace net {
namespace {

constexpr uint32_t kMaxJpeg = 64 * 1024;  // QVGA JPEG is ~10-25 KB (ASSUMPTION, measure)

volatile bool g_on = false;
volatile uint8_t g_cam = 1;
volatile uint8_t g_fps = 5;
uint8_t* g_buf = nullptr;
uint32_t g_len = 0;
uint32_t g_seq = 0;
SemaphoreHandle_t g_mu = nullptr;
float g_fpsMeasured = 0;
TaskHandle_t g_task = nullptr;

void previewTask(void*) {
  uint8_t* tmp = (uint8_t*)heap_caps_malloc(kMaxJpeg, MALLOC_CAP_SPIRAM);
  HTTPClient http;
  http.setReuse(true);
  uint32_t frames = 0, windowStart = millis();
  for (;;) {
    if (!g_on) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    uint32_t t0 = millis();
    uint32_t ip = cams::ipOf(g_cam);
    if (ip && tmp) {
      IPAddress a(ip);
      char url[64];
      snprintf(url, sizeof(url), "http://%u.%u.%u.%u/capture?res=0", a[0], a[1], a[2], a[3]);
      http.setTimeout(1500);
      if (http.begin(url)) {
        int code = http.GET();
        int size = http.getSize();
        if (code == 200 && size > 0 && (uint32_t)size <= kMaxJpeg) {
          WiFiClient* s = http.getStreamPtr();
          int got = 0;
          uint32_t deadline = millis() + 1500;
          while (got < size && millis() < deadline) {
            int n = s->read(tmp + got, size - got);
            if (n > 0) got += n; else vTaskDelay(1);
          }
          if (got == size) {
            xSemaphoreTake(g_mu, portMAX_DELAY);
            memcpy(g_buf, tmp, size);
            g_len = (uint32_t)size;
            g_seq++;
            xSemaphoreGive(g_mu);
            frames++;
          }
        } else {
          X4_LOG_EVERY(5000, x4::log::Level::Warn, "preview", "fetch", "cam=%u http=%d size=%d", g_cam, code, size);
        }
        http.end();
      }
    }
    uint32_t now = millis();
    if (now - windowStart >= 2000) {
      g_fpsMeasured = frames * 1000.0f / (now - windowStart);
      frames = 0;
      windowStart = now;
    }
    uint32_t period = 1000 / (g_fps ? g_fps : 1);
    uint32_t spent = millis() - t0;
    vTaskDelay(pdMS_TO_TICKS(spent < period ? period - spent : 1));
  }
}

}  // namespace

void setPreview(bool on, uint8_t cam, uint8_t fps) {
  if (!g_mu) {
    g_mu = xSemaphoreCreateMutex();
    g_buf = (uint8_t*)heap_caps_malloc(kMaxJpeg, MALLOC_CAP_SPIRAM);
    xTaskCreatePinnedToCore(previewTask, "preview", 6144, nullptr, 2, &g_task, 0);
  }
  if (g_on != on) X4_LOGI("preview", on ? "start" : "stop", "cam=%u fps=%u", cam, fps);
  g_cam = cam;
  g_fps = fps;
  g_on = on;
}

uint32_t takePreview(uint8_t* buf, uint32_t cap, uint32_t* seq) {
  if (!g_mu || !buf) return 0;
  uint32_t n = 0;
  xSemaphoreTake(g_mu, portMAX_DELAY);
  if (g_seq != *seq && g_len <= cap) {
    memcpy(buf, g_buf, g_len);
    n = g_len;
    *seq = g_seq;
  }
  xSemaphoreGive(g_mu);
  return n;
}

float previewFps() { return g_fpsMeasured; }

}  // namespace net
}  // namespace x4core
