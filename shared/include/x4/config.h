// x4 configuration: typed struct generated from config_schema.def, plus validation, safety
// clamping, (de)serialisation helpers and versioning. Storage (NVS) is done by the firmware.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace x4 {
namespace cfg {

// Bump when an option is renamed/removed or its meaning changes; add a step in migrate().
constexpr uint16_t kConfigVersion = 1;

enum Scope : uint8_t { CORE = 1, CAM = 2, BOTH = 3 };
enum Flags : uint8_t { SAFE_MAX = 1, SAFE_MIN = 2, REBOOT = 4 };

struct Config {
#define X4_CFG(name, def, mn, mx, unit, scope, flags, doc) int32_t name;
#include "config_schema.def"
#undef X4_CFG
};

struct OptionInfo {
  const char* name;
  int32_t def;
  int32_t min;
  int32_t max;
  const char* unit;
  uint8_t scope;
  uint8_t flags;
  const char* doc;
  size_t offset;  // byte offset of the field in Config
};

size_t optionCount();
const OptionInfo& option(size_t i);
const OptionInfo* find(const char* name);  // nullptr if unknown

int32_t get(const Config& c, const OptionInfo& o);
void put(Config& c, const OptionInfo& o, int32_t v);

Config defaults();

// One clamping decision, for logging (err=500).
struct Issue {
  const char* name;
  int32_t requested;
  int32_t applied;
  const char* reason;  // "below_min", "above_max", "safety_tighten_only"
};

// Validates every field. base = compile-time defaults (the safety reference). runtime=true means
// the values came from NVS/HTTP, so SAFE_MAX/SAFE_MIN options may not loosen past base.
// Writes up to maxIssues issues; returns the total number of clamped fields.
size_t validate(Config& c, const Config& base, bool runtime, Issue* issues, size_t maxIssues);

// Sets one option by name with the same rules as validate(runtime=true).
// Returns false for unknown names. *issue is filled if the value was clamped (issue->name != null).
bool set(Config& c, const Config& base, const char* name, int32_t value, Issue* issue);

// "log_level=2 flash_enabled=0 ..." for the boot banner (only options matching scope).
size_t summary(const Config& c, uint8_t scope, char* buf, size_t cap);
// {"log_level":2,...} for GET /config.
size_t toJson(const Config& c, uint8_t scope, char* buf, size_t cap);

// Applies migrations from `fromVersion` to kConfigVersion. Returns the number of steps applied.
int migrate(Config& c, uint16_t fromVersion);

}  // namespace cfg
}  // namespace x4
