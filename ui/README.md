# ui/ — design source of truth

- `tokens.json` — colours, type, layout, motion, taken from the Figma section
  fgEvP6Ms0bOAbWxfkFki4R / 73:368 (`figma: true` marks values from the file). Regenerate the C
  header with `python tools/gen_tokens.py` (writes `core/src/ui/tokens.h`; CI checks it).
- `fonts/` — Instrument Serif (Regular, Italic) and Space Mono (Regular, Bold) TTFs with their
  SIL OFL 1.1 licences. Convert with `bash tools/gen_fonts.sh` (lv_font_conv 1.5.2, 4 bpp).
- `assets/figma/` — images exported from Figma (overlays, flash bolt, dither strip, corner glows).
  `_reference-*.png` are design renders for comparison only. Convert with
  `python tools/gen_assets.py` (needs Pillow) → `core/src/ui/assets/`.
- An LVGL Pro / XML editor project can live here later; its C export goes to `core/src/ui_gen/`.

Details: ../docs/UI.md
