// Step 10/11 (Core side): press the button (or tap the screen) and the Core asks every camera
// that answers for a 5 MP photo, one after another, and saves them to microSD.
// Not simultaneous: the cameras shoot when asked, a second or so apart. Synchronised shots are
// what the full firmware adds (docs/PROTOCOL.md).
// Wiring: the full rig on the rail (Core, button, cameras running s10-camN), SD card in.
// Pass: /step10/0001_cam1.jpg ... on the card, each opens on a computer; times on screen.
#include <HTTPClient.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>

#include "core_common.h"
#include "net_common.h"

bool g_sdOk = false;
uint32_t g_shot = 0;

// Download http://<cam>/capture straight into a file. Returns bytes written (0 = failed).
size_t fetchToFile(int id, const char* path, uint32_t* ms) {
  HTTPClient http;
  http.setConnectTimeout(500);
  http.setTimeout(8000);  // a 5 MP grab + ~1 MB over WiFi
  uint32_t t0 = millis();
  http.begin("http://" + cameraIp(id).toString() + "/capture");
  int code = http.GET();
  size_t total = 0;
  if (code == 200) {
    File f = SD.open(path, FILE_WRITE);
    WiFiClient* s = http.getStreamPtr();
    int left = http.getSize();  // -1 if the camera did not say
    static uint8_t buf[8192];
    uint32_t lastData = millis();
    while (f && http.connected() && (left > 0 || left == -1) && millis() - lastData < 3000) {
      size_t n = s->available();
      if (!n) {
        delay(1);
        continue;
      }
      n = s->readBytes(buf, n > sizeof(buf) ? sizeof(buf) : n);
      f.write(buf, n);
      total += n;
      if (left > 0) left -= n;
      lastData = millis();
    }
    f.close();
    if (left > 0) total = 0;  // incomplete download
  }
  http.end();
  *ms = millis() - t0;
  Serial.printf("fetch cam=%d code=%d bytes=%u ms=%lu path=%s\n", id, code, (unsigned)total, (unsigned long)*ms, path);
  return total;
}

bool cameraAnswers(int id) {
  HTTPClient http;
  http.setConnectTimeout(300);
  http.setTimeout(500);
  http.begin("http://" + cameraIp(id).toString() + "/hello");
  bool ok = http.GET() == 200;
  http.end();
  return ok;
}

void shoot() {
  if (!g_sdOk) {
    screenLine(1, TFT_RED, "no SD card: nothing saved");
    return;
  }
  g_shot++;
  Preferences nvs;
  nvs.begin("step10", false);
  nvs.putUInt("shot", g_shot);
  nvs.end();
  SD.mkdir("/step10");
  screenLine(1, TFT_YELLOW, "shot %lu ...", (unsigned long)g_shot);
  uint32_t t0 = millis();
  int saved = 0;
  for (int id = 1; id <= kMaxCameras; id++) {
    if (!cameraAnswers(id)) {
      screenLine(2 + id, TFT_DARKGREY, "cam%d not online", id);
      continue;
    }
    char path[40];
    snprintf(path, sizeof(path), "/step10/%04lu_cam%d.jpg", (unsigned long)g_shot, id);
    uint32_t ms = 0;
    size_t n = fetchToFile(id, path, &ms);
    if (n) saved++;
    screenLine(2 + id, n ? TFT_GREEN : TFT_RED, n ? "cam%d %u kB in %lu ms" : "cam%d FAILED", id,
               (unsigned)(n >> 10), (unsigned long)ms);
  }
  uint32_t total = millis() - t0;
  Serial.printf("shot n=%lu saved=%d total_ms=%lu\n", (unsigned long)g_shot, saved, (unsigned long)total);
  screenLine(1, saved ? TFT_GREEN : TFT_RED, "shot %lu: %d saved, %lu ms", (unsigned long)g_shot, saved,
             (unsigned long)total);
}

void setup() {
  beginCoreSafely();
  pinMode(pins::kButton, INPUT_PULLUP);
  screenTitle("step 10: shoot to SD");
  SPI.begin(pins::kSpiSck, pins::kSpiMiso, pins::kSpiMosi, pins::kSdCs);
  g_sdOk = SD.begin(pins::kSdCs, SPI, 25000000);
  Preferences nvs;
  nvs.begin("step10", true);
  g_shot = nvs.getUInt("shot", 0);
  nvs.end();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(STEPS_AP_SSID, STEPS_AP_PASS, 6, 0, 6);
  Serial.printf("step10 sd_ok=%d last_shot=%lu ap=%s\n", g_sdOk, (unsigned long)g_shot, STEPS_AP_SSID);
  screenLine(0, g_sdOk ? TFT_WHITE : TFT_RED, g_sdOk ? "SD ok. Button or tap = shoot" : "SD card missing");
}

void loop() {
  M5.update();
  static bool wasDown = false;
  bool down = digitalRead(pins::kButton) == LOW;
  if ((down && !wasDown) || M5.Touch.getDetail().wasPressed()) shoot();
  wasDown = down;
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll > 2000) {
    lastPoll = millis();
    PowerBits b = readPowerBits();
    if (!b.safe()) printPowerBits("found_on", forcePowerOutputsOff());
    screenLine(2, TFT_WHITE, "WiFi devices: %d", WiFi.softAPgetStationNum());
  }
  delay(20);
}
