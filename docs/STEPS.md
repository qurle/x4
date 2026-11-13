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
| **Measure** | multimeter checks: volts, ohms (connected / not shorted), current |
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
- [ ] [Workshop 0: learn to solder](#workshop-0-learn-to-solder)
- [ ] [1. Hello](#1-hello): your first flash
- [ ] [Workshop A: the Port A cable](#workshop-a-the-port-a-cable)
- [ ] [2. The 5 V output is off](#2-the-5-v-output-is-off)
- [ ] [3. microSD](#3-microsd)
- [ ] [4. Touch](#4-touch)
- [ ] [Workshop B: the power rail](#workshop-b-the-power-rail) (leads, power check, rail)
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
- [ ] [14. Effects on the camera](#14-effects-on-the-camera-planned-no-firmware-yet) (planned)
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
| 14 | `s14-cam-effect` (planned) | a camera can bake crop + dither into its photo, and how fast | camera on USB |

---

## Level 0: gear up

**Parts.** Everything in the [bill of materials](HARDWARE.md#bill-of-materials). Count your Grove
cables: the rig needs **seven** (Core, button, flash, four cameras). Each unit comes with one
(*Not sure* the SE box has one too).

**Tools.**
- A multimeter with an **Ω (resistance)** range and a **10 A current jack** (most have one, marked
  `10A` or `A`). No clip leads needed: for current the probe tips are clamped in Wago 221 ports
  ([Measuring current](#measuring-current)).
- Wago 221 lever connectors for the rail ([WIRING: connectors](WIRING.md#connectors)).
- Wire cutter/stripper, heat-shrink, a temperature-controlled soldering iron, rosin-core solder
  and a flux pen: the USB-C breakout needs two soldered leads, plus a few wire splices. Full list:
  [SOLDERING: kit](SOLDERING.md#kit).
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

While you wait for parts: [DEVELOP.md](DEVELOP.md) covers building, the host tests and the **UI
simulator**, where you can click through the real UI on your computer.

> ⚠ `pio run` with no `-e` builds **every** environment, which is fine. But `pio run -t upload`
> with no `-e` (or the GUI's Upload with the *Default* environment selected) **uploads every
> environment one after another**, camera firmware included, to whatever board is plugged in.
> Always name the environment when you upload.

### Multimeter primer

Black probe in **COM**, red in **VΩ** for everything except current. In a current jack the meter
is a near short circuit: with the red probe in a current jack, **never** touch +5 V and GND at the
same time, and put the probe back in VΩ as soon as the current reading is done.

| mode | symbol | used for | rule |
|---|---|---|---|
| resistance, low | Ω, **200 Ω** range (or auto) | "is this wire connected to that", "is +5 V shorted to GND" ([below](#connected-or-shorted-check-with-ohms)) | **unpowered only**, bank unplugged |
| resistance, high | Ω, **20 kΩ** range | the breakout's CC resistors | **unpowered only**: ohms read wrong on a live circuit ([HARDWARE](HARDWARE.md#recorded-on-bring-up-fill-in) has an example) |
| DC volts | V⎓ (20 V range or auto) | rail, Port A, button and flash signals | red on the point, black on GND |
| DC current | A⎓, red probe in the **10 A** jack | rail current (step 5 on), current per board | always **in series**: the current flows *through* the meter. Never across +5 V and GND ([below](#measuring-current)) |

Grove sockets are too small to probe safely. Measure on the loose end of a Grove cable that is
plugged into the port (you make one in Workshop A). Keep the loose wire ends apart and taped,
except the two you're probing.

### Connected or shorted? Check with ohms

There's no beeper in this guide: every "is it connected" and "is it shorted" check uses the
**Ω 200 Ω** range. (Diode mode can do it too, but ohms gives a number you can write down.)

**Before any ohms check:** bank unplugged, no USB in any board, and V⎓ across the rail reads
≈ 0 V. The 2200 µF capacitor can stay charged after the bank is out, and ohms on a charged
circuit read nonsense. To empty it: wait, or hold the 47 Ω resistor across the rail for ~10 s.

**Zero the leads once:** touch the probe tips together. Most meters show a few tenths of an ohm
(the leads themselves, ASSUMPTION; write yours down). Call that *R₀*.

| check | expected | means |
|---|---|---|
| **connected**: one end of a wire/joint to the other | within ~1 Ω of *R₀*, and it **doesn't jump** while you flex the joint | good. Jumps or flickers when flexed → a bad joint or loose contact |
| **not shorted**: +5 V ↔ GND, or a pad ↔ its neighbour | **OL** (some meters show a lone **1** at the left: out of range) | good |
| … with the 2200 µF on the rail | starts low and **climbs** (to OL, or until it settles) | good: the meter is slowly charging the capacitor. It can take a while (Not sure how long with your meter) |
| … with boards on the rail | may **settle** at some value instead of OL: the boards' own circuits | good if it's well above ~5 Ω and stays put. Write it down the first time and compare each time you add a branch (Not sure what value to expect) |
| **shorted** | **stays under ~5 Ω** and doesn't climb (ASSUMPTION threshold) | stop: find the short before any power goes on |

The probe polarity doesn't matter for these checks.

### Measuring current

The meter has to sit **in** the +5 V line, so the rig's current runs through it. The rail is built
for that (Workshop B): a small **IN** Wago between the breakout and the switch, with one free
port, and one free port on the **+5 V rail**. The meter goes between those two free ports, so it
sits **across the switch**:

1. **Bank unplugged**, rail switch **off**. Wait ~10 s so the capacitor drains into the boards.
2. Dial to **A⎓**, red lead in the **10 A** jack, black in COM.
3. Open the lever of the IN node's free port, push the **red probe tip** in, close the lever. Same
   with the **black probe tip** in the +5 V rail's free port. Tug gently: it must not slide out.
   (Order only changes the sign. Not sure every probe tip grips well: they're tapered. If one
   wiggles, wrap 4–5 turns of bare solid wire tightly around the tip and clamp the wire instead.)
4. Plug the bank in. The switch is open, so the current runs through the meter: the rig powers
   up and the meter shows its current. Read the typical value over ~10 s (WiFi makes it jump).
5. Between readings: switch **on**. That bypasses the meter (it reads ~0) and lifts its time
   limit, without a power cut. Switch **off** again for the next reading.
6. Done: **unplug the bank first**, then take both probes out, red lead back to **VΩ**.

Why the bank comes out first: with the red lead in the 10 A jack, the two probe tips are joined
through the meter. While one is clamped in a live port, the loose tip is live too, and touching
GND with it shorts the bank.

Current of **one board**: open the lever on that board's red wire, take the wire out, clamp it
into a spare small Wago with the red probe, and the black probe into the rail's free port. Same
rule: bank unplugged while you set it up.

Know the limits:
- **Time limit on the 10 A jack.** Many meters allow high currents only for a short time
  (the label near the jack often says something like "10 A max 10 s / 15 min"; ASSUMPTION,
  read yours). The full rig's ~1–1.5 A is under most limits, but take the reading and bypass
  (step 5 above).
- **The meter costs voltage.** It and its leads add resistance (often around 0.1 Ω; Not sure for
  yours), so at 1.5 A the rail gets ~0.15 V less. Don't fire the flash or run sag tests with the
  meter in line; a reset then may be the meter's fault, not the rig's.
- **No peaks, no minimum.** A meter averages over a fraction of a second; it can't show the flash
  pulse, the switch-on inrush, or the minimum voltage. For those: the Core's own reading (step 7
  shows the dip during a flash), the meter's MIN/MAX in **volts** mode, or a shunt + scope
  ([DEBUGGING](DEBUGGING.md#measuring-current-with-the-flash-firing)).
- **Never** in the mA / µA jack: its fuse is for small currents and blows at the Core's draw.

---

## Workshop 0: learn to solder

The rig needs only a few solder joints: two leads on the USB-C breakout (unavoidable: it has
bare pads), maybe two switch lugs, and a few wire splices
([SOLDERING: what gets soldered](SOLDERING.md#what-gets-soldered)). None are on the M5 boards.
Practice first, on things you can afford to ruin: scrap wire and one of the **spare** breakouts
(you have five and the rig uses one).

Read [SOLDERING](SOLDERING.md) once: safety, the one technique, recipes R1–R3, and the "is the
joint good?" table.

1. **Three splices** (recipe R3) on scrap: 20 AWG to 20 AWG, then two with a thin wire (an old
   Grove cable or any thin stranded wire) to solid core. Heat-shrink on every one.
2. **Two pad joints** (recipe R2) on a **spare breakout**: a red lead on VBUS, a black on GND.
   Glue or tie them down.
3. Check each joint against [is the joint good?](SOLDERING.md#is-the-joint-good)

**Measure** (nothing powered):

| check | expected |
|---|---|
| each splice end to end | **connected** ([ohms check](#connected-or-shorted-check-with-ohms)), steady while you flex the joint |
| spare breakout: VBUS ↔ GND | **not shorted** (OL) |
| spare breakout: VBUS ↔ CC1, CC2, D+, D− | not shorted |
| spare breakout: pad ↔ far end of its lead | connected |

**Pass:** every joint passes the table and survives a gentle tug. Bad ones get redone, which is
the practice. You're not ready while a joint "probably" holds.

**Optional, if you have the 47 Ω ≥ 1 W resistor:** power the spare breakout from the bank and do
the [power check](SOLDERING.md#prove-the-power-path) P2–P4b on it. That's the same test you'll
do on the real one in Workshop B, with nothing at stake.

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
2. Make the ends ready for the Wago 221s later: strip red and black ~22 mm and fold the bare
   end double (Grove wire is thinner than the 221 is rated for), or solder a short 20 AWG
   pigtail on (recipe [R3](SOLDERING.md#r3-splice-and-pigtail-wire-to-wire)) and heat-shrink it
   ([WIRING: connectors](WIRING.md#connectors)).
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
sd mount mhz=25 type=… size_mb=15xxx
sd bench mhz=25 bytes=1048576 written=1048576 file_size=1048576 write_ms=… read_ms=… write_kbps=… read_kbps=… verify=1
```
then the same at 10 and 4 MHz. The test runs once at boot. If the monitor attached too late,
press RST once and watch again (or just read the screen).

**Pass:** *25 MHz ok* and *PASS* on screen (`mhz=25 … verify=1`), and `x4cam_step3.bin` (1 MB)
is on the card. 10 and 4 MHz are only there to narrow down a failure.

**If not:**
- *mount FAILED* → reseat the card, format it FAT32, try another card
  ([TROUBLESHOOTING: SD](TROUBLESHOOTING.md#sd)).
- *write short* → the card stopped taking data: card full, write-protected or worn out.
- *4 KB reads FAIL, 512 B ok* → the card handles one-sector reads but not multi-sector ones:
  look for `token error [12] 0x…` just above (the card's answer to CMD12, "stop reading").
  Try another card first.
- *verify FAILED* → an `sd verify … read=1/2/3 chunk=… bad=… first_bad=…` line follows. Both reads with
  the same `bad` and `first_bad`: the data went onto the card wrong. Different numbers: the
  reads are unstable. Fails at 25 MHz but passes at 10 or 4: the card or its contacts can't keep
  up with 25 MHz. Fails at every clock: try another card first, then send the log.

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

The rail is power bank → USB-C breakout → IN node → switch → Wago blocks, with the 2200 µF
capacitor across it. Build it in three parts, each checked before the next: **B1** leads on the
breakout, **B2** proof that the bank's power gets through and stays on, **B3** the rail itself.
**No M5 boards attached** during the whole workshop.

### B1. Breakout leads

Bank unplugged.

1. Write down the breakout's pad markings, then do the [P2 check](SOLDERING.md#prove-the-power-path)
   on the **bare** breakout: bank in, probes on the VBUS and GND pads, both plug orientations.
   Note which orientation gives 5 V, and whether the bank switches itself off within a minute
   with nothing connected. Bank out again.
2. Solder a red 20 AWG lead to **VBUS** and a black to **GND**
   ([recipe R2](SOLDERING.md#r2-wire-to-a-breakout-pad-vbus-gnd)). Leave CC, D+ and D− alone.
3. **Fix the breakout down** (screw, zip tie or hot glue onto the foam board or a scrap of
   wood), with the leads tied down next to it. The bank's built-in plug otherwise levers the small
   socket every time the bank moves.

### B2. Power check: the bank stays on

Do [P3, P4 and P4b](SOLDERING.md#prove-the-power-path) on the real breakout: ohms (connected / not shorted), then
5 V at the leads' far ends, then the **wiggle test**. Flex each joint and each wire, and gently
move the bank in its socket, while you watch the meter.

**Pass:** the reading never drops, in the orientation you'll use. Use the meter's MIN/MAX mode if
it has one.

**If not:** the bank turning off is the most common problem here. *When* it turns off tells you
why: [the bank keeps turning off](SOLDERING.md#the-bank-keeps-turning-off). Don't go on to B3
until this passes: everything after this step depends on that power staying on.

### B3. The rail

Bank unplugged.

1. Read the [WIRING diagram and pin table](WIRING.md#pin-table) once, top to bottom.
2. Wago 221 blocks per net, joined with short 20 AWG jumpers: **+5 V needs ≥ 10 ports**, **GND
   ≥ 9** (three 5-port blocks per net give 11; [WIRING: connectors](WIRING.md#connectors)).
3. Breakout red lead → **IN node** (a small 221, e.g. 3-port) → **switch** → +5 V blocks (red
   20 AWG). The switch goes in +5 V only (solder lugs: [recipe R4](SOLDERING.md#r4-switch-lugs)).
   Keep one IN port and one +5 V port **free**: that's where the meter goes
   ([Measuring current](#measuring-current)). Breakout black lead → GND blocks.
4. The capacitor goes **across** the rail: + leg to +5 V, **stripe (−) to GND**. Never in series.
5. Every wire in with the lever closed, then a **tug test**. Thin Grove ends folded double or
   pigtailed ([WIRING: connectors](WIRING.md#connectors)).

**Measure** (in this order):

| check | how | expected | if not |
|---|---|---|---|
| short | Ω 200 Ω, +5 V block ↔ GND block, bank unplugged, capacitor empty | **not shorted**: OL, or climbing once the capacitor is in ([ohms check](#connected-or-shorted-check-with-ohms)) | stays under ~5 Ω: find the short before anything else |
| CC resistors | Ω, breakout **unplugged**, CC1 → GND and CC2 → GND | ≈ 5.1 kΩ each | none: use the bank's USB-A port + an A-to-C cable. CC2 still off 5.1 kΩ: the plug may work in one orientation only ([HARDWARE](HARDWARE.md#recorded-on-bring-up-fill-in)) |
| rail, no load | bank in, switch on, DC V across the blocks | **4.75–5.25 V** | 0 V: switch, CC, or the bank is asleep (press its button). > 5.3 V: stop |
| switch | switch off, DC V | ≈ 0 V | wired across GND instead of in +5 V |
| IN node | switch off, DC V IN ↔ GND | ≈ the no-load value | IN is live whenever the bank is in: treat it like the rail |

Then run the [printable pre-power checklist](WIRING.md#printable-pre-power-checklist).

**Save:** breakout pads, working orientation, the bank's no-load turn-off time, CC values,
capacitor and switch ratings in [HARDWARE](HARDWARE.md#recorded-on-bring-up-fill-in).

---

## 5. Core on the rail

The first time the real power path is used. This step also teaches the move you'll repeat for
every Core step from now on.

From here on the Core sits in the rig upside down, so steps 5+ turn the screen 180 degrees
(`X4_SCREEN_FLIP=1` in `steps/platformio.ini`, section `core_rig`). The full firmware does the
same with the config option `ui_flip` (default 1; [CONFIG](CONFIG.md)).

### The USB swap (Core)

1. Rail switch **off**.
2. Unplug the **Port A plug** from the Core.
3. USB-C in. Flash.
4. **USB-C out.**
5. Port A plug back in. Rail switch **on**.

From here, while the Core runs on the rail there is **no serial log**: USB is unplugged.
**The screen is your log.** That's why every step firmware prints its state on screen.

**Rig:** the Port A cable's red/black go into the rail blocks (+5 V and GND). Yellow and white
stay taped. Meter ready for current: probes in the IN and +5 V free ports, as in
[Measuring current](#measuring-current).

**Flash:** USB swap with **`s05-rail`**. You can watch it on USB first (it runs there too), but
the test is on the rail.

**Measure:**
- Before switching on: +5 V ↔ GND **not shorted** once more ([ohms check](#connected-or-shorted-check-with-ohms)): you just added a branch. Write down the value it settles at.
- After switching on: DC V at the blocks ≈ the no-load value from Workshop B.
- Current: switch **off** with the meter in place, so the Core runs through the meter.
  About 0.15–0.3 A (ASSUMPTION; write yours down). Then bank out, probes out, red lead back to
  VΩ. The 10× on/off test below is done **without** the meter.
- **Drop test** ([P5](SOLDERING.md#prove-the-power-path)), switch **on**, meter back on V⎓ (not in
  the current jack): both probes on the same net. Red on the breakout's VBUS pad, black on a +5 V
  block: under ~0.05 V. Then red on a GND block, black on the breakout's GND pad: same.
- **Wiggle test on the rail:** tap the screen (resets *min*, *max* and *dips*), then flex every
  joint and wire on the power path, and gently move the bank in its socket. *dips* must stay
  **0** and *min* must stay within ~0.1 V of the normal reading. The firmware reads the 5 V input
  every ~5 ms and counts every drop below 4.5 V as a dip.

**Read (screen):**
```
last reset: power on
boots: N, brownouts: 0
5V input: 4950 mV (min …, max …)
outputs off: SAFE
uptime: … s
dips <4.5V: 0 (tap = reset)
```
(Over USB the same appears as `rail boot reset=… boots=… brownouts=…`,
`rail uptime_s=… vbus_mv=… min_mv=… max_mv=… dips=… safe=1` every 2 s, and a
`rail dip mv=… dips=…` line the moment a dip happens.)

**Pass:**
- Boots by itself on switch-on. *last reset: power on*.
- *5V input* about 4800–5100 mV, **SAFE**.
- Uptime keeps counting for 10 minutes.
- Switch off and on 10 times: *boots* goes up by 10, *brownouts* stays 0.

**If not:** no boot → Grove contact and polarity, then the bank (some banks sleep at low
current: press their button). *BROWNOUT* or *min* drops when you wiggle → a joint or contact on
the power path ([the bank keeps turning off](SOLDERING.md#the-bank-keeps-turning-off),
[TROUBLESHOOTING: power](TROUBLESHOOTING.md#power)).

**Save:** Core current in [MEASUREMENTS: current per device](MEASUREMENTS.md#current-per-device-5-v-rail),
drops in [MEASUREMENTS: rail and joints](MEASUREMENTS.md#rail-and-joints).

---

## Workshop C: button lead

1. Grove cable into the Mini Button. Cut the far plug, strip, fold red/black double (or pigtail).
2. Red → +5 V block, black → GND block. White loose and taped. Yellow is not used: tape it.
3. **Pre-check, Core unplugged:** rail on, DC V **white → GND**: about **3.3 V released, about
   0 V pressed**. About 5 V means a different design: **don't** connect it to the Core
   ([HARDWARE](HARDWARE.md#mini-button-unit)).
4. Rail off. Join the button's white to the Port A cable's white (splice
   [R3](SOLDERING.md#r3-splice-and-pigtail-wire-to-wire) + heat-shrink, or a small lever connector).

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
| the **dip** line on screen after each flash: the lowest 5 V reading the Core saw during the pulse and 100 ms after | ≥ 4.6 V (green) | bigger/closer capacitor, shorter wires. *reading never changed* = the Core's ADC is too slow to tell (Not sure yet how fast it is): then use the meter on **DC volts** across the blocks with MIN/MAX, which is only a hint. The real pass is still "no reset" below ([DEBUGGING](DEBUGGING.md#measuring-current-with-the-flash-firing)) |
| connector after 10 flashes, 8 s apart | warm is fine, **hot = stop** | thicker / shorter wires, better contacts |

**Read (screen):** *ARMED (tap to disarm)*, *fired #N*, *refused: cooling down*, *next flash in
x.x s*, *dip: min 4710 mV (before 4980), 6/140* (lowest reading, the reading just before the
flash, how often the value changed / how many reads). Over USB the same is in
`flash fired … vbus_min_mv=… changes=… reads=…`.

**Pass:** one flash per press, refusals inside 8 s, and no reset (the screen doesn't go back to
the title).

**Save:** the 50 ms row in [MEASUREMENTS: flash](MEASUREMENTS.md#flash), with the dip's
*changes/reads* (it tells us whether the Core's own reading is fast enough to trust).

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
   (ends folded double or pigtailed).
2. **Yellow and white: cut short, insulate each, tape them, and mark them with tape** so nobody
   "fixes" them later. They are the camera's USB D+/D− and must never touch the rail
   ([WIRING: pin table](WIRING.md#pin-table)).
3. Label each lead 1–4.

**Measure** (bank unplugged, [ohms check](#connected-or-shorted-check-with-ohms)): +5 V ↔ GND
**not shorted** (and not lower than the value you wrote down at step 5 by much), each new red
↔ +5 V **connected**, each black ↔ GND **connected**. The CamS3 has no reverse-polarity protection
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

**Measure:** rail current at idle with the meter in the IN and +5 V free ports, switch open
([Measuring current](#measuring-current)), about 1–1.5 A (ASSUMPTION). Read it and bypass
(switch on) within your meter's time limit. Don't shoot or stream with the meter in line.

**Drop test** again at this load ([P5](SOLDERING.md#prove-the-power-path)), switch on, meter on V⎓:
breakout VBUS pad → +5 V block, and GND block → breakout GND pad, under ~0.15 V each. After
10 minutes, touch the joints, the switch and the Wago blocks: warm is fine, **hot = stop** and
redo that one.

**Save:** [MEASUREMENTS: rig total](MEASUREMENTS.md#rig-total) and
[rail and joints](MEASUREMENTS.md#rail-and-joints).

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

**Pass:** four `evt=register` lines, `safe=1`, a button press gives four photos
(`/x4/000001-1.jpg` … `-4.jpg`; `exiftool -Comment` on one shows the shot's settings).

Then carry on with [BRINGUP step 5 onward](BRINGUP.md#step-5--flash-fire-stress-test): flash
stress test, WiFi, simultaneous shot with skew measurement, soak. If something fails here that
worked in steps 1–12, it's in the parts only the full firmware has: registration, clock sync,
the shooting state machine, the UI ([DEBUGGING](DEBUGGING.md)).

**Lighter flashing from now on:** after the first USB flash, the full firmware updates over WiFi
(OTA): no swap, no cables. Laptop on the `x4cam` WiFi, then
`pio run -e core -t upload --upload-port 192.168.4.1` (asks for `X4_OTA_PASS` from `secrets.h`)
([BRINGUP](BRINGUP.md#flashing-without-breaking-the-power-rules)).

---

## 14. Effects on the camera (planned, no firmware yet)

**Question:** can each camera bake the portrait crop and the dither into its own photo, so the
card holds finished pictures and no companion is needed? Today the photos stay original and the
settings ride along inside the JPEG ([PROTOCOL](PROTOCOL.md#sd-layout-fat32)).

**Why on the camera:** the camera module only outputs JPEG (VERIFIED, [HARDWARE](HARDWARE.md)),
so the photo has to be decoded, cropped/dithered and encoded again. The Core can't do that for
four photos in time, and its 8 MB PSRAM can't hold a decoded 5 MP frame (~15 MB) anyway. Each
CamS3 has its own ESP32-S3 + 8 MB PSRAM and is idle once its photo is out, and the four work in
parallel. Dithered output has to be PNG: JPEG smears the dither pattern and grows.

**Plan (one camera, USB, after step 13):** a `s14-cam-effect` env that takes a 5 MP photo,
decodes it in strips, applies the portrait crop + Bayer dither (`shared/src/dither.cpp`) and
encodes PNG, at full size and at 1296×972. Nothing like this has been measured; the times are
**Not sure** until this runs.

**Save:** decode / dither / encode ms, PNG bytes and peak PSRAM, both sizes, in
[MEASUREMENTS](MEASUREMENTS.md). **Decides:** if one photo is done in a few seconds, baked
effects become the default (original kept only on request); if not, fall back to the smaller size
or keep the in-photo settings.

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
| Workshop 0 | Ω: each practice joint end to end / to its neighbours | connected / OL |
| Workshop B1 | bare breakout VBUS ↔ GND pads, bank in, both orientations | 4.75–5.25 V (note which) |
| Workshop B2 | leads' far ends, bank in, wiggle everything | steady 5 V, never drops |
| Workshop B | Ω: +5 V ↔ GND, unpowered, capacitor empty | not shorted (OL or climbing) |
| Workshop B | CC1, CC2 → GND, breakout unplugged | ≈ 5.1 kΩ each |
| Workshop B | rail, no load | 4.75–5.25 V |
| every time you add a branch | Ω: +5 V ↔ GND | not shorted; compare with your last value |
| Workshop C | button white ↔ GND, rail on | ≈ 3.3 V released, ≈ 0 V pressed |
| Workshop D | flash yellow ↔ GND, rail on, Core unplugged | ≈ 0 V |
| Workshop E | Ω: each red ↔ +5 V, each black ↔ GND | connected (≈ R₀) |
| Workshop B | IN node ↔ GND, switch off | ≈ no-load value (IN is always live) |
| step 5 on | A⎓ in the 10 A jack, probes in the IN and +5 V free ports, switch **off**; bank out while you place them | Core ≈ 0.15–0.3 A, full rig ≈ 1–1.5 A ([MEASUREMENTS](MEASUREMENTS.md)) |
| step 5 | drop: breakout pad → Wago block, same net, each side, switch on, V⎓ | < ~0.05 V |
| step 11 | the same drop at full rig load | < ~0.15 V |
| step 7 on | the *dip* line on screen; else V⎓ across the blocks with MIN/MAX | ≥ 4.6 V |

More methods (shunt + scope for the flash, current per device): [DEBUGGING](DEBUGGING.md#multimeter).

### Log lines worth knowing

| line | means |
|---|---|
| `power … safe=1` | Core 5 V outputs off: good. `safe=0` or `when=found_on`: stop |
| `hello … psram_mb=8` | board settings right |
| `sd bench mhz=25 … verify=1` | card writes and reads back at the firmware's 25 MHz |
| `rail boot reset=power on … brownouts=0` | clean start on the rail |
| `rail dip mv=…` | the 5 V input fell below 4.5 V: a loose joint or plug on the power path |
| `flash refused reason=cooldown` | the guard working, not a fault |
| `cam sensor pid=0x039e … ok=1` | right camera sensor |
| `joined … ip=192.168.4.1N` | camera N is on the Core's WiFi |
| `fetch cam=N code=200 bytes=…` | photo N downloaded |
| full firmware: `err=NNN` | look it up in [LOGGING](LOGGING.md#error-codes) |
| full firmware: `evt=reset reason=brownout` | the rail sagged: [TROUBLESHOOTING](TROUBLESHOOTING.md#power) |
