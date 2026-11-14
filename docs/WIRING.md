# Wiring

> ### ⚠ One 5 V source per board, always
> **Before you plug USB-C into the Core or a camera: switch the rail OFF and unplug that board's
> Grove power lead.** Reconnect the rail only after the USB cable is out. On the Core, USB 5 V and
> the rail meet inside the board (see [POWER.md §2c](POWER.md#2-feeding-the-core-through-port-a-brief-32)).
> On a camera the Grove port *is* the USB port, so it cannot be on both anyway.
> Full procedure: [BRINGUP.md](BRINGUP.md#flashing-without-breaking-the-power-rules).

## Diagram

```mermaid
flowchart LR
  BANK["Anker Nano 5000<br/>USB-C 5 V ⎓ 3 A"] -->|USB-C cable| BRK["USB-C breakout #1<br/>(CC 5.1 kΩ!)"]
  BRK -->|"+5 V (red)"| IN["IN node<br/>Wago 221, 3 ports<br/>(1 free = meter)"]
  IN --> SW["power switch<br/>≥ 5 A DC"]
  BRK -->|"GND (black)"| GRAIL
  SW --> PRAIL["+5 V rail<br/>Wago 221 blocks, jumpered<br/>(1 port free = meter)"]
  subgraph rails[" "]
    PRAIL
    GRAIL["GND rail<br/>Wago 221 blocks, jumpered"]
  end
  CAP["2200 µF ≥10 V<br/>stripe → GND"]
  PRAIL --- CAP --- GRAIL
  PRAIL -->|red| C1[Cam1] & C2[Cam2] & C3[Cam3] & C4[Cam4]
  GRAIL -->|black| C1 & C2 & C3 & C4
  PRAIL -->|"red 20 AWG"| FL[Flashlight Unit]
  GRAIL -->|"black 20 AWG"| FL
  PRAIL -->|red| BTN[Mini Button]
  GRAIL -->|black| BTN
  PRAIL -->|"red"| PA["Core Port A<br/>(split Grove)"]
  GRAIL -->|"black"| PA
  FL -->|"yellow = EN"| PA
  BTN -->|"white = signal"| PA
  C1 & C2 & C3 & C4 -.-x NC["not connected: yellow + white of every camera cable<br/>(= the camera's USB D+/D−): cut short, insulate, tape"]
```

ASCII version (same thing):

```
                                          ┌──────── +5V rail (Wago 221 blocks, jumpered) ──────┐
Bank─USB-C─[breakout]─red─[IN: meter]─[SWITCH]─┤ cam1 cam2 cam3 cam4  core  flash  button  cap(+)  meter │
                  └─black─────────────────────┤ cam1 cam2 cam3 cam4  core  flash  button  cap(−)  spare │
                                              └──────── GND rail (Wago 221 blocks, jumpered) ──────┘
IN = one small Wago 221 between breakout and switch. Its free port and the +5V rail's free port
are where the multimeter goes for current (STEPS.md#measuring-current).

Core Port A (one Grove plug, wires split):
   red   ── +5V rail          (Core power IN)
   black ── GND rail
   yellow (G2) ── Flashlight yellow (EN)
   white  (G1) ── Mini Button white (signal)

Each camera Grove cable:  red → +5V rail, black → GND rail, yellow + white → cut short, insulate, tape.
Flashlight: red/black on their own 20 AWG branch to the rail (1 A pulses), white NC.
Mini Button: red → +5V rail, black → GND rail, white → Core Port A white. (yellow NC)
```

## Pin table

| from | wire | to | net | notes |
|---|---|---|---|---|
| bank USB-C | cable | breakout #1 USB-C | – | breakout needs 5.1 kΩ CC pull-downs (or use bank USB-A + A-to-C cable) |
| breakout V/VBUS | red 20 AWG | IN node (Wago 221) | +5 V, before the switch | **live whenever the bank is plugged in**, switch on or off |
| IN node | red 20 AWG | switch → +5V rail | +5 V | switch in +5 V only; IN keeps one port free for the meter |
| breakout G/GND | black 20 AWG | GND rail | GND | |
| Cam1..4 Grove red | red | +5V rail | +5 V | one branch each (star) |
| Cam1..4 Grove black | black | GND rail | GND | |
| Cam1..4 Grove yellow | yellow | **nothing** | USB D+ (G20) | cut and tape |
| Cam1..4 Grove white | white | **nothing** | USB D− (G19) | cut and tape |
| Core Port A red | red | +5V rail | +5 V (`BUS_OUT`) | Core power **input** |
| Core Port A black | black | GND rail | GND | |
| Core Port A yellow (G2) | yellow | Flashlight Grove yellow | flash EN | 3.3 V logic; unit has 10 k pull-down |
| Core Port A white (G1) | white | Mini Button Grove white | button signal | active LOW, internal pull-up on |
| Flashlight red | red → 20 AWG | +5V rail | +5 V | own branch, ≤ 30 cm |
| Flashlight black | black → 20 AWG | GND rail | GND | |
| Flashlight white | white | nothing | NC | |
| Mini Button red | red | +5V rail | +5 V | see HARDWARE.md: released level 3.3 V |
| Mini Button black | black | GND rail | GND | |
| Mini Button yellow | yellow | nothing | NC | |
| 2200 µF + | leg | +5V rail | +5 V | **parallel**, stripe side to GND |
| 2200 µF − (stripe) | leg | GND rail | GND | |

Colour convention: **red = +5 V, black = GND, yellow = signal/data (EN on the flash, D+ on
cameras), white = second signal (button, D− on cameras)**. Mark the cut camera data wires with
tape so nobody "fixes" them later.

## Connectors

The rail uses **Wago 221 lever connectors** (owner, 2026-11-13; it was 2273 push-in before).
- ASSUMPTION (Wago's spec as remembered, check the print on yours): the 221-41x series takes
  **solid, stranded and fine-stranded** wire, 0.2–4 mm², strip length 11 mm, in 2-, 3- and 5-port
  sizes. So the 20 AWG stranded wire (0.52 mm²) goes straight in: no pigtails, no ferrules.
- **Grove wires are thin.** Their gauge is Not sure (often quoted as 26–28 AWG, 0.08–0.13 mm²),
  which would be under the 221's 0.2 mm² minimum. Strip ~22 mm and fold the bare end double,
  or keep a solid/20 AWG pigtail soldered on. Then tug-test.
- **Tug-test every connection** with the lever closed. Connection quality, not gauge, is the
  limit. Do not tin stranded wire for clamp terminals (solder creeps, the contact loosens).
- How to solder the breakout leads, splices and pigtails, and how to check them:
  [SOLDERING.md](SOLDERING.md).
- **Ports.** Each net has 9 connections (source, 4 cams, Core, flash, button, cap). The +5 V net
  needs one more free port for the meter, so 10. Join blocks with short jumpers: two 5-port
  blocks give 8 usable ports (2 go to the jumper), three give 11. The **IN node** is one more
  small block (3 ports: breakout, switch, meter).
- The jumpers carry the whole rig current: same 20 AWG, short, tug-tested.

## Switching the rig on and off

**The problem (owner, 2026-11-14):** flipping the rail switch on doesn't start the bank once it
has gone to sleep; its button has to be pressed too. The button can't turn the bank **off**
either.

**Why** (the owner's re-plug test below confirms it for the A1653):
- A USB-C power bank switches its output on when it sees a device plug in, and off when it
  unplugs. It sees that through the CC resistors (5.1 kΩ to GND). **VERIFIED:** USB-C sources
  apply VBUS only after detecting an attached sink and remove it after a detach. Source: the
  USB-C spec timing, as mirrored by the Zephyr USB-C constants, see
  [SOLDERING](SOLDERING.md#sources).
- The breakout's CC resistors sit on the bank's side of the switch. So the bank sees "plugged
  in" all the time, switch on or off.
- With the switch off nothing draws current, and the bank's low-current auto-off stops the
  output. Anker's general articles say about 30–90 mA keeps a bank on (*Not sure* for the A1653).
- Switching on later changes nothing the bank can see: no new plug-in. So it stays off until its
  button is pressed.

**The re-plug test** (proves a CC switch will work, no parts needed): leave the bank in the
breakout with nothing drawing until it sleeps (0 V at the VBUS pad or IN node), then unplug and
re-plug it **without** pressing the button. A physical re-plug is the same "plug-in" signal a CC
switch makes. **Result (owner, 2026-11-14): 5 V comes back by itself**, so a fresh plug-in wakes
the A1653 and the CC switch (option B) will work.

Three ways to deal with it:

| option | actions to switch on / off | standby drain | build |
|---|---|---|---|
| **C. Bank button** (today) | rail switch on + bank button / rail switch off | ≈ 0 | nothing |
| **A. Keep-alive resistor** | rail switch / rail switch | **≈ 0.5 W, all the time** | one resistor |
| **B. CC switch** | one switch / the same switch | ≈ 0 | small breakout mod |

### A. Keep-alive resistor (evaluated: not recommended)

A resistor that is always connected **before** the switch keeps a load on the bank, so it never
hits its low-current auto-off. The switch then only connects the rig.

```
Bank─USB-C─[breakout]─red─[IN node]─────[SWITCH]─── +5V rail
                              │
                           47 Ω ≥ 2 W   (the keep-alive load, always on)
                              │
                  └─black─────┴──────────────────── GND rail
```

Wiring: one 47 Ω resistor rated **≥ 2 W** (1 W minimum) between a spare IN-node port and the GND
rail. Use a 5-port IN block so the meter port stays free. Keep it off the foam board and plastic
with an air gap: it runs warm.

**Power draw** (5 V rail; bank energy as in [POWER §3](POWER.md#3-power-numbers-all-estimates-until-measured):
14.8 Wh usable, ASSUMPTION):

| | value |
|---|---|
| current | 5 V / 47 Ω ≈ **106 mA**: above Anker's 30–90 mA (*Not sure* it's enough for the A1653) |
| power, all the time | 5² / 47 ≈ **0.53 W** |
| a full bank, rig switched off | flat in about **28 h** (14.8 Wh / 0.53 W) |
| runtime while shooting, ~3.5 W rig | 4.2 h → **≈ 3.7 h** (−12 %) |
| runtime at ~7 W | 2.1 h → **≈ 2.0 h** (−7 %) |
| heat | 0.53 W in one resistor, continuously |

A smaller load (e.g. 100 Ω ≈ 50 mA, 0.25 W, ~59 h) wastes less, but may sit under the bank's
threshold (*Not sure*). Some keep-alive designs pulse the load instead (e.g. 100 mA for a split
second every few seconds) to waste far less. But nobody knows the A1653's timing, and that's a
timer circuit, more work than option B.

**Verdict:** it works only while the bank is plugged in. It quietly empties a full bank in about
a day, so you'd unplug the bank after every session anyway, which is no better than today.

### B. CC switch (recommended: one switch does everything)

Proven to work on the A1653 by the re-plug test above. Move the switch from the 5 V line to the CC resistors. **Switch off** takes the "device present"
resistors away, so the bank sees an unplug and turns its output off by itself. **Switch on** puts
them back, so the bank sees a fresh plug-in and turns on, every time, with no button.

```mermaid
flowchart LR
  BANK["Anker bank<br/>(USB-C source)"] -->|"USB-C plug"| REC["breakout socket"]
  REC -->|"VBUS"| IN["IN node"] -->|"short jumper wire<br/>(was: the switch)"| PRAIL["+5 V rail"]
  REC -->|"GND"| GRAIL["GND rail"]
  REC -->|"CC1"| R1["5.1 kΩ"]
  REC -->|"CC2"| R2["5.1 kΩ"]
  R1 --> S(("join"))
  R2 --> S
  S --> SW["rig switch<br/>(any small switch)"]
  SW --> GRAIL
```

The breakout, before and after (schematic; where the parts sit on your board is *Not sure*):

```
BEFORE (stock breakout)                    AFTER (CC switch mod)

 CC1 ──[5.1k]── GND                         CC1 ──[5.1k]──┐
 CC2 ──[5.1k]── GND                         CC2 ──[5.1k]──┤
                                                          └──o/ o── GND
 VBUS ── red ── IN ──[SWITCH]── +5V rail                 rig switch
 GND  ── black ───────────────── GND rail   VBUS ── red ── IN ──[jumper]── +5V rail
                                            GND  ── black ─────────────── GND rail
                                            (on-board 5.1k resistors removed or cut free)
```

**Parts:** the rig switch you have (now it carries only a fraction of a milliamp, ASSUMPTION; any
small switch works), thin wire, heat-shrink. Resistors: either **reuse the breakout's own two
5.1 kΩ** (free GND end of each, wire both freed ends to the switch: nothing to buy), or two new
5.1 kΩ (any size and wattage) if lifting the on-board ones is too fiddly on your board.

**Steps** (practice on a **spare** breakout first; bank unplugged):
1. **Find the on-board CC resistors.** Ohms, breakout unplugged: CC1 pad → GND and CC2 pad → GND
   each read ≈ 5.1 kΩ. Look for two small resistors marked **512** (5.1 kΩ in the 3-digit code)
   near the socket. Which end goes to CC and which to GND: *Not sure* for your board, so trace it
   with the meter's beep.
2. **Disconnect them from GND.** Either lift one end of each resistor with the iron, or cut the
   track between the resistor and GND with a sharp knife. Afterwards CC1 → GND and CC2 → GND must
   read **OL**.
3. **Reusing the on-board resistors?** Solder a thin wire to each freed GND-side end and join the
   two wires: that join is your switch point; skip to step 4. **Otherwise add new resistors:**
   one 5.1 kΩ from CC1, one from CC2. Solder to the CC pads, or to the
   CC end of the old resistors if the board has no CC pads. Join their free ends in one
   heat-shrunk joint.
4. **Wire the switch:** from that join to the switch, and from the switch to GND (a GND pad or
   the GND rail). Tie the thin wires down next to the breakout (strain relief,
   [SOLDERING R2](SOLDERING.md#r2-wire-to-a-breakout-pad-vbus-gnd)).
5. **Replace the old switch in the 5 V line** with a short 20 AWG jumper wire from the IN node to
   the +5 V rail.

**Check** (bank unplugged, ohms):

| switch | CC1 → GND | CC2 → GND |
|---|---|---|
| off | OL | OL |
| on | ≈ 5.1 kΩ | ≈ 5.1 kΩ |

Then with the bank in and no boards on the rail: switch **off** → rail ≈ 0 V; switch **on** →
4.75–5.25 V within a moment. A USB-C source waits at least ~100 ms after a plug-in before it
switches power on, and has to drop power within 650 ms of an unplug (spec timing via Zephyr's
constants; the A1653's own timing is *Not sure*). Do it 10 times. Then leave it switched off for
longer than the bank's sleep time and switch on again: it must come on with no button.

**Things that change with the mod:**
- **Current measurement:** the meter used to go across the switch. Now it goes in place of the
  IN → +5 V jumper: switch off (bank output off), take the jumper out, meter probes into the two
  free ports, switch on, read, switch off, jumper back ([STEPS: measuring current](STEPS.md#measuring-current)).
- **Turn-on:** the bank needs a moment (≥ ~100 ms) after the switch before the rail is live.
  Harmless.
- **Not sure:** whether the A1653 likes being "plugged in" and "unplugged" many times a day this
  way. It's the same as plugging it in, which it's built for.
- Two separate 5.1 kΩ resistors (not one shared one) keep each CC pin looking like a standard
  device. With a 2-pole switch you could switch each resistor on its own pole; one pole after
  the join works too (ASSUMPTION).

**Before building:** your breakout's markings decide where steps 1–3 happen. Write them in
[HARDWARE: recorded on bring-up](HARDWARE.md#recorded-on-bring-up-fill-in).

## Spare ports and future inputs

| resource | where | state | intended use |
|---|---|---|---|
| G1 | Port A white | **used** | shutter button |
| G2 | Port A yellow | **used** | flash EN |
| G7 | M-Bus pin 22 | free | **fallback flash EN** (`kFlashEnAlt`) if G2 idles high |
| G8 | M-Bus pin 4 | free | feature button 1 (`-DX4_EXTRA_BUTTONS=1`) |
| G9 | M-Bus pin 10 | free | feature button 2 |
| G5, G6 | M-Bus pins 8, 21 | free | **quadrature scroll wheel** (`-DX4_ENCODER_QUADRATURE=1`, driver stub in `core/src/input/inputs.cpp`) |
| G17, G18 | M-Bus pins 16, 15 | free | UART or two more buttons |
| G10 | M-Bus pin 2 | free | ADC (e.g. rail voltage divider later) |
| 5 V / GND | M-Bus 28 / 1,3,5 | rail node | do not draw extra loads through the Core |
| 3V3 | M-Bus 12 | Core's 3.3 V | low-current pull-ups only |

Dupont jumpers fit the M-Bus header (2×15, 2.54 mm, VERIFIED M5 Bus Socket spec). Buttons on
M-Bus: one leg to the GPIO, other leg to a GND pin; the firmware uses the internal pull-up.

**I2C encoder/scroll units** (e.g. an M5 encoder unit on Grove) need SDA/SCL. On the SE the only
Grove I2C pins are Port A G2/G1, which x4 uses for the flash and the button. To add an I2C unit:
move the flash EN to G7 and the button to G8 (M-Bus), free Port A G1/G2 for I2C, and power the
unit from the rail (its own branch). The firmware has the pins in `board_pins.h`; the input layer
maps the new source to actions with no UI change ([FIRMWARE.md](FIRMWARE.md#input)).

## Future low-latency trigger (documented, not built)

A hardwired trigger line from a Core GPIO to every camera would cut skew to microseconds. On the
CamS3 the only external pins are the Grove yellow/white = **G20/G19 = USB D+/D−**. Using them as
a trigger means losing USB serial/flashing on that pin pair while the line is attached, and a
wrong level during boot could put the USB PHY in a bad state. If ever built: series 1 kΩ per
camera, use the white (G19) line, and unplug it before flashing.

## Printable pre-power checklist

```
x4 PRE-POWER CHECKLIST                           date ______  by ______
[ ] Switch OFF. Bank unplugged.
[ ] Rail V ≈ 0 first. Beep/ohms +5V rail ↔ GND rail: silent or a chirp (OL or climbing), never a steady beep / < ~5 Ω.
[ ] Beep/ohms each red wire ↔ +5V rail and each black ↔ GND rail: solid beep (≈ 0 Ω), also while flexed.
[ ] Capacitor stripe (−) on GND rail. Rated ≥ 10 V.  Value ____ µF  ____ V
[ ] Camera yellow + white wires cut, insulated, taped (×4).
[ ] Core Port A: red→+5V, black→GND, yellow→flash yellow, white→button white.
[ ] Flash on its own 20 AWG branch. Flash slide switch on FLASH.
[ ] No USB-C cable in any board.
[ ] Every Wago lever closed, every wire tug-tested (Grove ends folded double or pigtailed).
[ ] IN node: breakout red + switch wire, one port free. +5V rail: one port free.
[ ] Bank plugged in, switch ON with NO boards plugged: rail = 4.75–5.25 V  (measured ____ V)
[ ] Then follow BRINGUP.md step 4 (one load at a time).
```
