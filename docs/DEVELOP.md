# Develop: build, test and run things on your computer

Everything here runs **without the boards**. Flashing and reading logs from the real hardware is
in [STEPS.md](STEPS.md) (step 1 walks through the first flash in VS Code and on the command
line), and log reading is in [LOGGING.md](LOGGING.md#reading-logs).

| what | what it proves | needs | how long |
|---|---|---|---|
| [firmware build](#1-build-the-firmware) | the code compiles for the real chips | PlatformIO | first time ~10 min (toolchain download), then ~1 min |
| [host tests](#2-host-tests) | shooting state machine, protocol, config limits, flash safety | PlatformIO | ~1 min |
| [UI simulator](#3-ui-simulator) | the real UI code draws what Figma shows; click through it | CMake, a C++ compiler, Pillow (SDL2 for the window) | first build ~2 min, then seconds |
| [generated files](#4-generated-files) | tokens, images, docs tables match their sources | Python, Pillow 13.0.0 | seconds |
| [docs site](#5-docs-site) | the manual builds | Node.js | ~1 min |

Every command runs from the repo root (`x4/`) unless it starts with `cd`.

## 0. Set up once

**GUI:** VS Code. Open the `x4` folder and accept the recommended extensions (PlatformIO IDE,
clangd, …). The repo ships VS Code **tasks** for most things on this page:
*Terminal → Run Task…* (or Cmd/Ctrl+Shift+P → *Tasks: Run Task*), then pick one starting with
`x4:`. Cmd/Ctrl+Shift+B runs the default one, *x4: build all (core)*.

**CLI:** the same things as commands. The PlatformIO IDE includes `pio`; its terminal button in
the status bar opens a shell where `pio` works.

Python packages, in a virtual environment so the pinned versions don't clash with anything else:
```bash
python3 -m venv .venv
```
```bash
source .venv/bin/activate
```
```bash
pip install platformio==6.2.0 pillow==13.0.0
```
(Windows: `.venv\Scripts\activate`.) Activate it again in each new terminal. Pillow is pinned
because image resampling changes between versions and CI checks the images byte for byte
([§4](#4-generated-files)).

For the UI simulator only, a C++ toolchain, CMake and (for the window) SDL2:

| system | install |
|---|---|
| macOS | `xcode-select --install`, then `brew install cmake ninja sdl2` |
| Ubuntu / Debian | `sudo apt install g++ cmake ninja-build git libsdl2-dev` |
| Windows | **Not sure**: not tested. WSL with Ubuntu and the line above is the likely route |

Tested on Ubuntu 24.04 (gcc 13, CMake 3.28, SDL2 2.30). macOS is expected to work (clang, Homebrew
SDL2 ships a CMake package) but has not been run yet: **Not sure**.

## 1. Build the firmware

Building = compiling without flashing. It's the quickest way to see whether a change breaks
anything for the real chips.

**GUI:** *Run Task… → x4: build all (core)* or *x4: build all (camera)*. Or open the `core/`
folder itself (*File → Open Folder…*), pick the environment in the status bar and click the
**✓ (Build)** button.

**CLI:**
```bash
cd core
```
```bash
pio run
```
That builds all three Core environments (`core`, `core-hello`, `hello-ui`). One only:
```bash
pio run -e core
```
Cameras: `cd camera && pio run` (`cam1`…`cam4`, `cam-hello`). Step firmware: `cd steps && pio run`
(15 environments, ~5 min).

**Pass:** each environment ends with `SUCCESS`. The first build downloads the pinned toolchain
([FIRMWARE](FIRMWARE.md#toolchain-and-pinned-versions)). The binary lands in
`core/.pio/build/<env>/firmware.bin`; you never need to touch it, because upload builds first.

`pio run` *builds* every environment, which is fine. `pio run -t upload` without `-e` would
*flash* every environment in turn: see the warning in [STEPS](STEPS.md#level-0-gear-up).

## 2. Host tests

32 tests of the shared logic (`shared/`), compiled for your computer, not the chip.

**GUI:** *Run Task… → x4: unit tests (native)*.

**CLI:**
```bash
cd core
```
```bash
pio test -e native
```
**Pass:** `32 test cases: 32 succeeded`. A failure names the test and the line.

## 3. UI simulator

Runs the **real UI code** (`core/src/ui/ui.cpp`, tokens, fonts, images) with LVGL 9.6 and the
device's own `lv_conf.h`, on your computer. Only the hardware glue is faked: clock, display,
touch, the status the app would feed it, and the app's `request()` / `setOption()`. Code:
`tools/ui_sim/`.

### Render every screen and compare with Figma

**GUI:** *Run Task… → x4: UI simulator: render + compare with Figma*.

**CLI:**
```bash
python tools/ui_sim/run.py
```
The first run downloads LVGL and compiles it (~2 min); after that it takes seconds. Then open
**`tools/ui_sim/out/index.html`** in a browser: every scene as *simulator · Figma · difference*.
The terminal prints the same as a table:

```
| scene | Figma | mean | changed | what / expected differences |
| `preview_photo` | 73:331 | 6.3 | 4.1% | viewfinder, footer default (SD free only) |
| `settings` tile t2 | 83:2132 | 6.5 | 1.9% | slider=0% |
```

- **mean**: average difference per colour channel, 0–255. Low single digits = the same picture
  give or take antialiasing.
- **changed**: share of pixels more than 24 levels off. Look at the diff image to see where.
- Big numbers on `preview_dithered` and `preview_portrait` are expected: Figma mocks those with
  other photos, the device processes the live one ([UI: compromises](UI.md#compromises-design--device)).
- Settings tiles are also compared one by one with their components (83:2147).

One scene, or all that start with a prefix:
```bash
python tools/ui_sim/run.py settings
```
Scene list (`--list`): viewfinder (photo, flash recharge, video, recording, dithered, portrait),
tap menu, settings (all tile states, plus one real tap on a tile), saving 1 and 3 of 4, errors
known and unknown, diagnostics. Scenes whose Figma frame uses another photo (portrait, errors)
get that photo, so they compare 1:1.

Some scenes check **behaviour**, and those do fail the run:
```
ok   a tap on the footer opens the menu (tap_footer vs preview_photo: brightness 95 vs 197)
ok   a tap on a faded side opens the menu (portrait_tap_side vs preview_portrait: brightness 59 vs 89)
ok   a new camera frame reaches the side copies (portrait_new_frame vs preview_portrait: mean pixel difference 43)
ok   a long swipe up sends settings away (settings_swipe_away vs preview_photo: mean pixel difference 0.0)
```
(10 in all: taps on the photo, the portrait side copies, the settings swipe (half, away, flick,
spring back), the cam delay drag, and no press effect on tiles.) In the live window the mouse
swipes too: press on settings, drag up, let go.

The run fails if the simulator doesn't build, a scene doesn't render, or a behaviour check fails.
Differences from Figma are for you to judge. CI runs the same and attaches `out/` as the **ui-sim** artifact, with the table in
the job summary.

### Click through it live

**GUI:** *Run Task… → x4: UI simulator: live window*. **CLI:**
```bash
python tools/ui_sim/run.py --live
```
A window opens with the viewfinder at the **real screen size**: 40.6 × 30.5 mm, like the
CoreS3-SE's 2.0" panel (320×240 at 200 pixels per inch: **VERIFIED** 2.0"/320×240 from M5's
CoreS3-SE spec; the mm assume the 2.0" is the visible diagonal). The mouse is your finger: tap the
photo for the menu, open settings, toggle the tiles, drag across the cam delay tile.

**Getting the size right.** Real size needs your monitor's points per inch. The simulator asks
SDL, which isn't reliable everywhere (**Not sure** about every Mac), and prints what it used:
```
window: 203 x 152 = real size, 40.6 x 30.5 mm at 127 points/inch (from SDL).
```
Hold a ruler (or the real device) against the window. If it isn't 40.6 mm wide, run once with
the corrected value, `--ppi <used × 40.6 / measured mm>`:
```bash
python tools/ui_sim/run.py --live --ppi 127
```
`run.py` remembers it (in `tools/ui_sim/build/live.json`), so plain `--live` uses it from then
on. If SDL can't tell at all, the window opens at 320×240 and says so. A fixed size instead of
real size: `--scale 2` (2 window pixels per screen pixel). You can also drag the window corner or
press `+` / `-`; `0` goes back to the start size.

Keys:

| key | does |
|---|---|
| space | shutter: white-out, Saving 1…4 of 4, "saved" toast (fake timings) |
| f | arm / disarm the flash (after an armed shot it recharges for 8 s: watch the footer) |
| e | cycle errors: SD full, no SD, too hot, unknown, none |
| c | cycle SD free: 820 MB, 12.3 GB, no SD |
| v / r | video mode / recording |
| p | preview on / off |
| m / d / esc | menu action / diagnostics / back |
| s | screenshot to `ui_sim_NN.bmp` in the current folder |
| + / - / 0 | bigger / smaller / back to the start size |
| q | quit |

The terminal shows the UI's log lines, in the same format as the device.

**What the simulator can't tell you:** speed on the ESP32 (the UI is far faster on a PC),
colours and text rendering on the real IPS panel, and how touch feels. Those still need the panel
([MEASUREMENTS: UI](MEASUREMENTS.md#ui)).

**If it fails:** `No module named 'PIL'` → the venv from [§0](#0-set-up-once) isn't active (or
wasn't made): `source .venv/bin/activate`, and the prompt starts with `(.venv)`. A plain
`pip3 install pillow` on macOS often stops with *externally-managed-environment*: that's why the
guide uses a venv. `cmake: command not found` or `SDL2 not found` → [§0](#0-set-up-once). The
window says *built without SDL2* → install SDL2, then delete `tools/ui_sim/build/` and run again.
LVGL download fails → it comes from GitHub (`lvgl/lvgl`, tag v9.6.0): check your network.

## 4. Generated files

Some files are made from others. Edit the source, run the generator, commit both. CI fails if
they are out of date.

| you changed | run | it writes |
|---|---|---|
| `ui/tokens.json` | `python tools/gen_tokens.py` | `core/src/ui/tokens.h` |
| `ui/figma/` (after `tools/figma_pull.py`) | `python tools/gen_figma_assets.py` | `ui/assets/figma/*.png` |
| `ui/assets/figma/*.png` | `python tools/gen_assets.py` | `core/src/ui/assets/*.c` |
| `config_schema.def`, `errors.def` | `python tools/gen_docs.py` | tables in `docs/CONFIG.md`, `docs/LOGGING.md` |
| `ui/fonts/*.ttf`, glyph range | `bash tools/gen_fonts.sh` (needs `npx lv_font_conv@1.5.2`) | `core/src/ui/fonts/*.c` |

Check them all (the same as CI):
```bash
python tools/gen_docs.py --check && python tools/gen_tokens.py --check && python tools/gen_figma_assets.py --check && python tools/gen_assets.py --check
```
**GUI:** *Run Task… → x4: check generated files*. Regenerating images: *x4: regenerate UI images*.

`gen_figma_assets.py` needs **Pillow 13.0.0** exactly; another version can produce slightly
different pixels and fail the check.

Pulling the design again needs a Figma token (read-only) in `FIGMA_TOKEN`:
```bash
python tools/figma_pull.py
```

## 5. Docs site

A landing page plus every `docs/*.md` (details: `site/README.md`):
```bash
cd site
```
```bash
npm ci
```
```bash
npm run dev
```
Then open the address it prints. `npm run build` is what CI runs.

## Before you push

Run what you touched (CI runs everything again):

| you touched | run |
|---|---|
| `core/`, `shared/` | `cd core && pio run && pio test -e native` |
| `camera/` | `cd camera && pio run` |
| `steps/` | `cd steps && pio run` |
| `core/src/ui/`, `ui/` | `python tools/ui_sim/run.py`, then look at `out/index.html` |
| tokens, images, `.def` files | the generated-files check above |
| `docs/` | `cd site && npm run build` |
