# ui/ — design source of truth

- `figma/` — raw pull of the Figma file fgEvP6Ms0bOAbWxfkFki4R (`python tools/figma_pull.py`,
  needs `FIGMA_TOKEN`): node JSON, component list, image fills, reference renders.
- `tokens.json` — colours, type, layout, motion, read from the node JSON in `figma/nodes/`
  (`figma: true` marks values from the file). Regenerate the C
  header with `python tools/gen_tokens.py` (writes `core/src/ui/tokens.h`; CI checks it).
- `fonts/` — Instrument Serif (Regular, Italic) and Space Mono (Regular, Bold) TTFs with their
  SIL OFL 1.1 licences. Convert with `bash tools/gen_fonts.sh` (lv_font_conv 1.5.2, 4 bpp).
- `assets/figma/` — UI bitmaps (overlays, flash bolt, dither strip, glows), built from `figma/` by
  `python tools/gen_figma_assets.py`. `_reference-*.png` are design renders for comparison only.
  Convert with `python tools/gen_assets.py` (needs Pillow) → `core/src/ui/assets/`.
- An LVGL Pro / XML editor project can live here later; its C export goes to `core/src/ui_gen/`.

Details: ../docs/UI.md
