# Measurements

Nothing here is measured yet (no boards on hand). Fill the tables with a USB power meter (bank →
breakout), a multimeter, the serial log (`tools/x4log.py`) and the web page. Keep the date and
firmware git hash on every row. Estimates from the brief are in *italics*.

Tools: USB power meter able to show peak/min, multimeter, phone with 240 fps slow motion,
optionally a cheap scope for the flash and rail sag.

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

## Rig total

| condition | average mA / W | peak mA | min rail V | notes |
|---|---|---|---|---|
| standby (all on, idle) | *3–4 W* | | | |
| preview on | | | | |
| shot without flash | | | | |
| shot with flash | *7–10 W + flash* | | | |
| inrush at switch-on (peak, duration) | – | | | bank shut off? Y/N |

## Flash

| pulse_ms | rail min V during pulse | resets? | light duration (slow-mo) | connector temp after 10 | date |
|---|---|---|---|---|---|
| 50 (test) | | | | | |
| 100 (default) | | | | | |
| 150 (cap) | | | | | |

## Bank behaviour

| test | result |
|---|---|
| auto-off threshold (lowest current that keeps it on) | |
| does it wake on plug-in / need a button press? | |
| runtime from full, standby | *≈ 4.2 h at 3.5 W (est.)* |
| runtime from full, 1 shot / 30 s + preview | |

## Temperatures (die, from logs; and touch/IR thermometer on the case)

| device | idle °C | 10 min preview °C | after 20 shots °C | ambient °C | enclosure? |
|---|---|---|---|---|---|
| Core | | | | | |
| Cam1..4 | | | | | |
| Flashlight case | – | – | | | |

## Camera capture (from `cam-hello` bench and `evt=captured`)

| res | JPEG size (avg) | grab ms (min/max) | frame period ms | res switch ms | quality setting |
|---|---|---|---|---|---|
| QVGA 320×240 | | | | | |
| VGA 640×480 | | | | | |
| HD 1280×720 | | | | | |
| UXGA 1600×1200 | | | | | |
| FHD 1920×1080 | | | | | |
| **5MP 2592×1944** | *1–2.5 MB* | | | | |

## Shot timing (target: all 4 stored ≤ 5 s)

From `python tools/x4log.py core.log --shots` and the web page.

| shot | skew ms (timestamps) | skew ms (slow-mo) | shutter→captured ms | transfers (ms, kB/s each) | total ms | xfer_parallel | quality |
|---|---|---|---|---|---|---|---|
| | | | | | | | |

## Throughput

| test | result | notes |
|---|---|---|
| SD write speed (bench, 1 MB, 16 KB chunks) | kB/s | `evt=bench` / web *SD speed test* |
| SD read speed | kB/s | |
| one camera → Core HTTP (5 MP) | kB/s | `evt=done kBps=` |
| 2 cameras in parallel | kB/s aggregate | |
| 4 cameras in parallel | kB/s aggregate | |
| SoftAP RSSI per camera at rig distance | dBm | `/api/status` |

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
