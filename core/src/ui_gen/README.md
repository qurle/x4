# ui_gen/ — generated UI code (never hand-edit)

Reserved for code exported from LVGL Pro / the LVGL XML editor (compile-time export; the runtime
XML loader is not public). Regenerating the UI overwrites only this folder.

How it plugs in (docs/UI.md "LVGL Pro / XML"):
1. Export the editor project into this folder.
2. Keep `-DLV_LVGL_H_INCLUDE_SIMPLE` (already set in platformio.ini).
3. Call `editor_project_init("")` once after `lv_init()` (in `display::begin`).
4. Wrap generated screens from `core/src/ui/ui.cpp` so the app API (`x4ui::update`, moments,
   `x4core::app::request`) stays the same.

The current UI is hand-written in `core/src/ui/` from `ui/tokens.json`.
