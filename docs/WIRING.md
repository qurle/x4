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
