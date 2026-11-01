# Decisions (ADR-style)

Each entry: context → decision → consequences. Rejected options are listed with the reason.

## ADR-001 Arduino framework on PlatformIO in VS Code
**Context.** Owner is a designer with some C++, prefers VS Code. **Decision.** Arduino framework
(calling ESP-IDF APIs where useful: esp_timer, esp_http_server, sockets, heap caps) on PlatformIO.
**Rejected:** ESP-IDF as the toolchain (owner rejected; more boilerplate), MicroPython/UIFlow2
(M5 supports it, but camera support at 5 MP and the LVGL Pro workflow are unclear; slower).

## ADR-002 Keep the agreed repo layout, adapt where needed
**Decision.** Kept `core/ camera/ shared/ ui/ .vscode/ README.md .github/assets/cover.png`.
Changes and why:
- `core/src/main.cpp` was a placeholder (`setup(); loop();`) → replaced with real entry points.
- `core/platformio.ini`: was unpinned (`platform = espressif32`, `m5stack/M5Unified`, `lvgl/lvgl`);
  now pinned and with four envs (ADR-003, ADR-004).
- Added `core/src/hw/` (power safety, flash, sysinfo, SPI lock), `core/src/hello_ui/`,
  `core/test/`, `camera/boards/`, `tools/`, `docs/`, `app/` (stub) — none were in the old tree.
- `.gitkeep` files removed where real files now exist.
- `.vscode/extensions.json` extended (Red Hat XML, Error Lens), `settings.json` + `tasks.json` added.

## ADR-003 pioarduino (Arduino-ESP32 3.x) instead of stock `espressif32`
**Context.** The CamS3-5MP sensor needs esp32-camera's MEGA_CCM driver; M5 says the new revision
works with the latest core. Stock PlatformIO `espressif32` ships Arduino 2.x. **Decision.**
pioarduino `55.03.312-1` (Arduino 3.3.12, IDF 5.5.5) for both firmwares; all libs pinned by git
tag. **Consequences.** One toolchain; old-revision cameras (0x0200 = 0xFF) would need a separate
env on Arduino 3.1.0 + M5's patched library ([TROUBLESHOOTING](TROUBLESHOOTING.md#old-cams3-hardware)).

## ADR-004 Separate Stage 0 builds
**Decision.** `core-hello` (bring-up screen, no WiFi), `hello-ui` (LVGL smoke test), `cam-hello`
(home WiFi, capture bench) next to the full `core` and `cam1..cam4`. **Why.** The first flash
must be self-explanatory and work without the rest of the rig.

## ADR-005 `shared/` as a PlatformIO library (`symlink://../shared`)
**Context.** Brief said `-I../shared`. **Decision.** A real library with `include/` and `src/`
so shared `.cpp` files (log, config, protocol, FSM) compile into both firmwares and the native
tests. **Consequence.** Hardware-independent logic lives in `shared/`; `core/src/app` owns the
glue (the FSM itself is in `shared/src/shoot.cpp`).

## ADR-006 Core powered through Port A (BUS_OUT) with both 5 V enables forced off
**Context.** Owner wants Grove power. **Decision.** Port A red/black from the rail;
`cfg.output_power=false`; readback + enforcement at boot and every 2 s. Evidence and alternatives:
[POWER.md](POWER.md). **Rejected:** USB-C power for the Core (owner: avoid; also would need a
second breakout), M5 battery module as shared source (would split the rig into two power systems
and charge paths).

## ADR-007 Signals on the same split Grove cable (G1 button, G2 flash EN)
**Context.** The SE has only Port A. **Decision.** One split Grove cable: red/black power, white
G1 button, yellow G2 flash EN. Flash on yellow matches the Flashlight's own pinout (EN = pin 2 =
yellow), button on white matches the Unit Button (signal = pin 1 = white).
**Fallback.** G7/G8 on the M-Bus header if G2 turns out to idle high. ASSUMPTION until bring-up
step 3; [OPEN_QUESTIONS Q1](OPEN_QUESTIONS.md).

## ADR-008 Network: Core SoftAP, UDP trigger with scheduled capture time, HTTP pull
**Decision.** Core runs its own AP (fixed channel, 6 stations, power save off). Trigger = UDP
broadcast + per-camera unicast, ×3, carrying a **future Core timestamp** (`trig_lead_ms`);
cameras clock-sync on every heartbeat and capture at that time. Photos stay in camera PSRAM and
the **Core pulls** them (`GET /shot`) with a configurable parallelism.
**Why.** Scheduled capture removes network jitter from the skew; pulling keeps SD and WiFi
contention under the Core's control. **Rejected:** phone hotspot mode (needs a phone, adds a
router), BLE in v1 (too slow for photos; possible later for control), sequential HTTP triggers
(skew = sum of latencies), camera push uploads (no back-pressure), cameras saving to their own SD
(owner: Core's card only).

## ADR-009 Flash cooldown 8 s minimum (stricter than the brief's 1 s)
**Context.** Brief: cap ≈ 150 ms, cooldown ≥ 1 s. M5 recommends 8–15 s. **Decision.** Absolute
minimum 8 s, default 10 s. **Consequence.** Flash shots at most every 8 s; non-flash shots are
not limited. This tightens a safety rule, so it does not conflict with the brief.

## ADR-010 Logfmt logs, one module for both firmwares
**Decision.** `key=value` lines with t/lvl/dev/mod/evt, error codes from a catalogue, ESP-IDF
logs captured, camera WARN+ forwarded. **Rejected:** JSON lines (longer at 115200 baud, harder to
read live).

## ADR-011 Config schema as an X-macro `.def` file
**Decision.** One line per option with range, unit, scope, flags and doc; struct, validation,
JSON, NVS keys and docs/CONFIG.md are generated from it. **Rejected:** separate `config.h` +
hand-written docs (drift), JSON on SD (needs a card to boot).

## ADR-012 Hand-written LVGL now, LVGL Pro export later
**Decision.** UI v1 is hand-written and token-driven; `ui_gen/` reserved for Editor C export.
Runtime XML is paid and removed from open-source LVGL, so XML = compile-time export only.

## ADR-013 Figma not accessible → placeholder tokens (superseded by ADR-016)
**Context.** The Figma MCP returned "no access to this file" (connector on the work account).
**Decision.** Build on placeholder tokens; ask the owner. Superseded once access worked.

## ADR-018 Website: own Astro site, no template
**Decision.** Replace the notes. template with a small Astro site of our own (owner, 2026-11-01):
a landing page plus every doc. Type and colour are the device UI's (Instrument Serif, Space Mono,
ink on `#D1D1D1`, `#1E1E1E` dark); no uppercase labels, thin rules or decorative separators.
`docs/*.md` is read in place through an Astro content collection (no copy step); the page list in
`site/src/lib/docs.mjs` fails the build if a doc is missing from it. Supersedes ADR-017.
**Dropped with the template:** fuzzy search, keyboard browsing, theme switcher (the browser's
find and the system colour scheme cover them).

## ADR-017 Manual site from docs/ with the notes. template (superseded by ADR-018)
**Decision.** `site/` is the owner's notes. Astro template (MIT) minus its demo content and
framework islands. `docs/*.md` stays the only source: a sync script copies it into
`site/src/pages/` (gitignored) and a remark plugin rewrites links between docs and to repo files.
Mermaid renders client-side, lazily. **Rejected:** moving the docs into `site/src/pages/`
(breaks GitHub reading and every relative link), Starlight/Docusaurus (not the owner's style).

## ADR-016 UI v2 implemented from Figma 73:368
**Decision.** Replace the placeholder four-ring UI with the file's photo-first design: full-bleed
preview, footer, tap menu, 2×2 settings tiles, Saving/Error overlays. Figma raster overlays are
exported through the MCP (asset host blocked in the build sandbox, so via rendered screenshots)
and baked into RGB565+A8 images by `tools/gen_assets.py`. New config options back the tiles
(`ui_orient`, `ui_effect`, `cam_delay_ms`; a `ui_mode` minimal/full switch existed until 2026-11-01 and was dropped with the footer redesign); the camera delay needed a protocol field
(`dl=`, backwards compatible). Deviations are listed in UI.md "Compromises".

## ADR-014 No battery gauge
The SE has no battery and the bank's level is not readable; the UI shows none. An energy-counting
estimate (`~NN%` from a power model × uptime, reset by hand after charging) was built and then
removed on 2026-11-01: it could not know when the bank was charged, so it needed manual resets
and still drifted. The bank's own indicator is the truth.

## ADR-015 Live preview by polling QVGA JPEGs
**Decision.** The Core polls `/capture?res=0` from one camera and decodes in software.
**Rejected:** MJPEG stream parsing on the Core (more code, same decode cost), 5 MP decode at 1/8
(too slow to transfer and decode). Fallbacks in [UI.md](UI.md#live-preview-stage-4).
