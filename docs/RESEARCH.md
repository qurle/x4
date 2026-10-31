# Research log

Date of all entries: **2026-10-31**. Sources: M5Stack official knowledge base (M5Stack Chat MCP,
which indexes docs.m5stack.com and schematic descriptions), M5Stack GitHub source, Espressif
GitHub source and the pioarduino build actually used, Awinic, Anker, Wago, LVGL, Espressif
datasheets. **VERIFIED** = read in the primary source; **ASSUMPTION** = inferred.

Not covered (no access from this environment): M5 community forum threads and GitHub
*discussions* were not searched one by one; issues were checked through the library source
(comments document several field problems). Add forum findings here when you read them.

## 1. CoreS3 / CoreS3-SE

| finding | x4 impact | tag | source |
|---|---|---|---|
| SE = CoreS3 without camera, proximity sensor, IMU, magnetometer and without the battery base; **only Port A** on the body | power + button + flash share one split Grove cable; extra I/O via M-Bus | VERIFIED | SE schematic description (single J3), product page |
| 5 V power path: 4 load switches (U14/U17/U18/U19) controlled by `BUS_OUT_EN` (AW9523B P0_1) and `USB_OTG_EN` (P0_5); SY7088 boost via `BOOST_EN` (P1_7) | the whole Port A back-feed analysis ([POWER §2](POWER.md)) | VERIFIED | SE schematic description; M5Unified 0.2.29 constants |
| M5Unified default `cfg.output_power = true` → `setExtOutput(true)` at `M5.begin()` | x4 must set `output_power = false` | VERIFIED | M5Unified 0.2.29 `M5Unified.hpp` line 133, `M5Unified.inl` `_begin()` |
| M5Unified cancels `setExtOutput(true)` when there is no battery and external 5 V is on BUS_OUT (TS ADC > 2.0 V) | second safety net for x4's setup; also shows the setup is known and handled | VERIFIED | `_core_s3_ext_output_unsafe()` |
| BUS_OUT can **latch** to VBUS after a precharge even with `BUS_OUT_EN = 0`; turning it off needs BOOST off + 200 ms | explains a possible "5 V on Port A while on USB" reading; bring-up step 3 advice | VERIFIED | M5Unified comments in `_core_s3_output_locked()` |
| A DCDC dip below 85 % (inrush on a weak supply or the bus switch-over) makes the AXP2101 power itself off; M5 disables DCDC UVP power-off so the ESP32 browns out and restarts instead | brown-outs reset instead of a dead board; inrush matters | VERIFIED | M5Unified + M5GFX comments, reg 0x23 |
| M5GFX autodetect enables BOOST + precharges/enables `BUS_OUT_EN` **only if** GPIO35/36/37 read low with pull-ups | on x4 (nothing on those pins) the output stays off; don't stack M-Bus modules | VERIFIED | M5GFX 0.2.36 `M5GFX.cpp` CoreS3 branch |
| SD shares SPI with the LCD (MOSI G37, SCK G36, MISO/DC G35); M5GFX uses SPI2 (same peripheral as Arduino `SPI`) | SPI mutex; chunked SD writes; UI may stutter while saving | VERIFIED (pins, host); lock behaviour of the libs *Not sure* → x4 serialises itself | M5 pin map; M5GFX `bus_cfg.spi_host = SPI2_HOST` |
| microSD max 16 GB | owner's 16 GB card is at the limit, OK | VERIFIED | SE product page |
| Grove output max 680 mA (USB powered), 940 mA (battery) | direction opposite to x4's use; input limit *Not sure* | VERIFIED (output) | SE spec table |
| Download mode: hold RST ~3 s until green LED | BRINGUP | VERIFIED | SE product page |
| PSRAM on the Core is **quad** (8 MB), the cameras' is **octal** | different board memory types | VERIFIED | SE spec "8MB Quad"; N16R8 datasheet |
| Free GPIOs on the SE M-Bus: G5, G6, G7, G8, G9, G10, G17, G18 (no Port B/C hardware) | spare-port plan | VERIFIED (pin map) / ASSUMPTION (nothing else on the SE board drives them) | SE M-Bus table |
| M5Unified/M5GFX init order: power begin → ext output → peripherals; touch INT via AW9523 | use `M5.begin(cfg)` once, then x4 init | VERIFIED | source |

## 2. Unit CamS3-5MP

| finding | x4 impact | tag | source |
|---|---|---|---|
| Sensor marketed as "PY260", 5 MP, 2592×1944, 88° DFoV, fixed focus 0.6 m; **not OV5640** | no OV5640 driver; the brief's note was wrong | VERIFIED | M5 product description |
| Sensor module at SCCB 0x1F, 16-bit registers = esp32-camera **MEGA_CCM** (PID 0x039E), supports up to `FRAMESIZE_5MP`; built into Arduino-ESP32 3.3.12 libs (`CONFIG_MEGA_CCM_SUPPORT=1`) | full 5 MP works with stock esp32-camera; firmware checks PID | VERIFIED | esp32-camera `sensor.h`, `mega_ccm.c`; pioarduino sdkconfig |
| Two hardware revisions (reg 0x0200 = 0x01 new / 0xFF old); old needs Arduino 3.1.0 + patched lib | probe at boot, `err=206` | VERIFIED | M5 Arduino guide |
| Official 5 MP example: M5's `CameraWebServer.zip`; board settings: USB CDC On Boot enabled, PSRAM = OPI | board json, Stage 0 bench | VERIFIED | M5 Arduino guide |
| Pin map (DVP, SCCB, reset, PWDN tied low, LED G14 active-low, SD, mic) | `cam_pins.h` | VERIFIED | schematic + pin table |
| Grove = **USB D+/D− (G20/G19)** + 5 V + GND; flashing via Grove-to-USB-C adapter; download mode = G0→GND before power-up | never wire yellow/white to the rail; USB and rail cannot coexist | VERIFIED | schematic J1, M5 docs |
| No TVS/fuse/reverse-polarity protection on the camera input | polarity checks are critical | VERIFIED | schematic |
| JPEG frame buffer default = w×h/5 (≈ 1 MB at 5 MP) unless `jpeg_buffer_size` is set | x4 sets 2.5 MB (`cam_jpeg_buf_kb`), checks the JPEG end marker | VERIFIED | esp32-camera `cam_hal.c` |
| `fb->timestamp` = `esp_timer` at frame start (`cam_start_frame`) | capture-time reporting and skew measurement | VERIFIED | `cam_hal.c` |
| MEGA_CCM quality register semantics (0 high / 1 default / 2 low) | `cam_quality` default 1 | ASSUMPTION (driver writes the value raw; ArduCam-style convention) | `mega_ccm.c` |
| ESP32-S3-WROOM-1-N16R8 is the **65 °C** ambient version (85 °C with PSRAM ECC) | thermal thresholds, enclosure vents | VERIFIED | Espressif module datasheet ordering table |
| Brown-outs under WiFi + camera, heating | x4 staggers init, logs reset reasons, throttles by temperature | ASSUMPTION (common reports for ESP32 cams; not checked for this unit) | – |

## 3. Arduino-ESP32 2.x vs 3.x, PlatformIO vs pioarduino

| finding | x4 impact | tag |
|---|---|---|
| MEGA_CCM needs Arduino-ESP32 3.x; M5 says "latest esp32 core" for the new camera revision | **pioarduino** (`55.03.312-1` = Arduino 3.3.12 / IDF 5.5.5) | VERIFIED |
| M5Unified 0.2.29 and M5GFX 0.2.36 build on it (this repo builds them) | pinned | VERIFIED (build) |
| LVGL 9.6.0 builds on it with `LV_CONF_INCLUDE_SIMPLE` | pinned | VERIFIED (build) |
| `m5stack-cores3` board exists in pioarduino; no board for the Unit CamS3 → local board file with variant `m5stack_unit_cams3` | `camera/boards/` | VERIFIED |
| PlatformIO registry was unreachable from this build sandbox; GitHub tags work everywhere | libraries pinned as git tags | VERIFIED (environment) |

## 4. ESP32-S3 SoftAP

| finding | x4 impact | tag |
|---|---|---|
| Arduino `WiFi.softAP()` default `max_connection` = 4 | x4 passes 6 (`ap_max_clients`) and logs the verified value | VERIFIED (Arduino API) |
| ESP-IDF 5.5.5 S3 limit `ESP_WIFI_MAX_CONN_NUM = 15` | x4 caps at 10 | VERIFIED (header in this build) |
| Station power save adds 100+ ms latency; AP buffers broadcasts for stations in power save until DTIM | cameras `setSleep(false)`; trigger = broadcast + unicast | VERIFIED (802.11 behaviour) / ASSUMPTION (exact numbers on S3) |
| SoftAP throughput with 4 clients pulling 1–2.5 MB each | the main performance risk; `xfer_parallel` tunable; measure | ASSUMPTION (expect ~1–2 MB/s aggregate) |
| 5 MP JPEG size | 1–2.5 MB (brief) | ASSUMPTION, measure with `cam-hello` |

## 5. LVGL 9 on ESP32-S3

| finding | x4 impact | tag |
|---|---|---|
| Partial render buffers in internal DMA RAM are the usual choice; PSRAM buffers are slower for the CPU renderer | 2 × 40 lines internal | ASSUMPTION (common practice) |
| `LV_COLOR_FORMAT_RGB565_SWAPPED` blending exists in 9.6 | preview without byte-swap copies | VERIFIED (source) |
| No hardware JPEG decoder on the S3 | software TJpgDec for the preview; QVGA only | VERIFIED |
| Runtime XML loader not public (`lv_xml` removed from open source in 9.5); Editor C export is MIT-clean; Editor free for personal/non-commercial; CLI in paid tier | hand-written UI now, `ui_gen/` reserved | VERIFIED (lvgl.io) / CLI terms *Not sure* |
| 30–60 fps on 320×240 SPI | measure | ASSUMPTION |

## 6. Flashlight, button, inputs

| finding | x4 impact | tag |
|---|---|---|
| Flashlight = AW3641EDNR; EN on Grove yellow with 10 k pull-down; FLASH/TORCH by internal slide switch | EN on Core G2, idle-low safe | VERIFIED (schematic) |
| Flash mode: EN high fires, EN pulse count selects 8 levels and the timeout (~220 ms for 1–8 edges, ~1.3 s for 9–16) | one clean edge, 100 ms default, 150 ms cap | VERIFIED (datasheet summary) / exact timing to scope |
| M5: 8–15 s between flashes to avoid overheating | 8 s minimum cooldown (stricter than the brief) | VERIFIED |
| Unit Button: 10k/20k divider from VCC, active low; released ≈ 2/3 VCC | 5 V VCC → 3.33 V on G1 (OK); verify the owner's unit | VERIFIED (U027) / ASSUMPTION (owner's unit is U027-like) |
| Scroll wheel options: quadrature encoder on two free GPIOs (stub ready), or an I2C encoder unit (needs Port A's I2C pins → move button/flash to M-Bus) | input layer ready | ASSUMPTION (no part chosen) |

## 7. Power bank, inrush, USB-C

| finding | x4 impact | tag |
|---|---|---|
| Anker A1653: USB-C out 5V⎓3A (also 9 V / 10 V via PD), 18 W total | ~3 A worst-case rig peak is at the limit | VERIFIED |
| Without a PD controller the sink only gets 5 V (vSafe5V); C-to-C needs 5.1 kΩ Rd on CC1/CC2 | check the breakout | VERIFIED (USB Type-C spec) |
| Low-current auto-off of the A1653 | measure | *Not sure* |
| 2200 µF stores ≈ 1 mC per 0.5 V: smooths edges, does not supply a 100 mC flash pulse | expectations in POWER.md | VERIFIED (arithmetic) |

## 8. Things that commonly blow up or burn in such rigs

- Two 5 V sources on one board (USB + rail) → back-feed into the PC port or the bank: rule + docs.
- Reversed polarity on a branch (no protection on CamS3) → dead camera: continuity checklist.
- USB D+/D− (camera Grove yellow/white) wired to 5 V → dead USB PHY/ESP32: cut and tape.
- Flash left on (firmware hang, wrong pin level at boot) → LED/driver overheating: 6 layers of
  protection ([hw/flash.h](../core/src/hw/flash.h)), hardware timeout, 8 s cooldown.
- Loose high-resistance contacts in push-in terminals → heat: solid pigtails/ferrules, tug test,
  touch test after bursts.
- Electrolytic capacitor reversed or under-rated → venting: stripe to GND, ≥ 10 V.
- Closed enclosures with four 65 °C-rated modules → thermal throttling: vents, throttle logic.
