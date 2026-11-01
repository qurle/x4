# x4cam

my first hardware project i guess

![](/.github/assets/cover.png)

A portable, battery-powered rig with **four cameras that shoot at the same moment**. An M5Stack
CoreS3-SE with a touchscreen runs the whole UI, triggers four Unit CamS3-5MP cameras over its own
WiFi, fires a flash, and stores all four full 5 MP photos on its microSD within ~5 s. Offline,
no router, no phone needed.

```
 [bank]─USB-C─[switch]─┬─ 5 V rail ─┬─ cam1 cam2 cam3 cam4   (WiFi → Core AP)
                       │            ├─ Core (Port A, also button + flash EN)
                       └─ GND rail ─┴─ flash, button, 2200 µF
```

## Status

| stage | what | state |
|---|---|---|
| 0 | flashable hello builds, logging + config, power-safety assertion, bring-up tests, camera bench incl. 5 MP | built, unit-tested, **needs hardware** |
| 1 | SoftAP, registration, heartbeat + clock sync, UDP trigger, status on screen | built, needs hardware |
| 2 | shooting state machine, input abstraction, flash safety, capture-then-transfer, SD layout, web debug page | built, needs hardware |
| 3 | UI v2 from the Figma file (photo-first viewfinder, tap menu, settings tiles, Saving/Error overlays, Instrument Serif + Space Mono) + diagnostics | built, needs the panel to judge |
| 4 | live preview from one camera | built, needs measuring |
| 5 | OTA over the AP, docs, measurement templates | OTA built; measurements pending |

## Quick start

1. VS Code + PlatformIO IDE (accept the recommended extensions).
2. Copy the secrets and set your own AP password:
   ```bash
   cp core/include/secrets.h.example core/include/secrets.h
   ```
   ```bash
   cp camera/include/secrets.h.example camera/include/secrets.h
   ```
3. Build and test (no boards needed):
   ```bash
   cd core && pio run && pio test -e native
   ```
   ```bash
   cd camera && pio run
   ```
4. Before the boards arrive: click through the UI on your computer,
   `python tools/ui_sim/run.py --live` ([docs/DEVELOP.md](docs/DEVELOP.md): build, test, simulate).
5. When the boards arrive: **[docs/STEPS.md](docs/STEPS.md)**: 13 small steps, each with its
   own tiny firmware in `steps/`, from "the Core boots" to the full rig. Read the one-5V-source
   rule at the top of [docs/WIRING.md](docs/WIRING.md) before you plug anything in.

Firmware environments: Core `core`, `core-hello`, `hello-ui`; cameras `cam1`…`cam4`,
`cam-hello`; tests `native`; step-by-step bring-up `s01-hello` … `s10-camN` in `steps/`.
CI builds all of them.

## Layout

`steps/` small bring-up firmware, one per step · `core/` Core firmware · `camera/` camera firmware · `shared/` code both use (logging, config,
safety, protocol, state machine) · `ui/` design tokens + fonts · `tools/` generators + log tool ·
`app/` companion (later) · `docs/` everything else ([index](docs/README.md)) · `site/` the docs
as a browsable manual (notes. template, `cd site && npm i && npm run dev`).

## Read first

- [Power and safety](docs/POWER.md) — why the Core can be fed through Port A, and what never to do
- [Open questions](docs/OPEN_QUESTIONS.md) — what still needs your input (Q17–Q20 are new from the Figma pass)
- [Decisions](docs/DECISIONS.md)
