#!/usr/bin/env bash
# Converts ui/fonts/*.ttf into LVGL C fonts in core/src/ui/fonts/ with lv_font_conv.
#   npm i -g lv_font_conv@1.5.2    (or: npx lv_font_conv@1.5.2 ...)
#   bash tools/gen_fonts.sh
# Glyphs: printable ASCII + a few typographic symbols. No Cyrillic (owner decision 2026-10-31);
# add 0x400-0x4FF to RANGE and re-run if that ever changes.
# 4 bpp = 16 levels of anti-aliasing (the LVGL default for good small-text quality).
set -euo pipefail
cd "$(dirname "$0")/.."
CONV=${LV_FONT_CONV:-lv_font_conv}
OUT=core/src/ui/fonts
RANGE="0x20-0x7E,0xB0,0xB7,0xD7,0x2013,0x2014,0x2026,0x2022"
mkdir -p "$OUT"
gen() { # name ttf size
  "$CONV" --no-compress --no-prefilter --bpp 4 --size "$3" --font "ui/fonts/$2" \
    -r "$RANGE" --format lvgl --lv-include lvgl.h -o "$OUT/$1.c" --force-fast-kern-format
  echo "generated $OUT/$1.c"
}
# Sizes follow the Figma file (node 73:368): serif 48 (Saving/Error), 36 + 34 (settings tiles),
# 32 (tap menu), 24 ("1 of 4"); Space Mono Bold 13 (footer). 64 = splash, mono 12/14/18 = diagnostics.
gen x4_serif_64 InstrumentSerif-Regular.ttf 64
gen x4_serif_48 InstrumentSerif-Regular.ttf 48
gen x4_serif_36 InstrumentSerif-Regular.ttf 36
gen x4_serif_34 InstrumentSerif-Regular.ttf 34
gen x4_serif_32 InstrumentSerif-Regular.ttf 32
gen x4_serif_24 InstrumentSerif-Regular.ttf 24
gen x4_mono_12 SpaceMono-Regular.ttf 12
gen x4_mono_14 SpaceMono-Regular.ttf 14
gen x4_mono_18 SpaceMono-Regular.ttf 18
gen x4_mono_bold_13 SpaceMono-Bold.ttf 13
gen x4_mono_bold_14 SpaceMono-Bold.ttf 14
