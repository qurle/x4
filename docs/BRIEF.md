# Original brief (verbatim)

> Kept unchanged so no information from the kickoff prompt is ever lost. Everything else in docs/ is the structured, researched version of it. Where research contradicted the brief, the docs say so and DECISIONS.md / OPEN_QUESTIONS.md record it.

# x4 — kickoff prompt for Claude Code (v2, with the owner's answers)

Paste everything below this line into Claude Code, started in the root of `github.com/qurle/x4`.

---

You are joining a hardware + firmware + design project. The owner (Nick, a product/UX designer who knows some C++ and prefers VS Code) wants a **flashable base project** for a portable 4-camera rig, plus thorough docs, so he can burn firmware the moment the M5Stack boards arrive (or with a few patches).

## 0. How to work

1. **Inspect the repo first.** It already has `.vscode/`, `camera/`, `core/`, `shared/`, `ui/`, `README.md`, `.github/assets/cover.png`. Keep what is there unless it is wrong; explain every structural change in `docs/DECISIONS.md`.
2. **Plan first, then build.** Write a short plan. The owner already answered the main questions (section 12), so don't re-ask them. For anything still open, pick a sensible default, mark it `ASSUMPTION`, list it in `docs/OPEN_QUESTIONS.md`, and continue. Ask the owner only when a wrong guess could damage hardware or force a rewrite, and ask everything in one batch.
3. **Never invent facts about hardware.** Pin numbers, I2C addresses, voltages, current limits and library behaviour must come from official docs, schematics, source code or GitHub issues. Cite the source (link + date/version). Tag every hardware claim `VERIFIED` (with source) or `ASSUMPTION`. If something is unknown, write "not sure".
4. **Work in stages** (section 11), commit after each stage with a clear message, keep the tree compiling.
5. **The owner has no M5Stack boards yet.** Everything must at least compile (`pio run`) for every environment, and hardware-independent logic needs native unit tests (`pio test -e native`). Add a GitHub Actions workflow that builds every environment.
6. **Safety first.** The owner's third priority is "it shouldn't blow up or burn my components". Section 3 has hard rules. If anything in this prompt conflicts with them, stop and tell the owner.
7. **Logs and configs are first-class** (section 5.7). The owner wants clean structured logs and documented configs everywhere.
8. Reply concisely; put the detail in `docs/`.

## 1. Project in one paragraph

A portable, battery-powered rig with **4 independent camera modules** that shoot **simultaneously** when the user presses a button (acceptable skew: **50–250 ms**; tighter is a bonus). One camera also streams a **live preview**. A **Core controller with a touchscreen** runs the **whole UI on-board** and orchestrates cameras, flash and storage. A **companion app** is optional and later; it mostly does **post-processing**. Speed and cheapness over absolute image quality for v1, but **each shot is full 5 MP** and **all four photos must be safely stored within about 5 seconds** of the shutter (the owner can wait 5 s). Everything works **on-site, offline, portable** (no router, no internet, no phone needed to shoot).

**Owner's priorities, in order of emphasis:**
1. **Design and interaction delight** (design-heavy project, section 7).
2. **Performance** (UI smoothness, shutter latency, sync, transfer time).
3. **It must not blow up or burn components** (power and wiring safety, thermal limits).

## 2. Hardware

**Not yet bought (boards arrive later):**
- 1x **M5Stack CoreS3-SE** (ESP32-S3, 320x240 capacitive touch, no battery in the SE variant, M-Bus, Grove ports, microSD slot, speaker)
- 4x **M5Stack Unit CamS3-5MP** (module: ESP32-S3-WROOM-1-N16R8 = 16 MB flash + 8 MB octal PSRAM; sensor: "OV5640 derivative"). **NOTE:** earlier planning notes said "PY260", so the **real sensor must be verified** from M5 docs/schematic and at runtime by reading the sensor ID; this decides the driver and the available resolutions/formats.
- 1x **M5Stack Flashlight Unit** (Grove HY2.0-4P: red=5 V, yellow=light-control signal, white=NC; owner-confirmed peak current ≈ **1 A for ≈ 100 ms**). The owner will use the unit's **flash (spike) mode**: verify from the docs/source exactly how that mode is triggered (pulse width? level? command?), its maximum on-time and any cooldown, and what the signal pin expects (3.3 V logic).

**Already on hand:**
- 1x **M5Stack Mini Button Unit** (no LED; verify its pinout and active level)
- 1x **Anker Nano 5000 mAh power bank** (the single power source; look up its exact output ratings and low-current auto-shutoff behaviour)
- 1x plain **power switch**
- **2200 µF capacitors** (low-ESR electrolytic expected; check voltage rating ≥ 10 V)
- **5x USB-C breakout boards with inline capacitors**. **Only ONE is used** (bank input). The owner avoids using more until explicitly necessary. The other four stay spare.
- **Wago 2273-208** (8-port push-in). The owner has **several**, so using **two per net** is fine and there are more in stock.
- **20 AWG stranded** wire (bunch)
- **Dupont** cables of various kinds
- **In-box Grove cables** for the M5Stack products
- **Smartbuy microSDHC Class 10, 16 GB** (goes in the **Core's** slot; the single storage for all photos)

**Power model: a single power source. The Core and the cameras are Grove-powered.** The Core is fed through a **Grove port 5 V pin**; **USB-C power to the Core is avoided** unless explicitly necessary (see 3.2 and the flashing rule in 3.6).

**Physical controls:** the Mini Button Unit (shutter), the touchscreen, and the power switch. The owner **may later replace the button with a nicer one or add a scroll wheel/encoder driver and extra feature buttons**, so the input layer must be designed for that (5.5).

## 3. Power and safety (hard rules)

### 3.1 Architecture (target)

```
Anker Nano 5000 -> USB-C breakout (the ONE in use) -> power switch (+5V line only)
   -> +5V rail (Wago 2273-208, two blocks jumpered) --+--> Cam1..Cam4 (Grove red)
   -> GND rail (Wago 2273-208, two blocks jumpered) --+--> Core (Grove cable to a Port 5V/GND pins, see 3.2)
                                                      +--> Flashlight (red/black via 20 AWG, from the rail)
                              2200 uF across +5V/GND near the camera branches
```
- **Star topology**, never daisy-chained Grove power. Each load has its own branch from the rail.
- Capacitor in **parallel** across +5V and GND (stripe = minus = GND). **Never in series.**
- Flashlight power comes **straight from the rail** (20 AWG), not through the Core's Grove port (1 A peaks); only its yellow signal wire goes to a Core GPIO.
- Cam Grove cables: red = 5 V, black = GND. Yellow/white are the camera's **USB D−/D+** lines; **never** connect them to the rail, cap/tape their ends.
- Net count: +5V needs source, 4 cams, Core, flash, cap = **8 connections**, same for GND, which fills one 2273-208 completely. The owner has more blocks and OKs two per net: jumper two blocks per net so there are spare ports; document it.
- Thin Grove wire may not grip in a push-in port: fold the stripped end back on itself or use a wire ferrule; tug-test every connection. Wire gauge is not the limit, connection quality is.

### 3.2 The Core is powered through a Grove port — confirm the exact conditions

On CoreS3 the Grove 5 V pin is normally an **output** from the Core's own 5 V domain. Feeding it from outside while the Core could also drive 5 V (AXP2101 `BUS_OUT_EN` boost, or USB OTG) puts **two 5 V sources in contention** and can damage the Core or the bank.

**Evidence the owner attached (Appendix C, image 1):** an AXP2101 power-path diagram with four modes. In the top-left mode, **`BUS_OUT_EN=0` and `USB_OTG_EN=0`**, the BUS/Port A/B/C 5 V node is drawn as an **input** into the AXP2101 (red arrow into the chip), alongside the USB-C input. That suggests feeding the Core through the 5 V bus/Grove pin is an intended mode **as long as both enables stay 0**. In the `BUS_OUT_EN=1` modes the AXP2101 drives the bus, which is the contention case. Only the picture is available: find the official page/schematic it comes from and confirm:
(a) Grove 5 V and M-Bus 5 V are the same node on the SE;
(b) the current limit of that input path;
(c) what happens with USB-C plugged in at the same time (relevant to flashing, 3.6);
(d) the **default `BUS_OUT_EN`/`USB_OTG_EN` state at power-up before firmware runs** (the Core might drive 5 V briefly at boot; this is the key risk to verify);
(e) which Grove port to use for the power lead (the owner said "port pin" without naming it). Choose per docs, avoid conflicts with the button/flash signal pins, and record it.

Write the result in `docs/POWER.md` with sources. If back-feeding via Grove is **not** safe, propose the nearest safe alternative (M-Bus 5 V input pin, or USB-C input through a spare breakout) and **ask the owner before changing the plan**.
Firmware must **never** enable the Core's 5 V output or OTG (`M5.config()` output/ext-power options off; verify exact names) and should assert the state at boot and log it.

### 3.3 Power numbers (all unmeasured guesses; replace with measurements)

- Camera: idle/standby ≈ 0.75 W, streaming ≈ 1 W, burst ≈ 1.5 W (peaks ≈ 0.3–0.4 A at 5 V per camera).
- Whole rig: standby ≈ 3–4 W, burst ≈ 7–10 W, plus the flash up to **1 A for ≈ 100 ms**.
- Runtime ≈ bank Wh × 0.8 / load W; bank Wh = mAh × 3.7 / 1000 (5000 mAh ≈ 18.5 Wh).
- Create `docs/MEASUREMENTS.md` with empty tables for: per-device idle/stream/burst current, rig total, flash peak, inrush at switch-on, bank shutoff behaviour, temperatures, transfer throughput, SD write speed. The owner will fill them with a USB power meter and multimeter.

### 3.4 Known risks to document and mitigate

- **Inrush at switch-on**: 2200 µF + inline capacitors + 6 loads starting at once can trip the bank's overcurrent protection or shut it off. Evaluate, test, propose mitigation (NTC inrush limiter, smaller cap, soft-start) if needed.
- **Power switch rating**: in the +5V line only, rated well above ~3 A at 5 V DC.
- **Bank auto-off**: many banks cut output below ~50–100 mA; standby draw should exceed that, verify on the real unit.
- **Flash current peak** dipping the rail and browning out cameras/Core: the capacitor is there for this; run a flash-fire stress test.
- **USB-C breakout**: some have CC pull-down resistors (5.1 kΩ on CC1 and CC2 to GND), some don't. Without them a C-to-C cable may deliver no power from the bank. Check via the listing, resistors marked "512", ~5.1 kΩ with a multimeter, or measure output voltage. Using the bank's USB-A port + A-to-C cable avoids CC negotiation.
- **Wrong polarity / shorts**: continuity test +5V vs GND (must not beep) before first power-up.
- **Unknown on the owner's breakouts:** whether the inline capacitors are on the output side and their values/voltage rating. Treat as unknown; check by inspection/measurement and record in `docs/HARDWARE.md`.

### 3.5 Firmware safety rules (implement and test)

- Flash GPIO configured **LOW (off)** as the very first thing in `setup()`, with a pull-down if the pin allows. Hard cap on pulse length and a minimum cooldown (constants in `shared/`; default cap ≈ 150 ms, cooldown ≥ 1 s; configurable only below an absolute max). Use a shutdown hook/watchdog so a crash or hang can't leave the flash on.
- Never enable the Core's 5 V output / OTG (3.2).
- Log reset reason at every boot (`esp_reset_reason`), brown-out detector on, boot counter in NVS, show brown-outs on screen and serial.
- Thermal: read chip temperature (`temperatureRead()` on S3 or the IDF sensor); throttle streaming / refuse long bursts above a threshold; show a warning.
- Cameras: all four power up together at switch-on; avoid heavy init spikes (stagger camera init by a per-camera delay derived from its ID).
- Core SE has no battery: **power-bank level is not readable**; don't fake a battery gauge. An estimated "session time" is fine if labelled as such.

### 3.6 Flashing without breaking the power rules

Boards must be flashed over USB-C at least once, which conflicts with "no USB power". Write a clear procedure in `docs/BRINGUP.md` and warn on the first page of `docs/WIRING.md`:
- **Switch the rail off and unplug the Grove power lead from the Core (or camera) before connecting USB-C**, so only one 5 V source is ever present. Reconnect the rail only after unplugging USB.
- Same for each CamS3 (verify how it enters download mode and whether the Grove power and USB can be present together).
- Check whether the Core / cameras are safe with USB and rail connected simultaneously; if the docs/schematic prove it is, say so, otherwise keep the rule above.
- Add **OTA updates over the Core's AP** (Core and cameras) as a later stage to cut USB-C use; the first flash is always USB.

## 4. Signal wiring

- **Mini Button Unit → a Core Grove port** (choose per docs; verify GPIO numbers and active level). Internal pull-up, debounced in firmware.
- **Flashlight yellow → one Core GPIO** (verify it is free and 3.3 V-safe).
- **Cameras talk to the Core only over WiFi** (UDP/HTTP). No I2C hub — none of these parts use I2C.
- Reserve a **spare-port plan** for future inputs (extra buttons, scroll wheel/encoder): document which Grove ports and GPIOs are used and which are free, including what an I2C encoder unit would require.
- Future low-latency option (document, do not build): a hardwired trigger line from the Core to each camera's Grove yellow pin. Caveat: on the CamS3 those GPIOs double as USB D−/D+ (G19/G20), a risk for flashing/debug.
- `docs/WIRING.md`: wiring diagram (ASCII or Mermaid), a pin table for every connection, colour conventions (red/black/yellow), and a printable pre-power checklist.

## 5. Software architecture

### 5.1 Toolchain
- **Arduino framework** on **PlatformIO inside VS Code**. ESP-IDF as a direct toolchain is **rejected** by the owner (calling IDF APIs from Arduino code is fine).
- Research whether to use stock `espressif32` or **pioarduino** (Arduino-ESP32 3.x) based on compatibility of M5Unified, M5GFX, LVGL 9 and `esp32-camera`; **pin all versions** in `platformio.ini`; document why.
- Core board: `m5stack-cores3` (verify). CamS3-5MP: find the right PlatformIO board/settings (16 MB flash, **OPI PSRAM**, partition table, USB CDC on boot, upload mode); check M5's docs for the exact Arduino board settings.
- `.vscode/`: recommend PlatformIO IDE, clangd (disable Microsoft IntelliSense if using it), Red Hat XML, Error Lens; tasks for build/upload/monitor per env.

### 5.2 Repo layout (agreed earlier; adapt, don't break)

```
x4/
├─ core/            PlatformIO project for CoreS3-SE
│  ├─ platformio.ini, include/lv_conf.h
│  └─ src/  main.cpp (setup/loop only) | display/ (flush+touch glue) | input/ (button, touch gestures, future encoder)
│            net/ (AP, camera client, trigger, transfer) | app/ (state machine) | storage/ (SD) | log/ | ui/ | ui_gen/ (generated, never hand-edited)
├─ camera/          PlatformIO project for CamS3-5MP (camera server)
├─ shared/          protocol.h, config schema, safety limits, log format constants
├─ ui/              design source of truth: tokens, fonts, assets, (optional) LVGL Pro/XML project
├─ app/             companion app (later; stub only)
├─ docs/
└─ .github/workflows/
```
Rules: UI code never makes HTTP calls; it emits events to `app/`, and `app/` talks to `net/`. Regenerating UI code only overwrites `ui_gen/`. Core and camera projects both include `shared/` (`-I../shared`).

### 5.3 Networking (v1 decisions)
- The **Core runs its own WiFi SoftAP** (no router, no phone hotspot). Cameras join as STA clients. **BLE is not in v1** (document as a possible later addition).
- **ESP32 SoftAP defaults to max 4 clients**: configure ≥ 6 (4 cams + phone + spare) and verify the limit.
- Fixed Core IP (default `192.168.4.1`); cameras self-register (`POST /register`: id, IP, firmware version, sensor ID, free heap/PSRAM) and send heartbeats; the Core tracks online/offline and shows it on screen.
- Cameras: `WiFi.setSleep(false)` (power-save causes 100+ ms random latency), fixed AP channel.
- Fan-out trigger: prefer a **UDP broadcast** "shoot" packet (lowest latency), HTTP as fallback. Never trigger cameras sequentially.
- **Capture instantly, transfer later**: on trigger each camera grabs a full 5 MP JPEG into PSRAM immediately; the upload to the Core runs afterwards through a queue with progress shown in the UI. Pause the live preview during bursts.
- **Transfer budget (the main performance risk):** target is all 4 full-5MP photos **stored on the Core's SD within ~5 s** of the shutter. Rough guess, to be measured: 4 JPEGs of 1–2.5 MB = 4–10 MB, so 1–2 MB/s aggregate through one ESP32 SoftAP plus SD writes. Investigate and document: JPEG quality vs file size, staggered vs parallel uploads, TCP vs UDP-based transfer, SoftAP throughput with 4 simultaneous clients, SD write speed. Provide fallbacks in order: tune JPEG quality to a target size, stagger uploads, show per-camera progress and a "shot captured" confirmation as soon as the frame sits in camera PSRAM (so the UI feels instant even if storing takes a few seconds), block the next shot until buffers are free. Report measured numbers in `docs/MEASUREMENTS.md`.
- Camera HTTP API (baseline from earlier work): `/capture` and `/settings` on port 80, MJPEG `/stream` on port 81, CORS enabled. Extend with `/info`, `/register`, `/health`. Define everything in `docs/PROTOCOL.md`.
- **Storage:** photos land on the Core's **microSD only** (cameras do not use their own cards). Verify whether the CoreS3 SD slot shares an SPI bus with the display and what that means for write speed and UI smoothness (schedule SD writes so they don't stall UI frames). Folder per shot (timestamp or counter + camera index), JSON sidecar with settings and timings. FAT32; readable on a PC.
- **Companion access (later):** the Core serves the photo list and files over HTTP on its AP. API stays app-agnostic. A Flutter app (**Android and iOS both required**) joins the AP programmatically (WifiNetworkSpecifier on Android 10+, NEHotspotConfiguration on iOS); note cleartext-HTTP exceptions on both platforms and the iOS entitlement.
- **Web companion first, for debugging:** a **lightweight web debug/viewer page served by the Core itself over HTTP** (same origin as the API, so no mixed-content or CORS trouble; a hosted HTTPS page cannot call plain-HTTP devices). It should show device status, logs, config, latest photos and a manual trigger. Flutter comes afterwards for the "native" experience.
- Post-processing: earlier planning produced on-device ordered (Bayer) dithering (1-bit and 16-colour BMP output); keep it as an optional capability and a candidate for the companion app.

### 5.4 Camera firmware
- Arduino camera server on ESP32-S3 + OPI PSRAM; frame buffers in PSRAM; JPEG mode; **full 5 MP** as the target, other sizes configurable.
- **First thing at boot: log chip, PSRAM size, flash size and the sensor ID**; refuse to continue with a clear error if sensor/driver mismatch. Check there is a **working full-5MP example** for this exact module before relying on full resolution (the owner's biggest purchase risk). Document capture latency, JPEG size and transfer time per resolution.
- Stable per-camera identity (1–4) from the MAC address or a config constant (see 5.7). WiFi credentials in a gitignored `secrets.h` with a committed `secrets.h.example`.

### 5.5 Core firmware
- M5Unified + M5GFX for hardware, **LVGL 9.x** for UI, partial render buffers in fast RAM, `lv_timer_handler()` in the main loop with no blocking calls; networking and SD in separate FreeRTOS tasks communicating via queues/events.
- Reuse the display/touch glue pattern from earlier planning: LVGL flush writes via M5GFX (`setAddrWindow` + `writePixels`), touch via `M5.Touch.getDetail()` into an LVGL pointer indev, `lv_tick_set_cb(millis)`. (Never tested on real hardware: treat as unverified.)
- `lv_conf.h`: `LV_COLOR_DEPTH 16` and the fonts the design uses; build flags `-DLV_CONF_INCLUDE_SIMPLE`, `-DLV_LVGL_H_INCLUDE_SIMPLE`.
- **Input abstraction:** physical sources (Mini Button, touch gestures, future extra buttons, future scroll wheel/encoder) produce **logical actions** (`SHUTTER`, `NEXT`, `PREV`, `SELECT`, `BACK`, `SCROLL(±n)`, ...); a documented mapping table (in config) binds sources to actions, so swapping the button or adding a wheel needs no UI changes. Provide a stub driver for an encoder/scroll unit and show how to add one.
- State machine for the shooting flow (idle / armed / shooting / transferring / preview / error), error states (camera offline, SD full/missing, brown-out, overheating), persistent settings store (NVS).
- Performance targets: smooth animations (aim for 30–60 fps, measure and report actual), low touch-to-feedback latency, shutter press → all cameras triggered within the skew budget. Built-in **diagnostics screen**: FPS, heap/PSRAM, per-camera RTT and skew, transfer progress, reset reason, temperature, SD speed, log tail.

### 5.6 LVGL Pro / XML
- The owner's plan is LVGL + LVGL Pro editor with the Figma workflow. You can't drive the Editor GUI, so build the UI first as **hand-written LVGL C++ driven by design tokens** extracted from Figma, structured so the same screens can later be authored in XML/LVGL Pro and exported into `ui_gen/`. Evaluate whether the LVGL CLI/XML export can run from the terminal and document the verdict plus the Arduino/PlatformIO steps (exported project under `src/`, `LV_LVGL_H_INCLUDE_SIMPLE` build flag, `editor_project_init("")` after `lv_init()`). The runtime XML loader isn't public, so XML means compile-time export. Check the LVGL Pro licence terms (personal-use only on the free tier) and record them.

### 5.7 Logging and configuration (owner requirement: clean structured logs, documented configs)

**Logging** (one module in `shared/` used by both firmwares; no stray `Serial.print` anywhere):
- Levels (`ERROR/WARN/INFO/DEBUG/TRACE`), compile-time default plus runtime override via config/HTTP.
- Consistent, machine-parsable lines, e.g. `t=12345 lvl=INFO mod=net cam=2 evt=register ip=192.168.4.12 fw=0.1.0` (key=value or JSON lines; pick one, document it, keep it identical on Core and cameras). Every line carries uptime ms, level, module, and device id; events use stable names (`evt=`), not free text only. Errors include a code from a documented table.
- Boot banner with firmware version, git hash, build time, chip info, PSRAM/flash size, reset reason, boot count, config summary, and the power-safety state (`BUS_OUT_EN`/OTG asserted off).
- Sinks: serial, an in-RAM ring buffer (shown on the diagnostics screen and served at `/logs`), and optional rotating log files on the Core's SD (size-capped, off by default for perf). Cameras forward WARN+ to the Core.
- Logging must not hurt performance (no blocking in hot paths, no logging inside LVGL flush/ISR; rate-limit repeated messages).
- `docs/LOGGING.md`: format spec, level policy, module/event/error-code catalogue, how to filter and parse logs (include a small script that turns serial output into a table), how to read them while debugging.

**Configuration:**
- A single, **fully documented config system**: compile-time defaults in committed headers (`config.h`-style with `config.h.example`, or a schema file), runtime-adjustable settings in NVS, and secrets only in gitignored `secrets.h`.
- Every option is documented where it is defined (name, meaning, unit, default, allowed range, whether it is safety-limited, whether it needs a reboot) and listed in `docs/CONFIG.md` (generate this doc from the schema if practical, so it can't drift).
- Validate config at boot: clamp unsafe values, log a WARN with the option name and the applied value; safety limits (flash cap, cooldown, SoftAP clients, thermal thresholds) can only be tightened, not loosened, at runtime.
- Config is versioned; migrations are logged; the active config summary is printed at boot and served at `/config`.

## 6. Companion app

Not on the critical path. For v1 produce: the API contract, the SD folder/file format, and `docs/COMPANION.md` covering the plan **web debug page first (served by the Core), Flutter later (Android + iOS)** with the platform constraints in 5.3. Don't start Flutter until the owner asks. The web debug page is part of Stage 2/3 (below).

## 7. Design (important)

Mockups (final, exact node): https://www.figma.com/design/fgEvP6Ms0bOAbWxfkFki4R/x4?node-id=73-368&t=ZZea3M8aQB3PEhvZ-11 (fileKey `fgEvP6Ms0bOAbWxfkFki4R`, node `73:368`). It is a **section containing a couple of frame sets**; the owner named nodes so they explain themselves. Treat this section as final and ignore everything else in the file unless told otherwise. If a Figma MCP/Dev Mode connection is available, use it (design context, screenshots, variables, metadata); otherwise ask the owner for exports. Then:
- Extract **design tokens** (colours, typography, spacing, radii, materials, motion durations/easing) into `ui/tokens.*` and generate LVGL styles from them.
- **Fonts: Instrument Serif and Space Mono** (the owner confirms both are licensed). I believe both are open-source (SIL OFL) on Google Fonts, but verify and record the licence. Convert them with `lv_font_conv` into LVGL fonts, include only needed glyphs (Latin + Cyrillic if the UI needs it; ask), choose bpp (4 bpp anti-aliased is the usual), and check legibility of the serif at small sizes on 320x240. Watch flash/RAM cost.
- List every **screen and state** (including empty/error/loading) and every **transition** in `docs/UI.md`, mapped to LVGL objects and animations.
- **Pick up the design language and keep it.** Where a mockup can't be reproduced on a 320x240 LVGL display (blur, shadows, fonts, gradients, performance), choose the closest compromise, record it in `docs/UI.md` with a before/after note, and tell the owner; never degrade silently.
- Interaction delight is a requirement: easing (LVGL ease-in/out, overshoot, bounce), staggered reveals, press feedback, a satisfying shutter moment (optional sound through the Core's speaker; there is no haptics hardware, verify), polished empty/error states. Keep every animation cheap and measure FPS after each addition.
- Smoke-test screen to keep as a `hello-ui` env: black screen, "x4cam" label, eased fade-in plus a short upward slide, tap to replay.

## 8. Research (early; summarise in `docs/RESEARCH.md`)

Check M5Stack's GitHub (issues + discussions), M5 docs, M5 community/forum, Espressif `esp32-camera` and `arduino-esp32` issues, LVGL forum, relevant blogs. For each finding record link, date, versions, what it means for x4, `VERIFIED`/`ASSUMPTION`. Topics:
1. CoreS3 / CoreS3-SE: Grove back-feeding, AXP2101 + AW9523B power control, `BUS_OUT_EN`/OTG defaults at power-up, USB-C vs rail conflicts, pin maps, free GPIOs, SD slot SPI sharing with the display, known M5Unified/M5GFX init-order issues.
2. Unit CamS3-5MP: actual sensor, driver support, full 5 MP capture, PSRAM config, pinout, Grove/USB pin sharing, download-mode procedure, brown-outs under WiFi+camera, frame buffer / `fb_count` / grab-mode issues, known heating problems.
3. Arduino-ESP32 2.x vs 3.x compatibility with M5Unified, M5GFX, LVGL 9, `esp32-camera`; PlatformIO vs pioarduino.
4. ESP32-S3 SoftAP: max clients, throughput with several clients, channel choice, power-save latency; 5 MP JPEG sizes and upload time.
5. LVGL 9 on ESP32-S3: buffer placement (internal RAM vs PSRAM), DMA flush, achievable frame rates on 320x240, JPEG decoding cost for a live-preview widget (S3 has no hardware JPEG decoder).
6. Flashlight Unit (flash/spike mode control, current, limits), Mini Button Unit, and options for a scroll wheel/encoder unit and extra buttons.
7. Power bank behaviour (Anker Nano 5000: low-current cut-off), switch-on inrush with large capacitors, USB-C CC handling.
8. Anything else that commonly "blows up or burns" in similar M5Stack camera/flash rigs.

## 9. Docs folder (`docs/`) — required files

Reformat freely, but **do not drop any information from this prompt**.
- `README.md` index; repo-root `README.md` gets a short overview, quick start, links.
- `HARDWARE.md` (BOM, exact variants, links, pin tables, confirmed vs assumed), `WIRING.md`, `POWER.md`, `MEASUREMENTS.md`.
- `FIRMWARE.md` (architecture, tasks, state machine, memory map, build flags, pinned versions), `PROTOCOL.md` (API, UDP trigger, registration, errors, versioning).
- `LOGGING.md` and `CONFIG.md` (5.7).
- `UI.md` (design language, tokens, fonts, screens/states/transitions, compromises).
- `BRINGUP.md` (first flash and test ladder, section 10; includes the flashing procedure from 3.6).
- `TROUBLESHOOTING.md`: symptom → likely cause → check → fix for power (brown-outs, resets, bank shut-off, hot parts), flashing (download mode, USB, "both power sources" mistake), camera (no frame, wrong sensor, black/green frames, slow capture), WiFi (can't join, dropouts, AP full, latency), SD (mount fails, slow), transfer (misses the 5 s target), UI (tearing, low FPS, touch offsets, font rendering), flash unit, and the mixed-content/HTTP trap.
- `DEBUGGING.md`: serial logging conventions, how to measure shutter skew (e.g. flash/LED + phone slow-motion video, or timestamped trigger ACKs), multimeter and USB meter use, measuring current with the flash firing, diagnostics screen, web debug page.
- `RESEARCH.md`, `DECISIONS.md` (ADR-style, including rejected options: ESP-IDF, MicroPython, phone hotspot mode, BLE in v1, M5 battery module as shared source), `OPEN_QUESTIONS.md`, `COMPANION.md`.
- `ENCLOSURE.md`: v1 foam board (3–5 mm) skeleton + cardboard skin, low-temp hot glue + double-sided foam tape/hook-and-loop for modular parts, no superglue/solvent glue near foam or lenses; v2 3D-printed or laser-cut translucent acrylic (Y2K/old-Mac look); shield the flash and lenses with matte black opaque sleeves (clear plastic carries stray light); mount cameras on a rigid inner frame; vents (sealed shells trap heat); test in a dark room with flash; brass heat-set inserts + M2/M2.5 screws for structural parts; check M5's GitHub structure files for screw-hole positions; leave room for future buttons/scroll wheel and access to the charging port. The owner's renders (Appendix C) are a **rough sketch of intent, not a source of truth**.

## 10. First-flash bring-up (write as `docs/BRINGUP.md`)

Test ladder, each step with an expected result and an "if it fails" link:
1. **No boards connected:** continuity +5V↔GND (must not beep), polarity, USB-C breakout/CC check, 5.0 V at the rail with a multimeter, rail voltage with the switch on and nothing attached.
2. **Each board alone on USB** (rail off, Grove power lead unplugged; see 3.6): flash `hello` firmware, read serial.
3. **Verify the Core's Grove/bus 5 V pins read ~0 V** with the Core running and nothing attached (so its 5 V output is off) before it ever sees the rail.
4. **Power up one load at a time** from the rail, watching current with a USB power meter: Core → +1 camera → … → 4 cameras → flash.
5. **Flash-fire stress test** (short pulses at the hard cap): rail dip, resets, connector warmth (warm is fine, hot is not).
6. **WiFi:** cameras join the Core's AP, register, heartbeat.
7. **Simultaneous shot:** measure skew, per-camera capture time, transfer times, SD write speed, and total time to "all 4 stored" against the 5 s target.
8. **Soak test:** long streaming + bursts, record temperatures, brown-outs, bank behaviour.

## 11. Stages (commit after each)

- **Stage 0 — flashable hello-world for each board**, buildable now: the **logging module and config system** (5.7) first; Core prints chip/PSRAM/flash info, shows a test screen, tests touch, SD mount + write speed, Mini Button read, the guarded flash test (off by default, on-screen confirm, 50 ms pulse), power/reset diagnostics including the 3.2 power-state assertion. Camera firmware prints chip/PSRAM/sensor ID, captures JPEGs at several sizes **including full 5 MP** with timings, serves `/info` and `/capture` after joining a configured WiFi. Add `secrets.h.example`/`config.h.example`, CI build, `pio` tasks, and `docs/BRINGUP.md`. This is the stage the owner flashes first: make it robust and self-explanatory.
- **Stage 1 — network skeleton:** Core SoftAP, camera registration/heartbeat, UDP trigger, parallel capture, status on screen.
- **Stage 2 — shooting flow:** state machine, input abstraction, flash timing/safety, capture-then-transfer queue, SD storage layout, error handling, **web debug page served by the Core** (status, logs, config, photos, manual trigger).
- **Stage 3 — UI v1** from the Figma node's tokens and screens, fonts, animations, performance measurements, diagnostics screen.
- **Stage 4 — live preview** from one camera, measured, with a documented fallback if the Core can't keep up.
- **Stage 5 — polish & docs:** troubleshooting, measurement templates filled with whatever is known, API docs, companion-app contract, OTA updates over the AP.

For each stage report: what was built, what was verified (build/test vs needs hardware), what still needs the real boards, anything risky.

## 12. Owner's answers (already settled — do not re-ask)

1. **USB-C breakouts:** only **one** is used (power-bank input). The owner avoids using more until explicitly necessary.
2. **Core power:** fed through a **Grove port 5 V pin**; **avoid USB-C power** until explicitly necessary (flashing excepted, see 3.6).
3. **Wago 2273-208:** two per net is OK; the owner has more.
4. **Storage:** the **Core's single microSD card** only.
5. **Flash unit:** powered **from the rail**; it will use the unit's **flash (spike) mode**.
6. **Figma:** the node in the link in section 7 is the exact, final target (a section with a couple of frame sets). **Fonts: Instrument Serif and Space Mono**, all licensed.
7. **Controls:** button, touchscreen, power switch. The button has **no LED**. The owner **may replace it with a cooler button or even a scroll-wheel driver later**; keep room for more buttons assigned to features (not mandatory, but scalable).
8. **Resolution:** full **5 MP**. Acceptable wait after a shot: **5 s**.
9. **Companion app:** not decided; **web first for debugging, then Flutter** for a native feel; **both Android and iOS**.
10. **Renders are NOT a source of truth.** The owner currently has **Wago 2273-208**.
11. Extra requirement: **clean structured logs and documented configs** (5.7).

**Still open — decide with a default and log it in `docs/OPEN_QUESTIONS.md`:** which Grove ports to use for the Core power lead, button and flash; whether the breakout's inline capacitors are on the output and their ratings; exact trigger semantics of the Flashlight Unit's flash mode; font glyph sets (Latin only or Cyrillic too); what a second button/scroll wheel would be.

---

## Appendix A — the owner's brief, verbatim (nothing here may be lost)

> Well, I got all the parts ready excluding boards (core, cams) and LED flashlight. Let's prepare a first comprehensive prompt for Claude Code.
> My repository is here: https://github.com/qurle/x4. You already helped me with project structure, remember?
> My setup is:
> * M5Stack CoreS3-SE
> * M5Stack CamS3-5MP (ESP32-S3-WROOM-1-N16R8, OV5640 derivative)
> * M5Stack Flashlight Unit
> * M5Stack Mini Button Unit
> * Anker Nano 5000 MAh power bank
> * Plain power switch
> * 2200uF capacitors
> * 5 USB-C breakouts with inline capacitors
> * Wago 2273-208 (may reconsider but that's it)
> * Bunch of 20AWG stranded wires
> * Different ways Dupont cables
> * In-box grove cables for M5Stack products
> * Smartbuy MicroSDHC 10-Class 16GB
>
> Single power source, grove-powered Core and Cams.
>
> All stuff should work on-site, portable setup. Companion app is possible (flutter or web, I guess), but should mostly do post-processing.
>
> UI mockups can be found here: https://www.figma.com/design/fgEvP6Ms0bOAbWxfkFki4R/x4?node-id=73-368&t=ZZea3M8aQB3PEhvZ-11, I tried to make it self-explaining using node names, but feel free to ask for more context and mockups, it's design-heavy project, to be honest, so pick up the design language and find compromises to keep it.
>
> What's important for me:
> * design and interaction delight
> * performance
> * it shouldn't blow up or burn my components
>
> I got all the components, but not the M5Stack stuff. I'd like to have a base project so I could try to burn the firmware right away or with few patches.
>
> Ask Claude Code to check M5Stack GitHub issues and blogs for detailed info and possible problems.
>
> Make the docs folder with debug info, troubleshooting and stuff.
>
> Do not omit any information. You can reformat it or add important stuff.

Follow-up from the owner: "I like: clean structured logs and documented configs."

## Appendix B — decisions carried over from earlier planning (settled unless the owner says otherwise)

- Hardware stack: CoreS3 controller + 4x CamS3-5MP + Flashlight + button; **one** power bank and one charge port for the whole rig.
- Software: **Arduino/C++**, **LVGL**, **PlatformIO in VS Code**; UI on the Core's own screen; **no ESP-IDF**; MicroPython considered and rejected (M5 supports it through UIFlow2, but camera support and the LVGL Pro workflow are unclear).
- Network: Core's **own AP**, no hotspot, BLE deferred; the companion app fetches photos over HTTP after joining the AP and joins programmatically.
- Camera trigger by network (50–250 ms skew accepted); hardwired trigger line is a documented future upgrade.
- Cable/connection practice: star topology; thin Grove wire into push-in terminals needs ferrules or fold-back; tug-test; capacitor across the rail, never in series.
- All power numbers are estimates until measured.

## Appendix C — images attached by the owner (describe them in `docs/`; copy the original files into `docs/assets/` if the owner provides them)

1. **AXP2101 power-path diagram** (four panels: USB input/output × BUS input/output, with `BUS_OUT_EN` and `USB_OTG_EN` values and Port A/B/C 5 V). Used in 3.2. Panels: `BUS_OUT_EN=0, USB_OTG_EN=0` = USB in, bus 5 V flows **into** the AXP2101; `BUS_OUT_EN=1, USB_OTG_EN=0` = USB in, AXP drives the bus; `BUS_OUT_EN=0, USB_OTG_EN=1` = USB **out** (OTG), bus feeds in; `BUS_OUT_EN=1, USB_OTG_EN=1` = USB out and bus out. Firmware must stay in the first panel's state.
2. **3D render, rear view of a planned enclosure layout** (grey mock-up in a thin bounding-box frame): the Core's back shell in the middle; a small M5 unit at the top left; a row of Wago-style blocks along the bottom; the **power bank standing on the right edge** with a **two-button part and a bracket on top**; a **USB-C breakout** (pads G, D+, D−, V) next to the bank with a USB-C plug.
3. **3D render, front view**: power bank on the left edge with the two-button part on top; the Core above; **four CamS3 units in a row** with conical lens collars and base clips; a unit at the top right; one lens hole shows a small green PCB.

**The renders are a rough sketch of intent, not a source of truth** (the owner says so: they show 5-port blocks and two buttons, while the real parts are Wago 2273-208 and one Mini Button Unit). Use them only for the general feel and envelope in `ENCLOSURE.md`; leave space for extra buttons and a scroll wheel.

## Appendix D — the owner's answers, verbatim

> **What is each of the 5 USB-C breakouts for (bank input, per-device USB-C power, debug taps...)? Do the inline capacitors sit on the breakouts' outputs?**
> I got one for powerbank, I avoid using more until explicitly necessary
>
> **How exactly is the Core fed through Grove — Port A 5 V pin? Is USB-C ever connected to it while on the rail?**
> Port pin, avoid USB-C power until explicitly necessary
>
> **How many Wago 2273-208 do you have; OK to use two per net?**
> Ok, but I have some more
>
> **Does each camera need its own SD card, or is the Core's single card the only storage?**
> Core's single card
>
> **Is the flash unit going to be powered from the rail (recommended) and is on/off enough or do you want brightness control?**
> From the rail. It have flash (spike) mode, I guess I'll use that
>
> **Which Figma pages/nodes are final vs exploratory, and which fonts are licensed for embedding?**
> Node on this link exactly: https://www.figma.com/design/fgEvP6Ms0bOAbWxfkFki4R/x4?node-id=73-368&t=ZZea3M8aQB3PEhvZ-11. It's a section with couple of frame sets. I am using Instrument Serif and Space Mono. All licensed.
>
> **Is the Mini Button Unit the only physical control besides the touchscreen? Does it have an LED to use?**
> I got the button, touchscreen and power switch. Button does not have LED. I may replace it with.. more cool button later. Or maybe even a scrollwheel driver
>
> **Target resolution per shot (full 5 MP or lower) and acceptable transfer time after a shot?**
> Full 5MP. I can wait for 5 seconds.
>
> **Companion app: Flutter (needs phone join) or web, and is Android/iOS/both a requirement?**
> Not decided yet, maybe i'll start with web for debugging and then move to flutter for more "native" experience. Both platfroms.
>
> **The renders show blocks of 5 ports, while the owner lists Wago 2273-208 (8 ports). Which is real?**
> Render is NOT the source of trust. Currently I have some Wago 2273-208.
>
> **The renders show two buttons on top of the power bank, but the parts list has one Mini Button Unit. What is the second button?**
> I'd keep it open for more buttons assigned to features. Not mandatory but cool to have a space for scaling
>
> **Extra:** I like: clean structured logs and documented configs.
