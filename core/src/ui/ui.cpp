// x4 UI v3 — implemented from the Figma file fgEvP6Ms0bOAbWxfkFki4R (node JSON in ui/figma/nodes/).
// Frame / component → LVGL mapping (details and compromises: docs/UI.md):
//   preview: photo / dithered / portrait  73:368     → Main screen (live photo full-bleed)
//   Footer States 83:1905                            → footer (record · flash charge · SD free)
//   on tap: buttons                                  → tap menu (video · settings · media)
//   settings 73:254 + tile components 83:2147        → Settings screen
//   saving overlay 83:2228 (progress k of 4)         → saving overlay (image opacity k/n)
//   error overlay 83:2286 (unknown true/false)       → error overlay ("Error" or the known reason)
// Positions are Figma's; text labels are placed by baseline (see the layout tokens in ui/tokens.json).
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
void animImgOpa(void* o, int32_t v) { lv_obj_set_style_image_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }
void animW(void* o, int32_t v) { lv_obj_set_width((lv_obj_t*)o, v < 1 ? 1 : v); }  // spring may undershoot

// Row that hugs its content (Figma auto layout, horizontal, items centred).
lv_obj_t* hugRow(lv_obj_t* parent, int32_t gap) {
  lv_obj_t* r = plain(parent);
  lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, gap, 0);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
  return r;
}

// Label in a fixed-width box, so Figma's CENTER / RIGHT text alignment maps 1:1.
lv_obj_t* boxLabel(lv_obj_t* parent, const lv_font_t* f, uint32_t color, const char* txt, int32_t w,
                   lv_text_align_t align) {
  lv_obj_t* l = label(parent, f, color, txt);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, align, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  return l;
}

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
  lv_obj_t* a;   // orientation: bar 1 · delay: fill · style: gradient
  lv_obj_t* b;   // orientation: bar 2 · delay: value label inside the fill · style: dither strip
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
  lv_obj_t* recGlow = nullptr;  // footer left: red glow while recording
  lv_obj_t* recDot = nullptr;   // white dot idle, red while recording
  lv_obj_t* recTime = nullptr;  // "-:--" idle, "m:ss" while recording
  lv_obj_t* glowMid = nullptr;  // footer centre glow, with the flash charge
  lv_obj_t* flashInd = nullptr; // footer centre: bolt + charge %, while the flash is armed and charging
  lv_obj_t* flashBolt = nullptr;
  lv_obj_t* flashPct = nullptr;
  lv_obj_t* sdNum = nullptr;    // footer right: SD free, number + unit
  lv_obj_t* sdUnit = nullptr;
  int lastRec = -1;
  lv_obj_t* menu = nullptr;
  uint32_t menuUntil = 0;
  lv_obj_t* saving = nullptr;
  lv_obj_t* savingImg = nullptr;
  lv_obj_t* savingK = nullptr;
  lv_obj_t* savingOf = nullptr;
  int savingOpa = -1;
  lv_obj_t* error = nullptr;
  lv_obj_t* errorTitle = nullptr;
  lv_obj_t* whiteout = nullptr;
  lv_obj_t* toast = nullptr;
  lv_obj_t* toastL = nullptr;
  uint8_t effect = 0;
  // settings
  lv_obj_t* settings = nullptr;
  Tile tOrient{}, tDelay{}, tEffect{}, tFlash{};
  int32_t shownDelay = -1;
  int lastOrient = -1, lastEffect = -1, lastFlash = -1;
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
  lv_obj_t* t = label(g.splash, fontDisplay(), kOnPhoto, X4_PROJECT_NAME);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -12);
  lv_obj_t* v = label(g.splash, fontFooter(), kFgMuted, "x4 " X4_FW_VERSION);
  lv_obj_align(v, LV_ALIGN_CENTER, 0, 34);
  reveal(t, 0);
  reveal(v, 4);
}

// ---------------------------------------------------------------- main (viewfinder)
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

  // Footer (Footer States 83:1905, 320×47). Footer coordinates below are Figma's.
  //   left: record state (Rec Timer 83:866) · centre: flash charge (83:1188) · right: SD free (18:782)
  // Glows: Overlay Element 18:587, 80 px ellipses centred on the footer's bottom edge, blurred;
  // they reach 25 px above the footer, so the footer does not clip its children.
  g.footer = plain(g.main);
  lv_obj_set_size(g.footer, kScreenW, kFooterH);
  lv_obj_align(g.footer, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_flag(g.footer, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_pos(image(g.footer, &x4_img_glow_corner_left), 0, kGlowY);
  lv_obj_set_pos(image(g.footer, &x4_img_glow_corner_right), kScreenW - x4_img_glow_corner_right.header.w, kGlowY);
  g.glowMid = image(g.footer, &x4_img_glow_mid);
  lv_obj_set_pos(g.glowMid, (kScreenW - x4_img_glow_mid.header.w) / 2, kGlowY);
  lv_obj_add_flag(g.glowMid, LV_OBJ_FLAG_HIDDEN);

  // Left. The record glow (60×23 ellipse, centred on the timer's bottom-left corner) shows only
  // while recording. Figma shows this block only in the Video footer state; it is always shown
  // here (owner, 2026-11-01): "-:--" until video exists.
  g.recGlow = image(g.footer, &x4_img_rec_glow);
  lv_obj_set_pos(g.recGlow, 0, kRecGlowY);
  lv_obj_add_flag(g.recGlow, LV_OBJ_FLAG_HIDDEN);
  g.recDot = plain(g.footer);
  lv_obj_set_size(g.recDot, kRecDot, kRecDot);
  lv_obj_set_style_radius(g.recDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g.recDot, c(kOnPhoto), 0);
  lv_obj_set_style_bg_opa(g.recDot, LV_OPA_COVER, 0);
  lv_obj_set_pos(g.recDot, kFooterPadX, kRecDotY);
  g.recTime = label(g.footer, fontFooter(), kOnPhoto, "-:--");
  lv_obj_set_pos(g.recTime, kRecTextX, kFooterTextY);

  // Centre: bolt (6.6×9.7) + gap 4 + "80%", the group centred on the screen. The bolt fades in with
  // the charge so it reads as filling up; the number moves in 1 % steps.
  g.flashInd = hugRow(g.footer, kFlashGap);
  g.flashBolt = image(g.flashInd, &x4_img_flash_bolt);
  g.flashPct = label(g.flashInd, fontFooter(), kOnPhoto, "");
  lv_obj_align(g.flashInd, LV_ALIGN_TOP_MID, 0, kFooterTextY);
  lv_obj_add_flag(g.flashInd, LV_OBJ_FLAG_HIDDEN);

  // Right: "820 MB", right edge 8 px from the screen edge. Figma separates number and unit with a
  // thin space, which Space Mono lacks: two labels, 3 px apart.
  lv_obj_t* sd = hugRow(g.footer, kSdUnitGap);
  g.sdNum = label(sd, fontFooter(), kOnPhoto, "");
  g.sdUnit = label(sd, fontFooter(), kOnPhoto, "");
  lv_obj_align(sd, LV_ALIGN_TOP_RIGHT, -kFooterPadX, kFooterTextY);

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

  // Saving overlay (83:2228): holographic image whose opacity follows progress (k of 4 = k/4),
  // "Saving..." serif 48 at x 105 (the word centred, the dots trail), "k of n" serif 24 with k
  // right-aligned against a fixed " of n".
  g.saving = plain(g.main);
  lv_obj_set_size(g.saving, kScreenW, kScreenH);
  g.savingImg = image(g.saving, &x4_img_overlay_saving);
  lv_obj_t* st = label(g.saving, fontState(), kOnPhoto, "Saving...");
  lv_obj_set_pos(st, kSavingTitleX, kStateTitleY);
  g.savingK = boxLabel(g.saving, fontSub(), kOnPhoto, "", 40, LV_TEXT_ALIGN_RIGHT);
  lv_obj_set_pos(g.savingK, kSavingOfX - 40, kStateSubY);
  g.savingOf = label(g.saving, fontSub(), kOnPhoto, "");
  lv_obj_set_pos(g.savingOf, kSavingOfX, kStateSubY);
  lv_obj_add_flag(g.saving, LV_OBJ_FLAG_HIDDEN);

  // Error overlay (83:2286): red image at 90 % (baked), one centred serif 48 line: the reason when
  // it is known (unknown=false), "Error" otherwise (unknown=true).
  g.error = plain(g.main);
  lv_obj_set_size(g.error, kScreenW, kScreenH);
  image(g.error, &x4_img_overlay_error);
  g.errorTitle = boxLabel(g.error, fontState(), kOnPhoto, "Error", kScreenW, LV_TEXT_ALIGN_CENTER);
  lv_obj_set_pos(g.errorTitle, 0, kStateTitleY);
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

// Known reasons replace "Error" (Figma 83:2282 'unknown=false' shows "SD is full"). Sentence case
// like the design; the longest, "Too hot, cooling", is 261 px wide in serif 48.
const char* errorReason(const Status& s) {
  if (s.faults & (uint8_t)x4::shoot::Fault::SdMissing) return "No SD card";
  if (s.faults & (uint8_t)x4::shoot::Fault::SdFull) return "SD is full";
  if (s.faults & (uint8_t)x4::shoot::Fault::Overheat) return "Too hot, cooling";
  return "Error";
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
  bool savingAppears = saving && lv_obj_has_flag(g.saving, LV_OBJ_FLAG_HIDDEN);
  fadeTo(g.saving, saving, kMsBase);
  fadeTo(g.error, err, kMsBase);
  if (saving) {
    int k = stored + 1 > expected ? expected : stored + 1;
    snprintf(buf, sizeof(buf), "%d", k);
    setText(g.savingK, expected ? buf : "");
    snprintf(buf, sizeof(buf), " of %d", expected);
    setText(g.savingOf, expected ? buf : "");
    // Image opacity = progress (Figma: 25 / 50 / 75 / 100 % for 1..4 of 4); eases between steps.
    int opa = expected ? LV_OPA_COVER * k / expected : LV_OPA_COVER / 4;
    if (opa != g.savingOpa) {
      lv_anim_delete(g.savingImg, animImgOpa);
      if (savingAppears || g.savingOpa < 0) animImgOpa(g.savingImg, opa);
      else anim(g.savingImg, animImgOpa, g.savingOpa, opa, kMsBase, pathBase());
      g.savingOpa = opa;
    }
  }
  if (err) setText(g.errorTitle, errorReason(s));

  // Placeholder when no photo is on screen.
  bool live = g.hasFrame && s.previewOn;
  setHidden(g.preview, !live);
  const char* ph = "";
  if (!live && !saving && !err) {
    if (s.state == State::Booting) ph = "starting";
    else if (s.state == State::Idle) {
      snprintf(buf, sizeof(buf), "waiting for cameras · %d of 4", s.camsOnline);
      ph = buf;
    } else {
      snprintf(buf, sizeof(buf), "%d of 4 cameras ready", s.camsOnline);
      ph = buf;
    }
  }
  setText(g.placeholder, ph);

  // Settings reflected live.
  setHidden(g.dimL, s.uiOrient != 1);
  setHidden(g.dimR, s.uiOrient != 1);
  g.effect = s.uiEffect;
  setHidden(g.footer, saving || err);

  // Left: record state. Idle = white dot + "-:--"; recording = red dot with the red glow + timer.
  int rec = s.recording ? 1 : 0;
  if (rec != g.lastRec) {
    g.lastRec = rec;
    lv_obj_set_style_bg_color(g.recDot, c(rec ? kRec : kOnPhoto), 0);
    setHidden(g.recGlow, !rec);
  }
  if (rec) {
    uint32_t sec = s.recordMs / 1000;
    snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
    setText(g.recTime, buf);
  } else {
    setText(g.recTime, "-:--");
  }

  // Centre: flash charge, only while armed and not yet ready.
  bool charging = s.flashArmed && !s.flashReady;
  setHidden(g.flashInd, !charging);
  setHidden(g.glowMid, !charging);
  if (charging) {
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)s.flashChargePct);
    setText(g.flashPct, buf);
    lv_obj_set_style_image_opa(g.flashBolt, (lv_opa_t)(LV_OPA_30 + (LV_OPA_COVER - LV_OPA_30) * s.flashChargePct / 100), 0);
  }

  // Right: SD free.
  const char* unit = "";
  if (!s.sdMounted) {
    snprintf(buf, sizeof(buf), "NO SD");
  } else if (s.sdFreeMb >= 10240) {
    snprintf(buf, sizeof(buf), "%.1f", s.sdFreeMb / 1024.0f);
    unit = "GB";
  } else {
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.sdFreeMb);
    unit = "MB";
  }
  setText(g.sdNum, buf);
  setText(g.sdUnit, unit);
  setHidden(g.sdUnit, !*unit);

  if (!lv_obj_has_flag(g.menu, LV_OBJ_FLAG_HIDDEN) && (int32_t)(now - g.menuUntil) >= 0) hideMenu();
}

// ---------------------------------------------------------------- settings (Figma "settings" 73:254)
// Tiles are the components in section 83:2147: 140×100, 2 px black frame (inside), black ink on
// #D1D1D1. LVGL places children inside the border, so tile-local Figma coordinates go through at().
void at(lv_obj_t* o, int32_t x, int32_t y) { lv_obj_set_pos(o, x - kBorderStrong, y - kBorderStrong); }

Tile makeTile(lv_obj_t* parent, int32_t x, int32_t y) {
  Tile t{};
  t.box = plain(parent);
  lv_obj_set_size(t.box, kTileW, kTileH);
  lv_obj_set_pos(t.box, x, y);
  lv_obj_set_style_border_width(t.box, kBorderStrong, 0);
  lv_obj_set_style_border_color(t.box, c(kInk), 0);
  lv_obj_set_style_bg_color(t.box, c(kInk), 0);
  lv_obj_set_style_bg_opa(t.box, LV_OPA_TRANSP, 0);
  // Press feedback (not in Figma): the tile takes a 20 % ink tint while pressed.
  lv_obj_set_style_bg_opa(t.box, LV_OPA_20, LV_STATE_PRESSED);
  lv_obj_add_flag(t.box, LV_OBJ_FLAG_CLICKABLE);
  return t;
}

lv_obj_t* inkRect(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
  lv_obj_t* b = plain(parent);
  lv_obj_set_style_bg_color(b, c(kInk), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(b, w, h);
  at(b, x, y);
  return b;
}

// cam delay steps: 0 (all at once), 0.2 … 1.0 s
constexpr int32_t kDelaySteps[] = {0, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
constexpr int kDelayCount = sizeof(kDelaySteps) / sizeof(kDelaySteps[0]);
constexpr int32_t kDelayMax = 1000;

int delayIndex(int32_t ms) {
  int best = 0;
  for (int i = 0; i < kDelayCount; i++)
    if (abs(kDelaySteps[i] - ms) < abs(kDelaySteps[best] - ms)) best = i;
  return best;
}

// Fill width: delay / 1 s of the tile (Figma slider=0% / 20% / 80% / 1: 1, 28, 112, 140 px).
int32_t fillW(int32_t ms) {
  int32_t w = (kTileW * ms + kDelayMax / 2) / kDelayMax;
  return w < 1 ? 1 : w;
}

void onOrient(lv_event_t*) { app::setOption("ui_orient", x4core::status::get().uiOrient ? 0 : 1); }
void onEffect(lv_event_t*) { app::setOption("ui_effect", x4core::status::get().uiEffect ? 0 : 1); }
void onFlash(lv_event_t*) {
  app::request(x4core::status::get().flashArmed ? app::Request::FlashDisarm : app::Request::FlashArm);
}
void onDelayDrag(lv_event_t*) {
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
}

void buildSettings() {
  g.settings = screenBase(kSettingsBg);
  lv_obj_add_event_cb(g.settings, [](lv_event_t*) { show(Screen::Main); }, LV_EVENT_GESTURE, nullptr);
  const int32_t x0 = kSettingsPad, x1 = kSettingsPad + kTileW + kSettingsGap;
  const int32_t y0 = kSettingsPad, y1 = kSettingsPad + kTileH + kSettingsGap;
  const int32_t labelW = kTileW;

  // orientation (83:2168): horzntl = 25 px bars top + bottom; vertcl = 30 px bars left + right.
  // Label serif 36, centred.
  g.tOrient = makeTile(g.settings, x0, y0);
  g.tOrient.a = inkRect(g.tOrient.box, 0, 0, kTileW, kTileBarH);
  g.tOrient.b = inkRect(g.tOrient.box, 0, kTileH - kTileBarH, kTileW, kTileBarH);
  g.tOrient.label = boxLabel(g.tOrient.box, fontTile(), kInk, "horzntl", labelW, LV_TEXT_ALIGN_CENTER);
  at(g.tOrient.label, 0, kTileLabelY);

  // cam delay (83:2173): black fill from the left, width = delay / 1 s. The value (serif 36, right
  // edge 8 px in) is white with EXCLUSION blend in Figma: dark grey over the grey, white over the
  // fill. Drawn as two labels: a dark one on the tile and a white one inside the fill, which clips it.
  g.tDelay = makeTile(g.settings, x1, y0);
  g.tDelay.label = boxLabel(g.tDelay.box, fontTile(), kSliderText, "0 s", kTileW - kDelayLabelR, LV_TEXT_ALIGN_RIGHT);
  at(g.tDelay.label, 0, kDelayLabelY);
  g.tDelay.a = inkRect(g.tDelay.box, 0, 0, 1, kTileH);
  g.tDelay.b = boxLabel(g.tDelay.a, fontTile(), kOnPhoto, "0 s", kTileW - kDelayLabelR, LV_TEXT_ALIGN_RIGHT);
  lv_obj_set_pos(g.tDelay.b, 0, kDelayLabelY);  // the fill sits at the tile's (0, 0)
  lv_obj_add_event_cb(g.tDelay.box, onDelayDrag, LV_EVENT_PRESSING, nullptr);
  lv_obj_remove_flag(g.tDelay.box, LV_OBJ_FLAG_GESTURE_BUBBLE);  // a fast drag must not leave the screen

  // style (83:2215): 40×100 strip on the right: gradient (transparent #D1D1D1 → #1E1E1E) when
  // normal, dither image when dithered. Label serif 34 bottom-left.
  g.tEffect = makeTile(g.settings, x0, y1);
  g.tEffect.a = plain(g.tEffect.box);
  lv_obj_set_size(g.tEffect.a, kTileW - kStyleStripX, kTileH);
  at(g.tEffect.a, kStyleStripX, 0);
  lv_obj_set_style_bg_color(g.tEffect.a, c(kSettingsBg), 0);
  lv_obj_set_style_bg_grad_color(g.tEffect.a, c(kBg), 0);
  lv_obj_set_style_bg_grad_dir(g.tEffect.a, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(g.tEffect.a, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_main_opa(g.tEffect.a, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_grad_opa(g.tEffect.a, LV_OPA_COVER, 0);
  lv_obj_remove_flag(g.tEffect.a, LV_OBJ_FLAG_CLICKABLE);
  g.tEffect.b = image(g.tEffect.box, &x4_img_dither_strip);
  at(g.tEffect.b, kStyleStripX, 0);
  g.tEffect.label = label(g.tEffect.box, fontTileSmall(), kInk, "normal");
  at(g.tEffect.label, kStyleLabelX, kStyleLabelY);

  // flashlight (83:2219): no flash = black tile, grey label; flash = empty tile, black label.
  // Serif 36, centred. Runtime arm, like the button gesture: the rig always boots with the flash off.
  g.tFlash = makeTile(g.settings, x1, y1);
  g.tFlash.label = boxLabel(g.tFlash.box, fontTile(), kSettingsBg, "no flash", labelW, LV_TEXT_ALIGN_CENTER);
  at(g.tFlash.label, 0, kFlashLabelY);

  lv_obj_add_event_cb(g.tOrient.box, onOrient, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(g.tEffect.box, onEffect, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(g.tFlash.box, onFlash, LV_EVENT_CLICKED, nullptr);
}

void updateSettings(const Status& s) {
  if (s.uiOrient != g.lastOrient) {
    g.lastOrient = s.uiOrient;
    if (s.uiOrient == 0) {
      lv_obj_set_size(g.tOrient.a, kTileW, kTileBarH);
      at(g.tOrient.a, 0, 0);
      lv_obj_set_size(g.tOrient.b, kTileW, kTileBarH);
      at(g.tOrient.b, 0, kTileH - kTileBarH);
      setText(g.tOrient.label, "horzntl");
    } else {
      lv_obj_set_size(g.tOrient.a, kTileBarV, kTileH);
      at(g.tOrient.a, 0, 0);
      lv_obj_set_size(g.tOrient.b, kTileBarV, kTileH);
      at(g.tOrient.b, kTileW - kTileBarV, 0);
      setText(g.tOrient.label, "vertcl");
    }
  }
  if ((int32_t)s.camDelayMs != g.shownDelay) {
    int idx = delayIndex(s.camDelayMs);
    int32_t to = fillW(kDelaySteps[idx]);
    lv_anim_delete(g.tDelay.a, animW);
    if (g.shownDelay < 0) lv_obj_set_width(g.tDelay.a, to);
    else anim(g.tDelay.a, animW, lv_obj_get_width(g.tDelay.a), to, kMsSpring, pathSpring());
    g.shownDelay = s.camDelayMs;
    char b[12];
    if (kDelaySteps[idx] == 0) snprintf(b, sizeof(b), "0 s");
    else if (kDelaySteps[idx] == 1000) snprintf(b, sizeof(b), "1 s");
    else snprintf(b, sizeof(b), "0.%d s", (int)(kDelaySteps[idx] / 100));
    setText(g.tDelay.label, b);
    setText(g.tDelay.b, b);
  }
  if (s.uiEffect != g.lastEffect) {
    g.lastEffect = s.uiEffect;
    setText(g.tEffect.label, s.uiEffect ? "dithered" : "normal");
    setHidden(g.tEffect.a, s.uiEffect);
    setHidden(g.tEffect.b, !s.uiEffect);
  }
  if ((int)s.flashArmed != g.lastFlash) {
    g.lastFlash = s.flashArmed;
    bool on = s.flashArmed;
    lv_obj_set_style_bg_opa(g.tFlash.box, on ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(g.tFlash.box, on ? LV_OPA_20 : LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(g.tFlash.label, c(on ? kInk : kSettingsBg), 0);
    setText(g.tFlash.label, on ? "flash" : "no flash");
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

// On-screen confirmation (flash test: requirement "on-screen confirm").
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
  if (partial) snprintf(b, sizeof(b), "saved, some cameras missing · %.1fs", totalMs / 1000.0f);
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
