# steps/: bring-up, one small thing at a time

Tiny test firmware for the 13-step ladder in [docs/STEPS.md](../docs/STEPS.md). Each step is one
short `src/sNN_*/main.cpp` built on M5Unified + plain Arduino only (no LVGL, no `shared/`), so
you can read exactly what it does before you flash it.

```bash
pio run -e s01-hello -t upload
```
```bash
pio device monitor -e s01-hello
```

Shared by the steps: `include/core_common.h` (Core pins, 5 V outputs off + readback, flash pin
LOW from boot), `include/cam_common.h` (camera pins + init), `include/net_common.h` (the steps'
WiFi name and fixed camera addresses). The steps' WiFi uses bench defaults (`x4cam-steps`), not
your `secrets.h`.
