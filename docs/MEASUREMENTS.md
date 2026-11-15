# Measurements

Mostly empty still: so far step 15 round 1 and a few rows in Throughput. Fill the tables with a multimeter (current:
across the open rail switch, [STEPS: measuring current](STEPS.md#measuring-current)), the
serial log (`tools/x4log.py`) and the web page. Keep the date and
firmware git hash on every row. Estimates from the brief are in *italics*.

Tools: multimeter with a 10 A jack (MIN/MAX helps), phone with 240 fps slow
motion, optionally a cheap scope for the flash and rail sag. A multimeter gives averages: leave
peak and min columns empty unless a scope or MIN/MAX measured them (say which in notes).

## Current per device (5 V rail)

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

Runtime per bank charge ≈ usable bank energy ÷ rig total below (see [POWER](POWER.md)).

## Rig total

| condition | average mA / W | peak mA | min rail V | notes |
|---|---|---|---|---|
| standby (all on, idle) | *3–4 W* | | | |
| preview on | | | | |
| shot without flash | | | | |
| shot with flash | *7–10 W + flash* | | | |
| inrush at switch-on (peak, duration) | – | | | bank shut off? Y/N |

## Rail and joints

Voltage drop along each side of the power path, both probes on the same net
([SOLDERING: P5](SOLDERING.md#prove-the-power-path)). Thresholds are an ASSUMPTION; record what
you see.

| load | +5 V: breakout VBUS pad → +5 V Wago (V) | GND: GND Wago → breakout GND pad (V) | wiggle test: dips | warm spots after 10 min | date |
|---|---|---|---|---|---|
| Core only (step 5), target < ~0.05 V | | | | | |
| full rig idle (step 11), target < ~0.15 V | | | | | |

## Flash

| pulse_ms | rail min V during pulse (`s07` dip line: min, changes/reads) | resets? | light duration (slow-mo) | connector temp after 10 | date |
|---|---|---|---|---|---|
| 50 (test) | | | | | |
| 100 (default) | | | | | |
| 150 (cap) | | | | | |

## Bank behaviour

| test | result |
|---|---|
| auto-off threshold (lowest current that keeps it on) | |
| no-load turn-off time (nothing connected) | |
| turns off when the bank moves in the breakout socket? | |
| does it wake on plug-in / need a button press? | |
| runtime from full, standby | *≈ 4.2 h at 3.5 W (est.)* |
| runtime from full, 1 shot / 30 s + preview | |

## Temperatures (die, from logs; and touch/IR thermometer on the case)

| device | idle °C | 10 min preview °C | after 20 shots °C | ambient °C | enclosure? |
|---|---|---|---|---|---|
| Core | | | | | |
| Cam1..4 | | | | | |
| Flashlight case | – | – | | | |

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

| res | JPEG size (avg) | grab ms (min/max) | frame period ms | res switch ms | quality setting |
|---|---|---|---|---|---|
| QVGA 320×240 | | | | | |
| VGA 640×480 | | | | | |
| HD 1280×720 | | | | | |
| UXGA 1600×1200 | | | | | |
| FHD 1920×1080 | | | | | |
| **5MP 2592×1944** | *1–2.5 MB*; step 11 saw 137–155 kB (2026-11-15, size not checked) | | | | 12 |

## Shot timing (target: all 4 stored ≤ 5 s)

From `python tools/x4log.py core.log --shots` and the web page.

| shot | skew ms (timestamps) | skew ms (slow-mo) | shutter→captured ms | transfers (ms, kB/s each) | total ms | xfer_parallel | quality |
|---|---|---|---|---|---|---|---|
| step 11, 3 cameras, several shots (owner, 2026-11-15, main @ 875d088) | – (not synchronised) | – | – | 300–650 ms each, one after another | ≈ 1800 usually | 1 (sequential) | 12 |
| | | | | | | | |

## Throughput

| test | result | notes |
|---|---|---|
| SD write speed (bench, 1 MB, 16 KB chunks) | kB/s | `evt=bench` / web *SD speed test* |
| SD read speed | kB/s | |
| one camera → Core HTTP (5 MP) | usually 300–650 ms per camera over several shots. One shot in detail: cam 1 155 kB in 531 ms (≈ 290 kB/s), cam 2 148 kB in 304 ms (≈ 490 kB/s), cam 3 137 kB in 265 ms (≈ 520 kB/s); cam 4 not connected yet | owner, 2026-11-15, step 11 (`s10-core-shot` + `s10-camN`), main @ 875d088. Time = HTTP GET start → file closed: includes the camera's grab and the SD write, cameras asked one after another. 137–155 kB is far below the *1–2.5 MB* estimate for 5 MP at quality 12: check a saved file's pixel size |
| 2 cameras in parallel | kB/s aggregate | |
| 4 cameras in parallel | kB/s aggregate | |
| camera ping (round trip) | 18 ms (cam 1) | owner, 2026-11-15, main @ 875d088. **Not sure**: which host pinged, single value or average |
| SoftAP RSSI per camera at rig distance | 26 (as reported) | owner, 2026-11-15, main @ 875d088. `WiFi.RSSI()` is negative dBm, so this is most likely −26 dBm (very strong, camera close to the Core). **Not sure**: sign, which camera, distance. `/api/status` |
| photo save time, empty card vs ~10k files in `/x4` | ms per photo | flat folder: each new file scans the whole folder (ADR-019). `evt=done ms=` |

## Effects on the camera (STEPS step 14, planned)

| output | decode ms | crop + dither ms | encode ms | PNG kB | peak PSRAM kB |
|---|---|---|---|---|---|
| 2592×1944 portrait + dithered | | | | | |
| 1296×972 portrait + dithered | | | | | |

## UI

| screen / animation | avg fps | worst frame ms | date / git |
|---|---|---|---|
| splash | | | |
| main idle | | | |
| main + preview | | | |
| shutter moment | | | |
| saving (4 progress rings) | | | |
| diagnostics | | | |
| touch-to-feedback latency (slow-mo) | ms | | |
