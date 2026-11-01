// Step 9 (Core side): the Core makes the rig's WiFi and checks which cameras answer.
// Wiring: Core on USB or on the rail (never both). Camera(s) running s09-cam / s10-camN.
// Pass: "WiFi x4cam-steps up"; after a few seconds cam1 shows "ok" with a reply time < 50 ms.
#include <HTTPClient.h>
#include <WiFi.h>

#include "core_common.h"
#include "net_common.h"

void setup() {
  beginCoreSafely();
  screenTitle("step 9: Core WiFi");
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(STEPS_AP_SSID, STEPS_AP_PASS, 6 /*channel*/, 0 /*visible*/, 6 /*max clients*/);
  Serial.printf("wifi ap ok=%d ssid=%s ip=%s\n", ok, STEPS_AP_SSID, WiFi.softAPIP().toString().c_str());
  screenLine(0, ok ? TFT_GREEN : TFT_RED, ok ? "WiFi %s up, 192.168.4.1" : "WiFi FAILED", STEPS_AP_SSID);
}

void loop() {
  screenLine(1, TFT_WHITE, "devices joined: %d", WiFi.softAPgetStationNum());
  for (int id = 1; id <= kMaxCameras; id++) {
    HTTPClient http;
    http.setConnectTimeout(300);
    http.setTimeout(500);
    String url = "http://" + cameraIp(id).toString() + "/hello";
    uint32_t t0 = millis();
    http.begin(url);
    int code = http.GET();
    uint32_t ms = millis() - t0;
    String body = code == 200 ? http.getString() : String();
    http.end();
    body.trim();
    if (code == 200) {
      Serial.printf("cam%d ok ms=%lu %s\n", id, (unsigned long)ms, body.c_str());
      screenLine(2 + id, TFT_GREEN, "cam%d ok, %lu ms  %s", id, (unsigned long)ms, body.c_str());
    } else {
      screenLine(2 + id, TFT_DARKGREY, "cam%d -- (%s)", id, cameraIp(id).toString().c_str());
    }
  }
  PowerBits b = readPowerBits();
  if (!b.safe()) printPowerBits("found_on", forcePowerOutputsOff());
  delay(2000);
}
