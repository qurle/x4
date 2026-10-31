#include "log_setup.h"

#include <Arduino.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

namespace x4core {
namespace logsys {
namespace {

Ring g_ring;
QueueHandle_t g_sdQueue = nullptr;
volatile bool g_sdOn = false;
constexpr size_t kSdLine = 200;

uint32_t clockMs() { return (uint32_t)millis(); }

void serialSink(const char* line, size_t len, x4::log::Level, void*) {
  // HWCDC: zero TX timeout (set in begin) so a missing USB host never blocks the caller.
  Serial.write((const uint8_t*)line, len);
  Serial.write('\n');
}

void ringSink(const char* line, size_t len, x4::log::Level, void*) { g_ring.append(line, len); }

void sdSink(const char* line, size_t len, x4::log::Level, void*) {
  if (!g_sdOn || !g_sdQueue) return;
  char buf[kSdLine];
  size_t n = len < kSdLine - 1 ? len : kSdLine - 1;
  memcpy(buf, line, n);
  buf[n] = 0;
  xQueueSend(g_sdQueue, buf, 0);  // drop if full: logging must not slow the system down
}

// Route ESP-IDF / library ESP_LOGx output into the same structured stream:
//   "E (1234) wifi: something" -> t=.. lvl=ERROR dev=core mod=idf evt=wifi msg=something
int idfVprintf(const char* fmt, va_list ap) {
  char raw[200];
  int n = vsnprintf(raw, sizeof(raw), fmt, ap);
  if (n <= 0) return n;
  char* s = raw;
  // strip ANSI colour codes
  if (s[0] == '\x1b') {
    char* m = strchr(s, 'm');
    if (m) s = m + 1;
  }
  x4::log::Level lvl = x4::log::Level::Info;
  switch (s[0]) {
    case 'E': lvl = x4::log::Level::Error; break;
    case 'W': lvl = x4::log::Level::Warn; break;
    case 'I': lvl = x4::log::Level::Info; break;
    case 'D': lvl = x4::log::Level::Debug; break;
    case 'V': lvl = x4::log::Level::Trace; break;
    default: break;
  }
  char tag[24] = "log";
  char* msg = s;
  char* close = strchr(s, ')');
  if (close && close[1] == ' ') {
    char* colon = strchr(close + 2, ':');
    if (colon) {
      size_t tl = (size_t)(colon - (close + 2));
      if (tl >= sizeof(tag)) tl = sizeof(tag) - 1;
      memcpy(tag, close + 2, tl);
      tag[tl] = 0;
      msg = colon + 1;
      while (*msg == ' ') msg++;
    }
  }
  // trim trailing newline / colour reset
  size_t ml = strlen(msg);
  while (ml && (msg[ml - 1] == '\n' || msg[ml - 1] == '\r')) msg[--ml] = 0;
  char* esc = strchr(msg, '\x1b');
  if (esc) *esc = 0;
  char safeTag[24], safeMsg[160];
  x4::log::sanitizeValue(safeTag, sizeof(safeTag), tag);
  x4::log::sanitizeValue(safeMsg, sizeof(safeMsg), msg);
  if (X4_LOG_ENABLED(lvl)) x4::log::write(lvl, "idf", safeTag, "msg=%s", safeMsg);
  return n;
}

}  // namespace

void begin(const char* device) {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
  Serial.setTxTimeoutMs(0);
#endif
  x4::log::setDevice(device);
  x4::log::setClock(clockMs);
  x4::log::addSink(serialSink, nullptr, x4::log::Level::Trace);
  x4::log::addSink(ringSink, nullptr, x4::log::Level::Trace);
  g_sdQueue = xQueueCreate(32, kSdLine);
  x4::log::addSink(sdSink, nullptr, x4::log::Level::Info);
  esp_log_set_vprintf(idfVprintf);
}

Ring& ring() { return g_ring; }

void injectRemote(const char* line, size_t len, x4::log::Level lvl) {
  serialSink(line, len, lvl, nullptr);
  ringSink(line, len, lvl, nullptr);
  sdSink(line, len, lvl, nullptr);
}

void enableSdQueue(bool on) { g_sdOn = on; }

bool popSdLine(char* out, size_t cap) {
  if (!g_sdQueue || cap < kSdLine) return false;
  return xQueueReceive(g_sdQueue, out, 0) == pdTRUE;
}

}  // namespace logsys
}  // namespace x4core
