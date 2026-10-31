# Enclosure

The owner's renders (brief Appendix C, images 2 and 3) are a **rough sketch of intent, not a
source of truth**: they show 5-port blocks and two buttons, while the real parts are Wago
2273-208 and one Mini Button. Use them for the general feel and envelope only.

What the renders show (described, the images were not provided as files; add them to
`docs/assets/` if you want them versioned):
- **Rear view:** grey mock-up in a thin bounding-box frame; the Core's back shell in the middle; a
  small M5 unit top-left; a row of Wago-style blocks along the bottom; the power bank standing on
  the right edge with a two-button part and a bracket on top; a USB-C breakout (pads G, D+, D−, V)
  next to the bank with a USB-C plug.
- **Front view:** bank on the left edge with the two-button part on top; the Core above; four
  CamS3 units in a row with conical lens collars and base clips; a unit top-right; one lens hole
  shows a small green PCB.

## v1: foam board + cardboard

- Skeleton: 3–5 mm foam board; skin: cardboard.
- Joints: low-temperature hot glue, double-sided foam tape, hook-and-loop for modules you want to
  remove (bank, Core, flash). **No superglue or solvent glue** near foam (melts it) or lenses
  (cyanoacrylate fumes fog lenses).
- Mount the four cameras on a **rigid inner frame** (a stiff strip of thick board or a wooden
  ruler) so their relative pose does not change between shots.
- **Shield the flash and every lens with matte black opaque sleeves**: clear plastic carries stray
  light (flash light piped into lenses). Test in a dark room with the flash.
- **Vents**: sealed shells trap heat; the cameras' modules are 65 °C-ambient parts (VERIFIED,
  Espressif). Leave openings near each camera and the Core, ideally low intake + high exhaust.
- Keep access to the bank's charging port, the Core's USB-C (first flash), the cameras' Grove
  ports and the G0 header (download mode), and the microSD slot.
- Leave room for extra feature buttons and a scroll wheel (G5/G6/G8/G9 on the M-Bus header).
- Wago blocks and the capacitor inside, away from the lenses; tape the cut camera data wires.

## v2: 3D-printed or laser-cut translucent acrylic

- Y2K / old-Mac look in translucent acrylic; still keep **opaque matte black sleeves** around
  lenses and the flash inside the translucent shell.
- Structural screws: brass heat-set inserts + M2 / M2.5 screws.
- Screw-hole positions: M5's structure files on GitHub (`M5_Hardware`), e.g.
  `Products/U174‑B_Unit_CamS3‑5MP/Structures` and `Products/K128-Lite_CoreS3-Lite/Structures`
  (the SE shares the CoreS3-Lite body per M5's SE page drawing link). The SE ships with a 2.0 mm
  hex key for M2.5 screws.
- Vents designed in from the start; consider a small channel past all four cameras.
