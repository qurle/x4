// Step 5: the Core runs from the rail (power bank -> USB-C breakout -> switch -> Port A),
// with NO USB cable. First time the "backwards" power path is used for real.
// Before: flash this over USB, unplug USB, THEN connect Port A red/black to the rail.
// Wiring: rail (+5 V red, GND black) into Port A. Nothing else.
// Pass: boots on switch-on; screen shows vbus ~4.8-5.1 V, SAFE, uptime keeps counting;
//       reset reason is "power on", not "brownout". Leave it 10 minutes.
#include <Preferences.h>
#include <esp_system.h>

#include "core_common.h"

const char* resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    default: return "other";
  }
}

uint32_t g_boots = 0, g_brownouts = 0;
esp_reset_reason_t g_reason;
int g_minMv = 99999, g_maxMv = 0;

void setup() {
  PowerBits b = beginCoreSafely();
  g_reason = esp_reset_reason();
  // Count boots and brownouts across restarts (stored in flash, survives power-off).
  Preferences nvs;
  nvs.begin("step5", false);
  g_boots = nvs.getUInt("boots", 0) + 1;
  g_brownouts = nvs.getUInt("brown", 0) + (g_reason == ESP_RST_BROWNOUT ? 1 : 0);
  nvs.putUInt("boots", g_boots);
  nvs.putUInt("brown", g_brownouts);
  nvs.end();
  Serial.printf("rail boot reset=%s boots=%lu brownouts=%lu\n", resetName(g_reason), (unsigned long)g_boots,
                (unsigned long)g_brownouts);
  screenTitle("step 5: Core on the rail");
  screenLine(0, g_reason == ESP_RST_BROWNOUT ? TFT_RED : TFT_WHITE, "last reset: %s", resetName(g_reason));
  screenLine(1, TFT_WHITE, "boots: %lu, brownouts: %lu", (unsigned long)g_boots, (unsigned long)g_brownouts);
  (void)b;
}

void loop() {
  PowerBits b = readPowerBits();
  if (!b.safe()) b = forcePowerOutputsOff();
  if (b.vbusMv > 0 && b.vbusMv < g_minMv) g_minMv = b.vbusMv;
  if (b.vbusMv > g_maxMv) g_maxMv = b.vbusMv;
  bool usbOrRail = b.vbusMv > 4000;
  screenLine(3, usbOrRail ? TFT_WHITE : TFT_RED, "5V input: %d mV (min %d, max %d)", b.vbusMv, g_minMv, g_maxMv);
  screenLine(4, b.safe() ? TFT_GREEN : TFT_RED, b.safe() ? "outputs off: SAFE" : "output ON: unplug!");
  screenLine(5, TFT_WHITE, "uptime: %lu s", (unsigned long)(millis() / 1000));
  Serial.printf("rail uptime_s=%lu vbus_mv=%d min_mv=%d safe=%d\n", (unsigned long)(millis() / 1000), b.vbusMv,
                g_minMv, b.safe());
  delay(2000);
}
