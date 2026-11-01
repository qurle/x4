// Step 10/11 (camera side): step 9 plus the camera. Serves a fresh 5 MP JPEG at /capture.
// Flash s10-cam1 for step 10; s10-cam2..4 on the other three cameras for step 11.
// Wiring: camera on the rail harness (no USB) once it has been flashed. Core runs s10-core-shot.
// Pass: blue LED on after a few seconds = joined and camera ready.
#include <WebServer.h>
#include <WiFi.h>

#include "cam_common.h"
#include "net_common.h"

WebServer server(80);
bool g_camOk = false;

void handleHello() {
  char buf[96];
  snprintf(buf, sizeof(buf), "cam=%d cam_ok=%d rssi=%d uptime_ms=%lu\n", CAM_ID, g_camOk, WiFi.RSSI(),
           (unsigned long)millis());
  server.send(200, "text/plain", buf);
}

void handleCapture() {
  if (!g_camOk) {
    server.send(503, "text/plain", "camera not ready");
    return;
  }
  cameraWarmUp(1);
  uint32_t t0 = millis();
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "capture failed");
    return;
  }
  Serial.printf("capture bytes=%u grab_ms=%lu\n", (unsigned)fb->len, (unsigned long)(millis() - t0));
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  server.client().write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  led(false);
  g_camOk = cameraBegin(FRAMESIZE_5MP) == kExpectedPid;
  if (g_camOk) cameraWarmUp();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.config(cameraIp(CAM_ID), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.begin(STEPS_AP_SSID, STEPS_AP_PASS);
  server.on("/hello", handleHello);
  server.on("/capture", handleCapture);
  server.begin();
}

void loop() {
  static bool joined = false;
  bool now = WiFi.status() == WL_CONNECTED;
  if (now != joined) {
    joined = now;
    led(joined && g_camOk);
    Serial.printf("wifi joined=%d ip=%s cam_ok=%d\n", joined, WiFi.localIP().toString().c_str(), g_camOk);
  }
  server.handleClient();
  delay(2);
}
