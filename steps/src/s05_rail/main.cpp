// Step 5: the Core runs from the rail (power bank -> USB-C breakout -> switch -> Port A),
// with NO USB cable. First time the "backwards" power path is used for real.
// Before: flash this over USB, unplug USB, THEN connect Port A red/black to the rail.
// Wiring: rail (+5 V red, GND black) into Port A. Nothing else.
// Pass: boots on switch-on; screen shows vbus ~4.8-5.1 V, SAFE, uptime keeps counting;
//       reset reason is "power on", not "brownout". Leave it 10 minutes.
// Wiggle test (docs/SOLDERING.md "Prove the power path"): tap the screen to reset min/max and
// the dip counter, flex every joint on the power path, then check that "dips" stayed 0.
// The 5 V input is read every ~5 ms, so short dropouts are caught. How often the AXP2101's own
// ADC refreshes that value is Not sure, so a dip of a few ms may still be missed.
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

constexpr int kDipMv = 4500;  // below this counts as a dip (rail normally 4.75-5.25 V)

uint32_t g_boots = 0, g_brownouts = 0;
esp_reset_reason_t g_reason;
int g_minMv = 99999, g_maxMv = 0, g_lastMv = 0;
uint32_t g_dips = 0;
bool g_inDip = false;

void resetStats() {
  g_minMv = 99999;
  g_maxMv = 0;
  g_dips = 0;
  g_inDip = false;
}

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
  M5.update();
  if (M5.Touch.getDetail().wasPressed()) {
    resetStats();
    Serial.println("rail stats reset by tap");
  }

  // Fast path: sample the 5 V input as often as the bus allows, track min/max and dips.
  int mv = M5.Power.getVBUSVoltage();
  g_lastMv = mv;
  if (mv > 0 && mv < g_minMv) g_minMv = mv;
  if (mv > g_maxMv) g_maxMv = mv;
  if (mv < kDipMv && !g_inDip) {
    g_inDip = true;
    g_dips++;
    Serial.printf("rail dip mv=%d dips=%lu t=%lu\n", mv, (unsigned long)g_dips, (unsigned long)millis());
  } else if (mv >= kDipMv + 100) {
    g_inDip = false;
  }

  // Screen twice a second.
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw >= 500) {
    lastDraw = millis();
    screenLine(3, mv > 4000 ? TFT_WHITE : TFT_RED, "5V input: %d mV (min %d, max %d)", mv, g_minMv, g_maxMv);
    screenLine(5, TFT_WHITE, "uptime: %lu s", (unsigned long)(millis() / 1000));
    screenLine(6, g_dips ? TFT_RED : TFT_GREEN, "dips <%.1fV: %lu (tap = reset)", kDipMv / 1000.0f,
               (unsigned long)g_dips);
  }

  // Safety readback + serial line every 2 s, as before.
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll >= 2000) {
    lastPoll = millis();
    PowerBits b = readPowerBits();
    if (!b.safe()) b = forcePowerOutputsOff();
    screenLine(4, b.safe() ? TFT_GREEN : TFT_RED, b.safe() ? "outputs off: SAFE" : "output ON: unplug!");
    Serial.printf("rail uptime_s=%lu vbus_mv=%d min_mv=%d max_mv=%d dips=%lu safe=%d\n",
                  (unsigned long)(millis() / 1000), g_lastMv, g_minMv, g_maxMv, (unsigned long)g_dips, b.safe());
  }
  delay(5);
}
