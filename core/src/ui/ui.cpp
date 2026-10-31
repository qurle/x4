// x4 UI v1. Hand-written LVGL from ui/tokens.json (placeholder values until the Figma tokens are
// imported; docs/UI.md lists every screen, state, transition and compromise).
//
// Main screen ("four eyes"): four rings, one per camera. Ring = camera state at a glance:
//   faint outline = offline, paper outline = online, accent pulse = capturing,
//   progress arc = transferring, solid ok = stored, error = failed.
// Big serif shot counter bottom-left, state line top, toasts bottom-centre.
#include "ui.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>

#include "../app/app.h"
#include "../display/lvgl_port.h"
#include "../log/log_setup.h"
#include "tokens.h"
#include "x4/log.h"
#include "x4/version.h"

namespace x4ui {
namespace {

using namespace tok;
using x4core::Status;
namespace app = x4core::app;

// ---------------------------------------------------------------- helpers
lv_obj_t* label(lv_obj_t* parent, const lv_font_t* f, uint32_t color, const char* txt) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, c(color), 0);
  lv_label_set_text(l, txt);
  return l;
}

void setText(lv_obj_t* l, const char* txt) {
  // Avoid invalidating (and redrawing) unchanged labels.
  const char* cur = lv_label_get_text(l);
  if (!cur || strcmp(cur, txt) != 0) lv_label_set_text(l, txt);
}

lv_obj_t* screenBase() {
  lv_obj_t* s = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(s, c(kBg), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(s, 0, 0);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

void animOpa(void* o, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }
void animY(void* o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t*)o, v, 0); }
void animBgOpa(void* o, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }
void animArcW(void* o, int32_t v) { lv_obj_set_style_arc_width((lv_obj_t*)o, v, LV_PART_MAIN); }

void anim(lv_obj_t* o, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms, lv_anim_path_cb_t path,
          uint32_t delay = 0) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_exec_cb(&a, cb);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, path);
  lv_anim_start(&a);
}

// Fade + rise reveal used across the UI (staggered by index).
void reveal(lv_obj_t* o, int idx) {
  lv_obj_set_style_opa(o, LV_OPA_TRANSP, 0);
  anim(o, animOpa, LV_OPA_TRANSP, LV_OPA_COVER, kMsBase, pathBase(), idx * kMsStagger);
  anim(o, animY, 10, 0, kMsBase, pathBase(), idx * kMsStagger);
}

lv_obj_t* pillButton(lv_obj_t* parent, const char* txt, bool primary, lv_event_cb_t cb, void* ud) {
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_style_radius(b, kRadiusPill, 0);
  lv_obj_set_style_bg_color(b, c(primary ? kAccent : kSurface2), 0);
  lv_obj_set_style_bg_color(b, c(primary ? kFlash : kLine), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(b, primary ? 0 : kBorderHairline, 0);
  lv_obj_set_style_border_color(b, c(kLine), 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_pad_hor(b, kSpaceM, 0);
  lv_obj_set_style_pad_ver(b, kSpaceS, 0);
  // Press feedback: quick shrink, springs back on release.
  lv_obj_set_style_transform_scale(b, 240, LV_STATE_PRESSED);
  static lv_style_transition_dsc_t tr;
  static const lv_style_prop_t props[] = {LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y, LV_STYLE_BG_COLOR,
                                          LV_STYLE_PROP_INV};
  static bool init = false;
  if (!init) {
    lv_style_transition_dsc_init(&tr, props, lv_anim_path_overshoot, kMsFast, 0, nullptr);
    init = true;
  }
  lv_obj_set_style_transition(b, &tr, 0);
  lv_obj_set_style_transition(b, &tr, LV_STATE_PRESSED);
  lv_obj_t* l = label(b, fontBodyBold(), primary ? kBg : kFg, txt);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

void beep(uint16_t f1, uint16_t ms1, uint16_t f2 = 0, uint16_t ms2 = 0) {
  if (!M5.Speaker.isEnabled()) return;
  M5.Speaker.tone(f1, ms1);
  if (f2) M5.Speaker.tone(f2, ms2, 1, false);
}

// ---------------------------------------------------------------- state
struct Eye {
  lv_obj_t* ring;
  lv_obj_t* num;
  lv_obj_t* cap;
  uint8_t look = 255;
};

struct {
  bool hello = false;
  Screen cur = Screen::Splash;
  // splash
  lv_obj_t* splash = nullptr;
  // main
  lv_obj_t* main = nullptr;
  lv_obj_t* preview = nullptr;
  lv_image_dsc_t previewDsc{};
  M5Canvas* canvas = nullptr;
  lv_obj_t* stateL = nullptr;
  lv_obj_t* infoL = nullptr;
  lv_obj_t* counter = nullptr;
  lv_obj_t* counterCap = nullptr;
  lv_obj_t* hint = nullptr;
  lv_obj_t* flashL = nullptr;
  lv_obj_t* whiteout = nullptr;
  Eye eye[4];
  uint32_t shots = UINT32_MAX;
  // toast
  lv_obj_t* toast = nullptr;
  lv_obj_t* toastL = nullptr;
  // diagnostics
  lv_obj_t* diag = nullptr;
  lv_obj_t* diagSys = nullptr;
  lv_obj_t* diagCams = nullptr;
  lv_obj_t* diagLog = nullptr;
  lv_obj_t* touchDot = nullptr;
  lv_obj_t* confirm = nullptr;
  uint32_t lastDiag = 0;
  uint32_t splashUntil = 0;
} g;

// ---------------------------------------------------------------- splash
void buildSplash() {
  g.splash = screenBase();
  lv_obj_t* t = label(g.splash, fontDisplay(), kFg, "Four Eyes");
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -12);
  lv_obj_t* v = label(g.splash, fontCaption(), kFgMuted, "x4 · " X4_FW_VERSION);
  lv_obj_align(v, LV_ALIGN_CENTER, 0, 34);
  reveal(t, 0);
  reveal(v, 4);
}

// ---------------------------------------------------------------- main
void onScreenLongPress(lv_event_t*) { show(Screen::Diagnostics); }

void buildMain() {
  g.main = screenBase();
  lv_obj_add_event_cb(g.main, onScreenLongPress, LV_EVENT_LONG_PRESSED, nullptr);

  // Live preview fills the screen behind everything (RGB565 big-endian straight from TJpgDec).
  g.preview = lv_image_create(g.main);
  lv_obj_set_size(g.preview, kScreenW, kScreenH);
  lv_obj_align(g.preview, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(g.preview, LV_OBJ_FLAG_HIDDEN);

  g.stateL = label(g.main, fontCaption(), kFgMuted, "STARTING");
  lv_obj_align(g.stateL, LV_ALIGN_TOP_LEFT, kSpaceL, kSpaceM);
  g.infoL = label(g.main, fontCaption(), kFgMuted, "");
  lv_obj_align(g.infoL, LV_ALIGN_TOP_RIGHT, -kSpaceL, kSpaceM);

  // Four eyes.
  constexpr int32_t kD = 58, kGap = 14;
  const int32_t x0 = (kScreenW - (4 * kD + 3 * kGap)) / 2;
  for (int i = 0; i < 4; i++) {
    Eye& e = g.eye[i];
    e.ring = lv_arc_create(g.main);
    lv_obj_set_size(e.ring, kD, kD);
    lv_obj_set_pos(e.ring, x0 + i * (kD + kGap), 62);
    lv_arc_set_bg_angles(e.ring, 0, 360);
    lv_arc_set_rotation(e.ring, 270);
    lv_arc_set_range(e.ring, 0, 100);
    lv_arc_set_value(e.ring, 0);
    lv_obj_remove_style(e.ring, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(e.ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(e.ring, kBorderStrong, LV_PART_MAIN);
    lv_obj_set_style_arc_width(e.ring, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(e.ring, c(kFgFaint), LV_PART_MAIN);
    lv_obj_set_style_arc_color(e.ring, c(kOk), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(e.ring, true, LV_PART_INDICATOR);
    char n[2] = {(char)('1' + i), 0};
    e.num = label(e.ring, fontHeading(), kFgFaint, n);
    lv_obj_center(e.num);
    e.cap = label(g.main, fontCaption(), kFgFaint, "off");
    lv_obj_set_width(e.cap, kD + kGap);
    lv_obj_set_style_text_align(e.cap, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(e.cap, x0 + i * (kD + kGap) - kGap / 2, 62 + kD + kSpaceXS);
    reveal(e.ring, i + 1);
  }

  g.counter = label(g.main, fontTitle(), kFg, "0");
  lv_obj_align(g.counter, LV_ALIGN_BOTTOM_LEFT, kSpaceL, -kSpaceS);
  g.counterCap = label(g.main, fontCaption(), kFgMuted, "shots");
  lv_obj_align_to(g.counterCap, g.counter, LV_ALIGN_OUT_RIGHT_BOTTOM, kSpaceS, -6);
  g.hint = label(g.main, fontCaption(), kFgMuted, "");
  lv_obj_align(g.hint, LV_ALIGN_BOTTOM_RIGHT, -kSpaceL, -kSpaceM - 14);
  g.flashL = label(g.main, fontCaption(), kFgFaint, "flash off");
  lv_obj_align(g.flashL, LV_ALIGN_BOTTOM_RIGHT, -kSpaceL, -kSpaceM);
  reveal(g.counter, 5);

  // Shutter white-out lives on the top layer so it covers every screen.
  g.whiteout = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(g.whiteout);
  lv_obj_set_size(g.whiteout, kScreenW, kScreenH);
  lv_obj_set_style_bg_color(g.whiteout, c(kFlash), 0);
  lv_obj_set_style_bg_opa(g.whiteout, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(g.whiteout, LV_OBJ_FLAG_CLICKABLE);

  // Toast pill (top layer, hidden).
  g.toast = lv_obj_create(lv_layer_top());
  lv_obj_set_style_radius(g.toast, kRadiusPill, 0);
  lv_obj_set_style_bg_color(g.toast, c(kSurface2), 0);
  lv_obj_set_style_border_width(g.toast, kBorderHairline, 0);
  lv_obj_set_style_border_color(g.toast, c(kLine), 0);
  lv_obj_set_style_pad_hor(g.toast, kSpaceM, 0);
  lv_obj_set_style_pad_ver(g.toast, kSpaceXS + 2, 0);
  lv_obj_set_size(g.toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_remove_flag(g.toast, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(g.toast, LV_OBJ_FLAG_SCROLLABLE);
  g.toastL = label(g.toast, fontCaption(), kFg, "");
  lv_obj_align(g.toast, LV_ALIGN_BOTTOM_MID, 0, -kSpaceXXL - kSpaceL);
  lv_obj_add_flag(g.toast, LV_OBJ_FLAG_HIDDEN);
}

// Ring looks: 0 off, 1 online, 2 capturing, 3 transferring, 4 stored, 5 failed, 6 registered-but-offline
void setEyeLook(Eye& e, uint8_t look, int pct, const char* cap) {
  setText(e.cap, cap);
  if (look == 3) lv_arc_set_value(e.ring, pct);
  if (look == e.look) return;
  e.look = look;
  uint32_t main = kFgFaint, num = kFgFaint, ind = kOk, capc = kFgFaint;
  int val = 0;
  switch (look) {
    case 1: main = kFg; num = kFg; capc = kFgMuted; break;
    case 2: main = kAccent; num = kAccent; capc = kAccent; break;
    case 3: main = kLine; num = kFg; ind = kOk; val = pct; capc = kFgMuted; break;
    case 4: main = kOk; num = kOk; ind = kOk; val = 100; capc = kOk; break;
    case 5: main = kError; num = kError; capc = kError; break;
    case 6: main = kError; num = kFgFaint; capc = kError; break;
    default: break;
  }
  lv_obj_set_style_arc_color(e.ring, c(main), LV_PART_MAIN);
  lv_obj_set_style_arc_color(e.ring, c(ind), LV_PART_INDICATOR);
  lv_obj_set_style_text_color(e.num, c(num), 0);
  lv_obj_set_style_text_color(e.cap, c(capc), 0);
  lv_arc_set_value(e.ring, val);
  if (look == 2) {
    // capture pulse: ring thickens and settles
    anim(e.ring, animArcW, 8, kBorderStrong, kMsSpring, pathSpring());
  }
}

const char* stateText(const Status& s, char* buf, size_t cap) {
  using x4::shoot::State;
  switch (s.state) {
    case State::Booting: return "STARTING";
    case State::Idle: snprintf(buf, cap, "WAITING FOR CAMERAS %d/4", s.camsOnline); return buf;
    case State::Armed: snprintf(buf, cap, "READY · %d EYES", s.camsOnline); return buf;
    case State::Preview: snprintf(buf, cap, "LIVE · %d EYES", s.camsOnline); return buf;
    case State::Shooting: return "CAPTURING";
    case State::Transferring: {
      int stored = 0, exp = 0;
      for (const auto& c : s.cam) {
        exp += c.expected;
        stored += c.stored;
      }
      snprintf(buf, cap, "SAVING %d/%d", stored, exp);
      return buf;
    }
    case State::Error:
      if (s.faults & (uint8_t)x4::shoot::Fault::SdMissing) return "NO SD CARD";
      if (s.faults & (uint8_t)x4::shoot::Fault::SdFull) return "SD CARD FULL";
      if (s.faults & (uint8_t)x4::shoot::Fault::Overheat) return "TOO HOT · COOLING";
      return "ERROR";
  }
  return "";
}

void updateMain(const Status& s) {
  char buf[64];
  setText(g.stateL, stateText(s, buf, sizeof(buf)));
  lv_obj_set_style_text_color(g.stateL, c(s.state == x4::shoot::State::Error ? kError : s.overheat || s.hot ? kWarn : kFgMuted), 0);
  char sd[20];
  if (s.sdMounted) snprintf(sd, sizeof(sd), "SD %lu MB", (unsigned long)s.sdFreeMb);
  else snprintf(sd, sizeof(sd), "NO SD");
  snprintf(buf, sizeof(buf), "%.0f°  %s", s.tempC, sd);
  setText(g.infoL, buf);

  if (s.shotsTaken != g.shots) {
    bool first = g.shots == UINT32_MAX;
    g.shots = s.shotsTaken;
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.shotsTaken);
    setText(g.counter, buf);
    lv_obj_align_to(g.counterCap, g.counter, LV_ALIGN_OUT_RIGHT_BOTTOM, kSpaceS, -6);
    if (!first) anim(g.counter, animY, 16, 0, kMsSpring, pathSpring());  // counter rolls up
  }

  bool shooting = s.shooting;
  for (int i = 0; i < 4; i++) {
    const auto& c = s.cam[i];
    uint8_t look;
    int pct = 0;
    const char* cap;
    char cb[16];
    if (shooting && c.expected) {
      if (c.failed) { look = 5; cap = "failed"; }
      else if (c.stored) { look = 4; cap = "saved"; }
      else if (c.ok) {
        look = 3;
        pct = c.xferTotal ? (int)(100ull * c.xferBytes / c.xferTotal) : 0;
        snprintf(cb, sizeof(cb), "%d%%", pct);
        cap = cb;
      } else { look = 2; cap = "shoot"; }
    } else if (c.online) {
      look = 1;
      snprintf(cb, sizeof(cb), "%d°", c.tempC);
      cap = cb;
    } else if (c.registered) { look = 6; cap = "lost"; }
    else { look = 0; cap = "off"; }
    setEyeLook(g.eye[i], look, pct, cap);
  }

  if (s.flashArmed) {
    if (s.flashReady) snprintf(buf, sizeof(buf), "flash ready");
    else snprintf(buf, sizeof(buf), "flash %lus", (unsigned long)((s.flashReadyInMs + 999) / 1000));
  } else {
    snprintf(buf, sizeof(buf), "flash off");
  }
  setText(g.flashL, buf);
  lv_obj_set_style_text_color(g.flashL, c(s.flashArmed ? (s.flashReady ? kFlash : kWarn) : kFgFaint), 0);
  using x4::shoot::State;
  setText(g.hint, s.state == State::Armed || s.state == State::Preview ? "press to shoot"
                  : s.state == State::Transferring                    ? "saving…"
                                                                      : "");
}

// ---------------------------------------------------------------- diagnostics / bring-up
void onBack(lv_event_t*) { show(Screen::Main); }
void onArm(lv_event_t*) { app::request(app::Request::FlashArm); }
void onBench(lv_event_t*) { app::request(app::Request::SdBench); }
void onShoot(lv_event_t*) { app::request(app::Request::Shutter); }
void closeConfirm() {
  if (g.confirm) {
    lv_obj_delete(g.confirm);
    g.confirm = nullptr;
  }
}
void onConfirmYes(lv_event_t*) {
  closeConfirm();
  app::request(app::Request::FlashTest);
}
void onConfirmNo(lv_event_t*) { closeConfirm(); }

// On-screen confirmation for the flash test (requirement: "on-screen confirm").
void onFlashTest(lv_event_t*) {
  closeConfirm();
  g.confirm = lv_obj_create(lv_layer_top());
  lv_obj_set_size(g.confirm, kScreenW, kScreenH);
  lv_obj_set_style_bg_color(g.confirm, c(kBg), 0);
  lv_obj_set_style_bg_opa(g.confirm, LV_OPA_80, 0);
  lv_obj_set_style_border_width(g.confirm, 0, 0);
  lv_obj_t* t = label(g.confirm, fontHeading(), kFg, "Fire one 50 ms flash?");
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -40);
  lv_obj_t* d = label(g.confirm, fontCaption(), kWarn, "Flashlight Unit on G2 (Port A yellow).\nDon't look into it.");
  lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(d, LV_ALIGN_CENTER, 0, -6);
  lv_obj_t* y = pillButton(g.confirm, "Fire", true, onConfirmYes, nullptr);
  lv_obj_align(y, LV_ALIGN_CENTER, 50, 44);
  lv_obj_t* n = pillButton(g.confirm, "Cancel", false, onConfirmNo, nullptr);
  lv_obj_align(n, LV_ALIGN_CENTER, -50, 44);
  reveal(t, 0);
}

void onTouchTest(lv_event_t* e) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  if (!g.touchDot) return;
  lv_obj_remove_flag(g.touchDot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(g.touchDot, p.x - 6, p.y - 6);
  X4_LOGI("ui", "touch_test", "x=%d y=%d", (int)p.x, (int)p.y);
  (void)e;
}

void buildDiag() {
  g.diag = screenBase();
  lv_obj_t* title = label(g.diag, fontHeading(), kFg, g.hello ? "Bring-up" : "Diagnostics");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, kSpaceL, kSpaceS);

  lv_obj_t* col = lv_obj_create(g.diag);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, kScreenW - 2 * kSpaceL, 150);
  lv_obj_align(col, LV_ALIGN_TOP_LEFT, kSpaceL, 40);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, kSpaceXS, 0);
  lv_obj_add_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(col, onTouchTest, LV_EVENT_PRESSED, nullptr);
  g.diagSys = label(col, fontCaption(), kFg, "");
  g.diagCams = label(col, fontCaption(), kFgMuted, "");
  g.diagLog = label(col, fontCaption(), kFgFaint, "");
  lv_obj_set_width(g.diagLog, kScreenW - 2 * kSpaceL);
  lv_label_set_long_mode(g.diagLog, LV_LABEL_LONG_MODE_CLIP);

  lv_obj_t* row = lv_obj_create(g.diag);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, kScreenW, 44);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -kSpaceXS);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (!g.hello) pillButton(row, "Back", false, onBack, nullptr);
  pillButton(row, "Arm", false, onArm, nullptr);
  pillButton(row, "Flash", false, onFlashTest, nullptr);
  pillButton(row, "SD", false, onBench, nullptr);
  if (!g.hello) pillButton(row, "Shoot", true, onShoot, nullptr);

  g.touchDot = lv_obj_create(g.diag);
  lv_obj_set_size(g.touchDot, 12, 12);
  lv_obj_set_style_radius(g.touchDot, kRadiusPill, 0);
  lv_obj_set_style_bg_color(g.touchDot, c(kAccent), 0);
  lv_obj_set_style_border_width(g.touchDot, 0, 0);
  lv_obj_add_flag(g.touchDot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(g.touchDot, LV_OBJ_FLAG_CLICKABLE);
}

void updateDiag(const Status& s, uint32_t now) {
  if (now - g.lastDiag < 250) return;  // 4 Hz is plenty for text
  g.lastDiag = now;
  char b[640];
  snprintf(b, sizeof(b),
           "fw %s %s · up %lus · fps %.0f (worst %.1f ms)\n"
           "5V out %s · OTG %s · rail %ld mV · %s\n"
           "temp %.1f°C%s · heap %luk (min %luk) · psram %luk\n"
           "reset %s · boots %lu · brown-outs %lu\n"
           "SD %s %lu/%lu MB · w %.0f r %.0f kB/s\n"
           "button %s · flash %s%s · pulses %lu",
           X4_FW_VERSION, X4_GIT_HASH, (unsigned long)(s.uptimeMs / 1000), s.fps, s.frameMaxUs / 1000.0f,
           s.busOutEn ? "ON!" : "off", s.usbOtgEn ? "ON!" : "off", (long)s.vbusMv, s.powerSafe ? "safe" : "UNSAFE",
           s.tempC, s.overheat ? " HOT" : s.hot ? " warm" : "", (unsigned long)(s.heap / 1024),
           (unsigned long)(s.heapMin / 1024), (unsigned long)(s.psram / 1024), s.resetReason ? s.resetReason : "?",
           (unsigned long)s.bootCount, (unsigned long)s.brownouts, s.sdMounted ? "ok" : "MISSING",
           (unsigned long)s.sdFreeMb, (unsigned long)s.sdTotalMb, s.sdWriteKBps, s.sdReadKBps,
           s.buttonDown ? "DOWN" : "up", s.flashEnabled ? "armed" : "disarmed",
           s.flashEnabled ? (s.flashReady ? " ready" : " cooling") : "", (unsigned long)s.flashPulses);
  setText(g.diagSys, b);
  lv_obj_set_style_text_color(g.diagSys, c(s.powerSafe ? kFg : kError), 0);
  size_t w = 0;
  for (int i = 0; i < 4 && w < sizeof(b); i++) {
    const auto& c = s.cam[i];
    w += snprintf(b + w, sizeof(b) - w, "cam%d %s %s %d° rssi %d hb %lums last %s %+.1fms\n", i + 1,
                  c.online ? "on " : "off", c.ip, c.tempC, c.rssi, (unsigned long)c.lastHbAgeMs,
                  c.stored ? "stored" : c.failed ? "failed" : c.ok ? "captured" : "-", c.capOffsetUs / 1000.0f);
  }
  if (w < sizeof(b))
    snprintf(b + w, sizeof(b) - w, "last shot #%lu: %d/%d in %lums · skew %.1fms · stations %d",
             (unsigned long)s.lastShotId, s.lastStored, s.lastExpected, (unsigned long)s.lastTotalMs,
             s.lastSkewUs / 1000.0f, s.apStations);
  setText(g.diagCams, b);
  char tail[900];
  x4core::logsys::ring().tail(tail, sizeof(tail), 6);
  setText(g.diagLog, tail);
}

}  // namespace

// ---------------------------------------------------------------- public API
void begin(bool helloMode) {
  g.hello = helloMode;
  buildSplash();
  lv_screen_load(g.splash);
  buildMain();
  buildDiag();
  g.splashUntil = millis() + 1400;
  g.cur = Screen::Splash;
  X4_LOGI("ui", "begin", "mode=%s", helloMode ? "hello" : "full");
}

Screen current() { return g.cur; }

void show(Screen s) {
  if (s == g.cur) return;
  lv_obj_t* scr = s == Screen::Main ? g.main : s == Screen::Diagnostics ? g.diag : g.splash;
  lv_screen_load_anim(scr, s == Screen::Diagnostics ? LV_SCREEN_LOAD_ANIM_MOVE_TOP : LV_SCREEN_LOAD_ANIM_FADE_IN,
                      kMsBase, 0, false);
  X4_LOGD("ui", "screen", "from=%d to=%d", (int)g.cur, (int)s);
  g.cur = s;
}

void update(const Status& s, uint32_t now) {
  if (g.cur == Screen::Splash && (int32_t)(now - g.splashUntil) >= 0)
    show(g.hello ? Screen::Diagnostics : Screen::Main);
  if (g.cur == Screen::Main || g.cur == Screen::Splash) updateMain(s);
  if (g.cur == Screen::Diagnostics) updateDiag(s, now);
}

void onAction(const x4::input::Event& e) {
  using x4::input::Action;
  switch (e.action) {
    case Action::Diagnostics:
      show(g.cur == Screen::Diagnostics && !g.hello ? Screen::Main : Screen::Diagnostics);
      break;
    case Action::Menu: show(Screen::Diagnostics); break;
    case Action::Back:
      closeConfirm();
      if (!g.hello) show(Screen::Main);
      break;
    default: break;
  }
}

void shutterMoment(bool flash) {
  // White-out: instant up, eased fade. Feels like a mechanical shutter + flash.
  lv_anim_delete(g.whiteout, animBgOpa);
  anim(g.whiteout, animBgOpa, flash ? LV_OPA_COVER : LV_OPA_70, LV_OPA_TRANSP, kMsSlow, pathBase());
  beep(2600, 18, 1300, 30);
  if (g.cur != Screen::Main && !g.hello) show(Screen::Main);
}

void shotCaptured() { beep(1800, 12); }

void shotStored(bool partial, uint32_t totalMs, bool overBudget) {
  char b[48];
  if (partial) snprintf(b, sizeof(b), "saved with missing eyes · %.1fs", totalMs / 1000.0f);
  else snprintf(b, sizeof(b), "saved · %.1fs", totalMs / 1000.0f);
  toast(b, partial ? kWarn : overBudget ? kWarn : kOk);
  if (!partial) beep(2200, 20, 3300, 40);
}

void shutterRefused(const char* why) {
  char b[48];
  snprintf(b, sizeof(b), "not ready · %s", why);
  toast(b, kWarn, 1500);
  // little "no" shake on the state line
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, g.stateL);
  lv_anim_set_exec_cb(&a, [](void* o, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t*)o, v, 0); });
  lv_anim_set_values(&a, -6, 6);
  lv_anim_set_duration(&a, 50);
  lv_anim_set_playback_duration(&a, 50);
  lv_anim_set_repeat_count(&a, 2);
  lv_anim_set_completed_cb(&a, [](lv_anim_t* an) { lv_obj_set_style_translate_x((lv_obj_t*)an->var, 0, 0); });
  lv_anim_start(&a);
  beep(300, 60);
}

void toast(const char* text, uint32_t colorHex, uint32_t ms) {
  lv_label_set_text(g.toastL, text);
  lv_obj_set_style_text_color(g.toastL, c(colorHex), 0);
  lv_obj_remove_flag(g.toast, LV_OBJ_FLAG_HIDDEN);
  lv_anim_delete(g.toast, nullptr);
  anim(g.toast, animOpa, LV_OPA_TRANSP, LV_OPA_COVER, kMsFast, pathFast());
  anim(g.toast, animY, 12, 0, kMsBase, pathSpring());
  anim(g.toast, animOpa, LV_OPA_COVER, LV_OPA_TRANSP, kMsBase, pathBase(), ms);
}

void setPreviewJpeg(const uint8_t* jpg, uint32_t len) {
  if (!g.canvas) {
    g.canvas = new M5Canvas(&M5.Display);
    g.canvas->setPsram(true);
    g.canvas->setColorDepth(16);
    if (!g.canvas->createSprite(kScreenW, kScreenH)) {
      X4_LOGE("ui", "preview_alloc", "w=%ld h=%ld", (long)kScreenW, (long)kScreenH);
      delete g.canvas;
      g.canvas = nullptr;
      return;
    }
    g.previewDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    g.previewDsc.header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;  // M5GFX sprites are big-endian RGB565
    g.previewDsc.header.w = kScreenW;
    g.previewDsc.header.h = kScreenH;
    g.previewDsc.header.stride = kScreenW * 2;
    g.previewDsc.data_size = kScreenW * kScreenH * 2;
    g.previewDsc.data = (const uint8_t*)g.canvas->getBuffer();
  }
  uint32_t t0 = micros();
  g.canvas->drawJpg(jpg, len, 0, 0, kScreenW, kScreenH);
  uint32_t us = micros() - t0;
  lv_image_set_src(g.preview, &g.previewDsc);
  lv_obj_invalidate(g.preview);
  lv_obj_remove_flag(g.preview, LV_OBJ_FLAG_HIDDEN);
  X4_LOG_EVERY(5000, x4::log::Level::Debug, "ui", "preview_decode", "us=%lu bytes=%lu", (unsigned long)us,
               (unsigned long)len);
}

void clearPreview() {
  if (g.preview) lv_obj_add_flag(g.preview, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace x4ui
