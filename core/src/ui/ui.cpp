// x4 UI v2 — implemented from the Figma file fgEvP6Ms0bOAbWxfkFki4R, section 73:368 "states".
// Frame → LVGL mapping (details and compromises: docs/UI.md):
//   preview: full / minimal / dithered / portrait   → Main screen (live photo full-bleed)
//   on tap: buttons                                 → tap menu (video · settings · media)
//   settings (2×2 tiles)                            → Settings screen
//   state: saving (n of 4)                          → saving overlay (holographic image)
//   state: error                                    → error overlay (red image)
// Not in the design, kept for bring-up: splash, diagnostics screen, toasts, white-out shutter.
#include "ui.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>

#include "../app/app.h"
#include "../display/lvgl_port.h"
#include "../log/log_setup.h"
#include "assets/assets.h"
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
  const char* cur = lv_label_get_text(l);
  if (!cur || strcmp(cur, txt) != 0) lv_label_set_text(l, txt);
}

void setHidden(lv_obj_t* o, bool hidden) {
  if (hidden != lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

lv_obj_t* screenBase(uint32_t bg) {
  lv_obj_t* s = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(s, c(bg), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(s, 0, 0);
  lv_obj_set_style_border_width(s, 0, 0);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

lv_obj_t* image(lv_obj_t* parent, const lv_image_dsc_t* src) {
  lv_obj_t* i = lv_image_create(parent);
  lv_image_set_src(i, src);
  lv_obj_remove_flag(i, LV_OBJ_FLAG_CLICKABLE);
  return i;
}

void animOpa(void* o, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }
void animY(void* o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t*)o, v, 0); }
void animX(void* o, int32_t v) { lv_obj_set_x((lv_obj_t*)o, v); }
void animBgOpa(void* o, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }

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

// Fade (and optional rise) in/out of an overlay, hiding it when fully faded.
void fadeTo(lv_obj_t* o, bool show, uint32_t ms) {
  bool hidden = lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN);
  if (show && !hidden) return;
  if (!show && hidden) return;
  lv_anim_delete(o, animOpa);
  if (show) {
    lv_obj_set_style_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    anim(o, animOpa, LV_OPA_TRANSP, LV_OPA_COVER, ms, pathBase());
  } else {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, animOpa);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, pathFast());
    lv_anim_set_completed_cb(&a, [](lv_anim_t* an) { lv_obj_add_flag((lv_obj_t*)an->var, LV_OBJ_FLAG_HIDDEN); });
    lv_anim_start(&a);
  }
}

void reveal(lv_obj_t* o, int idx) {
  lv_obj_set_style_opa(o, LV_OPA_TRANSP, 0);
  anim(o, animOpa, LV_OPA_TRANSP, LV_OPA_COVER, kMsBase, pathBase(), idx * kMsStagger);
  anim(o, animY, 10, 0, kMsBase, pathBase(), idx * kMsStagger);
}

lv_obj_t* pillButton(lv_obj_t* parent, const char* txt, bool primary, lv_event_cb_t cb, void* ud) {
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_style_radius(b, kRadiusPill, 0);
  lv_obj_set_style_bg_color(b, c(primary ? kAccent : kSurface2), 0);
  lv_obj_set_style_bg_color(b, c(primary ? kFgMuted : kLine), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_pad_hor(b, kSpaceM, 0);
  lv_obj_set_style_pad_ver(b, kSpaceS, 0);
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

// Ordered (Bayer 4×4) colour dither in place on a big-endian RGB565 buffer: 4 levels per channel.
// Used for the "dithered" effect on the live preview (≈ 3–5 ms per 320×240 frame, ASSUMPTION).
void ditherBE565(uint16_t* px, int32_t w, int32_t h) {
  static const uint8_t kB[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  for (int32_t y = 0; y < h; y++) {
    for (int32_t x = 0; x < w; x++) {
      uint16_t v = px[y * w + x];
      v = (uint16_t)((v >> 8) | (v << 8));
      int32_t r = ((v >> 11) & 0x1F) << 3, g = ((v >> 5) & 0x3F) << 2, b = (v & 0x1F) << 3;
      int32_t t = (kB[y & 3][x & 3] * 85) / 16 - 42;  // threshold offset ±42 (one quantisation step = 85)
      auto q = [t](int32_t c8) {
        int32_t l = (c8 + t + 42) / 85;
        return l < 0 ? 0 : l > 3 ? 3 : l;
      };
      int32_t rq = q(r) * 85, gq = q(g) * 85, bq = q(b) * 85;
      v = (uint16_t)(((rq >> 3) << 11) | ((gq >> 2) << 5) | (bq >> 3));
      px[y * w + x] = (uint16_t)((v >> 8) | (v << 8));
    }
  }
}

// ---------------------------------------------------------------- state
struct Tile {
  lv_obj_t* box;
  lv_obj_t* label;
  lv_obj_t* a;  // decoration: bar / thumb / dither strip
  lv_obj_t* b;  // second bar
};

struct {
  bool hello = false;
  Screen cur = Screen::Splash;
  lv_obj_t* splash = nullptr;
  // main (viewfinder)
  lv_obj_t* main = nullptr;
  lv_obj_t* preview = nullptr;
  lv_image_dsc_t previewDsc{};
  M5Canvas* canvas = nullptr;
  bool hasFrame = false;
  lv_obj_t* placeholder = nullptr;
  lv_obj_t* dimL = nullptr;
  lv_obj_t* dimR = nullptr;
  lv_obj_t* footer = nullptr;
  lv_obj_t* footL = nullptr;
  lv_obj_t* footR = nullptr;
  lv_obj_t* flashInd = nullptr;
  lv_obj_t* menu = nullptr;
  uint32_t menuUntil = 0;
  lv_obj_t* saving = nullptr;
  lv_obj_t* savingSub = nullptr;
  lv_obj_t* error = nullptr;
  lv_obj_t* errorSub = nullptr;
  lv_obj_t* whiteout = nullptr;
  lv_obj_t* toast = nullptr;
  lv_obj_t* toastL = nullptr;
  uint8_t effect = 0;
  // settings
  lv_obj_t* settings = nullptr;
  Tile tOrient{}, tDelay{}, tEffect{}, tUi{};
  int32_t shownDelay = -1;
  int lastOrient = -1, lastEffect = -1, lastUi = -1;
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

// ---------------------------------------------------------------- splash (not in Figma)
void buildSplash() {
  g.splash = screenBase(kBg);
  lv_obj_t* t = label(g.splash, fontDisplay(), kOnPhoto, "Four Eyes");
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -12);
  lv_obj_t* v = label(g.splash, fontFooter(), kFgMuted, "x4 " X4_FW_VERSION);
  lv_obj_align(v, LV_ALIGN_CENTER, 0, 34);
  reveal(t, 0);
  reveal(v, 4);
}

// ---------------------------------------------------------------- main (viewfinder)
void onBatteryLongPress(lv_event_t*);  // defined with the confirm overlay below

void hideMenu() { fadeTo(g.menu, false, kMsFast); }

void onViewfinderTap(lv_event_t*) {
  bool open = !lv_obj_has_flag(g.menu, LV_OBJ_FLAG_HIDDEN);
  if (open) {
    hideMenu();
  } else {
    fadeTo(g.menu, true, kMsFast);
    g.menuUntil = millis() + 4000;
  }
}

void onMenu(lv_event_t* e) {
  intptr_t which = (intptr_t)lv_event_get_user_data(e);
  hideMenu();
  if (which == 1) show(Screen::Settings);
}

void buildMain() {
  g.main = screenBase(kBg);
  lv_obj_add_flag(g.main, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g.main, onViewfinderTap, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(g.main, [](lv_event_t*) { show(Screen::Diagnostics); }, LV_EVENT_LONG_PRESSED, nullptr);

  // Photo, full-bleed (Figma "Photo Example" 320×240). RGB565 big-endian straight from TJpgDec.
  g.preview = lv_image_create(g.main);
  lv_obj_set_size(g.preview, kScreenW, kScreenH);
  lv_obj_set_pos(g.preview, 0, 0);
  lv_obj_remove_flag(g.preview, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(g.preview, LV_OBJ_FLAG_HIDDEN);

  // No photo yet: quiet serif line in the design's voice (state not drawn in Figma).
  g.placeholder = label(g.main, fontSub(), kOnPhoto, "");
  lv_obj_set_style_text_opa(g.placeholder, LV_OPA_60, 0);
  lv_obj_align(g.placeholder, LV_ALIGN_CENTER, 0, 0);

  // Portrait: the centre 180 px crop stays bright, the sides drop to 30 % (Figma "preview: portrait").
  int32_t side = (kScreenW - kPortraitW) / 2;
  g.dimL = plain(g.main);
  g.dimR = plain(g.main);
  for (lv_obj_t* d : {g.dimL, g.dimR}) {
    lv_obj_set_size(d, side, kScreenH);
    lv_obj_set_style_bg_color(d, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_70, 0);
    lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_pos(g.dimL, 0, 0);
  lv_obj_set_pos(g.dimR, kScreenW - side, 0);

  // Footer (Figma "Footer", 320×47): corner glows + Space Mono Bold 13 values.
  g.footer = plain(g.main);
  lv_obj_set_size(g.footer, kScreenW, kFooterH);
  lv_obj_align(g.footer, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t* gl = image(g.footer, &x4_img_glow_corner_left);
  lv_obj_align(gl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t* gr = image(g.footer, &x4_img_glow_corner_right);
  lv_obj_align(gr, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  g.footL = label(g.footer, fontFooter(), kOnPhoto, "");
  lv_obj_align(g.footL, LV_ALIGN_BOTTOM_LEFT, kFooterPadX, -kFooterPadB);
  // Long-press the battery estimate to mark the bank as charged (not in Figma; no other place to
  // do it without a phone).
  lv_obj_t* batHit = plain(g.footer);
  lv_obj_set_size(batHit, 80, kFooterH);
  lv_obj_align(batHit, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_flag(batHit, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(batHit, onBatteryLongPress, LV_EVENT_LONG_PRESSED, nullptr);
  g.footR = label(g.footer, fontFooter(), kOnPhoto, "");
  lv_obj_align(g.footR, LV_ALIGN_BOTTOM_RIGHT, -kFooterPadX, -kFooterPadB);

  // Flash recharge indicator (Figma component 81:184, 82×32, bottom centre).
  g.flashInd = image(g.main, &x4_img_flash_recharge);
  lv_obj_align(g.flashInd, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(g.flashInd, LV_OBJ_FLAG_HIDDEN);

  // Tap menu (Figma "on tap: buttons"): 80 px black 80 %→0 gradient, three serif-32 labels.
  g.menu = plain(g.main);
  lv_obj_set_size(g.menu, kScreenW, kMenuH);
  lv_obj_set_pos(g.menu, 0, 0);
  lv_obj_set_style_bg_color(g.menu, lv_color_black(), 0);
  lv_obj_set_style_bg_grad_color(g.menu, lv_color_black(), 0);
  lv_obj_set_style_bg_grad_dir(g.menu, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(g.menu, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_main_opa(g.menu, LV_OPA_80, 0);
  lv_obj_set_style_bg_grad_opa(g.menu, LV_OPA_TRANSP, 0);
  // video and media are hidden in v1 (owner, 2026-11-01); their cells keep the Figma positions.
  static const char* kItems[] = {"video", "settings", "media"};
  for (int i = 0; i < 3; i++) {
    if (i != 1) continue;
    lv_obj_t* cell = plain(g.menu);
    lv_obj_set_size(cell, kScreenW / 3 + (i == 1 ? 1 : 0), kMenuH);
    lv_obj_set_pos(cell, i * kScreenW / 3, 0);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cell, onMenu, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* l = label(cell, fontMenu(), kOnPhoto, kItems[i]);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, kMenuPadTop);
    lv_obj_set_style_text_opa(l, LV_OPA_60, LV_STATE_PRESSED);
    lv_obj_add_flag(l, LV_OBJ_FLAG_EVENT_BUBBLE);
  }
  lv_obj_add_flag(g.menu, LV_OBJ_FLAG_HIDDEN);

  // Saving overlay (Figma "state: saving (n of 4)"): holographic image (alpha baked), serif 48 + 24.
  g.saving = plain(g.main);
  lv_obj_set_size(g.saving, kScreenW, kScreenH);
  image(g.saving, &x4_img_overlay_saving);
  lv_obj_t* st = label(g.saving, fontState(), kOnPhoto, "Saving...");
  lv_obj_align(st, LV_ALIGN_TOP_MID, 0, 89);
  g.savingSub = label(g.saving, fontSub(), kOnPhoto, "");
  lv_obj_align(g.savingSub, LV_ALIGN_TOP_MID, 0, kScreenH - kStateSubBottom);
  lv_obj_add_flag(g.saving, LV_OBJ_FLAG_HIDDEN);

  // Error overlay (Figma "state: error"): red image at 90 %, serif 48 "Error" + our reason line.
  g.error = plain(g.main);
  lv_obj_set_size(g.error, kScreenW, kScreenH);
  image(g.error, &x4_img_overlay_error);
  lv_obj_t* et = label(g.error, fontState(), kOnPhoto, "Error");
  lv_obj_align(et, LV_ALIGN_TOP_MID, 0, 89);
  g.errorSub = label(g.error, fontSub(), kOnPhoto, "");
  lv_obj_align(g.errorSub, LV_ALIGN_TOP_MID, 0, kScreenH - kStateSubBottom);
  lv_obj_add_flag(g.error, LV_OBJ_FLAG_HIDDEN);

  // Shutter white-out on the top layer (not in Figma; tactile feedback replacing haptics).
  g.whiteout = plain(lv_layer_top());
  lv_obj_set_size(g.whiteout, kScreenW, kScreenH);
  lv_obj_set_style_bg_color(g.whiteout, c(kFlash), 0);
  lv_obj_set_style_bg_opa(g.whiteout, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(g.whiteout, LV_OBJ_FLAG_CLICKABLE);

  // Toast (not in Figma): dark pill, footer type.
  g.toast = plain(lv_layer_top());
  lv_obj_set_style_radius(g.toast, kRadiusPill, 0);
  lv_obj_set_style_bg_color(g.toast, c(kBg), 0);
  lv_obj_set_style_bg_opa(g.toast, LV_OPA_80, 0);
  lv_obj_set_style_pad_hor(g.toast, kSpaceM, 0);
  lv_obj_set_style_pad_ver(g.toast, kSpaceXS + 2, 0);
  lv_obj_set_size(g.toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  g.toastL = label(g.toast, fontFooter(), kOnPhoto, "");
  lv_obj_align(g.toast, LV_ALIGN_BOTTOM_MID, 0, -kFooterH);
  lv_obj_add_flag(g.toast, LV_OBJ_FLAG_HIDDEN);
}

const char* errorReason(const Status& s) {
  if (s.faults & (uint8_t)x4::shoot::Fault::SdMissing) return "no sd card";
  if (s.faults & (uint8_t)x4::shoot::Fault::SdFull) return "sd card full";
  if (s.faults & (uint8_t)x4::shoot::Fault::Overheat) return "too hot, cooling";
  return "";
}

void updateMain(const Status& s, uint32_t now) {
  using x4::shoot::State;
  char buf[48];
  int expected = 0, stored = 0;
  for (const auto& cv : s.cam) {
    expected += cv.expected;
    stored += cv.stored;
  }

  // Overlays by state.
  bool saving = s.state == State::Shooting || s.state == State::Transferring;
  bool err = s.state == State::Error;
  fadeTo(g.saving, saving, kMsBase);
  fadeTo(g.error, err, kMsBase);
  if (saving) {
    int k = stored + 1 > expected ? expected : stored + 1;
    snprintf(buf, sizeof(buf), "%d of %d", expected ? k : 0, expected);
    setText(g.savingSub, buf);
  }
  if (err) setText(g.errorSub, errorReason(s));

  // Placeholder when no photo is on screen.
  bool live = g.hasFrame && s.previewOn;
  setHidden(g.preview, !live);
  const char* ph = "";
  if (!live && !saving && !err) {
    if (s.state == State::Booting) ph = "starting";
    else if (s.state == State::Idle) {
      snprintf(buf, sizeof(buf), "waiting for eyes · %d of 4", s.camsOnline);
      ph = buf;
    } else {
      snprintf(buf, sizeof(buf), "%d of 4 eyes ready", s.camsOnline);
      ph = buf;
    }
  }
  setText(g.placeholder, ph);

  // Settings reflected live.
  setHidden(g.dimL, s.uiOrient != 1);
  setHidden(g.dimR, s.uiOrient != 1);
  g.effect = s.uiEffect;
  setHidden(g.footer, s.uiMode != 1 || saving || err);
  if (s.uiMode == 1) {
    // Left slot: battery % ESTIMATED by energy counting (x4/energy.h); "~" marks it as an estimate.
    snprintf(buf, sizeof(buf), "~%d%%", s.batteryPct);
    setText(g.footL, buf);
    if (!s.sdMounted) snprintf(buf, sizeof(buf), "NO SD");
    else if (s.sdFreeMb >= 10240) snprintf(buf, sizeof(buf), "%.1f GB", s.sdFreeMb / 1024.0f);
    else snprintf(buf, sizeof(buf), "%lu MB", (unsigned long)s.sdFreeMb);
    setText(g.footR, buf);
  }
  setHidden(g.flashInd, !(s.flashArmed && !s.flashReady) || saving || err);

  if (!lv_obj_has_flag(g.menu, LV_OBJ_FLAG_HIDDEN) && (int32_t)(now - g.menuUntil) >= 0) hideMenu();
}

// ---------------------------------------------------------------- settings (Figma "settings")
void tilePressed(lv_event_t* e) {
  Tile* t = (Tile*)lv_event_get_user_data(e);
  // Press feedback: tile flashes ink for a beat (the design is pure ink on grey).
  lv_obj_set_style_bg_opa(t->box, LV_OPA_20, 0);
}
void tileReleased(lv_event_t* e) {
  Tile* t = (Tile*)lv_event_get_user_data(e);
  lv_obj_set_style_bg_opa(t->box, LV_OPA_TRANSP, 0);
}

Tile makeTile(lv_obj_t* parent, int32_t x, int32_t y) {
  Tile t{};
  t.box = plain(parent);
  lv_obj_set_size(t.box, kTileW, kTileH);
  lv_obj_set_pos(t.box, x, y);
  lv_obj_set_style_border_width(t.box, kBorderStrong, 0);
  lv_obj_set_style_border_color(t.box, c(kInk), 0);
  lv_obj_set_style_bg_color(t.box, c(kInk), 0);
  lv_obj_set_style_bg_opa(t.box, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(t.box, LV_OBJ_FLAG_CLICKABLE);
  return t;
}

lv_obj_t* bar(lv_obj_t* parent) {
  lv_obj_t* b = plain(parent);
  lv_obj_set_style_bg_color(b, c(kInk), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_IGNORE_LAYOUT);
  return b;
}

// cam delay steps: 0 (all at once), 0.2 … 1.0 s
constexpr int32_t kDelaySteps[] = {0, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
constexpr int kDelayCount = sizeof(kDelaySteps) / sizeof(kDelaySteps[0]);

int delayIndex(int32_t ms) {
  int best = 0;
  for (int i = 0; i < kDelayCount; i++)
    if (abs(kDelaySteps[i] - ms) < abs(kDelaySteps[best] - ms)) best = i;
  return best;
}

int32_t thumbX(int idx) {
  int32_t travel = kTileW - 2 * kBorderStrong - kSliderW;
  return -kBorderStrong + (travel + kBorderStrong) * idx / (kDelayCount - 1);
}

void onOrient(lv_event_t*) { app::setOption("ui_orient", x4core::status::get().uiOrient ? 0 : 1); }
void onEffect(lv_event_t*) { app::setOption("ui_effect", x4core::status::get().uiEffect ? 0 : 1); }
void onUi(lv_event_t*) { app::setOption("ui_mode", x4core::status::get().uiMode ? 0 : 1); }
void onDelayDrag(lv_event_t* e) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  lv_area_t a;
  lv_obj_get_coords(g.tDelay.box, &a);
  int32_t w = lv_area_get_width(&a);
  int32_t rel = p.x - a.x1;
  int idx = (int)((rel * (kDelayCount - 1) + w / 2) / (w > 0 ? w : 1));
  if (idx < 0) idx = 0;
  if (idx >= kDelayCount) idx = kDelayCount - 1;
  if (kDelaySteps[idx] != x4core::status::get().camDelayMs) app::setOption("cam_delay_ms", kDelaySteps[idx]);
  (void)e;
}

void buildSettings() {
  g.settings = screenBase(kSettingsBg);
  lv_obj_add_event_cb(g.settings, [](lv_event_t*) { show(Screen::Main); }, LV_EVENT_GESTURE, nullptr);
  const int32_t x0 = kSettingsPad, x1 = kSettingsPad + kTileW + kSettingsGap;
  const int32_t y0 = kSettingsPad, y1 = kSettingsPad + kTileH + kSettingsGap;

  // orientation: horzntl = bars top + bottom; portrt = bars left + right
  g.tOrient = makeTile(g.settings, x0, y0);
  g.tOrient.a = bar(g.tOrient.box);
  g.tOrient.b = bar(g.tOrient.box);
  g.tOrient.label = label(g.tOrient.box, fontTile(), kInk, "horzntl");
  lv_obj_align(g.tOrient.label, LV_ALIGN_CENTER, 0, 0);

  // cam delay: vertical thumb that slides left→right, value bottom-right
  g.tDelay = makeTile(g.settings, x1, y0);
  g.tDelay.a = bar(g.tDelay.box);
  lv_obj_set_size(g.tDelay.a, kSliderW, kTileH);
  lv_obj_set_y(g.tDelay.a, -kBorderStrong);
  g.tDelay.label = label(g.tDelay.box, fontTile(), kInk, "0 s");
  lv_obj_align(g.tDelay.label, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
  lv_obj_add_event_cb(g.tDelay.box, onDelayDrag, LV_EVENT_PRESSING, nullptr);

  // effect: label bottom-left, dither strip on the right edge when dithered
  g.tEffect = makeTile(g.settings, x0, y1);
  lv_obj_set_style_clip_corner(g.tEffect.box, true, 0);
  g.tEffect.a = image(g.tEffect.box, &x4_img_dither_strip);
  lv_obj_set_pos(g.tEffect.a, kTileW - 2 * kBorderStrong - 40 + kBorderStrong, -kBorderStrong);
  g.tEffect.label = label(g.tEffect.box, fontTileSmall(), kInk, "normal");
  lv_obj_align(g.tEffect.label, LV_ALIGN_BOTTOM_LEFT, 4, -2);

  // ui: minml / full
  g.tUi = makeTile(g.settings, x1, y1);
  g.tUi.label = label(g.tUi.box, fontTileSmall(), kInk, "full");
  lv_obj_align(g.tUi.label, LV_ALIGN_CENTER, 0, 0);

  Tile* tiles[] = {&g.tOrient, &g.tDelay, &g.tEffect, &g.tUi};
  for (Tile* t : tiles) {
    lv_obj_add_event_cb(t->box, tilePressed, LV_EVENT_PRESSED, t);
    lv_obj_add_event_cb(t->box, tileReleased, LV_EVENT_RELEASED, t);
    lv_obj_add_event_cb(t->box, tileReleased, LV_EVENT_PRESS_LOST, t);
  }
  lv_obj_add_event_cb(g.tOrient.box, onOrient, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(g.tEffect.box, onEffect, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(g.tUi.box, onUi, LV_EVENT_CLICKED, nullptr);
}

void updateSettings(const Status& s) {
  if (s.uiOrient != g.lastOrient) {
    g.lastOrient = s.uiOrient;
    const int32_t inner = kTileW - 2 * kBorderStrong;
    if (s.uiOrient == 0) {
      lv_obj_set_size(g.tOrient.a, kTileW, kTileBar);
      lv_obj_set_pos(g.tOrient.a, -kBorderStrong, -kBorderStrong);
      lv_obj_set_size(g.tOrient.b, kTileW, kTileBar);
      lv_obj_set_pos(g.tOrient.b, -kBorderStrong, kTileH - kTileBar - kBorderStrong);
      setText(g.tOrient.label, "horzntl");
    } else {
      lv_obj_set_size(g.tOrient.a, kTileBar, kTileH);
      lv_obj_set_pos(g.tOrient.a, -kBorderStrong, -kBorderStrong);
      lv_obj_set_size(g.tOrient.b, kTileBar, kTileH);
      lv_obj_set_pos(g.tOrient.b, inner - kTileBar + kBorderStrong, -kBorderStrong);
      setText(g.tOrient.label, "portrt");
    }
  }
  if ((int32_t)s.camDelayMs != g.shownDelay) {
    int idx = delayIndex(s.camDelayMs);
    int32_t to = thumbX(idx);
    if (g.shownDelay < 0) {
      lv_obj_set_x(g.tDelay.a, to);
    } else {
      lv_anim_delete(g.tDelay.a, animX);
      anim(g.tDelay.a, animX, lv_obj_get_x(g.tDelay.a), to, kMsSpring, pathSpring());
    }
    g.shownDelay = s.camDelayMs;
    char b[12];
    if (kDelaySteps[idx] == 0) snprintf(b, sizeof(b), "0 s");
    else if (kDelaySteps[idx] == 1000) snprintf(b, sizeof(b), "1 s");
    else snprintf(b, sizeof(b), "0.%d s", (int)(kDelaySteps[idx] / 100));
    setText(g.tDelay.label, b);
  }
  if (s.uiEffect != g.lastEffect) {
    g.lastEffect = s.uiEffect;
    setText(g.tEffect.label, s.uiEffect ? "dithered" : "normal");
    setHidden(g.tEffect.a, !s.uiEffect);
  }
  if (s.uiMode != g.lastUi) {
    g.lastUi = s.uiMode;
    setText(g.tUi.label, s.uiMode ? "full" : "minml");
  }
}

// ---------------------------------------------------------------- diagnostics / bring-up (not in Figma)
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
void onConfirmYes(lv_event_t* e) {
  auto r = (app::Request)(intptr_t)lv_event_get_user_data(e);
  closeConfirm();
  app::request(r);
}
void onConfirmNo(lv_event_t*) { closeConfirm(); }

// On-screen confirmation (flash test: requirement "on-screen confirm"; battery reset).
void showConfirm(const char* title, const char* detail, const char* yes, app::Request r) {
  closeConfirm();
  g.confirm = lv_obj_create(lv_layer_top());
  lv_obj_set_size(g.confirm, kScreenW, kScreenH);
  lv_obj_set_style_bg_color(g.confirm, c(kBg), 0);
  lv_obj_set_style_bg_opa(g.confirm, LV_OPA_80, 0);
  lv_obj_set_style_border_width(g.confirm, 0, 0);
  lv_obj_t* t = label(g.confirm, fontSub(), kOnPhoto, title);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -40);
  lv_obj_t* d = label(g.confirm, fontCaption(), kWarn, detail);
  lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(d, LV_ALIGN_CENTER, 0, -6);
  lv_obj_t* y = pillButton(g.confirm, yes, true, onConfirmYes, (void*)(intptr_t)r);
  lv_obj_align(y, LV_ALIGN_CENTER, 50, 44);
  lv_obj_t* n = pillButton(g.confirm, "Cancel", false, onConfirmNo, nullptr);
  lv_obj_align(n, LV_ALIGN_CENTER, -50, 44);
  reveal(t, 0);
}

void onFlashTest(lv_event_t*) {
  showConfirm("Fire one 50 ms flash?", "Flashlight Unit on G2 (Port A yellow).\nDon't look into it.", "Fire",
              app::Request::FlashTest);
}

void onBatteryLongPress(lv_event_t*) {
  showConfirm("Bank charged?", "Sets the battery estimate to 100 %.\nIt counts down from there.", "Yes",
              app::Request::BatteryFull);
}

void onTouchTest(lv_event_t*) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  if (!g.touchDot) return;
  lv_obj_remove_flag(g.touchDot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(g.touchDot, p.x - 6, p.y - 6);
  X4_LOGI("ui", "touch_test", "x=%d y=%d", (int)p.x, (int)p.y);
}

void buildDiag() {
  g.diag = screenBase(kBg);
  lv_obj_t* title = label(g.diag, fontSub(), kOnPhoto, g.hello ? "Bring-up" : "Diagnostics");
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
  lv_obj_set_style_bg_color(g.touchDot, c(kError), 0);
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
           "button %s · flash %s%s · pulses %lu\n"
           "battery ~%d%% · %.1f h left at %lu mW (estimate)",
           X4_FW_VERSION, X4_GIT_HASH, (unsigned long)(s.uptimeMs / 1000), s.fps, s.frameMaxUs / 1000.0f,
           s.busOutEn ? "ON!" : "off", s.usbOtgEn ? "ON!" : "off", (long)s.vbusMv, s.powerSafe ? "safe" : "UNSAFE",
           s.tempC, s.overheat ? " HOT" : s.hot ? " warm" : "", (unsigned long)(s.heap / 1024),
           (unsigned long)(s.heapMin / 1024), (unsigned long)(s.psram / 1024), s.resetReason ? s.resetReason : "?",
           (unsigned long)s.bootCount, (unsigned long)s.brownouts, s.sdMounted ? "ok" : "MISSING",
           (unsigned long)s.sdFreeMb, (unsigned long)s.sdTotalMb, s.sdWriteKBps, s.sdReadKBps,
           s.buttonDown ? "DOWN" : "up", s.flashEnabled ? "armed" : "disarmed",
           s.flashEnabled ? (s.flashReady ? " ready" : " cooling") : "", (unsigned long)s.flashPulses, s.batteryPct,
           s.batteryHours, (unsigned long)s.powerMw);
  setText(g.diagSys, b);
  lv_obj_set_style_text_color(g.diagSys, c(s.powerSafe ? kFg : kError), 0);
  size_t w = 0;
  for (int i = 0; i < 4 && w < sizeof(b); i++) {
    const auto& cv = s.cam[i];
    w += snprintf(b + w, sizeof(b) - w, "cam%d %s %s %d° rssi %d hb %lums last %s %+.1fms\n", i + 1,
                  cv.online ? "on " : "off", cv.ip, cv.tempC, cv.rssi, (unsigned long)cv.lastHbAgeMs,
                  cv.stored ? "stored" : cv.failed ? "failed" : cv.ok ? "captured" : "-", cv.capOffsetUs / 1000.0f);
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
  buildSettings();
  buildDiag();
  g.splashUntil = millis() + 1400;
  g.cur = Screen::Splash;
  X4_LOGI("ui", "begin", "mode=%s design=figma:73:368", helloMode ? "hello" : "full");
}

Screen current() { return g.cur; }

void show(Screen s) {
  if (s == g.cur) return;
  lv_obj_t* scr = s == Screen::Main         ? g.main
                  : s == Screen::Settings   ? g.settings
                  : s == Screen::Diagnostics ? g.diag
                                             : g.splash;
  lv_screen_load_anim_t a = s == Screen::Diagnostics ? LV_SCREEN_LOAD_ANIM_MOVE_TOP
                            : s == Screen::Settings  ? LV_SCREEN_LOAD_ANIM_OVER_BOTTOM
                                                     : LV_SCREEN_LOAD_ANIM_FADE_IN;
  lv_screen_load_anim(scr, a, kMsBase, 0, false);
  X4_LOGD("ui", "screen", "from=%d to=%d", (int)g.cur, (int)s);
  g.cur = s;
}

void update(const Status& s, uint32_t now) {
  if (g.cur == Screen::Splash && (int32_t)(now - g.splashUntil) >= 0)
    show(g.hello ? Screen::Diagnostics : Screen::Main);
  if (g.cur == Screen::Main || g.cur == Screen::Splash) updateMain(s, now);
  if (g.cur == Screen::Settings) updateSettings(s);
  if (g.cur == Screen::Diagnostics) updateDiag(s, now);
}

void onAction(const x4::input::Event& e) {
  using x4::input::Action;
  switch (e.action) {
    case Action::Diagnostics:
      show(g.cur == Screen::Diagnostics && !g.hello ? Screen::Main : Screen::Diagnostics);
      break;
    case Action::Menu: show(g.cur == Screen::Main ? Screen::Settings : Screen::Diagnostics); break;
    case Action::Back:
      closeConfirm();
      if (!g.hello) show(Screen::Main);
      break;
    default: break;
  }
}

void shutterMoment(bool flash) {
  lv_anim_delete(g.whiteout, animBgOpa);
  anim(g.whiteout, animBgOpa, flash ? LV_OPA_COVER : LV_OPA_70, LV_OPA_TRANSP, kMsSlow, pathBase());
  beep(2600, 18, 1300, 30);
  if (!lv_obj_has_flag(g.menu, LV_OBJ_FLAG_HIDDEN)) hideMenu();
  if (g.cur != Screen::Main && !g.hello) show(Screen::Main);  // shutter always returns to the viewfinder
}

void shotCaptured() { beep(1800, 12); }

void shotStored(bool partial, uint32_t totalMs, bool overBudget) {
  char b[48];
  if (partial) snprintf(b, sizeof(b), "saved, some eyes missing · %.1fs", totalMs / 1000.0f);
  else snprintf(b, sizeof(b), "saved · %.1fs", totalMs / 1000.0f);
  toast(b, partial || overBudget ? kWarn : kOnPhoto);
  if (!partial) beep(2200, 20, 3300, 40);
}

void shutterRefused(const char* why) {
  char b[48];
  snprintf(b, sizeof(b), "not ready · %s", why);
  toast(b, kWarn, 1500);
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

void setPreviewJpeg(const uint8_t* jpg, uint32_t len, float scale) {
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
  g.canvas->drawJpg(jpg, len, 0, 0, kScreenW, kScreenH, 0, 0, scale, scale);
  if (g.effect == 1) ditherBE565((uint16_t*)g.canvas->getBuffer(), kScreenW, kScreenH);
  uint32_t us = micros() - t0;
  lv_image_set_src(g.preview, &g.previewDsc);
  lv_obj_invalidate(g.preview);
  g.hasFrame = true;
  X4_LOG_EVERY(5000, x4::log::Level::Debug, "ui", "preview_decode", "us=%lu bytes=%lu dither=%u", (unsigned long)us,
               (unsigned long)len, g.effect);
}

void clearPreview() { g.hasFrame = false; }

}  // namespace x4ui
