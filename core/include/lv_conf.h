// LVGL 9.6 configuration for the x4 Core (CoreS3-SE, 320x240 ILI9342C, RGB565).
// Only non-default settings are listed; lv_conf_internal.h fills in everything else.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// Use the C library heap (internal RAM + PSRAM via the ESP32 heap caps) instead of LVGL's own
// fixed pool: widgets and image caches can then spill into the 8 MB PSRAM.
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_DEF_REFR_PERIOD 16  // ms; ~60 fps cap. Real FPS is measured and shown in diagnostics.
#define LV_DPI_DEF 130

#define LV_USE_OS LV_OS_NONE  // LVGL runs only in the UI task; other tasks talk to it via queues

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

#define LV_USE_SYSMON 0
#define LV_USE_PERF_MONITOR 0

// Draw: software renderer. 4-bpp anti-aliased fonts.
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_COMPLEX 1  // rounded corners, shadows (shadows are expensive: see docs/UI.md)

// Fonts: x4 uses Instrument Serif + Space Mono converted with lv_font_conv (core/src/ui/fonts/).
// Montserrat stays as a fallback for symbols.
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_FONT_COMPRESSED 0

// Widgets we use. Others stay at LVGL defaults (enabled) but are not linked unless referenced.
#define LV_USE_LABEL 1
#define LV_USE_IMAGE 1
#define LV_USE_BAR 1
#define LV_USE_ARC 1
#define LV_USE_SPINNER 1
#define LV_USE_CANVAS 1

// Decoders: the live preview decodes JPEG with M5GFX (TJpgDec) into a buffer that LVGL shows.
#define LV_USE_TJPGD 0

#define LV_USE_OBSERVER 1
#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif  // LV_CONF_H
