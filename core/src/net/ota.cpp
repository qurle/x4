// Stage 5: ArduinoOTA over the Core's own AP, so after the first USB flash you can update the
// Core without plugging USB-C in (which would break the one-5V-source rule, docs/BRINGUP.md).
//   pio run -e core -t upload --upload-port 192.168.4.1   (laptop joined to the x4cam AP)
// Disabled if X4_OTA_PASS is empty. The flash pin is forced low before an update starts.
#include <ArduinoOTA.h>

#include "../hw/flash.h"
#include "net.h"
#include "x4/log.h"
#include "x4/version.h"
#include "x4_secrets.h"

namespace x4core {
namespace net {

static bool g_ota = false;

void beginOta() {
  if (sizeof(X4_OTA_PASS) <= 1) {
    X4_LOGI("ota", "disabled", "reason=no_password");
    return;
  }
  ArduinoOTA.setHostname(X4_PROJECT_NAME "-core");
  ArduinoOTA.setPassword(X4_OTA_PASS);
  ArduinoOTA.onStart([]() {
    flash::earlyOff();
    X4_LOGW("ota", "start", "type=%s", ArduinoOTA.getCommand() == U_FLASH ? "flash" : "fs");
  });
  ArduinoOTA.onEnd([]() { X4_LOGW("ota", "end", "reboot=1"); });
  ArduinoOTA.onError([](ota_error_t e) { X4_LOGE("ota", "error", "code=%d", (int)e); });
  ArduinoOTA.begin();
  g_ota = true;
  X4_LOGI("ota", "ready", "host=" X4_PROJECT_NAME "-core port=3232");
}

void pollOta() {
  if (g_ota) ArduinoOTA.handle();
}

}  // namespace net
}  // namespace x4core
