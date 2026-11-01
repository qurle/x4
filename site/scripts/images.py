#!/usr/bin/env python3
"""Refreshes the website's pictures of the device UI: public/screens/*.png and public/og.png.

    python tools/ui_sim/run.py preview_photo settings saving_3of4 preview_portrait
    python site/scripts/images.py

The screens are the UI simulator's renders (the real UI code, tools/ui_sim), not mock-ups.
Needs Pillow (13.0.0, like the rest of the repo).
"""
import pathlib

from PIL import Image, ImageDraw, ImageFont

SITE = pathlib.Path(__file__).resolve().parent.parent
ROOT = SITE.parent
SIM = ROOT / "tools" / "ui_sim" / "out"
SCREENS = {"viewfinder": "preview_photo", "settings": "settings", "saving": "saving_3of4", "portrait": "preview_portrait"}

out = SITE / "public" / "screens"
out.mkdir(parents=True, exist_ok=True)
for name, scene in SCREENS.items():
    src = SIM / f"{scene}.png"
    if not src.exists():
        raise SystemExit(f"{src} missing: run tools/ui_sim/run.py {scene} first")
    Image.open(src).convert("RGB").save(out / f"{name}.png", optimize=True)

# Social preview, 1200 x 630: the name in Instrument Serif on the settings grey, the viewfinder
# at 2x in a black bezel.
og = Image.new("RGB", (1200, 630), (0xD1, 0xD1, 0xD1))
d = ImageDraw.Draw(og)
serif = ImageFont.truetype(str(ROOT / "ui" / "fonts" / "InstrumentSerif-Regular.ttf"), 210)
d.text((64, 150), "x4cam", font=serif, fill=(0, 0, 0))
small = ImageFont.truetype(str(ROOT / "ui" / "fonts" / "InstrumentSerif-Regular.ttf"), 44)
d.text((70, 420), "Four cameras, one button.", font=small, fill=(0, 0, 0))
screen = Image.open(out / "viewfinder.png").resize((480, 360), Image.LANCZOS)
d.rounded_rectangle((640, 127, 640 + 480 + 32, 127 + 360 + 32), radius=14, fill=(0, 0, 0))
og.paste(screen, (656, 143))
og.save(SITE / "public" / "og.png", optimize=True)
print("wrote public/screens/*.png and public/og.png")
