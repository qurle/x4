# Hardware

Bill of materials, exact variants, pin tables, and what is confirmed vs assumed.
**VERIFIED** = from an official document, schematic or library source (cited). **ASSUMPTION** =
reasoned but not confirmed. **Not sure** = unknown. Research date 2026-10-31.

## Bill of materials

| # | part | qty | status | notes |
|---|---|---|---|---|
| 1 | M5Stack **CoreS3-SE** (SKU K128-SE) | 1 | not bought yet | ESP32-S3, 16 MB flash, 8 MB **quad** PSRAM, 2.0" 320×240 ILI9342C IPS, FT6336U touch, AXP2101 PMIC, AW9523B IO expander, AW88298 amp + 1 W speaker, ES7210 + 2 mics, BM8563 RTC, microSD (max 16 GB), **no battery, no IMU, no camera, one Grove port (Port A)** |
| 2 | M5Stack **Unit CamS3-5MP** (SKU U174-B) | 4 | not bought yet | ESP32-S3-WROOM-1-**N16R8** (16 MB flash, 8 MB **octal** PSRAM), 5 MP sensor module (see below), microSD slot (unused), PDM mic, blue LED, Grove = USB D+/D− |
| 3 | M5Stack **Unit FlashLight** (SKU U152) | 1 | not bought yet | AW3641EDNR flash driver, 1 white LED, internal Flash/Torch slide switch |
| 4 | M5Stack **Mini Button Unit** (no LED) | 1 | on hand | ASSUMPTION: electrically the "Unit Button" (U027) design, see below |
| 5 | **Anker Nano Power Bank 5000 mAh** (A1653, 22.5 W name, built-in USB-C plug) | 1 | on hand | USB-C out 5V⎓3A / 9V⎓2.22A / 10V⎓2.25A, total 18 W max; input 5V⎓3A / 9V⎓2A |
| 6 | power switch | 1 | on hand | must be ≥ 5 A DC; in +5 V only |
| 7 | 2200 µF electrolytic capacitor | ≥1 | on hand | **rated 15 V** (owner, 2026-10-31): OK, ≥ 10 V needed; low-ESR expected |
| 8 | USB-C breakout with inline capacitors | 5 (1 used) | on hand | only one used (bank input); **CC resistors present** (owner, 2026-10-31) → C-to-C from the bank works; inline capacitor value/side still unknown → bring-up step 1 |
| 9 | Wago **2273-208** (8-port push-in) | several | on hand | **solid conductors only** per Wago data, 0.5–2.5 mm² (≈ 20–14 AWG); two per net |
| 10 | 20 AWG stranded wire | bunch | on hand | 0.52 mm², ≈ 33 mΩ/m |
| 11 | Dupont cables | various | on hand | fit the M5-Bus header (2.54 mm) |
| 12 | Grove cables (in box) | per unit | come with the units | HY2.0-4P |
| 13 | **Smartbuy microSDHC 16 GB Class 10** | 1 | on hand | goes in the **Core**; 16 GB = the CoreS3 documented maximum (OK), FAT32 |

Suggested extras (not required): crimp ferrules 0.5 mm² or Wago **221-415** lever connectors
(they take stranded wire), a USB power meter (5 V, ≥ 3 A), a multimeter with continuity beeper.

## CoreS3-SE

Source: M5Stack knowledge base, CoreS3-SE product page + schematic descriptions (official),
https://docs.m5stack.com/en/core/M5CoreS3%20SE. All rows VERIFIED unless tagged.

| function | pins | notes |
|---|---|---|
| Port A (Grove, red) | Black GND, Red 5 V (`BUS_OUT`), **Yellow G2 (SDA)**, **White G1 (SCL)** | J3 pin 1 = SCL/G1, pin 2 = SDA/G2, pin 3 = BUS_OUT, pin 4 = GND. Schematic footprint label says "GH2.0-4P", product page says HY2.0-4P/Grove (*Not sure* which label is right; the Grove cable is what M5 ships for it) |
| Ports B/C | – | **not present on the SE** (they live on M5GO/DIN bases). The product page still lists "PORT.B G9/G8, PORT.C G17/G18": those pins are reachable only on the M5-Bus header |
| LCD ILI9342C | MOSI G37, SCK G36, CS G3, DC G35, RST = AW9523B P1_1, BL = AXP2101 DLDO1 | shares SPI with SD |
| microSD | MOSI G37, SCK G36, MISO G35, CS G4, detect = AW9523B P0_4 | **max 16 GB** (M5 docs) |
| Touch FT6336U (0x38) | SDA G12, SCL G11, INT = AW9523B P1_2, RST = AW9523B P0_0 | |
| Internal I2C | SDA G12, SCL G11 | AXP2101 0x34, AW9523B 0x58, AW88298 0x36, ES7210 0x40, BM8563 0x51 |
| Speaker AW88298 | BCK G34, WS G33, DOUT G13 | haptics: **none** on the SE (no vibration motor) |
| Mics ES7210 | MCLK G0, DIN G14 | |
| USB-C | native USB G19/G20, CC1/CC2 5.1 kΩ to GND | |
| Power keys | side power key (AXP2101 PWRON), RST key on the bottom | download mode: hold RST ~3 s until the green LED lights, release |
| 5 V direction | AW9523B P0_1 `BUS_OUT_EN`, P0_5 `USB_OTG_EN`, P1_7 `BOOST_EN` | see [POWER.md](POWER.md) |

M5-Bus (2×15, 2.54 mm, VERIFIED M5 Bus Socket spec), relevant pins:

| pin | signal | pin | signal |
|---|---|---|---|
| 1, 3, 5 | GND | 2 | G10 (ADC) |
| 4 | **G8** (PB_IN) — free on the SE | 6 | RST/EN |
| 7 | G37 MOSI | 8 | **G5** — free |
| 9 | G35 MISO | 10 | **G9** (PB_OUT) — free |
| 11 | G36 SCK | 12 | 3V3 |
| 13 | G44 RXD0 | 14 | G43 TXD0 |
| 15 | **G18** (PC_RX) — free | 16 | **G17** (PC_TX) — free |
| 17 | G12 int SDA | 18 | G11 int SCL |
| 19 | G2 Port A SDA | 20 | G1 Port A SCL |
| 21 | **G6** — free | 22 | **G7** — free |
| 23 | G13 I2S DOUT | 24 | G0 I2S LRCK / boot |
| 25, 27, 29 | NC | 26 | G14 I2S DIN |
| 28 | **5 V (`BUS_OUT`)** | 30 | BAT (unused on SE) |

Strapping pins to avoid: G0, G3, G45, G46 (ESP32-S3).

## Unit CamS3-5MP

Sources (VERIFIED): M5 product page + schematic SCH_UNIT_CAMS3_V1.0
(https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1050/SCH_UNIT_CAMS3_V1.0.pdf), M5 Arduino guide
(https://docs.m5stack.com/en/arduino/m5unitcams3_5mp/program), esp32-camera source.

**Sensor identity — the owner's biggest purchase risk, now mostly resolved:**

- M5 calls it a custom **"PY260"** 5 MP camera, 88° DFoV, fixed focus at 0.6 m, max
  **2592 × 1944**, JPEG output (VERIFIED, M5 product description). It is **not an OV5640
  derivative** as the planning notes said.
- It answers on SCCB address **0x1F** with 16-bit registers (VERIFIED: M5's version-check code
  reads 0x1F/0x0200). In Espressif's `esp32-camera` that is the **MEGA_CCM** driver
  (`MEGA_CCM_SCCB_ADDR = 0x1F`, `MEGA_CCM_PID = 0x039E`, max `FRAMESIZE_5MP`), present in the
  pioarduino Arduino-ESP32 3.3.12 libraries with `CONFIG_MEGA_CCM_SUPPORT=1` (VERIFIED in this
  build). The module does its own ISP + JPEG; the ESP32 only receives JPEG over DVP.
- Supported sizes in the driver: QVGA, VGA, HD (1280×720), UXGA (1600×1200), FHD (1920×1080),
  5MP (2592×1944), plus 96×96, 128×128, 320×320 (VERIFIED: `mega_ccm.c` `set_framesize`).
- **Two hardware revisions exist.** Register 0x0200 reads 0x01 on the new revision and 0xFF on
  the old one. M5 says the old one needs Arduino-ESP32 **3.1.0** plus a patched
  `libespressif__esp32-camera.a` they distribute (VERIFIED: M5 Arduino guide). x4 reads it at
  boot and logs `err=206 CAM_OLD_HW` for old units. If you receive old units, see
  [TROUBLESHOOTING.md](TROUBLESHOOTING.md#old-cams3-hardware).
- **Working full-5MP example:** M5's `CameraWebServer.zip` for this unit (official, Arduino 3.x).
  x4's `cam-hello` environment benchmarks every size including 5 MP on first flash.
- The firmware refuses the capture path if the PID is not 0x039E (`err=201`).

| function | ESP32-S3 pin |
|---|---|
| SCCB SDA / SCL | G17 / G41 |
| XCLK | G11 (via 47 Ω) |
| VSYNC / HREF / PCLK | G42 / G18 / G12 |
| D0..D7 (Y2..Y9) | G6, G15, G16, G7, G5, G10, G4, G13 |
| RESET | G21 |
| PWDN | tied to GND (R5 10k) |
| microSD (unused by x4) | CS G9, MOSI G38, CLK G39, MISO G40 |
| blue LED | G14 → 1 kΩ → LED → 3V3 (LOW = on) |
| PDM mic | CLK G47, DATA G48 |
| **Grove HY2.0-4P** | **Black GND, Red 5 V, Yellow G20 (USB D+ / TX), White G19 (USB D− / RX)** |
| download header P1/P2 | P1: G3/SDA, RXD0, TXD0, 5 V; P2: G8/SCL, EN, **G0**, GND |

Power tree: 5 V → SY8089 buck → 3 V → 1.2 V (HX6306, 300 mA) and 2.8 V (XC6206, 150 mA) for the
sensor. No TVS, fuse or reverse-polarity protection on the Grove input (VERIFIED: schematic). A
reversed red/black on a camera branch will likely kill it: check polarity twice.

Board settings (VERIFIED: M5 Arduino guide): board "M5UnitCAMS3", **USB CDC On Boot: Enabled**,
**PSRAM: OPI PSRAM**. x4 uses a local PlatformIO board (`camera/boards/m5stack-unitcams3.json`):
16 MB QIO flash + OPI PSRAM (`memory_type = qio_opi`, confirmed in the build log), partition table
`default_16MB.csv`, variant `m5stack_unit_cams3`.

Download mode: short **G0 to GND before power-up** with the supplied jumper wire, then connect
the Grove-to-USB-C adapter (VERIFIED: M5 docs). The Grove port is the USB port: the camera cannot
be on the rail and on USB at the same time anyway.

## Unit FlashLight (U152)

Sources (VERIFIED): M5 product page + schematic, Awinic AW3641E datasheet summary.

| item | value |
|---|---|
| driver | AW3641EDNR (charge-pump flash/torch LED driver, 1 A max flash) |
| Grove | pin 1 (white) NC, **pin 2 (yellow) = EN**, pin 3 (red) = 5 V, pin 4 (black) = GND |
| EN input | 10 kΩ pull-down to GND on the unit (R1) → idles OFF when the wire is floating |
| mode switch | internal slide switch S1 sets FLASH/TORCH pin (R4 10 kΩ pull-up to 5 V **on the mode pin only**, not on EN) |
| flash mode | EN high → flash at the set current; **1-wire pulses on EN** select 8 levels (100 % … 30 %) and the timeout; hardware **flash timeout ≈ 220 ms typ. (180–310 ms) for 1–8 rising edges, ≈ 1.3 s for 9–16 edges** (datasheet V1.4 summary) |
| torch mode | PWM dimming (on the FLASH pin per datasheet summary; M5 page says on EN — *Not sure*, x4 does not use torch) |
| current | owner-confirmed ≈ 1 A for ≈ 100 ms; RSET = 100 kΩ, Rsense = 0.22 Ω on this board |
| logic level | EN driven by a 3.3 V GPIO: OK. AW3641 VIH threshold *Not sure* (typ. ~1.2 V for this family) |
| thermal | **M5: "interval of 8~15 seconds between flashes is recommended" to avoid overheating** |
| colour temp | schematic 5000–5700 K; spec table 6000–6500 K (M5's own docs disagree) |
| protection | over-temperature alarm, over-voltage, short-circuit (IC-internal, per M5 page) |

**x4 flash semantics (ASSUMPTION until scoped):** slide switch in **Flash** position; one clean
rising edge on EN (= 1 edge → level 100 %, timeout ≈ 220 ms), held high for `flash_pulse_ms`
(default 100 ms, hard cap 150 ms), then low. Our pulse ends before the IC's own timeout, which
remains as a backstop. Confirm with a scope/phone slow-mo during bring-up step 5.

## Mini Button Unit

*ASSUMPTION:* the owner's "Mini Button Unit" is electrically the M5 **Unit Button (U027)**:
J1 pin 1 (white) = signal, pin 2 NC, pin 3 = VCC, pin 4 = GND; VCC → 10 kΩ → signal → 20 kΩ →
GND, 100 nF to GND, button shorts signal to GND (**active LOW**) (VERIFIED for U027 schematic).

With VCC = 5 V from the rail the released level is 2/3 × 5 V = 3.33 V (under the ESP32-S3's
VDD + 0.3 V absolute max); pressed = 0 V. x4 also enables the internal pull-up (harmless).
**Check at bring-up step 2:** with 5 V on red, measure white-to-black: ~3.3 V released, ~0 V
pressed. If you see ~5 V released, do not connect it to G1.

## Power bank

Anker A1653 (VERIFIED: Anker spec page): USB-C output 5V⎓3A / 9V⎓2.22A / 10V⎓2.25A, 18 W total.
The x4 breakout has no PD controller, so the bank stays at **5 V** (USB Type-C sink with Rd only
gets vSafe5V). A C-to-C connection needs 5.1 kΩ Rd on CC1 and CC2 at the sink (the breakout).
Low-current auto-off threshold: **Not sure** (not documented by Anker); measure at bring-up step 8.

## Recorded on bring-up (fill in)

| item | value | how |
|---|---|---|
| USB-C breakout CC resistors | **present** (owner). Measured with the bank connected: CC1 ≈ 5 kΩ, CC2 ≈ 9.2 kΩ → **re-measure unplugged** (ohms are only valid on an unpowered circuit). If CC2 stays off 5.1 kΩ: power works in one plug orientation only (flip the plug if the rail reads 0 V) or use the bank's USB-A port | multimeter 20k range, GND ↔ CC1 / CC2, breakout unplugged |
| breakout inline capacitor | value: ___ V rating: ___ input/output side: ___ | read marking |
| 2200 µF voltage rating | **15 V** (owner) | read the can |
| power switch rating | ___ A DC | read the marking |
| CamS3 hardware revision (0x0200) | cam1 ___ cam2 ___ cam3 ___ cam4 ___ | boot log `evt=hw_rev` |
| CamS3 MACs | cam1 ___ cam2 ___ cam3 ___ cam4 ___ | boot log `evt=chip` |
| Core Port A G2 idle voltage (firmware running, nothing attached) | ___ V | bring-up step 3 |
| Mini Button released / pressed voltage at 5 V | ___ / ___ V | bring-up step 2 |
