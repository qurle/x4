# Debugging

## Serial logging conventions

- One port per board: `pio device monitor -e core` / `-e cam2` (115200, exception decoder on).
- Everything is x4 logfmt ([LOGGING.md](LOGGING.md)); camera WARN+ lines also arrive on the Core,
  so the Core's port alone shows the whole rig in the field.
- Raise detail live without reflashing: web page → Config → `log_level` = 3 (DEBUG) or 4 (TRACE,
  includes every heartbeat). Cameras get `log_level` from the Core at their next registration.
- Save a session: `pio device monitor -e core | tee core.log`, then
  `python tools/x4log.py core.log --shots` or `--lvl WARN`.

## Diagnostics screen (on the Core)

Long-press the shutter (or long-press the screen / swipe up). Shows FPS + worst frame time, 5 V
output + OTG state, rail mV (AXP2101 VBUS), die temperature, heap/min heap/PSRAM, reset reason,
boot and brown-out counters, SD state and speed, button state, flash state and pulse count; per
camera online, IP, temperature, RSSI, heartbeat age, last shot result and capture offset; last
shot total/skew; the last 6 log lines. Buttons: Arm, Flash (test pulse, on-screen confirm), SD
(speed test), Shoot.

## Web debug page

Join `x4-rig`, open `http://192.168.4.1/`. Status card (state, faults, power, system, SD, UI,
flash, button), cameras table with a transfer bar per camera, live log tail with level/text
filter and pause, full config table with inline editing (safety options marked ⚑, reboot ↻),
photos list with thumbnails and `shot.json`, bring-up actions (arm, test pulse with confirm,
reboot). Raw JSON: `/api/status`, `/config`, `/logs?since=0`, `/api/shots`.

## Measuring shutter skew

Three independent methods; use at least two.

1. **Timestamps (built in).** Each camera reports the frame-start time of the frame it kept,
   converted to Core time. The Core logs `skew_us` (max − min) in `evt=captured` and per camera
   `cap_vs_at_us` in `evt=ack`. Accuracy is limited by clock sync (≈ RTT/2; the camera's
   `/info` shows `clock.rtt_us`). Fast, every shot.
2. **Phone slow motion + flash/LED.** Put the four cameras so they all see a phone screen running
   a millisecond stopwatch (or an LED blinking at a known rate). Take a shot, compare the stopwatch
   value in the four photos. Resolution = the phone screen refresh (8–16 ms) — enough for the
   50–250 ms budget. With the flash: film the rig at 240 fps and compare the flash moment with the
   camera LEDs (keep the camera LEDs off normally; they point backwards).
3. **Network ACK times.** `rx=` in each ACK is when the camera received the SHOOT (Core time);
   the spread of `rx` vs `at` tells you whether the trigger lead (`trig_lead_ms`) is long enough.
   If any camera's `rx` is after `at`, it logs `evt=unscheduled late_us=` → raise the lead.

## Multimeter and USB meter

- Continuity before power (rail-to-rail must not beep), polarity per branch.
- DC volts at the rail with no load, with all loads, and at the Core's Port A pins.
- USB meter between bank and breakout: average and peak current; most meters show min voltage.
- Current per device: put the USB meter in that board's branch only (via a spare breakout or a
  meter with screw terminals), everything else unplugged.

## Measuring current with the flash firing

A USB meter's sampling is too slow to see a 100 ms pulse's true peak but shows the dip. Better:
a 0.1 Ω shunt in the flash's GND branch and a scope (100 mV = 1 A), or a clamp meter on the 20 AWG
branch. Record rail minimum with the scope probe on the capacitor. Fire at the 8 s cooldown, 10×.

## Common investigations

- *Who reset?* `grep 'evt=reset'` per device → `reason=brownout|panic|task_wdt|…`.
- *Why was the shutter refused?* `evt=refused state=… why=…`.
- *Where did the 5 s go?* `--shots` table (capture vs transfer vs SD), then `evt=done kBps=`.
- *Is the clock sync good?* camera `/info` → `clock.synced`, `rtt_us`; Core ACK `cap_vs_at_us`.
- *Is the flash guard working?* `mod=flash` lines: `fire`, `refused verdict=cooldown wait_ms=…`.
