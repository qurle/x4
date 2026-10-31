# ui/ — design source of truth

- `tokens.json` — colours, type, spacing, radii, borders, motion. **Placeholder values** until the
  Figma node (fgEvP6Ms0bOAbWxfkFki4R / 73:368) is imported. Regenerate the C header with
  `python tools/gen_tokens.py` (writes `core/src/ui/tokens.h`; CI checks it is fresh).
- `fonts/` — Instrument Serif (Regular, Italic) and Space Mono (Regular, Bold) TTFs with their
  SIL OFL 1.1 licences. Convert with `bash tools/gen_fonts.sh` (lv_font_conv 1.5.2, 4 bpp).
- `assets/` — exported icons/images from Figma (empty for now).
- An LVGL Pro / XML editor project can live here later; its C export goes to `core/src/ui_gen/`.

Details: ../docs/UI.md
