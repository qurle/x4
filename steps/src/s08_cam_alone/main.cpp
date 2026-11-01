// Step 8: one camera, no Core. The camera makes its own small WiFi; a phone shows the photo.
// Wiring: camera on M5's Grove-to-USB-C adapter only (that is its power AND its serial port).
// Use: join WiFi "x4cam-cam-test" (password x4cam-steps), open http://192.168.4.1/
// Pass: serial shows psram_mb=8 and pid=0x039e ok=1; /capture opens a 2592x1944 JPEG.
#include <WebServer.h>
#include <WiFi.h>

#include "cam_common.h"

WebServer server(80);

void handleRoot() {
  server.send(200, "text/html",
              "<h1>x4cam step 8</h1><p><a href=/capture>5 MP photo</a> (takes a few seconds)</p>"
              "<p><a href=/info>info</a></p>");
}

void handleInfo() {
  sensor_t* s = esp_camera_sensor_get();
  char buf[200];
  snprintf(buf, sizeof(buf), "pid=0x%04x psram_mb=%u free_psram_kb=%u uptime_ms=%lu\n", s ? s->id.PID : 0,
           (unsigned)(ESP.getPsramSize() >> 20), (unsigned)(ESP.getFreePsram() >> 10), (unsigned long)millis());
  server.send(200, "text/plain", buf);
}

void handleCapture() {
  cameraWarmUp(1);  // drop the buffered (older) frame, then take a fresh one
  uint32_t t0 = millis();
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("capture FAILED");
    server.send(500, "text/plain", "capture failed");
    return;
  }
  uint32_t grabMs = millis() - t0;
  Serial.printf("capture w=%u h=%u bytes=%u grab_ms=%lu\n", fb->width, fb->height, (unsigned)fb->len,
                (unsigned long)grabMs);
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  server.client().write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // give the USB serial a moment so the first lines are not lost
  led(false);
  Serial.printf("step8 chip=%s psram_mb=%u flash_mb=%u\n", ESP.getChipModel(), (unsigned)(ESP.getPsramSize() >> 20),
                (unsigned)(ESP.getFlashChipSize() >> 20));
  if (cameraBegin(FRAMESIZE_5MP) == 0) return;  // see the line above for the reason
  cameraWarmUp();
  WiFi.softAP("x4cam-cam-test", "x4cam-steps");
  Serial.printf("wifi ap ssid=x4cam-cam-test ip=%s\n", WiFi.softAPIP().toString().c_str());
  server.on("/", handleRoot);
  server.on("/info", handleInfo);
  server.on("/capture", handleCapture);
  server.begin();
  led(true);  // blue LED on = ready
}

void loop() { server.handleClient(); }
