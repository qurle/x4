# Measurements

What to measure on the bench, how, and what number counts as a pass. Keep the date, the firmware
(step env or `core`/`camN`) and the git hash on every row. Estimates from the brief are in
*italics*. Recorded so far: step 15 round 1, step 11 transfers, one ping and one RSSI.

## Two kinds of measurement

- **Check (has a target).** A number to beat. If it misses, stop and fix that before you go on:
  the later steps depend on it. Example: the voltage lost along the power leads.
- **Record (no target).** A number to plan with, not a pass/fail. Write down what you see.
  Example: how much current one camera draws.

Each section below says which kind it is, what it needs (rig, firmware), the meter setting and
where the probes go.

## What to do first (rig at step 11: Core, button, flash, cams 1–3)

In this order. All of it works with the firmware that's on the boards now (`s10-core-shot` on
the Core, `s10-camN` on the cameras), except item 6.

| # | what | kind | section | time |
|---|---|---|---|---|
| 1 | voltage drop along the + and − leads at rig load | **check** | [rail and joints](#rail-and-joints) | 5 min |
| 2 | rail voltage at the blocks, idle | **check** | [rail and joints](#rail-and-joints) | 1 min |
| 3 | switch on/off 10× | **check** | [rig total](#rig-total) | 5 min |
| 4 | rig current, idle | record | [rig total](#rig-total) | 5 min |
| 5 | 10 shots: time per camera and per shot | record (target comes with the full firmware) | [shot timing](#shot-timing-target-all-4-stored--5-s) | 5 min |
| 6 | flash dip, 10 flashes 8 s apart (needs `s07-flash` on the Core: USB swap) | **check** | [flash](#flash) | 10 min |
| 7 | joints and boards after 10 min powered | **check** (warm OK, hot = stop) | [temperatures](#temperatures-die-from-logs-and-touchir-thermometer-on-the-case) | touch |
| 8 | current per board | record | [current per device](#current-per-device-5-v-rail) | 15 min |
| 9 | bank behaviour | record | [bank behaviour](#bank-behaviour) | during the session |

Later: cam 4 (repeat 1–4 and 8 with it in), the full firmware (step 13: SD speed, shot timing with
skew, temperatures from logs, UI fps), and anything that needs a scope (peaks, inrush).

## Meter basics

Full primer: [STEPS: multimeter primer](STEPS.md#multimeter-primer). The short version:

| measuring | dial | red lead in | black lead in | the meter goes… | bank |
|---|---|---|---|---|---|
| voltage | **V⎓** (DC volts). Auto, or the **20 V** range for the rail; **2 V** (or 200 mV) for drops | **VΩ** | COM | **across**: red on the point, black on the reference | in, rig on |
| current | **A⎓** (DC amps) | **10 A** jack (never mA/µA) | COM | **in series**: the current flows through it | **out** while you place or remove the probes |
| short / connected | beep 🔊 or Ω 200 Ω | VΩ | COM | across | out, capacitor empty |

- The red lead goes back to **VΩ** as soon as a current reading is done. In the 10 A jack the meter
  is a near short: touching +5 V and GND with it shorts the bank.
- A multimeter shows an average. It can't show the flash pulse, the WiFi bursts or the switch-on
  spike. Leave "peak" columns empty unless a scope or the meter's MIN/MAX measured them, and say
  which in the notes.
- Probing a Wago block: every port is in use (WIRING: connectors), so first pull the **Mini
  Button's** lead out of the block you want (power checks don't need the button). Open that
  lever, push the probe tip in, close the lever, tug gently. If the tip doesn't grip, wrap 4–5
  turns of bare solid wire around it and clamp the wire. Put the button lead back afterwards.
- Grove sockets are too small to probe. Measure on a Wago port or a loose wire end.

## Rail and joints

**Kind: check.** It proves the power gets through the leads, the switch and the Wago blocks without
losing much on the way. A bad joint loses voltage and gets warm.

**Needs:** the rig powered and running (idle is fine), switch **on**. Firmware: anything.

**Meter:** V⎓, red in VΩ. For the drops use the **2 V** range (or 200 mV, or auto): the numbers
are small, and the 20 V range shows too few digits.

**Probes:** both probes on the **same wire**, one at each end, so the meter shows only what is lost
along it ([SOLDERING: P5](SOLDERING.md#prove-the-power-path)).
- **+ side:** red on the breakout's **VBUS pad** (the solder joint of the red lead), black in the
  freed port of **+5 V block 2** (the button's block). This spans the red lead, the switch and
  that block's feed. Block 1 has its own feed: to check it too, swap the button's place with
  cam 1 for the measurement.
- **− side:** red in the freed port of **GND block 2**, black on the breakout's **GND pad**.
- **Rail voltage:** red in the freed +5 V port, black in the freed GND port.
- **Wiggle:** keep the probes on the + side and gently flex each joint, the switch and the bank
  in its socket. Use MIN/MAX if the meter has it.

**Targets (ASSUMPTION: from 20 AWG at ~1.5 A, see SOLDERING):** each drop under ~0.05 V with the
Core only, under ~0.15 V with the full rig. Over ~0.2 V: a bad joint, a loose Wago or a poor
switch contact; feel for the warm spot. Rail at the blocks 4.75–5.25 V. A negative reading only
means the probes are swapped: write down the number without the sign.

| load | + drop: VBUS pad → +5 V block (V) | − drop: GND block → GND pad (V) | rail at blocks (V) | wiggle: dips? | warm spots after 10 min | date |
|---|---|---|---|---|---|---|
| Core only (step 5), target < ~0.05 V | | | | | | |
| Core + button + flash + cams 1–3, idle (step 11 now) | | | | | | |
| full rig idle (4 cams), target < ~0.15 V | | | | | | |

## Rig total

**Kind:** record for current; **check** for the on/off test.

**Needs:** step 11 or later. Say how many cameras were on.

**Meter (current):** A⎓, red lead in the **10 A** jack, black in COM.

**Probes (current), the meter across the open switch**
([STEPS: measuring current](STEPS.md#measuring-current)):
1. Bank **out**, switch **off**. Wait ~10 s.
2. Red probe clipped (crocodile lead) on the switch's **input** terminal (where the breakout's red
   lead arrives). Black probe in a free **+5 V** Wago port.
3. Bank in (press its button if it's asleep). The switch is open, so the whole rig runs through the
   meter. Wait for the cameras to join, then read the typical value over ~10 s (WiFi makes it jump).
4. Switch **on** to bypass the meter within its time limit (read the label near the 10 A jack).
5. Done: bank **out** first, then the probes, red lead back to VΩ.

With the CC switch mod there's no switch in the +5 V line: pull the breakout's red lead out of the
+5 V block, clip the red probe to its bare end, black probe in the freed port.

Don't shoot, stream or flash with the meter in line: it costs ~0.1 V at this current (Not sure for
your meter), and a reset then may be the meter's fault, not the rig's. So the preview and shot rows
need another tool: a scope with a shunt, or a USB-C power meter between the bank and the breakout
(ASSUMPTION: that works with the bank's built-in plug, but it adds one more contact). Leave them
empty until then.

**On/off ×10 (check):** meter out, switch the rig off and on 10 times, ~5 s apart, all boards
attached. **Pass:** the bank never shuts off, no board restarts by itself (Core screen back at the
title, a camera missing from *WiFi devices*). Fail: inrush, [POWER §4](POWER.md#4-known-risks-and-mitigations).
With the bank button needed after a sleep (no CC switch yet), only count a fail when the bank goes
off *during* a switch-on, not when it was already asleep.

| condition | cameras on | average mA / W | peak mA | min rail V | notes |
|---|---|---|---|---|---|
| standby (all on, idle), *≈ 1–1.5 A for the full rig (ASSUMPTION)* | | *3–4 W* | | | |
| preview on | | | | | needs another tool (above) |
| shot without flash | | | | | needs another tool |
| shot with flash | | *7–10 W + flash* | | | needs another tool |
| inrush at switch-on (peak, duration) | | – | | | scope only |
| on/off ×10: bank shut-off Y/N, restarts Y/N | | – | – | – | |

## Current per device (5 V rail)

**Kind: record.** Tells you where the power goes and what one more camera adds.

**Needs:** step 11 or later. Idle = powered and joined, nothing happening. *Streaming* needs the
full firmware with the preview on (step 13); leave it empty before that.

**Meter:** A⎓, red lead in the **10 A** jack (never the mA jack: the Core's draw blows its fuse).

**Probes, one board at a time:**
1. Bank **out**.
2. Open the lever on that board's **red** wire in the +5 V block and take the wire out.
3. Red probe clipped to the wire's bare end, black probe in the freed +5 V port. Nothing else
   touches the clip.
4. Bank in, wait ~30 s for the board to settle (a camera joins WiFi), read the typical value.
5. Bank **out**, wire back in, lever closed, tug test. Next board.

The Core's red wire is the red of its Port A lead. Another way, when the board under test is
easy to reach: rig current with it, minus rig current without it (bank out to unplug it).

**Target:** none. Sanity: Core *≈ 0.15–0.3 A*, camera *≈ 0.15–0.2 A* (both ASSUMPTION). Far more:
check for a warm part. 0: the wire isn't in the meter's path.

| device | idle mA | streaming mA | capture/burst peak mA | WiFi TX peak mA | date / git |
|---|---|---|---|---|---|
| CoreS3-SE (UI on, AP up, no cameras) | | – | | | |
| CoreS3-SE (preview on) | | | | | |
| Cam1 | *≈ 150 (0.75 W)* | *≈ 200 (1 W)* | *≈ 300–400 (1.5 W)* | | |
| Cam2 | | | | | |
| Cam3 | | | | | |
| Cam4 | | | | | |
| Mini Button | *≈ 0.2* | – | – | – | |
| Flashlight (pulse) | 0 | – | *≈ 1000 for ≈ 100 ms* | – | |

The Mini Button's ~0.2 mA is below what the 10 A jack can resolve: leave it empty unless your meter
shows it. Flashlight idle 0 = it should read 0.00 A with the flash dark.

Runtime per bank charge ≈ usable bank energy ÷ rig total (see [POWER](POWER.md)).

## Flash

**Kind: check.** The 1 A flash pulse must not dip the rail enough to reset anything.

**Needs:** `s07-flash` on the Core (USB swap) for the 50 ms row. The 100 and 150 ms rows need the
full firmware (`flash_pulse_ms`, [CONFIG](CONFIG.md)). Cameras can stay attached.

**Read:** the *dip* line on the Core's screen after each flash: lowest 5 V reading, the reading
before, *changes/reads*. *changes* near 0 means the Core's reading is too slow to trust; then use
the meter as a hint.

**Meter (hint only):** V⎓, 20 V range, red in a free **+5 V** port, black in a free **GND** port,
**MIN/MAX** on, then flash. The meter's MIN is slower than the pulse (Not sure how much for
yours), so the real dip is lower than it shows.

**Light duration:** film the flash with the phone at 240 fps, count the lit frames, × 4.2 ms.

**Connector temperature:** after 10 flashes 8 s apart, touch the flash's Grove plug and its Wago
wires.

**Targets:** dip ≥ 4.6 V, no reset (the Core screen doesn't go back to the title, no camera drops
from *WiFi devices*). Connector warm is fine, **hot = stop**.

| pulse_ms | rail min V during pulse (`s07` dip line: min, changes/reads) | meter MIN V | resets? | light duration (slow-mo) | connector temp after 10 | date |
|---|---|---|---|---|---|---|
| 50 (test) | | | | | | |
| 100 (default) | | | | | | |
| 150 (cap) | | | | | | |

## Bank behaviour

**Kind: record.** Tells you how the bank switches itself off, so you know when it's a fault and
when it's normal ([SOLDERING: the bank keeps turning off](SOLDERING.md#the-bank-keeps-turning-off)).

**Meter:** V⎓, 20 V range. Probes on the switch's **input** terminal (red) and a free **GND** port
(black): that point is live whenever the bank is on, whatever the switch is set to.

| test | how | result |
|---|---|---|
| no-load turn-off time | switch off (nothing draws), start a timer, watch the meter fall to 0 V | ___ s / never |
| turns off when the bank moves in the breakout socket? | rig idle, gently move the bank | |
| wakes on re-plug without the button? | **yes** (owner, 2026-11-14) | yes |
| runtime from full, standby | bank charged to full, rig idle, note the time until it shuts off | *≈ 4.2 h at 3.5 W (est.)* |
| runtime from full, 1 shot / 30 s + preview | full firmware | |

## Temperatures (die, from logs; and touch/IR thermometer on the case)

**Kind:** **check** for joints and connectors (warm OK, hot = stop); record for the boards.

**Needs:** die temperatures come from the full firmware (diagnostics screen, web page, camera
`hb … temp=` log lines). With the step firmware: touch only, or an IR thermometer.

**Targets (ASSUMPTION thresholds, [POWER §4](POWER.md#4-known-risks-and-mitigations)):** die
warning at 70 °C, throttle at 80 °C. The cameras' module is rated for 65 °C **ambient**
(VERIFIED there), so a closed box matters: say whether the enclosure was on.

| device | idle °C | 10 min preview °C | after 20 shots °C | ambient °C | enclosure? |
|---|---|---|---|---|---|
| Core | | | | | |
| Cam1..4 | | | | | |
| Flashlight case | – | – | | | |
| joints, switch, Wago blocks (touch: cool / warm / hot) | | – | | | |

## Exposure (step 15)

One row per shot from the step 15 page (its table, or the `exp shot` serial lines). Light: e.g.
"living room, 2 ceiling lamps", "daylight window". Sharp = a waving hand / spinning fan looks
frozen.

Round 1 (owner, 2026-11-15, indoor light, summary only: rows not recorded):
- exposure compensation: little effect, a slight colour temperature shift;
- manual exposure (0x0133/34) takes effect only with AGC mode (0x0130) = 1;
- AGC mode 0 (auto) was brighter than AGC mode 1 with the highest exposure tried (value not
  recorded);
- gain, AE mode and clock: little effect in that light;
- 5 MP: frame 207 ms, grab 320 ms; VGA: 40 / 40 ms; 5 MP is much brighter.

Round 2 rows (the page also gives `luma` 0–255 and fps = 1000 / frame ms):

| light | MHz | size | comp | agc | gain | ae | exp | frame ms | fps | luma | motion (blurred / soft / sharp) | noise (none / some / heavy) | stripes (Y/N) | note |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| | 20 | QVGA | – | – | – | – | – | | | | | | | baseline |
| | 20 | VGA | – | – | – | – | – | | | | | | | baseline |
| | | | | | | | | | | | | | | |

**Answer here:** which lever makes motion sharper: clock N , size Y (frame time) , compensation N ,
manual exposure Y (round 1). Round 2: QVGA fps auto ___ / manual ___ ; exposure value where luma
stops rising ___ ; shortest usable exposure ___ ; useful gain range ___ ; setting survives the
switch to 5 MP ___ (Y/N); preview luma ≈ 5 MP luma ___ (Y/N); fan blades bent at 5 MP ___ (Y/N).
Best setting with a sharp hand: ___ ; at that setting the photo is ___ .

## Camera capture (from `cam-hello` bench and `evt=captured`)

**Kind: record.** JPEG size drives the transfer time.

**How:** sizes from the files on the SD card (or the `capture … bytes=` / `fetch … bytes=` serial
lines), grab ms from `capture … grab_ms=` (step 8) or `evt=captured` (full firmware). Check the
**pixel size** of one saved file on a computer: if a "5 MP" file isn't 2592×1944, the camera fell
back to another size.

| res | JPEG size (avg) | grab ms (min/max) | frame period ms | res switch ms | quality setting |
|---|---|---|---|---|---|
| QVGA 320×240 | | | | | |
| VGA 640×480 | | | | | |
| HD 1280×720 | | | | | |
| UXGA 1600×1200 | | | | | |
| FHD 1920×1080 | | | | | |
| **5MP 2592×1944** | *1–2.5 MB*; step 11 saw 137–155 kB (2026-11-15, size not checked) | | | | 12 |

## Shot timing (target: all 4 stored ≤ 5 s)

**Kind:** record with the step firmware; **check** with the full firmware: all 4 stored ≤ 5 s,
skew 50–250 ms or tighter ([BRIEF](BRIEF.md)).

**Step 11 now:** press the button 10 times, ~10 s apart. Read the screen: *camN … kB in … ms* and
*shot N: … saved, … ms*. The cameras are asked one after another, so there is no skew to measure
yet.

**Full firmware (step 13):** `python tools/x4log.py core.log --shots`, the web page, and skew by two
of the methods in [DEBUGGING: shutter skew](DEBUGGING.md#measuring-shutter-skew).

| shot | skew ms (timestamps) | skew ms (slow-mo) | shutter→captured ms | transfers (ms, kB/s each) | total ms | xfer_parallel | quality |
|---|---|---|---|---|---|---|---|
| step 11, 3 cameras, several shots (owner, 2026-11-15, main @ 875d088) | – (not synchronised) | – | – | 300–650 ms each, one after another | ≈ 1800 usually | 1 (sequential) | 12 |
| | | | | | | | |

## Throughput

**Kind: record.** Shows where the 5 s budget goes: WiFi, the camera or the SD card.

**How:**
- Ping: laptop on the rig's WiFi (`x4cam-steps` with the step firmware, `x4cam` with the full
  firmware), `ping -c 20 192.168.4.1N` (N = camera number, e.g. `192.168.4.11` for cam 1); write
  the average and the max.
- RSSI: the full firmware's web page or diagnostics screen. It's in dBm and normally negative;
  closer to 0 is stronger.
- SD speed: full firmware, web page → *SD speed test*.

| test | result | notes |
|---|---|---|
| SD write speed (bench, 1 MB, 16 KB chunks) | kB/s | `evt=bench` / web *SD speed test* |
| SD read speed | kB/s | |
| one camera → Core HTTP (5 MP) | usually 300–650 ms per camera over several shots. One shot in detail: cam 1 155 kB in 531 ms (≈ 290 kB/s), cam 2 148 kB in 304 ms (≈ 490 kB/s), cam 3 137 kB in 265 ms (≈ 520 kB/s); cam 4 not connected yet | owner, 2026-11-15, step 11 (`s10-core-shot` + `s10-camN`), main @ 875d088. Time = HTTP GET start → file closed: includes the camera's grab and the SD write, cameras asked one after another. 137–155 kB is far below the *1–2.5 MB* estimate for 5 MP at quality 12: check a saved file's pixel size |
| 2 cameras in parallel | kB/s aggregate | full firmware, `xfer_parallel` = 2 |
| 4 cameras in parallel | kB/s aggregate | full firmware, `xfer_parallel` = 4 |
| camera ping (round trip) | 18 ms (cam 1) | owner, 2026-11-15, main @ 875d088. **Not sure**: which host pinged, single value or average |
| SoftAP RSSI per camera at rig distance | 26 (as reported) | owner, 2026-11-15, main @ 875d088. `WiFi.RSSI()` is negative dBm, so this is most likely −26 dBm (very strong, camera close to the Core). **Not sure**: sign, which camera, distance. `/api/status` |
| photo save time, empty card vs ~10k files in `/x4` | ms per photo | flat folder: each new file scans the whole folder (ADR-019). `evt=done ms=` |

## Effects on the camera (STEPS step 14, planned)

**Kind: record.** No firmware yet.

| output | decode ms | crop + dither ms | encode ms | PNG kB | peak PSRAM kB |
|---|---|---|---|---|---|
| 2592×1944 portrait + dithered | | | | | |
| 1296×972 portrait + dithered | | | | | |

## UI

**Kind:** **check** against 30–60 fps ([BRIEF](BRIEF.md)); record the rest.

**Needs:** the full firmware. FPS from the diagnostics screen (long-press the shutter). Touch
latency: film the screen and your finger at 240 fps, count frames from touch to the first change,
× 4.2 ms.

| screen / animation | avg fps | worst frame ms | date / git |
|---|---|---|---|
| splash | | | |
| main idle | | | |
| main + preview | | | |
| shutter moment | | | |
| saving (4 progress rings) | | | |
| diagnostics | | | |
| touch-to-feedback latency (slow-mo) | ms | | |
