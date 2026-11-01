# UI

Owner priority 1: **design and interaction delight**. This page maps the design to LVGL objects,
lists every screen/state/transition, and records every compromise.

**Design source:** Figma `fgEvP6Ms0bOAbWxfkFki4R`: section **73:368 "states"**, section **83:2147
"settings states"** and the components they use (Footer States 83:1905, saving overlay 83:2228,
error overlay 83:2286, Rec Timer 83:866, Overlay Element 18:587). Pulled through the REST API with
`tools/figma_pull.py` into `ui/figma/` (node JSON, image fills, reference renders; committed, so
design changes show up as diffs). UI v3 (`core/src/ui/ui.cpp`) is implemented from that node data
(2026-11-01). Older reference render: `ui/assets/figma/_reference-states.png`.

## Design language (from the file)

- **The photo is the interface.** Every camera state is drawn over a full-bleed 320×240 photo; UI
  is a thin footer and nothing else.
- **Two voices.** *Instrument Serif* in large sizes for words (Saving…, Error, video / settings /
  media, setting values); *Space Mono Bold 13* for small numbers over the photo (-:--, 47%, 820 MB).
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
| colour (tiles) | ink `#000000` (tile frames, bars, text: the tile components use pure black), rec `#EE2E2E`, sliderText `#2E2E2E` (see cam delay) |
| layout | footer 47 px, text inset 8, record dot 9 at (8, 29), time at x 22; tap menu 80 px; settings padding 16, gap 8, tiles 140×100, bars 25 (horzntl) / 30 (vertcl), style strip 40 at x 100; "Saving..." at x 105; "k" right-aligned to x 147; portrait crop 180 px |
| text placement | each label's top = Figma baseline − LVGL baseline offset (`line_height − base_line` of the generated font). Baseline = Figma box top + ascent (0.99 em serif, 1.12 em mono; Figma line heights are 130 % / 148 %, i.e. ascent + descent), or + cap height (0.72 em) for the cap-trimmed tile labels. Values: `*Y` tokens in `ui/tokens.json` |
| motion (ours) | fast 140 ms (menu), base 260 ms (overlays, saving opacity steps), slow 520 ms (white-out, screens), spring 420 ms overshoot (cam delay fill) |

Images (`ui/assets/figma/*.png`) are built from the pulled data by `python tools/gen_figma_assets.py`
(shapes, gradients and blurs from the node JSON, photos from the image fills in `ui/figma/fills/`),
then converted to `core/src/ui/assets/*.c` by `python tools/gen_assets.py` (RGB565 + 8-bit alpha;
plain RGB565 when opaque). CI checks both steps are fresh.

| image | size | from |
|---|---|---|
| `overlay-saving` | 320×240, opaque RGB565 | fill of 83:2240 (565×740, STRETCH with its imageTransform); opacity set at runtime |
| `overlay-error` | 320×240, alpha 90 % | fill of 83:2288 (960×720, FILL), layer opacity 90 % baked |
| `dither-strip` | 40×100 | fill of 83:2217 (80×200, FILL), box-filtered (matches the Figma render best) |
| `glow-corner-left/right` | 72×72 | ellipse 18:687: 80×80, radial `#212121` 50 % → 10 %, layer blur 32, centred on the footer's bottom corner; the part above the screen bottom (reaches 25 px above the footer) |
| `glow-mid` | 144×72 | same ellipse, under the flash charge |
| `rec-glow` | 54×31 | ellipse 83:868: 60×23 `#EE2E2E` 50 %, layer blur 16, centred on the timer's bottom-left corner, clipped to the screen |
| `flash-bolt` | 7×10 | vector 83:1865 ("flash (compact)" 83:1866), white |

Figma layer blur radius *r* is drawn as a gaussian with σ = 0.42 *r*, fitted to the Figma renders
of 18:523 and 83:867 (mean alpha error < 0.4 %).

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
| **preview: photo** | Viewfinder | full-screen `lv_image` (RGB565 swapped, live preview), footer (Footer States 83:1905): corner-glow images + Space Mono Bold 13 labels, baseline at y 38 of the footer | left = **record state** (Rec Timer 83:866), **video mode only** (Footer state "Video"; hidden in "Default" and "Flash Recharge", with its corner glow): white 9 px dot + `-:--`; red `#EE2E2E` dot, red glow and `m:ss` while recording. Centre = **flash charge** (83:1188, Footer state "Flash Recharge"): 7×10 bolt, 4 px gap, `0…100%`, group centred, with a centre glow, while the flash is armed and recharging (percent, not seconds, so it moves in small steps; the bolt fades in with it). Right = SD free (`820 MB` / `12.3 GB` / `NO SD`), right edge 8 px in |
| **preview: dithered** | Viewfinder, `ui_effect = dithered` | same; each preview frame dithered in place (Bayer 4×4, 4 levels per channel) | photos on SD stay original; `shot.json` records `"effect":"dithered"` for the companion |
| **preview: portrait** | Viewfinder, `ui_orient = portrait` | two black 70 % bands (70 px each side) leave a bright 180 px centre crop | `shot.json` records `"orientation":"portrait"` |
| **preview: video, not recording / recording** | Viewfinder | footer left block appears (dot + `-:--`), then the recording look | video is not implemented yet: `Status.videoMode` and `Status.recording` are always false, so in v1 the footer shows only the flash charge (when charging) and SD free |
| **on tap: buttons** | tap menu | 80 px vertical gradient black 80 %→0, three cells with serif 32 white labels | tap the photo: fades in (140 ms), auto-hides after 4 s, tap again to hide. Only *settings* is shown in v1 (centre cell, Figma position); *video* and *media* are hidden (owner, 2026-11-01) |
| **settings** | Settings screen | `#D1D1D1` screen, four 140×100 tiles with 2 px ink frames | opens with "over bottom" slide (260 ms). Leave: swipe, Back, or press the shutter (the shutter always returns to the viewfinder) |
| settings · orientation | tile 1 (83:2168) | black bars 140×25 top + bottom (`horzntl`) or 30×100 left + right (`vertcl`), serif 36 centred | tap toggles |
| settings · cam delay | tile 2 (83:2173) | black fill from the left, width = delay / 1 s (0 → 1 px, 0.2 s → 28, 0.8 s → 112, 1 s → 140); value serif 36, right edge 8 px in, white over the fill and `#2E2E2E` over the grey | drag across the tile: 0, 0.2 … 1.0 s; the fill springs to its width (420 ms overshoot) |
| settings · style | tile 3 (83:2215) | 40×100 strip on the right: gradient transparent → `#1E1E1E` (normal) or the dither image (dithered); serif 34 bottom-left | tap toggles normal / dithered |
| settings · flashlight | tile 4 (83:2219) | `no flash`: black tile, `#D1D1D1` label; `flash`: empty tile, black label; serif 36 centred | tap arms / disarms the flash (same as the shutter gesture). Not persisted: the rig always boots with the flash off |
| **state: saving (k of 4)** | saving overlay (83:2228) | holographic image at k/n opacity (25 / 50 / 75 / 100 % for 4 cameras), serif 48 "Saving..." at (105, baseline 142), serif 24 `k` right-aligned to x 147 + ` of n` from x 147 (baseline 223) | fades in (260 ms) from shutter until all photos are stored; k = stored + 1; the image eases to the next opacity step (260 ms) |
| **state: error** | error overlay (83:2286) | red image (90 %), one serif 48 line centred at baseline 142: the reason when known (`No SD card`, `SD is full`, `Too hot, cooling`), otherwise `Error` | while the state machine is in Error |
| (none) shutter moment | top layer | white rectangle | 70 % (100 % with flash) → 0 over 520 ms + two-tone click |
| (none) toasts | top layer | dark pill, Space Mono Bold 13 | "saved · 3.4s", "not ready · busy", menu hints |
| (none) diagnostics / bring-up | Diagnostics screen | unchanged from v1 (developer screen) | long-press shutter or long-press the photo |

Press feedback: settings tiles tint with ink (20 %; the black "no flash" tile lightens to 80 %) while pressed; menu labels drop to 60 %;
diagnostics pills scale to 94 % with an overshoot transition. Haptics: none on the SE; the speaker
gives the click.

## Settings → config

| tile | option | values | effect on the rig |
|---|---|---|---|
| orientation | `ui_orient` | 0 horzntl, 1 vertcl | preview crop + `shot.json` (companion crops) |
| cam delay | `cam_delay_ms` | 0, 200…1000 | camera N captures (N−1) × delay after the first (`dl=` in SHOOT, see PROTOCOL.md). 0 = all four at once |
| effect | `ui_effect` | 0 normal, 1 dithered | preview dither + `shot.json` |
| flashlight | (runtime, `FlashArm` / `FlashDisarm`) | flash / no flash | flash fires with the next shots when charged |

Changes go through `x4core::app::setOption()` → `conf::set()` (validated, persisted in NVS, also
editable on the web page).

## Compromises (design → device)

| design | device | why |
|---|---|---|
| tap-menu gradient with eased stops (16 stops) | LVGL linear 2-stop gradient, 80 % → 0 | LVGL draws 2-stop gradients cheaply; visually close at 80 px |
| footer corner glows = blurred SVG ellipses | pre-rendered gaussian alpha images (σ = 0.42 × Figma blur radius, fitted) | no runtime blur on the MCU |
| centre glow sits on the Middle frame's left edge (x ≈ 135), apparently copied from Bottom Left | centred under the flash group (x 160) | *Not sure* which is intended; one line in `ui.cpp` (`g.glowMid` position) |
| `820 MB` uses a thin space (U+2009), missing in Space Mono (Figma falls back to another font) | number and unit as two labels, 3 px apart | same width as the design |
| cam delay value: white text with EXCLUSION blend | two labels: `#2E2E2E` on the grey, white inside the fill (which clips it) | LVGL has no exclusion blend; same pixels for these two backgrounds |
| error example text "SD is full" | sentence-case reasons in the same style | only SD full is drawn in the file |
| holographic / red overlays are photographic | baked images: saving opaque RGB565 (150 KB, opacity at runtime), error RGB565 + alpha (225 KB) | static; drawing one full-screen image per redraw is acceptable because nothing else animates underneath while saving |
| saving/error overlays over the *last photo* | over whatever the viewfinder shows (preview pauses at the shutter, so it is the frame just before the shot) | the Core never decodes the 5 MP result |
| "portrait" mock shows three photos (centre + two dim copies) | one live photo with dimmed sides | same look, one camera stream |
| "dithered" mock (coarse colour dither) | 4-level-per-channel Bayer 4×4 on the preview | close; tune levels after seeing it on the panel |
| settings type uses cap-height trim (`text-box-trim`) | label tops computed from the cap-height baseline (see Tokens) | LVGL has no cap trimming |
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
