# Soldering: the rig's joints, and proving they carry power

You need about 20 solder joints for the whole rig, and none of them are on the M5 boards. They
are wires to wires, two wires to the USB-C breakout, and maybe two to the switch. This page shows
how to make each kind, how to tell a good joint from a bad one, and how to prove the power path
works **before** anything valuable is connected to it.

New to soldering? Do [Workshop 0](STEPS.md#workshop-0-learn-to-solder) of the play-through
first: practice on scrap wire and on one of the spare breakouts (you have five and use one).

## What gets soldered

| # | joint | wire | play-through |
|---|---|---|---|
| 1 | breakout #1 **VBUS** pad → red lead | 20 AWG red | [Workshop B](STEPS.md#workshop-b-the-power-rail) |
| 2 | breakout #1 **GND** pad → black lead | 20 AWG black | Workshop B |
| 3–4 | power switch lugs (only if your switch has solder lugs) | 20 AWG red | Workshop B |
| 5–6 | Port A cable red/black → solid pigtails | Grove → 20–22 AWG solid | [Workshop A](STEPS.md#workshop-a-the-port-a-cable) |
| 7–8 | button red/black → pigtails | Grove → solid | [Workshop C](STEPS.md#workshop-c-button-lead) |
| 9 | button white ↔ Port A white | Grove ↔ Grove | Workshop C |
| 10–11 | flash red/black → 20 AWG branch | Grove → 20 AWG | [Workshop D](STEPS.md#workshop-d-flash-branch) |
| 12 | flash yellow ↔ Port A yellow | Grove ↔ Grove | Workshop D |
| 13–20 | camera leads red/black → pigtails (×4) | Grove → solid | [Workshop E](STEPS.md#workshop-e-camera-leads) |
| – | stranded 20 AWG ends going into a Wago 2273 | → solid pigtail, or use a Wago 221 instead | wherever it happens |

Why pigtails at all: the Wago 2273 blocks are made for **solid** wire only (VERIFIED, Wago data
sheet, see [WIRING: connectors](WIRING.md#connectors)). Stranded and Grove wire may not grip.

## Safety (read once)

- **Nothing powered while you solder.** Bank unplugged from the breakout, no USB in any board.
  Solder cables, not boards: unplug the Grove cable from its unit before you work on its far end.
- The tip is at 300–400 °C. Put the iron back in its stand every time. **Never catch a falling
  iron.**
- Flux smoke irritates: work by an open window, or with a small fan pulling the smoke away.
- Leaded solder: wash your hands afterwards, no food or drink at the bench.
- Glasses on when you clip wire (ends fly).
- Heat-shrink: heat gun, or the side of the iron's barrel (not the tip). A lighter works but
  leaves soot.
- When you're done: iron off, let it cool on the stand.

## Kit

| item | why | notes |
|---|---|---|
| temperature-controlled iron | steady heat; 20 AWG wire sinks heat fast | a screwdriver/chisel tip 2–3 mm wide moves heat better than a needle tip |
| solder, rosin core, 0.6–0.8 mm | | **leaded 60/40 or 63/37** is the easy one for a first project (lower melting point, flows better). Lead-free works but needs more heat. Use the temperature printed on the spool |
| flux pen | makes solder flow onto old or oxidised wire and pads | rosin / "no-clean" |
| brass wool or a damp sponge | keeps the tip clean and shiny | |
| something to hold work still | a joint that moves while it cools is a bad joint | helping hands, blu-tack, tape on the table, a clamp |
| heat-shrink 2–3 mm and 4–6 mm | insulation over every joint | cut ~2 cm pieces |
| wire stripper, flush cutters | | |
| multimeter with continuity beep | every check below | |
| optional: a 47 Ω resistor rated ≥ 1 W | dummy load for the power check | 5 V / 47 Ω ≈ 0.1 A, ≈ 0.5 W: it gets warm. Not needed if you skip to the Core as the load |

**Temperature:** start around **340–370 °C** for leaded solder. Adafruit's guide uses 370 °C for
60/40 and 400 °C for lead-free on a temperature-controlled iron, and below ~380 °C the tip lasts
longer. If the solder melts slowly and looks grainy, go up 20 °C. If the insulation shrinks back a
lot or the flux burns off instantly, go down.

## The one technique

**Heat the joint, and feed the solder to the joint, not to the iron.**

1. Clean, shiny tip: wipe it, then melt a little solder on it ("tinning the tip"). A dull tip
   doesn't pass heat.
2. Hold the parts still.
3. Touch the tip so it touches **both** parts (wire and pad, or both wires).
4. After 1–2 s, feed solder into the spot where the parts meet, on the side away from the iron.
   It should melt and run into the joint by itself.
5. Take the solder away, then the iron. A whole joint takes **about 3 s**. Much longer than that
   means not enough heat: clean the tip, add flux, or turn it up.
6. **Don't move anything for 2–3 s** while it sets.

## Recipes

### R1. Tin a wire

Strip 5–8 mm, twist the strands tight, add flux, heat the wire and let solder soak into it. Stop
as soon as it's wet all round. A tinned end is stiff and silver with the strands still visible.

Tin wire **only** where it gets soldered. A tinned end must **not** go into a push-in terminal:
the solder creeps under pressure and the contact loosens ([WIRING](WIRING.md#connectors)).

### R2. Wire to a breakout pad (VBUS, GND)

1. Find the pads. On USB-C breakouts they're usually marked **VBUS** (or V, 5V) and **GND** (or
   G), next to **CC1, CC2, D+, D−** (*Not sure* about yours: write the markings in
   [HARDWARE](HARDWARE.md#recorded-on-bring-up-fill-in)). Only VBUS and GND get wires. Leave CC
   alone: the CC resistors are already on the board and they're what makes the bank switch on.
2. Hold the breakout down, connector side away from you. Keep the iron off the plastic USB-C
   socket.
3. Strip 3–4 mm of the 20 AWG wire and tin it (R1). Add a little solder to the pad too.
4. **Through-hole pad:** push the wire through from the top, solder on the bottom. **Flat pad:**
   lay the tinned wire on the tinned pad, press the iron on top for 1–2 s until both melt
   together.
5. **Strain relief:** tie or hot-glue both wires to the board (or to its mounting base) 1–2 cm
   from the pads. A pulled wire otherwise rips the pad off the board.
6. Check: no beep between **VBUS and GND**, and none between VBUS and CC1 / CC2 / D+ / D−.

### R3. Splice and pigtail (wire to wire)

1. **Slide the heat-shrink on first**, a good 3 cm away from where you'll solder (or it shrinks
   early from the heat). Forgetting it is the classic mistake: check before every joint.
2. Strip 8–10 mm on both wires. Grove wires are thin: strip gently and don't nick the strands.
3. Twist them together (lineman twist), or lay them side by side overlapping.
4. Flux, heat, solder: the solder should soak through all the strands.
5. Let it cool fully, then slide the shrink over the joint and shrink it.

The Grove wire's insulation melts quickly. Be fast, and give the iron to the thicker wire.

### R4. Switch lugs

Hook the tinned wire through the lug and squeeze it closed with pliers so it holds by itself.
Solder, then shrink a piece of tube over the lug. Switch lugs sit in plastic, so be quick:
3 s, then wait before the next lug.

## Is the joint good?

Check every joint before you shrink it.

| look / test | good | bad → redo |
|---|---|---|
| shape | smooth, concave, solder flows onto both parts, you can still see the wire's shape | a ball sitting on top (didn't wet), a blob, solder only on the wire and not the pad |
| surface | shiny (leaded) or satin (lead-free) | dull, grainy, cracked: it moved while cooling, or was too cold. Reheat with a touch of flux |
| strands | none sticking out | a stray strand: it can short to the neighbour, so cut it and reheat |
| insulation | ends within ~1–2 mm of the joint | melted back a lot: too hot or too slow |
| pad | flat and attached | lifted or wobbling: stop, use a spare breakout |
| tug | a gentle pull doesn't move it | moves or cracks |
| beep | end to end beeps | no beep, or crackles on and off when you flex it |
| no bridge | no beep to anything it shouldn't touch | beeps to a neighbour: remove the bridge (flux + clean iron drags it away) |

Adafruit's guide has photos of each failure: cold, disturbed, overheated and insufficient wetting
([common problems](https://learn.adafruit.com/adafruit-guide-excellent-soldering/common-problems)).

## Prove the power path

This is how you know the power actually gets through, and that it **stays** there. Do P1–P4 with
**no M5 boards connected**. P5 happens later, with real loads.

| stage | setup | how | pass |
|---|---|---|---|
| **P1** bank | bank charged | press its button, look at its indicator | it shows charge. How the A1653 shows its output is on: *Not sure* |
| **P2** bare breakout | bank → breakout #1, **no wires soldered yet** | DC V, probes held on the VBUS and GND pads | 4.75–5.25 V. Try **both plug orientations** and note which give 5 V. Wait 60 s and note whether it drops to 0 V by itself (that's the bank's no-load shut-off, not your fault) |
| **P3** leads | the two leads soldered (R2), bank unplugged | beep: pad ↔ far end of each lead; **no** beep VBUS ↔ GND | beeps / no beep |
| **P4** leads, powered | bank in; leads' far ends apart and taped. Optional: the 47 Ω load across the ends | DC V at the far ends of the leads | the same voltage as P2 |
| **P4b** wiggle test | as P4. Use the meter's MIN/MAX mode if it has one | watch the reading while you flex each joint, each wire, and gently move **the bank in the socket** | the reading never drops. It drops when you move a wire → that joint. It drops when you move the bank → see below |
| **P5** drop under load | rail built, Core on it ([step 5](STEPS.md#5-core-on-the-rail)); later the full rig ([step 11](STEPS.md#11-four-cameras)) | **both probes on the same net**: red on the breakout VBUS pad, black on the +5 V Wago block. Then the same on GND (red on the GND Wago, black on the breakout GND pad) | under ~0.05 V each at Core load, under ~0.15 V each with the full rig. More than ~0.2 V: a bad joint, a loose Wago or a poor switch contact. Feel for the warm spot |

Why P5 works: both probes sit on one wire, so the meter shows only the voltage lost along it.
20 AWG loses about 33 mΩ per metre, so 30 cm at 1.5 A is about 15 mV. A bad joint of 0.1 Ω
costs 150 mV at the same current and gets warm. These thresholds are an ASSUMPTION: write down
what you measure in [MEASUREMENTS](MEASUREMENTS.md#rail-and-joints).

Don't connect a camera until P4b passes. A flickering rail makes boards reset at random, and a
reset in the middle of an SD write can leave a broken file.

## "The bank keeps turning off"

When exactly it turns off tells you why.

| it turns off… | most likely | test | fix |
|---|---|---|---|
| **the moment the bank or its plug moves** | the USB-C plug loses contact in the breakout's socket. The bank sees a disconnect and switches its output off: a Type-C source removes VBUS once the sink detaches, and only switches it on again after it has seen the device reconnected for ≥ 100 ms (secondary sources, see below). The A1653's **built-in plug** hangs the whole bank off a small breakout socket, which levers it | P4b: move only the bank | fix the breakout to something solid (screw, zip tie or hot glue on the foam board) and support the bank's body so it doesn't hang off the plug. Or use a short C-to-C cable, which works because the breakout has CC resistors. If it doesn't come back by itself, press the bank's button (*Not sure* the A1653 needs that) |
| the moment you **touch a wire** to the rail or a pad | a short (probe or wire slipping between VBUS and GND), or **inrush**: the empty 2200 µF capacitor pulls a big current spike and trips the bank's over-current protection | beep VBUS ↔ GND: no beep? Then try once **without the capacitor** | solder the joints instead of holding wires. If the capacitor trips it: [POWER §4](POWER.md#4-known-risks-and-mitigations) (pre-charge resistor or a smaller capacitor) |
| after **seconds or minutes with nothing (or almost nothing) connected** | the bank's low-current auto-off. Anker says its banks need a device to draw at least about **30–90 mA** to keep going; that's a general Anker article, so it's *Not sure* for the A1653 | P2: time it with no load; then repeat with the 47 Ω load | normal, not a fault. The Core alone draws ~150–300 mA (ASSUMPTION), well above that. Record the time in [MEASUREMENTS](MEASUREMENTS.md#bank-behaviour) |
| **never comes on** in one plug orientation | one CC resistor missing or wrong (you measured CC2 ≈ 9.2 kΩ, but with the bank connected, which isn't a valid ohms reading) | re-measure CC1 / CC2 → GND **unplugged** | use the orientation that works and mark it. Or use the bank's other port with a cable (*Not sure* the A1653 has one) |
| at **rig switch-on**, with all boards | inrush of the capacitor plus six loads starting | step 11's on/off ×10 test | [POWER §4](POWER.md#4-known-risks-and-mitigations) |

## Write it down

- Breakout pad markings, which orientations give 5 V, and the bank's no-load turn-off time:
  [HARDWARE: recorded on bring-up](HARDWARE.md#recorded-on-bring-up-fill-in).
- P5 drops: [MEASUREMENTS: rail and joints](MEASUREMENTS.md#rail-and-joints).

## Sources

- Adafruit, *Guide to Excellent Soldering*: preparation, tip tinning, temperatures, and the common
  problems pages. https://learn.adafruit.com/adafruit-guide-excellent-soldering
- SparkFun, *Working with Wire: how to splice wires*.
  https://learn.sparkfun.com/tutorials/working-with-wire/how-to-splice-wires
- r/AskElectronics wiki, soldering section (tip oxidation above ~380 °C).
- USB Type-C attach/detach timing, secondary sources mirroring the spec's timing tables: Zephyr
  USB-C API constants (`TC_T_CC_DEBOUNCE_MIN_MS = 100`, `TC_T_VBUS_OFF_MAX_MS = 650`),
  https://docs.zephyrproject.org/latest/doxygen/html/usbc__tc_8h.html. Check the USB Type-C
  specification itself before relying on exact values.
- Anker support, *What is Trickle Charging Mode?* and *How can I charge low-current devices?*
  (the 30–90 mA minimum and the auto-shutoff). These are general articles, not A1653-specific; the
  A1653 manual couldn't be read from the build environment.
  https://service.anker.com/article-description/What-is-Trickle-Charging-Mode
- Wago 2273 data (solid conductors only): see [POWER: sources](POWER.md#6-sources).
