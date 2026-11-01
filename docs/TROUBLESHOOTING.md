# Troubleshooting

Format: **symptom → likely cause → check → fix.** Start with `python tools/x4log.py <log> --lvl WARN`
and look up the first `err=` in [LOGGING.md](LOGGING.md#error-codes).

## Power

| symptom | likely cause | check | fix |
|---|---|---|---|
| nothing powers on | bank asleep; breakout lacks CC resistors; switch/polarity | rail voltage with nothing attached; CC1/CC2 → GND ≈ 5.1 kΩ | press the bank button; use bank USB-A + A-to-C cable; fix wiring |
| boards reset when the flash fires or several cameras start | rail sag → brown-out | next boot shows `reset=brownout` / `err=103`; USB meter min voltage; web page `vbus_mv` | capacitor closer to the loads, shorter/thicker branch wires, better Wago contacts, lower `wifi_tx_qdbm` (e.g. 60), `xfer_parallel=1` |
| bank shuts off at switch-on | inrush (2200 µF + 6 loads) trips its over-current protection | switch on 10×; does it recover? | pre-charge resistor bypassed after ~1 s, smaller capacitor (470–1000 µF), switch loads in two groups. See [POWER §4](POWER.md#4-known-risks-and-mitigations) |
| bank shuts off after minutes of idle | low-current auto-off | measure idle current | x4 idle is ≈ 0.7 A, normally enough; if not, *Not sure* the A1653 has a "trickle" mode: check its manual |
| a Wago terminal or Grove plug gets **hot** | high resistance contact (stranded wire in a solid-only push-in, half-inserted plug) | touch test after a flash burst; tug test | redo with solid pigtail/ferrule/Wago 221; re-seat plugs. Warm is OK, hot is not |
| reset loop right after boot | brown-out at WiFi start, or a crash | `reset=` reason on each boot, panic backtrace on serial | power: as above. Panic: decode with the `esp32_exception_decoder` monitor filter (on by default) |
| camera dead after wiring change | reversed polarity on its branch (no protection on the CamS3 input) | continuity red→+5V, black→GND | prevention only; replace the unit |

### Core 5V output found ON

`err=100 POWER_BUS_OUT_ON` / `err=101 POWER_OTG_ON`, web page "5V out ON (unsafe)", or ≈ 5 V on
Port A with the Core on USB and nothing attached.

- Cause: a library or code path enabled `BUS_OUT_EN`/`USB_OTG_EN` (M5Unified's default
  `cfg.output_power = true` if someone removed x4's override; M5GFX's precharge heuristic if
  GPIO35/36/37 read low), or BUS_OUT latched to VBUS after a precharge (M5Unified documents this).
- Check: boot log `mod=power evt=state … bus_out_en=` and `p0=`; `git diff` around `M5.begin`.
- Fix: x4 already forces it off and logs. If it keeps coming back: **disconnect the rail**, unplug
  USB for 10 s (clears the latch), make sure `cfg.output_power = false` is set before
  `M5.begin(cfg)`, remove M-Bus accessories, reflash. Never run the rig while it reports `safe=0`.

## Flashing

| symptom | likely cause | check | fix |
|---|---|---|---|
| Core: no serial port | not in download mode / charge-only cable | `pio device list` | hold RST ~3 s until the green LED, release; try a data cable |
| Camera: no serial port | Grove-to-USB-C adapter not used / download mode needed | M5 adapter plugged into the camera's Grove | short G0→GND before plugging in, upload, remove the jumper, replug |
| upload starts then fails at high speed | marginal cable | – | `upload_speed = 460800` in platformio.ini for that env |
| "both power sources" mistake: board was on the rail **and** USB | rule broken | – | unplug both, wait, reconnect one. Inspect for heat. Re-read [BRINGUP](BRINGUP.md#flashing-without-breaking-the-power-rules) |
| OTA upload refused | wrong password / laptop not on `x4cam` | `evt=ota` lines | `X4_OTA_PASS` in secrets.h, join the AP |
| build fails "secrets.h not found" | – | it is a warning, not an error | copy `secrets.h.example` to `secrets.h` to silence it |

## Camera

| symptom | likely cause | check | fix |
|---|---|---|---|
| `err=200 CAM_INIT_FAIL` | ribbon loose, wrong pins, no PSRAM | `esp_err=` value, `psram_mb` in the banner | reseat the sensor FPC; confirm env uses `m5stack-unitcams3` board (OPI PSRAM) |
| `err=203 CAM_NO_PSRAM` / `psram_mb=0` | board settings (needs **OPI** PSRAM) | build log shows `qio_opi` | use the provided camera env; don't switch to a generic S3 board |
| `err=201 CAM_SENSOR_MISMATCH` | a different sensor than MEGA_CCM (PID 0x039E) | `evt=sensor name= pid=` | report the PID; add the matching driver/size table before shooting |
| black / green / striped frames | XCLK too high, first frame after a size change, loose FPC | `/capture?res=1` repeatedly | `cam_xclk_mhz=16`; reseat FPC |
| `err=204 CAM_FRAME_TRUNCATED` | JPEG bigger than the frame buffer | `bytes=` close to `cam_jpeg_buf_kb` | raise `cam_jpeg_buf_kb` (max 3072) or `cam_quality` toward lower quality |
| slow capture (`cap_ms` ≫ frame period) | resolution switch (preview camera), PSRAM contention | `evt=res switch_ms=` | use a non-preview camera as reference; raise `trig_lead_ms` |
| `err=205 CAM_SHOT_EXPIRED` | Core fetched late or not at all | Core `xfer` lines | raise `cam_hold_ms`; check WiFi |
| camera hot, throttling | 65 °C ambient module in a closed box | `temp_c` in heartbeats | vents, less streaming, no camera LED |

### Old CamS3 hardware

`err=206 CAM_OLD_HW` (`reg_0x0200=0xff`). M5's guide: the old revision needs Arduino-ESP32
**3.1.0** with M5's patched `libespressif__esp32-camera.a` (download link in the guide). x4 is
pinned to 3.3.12. Options: ask the seller for the new revision; or create a separate PlatformIO
env with a pioarduino release that ships Arduino-ESP32 3.1.0 (check its release notes) and the patched library (not done here: no old unit to
test). *Not sure* which revision new stock ships with; the boot log tells you.

## WiFi

| symptom | likely cause | check | fix |
|---|---|---|---|
| camera never joins (`err=404`) | SSID/password mismatch between `core/` and `camera/` secrets; channel | `evt=join … status=` | same `X4_AP_SSID/PASS`; `ap_channel` the same on both (Core pushes it at register; first join uses the compiled default) |
| `err=405 NET_REGISTER_FAIL` / no id | camera built as `cam-hello` or id unset | `dev=cam?` in the log | flash `camN` or `GET /settings?cam_id=N` |
| AP full (5th/6th device cannot join) | `ap_max_clients` | `evt=ap … max_conn=` | raise it (max 10) |
| cameras drop out (`err=401`) | RSSI, interference, brown-out | `rssi` per camera, `offlineEvents`, camera `reset=` | move the Core closer/raise TX power; change `ap_channel` away from busy home WiFi |
| trigger latency/skew large and random | WiFi power save | camera log `WiFi.setSleep(false)` is in code | do not remove; keep phones off the AP while shooting (broadcast DTIM buffering) |
| `err=406` protocol mismatch | Core and camera built from different versions | `fw=`/`git=` in register lines | flash everything from the same commit |

## SD

| symptom | likely cause | check | fix |
|---|---|---|---|
| `err=300 SD_MOUNT_FAIL` | no card, exFAT, > 16 GB, bad contact | card in a PC | FAT32, ≤ 16 GB (CoreS3 documented limit); reseat |
| `err=302 SD_FULL` | < 12 MB free | web page SD | copy photos off, delete `/x4/shots/S…` folders |
| `err=303 SD_SLOW` (< 1000 kB/s) | slow/old card, SPI clock | `evt=bench` | a better card (A1/U1), keep `kSpiHz` at 25 MHz |
| UI stutters while saving | SD and LCD share the SPI bus | worst frame time during "SAVING" | expected to some degree; chunks are 16 KB; `xfer_parallel=1` smooths it |

## Transfer (misses the 5 s target, `err=407`)

1. Read `python tools/x4log.py core.log --shots`: is the time in **capture** (`captured_ms`),
   **transfer** (per-camera ms/kB/s) or **SD** (bench)?
2. JPEG too big → `cam_quality` toward lower quality (MEGA_CCM: 2 = low, ASSUMPTION) or smaller
   `cam_res` for a test.
3. Air time → `xfer_parallel` 1 vs 2 vs 4 (measure each), TX power, distance, channel.
4. SD slow → better card.
5. The "captured" sound/tick comes as soon as all frames are in camera PSRAM, so the shot is safe
   even if storing takes longer; the next shot waits until the buffers are free.

## UI

| symptom | likely cause | check | fix |
|---|---|---|---|
| tearing | partial refresh without TE sync (normal on this panel) | – | keep big animations short; accept |
| low FPS | large redraw areas, preview decode on the loop | diagnostics fps / worst frame | fewer animated pixels, `prev_fps` lower |
| touch offset / mirrored | panel/touch variant (M5GFX detects ILI9342C vs ILI9342E) | touch test dot vs finger | log `board=`; report; set rotation in `display::begin` |
| serif text shimmering / unreadable | serif below ~20 px | – | use Space Mono for small text (v1 already does) |
| tofu boxes instead of characters | glyph not in the converted range (e.g. Cyrillic) | – | add the range in `tools/gen_fonts.sh`, regenerate |

## Flash unit

| symptom | likely cause | check | fix |
|---|---|---|---|
| no light | slide switch on TORCH, EN wire on the wrong pin, guard refused | `evt=refused verdict=` | Arm first (disabled by default); check yellow→G2; slide switch FLASH |
| light at boot | EN pulled up before firmware runs | measure Port A G2 idle (bring-up step 3) | move EN to G7 (`kFlashEnAlt`) |
| `err=104 FLASH_STUCK_ON` | pin read high outside a pulse | – | the watchdog forced it low; report the log tail |
| very short/weak flash | 1-wire level selection (multiple EN edges) | slow-mo | x4 uses one clean edge; make sure nothing else toggles G2 |

## The mixed-content / HTTP trap

A web page served over **HTTPS** from the internet **cannot** call the rig's plain-HTTP API
(browsers block mixed content, and the AP has no internet anyway). Always use the page the Core
serves itself: `http://192.168.4.1/` — same origin, no CORS, no mixed content. Native apps need
cleartext exceptions for `192.168.4.1` ([COMPANION.md](COMPANION.md)).
