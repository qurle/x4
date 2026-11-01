// Step 9 (camera side): join the Core's WiFi as camera 1 and answer /hello.
// Wiring: camera on the Grove-to-USB-C adapter (for its serial log), Core running s09-core-ap.
// Pass: serial shows "joined ip=192.168.4.11"; blue LED on; the Core's screen shows cam1 ok.
#include <WebServer.h>
#include <WiFi.h>

#include "cam_common.h"
#include "net_common.h"

WebServer server(80);

void handleHello() {
  char buf[96];
  snprintf(buf, sizeof(buf), "cam=%d rssi=%d uptime_ms=%lu\n", CAM_ID, WiFi.RSSI(), (unsigned long)millis());
  server.send(200, "text/plain", buf);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  led(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // power save adds 100+ ms of random delay; the rig wants fast replies
  WiFi.config(cameraIp(CAM_ID), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.begin(STEPS_AP_SSID, STEPS_AP_PASS);
  server.on("/hello", handleHello);
  server.begin();
}

void loop() {
  static bool joined = false;
  bool now = WiFi.status() == WL_CONNECTED;
  if (now != joined) {
    joined = now;
    led(joined);
    if (joined) {
      Serial.printf("joined ssid=%s ip=%s rssi=%d\n", STEPS_AP_SSID, WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.println("wifi lost, retrying");
    }
  }
  server.handleClient();
  delay(2);
}
