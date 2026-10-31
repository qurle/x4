#include "x4/log.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace x4 {
namespace log {
namespace {

struct Sink {
  SinkFn fn;
  void* ctx;
  Level min;
};

constexpr int kMaxSinks = 4;
Sink g_sinks[kMaxSinks];
int g_sinkCount = 0;
char g_dev[8] = "x4";
ClockFn g_clock = nullptr;
volatile Level g_level = Level::Info;

size_t appendStr(char* buf, size_t cap, size_t pos, const char* s) {
  while (*s && pos + 1 < cap) buf[pos++] = *s++;
  if (pos < cap) buf[pos] = 0;
  return pos;
}

void dispatch(Level lvl, const char* line, size_t len) {
  for (int i = 0; i < g_sinkCount; i++) {
    if ((int)lvl <= (int)g_sinks[i].min) g_sinks[i].fn(line, len, lvl, g_sinks[i].ctx);
  }
}

}  // namespace

void setDevice(const char* dev) {
  strncpy(g_dev, dev ? dev : "x4", sizeof(g_dev) - 1);
  g_dev[sizeof(g_dev) - 1] = 0;
}
const char* device() { return g_dev; }
void setClock(ClockFn fn) { g_clock = fn; }
uint32_t now() { return g_clock ? g_clock() : 0; }
void setLevel(Level lvl) { g_level = lvl; }
Level level() { return g_level; }

bool addSink(SinkFn fn, void* ctx, Level minLevel) {
  if (!fn || g_sinkCount >= kMaxSinks) return false;
  g_sinks[g_sinkCount++] = Sink{fn, ctx, minLevel};
  return true;
}
void clearSinks() { g_sinkCount = 0; }

const char* levelName(Level lvl) {
  switch (lvl) {
    case Level::Error: return "ERROR";
    case Level::Warn: return "WARN";
    case Level::Info: return "INFO";
    case Level::Debug: return "DEBUG";
    case Level::Trace: return "TRACE";
  }
  return "?";
}

bool parseLevel(const char* s, Level* out) {
  if (!s || !*s || !out) return false;
  if (s[0] >= '0' && s[0] <= '4' && s[1] == 0) {
    *out = (Level)(s[0] - '0');
    return true;
  }
  static const char* names[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
  for (int i = 0; i < 5; i++) {
    const char* a = s;
    const char* b = names[i];
    while (*a && *b && toupper((unsigned char)*a) == *b) { a++; b++; }
    if (*a == 0 && *b == 0) {
      *out = (Level)i;
      return true;
    }
  }
  return false;
}

size_t sanitizeValue(char* dst, size_t cap, const char* src) {
  if (!dst || cap == 0) return 0;
  size_t w = 0;
  for (const char* p = src ? src : ""; *p && w + 1 < cap; p++) {
    char c = *p;
    if (c == ' ' || c == '\t') c = '_';
    if (c == '"' || c == '\n' || c == '\r' || c == '=') continue;
    dst[w++] = c;
  }
  dst[w] = 0;
  return w;
}

size_t format(char* buf, size_t cap, uint32_t t, Level lvl, const char* dev, const char* mod,
              const char* evt, const char* kv) {
  if (!buf || cap < 8) return 0;
  int n = snprintf(buf, cap, "t=%lu lvl=%s dev=%s mod=%s evt=%s", (unsigned long)t, levelName(lvl),
                   dev ? dev : "?", mod ? mod : "?", evt ? evt : "?");
  if (n < 0) return 0;
  size_t pos = (size_t)n < cap ? (size_t)n : cap - 1;
  if (kv && *kv) {
    pos = appendStr(buf, cap, pos, " ");
    pos = appendStr(buf, cap, pos, kv);
  }
  // Mark truncation so a reader knows the line was cut.
  size_t full = (size_t)n + (kv && *kv ? 1 + strlen(kv) : 0);
  if (full >= cap && cap >= 2) {
    buf[cap - 2] = '~';
    buf[cap - 1] = 0;
    pos = cap - 1;
  }
  return pos;
}

void vwrite(Level lvl, const char* mod, const char* evt, const char* fmt, va_list ap) {
  char kv[kMaxLine];
  kv[0] = 0;
  if (fmt && *fmt) vsnprintf(kv, sizeof(kv), fmt, ap);
  char line[kMaxLine];
  size_t len = format(line, sizeof(line), now(), lvl, g_dev, mod, evt, kv);
  dispatch(lvl, line, len);
}

void write(Level lvl, const char* mod, const char* evt, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vwrite(lvl, mod, evt, fmt, ap);
  va_end(ap);
}

void writeErr(Level lvl, const char* mod, const char* evt, Err code, const char* fmt, ...) {
  char tail[kMaxLine];
  tail[0] = 0;
  va_list ap;
  va_start(ap, fmt);
  if (fmt && *fmt) vsnprintf(tail, sizeof(tail), fmt, ap);
  va_end(ap);
  write(lvl, mod, evt, "err=%u err_name=%s%s%s", (unsigned)code, errName(code), tail[0] ? " " : "",
        tail);
}

bool RateLimit::allow(uint32_t t, uint32_t periodMs, uint32_t* suppressed) {
  if (!armed || (uint32_t)(t - last) >= periodMs) {
    if (suppressed) *suppressed = dropped;
    dropped = 0;
    last = t;
    armed = true;
    return true;
  }
  dropped++;
  return false;
}

}  // namespace log
}  // namespace x4
