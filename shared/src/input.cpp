#include "x4/input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace x4 {
namespace input {
namespace {

const char* const kActionNames[] = {"none",   "shutter", "next",        "prev",
                                    "select", "back",    "scroll",      "menu",
                                    "diagnostics", "flash_toggle", "preview_toggle"};
const char* const kSourceNames[] = {"shutter", "touch",   "button1", "button2",
                                    "button3", "encoder", "web"};
const char* const kGestureNames[] = {"press",       "release",    "click",    "long",
                                     "double",      "swipe_left", "swipe_right", "swipe_up",
                                     "swipe_down",  "rotate"};
static_assert(sizeof(kActionNames) / sizeof(kActionNames[0]) == (size_t)Action::kCount, "names");
static_assert(sizeof(kSourceNames) / sizeof(kSourceNames[0]) == (size_t)Source::kCount, "names");
static_assert(sizeof(kGestureNames) / sizeof(kGestureNames[0]) == (size_t)Gesture::kCount, "names");

template <typename E, size_t N>
bool parseName(const char* const (&names)[N], const char* s, size_t len, E* out) {
  for (size_t i = 0; i < N; i++) {
    if (strlen(names[i]) == len && strncmp(names[i], s, len) == 0) {
      *out = (E)i;
      return true;
    }
  }
  return false;
}

// Default bindings. Kept small on purpose: one physical button must stay instant for the shutter.
const Binding kDefaults[] = {
    {Source::ShutterButton, Gesture::Press, Action::Shutter},
    {Source::ShutterButton, Gesture::LongPress, Action::Diagnostics},
    {Source::Touch, Gesture::SwipeLeft, Action::Next},
    {Source::Touch, Gesture::SwipeRight, Action::Prev},
    {Source::Touch, Gesture::SwipeDown, Action::Back},
    {Source::Touch, Gesture::SwipeUp, Action::Menu},
    {Source::Button1, Gesture::Click, Action::FlashToggle},
    {Source::Button2, Gesture::Click, Action::PreviewToggle},
    {Source::Button3, Gesture::Click, Action::Menu},
    {Source::Encoder, Gesture::Rotate, Action::Scroll},
    {Source::Encoder, Gesture::Click, Action::Select},
    {Source::Encoder, Gesture::LongPress, Action::Back},
    {Source::Web, Gesture::Click, Action::Shutter},
};

}  // namespace

const char* actionName(Action a) {
  return (size_t)a < (size_t)Action::kCount ? kActionNames[(size_t)a] : "?";
}
const char* sourceName(Source s) {
  return (size_t)s < (size_t)Source::kCount ? kSourceNames[(size_t)s] : "?";
}
const char* gestureName(Gesture g) {
  return (size_t)g < (size_t)Gesture::kCount ? kGestureNames[(size_t)g] : "?";
}
bool parseAction(const char* s, Action* out) { return s && parseName(kActionNames, s, strlen(s), out); }
bool parseSource(const char* s, Source* out) { return s && parseName(kSourceNames, s, strlen(s), out); }
bool parseGesture(const char* s, Gesture* out) { return s && parseName(kGestureNames, s, strlen(s), out); }

InputMap::InputMap() {}

void InputMap::loadDefaults() {
  n_ = 0;
  for (const auto& b : kDefaults) bind(b.src, b.g, b.a);
}

bool InputMap::bind(Source s, Gesture g, Action a) {
  for (size_t i = 0; i < n_; i++) {
    if (b_[i].src == s && b_[i].g == g) {
      b_[i].a = a;
      return true;
    }
  }
  if (n_ >= kMax) return false;
  b_[n_++] = Binding{s, g, a};
  return true;
}

void InputMap::unbind(Source s, Gesture g) {
  for (size_t i = 0; i < n_; i++) {
    if (b_[i].src == s && b_[i].g == g) {
      b_[i] = b_[--n_];
      return;
    }
  }
}

Action InputMap::lookup(Source s, Gesture g) const {
  for (size_t i = 0; i < n_; i++)
    if (b_[i].src == s && b_[i].g == g) return b_[i].a;
  return Action::None;
}

bool InputMap::map(Source s, Gesture g, int16_t value, uint32_t t, Event* out) const {
  Action a = lookup(s, g);
  if (a == Action::None) return false;
  if (out) *out = Event{a, value, s, g, t};
  return true;
}

int InputMap::parse(const char* spec, int* badAt) {
  if (badAt) *badAt = -1;
  if (!spec) return 0;
  int applied = 0, idx = 0;
  const char* p = spec;
  while (*p) {
    while (*p == ' ' || *p == ',') p++;
    if (!*p) break;
    const char* start = p;
    while (*p && *p != ',') p++;
    // entry = src.gesture=action
    const char* dot = (const char*)memchr(start, '.', p - start);
    const char* eq = (const char*)memchr(start, '=', p - start);
    Source s;
    Gesture g;
    Action a;
    const char* end = p;
    while (end > start && end[-1] == ' ') end--;
    if (!dot || !eq || eq < dot || !parseName(kSourceNames, start, dot - start, &s) ||
        !parseName(kGestureNames, dot + 1, eq - dot - 1, &g) ||
        !parseName(kActionNames, eq + 1, end - eq - 1, &a)) {
      if (badAt) *badAt = idx;
      return applied;
    }
    if (a == Action::None)
      unbind(s, g);
    else
      bind(s, g, a);
    applied++;
    idx++;
  }
  return applied;
}

size_t InputMap::toString(char* buf, size_t cap) const {
  if (!buf || !cap) return 0;
  size_t w = 0;
  buf[0] = 0;
  for (size_t i = 0; i < n_; i++) {
    int n = snprintf(buf + w, cap - w, "%s%s.%s=%s", i ? "," : "", sourceName(b_[i].src),
                     gestureName(b_[i].g), actionName(b_[i].a));
    if (n < 0 || w + (size_t)n >= cap) break;
    w += (size_t)n;
  }
  return w;
}

uint32_t Button::update(uint32_t now, bool rawPressed) {
  uint32_t out = 0;
  if (rawPressed != raw_) {
    raw_ = rawPressed;
    rawSince_ = now;
  }
  if (!stable_) {
    // Press fires on the FIRST raw edge (minimum shutter latency). Contact bounce right after a
    // release is ignored for debounceMs.
    bool quiet = !released_ || (uint32_t)(now - releasedAt_) >= t_.debounceMs;
    if (raw_ && quiet) {
      stable_ = true;
      pressedAt_ = now;
      longFired_ = false;
      out |= gestureBit(Gesture::Press);
      if (pendingClick_ && t_.doubleMs && (uint32_t)(now - pendingSince_) <= t_.doubleMs) {
        pendingClick_ = false;
        out |= gestureBit(Gesture::DoubleClick);
        longFired_ = true;  // this press already produced a gesture: no click on release
      }
    }
  } else if (!raw_ && (uint32_t)(now - rawSince_) >= t_.debounceMs) {
    // Release needs the contact open for debounceMs.
    stable_ = false;
    released_ = true;
    releasedAt_ = now;
    out |= gestureBit(Gesture::Release);
    if (!longFired_) {
      if (t_.doubleMs) {
        pendingClick_ = true;
        pendingSince_ = now;
      } else {
        out |= gestureBit(Gesture::Click);
      }
    }
  }
  if (stable_ && !longFired_ && (uint32_t)(now - pressedAt_) >= t_.longMs) {
    longFired_ = true;
    pendingClick_ = false;
    out |= gestureBit(Gesture::LongPress);
  }
  if (pendingClick_ && !stable_ && (uint32_t)(now - pendingSince_) > t_.doubleMs) {
    pendingClick_ = false;
    out |= gestureBit(Gesture::Click);
  }
  return out;
}

Gesture SwipeDetector::update(uint32_t now, bool touching, int16_t x, int16_t y) {
  constexpr int kMin = 40;          // px
  constexpr uint32_t kMaxMs = 600;  // slower drags are not swipes
  if (touching) {
    if (!down_) {
      down_ = true;
      x0_ = x;
      y0_ = y;
      t0_ = now;
    }
    x_ = x;
    y_ = y;
    return Gesture::kCount;
  }
  if (!down_) return Gesture::kCount;
  down_ = false;
  int dx = x_ - x0_, dy = y_ - y0_;
  if (now - t0_ > kMaxMs) return Gesture::kCount;
  int ax = abs(dx), ay = abs(dy);
  if (ax < kMin && ay < kMin) return Gesture::kCount;
  if (ax > ay) return dx < 0 ? Gesture::SwipeLeft : Gesture::SwipeRight;
  return dy < 0 ? Gesture::SwipeUp : Gesture::SwipeDown;
}

}  // namespace input
}  // namespace x4
