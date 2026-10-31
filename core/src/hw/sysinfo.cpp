#include "sysinfo.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>

#include "x4/log.h"

namespace x4core {
namespace sys {
namespace {
Info g_info;
uint32_t g_shot = 0;
Preferences g_prefs;
}  // namespace

const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "other_wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    case ESP_RST_USB: return "usb";
    case ESP_RST_JTAG: return "jtag";
    default: return "unknown";
  }
}

void begin() {
  g_info.reset = esp_reset_reason();
  g_info.resetName = resetReasonName(g_info.reset);
  esp_chip_info_t ci;
  esp_chip_info(&ci);
  g_info.chipRev = ci.revision;
  g_info.cpuMhz = getCpuFrequencyMhz();
  g_info.flashBytes = ESP.getFlashChipSize();
  g_info.psramBytes = ESP.getPsramSize();
  esp_read_mac(g_info.mac, ESP_MAC_WIFI_STA);

  g_prefs.begin("x4sys", false);
  g_info.bootCount = g_prefs.getUInt("boots", 0) + 1;
  g_prefs.putUInt("boots", g_info.bootCount);
  g_info.brownouts = g_prefs.getUInt("brownouts", 0);
  if (g_info.reset == ESP_RST_BROWNOUT) {
    g_info.brownouts++;
    g_prefs.putUInt("brownouts", g_info.brownouts);
  }
  g_shot = g_prefs.getUInt("shot", 0);
}

const Info& info() { return g_info; }
bool lastResetWasBrownout() { return g_info.reset == ESP_RST_BROWNOUT; }
float chipTempC() { return temperatureRead(); }
uint32_t freeHeap() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }
uint32_t freePsram() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }
uint32_t minFreeHeap() { return heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL); }

uint32_t nextShotId() {
  g_shot++;
  g_prefs.putUInt("shot", g_shot);
  return g_shot;
}
uint32_t lastShotId() { return g_shot; }

}  // namespace sys
}  // namespace x4core
