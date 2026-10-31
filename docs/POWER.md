# Power and safety

> Priority 3 of the owner: *"it shouldn't blow up or burn my components"*. This page is the
> reasoning behind every power rule in the firmware and in [WIRING.md](WIRING.md).
> Tags: **VERIFIED** (source given) / **ASSUMPTION** (reasoned, to be measured) / **Not sure**.
> Research date: 2026-10-31. Library versions: M5Unified 0.2.29, M5GFX 0.2.36.

## TL;DR

1. Feeding the CoreS3-SE through **Port A's 5 V pin** is a supported way to power it, **as long
   as the Core never turns its own 5 V output on** (`BUS_OUT_EN = 0`, `USB_OTG_EN = 0`).
   The firmware guarantees that from the first milliseconds of boot and re-checks it every 2 s.
2. **Never have USB-C and the rail connected to the same board at the same time.** On the Core
   the two 5 V sources end up tied together through two load switches. Flashing procedure:
   [BRINGUP.md](BRINGUP.md#flashing-without-breaking-the-power-rules).
3. The rig's worst-case peak (4 cameras + Core + flash) is **close to the bank's 3 A limit**.
   Measure it at bring-up step 4/5 before trusting it ([MEASUREMENTS.md](MEASUREMENTS.md)).
4. **Flash cooldown is 8 s minimum** (M5 recommends 8–15 s), not the 1 s in the brief.
5. **Wago 2273 is rated for solid wire only.** Stranded 20 AWG and Grove wires need solid
   pigtails, ferrules or a lever connector (Wago 221). See [WIRING.md](WIRING.md#connectors).

No finding forces a plan change; nothing here needs the owner's decision before the first
power-up. Two items need a measurement at bring-up (Port A idle voltage, rail sag with flash).

## 1. Architecture

```
Anker Nano 5000 (USB-C out, 5 V ⎓ 3 A, no PD negotiation)
  └─ USB-C breakout #1 (the ONE in use; must have 5.1 kΩ CC pull-downs)
       └─ power switch (+5 V line only, ≥ 5 A DC)
            ├─ +5 V rail: Wago 2273-208 ×2 (jumpered)          GND rail: Wago 2273-208 ×2
            │    ├── Cam1 Grove red  ─┐                              ├── Cam1 Grove black
            │    ├── Cam2 Grove red   │ star: one branch per load     ├── Cam2 …
            │    ├── Cam3 Grove red   │                              ├── Cam3 …
            │    ├── Cam4 Grove red   │                              ├── Cam4 …
            │    ├── Core Port A red (split Grove cable, see WIRING)  ├── Core Port A black
            │    ├── Flashlight red (20 AWG)                          ├── Flashlight black (20 AWG)
            │    ├── Mini Button red (5 V, see HARDWARE "button")     ├── Mini Button black
            │    └── 2200 µF (+)  ── in PARALLEL ──  2200 µF (−, stripe) ┘
            └─ (spares: 2–3 free ports per net after jumpering two blocks)
```

- Star topology, never daisy-chained Grove power.
- Capacitor **in parallel** across +5 V and GND, stripe = minus = GND. **Never in series.**
  Place it near the camera branches. Voltage rating ≥ 10 V (6.3 V parts are too close to 5.25 V).
- The flash takes its 1 A from the rail on 20 AWG, never through the Core's Port A. Only its
  yellow EN wire goes to the Core.
- Camera Grove **yellow/white are USB D+/D−** (G20/G19). They must never touch the rail. Cut
  short, insulate and tape them in the harness (VERIFIED: Unit CamS3-5MP schematic J1 pin 1
  G19/D−, pin 2 G20/D+, M5 pin map "Yellow G20, White G19").
- Net count per rail: source, 4 cams, Core, flash, button, cap = **9** on +5 V and 9 on GND,
  which no longer fits one 8-port block. Two blocks per net, jumpered with a short solid wire,
  gives 14 usable ports per net (OK'd by the owner).

## 2. Feeding the Core through Port A (brief §3.2)

The CoreS3-SE has four 5 V load switches around the AXP2101 (VERIFIED: SE schematic via M5
knowledge base, "BUS 与 USB 电源方向控制"):

| switch | type | enabled when | direction |
|---|---|---|---|
| U14 ME1502AM5G | high-enable | `BUS_OUT_EN = 1` | SY7088 boost (`BUS_5V`) → `BUS_OUT` (Core drives 5 V out) |
| U19 ME1502CM5G | low-enable | `BUS_OUT_EN = 0` | `BUS_OUT` → `VBUS` (Core is fed from the bus) |
| U17 ME1502AM5G | high-enable | `USB_OTG_EN = 1` | `BUS_OUT` → `VUSB` (Core drives USB-C, OTG) |
| U18 ME1502CM5G | low-enable | `USB_OTG_EN = 0` | `VUSB` → `VBUS` (Core is fed from USB-C) |

`BUS_OUT_EN` = AW9523B P0_1, `USB_OTG_EN` = AW9523B P0_5, `BOOST_EN` = AW9523B P1_7
(VERIFIED: SE schematic AW9523B map; M5Unified 0.2.29 `Power_Class.inl` constants
`_core_s3_bus_en = 0b00000010`, `_core_s3_usb_en = 0b00100000`, BOOST = port 1 bit 7).

This matches the owner's AXP2101 diagram (brief Appendix C, image 1): the top-left panel
(`BUS_OUT_EN=0, USB_OTG_EN=0`) is the state x4 lives in.

**(a) Grove 5 V and M-Bus 5 V are the same node.** VERIFIED: SE schematic J3 pin 3 =
`BUS_OUT`, M5-Bus pin 28 = `BUS_OUT`.

**(b) Current limit of the input path.** *Not sure.* The ME1502 datasheet was not reviewed.
M5 documents "Grove output max current (USB powered) DC 5 V @ 680 mA" for the *output*
direction (VERIFIED: SE spec table). The Core itself draws ~170 mA typical at 5 V (VERIFIED:
SE spec "USB powered, working: DC 5V@166.27mA"); backlight, WiFi and speaker peaks add to that.
x4 keeps every other load off the Core's Port A, so the switch and the Grove contacts carry
only the Core's own current (estimate < 0.5 A peak, ASSUMPTION, measure).

**(c) USB-C plugged in at the same time.** With both enables at 0, U18 (USB → VBUS) and U19
(BUS → VBUS) are both on, so the PC's USB 5 V and the bank's 5 V meet at `VBUS`.
M5Unified's own comments describe current flowing between these nodes: *"BUS_OUT_EN=0 turns the
bus input switch ON, so if BUS_OUT is at 5 V when switching, USB VBUS keeps flowing into
BUS_OUT"* (VERIFIED: M5Unified 0.2.29 `Power_Class.inl`, comment translated from Japanese).
→ **Rule kept: never USB-C and rail together.** See (d) for a related latch effect.

**(d) State before firmware runs.** *ASSUMPTION, strongly supported.* On a battery-less SE fed
only through `BUS_OUT`, U19 is the **only** path from the bus into the Core. If `BUS_OUT_EN`
were 1 at reset, U19 would be off and the board could not power up from the bus at all. M5Unified
contains explicit handling for exactly this setup: *"with no battery and external 5 V on
BUS_OUT, enabling [the output] cuts its own supply, so the request is cancelled"* (VERIFIED:
`_core_s3_ext_output_unsafe()` in M5Unified 0.2.29). The AW9523B is itself powered from the
AXP2101's 3.3 V, so it cannot drive the enable high before the PMIC is up.
Measured confirmation is bring-up step 3.

What the libraries do during boot (VERIFIED by reading the source):

- **M5GFX autodetect** (`M5GFX.cpp`, CoreS3 branch) writes the AW9523B and sets bits 0 and 2 of
  P0 (`TOUCH_RST`, `AW_RST`). It turns `BOOST_EN` on and pulses `BUS_OUT_EN` (precharge) **only
  if GPIO35/36/37 all read low with pull-ups enabled** (its "USB host module attached" heuristic).
  On x4 nothing is attached to those pins, so they read high and the bus output stays off.
  *Residual risk:* if a future M-Bus accessory pulled those pins low, M5GFX would try to enable
  the 5 V output. M5Unified's cancel logic and x4's `power::enforce()` would then turn it off
  again within milliseconds to seconds. Do not stack M-Bus modules on the x4 Core.
- **M5Unified `Power.begin()`** sets `BOOST_EN = 1` (boost running, isolated by U14 while
  `BUS_OUT_EN = 0`). Then `M5.begin()` calls `setExtOutput(cfg.output_power)`.
  **The default `cfg.output_power` is `true`.** x4 sets it to `false` in every firmware that runs
  on the Core (`app.cpp`, `main_hello_ui.cpp`). `setExtOutput(false)` also clears `BOOST_EN`.
- **x4 `power::enforce("boot")`** reads P0/P1 back over I2C, forces `BUS_OUT_EN`, `USB_OTG_EN`
  and `BOOST_EN` off if any is set, logs `err=100/101`, and prints the state in the boot banner:
  `evt=state when=boot bus_out_en=0 usb_otg_en=0 boost_en=0 vbus_mv=5012 safe=1`.
  `power::poll()` repeats the check every 2 s; the web page and diagnostics screen show it.

Exact M5Unified config names: `cfg.output_power` (5 V output to external ports), plus x4 also
sets `cfg.external_display_value = 0` and `cfg.external_speaker_value = 0` (no probing of
Port A, whose G1/G2 carry the button and flash EN), `cfg.internal_imu = false` (SE has none).

**(e) Which port.** The SE has **one** Grove port, Port A (VERIFIED: SE schematic shows a single
4-pin connector J3; Ports B and C exist only on bases the SE does not ship with). So the power
lead, the button and the flash EN share one split Grove cable on Port A; details and an
alternative on the M-Bus header are in [WIRING.md](WIRING.md).

### Alternatives if Port A ever proves unsafe

Not needed based on the evidence above. If bring-up step 3 shows 5 V on Port A with nothing
attached and the firmware running, **stop** and use one of these (ask the owner first, it changes
the wiring):

1. M-Bus pin 28 (`BUS_OUT`). Same node, so it changes nothing electrically; it only helps if
   the Grove contact is the problem.
2. USB-C input through a spare breakout (VUSB → U18 → VBUS). This is the officially intended
   input. Then the Core must never also be connected to Port A 5 V, and flashing needs the rail
   lead unplugged.

## 3. Power numbers (all estimates until measured)

| load | idle / standby | streaming | burst / peak | source |
|---|---|---|---|---|
| CamS3-5MP | ≈ 0.75 W | ≈ 1 W | ≈ 1.5 W, 0.3–0.4 A peaks @ 5 V | brief, ASSUMPTION |
| CoreS3-SE | ≈ 0.85 W (166 mA) | – | ≈ 2.5 W (0.5 A) | M5 spec (typical), peak ASSUMPTION |
| Flashlight | 0 | – | ≈ 1 A for ≈ 100 ms | owner-confirmed |
| Mini Button | ≈ 0.8 mW (10k+20k divider at 5 V) | – | – | Unit Button schematic |
| **Rig** | **≈ 3–4 W** | – | **≈ 7–10 W + flash** | brief |

Worst-case instantaneous current: 4 × 0.4 A + 0.5 A + 1 A ≈ **3.1 A**, right at the bank's
5 V ⎓ 3 A rating (VERIFIED: Anker A1653 spec page lists USB-C output 5V⎓3A; total output 18 W
max). In practice the flash fires while cameras are *capturing* (not transmitting), so the real
peak should be lower. **Measure it** (bring-up step 5). Mitigations if the bank trips or the
rail sags below ~4.6 V: lower `wifi_tx_qdbm`, keep `xfer_parallel` at 1–2, fire the flash before
transfers start (default), and only as a last resort reduce `flash_pulse_ms`.

Runtime estimate: 5000 mAh × 3.7 V / 1000 = 18.5 Wh; × 0.8 efficiency = 14.8 Wh usable.
At 3.5 W average ≈ **4.2 h**; at 7 W ≈ 2.1 h. The SE has no battery and **the bank level is not
readable**, so the UI shows no battery gauge (an estimated session time may be added later and
must be labelled as an estimate).

About the 2200 µF capacitor: it stores Q = C·ΔV = 2200 µF × 0.5 V ≈ 1.1 mC, while a 1 A × 100 ms
flash needs 100 mC. It therefore **smooths fast edges** (WiFi TX bursts, flash turn-on) but the
bank supplies the flash pulse itself. Don't expect it to "power the flash".

## 4. Known risks and mitigations

| risk | why | mitigation | check |
|---|---|---|---|
| Inrush at switch-on | 2200 µF + breakout caps + six loads starting at once can trip the bank's over-current protection | Cameras stagger their sensor init and WiFi by `id × cam_stagger_ms` (400 ms). Test as-is first. If the bank trips: move the 2200 µF to after a 5–10 Ω pre-charge resistor bypassed after ~1 s by the switch's second pole, or use 470–1000 µF. An NTC would drop too much voltage at 2–3 A once warm (ASSUMPTION) | bring-up step 4: switch on with all loads 10×, watch for bank shut-off |
| Switch rating | 3 A DC continuous + inrush | ≥ 5 A DC rated switch, in +5 V only | read the switch marking |
| Bank auto-off | many banks cut output under ~50–100 mA | x4 standby is ≈ 3–4 W (≈ 0.7 A), well above that. *Not sure* whether the A1653 has a low-current mode | bring-up step 8 (soak) |
| Flash dips the rail | 1 A step | capacitor near the loads, short 20 AWG flash branch, brown-out detector on every board, boot logs `reset=brownout` | bring-up step 5 |
| USB-C breakout without CC resistors | a C-to-C cable gives **no** power without 5.1 kΩ Rd on CC1/CC2 (USB Type-C spec) | check for resistors marked "512" or measure ~5.1 kΩ CC→GND; alternative: bank's USB-A port + A-to-C cable (no CC needed) | bring-up step 1 |
| PD voltage | a PD trigger would ask for 9 V | the breakout has no PD controller, so the bank stays at 5 V (vSafe5V). **Never** use a PD trigger board on this rail | measure 5.0 V at the rail |
| Wrong polarity / shorts | swapped red/black or a stray strand | continuity +5 V ↔ GND must NOT beep before first power-up; capacitor stripe to GND | bring-up step 1 |
| Breakout inline capacitors | position/value/voltage unknown | inspect, read markings, record in [HARDWARE.md](HARDWARE.md) | bring-up step 1 |
| Loose push-in contacts | Wago 2273 is solid-wire-only (Wago datasheet); stranded/Grove wire may not grip | solid pigtails, ferrules or Wago 221; tug-test every wire | bring-up step 1 |
| Port A pull-ups on G2 (flash EN) | if the SE had pull-ups on Port A, the flash EN would idle high before firmware drives it low | firmware drives G2 low from a static constructor; the AW3641 flash timeout (~220 ms) bounds any accidental flash; bring-up step 3 measures G2 idle voltage. If it idles high, move flash EN to M-Bus G7 (`kFlashEnAlt`) | bring-up step 3 |
| Mini Button divider at 5 V | Unit Button outputs ~2/3 VCC = 3.33 V released (10k/20k divider) into a 3.3 V GPIO | under the ESP32-S3 absolute max (VDD + 0.3 V). If your unit reads ~5 V on the signal wire, STOP: it is a different design | bring-up step 2 |
| Thermal | the cameras' ESP32-S3-WROOM-1-**N16R8** is the **65 °C ambient** version (VERIFIED: Espressif module datasheet ordering table, "N16R8 (65 °C)"; 85 °C only with PSRAM ECC enabled, which costs 1/16 of the PSRAM). Four cameras in a closed box can get there | die-temperature warning at 70 °C and throttle at 80 °C (die runs hotter than ambient; thresholds are ASSUMPTIONS to calibrate); vents in the enclosure ([ENCLOSURE](ENCLOSURE.md)); no long streaming | bring-up step 8 |
| Flash overheating | AW3641 + LED heat per flash | 8 s minimum cooldown, 150 ms cap, refuse when the Core is hot | flash stress test |

## 5. Firmware rules (implemented)

| rule | where | status |
|---|---|---|
| Flash GPIO LOW as the very first thing (static constructor + first line of `setup()`), pull-down on | `core/src/hw/flash.cpp` `earlyOff()` | done |
| Flash hard cap 150 ms, cooldown ≥ 8 s, configurable only tighter | `shared/include/x4/safety.h`, `config_schema.def` (`SAFE_MAX`/`SAFE_MIN`) | done, unit-tested |
| Pulse ended by `esp_timer`, watchdog forces LOW if overdue, shutdown handler, loop WDT | `flash.cpp`, `app.cpp` `enableLoopWDT()` | done |
| Never enable Core 5 V output / OTG; assert at boot and every 2 s; log it | `core/src/hw/power_safety.cpp` | done |
| Reset reason every boot, brown-out detector on, boot counter + brown-out counter in NVS, shown on screen and serial | `sysinfo.cpp`, `camsys.cpp`, diagnostics screen | done (BOD is on in the pioarduino sdkconfig: `CONFIG_ESP_BROWNOUT_DET=1`, level 7, VERIFIED) |
| Thermal read, warn, throttle preview/streaming, refuse flash/bursts | `app.cpp` `thermal()`, camera `/capture` + `/stream` | done |
| Stagger camera init by id | `camera/src/main.cpp` | done |
| No fake battery gauge | UI shows none; `/api/status` says so | done |

## 6. Sources

- M5Stack knowledge base (official), CoreS3-SE schematic descriptions and product page
  (SKU K128-SE): pin maps, AW9523B map, load-switch topology, spec table. Read 2026-10-31.
  Product page: https://docs.m5stack.com/en/core/M5CoreS3%20SE
- M5Unified 0.2.29 `src/utility/Power_Class.inl`:
  https://github.com/m5stack/M5Unified/blob/0.2.29/src/utility/Power_Class.inl
- M5GFX 0.2.36 `src/M5GFX.cpp` (CoreS3 autodetect): https://github.com/m5stack/M5GFX/blob/0.2.36/src/M5GFX.cpp
- Unit CamS3-5MP schematic: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1050/SCH_UNIT_CAMS3_V1.0.pdf
- Unit FlashLight schematic: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/606/UNIT-FlashLight.pdf
  and page https://docs.m5stack.com/en/unit/Unit_FlashLight (8–15 s interval note)
- Awinic AW3641EDNR: https://www.awinic.com/en/productDetail/AW3641EDNR (datasheet V1.4)
- Anker Nano Power Bank A1653: https://www.anker.com/ca/products/a1653-usb-c-portable-charger-5000mah
- Wago 2273 series data (solid conductors): https://www.wago.com/2273-208 and distributor sheet
  https://www.tlc-direct.co.uk/Technical/DataSheets/Wago/Wago_202.pdf
- Espressif ESP32-S3-WROOM-1 datasheet (ordering table, N16R8 = 65 °C):
  https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf
