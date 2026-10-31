// x4 structured logging. One format for the Core and every camera. Spec: docs/LOGGING.md.
//
// Line format (logfmt, one event per line, values without spaces or quoted):
//   t=12345 lvl=INFO dev=core mod=net evt=register cam=2 ip=192.168.4.12 fw=0.1.0
//   ^uptime ms ^level  ^device  ^module ^event     ^free key=value pairs
//
// Rules:
//  * never call Serial.print directly; use X4_LOG* so every line has t/lvl/dev/mod/evt.
//  * mod and evt are short stable identifiers ([a-z0-9_]); they are what you grep for.
//  * errors carry err=<code> from errors.def: use X4_LOG_ERR.
//  * do not log from ISRs or from the LVGL flush callback (it may block on the serial port).
//  * repeated messages in hot paths: X4_LOG_EVERY(ms, ...) rate-limits per call site.
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "errors.h"

namespace x4 {
namespace log {

enum class Level : uint8_t { Error = 0, Warn = 1, Info = 2, Debug = 3, Trace = 4 };

#ifndef X4_LOG_COMPILE_LEVEL
// Anything more verbose than this is compiled out entirely.
#define X4_LOG_COMPILE_LEVEL 4
#endif

// Max formatted line length including the newline. Longer lines are cut and end with "~".
constexpr size_t kMaxLine = 240;

// A sink receives one complete line (no trailing newline) and its level.
using SinkFn = void (*)(const char* line, size_t len, Level lvl, void* ctx);
using ClockFn = uint32_t (*)();

void setDevice(const char* dev);  // "core", "cam1".."cam4"; copied, max 7 chars.
const char* device();
void setClock(ClockFn fn);         // uptime in ms; defaults to 0 until set.
void setLevel(Level lvl);          // runtime level (also limited by X4_LOG_COMPILE_LEVEL).
Level level();
// Register a sink. Up to 4 sinks. minLevel filters per sink (e.g. camera->Core forwarding = Warn).
bool addSink(SinkFn fn, void* ctx, Level minLevel);
void clearSinks();  // for tests

// printf-style key=value tail. Use for normal events.
void write(Level lvl, const char* mod, const char* evt, const char* fmt, ...)
    __attribute__((format(printf, 4, 5)));
void vwrite(Level lvl, const char* mod, const char* evt, const char* fmt, va_list ap);
// Same, with an error code prepended as err=<code> err_name=<NAME>.
void writeErr(Level lvl, const char* mod, const char* evt, Err code, const char* fmt, ...)
    __attribute__((format(printf, 5, 6)));

// Pure formatter (unit-tested). kv may be null/empty. Returns the line length (without NUL).
size_t format(char* buf, size_t cap, uint32_t t, Level lvl, const char* dev, const char* mod,
              const char* evt, const char* kv);

const char* levelName(Level lvl);                 // "ERROR", "WARN", ...
bool parseLevel(const char* s, Level* out);       // accepts names (any case) or digits 0..4

// Copies src into dst making it safe as a logfmt value: spaces -> '_', quotes/newlines removed.
size_t sanitizeValue(char* dst, size_t cap, const char* src);

// Rate limiter used by X4_LOG_EVERY. Returns true if this call may log; *suppressed gets the
// number of calls dropped since the last allowed one.
struct RateLimit {
  uint32_t last = 0;
  uint32_t dropped = 0;
  bool armed = false;
  bool allow(uint32_t now, uint32_t periodMs, uint32_t* suppressed);
};

uint32_t now();  // the configured clock

}  // namespace log
}  // namespace x4

#define X4_LOG_ENABLED(lvl)                                    \
  ((int)(lvl) <= X4_LOG_COMPILE_LEVEL && (int)(lvl) <= (int)::x4::log::level())

#define X4_LOG(lvl, mod, evt, ...)                         \
  do {                                                     \
    if (X4_LOG_ENABLED(lvl)) ::x4::log::write(lvl, mod, evt, __VA_ARGS__); \
  } while (0)

#define X4_LOGE(mod, evt, ...) X4_LOG(::x4::log::Level::Error, mod, evt, __VA_ARGS__)
#define X4_LOGW(mod, evt, ...) X4_LOG(::x4::log::Level::Warn, mod, evt, __VA_ARGS__)
#define X4_LOGI(mod, evt, ...) X4_LOG(::x4::log::Level::Info, mod, evt, __VA_ARGS__)
#define X4_LOGD(mod, evt, ...) X4_LOG(::x4::log::Level::Debug, mod, evt, __VA_ARGS__)
#define X4_LOGT(mod, evt, ...) X4_LOG(::x4::log::Level::Trace, mod, evt, __VA_ARGS__)

// Error with a code from errors.def: X4_LOG_ERR(Error, "sd", "mount", x4::Err::SD_MOUNT_FAIL, "card=%d", 0)
#define X4_LOG_ERR(lvlname, mod, evt, code, ...)                                        \
  do {                                                                                  \
    if (X4_LOG_ENABLED(::x4::log::Level::lvlname))                                      \
      ::x4::log::writeErr(::x4::log::Level::lvlname, mod, evt, code, __VA_ARGS__);       \
  } while (0)

// Rate-limited log: at most one line per periodMs per call site; appends dropped=N.
#define X4_LOG_EVERY(periodMs, lvl, mod, evt, fmt, ...)                                  \
  do {                                                                                  \
    static ::x4::log::RateLimit _x4_rl;                                                 \
    uint32_t _x4_sup = 0;                                                               \
    if (X4_LOG_ENABLED(lvl) && _x4_rl.allow(::x4::log::now(), periodMs, &_x4_sup))      \
      ::x4::log::write(lvl, mod, evt, fmt " dropped=%u", ##__VA_ARGS__, (unsigned)_x4_sup); \
  } while (0)
