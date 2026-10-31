# Protocol (Core ↔ cameras ↔ companion)

Version **1** (`X4_PROTOCOL_VERSION`). All devices on the Core's SoftAP, Core = `192.168.4.1`.
Source of truth for names and ports: `shared/include/x4/protocol.h` (tested in
`core/test/test_shared`).

## Versioning

- UDP datagrams start with `X4/<version>`. A receiver drops datagrams with a different version and
  logs `err=406 NET_PROTO_MISMATCH` (rate-limited).
- `POST /register` carries `proto`; the Core answers **409** for a mismatch.
- Additive changes (new keys) keep the version; parsers ignore unknown keys. Renames/removals or
  changed meaning bump it.

## UDP

| port | direction | messages |
|---|---|---|
| 4210 | Core → cameras (broadcast 255.255.255.255 + unicast) | `SHOOT`, `HBA`, `CMD` |
| 4211 | cameras → Core (unicast) | `HB`, `ACK`, `LOG` |

Grammar: `X4/1 <TYPE> key=value key=value …` on one line, ASCII, ≤ 196 bytes.

```
X4/1 SHOOT shot=42 at=123456789 fl=1 res=5 dl=0
X4/1 HB cam=2 seq=118 t1=98765432 heap=180000 psram=2800000 temp=51 st=1 rssi=-48 held=0
X4/1 HBA seq=118 t1=98765432 t2=123400000
X4/1 ACK cam=2 shot=42 ok=1 rx=123335000 cap=123458800 bytes=1534221 cap_ms=211 err=0
X4/1 LOG cam=2 t=5512 lvl=WARN dev=cam2 mod=therm evt=warn temp_c=71.2
X4/1 CMD cmd=reboot
```

| message | fields |
|---|---|
| `SHOOT` | `shot` (id, persistent counter on the Core), `at` = capture time in **Core** µs (`esp_timer`), 0 = now; `fl` 1 if the Core fires the flash; `res` = `cam_res` value; `dl` = per-camera delay in µs (camera N captures at `at + (N−1)·dl`; 0 = simultaneous; missing = 0) |
| `HB` | `cam` 1–4, `seq`, `t1` camera µs at send, `heap`, `psram`, `temp` °C (die), `st` camera state (0 booting, 1 idle, 2 armed, 3 capturing, 4 holding, 5 streaming, 6 error), `rssi`, `held` shot id held (0 none) |
| `HBA` | echoes `seq`, `t1`; `t2` = Core µs when the HB arrived |
| `ACK` | `cam`, `shot`, `ok`, `rx` / `cap` = receive and frame-start times **converted to Core µs**, `bytes`, `cap_ms` (grab duration), `err` (x4 error code if `ok=0`) |
| `LOG` | `cam`, then one complete x4 log line |
| `CMD` | `cmd` (`reboot`), optional args |

**Trigger fan-out.** The Core never triggers cameras one by one in sequence: one broadcast plus
one unicast per known camera, repeated `trig_repeat` (3) times about 8 ms apart. Unicast copies
matter: an AP buffers broadcasts until the next DTIM when any station (often a phone) is in power
save. Cameras de-duplicate by `shot`.

**Clock sync.** Every heartbeat is an NTP-style exchange: camera sends `t1`, Core stamps `t2`
on arrival and answers at once, camera stamps `t3` on receipt. `offset = t2 − (t1 + t3)/2`,
error ≤ RTT/2. The camera keeps the lowest-RTT sample of the last 8 (`ClockSync`). A SHOOT with
`at` is executed at `at − offset` in local time; without sync (or if already late) the camera
shoots immediately and logs `evt=unscheduled`.

## Camera HTTP API (port 80, CORS `*`)

| method + path | response |
|---|---|
| `GET /info` | JSON: id, fw, git, proto, mac, ip, rssi, uptime, reset reason, boots, chip, flash, PSRAM total/free, heap, temp, sensor {name, pid, ok, hw_rev, module_fw}, state, held_shot, registered, clock {synced, offset_us, rtt_us}, config |
| `GET /health` | `{"ok":true,"state":"idle","heap":…,"temp_c":…}` |
| `GET /capture?res=0..5` | a fresh JPEG at that size (default `cam_res`), header `X-Grab-Ms`. Refused (500 "too hot") above `therm_thr_c` for res ≥ 1 |
| `GET /shot?id=N` | the **held** JPEG of shot N (`Content-Length`, `X-Shot`); released after a successful send. **410 Gone** if not held (`err=205`) |
| `GET/POST /settings?key=value[&…]` | sets camera-scope config (persisted in NVS) or `cam_id=1..4` (reboot to apply); JSON results |
| `GET /logs` | last 60 log lines, text |
| `GET :81/stream?res=0..5` | MJPEG `multipart/x-mixed-replace; boundary=x4frame`; pauses while a shot is held |

Baseline from earlier work kept: `/capture` and `/settings` on :80, MJPEG `/stream` on :81, CORS.

## Core HTTP API (port 80)

| method + path | purpose |
|---|---|
| `GET /` | web debug page |
| `POST /register` | camera registration. Body: `{"id":2,"proto":1,"fw":"0.1.0","git":"abc","mac":"…","ip":"…","sensor_pid":926,"sensor":"MEGA_CCM","hw_rev":1,"heap":…,"psram":…,"chip_rev":…}` → `{"ok":true,"core_fw":"0.1.0","proto":1,"config":{<camera-scope options>}}`. 400 bad JSON/id, 409 protocol mismatch |
| `GET /api/status` | JSON snapshot: state, faults bitmask (1 no cams, 2 SD missing, 4 SD full, 8 overheat), shots_taken, last_shot {id,total_ms,captured_ms,skew_us,stored,expected}, cams[] {id, online, ip, fw, sensor, temp_c, rssi, state, hb_age_ms, shot_ok/stored/failed, bytes, xfer_bytes/total, cap_offset_us, cap_ms}, flash {…}, power {safe, bus_out_en, usb_otg_en, vbus_mv, battery}, system {…}, sd {…}, ui {fps, frame_max_us, preview_on, preview_fps, button_down} |
| `GET /logs?since=<seq>` | log lines newer than `seq`; header `X-Log-Seq` = newest seq |
| `GET /config` | options with value, default, min, max, unit, safety, reboot, doc |
| `POST /config?key=value[&…]` | set options (tighten-only rules); JSON per key: ok, value, clamped |
| `POST /api/action?a=…` | `shutter`, `flash_arm`, `flash_disarm`, `flash_test` (50 ms), `sd_bench`, `preview_on`, `preview_off`, `reboot` |
| `GET /api/shots` | newest 24 shots with their files and sizes |
| `GET /file?path=/x4/…` | stream a file from the SD (only below `/x4/`) |

Error body (all endpoints): `{"ok":false,"err":<code>,"err_name":"…","detail":"…"}`.

## SD layout (FAT32)

```
/x4/shots/S000042/cam1.jpg … cam4.jpg
/x4/shots/S000042/shot.json
/x4/logs/x4.log, x4.1.log         (only with log_sd=1)
```
`shot.json` (`format: "x4-shot/1"`): shot id, fw, git, rtc (empty: RTC not set), res, quality,
flash, temp_c, orientation (`horizontal`/`portrait`), effect (`normal`/`dithered`), cam_delay_ms, t0_ms, at_us, captured_ms, done_ms, total_ms, skew_us, and per camera: cam, ok,
stored, bytes, cap_ms, cap_us, rx_us, stored_ms, tries, file. Builder:
`shared/src/storage_layout.cpp` (unit-tested).
