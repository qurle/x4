// Runtime configuration for the Core: compile-time defaults from shared/x4/config_schema.def,
// overridden by values stored in NVS (namespace "x4cfg"), validated at boot with the safety
// rules (tighten-only for SAFE_MAX/SAFE_MIN options). Changed via the web page (POST /config).
#pragma once
#include "x4/config.h"

namespace x4core {
namespace conf {

void begin();                       // load + migrate + validate + log
x4::cfg::Config& get();
const x4::cfg::Config& base();      // compile-time defaults
// Validates, applies and persists one option. Returns false for unknown names.
bool set(const char* name, int32_t value, x4::cfg::Issue* issue);
void resetToDefaults();             // erases NVS overrides
void logSummary();
// Called after any successful set(); the app re-applies live settings.
using ChangeFn = void (*)(const x4::cfg::OptionInfo& o, int32_t value);
void onChange(ChangeFn fn);

}  // namespace conf
}  // namespace x4core
