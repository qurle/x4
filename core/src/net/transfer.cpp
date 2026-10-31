// Photo transfer: the Core PULLS each held JPEG from the cameras (GET /shot?id=N) with up to
// kWorkers parallel workers (cfg xfer_parallel limits how many the app starts at once) and
// streams it to the SD card in 16 KB chunks. Pull keeps the Core in control of concurrency, so
// SD writes and WiFi never fight more than configured. Measured numbers: docs/MEASUREMENTS.md.
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "../storage/sd_store.h"
#include "cams.h"
#include "net.h"
#include "x4/log.h"

namespace x4core {
namespace net {
namespace {

constexpr int kWorkers = 4;

struct Job {
  uint8_t cam;
  uint32_t shot;
};

struct Worker {
  QueueHandle_t q;
  volatile bool busy;
  XferProgress prog;
};

Worker g_w[kWorkers];
QueueHandle_t g_results = nullptr;
volatile uint32_t g_timeoutMs = 8000;

bool endsWithJpegEoi(const uint8_t* tail2) { return tail2[0] == 0xFF && tail2[1] == 0xD9; }

XferResult run(Worker& w, const Job& j, uint8_t* buf, size_t cap) {
  XferResult r{j.cam, j.shot, false, 0, 0, 0};
  uint32_t t0 = millis();
  uint32_t ip = cams::ipOf(j.cam);
  if (!ip || !sd::mounted() || !sd::ensureShotDir(j.shot)) return r;
  char url[64];
  IPAddress a(ip);
  snprintf(url, sizeof(url), "http://%u.%u.%u.%u/shot?id=%lu", a[0], a[1], a[2], a[3], (unsigned long)j.shot);
  HTTPClient http;
  http.setTimeout(g_timeoutMs);
  http.setConnectTimeout(2000);
  http.setReuse(false);
  if (!http.begin(url)) return r;
  r.httpCode = http.GET();
  if (r.httpCode != 200) {
    http.end();
    r.ms = millis() - t0;
    return r;
  }
  int total = http.getSize();
  w.prog = XferProgress{j.cam, j.shot, 0, total > 0 ? (uint32_t)total : 0, true};
  File f = sd::openCamFile(j.shot, j.cam);
  if (!f) {
    http.end();
    return r;
  }
  WiFiClient* s = http.getStreamPtr();
  uint8_t tail[2] = {0, 0};
  bool ok = true;
  uint32_t got = 0, lastData = millis();
  while (ok && (total < 0 || got < (uint32_t)total)) {
    size_t avail = s->available();
    if (!avail) {
      if (!s->connected() && total < 0) break;
      if (millis() - lastData > g_timeoutMs) {
        ok = false;
        break;
      }
      vTaskDelay(1);
      continue;
    }
    int n = s->read(buf, avail < cap ? avail : cap);
    if (n <= 0) continue;
    lastData = millis();
    ok = sd::writeChunk(f, buf, (size_t)n);
    if (n >= 2) {
      tail[0] = buf[n - 2];
      tail[1] = buf[n - 1];
    } else {
      tail[0] = tail[1];
      tail[1] = buf[0];
    }
    got += (uint32_t)n;
    w.prog.bytes = got;
  }
  sd::closeFile(f);
  http.end();
  r.bytes = got;
  r.ms = millis() - t0;
  r.ok = ok && got > 0 && (total < 0 || got == (uint32_t)total) && endsWithJpegEoi(tail);
  w.prog.active = false;
  return r;
}

void workerTask(void* arg) {
  Worker& w = *static_cast<Worker*>(arg);
  // PSRAM is fine for this buffer: it is a copy target, not a DMA source.
  uint8_t* buf = (uint8_t*)heap_caps_malloc(sd::kChunk, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  Job j;
  for (;;) {
    if (xQueueReceive(w.q, &j, portMAX_DELAY) != pdTRUE) continue;
    XferResult r = run(w, j, buf, sd::kChunk);
    float kBps = r.ms ? (r.bytes / 1024.0f) / (r.ms / 1000.0f) : 0;
    if (r.ok) {
      X4_LOGI("xfer", "done", "cam=%u shot=%lu bytes=%lu ms=%lu kBps=%.0f", r.cam, (unsigned long)r.shot,
              (unsigned long)r.bytes, (unsigned long)r.ms, kBps);
    } else {
      X4_LOGW("xfer", "fail", "cam=%u shot=%lu http=%d bytes=%lu ms=%lu", r.cam, (unsigned long)r.shot,
              r.httpCode, (unsigned long)r.bytes, (unsigned long)r.ms);
    }
    xQueueSend(g_results, &r, portMAX_DELAY);
    w.busy = false;
  }
}

}  // namespace

void applyTransferConfig(const x4::cfg::Config& c) { g_timeoutMs = (uint32_t)c.xfer_timeout_ms; }

void beginTransfer(const x4::cfg::Config& c) {
  applyTransferConfig(c);
  g_results = xQueueCreate(8, sizeof(XferResult));
  for (int i = 0; i < kWorkers; i++) {
    g_w[i].q = xQueueCreate(1, sizeof(Job));
    g_w[i].busy = false;
    g_w[i].prog = XferProgress{0, 0, 0, 0, false};
    char name[12];
    snprintf(name, sizeof(name), "xfer%d", i);
    xTaskCreatePinnedToCore(workerTask, name, 6144, &g_w[i], 4, nullptr, 0);
  }
  X4_LOGI("xfer", "init", "workers=%d chunk=%u timeout_ms=%lu", kWorkers, (unsigned)sd::kChunk,
          (unsigned long)g_timeoutMs);
}

bool startTransfer(uint8_t cam, uint32_t shot) {
  for (auto& w : g_w) {
    if (!w.busy) {
      w.busy = true;
      Job j{cam, shot};
      xQueueSend(w.q, &j, 0);
      return true;
    }
  }
  return false;
}

bool nextXferResult(XferResult* out) {
  return g_results && xQueueReceive(g_results, out, 0) == pdTRUE;
}

void progress(XferProgress out[x4::proto::kMaxCams]) {
  for (int i = 0; i < x4::proto::kMaxCams; i++) out[i] = XferProgress{(uint8_t)(i + 1), 0, 0, 0, false};
  for (auto& w : g_w) {
    if (w.prog.active && w.prog.cam >= 1 && w.prog.cam <= x4::proto::kMaxCams) out[w.prog.cam - 1] = w.prog;
  }
}

}  // namespace net
}  // namespace x4core
