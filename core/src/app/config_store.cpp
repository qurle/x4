#include "config_store.h"

#include <Preferences.h>

#include "x4/log.h"

namespace x4core {
namespace conf {
namespace {

using namespace x4::cfg;

Config g_base;
Config g_cfg;
Preferences g_nvs;
ChangeFn g_onChange = nullptr;

void logIssue(const Issue& is, const char* when) {
  X4_LOG_ERR(Warn, "cfg", "clamped", x4::Err::CFG_CLAMPED, "when=%s key=%s requested=%ld applied=%ld reason=%s",
             when, is.name, (long)is.requested, (long)is.applied, is.reason);
}

}  // namespace

void begin() {
  g_base = defaults();
  g_cfg = g_base;
  g_nvs.begin("x4cfg", false);
  uint16_t ver = g_nvs.getUShort("cfgver", 0);
  // Load overrides that exist in NVS.
  int loaded = 0;
  for (size_t i = 0; i < optionCount(); i++) {
    // All scopes: the Core also owns the camera options and pushes them at registration.
    const OptionInfo& o = option(i);
    if (g_nvs.isKey(o.name)) {
      put(g_cfg, o, g_nvs.getInt(o.name, o.def));
      loaded++;
    }
  }
  if (ver != kConfigVersion) {
    int steps = migrate(g_cfg, ver);
    g_nvs.putUShort("cfgver", kConfigVersion);
    X4_LOG_ERR(Info, "cfg", "migrate", x4::Err::CFG_MIGRATED, "from=%u to=%u steps=%d", ver,
               kConfigVersion, steps);
  }
  Issue issues[8];
  size_t n = validate(g_cfg, g_base, true, issues, 8);
  for (size_t i = 0; i < n && i < 8; i++) {
    logIssue(issues[i], "boot");
    put(g_cfg, *find(issues[i].name), issues[i].applied);
    g_nvs.putInt(issues[i].name, issues[i].applied);  // persist the clamped value
  }
  X4_LOGI("cfg", "loaded", "version=%u overrides=%d clamped=%u", kConfigVersion, loaded, (unsigned)n);
  x4::log::setLevel((x4::log::Level)g_cfg.log_level);
}

Config& get() { return g_cfg; }
const Config& base() { return g_base; }

bool set(const char* name, int32_t value, Issue* issue) {
  Issue is;
  const OptionInfo* o = find(name);
  if (!o) {
    X4_LOG_ERR(Warn, "cfg", "set", x4::Err::CFG_UNKNOWN_KEY, "key=%s", name ? name : "-");
    return false;
  }
  if (!x4::cfg::set(g_cfg, g_base, name, value, &is)) return false;
  if (is.name) logIssue(is, "set");
  int32_t applied = get(g_cfg, *o);
  g_nvs.putInt(o->name, applied);
  X4_LOGI("cfg", "set", "key=%s value=%ld reboot=%d", o->name, (long)applied, (o->flags & REBOOT) ? 1 : 0);
  if (issue) *issue = is;
  if (strcmp(o->name, "log_level") == 0) x4::log::setLevel((x4::log::Level)applied);
  if (g_onChange) g_onChange(*o, applied);
  return true;
}

void resetToDefaults() {
  g_nvs.clear();
  g_nvs.putUShort("cfgver", kConfigVersion);
  g_cfg = g_base;
  X4_LOGW("cfg", "reset", "defaults=1");
}

void logSummary() {
  char buf[2000];
  summary(g_cfg, BOTH, buf, sizeof(buf));
  // Split into lines that fit one log record.
  const char* p = buf;
  int part = 0;
  while (*p) {
    char chunk[180];
    size_t n = 0;
    const char* last = p;
    while (p[n] && n < sizeof(chunk) - 1) {
      if (p[n] == ' ') last = p + n;
      n++;
    }
    if (p[n] && last > p) n = (size_t)(last - p);
    memcpy(chunk, p, n);
    chunk[n] = 0;
    X4_LOGI("cfg", "summary", "part=%d %s", part++, chunk);
    p += n;
    while (*p == ' ') p++;
  }
}

void onChange(ChangeFn fn) { g_onChange = fn; }

}  // namespace conf
}  // namespace x4core
