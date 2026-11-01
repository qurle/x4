# Step by step: from "it boots" to the full rig

The full firmware does a lot at once: WiFi, four cameras, clock sync, UI, flash, SD. If you
flash that first and something is wrong, you won't know which part broke. So this page works
up to it in 13 small steps. Each step has its own tiny firmware in [`steps/`](../steps/), one
short `main.cpp` you can read top to bottom, that checks **one** new thing on top of the last
step.

Don't skip ahead. If a step fails, the problem is in what that step added.

Write the numbers into [MEASUREMENTS.md](MEASUREMENTS.md) as you go.

## Before you start

- Tools: VS Code + PlatformIO ([BRINGUP §0](BRINGUP.md#0-tools-and-software)), a multimeter,
  and a USB power meter between the bank and the breakout from step 5 on.
- Every command below runs from `steps/`:
  ```bash
  cd steps
  ```
- The one power rule, all the time: **a board gets 5 V from USB *or* from the rail, never
  both.** Unplug USB before the rail goes on, and switch the rail off before USB goes in.
  Why, and how to flash without breaking it: [BRINGUP](BRINGUP.md#flashing-without-breaking-the-power-rules).
- Every Core step switches the Core's 5 V output off and holds the flash pin (G2) LOW from the
  first instant of boot. That code is in `steps/include/core_common.h`.

| # | firmware | proves | power |
|---|---|---|---|
| 1 | `s01-hello` | Core boots, serial + screen work | USB |
| 2 | `s02-power` | Core's 5 V output is really off | USB |
| 3 | `s03-sd` | microSD writes and reads back | USB |
| 4 | `s04-touch` | touch screen | USB |
| 5 | `s05-rail` | Core runs from the power bank rail | rail |
| 6 | `s06-button` | Mini Button on G1 | rail |
| 7 | `s07-flash` | one guarded 50 ms flash | rail |
| 8 | `s08-cam-alone` | one camera: sensor + 5 MP photo | camera on USB |
| 9 | `s09-core-ap` + `s09-cam` | camera joins the Core's WiFi | Core any, camera USB |
| 10 | `s10-core-shot` + `s10-cam1` | button → photo → SD | rail |
| 11 | + `s10-cam2..4` | four cameras | rail |
| 12 | `core/` `hello-ui` | LVGL, fonts, animation | rail |
| 13 | `core/` `core` + `camera/` `cam1..4` | the real thing | rail |

Flash and watch any step (Core on USB, Port A empty):
```bash
pio run -e s01-hello -t upload
```
```bash
pio device monitor -e s01-hello
```

---

## 1. Hello

**Wiring:** USB-C from the computer into the Core. Nothing on Port A.

**Pass:** the screen says *step 1: hello* with chip/flash/PSRAM and an `alive` counter going
up. Serial prints `hello chip=ESP32-S3 … flash_mb=16 psram_mb=8` and then one line a second.

**If it fails:** no serial port → download mode: hold **RST** ~3 s until the green LED is on
(M5 docs), then upload again. `psram_mb=0` → wrong board settings (not expected with this
`platformio.ini`; report it).

## 2. The 5 V output is off

The rig powers the Core *backwards* through Port A. If the Core also pushed 5 V out of that
port, two supplies would fight. This step proves it doesn't.

**Wiring:** same as step 1.

**Pass:**
- Screen says **SAFE**, with all three bits at 0.
- Multimeter, DC volts. Port A **red (+) to black (−)**: about **0 V**. A few hundred mV of
  leakage is fine.
- Port A **yellow (G2) to black**: about **0 V** (the flash pin, held low).

Write both numbers in [HARDWARE.md](HARDWARE.md).

**If it fails:**
- Screen says **STOP**, or red reads about 5 V: **stop here** and don't connect the rail.
  Unplug USB, wait 10 s and retry, since M5Unified notes the output can stay latched after a
  precharge. If it still fails, report it ([TROUBLESHOOTING](TROUBLESHOOTING.md#core-5v-output-found-on)).
- Yellow reads about 3.3 V: something pulls G2 up. The flash would fire at boot. Move flash EN
  to G7 ([WIRING](WIRING.md#spare-ports-and-future-inputs)) before step 7.

## 3. microSD

**Wiring:** USB, plus a FAT32 microSD in the slot. 32 GB or smaller is the safe choice.

**Pass:** *verify ok*, plus write and read speeds for 1 MB. `x4cam_step3.bin` (1 MB) is on the
card. Write the speeds down: they set how fast photos can be stored.

**If it fails:** *mount FAILED* → reseat the card, format it FAT32, try another card
([TROUBLESHOOTING](TROUBLESHOOTING.md#sd)).

## 4. Touch

**Wiring:** USB.

**Pass:** red dots appear under your finger, all the way into the corners. Coordinates run
about 0…319 × 0…239. Hold the title bar to clear.

## 5. Core on the rail

The first time the real power path is used.

**Before connecting anything**, with the rail built but no boards on it:

| check | expected | if not |
|---|---|---|
| continuity +5V ↔ GND | **no beep** | find the short first |
| breakout CC1 / CC2 to GND (unplugged) | about 5.1 kΩ each | bank won't switch on: add 5.1 kΩ, or use an A-to-C cable |
| bank on, switch on, rail voltage | **4.75–5.25 V** | 0 V: press the bank's button / CC; > 5.3 V: stop |

Then:
1. Flash `s05-rail` over USB.
2. **Unplug USB.**
3. Rail switch off. Connect the split Grove cable's red/black from Port A to the rail.
4. Switch on.

**Pass:**
- Boots by itself. *last reset: power on*.
- *5V input* is about 4800–5100 mV, and the outputs show **off: SAFE**.
- Uptime keeps counting. Leave it 10 minutes.
- Switch off and on 10 times: the boot counter goes up by 10, with **brownouts: 0**.
- Current on the USB meter is about 0.15–0.3 A (ASSUMPTION; write yours down).

**If it fails:** no boot → check Grove contact and polarity, then the bank (some sleep at low
current; press their button). *BROWNOUT* → see [Power](TROUBLESHOOTING.md#power).

## 6. Mini Button

**Before wiring it to the Core:** power the button alone from the rail and measure **white to
GND**. You want about **3.3 V released, about 0 V pressed**. If you see about 5 V, don't
connect it ([HARDWARE](HARDWARE.md#mini-button-unit)).

**Wiring:** step 5, plus button red/black to the rail and white to Port A white (G1).

**Pass:** *DOWN* while pressed, a press counter, no phantom presses while you leave it alone
for a minute.

## 7. One guarded flash

**Wiring:** step 6, plus Flashlight red/black to the rail and Flashlight yellow (EN) to Port A
yellow (G2). Set the Flashlight's slide switch to FLASH. Only do this if step 2 measured G2 at
about 0 V.

**Use:** tap the screen to **ARM**. It disarms by itself after 60 s. Then press the button:
one 50 ms pulse. Another press inside 8 s is refused.

| watch | expected | if not |
|---|---|---|
| at switch-on | flash stays dark | lights at boot: **unplug it now**, G2 is pulled up (step 2) |
| rail minimum during a flash (meter min / scope) | ≥ 4.6 V | bigger/closer capacitor, shorter wires |
| resets | none (*last reset* in step 5 firmware stays *power on*) | [Power](TROUBLESHOOTING.md#power) |
| presses inside 8 s | *refused: cooling down* | – |
| connector after 10 flashes, 8 s apart | warm is fine, **hot = stop** | thicker / shorter wires |

## 8. One camera, alone

**Wiring:** one camera on M5's Grove-to-USB-C adapter. That's its power and its serial port.
Nothing else connected.

```bash
pio run -e s08-cam-alone -t upload
```
```bash
pio device monitor -e s08-cam-alone
```
If upload fails: unplug, short **G0 to GND** on the camera's header with the supplied jumper,
plug in, upload, remove the jumper, replug (M5 docs).

**Pass:**
- Serial shows `psram_mb=8` and `cam sensor pid=0x039e … ok=1`. The blue LED comes on.
- Join WiFi **x4cam-cam-test** (password `x4cam-steps`) from a phone and open
  `http://192.168.4.1/` → *5 MP photo*: a 2592×1944 JPEG.
- Note the file size and `grab_ms` from serial.

**If it fails:** `cam init FAILED … psram_mb=0` → PSRAM settings; a PID other than 0x039e →
different sensor or a bad ribbon connection ([TROUBLESHOOTING](TROUBLESHOOTING.md#camera)).

Do this for all four cameras before going on. It's also a good moment to write each camera's
number on it.

## 9. Camera joins the Core

**Wiring:** the Core runs `s09-core-ap`, on USB or on the rail (not both). Camera 1 runs
`s09-cam`, on its USB adapter so you can see its log.

```bash
pio run -e s09-core-ap -t upload
```
```bash
pio run -e s09-cam -t upload
```

**Pass:**
- The Core screen shows *WiFi x4cam-steps up* and, within a few seconds, **cam1 ok** with a
  reply time. Typically < 50 ms (ASSUMPTION; write yours down).
- Camera serial shows `joined … ip=192.168.4.11`.

Each camera gets a fixed address: camera N is `192.168.4.(10+N)`. No registration protocol
yet; that comes with the full firmware.

**If it fails:** see [WiFi](TROUBLESHOOTING.md#wifi). Check both are on this step's firmware
(SSID `x4cam-steps`).

## 10. Button → photo → SD

**Wiring:** the rig on the rail: Core, button, SD card, and camera 1 on the harness. The Core
runs `s10-core-shot`; camera 1 runs `s10-cam1`, flashed over USB first and then moved to the
harness. Flash it with the rail off, then unplug USB before the camera goes on the harness.

**Use:** press the button (or tap the screen). The Core asks each camera that answers for a
photo and saves it.

**Pass:** the screen shows *cam1 … kB in … ms*. The card has `/step10/0001_cam1.jpg`, and it
opens on a computer. Note the time: this is the transfer speed of one 5 MP photo over the
rig's WiFi.

This is **not** a synchronised shot. The Core asks the cameras one after another, so the
photos are a second or more apart. Shooting at the same instant is what the full firmware
adds ([PROTOCOL](PROTOCOL.md)).

## 11. Four cameras

Flash `s10-cam2`, `s10-cam3` and `s10-cam4` (same as step 10, one camera at a time on USB),
then put all four on the harness. The Core firmware stays the same.

**Pass:**
- The Core's *WiFi devices* line shows 4, and all four cameras show on a shot.
- One press saves `000N_cam1…cam4.jpg`.
- Switch the rig on and off 10 times with all four attached: no brownouts and no bank
  shut-off (inrush, [POWER §4](POWER.md#4-known-risks-and-mitigations)).
- Rail current at idle is about 1–1.5 A (ASSUMPTION; write yours down).

## 12. UI smoke test

The real UI stack (LVGL, fonts, display driver) with nothing else going on. From `core/`:
```bash
pio run -e hello-ui -t upload
```
**Pass:** a black screen, then "x4cam" in the serif font fading in and rising. Tap to replay.
Smooth, with no tearing ([UI](UI.md)).

## 13. The full firmware

From `core/` and `camera/` (copy the secrets first, see [BRINGUP §0](BRINGUP.md#0-tools-and-software)):
```bash
cd core && pio run -e core -t upload
```
```bash
cd camera && pio run -e cam1 -t upload
```
(`cam2`, `cam3`, `cam4` for the others.) Then carry on with
[BRINGUP step 5 onward](BRINGUP.md#step-5--flash-fire-stress-test): flash stress test, WiFi,
simultaneous shot with skew measurement, soak.

If something fails here that worked in steps 1–12, it's in the parts only the full firmware
has: registration, clock sync, the shooting state machine, the UI. Use
[DEBUGGING](DEBUGGING.md) and the logs.
