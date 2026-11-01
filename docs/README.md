# x4 docs

| doc | what's in it |
|---|---|
| [STEPS.md](STEPS.md) | **start here when the boards arrive**: 13 small steps from "it boots" to the full rig, one tiny firmware each |
| [BRINGUP.md](BRINGUP.md) | flashing without breaking the power rules; full-firmware tests (flash stress, WiFi, skew, soak) |
| [WIRING.md](WIRING.md) | diagram, pin table, colours, connectors, spare ports, printable pre-power checklist |
| [POWER.md](POWER.md) | power architecture, why Port A back-feeding is OK, risks, firmware safety rules |
| [HARDWARE.md](HARDWARE.md) | BOM, exact variants, pin tables, confirmed vs assumed, fill-in table |
| [MEASUREMENTS.md](MEASUREMENTS.md) | empty tables to fill on the bench |
| [FIRMWARE.md](FIRMWARE.md) | toolchain + pinned versions, layout, tasks, state machine, memory, input layer, tests |
| [PROTOCOL.md](PROTOCOL.md) | UDP trigger/heartbeat/clock sync, camera + Core HTTP APIs, SD layout |
| [CONFIG.md](CONFIG.md) | every option (generated), safety limits, how to change them |
| [LOGGING.md](LOGGING.md) | log format, levels, sinks, event + error-code catalogue, the log tool |
| [UI.md](UI.md) | design language, tokens, fonts, screens/states/transitions, compromises, LVGL Pro |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | symptom → cause → check → fix |
| [DEBUGGING.md](DEBUGGING.md) | serial, diagnostics screen, web page, measuring skew and current |
| [RESEARCH.md](RESEARCH.md) | findings with sources and VERIFIED/ASSUMPTION tags |
| [DECISIONS.md](DECISIONS.md) | ADRs incl. rejected options |
| [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) | what is still open, with the default used now |
| [COMPANION.md](COMPANION.md) | web page now, Flutter later, platform constraints |
| [ENCLOSURE.md](ENCLOSURE.md) | v1 foam board, v2 acrylic, light and heat |
| [BRIEF.md](BRIEF.md) | the original kickoff brief, verbatim |

The same docs as a browsable site (search, themes, rendered diagrams): [`site/`](../site/README.md),
`cd site && npm i && npm run dev`.

Tags used everywhere: **VERIFIED** (with source), **ASSUMPTION** (reasoned, to measure),
**Not sure** (unknown).
