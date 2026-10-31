// Input abstraction: physical sources -> gestures -> logical actions.
//
// The UI and the app only ever see Actions. Swapping the Mini Button for a nicer one, adding
// feature buttons or a scroll wheel means: write a driver that emits (Source, Gesture, value)
// and add bindings. No UI change. Default bindings and the text format: docs/FIRMWARE.md#input.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace x4 {
namespace input {

enum class Action : uint8_t {
  None = 0,
  Shutter,      // take a shot
  Next,         // next item / page
  Prev,         // previous item / page
  Select,       // confirm
  Back,         // cancel / go back
  Scroll,       // value = detents (+ clockwise / down)
  Menu,         // open settings
  Diagnostics,  // open diagnostics screen
  FlashToggle,  // flash on/off for next shots
  PreviewToggle,
  kCount
};

enum class Source : uint8_t {
  ShutterButton = 0,  // Mini Button Unit on Port A (G1)
  Touch,              // touchscreen gestures
  Button1,            // future feature buttons
  Button2,
  Button3,
  Encoder,            // future scroll wheel / encoder
  Web,                // web debug page buttons
  kCount
};

enum class Gesture : uint8_t {
  Press = 0,  // raw edge down (fastest, used for the shutter)
  Release,
  Click,      // press+release shorter than long-press
  LongPress,  // held past btn_long_ms (fires once while held)
  DoubleClick,
  SwipeLeft,
  SwipeRight,
  SwipeUp,
  SwipeDown,
  Rotate,     // encoder; value = detents
  kCount
};

struct Binding {
  Source src;
  Gesture g;
  Action a;
};

struct Event {
  Action action;
  int16_t value;  // detents for Scroll, 0 otherwise
  Source src;
  Gesture g;
  uint32_t t;     // ms
};

const char* actionName(Action a);
const char* sourceName(Source s);
const char* gestureName(Gesture g);
bool parseAction(const char* s, Action* out);
bool parseSource(const char* s, Source* out);
bool parseGesture(const char* s, Gesture* out);

class InputMap {
 public:
  static constexpr size_t kMax = 32;
  InputMap();                 // empty
  void loadDefaults();        // built-in table (see input.cpp)
  void clear() { n_ = 0; }
  bool bind(Source s, Gesture g, Action a);  // replaces an existing (s,g) binding
  void unbind(Source s, Gesture g);
  Action lookup(Source s, Gesture g) const;
  // Parses "shutter.press=shutter,shutter.long=diagnostics,encoder.rotate=scroll".
  // Returns number of bindings applied; stops at the first bad entry (*badAt = its index).
  int parse(const char* spec, int* badAt = nullptr);
  size_t toString(char* buf, size_t cap) const;
  size_t size() const { return n_; }
  const Binding& at(size_t i) const { return b_[i]; }
  // Convenience: build an Event if a binding exists. Returns false if unbound.
  bool map(Source s, Gesture g, int16_t value, uint32_t t, Event* out) const;

 private:
  Binding b_[kMax];
  size_t n_ = 0;
};

// Debounced button with click / long-press / optional double-click detection.
// Feed the raw level (true = pressed) every poll (>= every 5 ms) or on each edge.
class Button {
 public:
  struct Timing {
    uint16_t debounceMs = 25;
    uint16_t longMs = 600;
    uint16_t doubleMs = 0;  // 0 = no double-click (click fires on release immediately)
  };
  Button() = default;
  explicit Button(const Timing& t) : t_(t) {}
  void setTiming(const Timing& t) { t_ = t; }
  // Returns a bitmask of gestures that happened in this update (bit = 1 << Gesture).
  uint32_t update(uint32_t nowMs, bool rawPressed);
  bool pressed() const { return stable_; }

 private:
  Timing t_;
  bool raw_ = false;
  bool stable_ = false;
  uint32_t rawSince_ = 0;
  uint32_t pressedAt_ = 0;
  uint32_t releasedAt_ = 0;
  bool released_ = false;
  bool longFired_ = false;
  bool pendingClick_ = false;
  uint32_t pendingSince_ = 0;
};

inline uint32_t bit(Gesture g) { return 1u << (uint8_t)g; }

// Turns touch samples into swipes/taps. Thresholds in pixels.
class SwipeDetector {
 public:
  // Returns Gesture::kCount if nothing happened.
  Gesture update(uint32_t nowMs, bool touching, int16_t x, int16_t y);

 private:
  bool down_ = false;
  int16_t x0_ = 0, y0_ = 0, x_ = 0, y_ = 0;
  uint32_t t0_ = 0;
};

}  // namespace input
}  // namespace x4
