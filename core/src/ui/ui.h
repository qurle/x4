// x4 on-device UI (LVGL 9, hand-written, driven by tokens.h). The UI never touches the network
// or the SD card: it renders x4core::Status and emits requests through x4core::app::request().
// Screens, states and transitions: docs/UI.md. Generated (LVGL Pro/XML) code would live in
// core/src/ui_gen/ and be wrapped here.
#pragma once
#include <stdint.h>

#include "../app/status.h"
#include "x4/input.h"

namespace x4ui {

enum class Screen : uint8_t { Splash, Main, Settings, Diagnostics };

void begin(bool helloMode);
// Refresh visible widgets from the snapshot. Cheap: only changed values are written.
void update(const x4core::Status& s, uint32_t nowMs);
// Navigation from physical inputs (shutter long-press, swipes, encoder).
void onAction(const x4::input::Event& e);
Screen current();
void show(Screen s);

// Moments (called by the app when effects happen).
void shutterMoment(bool flash);      // white-out + iris + click sound
void shotCaptured();                 // "captured" tick, frames are in camera PSRAM
void shotStored(bool partial, uint32_t totalMs, bool overBudget);
void shutterRefused(const char* why);
void toast(const char* text, uint32_t colorHex, uint32_t ms = 2200);

// Live preview: decoded on the UI loop (JPEG -> RGB565) into an LVGL image.
// scale: 1.0 for QVGA (320x240 = the screen), 0.5 for VGA.
void setPreviewJpeg(const uint8_t* jpg, uint32_t len, float scale = 1.0f);
void clearPreview();

}  // namespace x4ui
