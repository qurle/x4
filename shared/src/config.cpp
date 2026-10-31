#include "x4/config.h"

#include <stdio.h>
#include <string.h>

namespace x4 {
namespace cfg {
namespace {

// NVS keys are limited to 15 characters.
#define X4_CFG(name, def, mn, mx, unit, scope, flags, doc)                                   \
  static_assert(sizeof(#name) <= 16, "config name too long for NVS: " #name);               \
  static_assert((def) >= (mn) && (def) <= (mx), "config default out of range: " #name);
#include "x4/config_schema.def"
#undef X4_CFG

const OptionInfo kOptions[] = {
#define X4_CFG(name, def, mn, mx, unit, scope, flags, doc) \
  {#name, def, mn, mx, #unit, scope, flags, doc, offsetof(Config, name)},
#include "x4/config_schema.def"
#undef X4_CFG
};
constexpr size_t kCount = sizeof(kOptions) / sizeof(kOptions[0]);

int32_t clampRange(int32_t v, const OptionInfo& o, const char** reason) {
  if (v < o.min) {
    *reason = "below_min";
    return o.min;
  }
  if (v > o.max) {
    *reason = "above_max";
    return o.max;
  }
  return v;
}

// Applies range + safety rules. Returns the applied value; *reason != null if clamped.
int32_t applyRules(const OptionInfo& o, int32_t v, int32_t baseValue, bool runtime,
                   const char** reason) {
  *reason = nullptr;
  int32_t a = clampRange(v, o, reason);
  if (runtime) {
    if ((o.flags & SAFE_MAX) && a > baseValue) {
      a = baseValue;
      *reason = "safety_tighten_only";
    }
    if ((o.flags & SAFE_MIN) && a < baseValue) {
      a = baseValue;
      *reason = "safety_tighten_only";
    }
  }
  return a;
}

size_t appendf(char* buf, size_t cap, size_t pos, const char* fmt, long v, const char* name) {
  if (pos >= cap) return pos;
  int n = snprintf(buf + pos, cap - pos, fmt, name, v);
  if (n < 0) return pos;
  return pos + (size_t)n >= cap ? cap - 1 : pos + (size_t)n;
}

}  // namespace

size_t optionCount() { return kCount; }
const OptionInfo& option(size_t i) { return kOptions[i < kCount ? i : 0]; }

const OptionInfo* find(const char* name) {
  if (!name) return nullptr;
  for (size_t i = 0; i < kCount; i++)
    if (strcmp(kOptions[i].name, name) == 0) return &kOptions[i];
  return nullptr;
}

int32_t get(const Config& c, const OptionInfo& o) {
  int32_t v;
  memcpy(&v, reinterpret_cast<const uint8_t*>(&c) + o.offset, sizeof(v));
  return v;
}

void put(Config& c, const OptionInfo& o, int32_t v) {
  memcpy(reinterpret_cast<uint8_t*>(&c) + o.offset, &v, sizeof(v));
}

Config defaults() {
  Config c;
  for (size_t i = 0; i < kCount; i++) put(c, kOptions[i], kOptions[i].def);
  return c;
}

size_t validate(Config& c, const Config& base, bool runtime, Issue* issues, size_t maxIssues) {
  size_t n = 0;
  for (size_t i = 0; i < kCount; i++) {
    const OptionInfo& o = kOptions[i];
    int32_t v = get(c, o);
    const char* reason = nullptr;
    int32_t a = applyRules(o, v, get(base, o), runtime, &reason);
    if (reason) {
      put(c, o, a);
      if (issues && n < maxIssues) issues[n] = Issue{o.name, v, a, reason};
      n++;
    }
  }
  return n;
}

bool set(Config& c, const Config& base, const char* name, int32_t value, Issue* issue) {
  if (issue) *issue = Issue{nullptr, 0, 0, nullptr};
  const OptionInfo* o = find(name);
  if (!o) return false;
  const char* reason = nullptr;
  int32_t a = applyRules(*o, value, get(base, *o), true, &reason);
  put(c, *o, a);
  if (reason && issue) *issue = Issue{o->name, value, a, reason};
  return true;
}

size_t summary(const Config& c, uint8_t scope, char* buf, size_t cap) {
  if (!buf || cap == 0) return 0;
  buf[0] = 0;
  size_t pos = 0;
  for (size_t i = 0; i < kCount; i++) {
    const OptionInfo& o = kOptions[i];
    if (!(o.scope & scope)) continue;
    pos = appendf(buf, cap, pos, pos ? " %s=%ld" : "%s=%ld", (long)get(c, o), o.name);
  }
  return pos;
}

size_t toJson(const Config& c, uint8_t scope, char* buf, size_t cap) {
  if (!buf || cap < 3) return 0;
  size_t pos = 0;
  buf[pos++] = '{';
  buf[pos] = 0;
  bool first = true;
  for (size_t i = 0; i < kCount; i++) {
    const OptionInfo& o = kOptions[i];
    if (!(o.scope & scope)) continue;
    pos = appendf(buf, cap, pos, first ? "\"%s\":%ld" : ",\"%s\":%ld", (long)get(c, o), o.name);
    first = false;
  }
  if (pos + 2 <= cap) {
    buf[pos++] = '}';
    buf[pos] = 0;
  }
  return pos;
}

int migrate(Config& c, uint16_t fromVersion) {
  (void)c;
  int steps = 0;
  // v0 = nothing stored yet (fresh NVS): defaults are already in place.
  if (fromVersion < 1) steps++;
  // Future: if (fromVersion < 2) { rename/convert fields; steps++; }
  return steps;
}

}  // namespace cfg
}  // namespace x4
