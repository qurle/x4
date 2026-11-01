# UI

Owner priority 1: **design and interaction delight**. This page maps the design to LVGL objects,
lists every screen/state/transition, and records every compromise.

**Design source:** Figma `fgEvP6Ms0bOAbWxfkFki4R`, section **73:368 "states"** (read 2026-10-31
through the Figma MCP, personal account). Reference render: `ui/assets/figma/_reference-states.png`.
UI v2 (`core/src/ui/ui.cpp`) is implemented from that section. v1 (four rings on black) was a
placeholder built before Figma access worked and is gone.

## Design language (from the file)

- **The photo is the interface.** Every camera state is drawn over a full-bleed 320×240 photo; UI
  is either absent ("minml") or a thin footer.
- **Two voices.** *Instrument Serif* in large sizes for words (Saving…, Error, video / settings /
  media, setting values); *Space Mono Bold 13* for small numbers over the photo (15%, 820 MB).
- **Ink on grey for settings.** A light `#D1D1D1` panel with 2 px `#1E1E1E` frames and solid ink
  bars, like printed toggles.
- **Material states.** Saving = holographic light-leak overlay; Error = red smeared overlay.
- One Figma variable: `Backround/Primary = #1E1E1E`.

## Tokens

`ui/tokens.json` (v2, values from Figma marked `figma: true`) → `core/src/ui/tokens.h`.

| group | values |
|---|---|
| colour (Figma) | bg/ink `#1E1E1E`, settingsBg `#D1D1D1`, onPhoto `#FFFFFF`, glow `#212121` (α≈33 % at the corner), rec `#FF2E2E` |
| colour (not in Figma: diagnostics/toasts) | surface `#141414`, surface2 `#2A2A2A`, line `#3A3A3A`, fg `#F2F2F2`, fgMuted `#9A9A9A`, fgFaint `#5A5A5A`, ok, warn, error |
| type | state Instrument Serif 48 · tile 36 · tileSmall 34 · menu 32 · sub 24 · footer Space Mono Bold 13 · (splash 64, diagnostics Space Mono 12/14/18) |
| layout | footer 47 px, footer text inset 8 / 4; tap menu 80 px; settings padding 16, gap 8, tiles 140×100, bar 25, slider thumb 19; "n of 4" 48 px from the bottom; portrait crop 180 px |
| motion (ours) | fast 140 ms (menu), base 260 ms (overlays), slow 520 ms (white-out, screens), spring 420 ms overshoot (slider thumb) |

Images exported from Figma (`ui/assets/figma/*.png` → `core/src/ui/assets/*.c`, RGB565 + 8-bit
alpha, via `python tools/gen_assets.py`; CI checks they are fresh):

| image | size | from |
|---|---|---|
| `overlay-saving` | 320×240, alpha 47–75 % | node 15:86 (holographic overlay, 75 % opacity baked in) |
| `overlay-error` | 320×240, alpha 90 % | node 18:448 |
| `flash-recharge` | 82×32 | component 81:184 (bolt + glow) |
| `dither-strip` | 40×100 | node 73:263 (settings effect tile) |
| `glow-corner-left/right` | 96×47 | reconstructed from the Footer render (gaussian σ≈36 px, #212121, peak α 85) because the corner ellipses are SVG blurs |

## Fonts

| font | licence |
|---|---|
| Instrument Serif Regular | **SIL OFL 1.1** (VERIFIED, `ui/fonts/OFL-InstrumentSerif.txt`) |
| Space Mono Regular + Bold | **SIL OFL 1.1** (VERIFIED, `ui/fonts/OFL-SpaceMono.txt`) |

`tools/gen_fonts.sh` (lv_font_conv 1.5.2), 4 bpp, no compression, glyphs printable ASCII
+ ° · × – — … •. **No Cyrillic** (owner decision 2026-10-31). Sizes are the Figma sizes listed
above. The serif is only used ≥ 24 px, where its hairlines survive on the 320×240 panel.

## Screens, states and transitions

| Figma frame | device | LVGL objects | behaviour |
|---|---|---|---|
| (none) splash | Splash | serif 64 "x4cam" (`X4_PROJECT_NAME`), mono caption | fade + rise, 1.4 s, then fade to viewfinder |
| **preview: full, photo** | Viewfinder, `ui_mode = full` | full-screen `lv_image` (RGB565 swapped, live preview), footer: two corner-glow images + Space Mono Bold 13 labels | left = **battery estimate "~72%"** (energy counting, see compromises; long-press it → "Bank charged?" confirm resets to 100 %), right = SD free ("820 MB" / "12.3 GB" / "NO SD") |
| **preview: minimal ui, photo, flash recharging** | Viewfinder, `ui_mode = minml` | photo only; flash-recharge image bottom-centre | the bolt shows (in both modes) while the flash is armed and cooling down |
| **preview: dithered** | Viewfinder, `ui_effect = dithered` | same; each preview frame dithered in place (Bayer 4×4, 4 levels per channel) | photos on SD stay original; `shot.json` records `"effect":"dithered"` for the companion |
| **preview: portrait** | Viewfinder, `ui_orient = portrait` | two black 70 % bands (70 px each side) leave a bright 180 px centre crop | `shot.json` records `"orientation":"portrait"` |
| **preview: video recording** | — | — | not in v1 (no video capture yet); the rec glow/dot asset is reserved |
| **on tap: buttons** | tap menu | 80 px vertical gradient black 80 %→0, three cells with serif 32 white labels | tap the photo: fades in (140 ms), auto-hides after 4 s, tap again to hide. Only *settings* is shown in v1 (centre cell, Figma position); *video* and *media* are hidden (owner, 2026-11-01) |
| **settings** | Settings screen | `#D1D1D1` screen, four 140×100 tiles with 2 px ink frames | opens with "over bottom" slide (260 ms). Leave: swipe, Back, or press the shutter (the shutter always returns to the viewfinder) |
| settings · orientation | tile 1 | two ink bars: top + bottom (`horzntl`) or left + right (`portrt`), serif 36 | tap toggles |
| settings · cam delay | tile 2 | 19 px ink thumb that slides left→right, value serif 36 bottom-right | drag across the tile: 0, 0.2 … 1.0 s; thumb springs into place (420 ms overshoot) |
| settings · effect | tile 3 | serif 34 bottom-left; dither strip on the right edge when dithered | tap toggles normal / dithered |
| settings · ui | tile 4 | serif 34 `minml` / `full` | tap toggles |
| **state: saving (1 of 4 / 3 of 4)** | saving overlay | holographic image, serif 48 "Saving..." at y 89, serif 24 "k of n" 48 px above the bottom | fades in (260 ms) from shutter until all photos are stored; k = stored + 1 |
| **state: error** | error overlay | red image, serif 48 "Error" + our serif 24 reason line ("no sd card", "sd card full", "too hot, cooling") | while the state machine is in Error |
| (none) shutter moment | top layer | white rectangle | 70 % (100 % with flash) → 0 over 520 ms + two-tone click |
| (none) toasts | top layer | dark pill, Space Mono Bold 13 | "saved · 3.4s", "not ready · busy", menu hints |
| (none) diagnostics / bring-up | Diagnostics screen | unchanged from v1 (developer screen) | long-press shutter or long-press the photo |

Press feedback: settings tiles tint with ink (20 %) while pressed; menu labels drop to 60 %;
diagnostics pills scale to 94 % with an overshoot transition. Haptics: none on the SE; the speaker
gives the click.

## Settings → config

| tile | option | values | effect on the rig |
|---|---|---|---|
| orientation | `ui_orient` | 0 horzntl, 1 portrt | preview crop + `shot.json` (companion crops) |
| cam delay | `cam_delay_ms` | 0, 200…1000 | camera N captures (N−1) × delay after the first (`dl=` in SHOOT, see PROTOCOL.md). 0 = all four at once |
| effect | `ui_effect` | 0 normal, 1 dithered | preview dither + `shot.json` |
| ui | `ui_mode` | 0 minml, 1 full | footer on/off |

Changes go through `x4core::app::setOption()` → `conf::set()` (validated, persisted in NVS, also
editable on the web page).

## Compromises (design → device)

| design | device | why |
|---|---|---|
| footer left "15%" (battery) | "~15%" estimate | no battery sensor on the SE and the bank is not readable: x4 integrates an estimated draw (per-device power model × time) and subtracts it from the bank's usable energy. "~" marks it as an estimate; calibrate `pw_*_mw` from MEASUREMENTS.md, reset with a long-press when the bank is charged |
| tap-menu gradient with eased stops (16 stops) | LVGL linear 2-stop gradient, 80 % → 0 | LVGL draws 2-stop gradients cheaply; visually close at 80 px |
| footer corner glows = blurred SVG ellipses | pre-rendered gaussian alpha images | no runtime blur on the MCU |
| holographic / red overlays are photographic | baked RGB565 + alpha images (≈ 230 KB each in flash) | static; drawing one full-screen alpha image per redraw is acceptable because nothing else animates underneath while saving |
| saving/error overlays over the *last photo* | over whatever the viewfinder shows (preview pauses at the shutter, so it is the frame just before the shot) | the Core never decodes the 5 MP result |
| "portrait" mock shows three photos (centre + two dim copies) | one live photo with dimmed sides | same look, one camera stream |
| "dithered" mock (coarse colour dither) | 4-level-per-channel Bayer 4×4 on the preview | close; tune levels after seeing it on the panel |
| settings type uses cap-height trim (`text-box-trim`) | normal label boxes, positions adjusted by eye | LVGL has no cap trimming |
| cam delay range 0.2–1 s in the file | 0 + 0.2–1 s | 0 keeps simultaneous capture, the rig's main purpose ([OPEN_QUESTIONS Q18](OPEN_QUESTIONS.md)) |
| video, media | hidden | not in v1. Media (on-device gallery) is moderate work: decode each 5 MP JPEG at 1/8 scale (≈ 324×243, TJpgDec does this natively) or save a thumbnail per shot. Video is bigger: needs a defined meaning for four cameras and an MJPEG pipeline to the SD |

## Performance

Partial rendering with 2 × 40-line buffers in internal DMA RAM; flush under the SPI lock;
labels only rewritten on change; diagnostics text at 4 Hz. The dither pass costs CPU on the UI
loop per preview frame (≈ 3–5 ms, ASSUMPTION). Full-screen overlays (saving, error) redraw the
whole screen once when they appear, then only the "k of n" label changes. **Measure FPS on the
real panel** for each screen and record it in MEASUREMENTS.md.

## Live preview (Stage 4)

One camera streams the viewfinder at `prev_res` (0 = QVGA 320×240, matches the screen, default;
1 = VGA, decoded at ½ scale). The Core polls `/capture?res=<prev_res>` at `prev_fps` (5) and decodes
with M5GFX's TJpgDec into a PSRAM canvas that LVGL shows as `RGB565_SWAPPED` (no copy). The
preview camera is `prev_cam`, or **the first online camera if that one is offline**, so a rig with
any single camera still has a viewfinder.

Resolution switching: only the preview camera ever leaves the shot size. On SHOOT it switches to
5 MP right away (one dropped frame after the switch), captures at the scheduled time, and goes
back to QVGA/VGA at the next preview request. Each camera reports its slowest switch (`sw=` in
heartbeats); the Core stretches the trigger lead to `max(trig_lead_ms, sw + 40 ms)` while the
preview runs, so that camera is not late. Paused for every shot, refused when hot. Fallbacks:
lower `prev_fps`, QVGA instead of VGA, turn it off (`prev_enabled=0`).

## LVGL Pro / XML

Hand-written LVGL for now; `core/src/ui_gen/` is reserved for LVGL Editor C export
(`editor_project_init("")` after `lv_init()`, `-DLV_LVGL_H_INCLUDE_SIMPLE` already set). Runtime
XML needs a paid licence and `lv_xml` left open-source LVGL in 9.5; the Editor is free for personal
non-commercial use; the CLI appears in the paid tier (*Not sure* for personal use). Sources:
lvgl.io/editor, pro.lvgl.io, lvgl.io blog "lv_xml removal from v9.5".

## Smoke test (`hello-ui`)

`pio run -e hello-ui -t upload`: "x4cam" (serif 64) with an eased fade + 18 px rise on
`#1E1E1E`, tap to replay. Also runs the power-safety boot path.
