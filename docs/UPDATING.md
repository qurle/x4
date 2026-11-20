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
2. **Mac:** stay on your home WiFi, the script switches for you (below). **Other computers:**
   join the **`x4cam`** WiFi first (password `X4_AP_PASS`, default `change-me-ap`).
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

4. The Core reboots last, so its WiFi drops for a few seconds. On a Mac the script rejoins your
   home WiFi right after; elsewhere rejoin `x4cam` if you still need it.
5. Check: Diagnostics or the web page (http://192.168.4.1) shows every camera online with the
   same firmware number as the Core. A camera's number in red means it differs
   ([FIRMWARE: firmware version](FIRMWARE.md#firmware-version)).

### On a Mac: WiFi switching

The script does what you'd do by hand, with `networksetup` (built into macOS):
1. **Builds first**, while you still have internet (a first build downloads packages).
2. If the Core isn't reachable, **joins `x4cam`** with `X4_AP_SSID` / `X4_AP_PASS` from
   `core/include/secrets.h` (or the example file).
3. Updates, then **rejoins your home WiFi**. Also after a failure or Ctrl-C.

Which network to go back to: macOS 15+ often hides the current network's name from scripts
(location privacy), so tell it once:

```bash
echo 'export X4_HOME_WIFI="Your Home WiFi"' >> ~/.zshrc
```

Without it the script reads the name if macOS lets it, else asks; Enter means stay on `x4cam`.
If rejoining fails because macOS wants the password, also set `X4_HOME_WIFI_PASS` the same way
(**Not sure** whether macOS takes it from the Keychain by itself: try without first). Options:
`--home "Name"` for one run, `--stay` to stay on `x4cam`, `--no-wifi` to leave WiFi alone. If you
were already on `x4cam`, the script doesn't switch at all.

Alternative with no switching: internet over a cable (USB-C Ethernet or iPhone USB tethering),
WiFi left on `x4cam`, and the cable above Wi-Fi in *System Settings → Network → … → Set Service
Order*. Then run with `--no-wifi`.

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
| `Can't reach the Core` / `Couldn't reach the Core on x4cam` | rig off or still booting; laptop not on `x4cam` (not a Mac); wrong `X4_AP_PASS` | switch on and wait ~10 s; check `secrets.h` matches what the Core was built with |
| `couldn't rejoin <home>` | macOS wants the home password, or the name is wrong | set `X4_HOME_WIFI_PASS`, check `X4_HOME_WIFI`; pick it from the WiFi menu for now |
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
