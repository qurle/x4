#include "inputs.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "../display/lvgl_port.h"
#include "board_pins.h"
#include "x4/log.h"

namespace x4core {
namespace inputs {
namespace {

using namespace x4::input;

InputMap g_map;
QueueHandle_t g_q = nullptr;
Button::Timing g_timing;

void emit(Source s, Gesture g, int16_t value) {
  Event e;
  if (!g_map.map(s, g, value, millis(), &e)) return;
  X4_LOGD("input", "action", "src=%s gesture=%s action=%s value=%d", sourceName(s), gestureName(g),
          actionName(e.action), value);
  if (g_q) xQueueSend(g_q, &e, 0);
}

void emitMask(Source s, uint32_t mask) {
  for (uint8_t g = 0; g < (uint8_t)Gesture::kCount; g++)
    if (mask & (1u << g)) emit(s, (Gesture)g, 0);
}

// ---- GPIO button (Mini Button Unit, future feature buttons) -----------------------------------
class GpioButton : public Driver {
 public:
  GpioButton(const char* n, int pin, bool activeLow, Source src)
      : name_(n), pin_(pin), activeLow_(activeLow), src_(src) {}
  const char* name() const override { return name_; }
  bool begin() override {
    // Internal pull-up; harmless with the Unit Button's own divider (docs/HARDWARE.md).
    pinMode(pin_, activeLow_ ? INPUT_PULLUP : INPUT_PULLDOWN);
    btn_.setTiming(g_timing);
    return true;
  }
  void poll(uint32_t now) override {
    bool raw = digitalRead(pin_) == (activeLow_ ? LOW : HIGH);
    emitMask(src_, btn_.update(now, raw));
  }
  void setTiming(const Button::Timing& t) { btn_.setTiming(t); }
  bool pressed() const { return btn_.pressed(); }

 private:
  const char* name_;
  int pin_;
  bool activeLow_;
  Source src_;
  Button btn_;
};

// ---- touch swipes (taps go to LVGL widgets directly) -------------------------------------------
class TouchSwipes : public Driver {
 public:
  const char* name() const override { return "touch"; }
  bool begin() override { return true; }
  void poll(uint32_t now) override {
    auto s = display::touchSample();
    Gesture g = det_.update(now, s.pressed, s.x, s.y);
    if (g != Gesture::kCount) emit(Source::Touch, g, 0);
  }

 private:
  SwipeDetector det_;
};

// ---- encoder stub ------------------------------------------------------------------------------
// Quadrature wheel on G5/G6 (M5-Bus pins 8/21). Not enabled: no hardware yet. Enable with
// -DX4_ENCODER_QUADRATURE=1. For an I2C encoder unit, see docs/WIRING.md "Spare ports": it needs
// Port A's I2C pins, so the button and flash would move to M5-Bus G8/G7 first.
class QuadratureEncoder : public Driver {
 public:
  const char* name() const override { return "encoder"; }
  bool begin() override {
    pinMode(pins::kEncoderA, INPUT_PULLUP);
    pinMode(pins::kEncoderB, INPUT_PULLUP);
    last_ = state();
    return true;
  }
  void poll(uint32_t) override {
    // Polled Gray-code decoder; good to ~200 detents/s at the loop rate. Use the PCNT peripheral
    // for faster wheels.
    uint8_t s = state();
    if (s == last_) return;
    static const int8_t kTable[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    acc_ += kTable[(last_ << 2) | s];
    last_ = s;
    if (acc_ >= 4 || acc_ <= -4) {
      emit(Source::Encoder, Gesture::Rotate, (int16_t)(acc_ / 4));
      acc_ %= 4;
    }
  }

 private:
  uint8_t state() { return (digitalRead(pins::kEncoderA) << 1) | digitalRead(pins::kEncoderB); }
  uint8_t last_ = 0;
  int8_t acc_ = 0;
};

GpioButton g_shutter("shutter", pins::kShutterButton, pins::kShutterActiveLow, Source::ShutterButton);
TouchSwipes g_touch;
#if X4_EXTRA_BUTTONS
GpioButton g_b1("button1", pins::kExtraButton1, true, Source::Button1);
GpioButton g_b2("button2", pins::kExtraButton2, true, Source::Button2);
#endif
#if X4_ENCODER_QUADRATURE
QuadratureEncoder g_enc;
#endif

Driver* const g_drivers[] = {
    &g_shutter, &g_touch,
#if X4_EXTRA_BUTTONS
    &g_b1, &g_b2,
#endif
#if X4_ENCODER_QUADRATURE
    &g_enc,
#endif
};

}  // namespace

void applyConfig(const x4::cfg::Config& c) {
  g_timing.debounceMs = (uint16_t)c.btn_debounce_ms;
  g_timing.longMs = (uint16_t)c.btn_long_ms;
  g_timing.doubleMs = (uint16_t)c.btn_double_ms;
  g_shutter.setTiming(g_timing);
}

void begin(const x4::cfg::Config& c) {
  g_q = xQueueCreate(16, sizeof(Event));
  g_map.loadDefaults();
  applyConfig(c);
  for (Driver* d : g_drivers) {
    bool ok = d->begin();
    X4_LOGI("input", "driver", "name=%s ok=%d", d->name(), ok);
  }
  char buf[400];
  g_map.toString(buf, sizeof(buf));
  X4_LOGI("input", "bindings", "%s", buf);
}

void poll(uint32_t now) {
  for (Driver* d : g_drivers) d->poll(now);
}

bool next(Event* out) { return g_q && xQueueReceive(g_q, out, 0) == pdTRUE; }

void inject(Source s, Gesture g, int16_t value) { emit(s, g, value); }

InputMap& map() { return g_map; }
bool shutterPressed() { return g_shutter.pressed(); }

}  // namespace inputs
}  // namespace x4core
