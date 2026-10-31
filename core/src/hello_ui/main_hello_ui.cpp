// hello-ui: the LVGL smoke test kept as its own environment (pio run -e hello-ui).
// Black screen, "Four Eyes" in Instrument Serif, eased fade-in + short upward slide, tap to replay.
// Also exercises the full power-safety boot path, because ANY firmware on the Core must keep the
// 5 V output off before the board ever sees the rail.
#include <M5Unified.h>

#include "../display/lvgl_port.h"
#include "../hw/flash.h"
#include "../hw/power_safety.h"
#include "../log/log_setup.h"
#include "../ui/tokens.h"
#include "x4/log.h"
#include "x4/version.h"

using namespace x4ui;

namespace {

lv_obj_t* g_title = nullptr;
lv_obj_t* g_sub = nullptr;
constexpr int32_t kSlide = 18;

void setOpa(void* o, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }
void setY(void* o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t*)o, v, 0); }

void animate(lv_obj_t* o, uint32_t delay) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_duration(&a, tok::kMsSlow);
  lv_anim_set_path_cb(&a, tok::pathBase());
  lv_anim_set_exec_cb(&a, setOpa);
  lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
  lv_anim_start(&a);
  lv_anim_set_exec_cb(&a, setY);
  lv_anim_set_values(&a, kSlide, 0);
  lv_anim_start(&a);
}

void replay() {
  setOpa(g_title, 0);
  setOpa(g_sub, 0);
  animate(g_title, 0);
  animate(g_sub, tok::kMsStagger * 4);
  X4_LOGI("ui", "replay", "fps=%.1f", x4core::display::stats().fps);
}

void onTap(lv_event_t*) { replay(); }

}  // namespace

void setup() {
  x4core::flash::earlyOff();
  x4core::logsys::begin("core");
  auto cfg = M5.config();
  cfg.output_power = false;          // NEVER drive 5 V onto Port A: the rail feeds us
  cfg.internal_imu = false;
  cfg.internal_mic = false;
  cfg.external_display_value = 0;    // no Port A probing
  cfg.external_speaker_value = 0;
  M5.begin(cfg);
  x4core::power::enforce("boot");
  X4_LOGI("boot", "banner", "app=hello-ui fw=%s git=%s", X4_FW_VERSION, X4_GIT_HASH);

  x4core::display::begin(160);
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, tok::c(tok::kBg), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(scr, onTap, LV_EVENT_CLICKED, nullptr);

  g_title = lv_label_create(scr);
  lv_label_set_text(g_title, "Four Eyes");
  lv_obj_set_style_text_font(g_title, tok::fontDisplay(), 0);
  lv_obj_set_style_text_color(g_title, tok::c(tok::kFg), 0);
  lv_obj_align(g_title, LV_ALIGN_CENTER, 0, -10);

  g_sub = lv_label_create(scr);
  lv_label_set_text(g_sub, "tap to replay");
  lv_obj_set_style_text_font(g_sub, tok::fontCaption(), 0);
  lv_obj_set_style_text_color(g_sub, tok::c(tok::kFgMuted), 0);
  lv_obj_align(g_sub, LV_ALIGN_CENTER, 0, 40);
  replay();
}

void loop() {
  M5.update();
  uint32_t wait = x4core::display::handle();
  x4core::power::poll(millis());
  x4core::flash::poll();
  delay(wait > 5 ? 5 : wait);
}
