# Updating the firmware over WiFi (OTA)

Once a board runs the **full firmware** (`core`, `cam1`…`cam4`, [step 13](STEPS.md#13-the-full-firmware)),
it updates over the rig's own WiFi: no USB, no Grove plugs pulled, and no chance of two 5 V
sources on one board. The bring-up firmware in `steps/` has no OTA: those still go over USB.

## Once

1. **PlatformIO on the command line** (`pio`), the same one VS Code uses: click the terminal icon
   in the PlatformIO status bar for a shell where `pio` works
   ([STEPS: level 0](STEPS.md#level-0-gear-up)).
2. **The OTA password** is `X4_OTA_PASS`: from `core/include/secrets.h` and
   `camera/include/secrets.h` if you made them, else `change-me-ota` from `secrets.h.example`.
   The script reads it from the same place the build does; set it the same on all boards.
3. **Laptop firewall:** during an upload the board connects **back** to the laptop. If an upload
   hangs at "Waiting for device", allow Python (or PlatformIO) through the firewall for private
   networks, then retry.

## Every time

1. Rig on, running from the bank. Nothing plugged into USB.
2. Laptop joins the **`x4cam`** WiFi (password `X4_AP_PASS`, default `change-me-ap`).
3. From the repo root:

```bash
python tools/ota.py
```

That updates every online camera, one at a time, waits until each has rebooted and registered
with the Core again, and then updates the Core. Other forms:

```bash
python tools/ota.py cams
```

```bash
python tools/ota.py cam2 cam4
```

```bash
python tools/ota.py core
```

```bash
python tools/ota.py --dry-run
```

4. The Core reboots last, so the WiFi drops for a few seconds: rejoin `x4cam` if the laptop
   doesn't by itself.
5. Check: Diagnostics or the web page (http://192.168.4.1) shows every camera online with the
   same firmware number as the Core. A camera's number in red means it differs
   ([FIRMWARE: firmware version](FIRMWARE.md#firmware-version)).

**Why cameras first, Core last:** when a change touches the Core↔camera protocol, a camera on the
old protocol is refused at registration (409, [PROTOCOL](PROTOCOL.md)). Updating everything in one
run keeps them matched. The Core goes last because its reboot drops the WiFi the script runs on.

### By hand (what the script runs)

Camera N (its IP is on the web page and on Diagnostics; use the env with **its** number, or it
changes identity):

```bash
PLATFORMIO_UPLOAD_FLAGS="--auth=change-me-ota" pio run -d camera -e cam2 -t upload --upload-port 192.168.4.3
```

Core:

```bash
PLATFORMIO_UPLOAD_FLAGS="--auth=change-me-ota" pio run -d core -e core -t upload --upload-port 192.168.4.1
```

PlatformIO prints a warning that it switched `upload_protocol` to `espota` because the port is an
IP address. That's expected.

## When it fails

| symptom | likely | fix |
|---|---|---|
| `Can't reach the Core` | laptop not on `x4cam`, or the rig is off | join the WiFi; switch on |
| `Authentication Failed` | password differs from the board's `X4_OTA_PASS` | same `secrets.h` values for Core and cameras; the board keeps the password it was built with |
| hangs at "Waiting for device…" | laptop firewall blocks the board's connection back | allow Python/PlatformIO on private networks |
| camera `offline, skipped` | it isn't registered with the Core | look at Diagnostics; power-cycle the rig |
| camera `didn't come back within 2 min` | the new firmware doesn't start WiFi (crash, wrong env) | that board needs one USB flash ([BRINGUP](BRINGUP.md#flashing-without-breaking-the-power-rules)); fix the cause first |
| upload stops halfway | weak WiFi, the board rebooted | retry; the board keeps the old firmware until a new one is complete (ESP32 OTA writes the other app slot) |

A broken build that crashes before WiFi comes up can only be fixed over USB: **Not sure** whether
this build rolls back to the previous firmware by itself (ASSUMPTION: it doesn't). Updating cameras
one at a time, Core last, keeps one bad build from taking out the whole rig.

## What can't go over OTA

- The `steps/` bring-up firmware (no OTA in it).
- The first flash of a board, and a board whose firmware doesn't start WiFi.
- Changes to the flash layout (partition table) or the bootloader. None are planned.
