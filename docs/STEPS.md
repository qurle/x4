# Play-through: from a box of parts to the full rig

The full firmware does a lot at once: WiFi, four cameras, clock sync, UI, flash, SD. If you
flash that first and something is wrong, you won't know which part broke. So this guide works
up to it in 13 small steps. Each step has its own tiny firmware in [`steps/`](../steps/): one
short `main.cpp` you can read top to bottom, which checks **one** new thing on top of the last
step. Between the steps are **workshops**, where you build the next piece of the rig with the
power off.

Play it in order. If a step fails, the problem is in what that step added.

## How to play

Every step is a card with the same parts:

| part | what it means |
|---|---|
| **Rig** | what is wired and powered during the step |
| **Flash** | which firmware goes on which board. Explained in full at step 1, then shorter at each later step |
| **Measure** | multimeter / USB meter checks |
| **Read** | which serial lines or on-screen lines tell you it worked |
| **Pass** | all of these, or don't go on |
| **If not** | the first things to check, plus a link to the details |
| **Save** | the numbers to write down, and where |

### The one rule

> **A board gets 5 V from USB *or* from the rail, never both.**
> Before USB goes into a board: rail switch **off** and that board's Grove plug **out**. Before
> the rail goes on: USB **out**. Why: [POWER §2c](POWER.md#2-feeding-the-core-through-port-a-brief-32).

Every Core step also switches the Core's own 5 V output off and holds the flash pin (G2) LOW
from the first instant of boot (`steps/include/core_common.h`). That code is the reason you can
wire the flash before you trust anything else.

### Progress

Tick these in your copy as you go.

- [ ] [Level 0: gear up](#level-0-gear-up)
- [ ] [1. Hello](#1-hello): your first flash
- [ ] [Workshop A: the Port A cable](#workshop-a-the-port-a-cable)
- [ ] [2. The 5 V output is off](#2-the-5-v-output-is-off)
- [ ] [3. microSD](#3-microsd)
- [ ] [4. Touch](#4-touch)
- [ ] [Workshop B: the power rail](#workshop-b-the-power-rail)
- [ ] [5. Core on the rail](#5-core-on-the-rail)
- [ ] [Workshop C: button lead](#workshop-c-button-lead)
- [ ] [6. Mini Button](#6-mini-button)
- [ ] [Workshop D: flash branch](#workshop-d-flash-branch)
- [ ] [7. One guarded flash](#7-one-guarded-flash)
- [ ] [8. One camera, alone](#8-one-camera-alone) (×4)
- [ ] [9. Camera joins the Core](#9-camera-joins-the-core)
- [ ] [Workshop E: camera leads](#workshop-e-camera-leads)
- [ ] [10. Button → photo → SD](#10-button--photo--sd)
- [ ] [11. Four cameras](#11-four-cameras)
- [ ] [12. UI smoke test](#12-ui-smoke-test)
- [ ] [13. The full firmware](#13-the-full-firmware)
- [ ] [Workshop F: into the box](#workshop-f-into-the-box)

| # | firmware (env) | proves | power |
|---|---|---|---|
| 1 | `s01-hello` | Core boots, serial + screen work | USB |
| 2 | `s02-power` | Core's 5 V output is really off | USB |
| 3 | `s03-sd` | microSD writes and reads back | USB |
| 4 | `s04-touch` | touch screen | USB |
| 5 | `s05-rail` | Core runs from the power bank rail | rail |
| 6 | `s06-button` | Mini Button on G1 | rail |
| 7 | `s07-flash` | one guarded 50 ms flash | rail |
| 8 | `s08-cam-alone` | one camera: sensor + 5 MP photo | camera on USB |
| 9 | `s09-core-ap` + `s09-cam` | camera joins the Core's WiFi | Core USB, camera USB |
| 10 | `s10-core-shot` + `s10-cam1` | button → photo → SD | rail |
| 11 | + `s10-cam2..4` | four cameras | rail |
| 12 | `core/` `hello-ui` | LVGL, fonts, animation | USB, then rail |
| 13 | `core/` `core` + `camera/` `cam1..4` | the real thing | USB or rail |

---

## Level 0: gear up

**Parts.** Everything in the [bill of materials](HARDWARE.md#bill-of-materials). Count your Grove
cables: the rig needs **seven** (Core, button, flash, four cameras). Each unit comes with one
(*Not sure* the SE box has one too).

**Tools.**
- A multimeter with a continuity beeper.
- A USB power meter (5 V, ≥ 3 A) that shows its minimum voltage, between the bank and the
  breakout from step 5 on.
- Wire cutter/stripper, heat-shrink, a soldering iron for the pigtails
  ([WIRING: connectors](WIRING.md#connectors)).
- A data USB-C cable (some cables only charge; they give no serial port).
- M5's Grove-to-USB-C adapter for the cameras (step 8 on).

**Software.** Pick one way to drive PlatformIO; the guide shows both.
- **GUI:** VS Code with the PlatformIO IDE extension. Opening the repo root offers the
  recommended extensions (`.vscode/extensions.json`); accept them.
- **CLI:** the `pio` command. The PlatformIO IDE includes it: click the terminal icon in the
  PlatformIO status bar for a shell where `pio` works. Without VS Code: `pip install platformio`
  (CI uses 6.2.0).

**Pre-flight build.** No boards needed. This downloads the pinned toolchain once (a big download,
several minutes) and builds all 15 step firmwares (~5 min):
```bash
cd steps
```
```bash
pio run
```
**Pass:** every environment ends with `SUCCESS`. If one fails here, it's the toolchain or the
network, not your hardware ([FIRMWARE: pinned versions](FIRMWARE.md#toolchain-and-pinned-versions)).

> ⚠ `pio run` with no `-e` builds **every** environment, which is fine. But `pio run -t upload`
> with no `-e` (or the GUI's Upload with the *Default* environment selected) **uploads every
> environment one after another**, camera firmware included, to whatever board is plugged in.
> Always name the environment when you upload.

### Multimeter primer

Black probe in **COM**, red in **VΩ**. Never the A / mA jack for these checks: in a current jack
the meter is a short circuit across whatever you touch.

| mode | symbol | used for | rule |
|---|---|---|---|
| continuity | 🔊 / diode-beep | shorts, "is this wire connected to that" | power **off**, bank unplugged |
| resistance | Ω (20 kΩ range) | the breakout's CC resistors | **unpowered only**: ohms read wrong on a live circuit ([HARDWARE](HARDWARE.md#recorded-on-bring-up-fill-in) has an example) |
| DC volts | V⎓ (20 V range or auto) | rail, Port A, button and flash signals | red on the point, black on GND |

Grove sockets are too small to probe safely. Measure on the loose end of a Grove cable that is
plugged into the port (you make one in Workshop A). Keep the loose wire ends apart and taped,
except the two you're probing.

---

## 1. Hello

Your first flash. This one is spelled out in full; later steps only say what changes.

**Rig:** USB-C from the computer into the Core. **Nothing on Port A.**

**Flash, GUI (VS Code):**
1. *File → Open Folder…* and pick **`x4/steps`** (the folder with `platformio.ini`). Opening that
   folder itself is the sure way for PlatformIO to treat it as the project (*Not sure* whether
   it also finds nested projects when the repo root is open).
2. Wait until the PlatformIO toolbar appears in the blue status bar at the bottom.
3. Click the environment switcher in that bar (it shows *Default (steps)* or `env:…`) and choose
   **`env:s01-hello`**. With *Default* selected, Upload would flash every environment (see the
   warning above).
4. Click **→ Upload** in the status bar. Or use the PlatformIO sidebar (ant-head icon) →
   *Project Tasks* → `s01-hello` → *General* → *Upload*. The terminal ends with `SUCCESS`.
5. Click **🔌 Serial Monitor** (or *Project Tasks* → `s01-hello` → *Monitor*). Next time,
   *Upload and Monitor* does both in one click.
6. To stop the monitor: click into the terminal and press **Ctrl+C**. Only one program can hold
   the serial port, so a monitor left open makes the next upload fail with a "port busy" error.

**Flash, CLI:**
```bash
cd steps
```
```bash
pio device list
```
The Core shows up as a USB serial port: `/dev/cu.usbmodem…` (macOS), `/dev/ttyACM…` (Linux),
`COMn` (Windows).
```bash
pio run -e s01-hello -t upload
```
```bash
pio device monitor -e s01-hello
```
**Ctrl+C** quits the monitor. Both in one go: `pio run -e s01-hello -t upload -t monitor`. With
more than one board plugged in, add `--upload-port <port>` (upload) or `--port <port>` (monitor).

**If the upload can't find the board:**
1. Try a different cable (charge-only cables are common).
2. Enter **download mode**: hold **RST** ~3 s until the green LED lights, then release (M5
   docs, [HARDWARE](HARDWARE.md#cores3-se)). Upload again. If the new firmware doesn't start on
   its own afterwards, press RST once.
3. More: [TROUBLESHOOTING: flashing](TROUBLESHOOTING.md#flashing).

**Read:** the step firmwares print plain `name key=value` lines at 115200 baud:
```
power when=boot read_ok=1 bus_out_en=0 usb_otg_en=0 boost_en=0 vbus_mv=50xx safe=1
hello chip=ESP32-S3 rev=… cores=2 flash_mb=16 psram_mb=8
hello n=0 uptime_ms=…
hello n=1 uptime_ms=…
```
The first two print once at boot, often before the monitor attaches, so you may only see the
`hello n=` lines. That's fine: the screen shows the same chip info. The `power` line is the 5 V
check every Core step runs; step 2 is about it.

**Pass:**
- The screen says *step 1: hello*, shows chip / flash / PSRAM, and an `alive` counter goes up.
- Serial prints one `hello n=` line a second.

**If not:** `psram_mb=0` means wrong board settings (not expected with this `platformio.ini`;
report it).

---

## Workshop A: the Port A cable

The Core gets power, the button and the flash all through **one** Grove plug in Port A, with the
four wires split at the other end ([WIRING: diagram](WIRING.md#diagram)). Make it now, because
step 2 measures through it.

1. Take one Grove cable. Cut off one plug and strip ~8 mm of each wire.
2. Make the ends ready for the Wago blocks later: solder a short solid-core pigtail to red and
   black, heat-shrink the joints ([WIRING: connectors](WIRING.md#connectors)).
3. Tape each of the four ends separately. They must not touch each other.
4. Label it "CORE Port A".

---

## 2. The 5 V output is off

The rig powers the Core *backwards* through Port A. If the Core also pushed 5 V out of that
port, two supplies would fight. This step proves it doesn't.

**Rig:** Core on USB. The Port A cable plugged into Port A, loose ends taped and apart.

**Flash:** same as step 1 with **`s02-power`**. GUI: pick `env:s02-power`, *Upload and Monitor*.
CLI:
```bash
pio run -e s02-power -t upload -t monitor
```

**Measure** (DC volts, on the cable's loose ends, firmware running):

| probe | expected | if not |
|---|---|---|
| red (+) to black (−) | **≈ 0 V** (a few hundred mV of leakage is fine) | ≈ 5 V: **stop**, see below |
| yellow (G2, flash pin) to black | **≈ 0 V** (held low) | ≈ 3.3 V: something pulls G2 up, see below |

**Read:** a line every 2 s:
```
power when=poll read_ok=1 bus_out_en=0 usb_otg_en=0 boost_en=0 vbus_mv=50xx safe=1
```
`vbus_mv` is the 5 V the Core sees on its input (here: USB). A line with `when=found_on`
means something switched the output on and the firmware forced it off again. That should never
happen.

**Pass:** the screen says **SAFE** with all three bits at 0, `safe=1` on serial, and both
readings ≈ 0 V.

**If not:**
- **STOP** on screen, `safe=0`, or red reads ≈ 5 V: don't build the rail yet. Unplug USB, wait
  10 s and retry (M5Unified notes the output can stay latched after a precharge). Still failing:
  [TROUBLESHOOTING](TROUBLESHOOTING.md#core-5v-output-found-on).
- Yellow ≈ 3.3 V: the flash would fire at boot. Move flash EN to G7 before step 7
  ([WIRING](WIRING.md#spare-ports-and-future-inputs)).

**Save:** both voltages in [HARDWARE: recorded on bring-up](HARDWARE.md#recorded-on-bring-up-fill-in).

---

## 3. microSD

**Rig:** step 2, plus the 16 GB microSD (FAT32) in the slot. M5 documents **16 GB** as the
CoreS3 maximum ([HARDWARE](HARDWARE.md#bill-of-materials)).

**Flash:** `s03-sd`, *Upload and Monitor*. CLI:
```bash
pio run -e s03-sd -t upload -t monitor
```

**Read:**
```
sd mount type=… size_mb=15xxx
sd bench bytes=1048576 write_ms=… read_ms=… write_kbps=… read_kbps=… verify=1
```
The test runs once at boot. If the monitor attached too late, press RST once and watch again
(or just read the screen).

**Pass:** *verify ok* on screen (`verify=1`), and `x4cam_step3.bin` (1 MB) is on the card.

**If not:** *mount FAILED* → reseat the card, format it FAT32, try another card
([TROUBLESHOOTING: SD](TROUBLESHOOTING.md#sd)).

**Save:** write and read kB/s in [MEASUREMENTS: throughput](MEASUREMENTS.md#throughput). They set
how fast photos can be stored.

---

## 4. Touch

**Rig:** same. **Flash:** `s04-touch`, with the monitor.

**Read:** one `touch x=… y=…` line per tap.

**Pass:** red dots under your finger, all the way into the corners. Coordinates run about
0…319 × 0…239. Hold the title bar to clear.

---

## Workshop B: the power rail

The rail is power bank → USB-C breakout → switch → Wago blocks, with the 2200 µF capacitor
across it. Build it with **no boards attached** and the bank unplugged.

1. Read the [WIRING diagram and pin table](WIRING.md#pin-table) once, top to bottom.
2. Two Wago 2273-208 blocks per net, joined with a short solid wire: one pair for **+5 V**, one for
   **GND**.
3. Breakout V/VBUS → **switch** → +5 V blocks (red 20 AWG). The switch goes in +5 V only.
   Breakout G/GND → GND blocks (black 20 AWG).
4. The capacitor goes **across** the rail: + leg to +5 V, **stripe (−) to GND**. Never in series.
5. Every stranded or Grove wire gets a solid pigtail (or ferrule / Wago 221), then a
   **tug test** ([WIRING: connectors](WIRING.md#connectors)).

**Measure** (in this order):

| check | how | expected | if not |
|---|---|---|---|
| short | continuity +5 V block ↔ GND block, bank unplugged | **no beep** | find the short before anything else |
| CC resistors | Ω, breakout **unplugged**, CC1 → GND and CC2 → GND | ≈ 5.1 kΩ each | none: use the bank's USB-A port + an A-to-C cable. CC2 still off 5.1 kΩ: the plug may work in one orientation only ([HARDWARE](HARDWARE.md#recorded-on-bring-up-fill-in)) |
| rail, no load | bank in, switch on, DC V across the blocks | **4.75–5.25 V** | 0 V: switch, CC, or the bank is asleep (press its button). > 5.3 V: stop |
| switch | switch off, DC V | ≈ 0 V | wired across GND instead of in +5 V |

Then run the [printable pre-power checklist](WIRING.md#printable-pre-power-checklist).

**Save:** CC values, capacitor and switch ratings in [HARDWARE](HARDWARE.md#recorded-on-bring-up-fill-in).

---

## 5. Core on the rail

The first time the real power path is used. This step also teaches the move you'll repeat for
every Core step from now on.

### The USB swap (Core)

1. Rail switch **off**.
2. Unplug the **Port A plug** from the Core.
3. USB-C in. Flash.
4. **USB-C out.**
5. Port A plug back in. Rail switch **on**.

From here, while the Core runs on the rail there is **no serial log**: USB is unplugged.
**The screen is your log.** That's why every step firmware prints its state on screen.

**Rig:** the Port A cable's red/black go into the rail blocks (+5 V and GND). Yellow and white
stay taped. USB power meter between the bank and the breakout.

**Flash:** USB swap with **`s05-rail`**. You can watch it on USB first (it runs there too), but
the test is on the rail.

**Measure:**
- Before switching on: continuity +5 V ↔ GND once more (**no beep**: you just added a branch).
- After switching on: DC V at the blocks ≈ the no-load value from Workshop B.
- USB meter: current. About 0.15–0.3 A (ASSUMPTION; write yours down).

**Read (screen):**
```
last reset: power on
boots: N, brownouts: 0
5V input: 4950 mV (min …, max …)
outputs off: SAFE
uptime: … s
```
(Over USB the same appears as `rail boot reset=… boots=… brownouts=…` and
`rail uptime_s=… vbus_mv=… min_mv=… safe=1` every 2 s.)

**Pass:**
- Boots by itself on switch-on. *last reset: power on*.
- *5V input* about 4800–5100 mV, **SAFE**.
- Uptime keeps counting for 10 minutes.
- Switch off and on 10 times: *boots* goes up by 10, *brownouts* stays 0.

**If not:** no boot → Grove contact and polarity, then the bank (some banks sleep at low
current: press their button). *BROWNOUT* → [TROUBLESHOOTING: power](TROUBLESHOOTING.md#power).

**Save:** Core current in [MEASUREMENTS: current per device](MEASUREMENTS.md#current-per-device-5-v-rail).

---

## Workshop C: button lead

1. Grove cable into the Mini Button. Cut the far plug, strip, pigtail red/black.
2. Red → +5 V block, black → GND block. White loose and taped. Yellow is not used: tape it.
3. **Pre-check, Core unplugged:** rail on, DC V **white → GND**: about **3.3 V released, about
   0 V pressed**. About 5 V means a different design: **don't** connect it to the Core
   ([HARDWARE](HARDWARE.md#mini-button-unit)).
4. Rail off. Join the button's white to the Port A cable's white (solder + heat-shrink, or a
   small lever connector).

**Save:** released / pressed volts in [HARDWARE](HARDWARE.md#recorded-on-bring-up-fill-in).

## 6. Mini Button

**Rig:** step 5 plus the button lead.

**Flash:** USB swap, `s06-button`.

**Read (screen):** *button: DOWN* while pressed, *up* when released, a *presses* counter.

**Pass:** each press counts once, and there are no phantom presses when you leave it alone for a
minute.

**If not:** stays *up*: wiring, or white is joined to the wrong Port A wire. Stuck *DOWN*: wrong
polarity or a different unit.

---

## Workshop D: flash branch

Only if step 2 measured G2 at about 0 V.

1. Flashlight Grove cable: red/black onto their **own 20 AWG branch**, ≤ 30 cm, into the rail.
   The flash pulls ~1 A, so it never takes power through the Core.
2. Yellow (EN) → joined to the Port A cable's yellow. White is not used: tape it.
3. Set the Flashlight's slide switch to **FLASH**.
4. **Pre-check, Core unplugged:** rail on, DC V flash **yellow → GND** ≈ 0 V (the unit's own
   10 kΩ pull-down, [HARDWARE](HARDWARE.md#unit-flashlight-u152)). The LED stays dark.

## 7. One guarded flash

**Rig:** step 6 plus the flash branch. Don't look into the LED.

**Flash:** USB swap, `s07-flash`.

**Use:** tap the screen to **ARM** (it disarms itself after 60 s), then press the button: one
50 ms pulse. Another press within 8 s is refused.

**Measure:**

| watch | expected | if not |
|---|---|---|
| at switch-on | flash stays dark | lights at boot: **switch off and unplug it now**, G2 is pulled up (step 2) |
| USB meter min voltage during a flash | ≥ 4.6 V | bigger/closer capacitor, shorter wires. A USB meter is too slow to see the true peak ([DEBUGGING](DEBUGGING.md#measuring-current-with-the-flash-firing)) |
| connector after 10 flashes, 8 s apart | warm is fine, **hot = stop** | thicker / shorter wires, better contacts |

**Read (screen):** *ARMED (tap to disarm)*, *fired #N*, *refused: cooling down*, *next flash in
x.x s*.

**Pass:** one flash per press, refusals inside 8 s, and no reset (the screen doesn't go back to
the title).

**Save:** the 50 ms row in [MEASUREMENTS: flash](MEASUREMENTS.md#flash).

---

## 8. One camera, alone

A new board. This flash is also spelled out in full.

**Rig:** one camera on M5's Grove-to-USB-C adapter, and nothing else. The camera's Grove port *is*
its USB port (the yellow/white wires are USB D+/D−), so the adapter is its power and its serial
port at once ([POWER](POWER.md#1-architecture)).

**Flash:** same as step 1, environment **`s08-cam-alone`**. GUI: pick `env:s08-cam-alone`,
*Upload and Monitor*. CLI:
```bash
pio run -e s08-cam-alone -t upload -t monitor
```
If the upload can't find the camera: unplug it, short **G0 to GND** on its 1×3 header with the
supplied jumper, plug it in, upload, remove the jumper, unplug and replug (M5 docs).

**Read:**
```
step8 chip=ESP32-S3 psram_mb=8 flash_mb=16
cam sensor pid=0x039e expected=0x039e ok=1
wifi ap ssid=x4cam-cam-test ip=192.168.4.1
capture w=2592 h=1944 bytes=… grab_ms=…        (each time a photo is taken)
```
This firmware waits 1.5 s before printing, so you normally catch the boot lines.

**Use:** from a phone, join WiFi **x4cam-cam-test** (password `x4cam-steps`), open
`http://192.168.4.1/` → *5 MP photo*.

**Pass:** `ok=1`, the blue LED on, and a 2592×1944 JPEG on the phone.

**If not:** `cam init FAILED … psram_mb=0` → PSRAM settings. PID not `0x039e` → different sensor
or a loose ribbon ([TROUBLESHOOTING: camera](TROUBLESHOOTING.md#camera)).

**Save:** JPEG size and `grab_ms` in [MEASUREMENTS](MEASUREMENTS.md). Do all four cameras, and
**write the number 1–4 on each one** now.

---

## 9. Camera joins the Core

Two boards at once, each on its own USB cable (that's fine: one source per board).

**Rig:** Core on USB (Port A plug out), camera 1 on the adapter.

**Flash:** one board at a time is easiest: **plug in only the board you're flashing**.
- Core: `s09-core-ap`
- Camera 1: `s09-cam`

Then plug both in. Monitoring both needs two terminals and the ports from `pio device list`:
```bash
pio device monitor -e s09-core-ap --port <core port>
```
```bash
pio device monitor -e s09-cam --port <camera port>
```

**Read:**
- Core: `wifi ap ok=1 ssid=x4cam-steps ip=192.168.4.1`, then every 2 s
  `cam1 ok ms=… cam=1 rssi=… uptime_ms=…`
- Camera: `joined ssid=x4cam-steps ip=192.168.4.11 rssi=…`

Camera N always gets `192.168.4.(10+N)`: no registration protocol yet.

**Pass:** the Core screen shows *WiFi x4cam-steps up* and, within a few seconds, **cam1 ok**
with a reply time, typically < 50 ms (ASSUMPTION; write yours down).

**If not:** both boards must run this step's firmware (SSID `x4cam-steps`).
[TROUBLESHOOTING: WiFi](TROUBLESHOOTING.md#wifi).

---

## Workshop E: camera leads

Four leads, one per camera, each its own branch (star, never daisy-chained).

1. Grove cable per camera, plug stays at the camera end. Far end: red → +5 V, black → GND
   (pigtails).
2. **Yellow and white: cut short, insulate each, tape them, and mark them with tape** so nobody
   "fixes" them later. They are the camera's USB D+/D− and must never touch the rail
   ([WIRING: pin table](WIRING.md#pin-table)).
3. Label each lead 1–4.

**Measure** (bank unplugged): continuity +5 V ↔ GND **no beep**, each new red ↔ +5 V beeps,
each black ↔ GND beeps. The CamS3 has no reverse-polarity protection
([TROUBLESHOOTING: power](TROUBLESHOOTING.md#power)), so this check matters.

### The USB swap (camera)

1. Rail switch **off**.
2. Unplug the camera's harness plug.
3. Plug in the Grove-to-USB-C adapter. Flash.
4. Adapter out, harness plug back in. Rail on.

---

## 10. Button → photo → SD

**Rig:** everything on the rail: Core, button, SD card, and camera 1 on its lead.

**Flash:**
- Core: USB swap, `s10-core-shot`
- Camera 1: camera swap, `s10-cam1`

**Use:** press the button (or tap the screen). The Core asks each camera that answers for a photo
and saves it.

**Read (screen):** *SD ok. Button or tap = shoot*, *WiFi devices: 1*, then per shot
*cam1 … kB in … ms* and *shot N: 1 saved, … ms*.

**Pass:** `/step10/0001_cam1.jpg` is on the card and opens on a computer. (Switch off before
you take the card out.)

This is **not** a synchronised shot yet: the Core asks the cameras one after another. Shooting
at the same instant is what the full firmware adds ([PROTOCOL](PROTOCOL.md)).

**Save:** the transfer time of one 5 MP photo in [MEASUREMENTS: throughput](MEASUREMENTS.md#throughput).

## 11. Four cameras

**Flash:** camera swap, `s10-cam2`, `s10-cam3`, `s10-cam4`, one camera at a time. The Core keeps
`s10-core-shot`.

**Pass:**
- *WiFi devices: 4*, and all four cameras appear on a shot.
- One press saves `000N_cam1…cam4.jpg`.
- Switch the rig off and on 10 times with all four attached: no bank shut-off, and no board
  restarting by itself (inrush, [POWER §4](POWER.md#4-known-risks-and-mitigations)).

**Measure:** rail current at idle on the USB meter, about 1–1.5 A (ASSUMPTION).

**Save:** [MEASUREMENTS: rig total](MEASUREMENTS.md#rig-total).

---

## 12. UI smoke test

The real UI stack (LVGL, fonts, display driver) with nothing else going on. A new project folder:
`core/`.

**Flash:** GUI: open the **`x4/core`** folder (*File → Open Folder…*), pick `env:hello-ui`.
CLI:
```bash
cd core
```
```bash
pio run -e hello-ui -t upload -t monitor
```
`core/` and `camera/` have several default environments too: name the environment when you
upload, as in `steps/`.

**Read:** from here on, logs use the structured x4 format
(`t=… lvl=INFO dev=core mod=… evt=… key=value`, [LOGGING](LOGGING.md#format-logfmt)). Pipe them
through the log tool to get a table:
```bash
pio device monitor -e hello-ui | python ../tools/x4log.py
```
Look for `mod=power evt=state … safe=1`, `mod=boot evt=banner app=hello-ui …` and
`mod=ui evt=lvgl_init`. Each tap to replay logs `mod=ui evt=replay fps=…`.

**Pass:** a black screen, then "x4cam" in the serif font fading in and rising. Tap to replay.
Smooth, with no tearing ([UI](UI.md)). Then do the USB swap and check it on the rail too.

**Save:** the `fps` from the replay lines in [MEASUREMENTS: UI](MEASUREMENTS.md#ui).

## 13. The full firmware

**Flash:**
1. Secrets once: copy `secrets.h.example` to `secrets.h` in `core/include/` and
   `camera/include/`, with the same AP name and password in both
   ([BRINGUP §0](BRINGUP.md#0-tools-and-software)).
2. Core: USB swap, `core/` → `core`.
3. Cameras: camera swap, `camera/` → `cam1`, `cam2`, `cam3`, `cam4` (one each).

**Bench trick for reading the log:** the Core can stay on USB (Port A plug out) while the cameras
run on the rail. Each board still has one source. The button and flash are unplugged then, so
shoot from the web page (`http://192.168.4.1/`). Camera warnings are forwarded to the Core, so
the Core's serial port shows the whole rig ([LOGGING: sinks](LOGGING.md#sinks)).

**Read:** the boot banner ([BRINGUP step 2](BRINGUP.md#step-2--each-board-alone-on-usb-rail-off-grove-power-lead-unplugged)
shows the full set):
```
t=… lvl=INFO dev=core mod=power evt=state when=boot bus_out_en=0 usb_otg_en=0 boost_en=0 vbus_mv=50xx … safe=1
t=… lvl=INFO dev=core mod=boot evt=reset reason=poweron boot_count=… brownouts_total=0
t=… lvl=INFO dev=core mod=net evt=register cam=1 ip=192.168.4.11 …        (×4)
```
Save a session and look at it two ways:
```bash
pio device monitor -e core | tee core.log
```
```bash
python ../tools/x4log.py core.log --lvl WARN
```
```bash
python ../tools/x4log.py core.log --shots
```
Any `err=NNN`: look it up in [LOGGING: error codes](LOGGING.md#error-codes). On the rail,
use the **diagnostics screen** (long-press the shutter) or the web page instead
([DEBUGGING](DEBUGGING.md#diagnostics-screen-on-the-core)).

**Pass:** four `evt=register` lines, `safe=1`, a button press gives four photos.

Then carry on with [BRINGUP step 5 onward](BRINGUP.md#step-5--flash-fire-stress-test): flash
stress test, WiFi, simultaneous shot with skew measurement, soak. If something fails here that
worked in steps 1–12, it's in the parts only the full firmware has: registration, clock sync,
the shooting state machine, the UI ([DEBUGGING](DEBUGGING.md)).

**Lighter flashing from now on:** after the first USB flash, the full firmware updates over WiFi
(OTA): no swap, no cables. Laptop on the `x4cam` WiFi, then
`pio run -e core -t upload --upload-port 192.168.4.1` (asks for `X4_OTA_PASS` from `secrets.h`)
([BRINGUP](BRINGUP.md#flashing-without-breaking-the-power-rules)).

---

## Workshop F: into the box

Once everything works on the bench, mount it in the v1 shell ([ENCLOSURE](ENCLOSURE.md#v1-foam-board--cardboard)):
- the four cameras on one **rigid strip**, so their positions don't change between shots;
- **matte black sleeves** around every lens and the flash;
- **vents** near the cameras and the Core;
- keep access to the Core's USB-C, each camera's Grove plug and G0 header, the SD slot and the
  bank's charging port: the swaps above need them;
- Wago blocks and the capacitor away from the lenses, camera data wires taped.

Then run the soak ([BRINGUP step 8](BRINGUP.md#step-8--soak)) **inside the box**: heat is the
thing a closed box changes.

---

## Cheat sheets

### Flashing, short form

| board | before | upload | after |
|---|---|---|---|
| Core | rail off, Port A plug out, USB in | `pio run -e <env> -t upload` (in `steps/` or `core/`) | USB out, Port A in, rail on |
| camera | rail off, harness plug out, adapter in | `pio run -e <env> -t upload` (in `steps/` or `camera/`) | adapter out, harness in, rail on |
| Core, full firmware | on the `x4cam` WiFi | `pio run -e core -t upload --upload-port 192.168.4.1` | – |

Stuck: Core → hold RST ~3 s (green LED). Camera → G0 jumper to GND while plugging in. Always
`-e`; close the monitor before uploading.

### Every multimeter check

| when | probe | expected |
|---|---|---|
| step 2 | Port A red ↔ black, Core on USB | ≈ 0 V |
| step 2 | Port A yellow (G2) ↔ black | ≈ 0 V |
| Workshop B | +5 V ↔ GND continuity, unpowered | no beep |
| Workshop B | CC1, CC2 → GND, breakout unplugged | ≈ 5.1 kΩ each |
| Workshop B | rail, no load | 4.75–5.25 V |
| every time you add a branch | +5 V ↔ GND continuity | no beep |
| Workshop C | button white ↔ GND, rail on | ≈ 3.3 V released, ≈ 0 V pressed |
| Workshop D | flash yellow ↔ GND, rail on, Core unplugged | ≈ 0 V |
| Workshop E | each red ↔ +5 V, each black ↔ GND | beep |
| step 5 on | USB meter: rail current and minimum voltage | [MEASUREMENTS](MEASUREMENTS.md) |
| step 7 on | rail minimum during a flash | ≥ 4.6 V |

More methods (shunt + scope for the flash, current per device): [DEBUGGING](DEBUGGING.md#multimeter-and-usb-meter).

### Log lines worth knowing

| line | means |
|---|---|
| `power … safe=1` | Core 5 V outputs off: good. `safe=0` or `when=found_on`: stop |
| `hello … psram_mb=8` | board settings right |
| `sd bench … verify=1` | card writes and reads back |
| `rail boot reset=power on … brownouts=0` | clean start on the rail |
| `flash refused reason=cooldown` | the guard working, not a fault |
| `cam sensor pid=0x039e … ok=1` | right camera sensor |
| `joined … ip=192.168.4.1N` | camera N is on the Core's WiFi |
| `fetch cam=N code=200 bytes=…` | photo N downloaded |
| full firmware: `err=NNN` | look it up in [LOGGING](LOGGING.md#error-codes) |
| full firmware: `evt=reset reason=brownout` | the rail sagged: [TROUBLESHOOTING](TROUBLESHOOTING.md#power) |
