// x4 camera firmware entry point (Unit CamS3-5MP).
// Boot order matters for power: identity + banner first (cheap), then a per-camera stagger
// delay, then the sensor init and WiFi (the two big current peaks), so four cameras switched on
// together do not all peak at once (docs/POWER.md "Inrush").
#include <Arduino.h>
#include <WiFi.h>

#include "cam.h"
#include "cam_pins.h"
#include "camsys.h"
#include "netcam.h"
#include "x4/log.h"

using namespace x4cam;

namespace {
uint32_t g_lastLed = 0;
bool g_ledOn = false;

void led(bool on) { digitalWrite(pins::kLed, (on ^ pins::kLedActiveLow) ? HIGH : LOW); }
}  // namespace

void setup() {
  pinMode(pins::kLed, OUTPUT);
  led(true);  // on while booting
  sys::beginLog();
  sys::banner();
  conf::begin();
  const auto& c = conf::get();

  uint32_t stagger = (uint32_t)sys::camId() * (uint32_t)c.cam_stagger_ms;
  X4_LOGI("boot", "stagger", "cam_id=%u delay_ms=%lu", sys::camId(), (unsigned long)stagger);
  delay(stagger);

  uint8_t rev = cam::probeHwRev();
  X4_LOGI("cam", "hw_rev", "reg_0x0200=0x%02x meaning=%s", rev, rev == 0x01 ? "new" : rev == 0xFF ? "old" : "unknown");
  bool camOk = cam::begin(c);
  if (!camOk) X4_LOGE("boot", "camera", "capture_path=disabled reason=see_previous_error");

#if X4_CAM_HELLO
  // Stage 0: benchmark before WiFi so the numbers are not disturbed by radio traffic.
  if (camOk) cam::bench();
#endif

  if (net::joinWifi()) net::begin();
  X4_LOGI("boot", "ready", "ms=%lu heap=%lu psram=%lu", (unsigned long)millis(), (unsigned long)ESP.getFreeHeap(),
          (unsigned long)ESP.getFreePsram());
}

void loop() {
  uint32_t now = millis();
  net::poll(now);
  if (WiFi.status() != WL_CONNECTED && now - g_lastLed > 150) {
    // Fast blink: not connected (the LED points backwards, keep it off while shooting).
    g_lastLed = now;
    g_ledOn = !g_ledOn;
    led(g_ledOn);
  } else if (WiFi.status() == WL_CONNECTED && g_ledOn) {
    g_ledOn = false;
    led(false);
  }
  static uint32_t lastTemp = 0;
  if (now - lastTemp > 5000) {
    lastTemp = now;
    float t = sys::tempC();
    if (t >= conf::get().therm_warn_c) X4_LOGW("therm", "warn", "temp_c=%.1f", t);
  }
  delay(10);
}
