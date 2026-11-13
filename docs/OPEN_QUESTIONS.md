# Open questions

Each has the **default x4 uses now** (so nothing is blocked) and what would change the plan.
Items marked ⚠ could cost a component if the default is wrong; their default is the safe choice.

| # | question | default now (ASSUMPTION) | how to settle | blocking? |
|---|---|---|---|---|
| Q1 | Which Grove port/pins for the Core power lead, button and flash? | The SE has **only Port A**: one split Grove cable, red/black = rail, white G1 = button, yellow G2 = flash EN. Fallback: flash EN on M-Bus G7, button on G8 | bring-up step 3 (G2 idle voltage) | no |
| Q2 | ~~Figma access~~ **resolved 2026-10-31**: connector re-linked to the personal account; UI v2 implemented from node 73:368 | – | – | closed |
| Q3 | USB-C breakout inline capacitor: input or output side, value, voltage? (**CC resistors: present**, 2200 µF cap: **15 V** — answered 2026-10-31) | treated as unknown; checked before first power-up | bring-up step 1, record in HARDWARE.md | no |
| Q4 | ~~Font glyph sets~~ **answered 2026-10-31: Latin only, no Cyrillic** | Latin + ° · × – — … • | – | closed |
| Q5 | Flash mode trigger semantics on the real unit | slide switch FLASH, one rising edge, EN high for `flash_pulse_ms` (100 ms), hardware timeout ≈ 220 ms as backstop | scope or 240 fps slow-mo at bring-up step 5 | no |
| Q6 | What would a second button / scroll wheel be? | GPIO buttons on G8/G9; quadrature wheel on G5/G6 (stub driver); I2C units need Port A moved | owner picks parts | no |
| Q7 | ⚠ Mini Button Unit exact model/circuit | Unit Button–style 10k/20k divider, active low, 5 V VCC → 3.33 V released | measure (bring-up step 2); if ≈ 5 V released, power it from 3.3 V via a level-safe design or use a bare switch to GND | no |
| Q8 | ~~Wago 2273 is solid-wire-only: pigtails, ferrules or Wago 221?~~ **answered 2026-11-13**: Wago 221 (owner). Grove ends folded double or pigtailed ([WIRING](WIRING.md#connectors)) | – | – | closed |
| Q9 | CamS3 hardware revision of the units you buy (0x01 new / 0xFF old) | new revision assumed; old logs `err=206` | boot log | only for old units |
| Q10 | MEGA_CCM JPEG quality values (0/1/2?) and resulting 5 MP sizes | `cam_quality=1` (default), 2.5 MB buffers | `cam-hello` bench at each value | no |
| Q11 | Bank low-current auto-off threshold, trickle mode | x4 idle ≈ 0.7 A, assumed safe | bring-up step 8 | no |
| Q12 | Real power peaks vs the bank's 3 A | estimated ≈ 3.1 A worst case | bring-up steps 4–5 | no |
| Q13 | Which camera streams the preview, and is its resolution-switch time OK? | camera 1 | `evt=res switch_ms=` | no |
| Q14 | Companion app scope (post-processing: alignment, dither, GIF/wiggle?) | not started; web page first | owner | no |
| Q15 | Should shooting require all four cameras? | `shoot_min_cams=1` (shoot with what's online, mark missing) | owner; set 4 to refuse | no |
| Q16 | ~~RTC clock set so files carry real timestamps?~~ **answered 2026-11-11**: no. Names are the shot counter only (`/x4/000042-1.jpg`), the RTC loses time at every power-off (ADR-019) | – | – | closed |

| Q17 | ~~Footer left slot~~ **answered 2026-11-01 (revised)**: record state (dot + `-:--` / timer), shown only in video mode as in Figma's Footer State "Video" (owner, 2026-11-01). The battery estimate was removed; use the bank's indicator | – | – | closed |
| Q18 | Cam delay = **between cameras** (owner). Is 0 (all four at once) allowed? Owner worried about data/current bursts | 0 kept as an option and default: capture happens into each camera's own PSRAM (sensors already run continuously), uploads are staggered by `xfer_parallel`, so 0 adds no data burst and little current; the flash is the real peak | measure rail sag on a 0-delay shot with flash (bring-up step 5/7); if it dips, make 0.2 s the minimum | no |
| Q19 | ~~video / media~~ **answered 2026-11-01**: hidden in v1 | – | – | closed |
| Q20 | Breakout CC2 read 9.2 kΩ, but measured with the bank connected (invalid for an ohm reading) | CC1 ≈ 5 kΩ OK; re-measure CC2 unplugged | owner, 1 minute with the meter | no |
| Q21 | Motion blurs indoors (owner, step 8, indoor light), but the rig is meant for action moments and dim parties. Can the camera's exposure be set short by hand? | auto exposure (the module's own); no exposure options in the full firmware | [step 15](STEPS.md#15-freeze-motion-exposure-test) (`s15-cam-exposure`): manual exposure/gain via registers 0x0130–0x0134, units Not sure. If it works: config options + a strobe-flash experiment. If not: more continuous light or another camera module | not for the rig to work; yes for action photos |
| Q22 | Can the owner's Android phone talk to the rig while staying on its home WiFi? (Android 12+ "concurrent local-only and internet connection", optional per phone) | the app joins the rig's WiFi and the phone's own WiFi drops while connected | a small test app showing `isStaConcurrencyForLocalOnlyConnectionsSupported()` and a real connection to step 8's camera WiFi ([COMPANION](COMPANION.md#android-staying-on-your-own-wifi-investigated-2026-11-13)). Needs Android Studio, or `dl.google.com` allowed in the cloud environment | no |

Questions the owner already answered (brief §12) are not repeated here.
