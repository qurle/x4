# UI

Owner priority 1: **design and interaction delight**. This page maps the design to LVGL objects,
lists every screen/state/transition, and records every compromise.

> **Status of the design source.** The Figma node
> `fgEvP6Ms0bOAbWxfkFki4R` / `73:368` (section with a couple of frame sets) is the final target.
> It **could not be read from this environment**: the Figma connection returned "no access to this
> file" (2026-10-31). So UI v1 is built on **placeholder tokens** (`ui/tokens.json`, marked
> ASSUMPTION) derived from the brief: black screen, Instrument Serif + Space Mono, "Four Eyes".
> Everything is token-driven, so importing the real values is a JSON edit + `python
> tools/gen_tokens.py`. What I need from the owner: [OPEN_QUESTIONS Q2](OPEN_QUESTIONS.md).

## Design language (v1, placeholder until Figma)

- **Dark, editorial, camera-like.** Pure black background (the 2.0" IPS panel's black is the
  frame), warm paper-white text, one hot accent (shutter red-orange) used only for the act of
  shooting.
- **Two voices:** *Instrument Serif* for the human, expressive layer (splash, shot counter,
  headings); *Space Mono* for the machine layer (states, numbers, diagnostics). Uppercase mono for
  status lines.
- **Four eyes** as the core metaphor: four rings, one per camera, that change look with the
  camera's state. Calm when idle, alive while shooting.
- **Motion has meaning**: ease-out for arrivals, overshoot for the shutter moment and counters,
  quick linear feedback for presses. Staggered reveals (40 ms) instead of everything at once.

## Tokens

`ui/tokens.json` → `core/src/ui/tokens.h` (generated; CI checks it is fresh).

| group | tokens |
|---|---|
| colour | bg `#000000`, surface `#141414`, surface2 `#1F1F1F`, line `#2E2E2E`, fg `#F2EFE8`, fgMuted `#8C877E`, fgFaint `#55524D`, accent `#FF4A1C`, ok `#7BD88F`, warn `#FFB020`, error `#FF5A4F`, flash `#FFF6D6` |
| type | display Instrument Serif 64, title 40, heading 24, headingIt Instrument Serif Italic 24, body Space Mono 14, bodyBold Space Mono Bold 14, data Space Mono 18, caption Space Mono 12 |
| space | 2, 4, 8, 12, 16, 24, 32 px |
| radius | 0, 6, 12, 20, pill |
| border | hairline 1, strong 2 |
| motion | instant 60 ms linear, fast 140 ms ease-out, base 260 ms ease-out, slow 520 ms ease-in-out, spring 420 ms overshoot, stagger 40 ms |

LVGL styles are applied from these constants in `core/src/ui/ui.cpp` (no magic numbers in
screens). "Materials" (blur, glass) are not tokens yet: LVGL 9 software rendering has no cheap
backdrop blur on this MCU (see compromises).

## Fonts

| font | licence | source |
|---|---|---|
| Instrument Serif Regular + Italic | **SIL Open Font License 1.1** (VERIFIED: `ui/fonts/OFL-InstrumentSerif.txt`, copyright 2022 The Instrument Serif Project Authors) | google/fonts `ofl/instrumentserif` |
| Space Mono Regular + Bold | **SIL OFL 1.1** (VERIFIED: `ui/fonts/OFL-SpaceMono.txt`, copyright 2016 The Space Mono Project Authors) | google/fonts `ofl/spacemono` |

OFL allows embedding in firmware; keep the licence files with the fonts (done).

Conversion: `tools/gen_fonts.sh` (lv_font_conv 1.5.2), **4 bpp** anti-aliasing, no compression
(faster rendering), glyphs **printable ASCII + ° · × – — … •** only. **Cyrillic is not included**
(ASSUMPTION, [OPEN_QUESTIONS Q4](OPEN_QUESTIONS.md)); adding `0x400-0x4FF` costs roughly +70 % per
font. Flash cost now ≈ 700 KB of C source → ≈ 300 KB in flash (serif 64 is the largest).

Legibility on 320×240: Instrument Serif is a display face with thin hairlines; below ~20 px those
hairlines fall under one pixel and the text shimmers. v1 therefore uses the serif only at 24, 40
and 64 px and Space Mono for everything ≤ 18 px. Check on the real panel (bring-up step 2).

## Screens, states and transitions

| screen / state | LVGL objects | enters with | leaves with |
|---|---|---|---|
| **Splash** | screen, serif-64 label "Four Eyes", mono caption "x4 · version" | fade + 10 px rise, staggered (title 0 ms, caption 160 ms), 260 ms ease-out | screen fade (260 ms) to Main after 1.4 s (to Bring-up in `core-hello`) |
| **Main · waiting** | state label "WAITING FOR CAMERAS n/4", four `lv_arc` rings (58 px), ring numerals (serif 24), captions | rings reveal staggered 40 ms | – |
| **Main · ready** | rings paper-white outline, caption = camera temperature, counter (serif 40) + "shots", hint "press to shoot", flash state | ring look change (instant) | shutter |
| **Main · live preview** | full-screen `lv_image` (RGB565 swapped, from the preview camera) under the same overlays | image appears when the first frame decodes | shutter pauses preview; hidden on stop |
| **Shutter moment** | top-layer white-out rectangle (`flash` colour) | bg opacity 70 % (100 % with flash) → 0 in 520 ms ease-out; two-tone click (2.6 kHz 18 ms + 1.3 kHz 30 ms) | – |
| **Main · capturing** | rings accent; arc width springs 8 → 2 px (420 ms overshoot) per camera as it starts capturing | state "CAPTURING" | all ACKs |
| **Main · captured** | short tick sound (1.8 kHz 12 ms): frames are safe in camera PSRAM | – | – |
| **Main · saving** | ring indicator arc = transfer % (ok green), caption "37%", state "SAVING k/n" | per-camera progress | all stored |
| **Main · saved** | rings solid ok, caption "saved"; counter rolls up 16 px with overshoot; toast "saved · 3.4 s"; rising chirp 2.2→3.3 kHz | toast slides up 12 px with spring + fades in 140 ms, fades out after 2.2 s | – |
| **Main · partial** | failed rings red "failed"; amber toast "saved with missing eyes · 4.1 s" | – | – |
| **Shutter refused** | state line shakes ±6 px ×2 (50 ms), amber toast "not ready · busy/error/not_ready", low 300 Hz beep | – | – |
| **Error** | state line red: "NO SD CARD", "SD CARD FULL", "TOO HOT · COOLING" | – | fault cleared |
| **Camera lost** | that ring red outline, caption "lost" | – | heartbeat returns |
| **Diagnostics / Bring-up** | serif title, scrollable mono-12 block: system (fw, uptime, FPS, worst frame, 5 V output, OTG, rail mV, temp, heap/PSRAM, reset reason, boots, brown-outs, SD, button, flash), cameras (on/off, IP, temp, RSSI, heartbeat age, last result, capture offset), last shot (stored, time, skew, stations), log tail (6 lines); pill buttons Back / Arm / Flash / SD / Shoot | long-press shutter, long-press screen, or swipe up: screen move-top 260 ms | Back, swipe down, long-press again: fade |
| **Flash confirm** (on-screen confirm for the test pulse) | top-layer dim overlay 80 %, heading "Fire one 50 ms flash?", amber warning, Cancel / Fire pills | heading reveal | either button |
| **Touch test** | red 12 px dot follows taps in the diagnostics text area | – | – |
| Empty states | counter "0", hint only when armed, rings "off" captions | – | – |
| Loading | splash; "STARTING" state; camera rings "off" until registered | – | – |

Press feedback on every pill button: scale 94 % + colour change with an overshoot transition
(140 ms). Haptics: **none available** (the CoreS3-SE has no vibration motor, VERIFIED: no motor in
the SE schematic); sound through the 1 W speaker replaces it (`ui_sound`, `ui_volume`).

Navigation map: `shutter.long` / `touch.swipe_up` → Diagnostics; `touch.swipe_down` / Back →
Main. All via logical actions ([FIRMWARE.md#input](FIRMWARE.md#input)).

## Performance

- Render: partial mode, 2 × 40-line buffers in internal DMA RAM, flush via `writePixels` with the
  SPI lock held only for that flush. `LV_DEF_REFR_PERIOD 16` (≤ 60 fps).
- Labels are only re-set when their text changes (`setText` diff), the diagnostics text refreshes
  at 4 Hz, so idle frames redraw almost nothing.
- FPS and worst frame time are measured every second (`LV_EVENT_RENDER_START/READY`) and shown on
  the diagnostics screen and in `/api/status`. **Report real numbers in MEASUREMENTS.md** after
  each animation is added (none measured yet: no hardware).
- Expensive things avoided: shadows (`shadow_width = 0` everywhere), full-screen opacity on
  complex trees (the white-out is a single plain rectangle on the top layer), transforms only on
  small pill buttons.

## Live preview (Stage 4)

The preview camera serves QVGA JPEGs at `/capture?res=0`; the Core's `preview` task polls at
`prev_fps` (5) and the UI decodes on the loop with M5GFX's TJpgDec straight into a PSRAM canvas
whose buffer LVGL shows as `LV_COLOR_FORMAT_RGB565_SWAPPED` (no copy, no byte swap). The ESP32-S3
has no hardware JPEG decoder, so each frame costs CPU on the UI loop (measured `us=` in
`evt=preview_decode` at DEBUG; ASSUMPTION ≈ 30–60 ms per QVGA frame).
Preview is paused for every shot and refused when hot.

Fallbacks if the Core cannot keep up (in order): lower `prev_fps` to 2–3; decode at half scale
(TJpgDec 1/2) and stretch; show the preview only on the diagnostics screen; turn it off
(`prev_enabled=0`). Risk: the preview camera switches resolution QVGA ↔ 5 MP for each shot
(`switch_ms` is logged); if that is longer than `trig_lead_ms`, raise the lead or pick a preview
camera whose small offset you accept.

## Compromises (mockup → LVGL)

Because the mockups were not readable, these are *anticipated* compromises; each needs a
before/after note once the Figma frames are in.

| likely in the mockup | on the device | why |
|---|---|---|
| backdrop blur / frosted glass | flat `surface` colour at 80–100 % opacity | no GPU; a 320×240 software blur per frame would cost > 30 ms |
| soft drop shadows | none, or a 1 px `line` hairline | LVGL shadows re-render large areas every frame |
| gradients | flat colours (LVGL can do simple 2-stop gradients if the design needs one; measure) | cost and banding at RGB565 |
| serif body text | Space Mono below 20 px | serif hairlines < 1 px at that size |
| exact Figma kerning | lv_font_conv kerning (fast format) | close but not identical |
| photographic thumbnails in the UI | not on device in v1 (5 MP JPEG decode is too slow); web page shows them | CPU |

## LVGL Pro / XML

Verdict for x4: **build hand-written LVGL now, structured so screens can later come from the LVGL
Editor (LVGL Pro) as compile-time C export into `core/src/ui_gen/`.**

- The Editor exports C that only depends on the MIT-licensed LVGL library; LVGL recommends the C
  export path. Runtime XML loading on the device requires a **paid** licence, and `lv_xml` was
  removed from the open-source tree as of 9.5 (VERIFIED: lvgl.io announcement "lv_xml removal from
  v9.5", pro.lvgl.io, read 2026-10-31).
- Licence: the Editor is free for personal / educational / open-source non-commercial projects
  (VERIFIED: lvgl.io/editor pricing table); the **CLI and CI integration appear in the paid tier**
  (*Not sure* for personal use). So: no terminal export in CI for now; the owner exports from the
  Editor GUI and commits `ui_gen/`.
- Steps when you do it: export the project into `core/src/ui_gen/`; keep
  `-DLV_LVGL_H_INCLUDE_SIMPLE` (set); call `editor_project_init("")` once after `lv_init()` in
  `display::begin()`; wrap the generated screens from `ui.cpp` so the app API stays the same.
- Figma → Editor: the LVGL Editor has a Figma plugin workflow; tokens should then come from the
  same Figma variables that feed `ui/tokens.json`.

## Smoke test (`hello-ui`)

`pio run -e hello-ui -t upload`: black screen, "Four Eyes" (serif 64), eased fade-in plus an 18 px
upward slide (520 ms), caption "tap to replay" 160 ms later; tap anywhere to replay (logs the FPS).
It also runs the power-safety boot path, because every Core firmware must keep the 5 V output off.
