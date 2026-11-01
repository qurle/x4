# Bring-up: first flash and test ladder

New boards? Go through [STEPS.md](STEPS.md) first. It checks one thing at a time with tiny
test firmware. This page holds the flashing rules those steps rely on, and the tests of the
**full** firmware (step 13 there continues at step 5 here).

Do the steps in order. Each has an **expected result** and an **if it fails** link. Write the
numbers into [MEASUREMENTS.md](MEASUREMENTS.md) as you go.

## 0. Tools and software

1. VS Code + the extensions in `.vscode/extensions.json` (PlatformIO IDE, clangd, Red Hat XML,
   Error Lens). PlatformIO will download the pinned toolchain on the first build.
2. Copy the secrets and change the password:
   ```bash
   cp core/include/secrets.h.example core/include/secrets.h
   ```
   ```bash
   cp camera/include/secrets.h.example camera/include/secrets.h
   ```
   Same `X4_AP_SSID` / `X4_AP_PASS` in both. For `cam-hello`, also fill `X4_HOME_SSID/PASS`.
3. Build everything once (no boards needed):
   ```bash
   cd core && pio run && pio test -e native
   ```
   ```bash
   cd camera && pio run
   ```
4. A USB power meter (5 V, ≥ 3 A) between bank and breakout, and a multimeter.

## Flashing without breaking the power rules

**Rule: only one 5 V source per board, ever.**

Core (CoreS3-SE):
1. Rail switch **OFF**. **Unplug the split Grove cable from Port A** (it carries the rail's 5 V).
2. Plug USB-C from the computer into the Core.
3. Upload: `pio run -e core-hello -t upload` (or `-e core`). If the port does not appear, enter
   download mode: hold the **RST** key ~3 s until the green LED lights, release (M5 docs).
4. Watch the log: `pio device monitor -e core-hello`.
5. **Unplug USB-C before** reconnecting Port A and switching the rail on.

Camera (Unit CamS3-5MP):
1. Rail switch **OFF**. Unplug the camera's Grove lead from the rail harness.
2. Plug M5's **Grove-to-USB-C adapter** into the camera's Grove port, USB-C into the computer.
   (The Grove pins are the ESP32-S3 USB D+/D−; the adapter supplies 5 V. That is why the camera
   can never be on the rail and USB at once.)
3. If upload fails: unplug, short **G0 to GND** on the 1×3 header with the supplied jumper
   *before* power-up, plug in, upload, remove the jumper, power-cycle (M5 docs).
4. Upload: `pio run -e cam1 -t upload` (cam2, cam3, cam4 for the others; or `cam-hello` first).
5. Unplug the adapter, then put the camera back on the harness.

Is "USB and rail at the same time" ever OK? **No, not for x4.** The CoreS3-SE schematic and
M5Unified's comments show USB VBUS and the bus node are joined through two load switches when
both enables are 0, so two 5 V sources would fight. Keep the rule.

After the first USB flash, update over WiFi (Stage 5, ArduinoOTA): join your laptop to the
`x4cam` AP, then
```bash
pio run -e core -t upload --upload-port 192.168.4.1
```
Cameras: `--upload-port x4cam-cam2.local` or their IP (shown on the web page). OTA needs the
`X4_OTA_PASS` from secrets.h (PlatformIO asks, or set `upload_flags = --auth=...`).

## Test ladder

### Step 1 — No boards connected

| check | expected | if it fails |
|---|---|---|
| continuity +5V rail ↔ GND rail | **no beep** | find the short before anything else ([TROUBLESHOOTING](TROUBLESHOOTING.md#power)) |
| polarity: each red to +5V, black to GND, cap stripe to GND | beeps where expected | re-wire |
| USB-C breakout CC1/CC2 to GND | ≈ 5.1 kΩ each (or resistors marked "512") | none: use bank USB-A + A-to-C cable, or add 5.1 kΩ |
| breakout inline capacitor: value, voltage, side | recorded in HARDWARE.md | – |
| bank on, switch on, nothing attached: rail voltage | **4.75–5.25 V** | 0 V: CC resistors / switch / bank asleep (press its button); > 5.3 V: stop, wrong source |

### Step 2 — Each board alone on USB (rail off, Grove power lead unplugged)

Core: flash `core-hello`. Expected serial lines (abridged):
```
t=812 lvl=INFO dev=core mod=boot evt=start fw=0.1.0 git=abc1234
t=905 lvl=INFO dev=core mod=power evt=state when=boot bus_out_en=0 usb_otg_en=0 boost_en=0 vbus_mv=50xx … safe=1
t=930 lvl=INFO dev=core mod=boot evt=chip model=esp32s3 rev=… flash_mb=16 psram_mb=8 … board=…
t=931 lvl=INFO dev=core mod=boot evt=reset reason=poweron boot_count=1 brownouts_total=0
t=1650 lvl=INFO dev=core mod=sd evt=mount type=sdhc total_mb=15xxx free_mb=…
t=4000 lvl=INFO dev=core mod=sd evt=bench ok=1 write_kbps=… read_kbps=…
```
Screen: "x4cam" splash → **Bring-up** screen with system lines, camera lines, log tail and
buttons *Arm / Flash / SD*. Touch anywhere in the text area: a red dot appears under your finger
(touch test; also logged as `evt=touch_test x=… y=…`). Button test needs step 4 wiring.

- `safe=0` or `err=100/101`: **stop.** Do not connect the rail. [TROUBLESHOOTING](TROUBLESHOOTING.md#core-5v-output-found-on)
- SD mount fails: [TROUBLESHOOTING](TROUBLESHOOTING.md#sd).

Camera: flash `cam-hello` first (it does not need the Core). Expected:
```
t=… lvl=INFO dev=cam? mod=boot evt=chip … flash_mb=16 psram_mb=8 …
t=… lvl=INFO dev=cam? mod=cam evt=hw_rev reg_0x0200=0x01 meaning=new
t=… lvl=INFO dev=cam? mod=cam evt=sensor name=MEGA_CCM pid=0x039e expected=0x039e …
t=… lvl=INFO dev=cam? mod=bench evt=size res=5mp ok=3 bad=0 avg_bytes=… grab_ms_min=… frame_period_ms=…
t=… lvl=INFO dev=cam? mod=net evt=joined ssid=your-wifi ip=192.168.1.57 …
```
Then open `http://<ip>/info` and `http://<ip>/capture` (full 5 MP JPEG) in a browser, and
`http://<ip>:81/stream?res=0` for a QVGA MJPEG stream. Record sizes and timings in MEASUREMENTS.md.

- `pid` not 0x039e or `hw_rev=0xff`: [TROUBLESHOOTING](TROUBLESHOOTING.md#camera).
- `psram_mb=0`: wrong board settings (needs OPI PSRAM): [TROUBLESHOOTING](TROUBLESHOOTING.md#camera).

Then flash each camera with its id: `cam1` … `cam4`. Write each MAC (boot line `evt=chip`) on a
label on the camera.

Mini Button check (no Core needed): 5 V on red, GND on black, multimeter on white:
**≈ 3.3 V released, ≈ 0 V pressed.** ≈ 5 V released → do not connect it to G1 ([HARDWARE](HARDWARE.md#mini-button-unit)).

### Step 3 — Core's 5 V output is really off

Core on USB only (running `core-hello`), nothing on Port A. Measure Port A red (5 V) to black:

| expected | if it fails |
|---|---|
| **≈ 0 V** (a few hundred mV of leakage is OK) | ≈ 5 V: the Core is driving or latched its bus output. Check the log for `bus_out_en=1`. M5Unified notes BUS_OUT can stay latched to VBUS after a precharge: unplug USB fully, wait 10 s, retry. Still 5 V → **stop**, do not connect the rail, report |

Also measure **Port A yellow (G2) to black** with the firmware running: expected **≈ 0 V**
(firmware drives it LOW). ≈ 3.3 V means a pull-up exists on Port A: move the flash EN to
**G7 (M-Bus pin 22)** and set `kFlashEnAlt` in `core/include/board_pins.h` ([WIRING](WIRING.md#spare-ports-and-future-inputs)).
Record both in HARDWARE.md.

### Step 4 — One load at a time from the rail

USB unplugged everywhere. USB meter between bank and breakout. Switch off between steps.

| add | expected (rough, update with your numbers) | if it fails |
|---|---|---|
| Core (Port A, split cable) | boots from the rail alone; ≈ 0.15–0.3 A; `vbus_mv` ≈ rail voltage | no boot: Grove contact, polarity; bank asleep (press its button) |
| + Mini Button (white → G1) | diagnostics "button DOWN" when pressed | stays "up": wiring; "DOWN" stuck: wrong polarity or wrong unit |
| + cam1 | +0.15–0.3 A; cam joins `x4cam`, registers; footer/diagnostics show 1 camera, viewfinder shows its preview | [TROUBLESHOOTING WiFi](TROUBLESHOOTING.md#wifi) |
| + cam2, cam3, cam4 | ≈ 1–1.5 A total at idle; diagnostics list four cameras online | brown-out resets: [Power](TROUBLESHOOTING.md#power) |
| + Flashlight (EN → G2, rail power) | nothing lights at boot; current unchanged | lights at boot: EN pulled up (step 3), unplug immediately |

Then switch the whole rig on/off 10× with everything attached and watch the bank: no shut-off,
no reset loop (inrush, [POWER §4](POWER.md#4-known-risks-and-mitigations)).

### Step 5 — Flash-fire stress test

Bring-up/Diagnostics screen: **Arm**, then **Flash** → on-screen confirm → **Fire** (one 50 ms
pulse). Or the web page *Bring-up* card. Then repeat at the configured cap (100 ms) through
shots with the flash armed, at the 8 s cooldown, 10 times.

| watch | expected | if it fails |
|---|---|---|
| rail during the pulse (meter min / scope) | stays ≥ 4.6 V | larger/closer capacitor, shorter flash wires, lower WiFi power |
| resets | none (`reset=brownout` would appear on the next boot) | [Power](TROUBLESHOOTING.md#power) |
| refused pulses inside 8 s | `evt=refused verdict=cooldown` | – (that is the guard working) |
| connector warmth after 10 flashes | warm OK, **hot = stop** | thicker/shorter wires, better contacts |
| flash duration (phone slow-mo 240 fps) | ≈ pulse_ms | none/very short: slide switch in TORCH? EN wire? |

### Step 6 — WiFi

Core running `core`. Cameras join, register, heartbeat. Phone/laptop joins `x4cam` and opens
`http://192.168.4.1/`.

| expected | if it fails |
|---|---|
| four `evt=register` lines, diagnostics and web page show 4 online, `hb_age_ms` < 1500 | [WiFi](TROUBLESHOOTING.md#wifi) |
| `evt=ap … max_conn=6 (verified=6)` | AP full: raise `ap_max_clients` |

### Step 7 — Simultaneous shot

Press the button (or *Shoot* on the web page). Then:
```bash
python tools/x4log.py core.log --shots
```
Record: skew (`skew_us`), per-camera `cap_ms`, transfer ms and kB/s per camera, SD write speed,
**total time to "all 4 stored"** vs the 5 s target. Measure skew independently too
([DEBUGGING.md](DEBUGGING.md#measuring-shutter-skew)).

### Step 8 — Soak

30–60 min: preview on, a shot every ~30 s, a few with flash. Record temperatures (`temp_c` on
every camera and the Core), any `reset=brownout`, bank behaviour (auto-off?), SD free space.
