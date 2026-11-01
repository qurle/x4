#!/usr/bin/env python3
"""Builds the UI bitmaps in ui/assets/figma/ from the pulled Figma data (ui/figma/), offline.

    pip install pillow
    python tools/gen_figma_assets.py          # write the PNGs
    python tools/gen_figma_assets.py --check  # CI: exit 1 if a PNG differs from what the data gives

Then `python tools/gen_assets.py` turns the PNGs into LVGL C images.

Sources (refresh with tools/figma_pull.py):
    ui/figma/nodes/*.json   vector shapes, gradients, blurs, opacities
    ui/figma/fills/*.png    image fills of the overlay and dither nodes, as uploaded to Figma

Blur: Figma's LAYER_BLUR radius r is drawn here as a gaussian with sigma = 0.42 * r. Fitted to
the Figma renders of 18:523 (footer corner, r 32: best sigma 13-13.5) and 83:867 (record glow,
r 16: best sigma 6.5-7); mean alpha error < 0.4 %.
"""
import json
import math
import pathlib
import re
import sys

from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parent.parent
NODES = ROOT / "ui" / "figma" / "nodes"
FILLS = ROOT / "ui" / "figma" / "fills"
OUT = ROOT / "ui" / "assets" / "figma"
SIGMA_PER_RADIUS = 0.42
SS = 4  # supersampling for shapes


# ------------------------------------------------------------------ node lookup
_index = {}


def node(nid):
    if not _index:
        for f in NODES.glob("*.json"):
            stack = [json.loads(f.read_text())["document"]]
            while stack:
                n = stack.pop()
                _index.setdefault(n["id"], n)
                stack.extend(n.get("children", []))
    if nid not in _index:
        sys.exit(f"node {nid} not in ui/figma/nodes (run tools/figma_pull.py)")
    return _index[nid]


def rgb(c):
    return tuple(round(c[k] * 255) for k in "rgb")


def blur_radius(n):
    for e in n.get("effects", []):
        if e["type"] == "LAYER_BLUR" and e.get("visible", True):
            return e["radius"]
    return 0.0


# ------------------------------------------------------------------ small float-image helpers
def gauss_kernel(sigma):
    k = int(math.ceil(3 * sigma))
    g = [math.exp(-(i * i) / (2 * sigma * sigma)) for i in range(-k, k + 1)]
    s = sum(g)
    return k, [v / s for v in g]


def blur(a, w, h, sigma):
    """Separable gaussian on a row-major float list, zero outside."""
    k, g = gauss_kernel(sigma)
    tmp = [0.0] * (w * h)
    for y in range(h):
        row = a[y * w:(y + 1) * w]
        for x in range(w):
            s = 0.0
            for i, gv in enumerate(g):
                xx = x + i - k
                if 0 <= xx < w:
                    s += row[xx] * gv
            tmp[y * w + x] = s
    out = [0.0] * (w * h)
    for x in range(w):
        for y in range(h):
            s = 0.0
            for i, gv in enumerate(g):
                yy = y + i - k
                if 0 <= yy < h:
                    s += tmp[yy * w + x] * gv
            out[y * w + x] = s
    return out


def ellipse_alpha(w, h, cx, cy, rx, ry, alpha_at):
    """Supersampled ellipse coverage; alpha_at(t) gives the fill alpha at normalised radius t."""
    a = [0.0] * (w * h)
    for y in range(h):
        for x in range(w):
            s = 0.0
            for sy in range(SS):
                for sx in range(SS):
                    px = x + (sx + 0.5) / SS
                    py = y + (sy + 0.5) / SS
                    t = math.hypot((px - cx) / rx, (py - cy) / ry)
                    if t <= 1.0:
                        s += alpha_at(t)
            a[y * w + x] = s / (SS * SS)
    return a


def to_image(alpha, w, h, color, crop=None):
    im = Image.new("RGBA", (w, h))
    im.putdata([color + (max(0, min(255, round(v * 255))),) for v in alpha])
    return im.crop(crop) if crop else im


def radial_stops(paint):
    """Alpha along the radius for a 2-stop GRADIENT_RADIAL centred on the ellipse."""
    (s0, s1) = paint["gradientStops"]
    a0 = s0["color"]["a"] * paint.get("opacity", 1)
    a1 = s1["color"]["a"] * paint.get("opacity", 1)
    p0, p1 = s0["position"], s1["position"]

    def at(t):
        if t <= p0:
            return a0
        if t >= p1:
            return a1
        return a0 + (a1 - a0) * (t - p0) / (p1 - p0)

    return rgb(s0["color"]), at


# ------------------------------------------------------------------ assets
def corner_glows():
    """Footer glows (Overlay Element 18:587): 80x80 ellipse, radial #212121 50 % -> 10 %, blur 32.
    Drawn on a canvas of the blurred ellipse (80 + 2*32 = 144), then cut to the footer's visible
    part: 72 px above the screen bottom (25 px above the 47 px footer), clipped by the screen edge."""
    e = node("18:687")
    r = blur_radius(e)
    size = e["absoluteBoundingBox"]["width"]
    color, at = radial_stops(e["fills"][0])
    pad = int(round(r))
    n = int(size + 2 * pad)
    c = n / 2
    a = ellipse_alpha(n, n, c, c, size / 2, size / 2, at)
    a = blur(a, n, n, r * SIGMA_PER_RADIUS)
    half = n // 2
    # Ellipse centre sits on the footer's bottom edge (ellipse y 7 + 40 = 47).
    return {
        "glow-corner-left": to_image(a, n, n, color, (half, 0, n, half)),   # centre at (0, 72)
        "glow-corner-right": to_image(a, n, n, color, (0, 0, half, half)),  # centre at (72, 72)
        "glow-mid": to_image(a, n, n, color, (0, 0, n, half)),              # centre at (72, 72)
    }


def rec_glow():
    """Record glow (Rec Timer 83:867 'Glow'): 60x23 ellipse #EE2E2E, 50 %, blur 16, centred on the
    timer group's bottom-left corner. Cut like Figma's render bounds in the footer: x >= 0 (screen
    edge), y <= footer bottom. Footer coords: ellipse at (-22, 32) -> image covers x 0..54, y 16..47."""
    e = node("83:868")
    r = blur_radius(e)
    bb = e["absoluteBoundingBox"]
    w, h = bb["width"], bb["height"]
    color = rgb(e["fills"][0]["color"])
    op = e.get("opacity", 1.0) * e["fills"][0]["color"]["a"]
    pad = int(round(r))
    cw, ch = int(w + 2 * pad), int(h + 2 * pad)
    a = ellipse_alpha(cw, ch, cw / 2, ch / 2, w / 2, h / 2, lambda t: op)
    a = blur(a, cw, ch, r * SIGMA_PER_RADIUS)
    # Ellipse left edge is 30 px left of the timer group (footer x 8): canvas x 0 = footer x -38.
    # Visible: footer x >= 0 -> canvas x >= 38 ... canvas end (92); footer y <= 47.
    left = int(round(30 + pad - 8))
    top = 0
    bottom = int(round(pad + h / 2 + (47 - 43.5)))  # footer bottom relative to the canvas
    return {"rec-glow": to_image(a, cw, ch, color, (left, top, cw, bottom))}


def svg_polys(path, ox, oy, scale):
    """Flattens an SVG path (M/L/C/Z, absolute) into polygons."""
    toks = re.findall(r"[MLCZ]|-?[\d.]+(?:e-?\d+)?", path)
    polys, cur, pos, i = [], [], (0.0, 0.0), 0

    def pt(x, y):
        return ((ox + x) * scale, (oy + y) * scale)

    while i < len(toks):
        t = toks[i]
        if t == "M":
            if cur:
                polys.append(cur)
            pos = (float(toks[i + 1]), float(toks[i + 2]))
            cur = [pt(*pos)]
            i += 3
        elif t == "L":
            pos = (float(toks[i + 1]), float(toks[i + 2]))
            cur.append(pt(*pos))
            i += 3
        elif t == "C":
            p1 = (float(toks[i + 1]), float(toks[i + 2]))
            p2 = (float(toks[i + 3]), float(toks[i + 4]))
            p3 = (float(toks[i + 5]), float(toks[i + 6]))
            for s in range(1, 17):
                u = s / 16
                x = (1 - u) ** 3 * pos[0] + 3 * (1 - u) ** 2 * u * p1[0] + 3 * (1 - u) * u * u * p2[0] + u ** 3 * p3[0]
                y = (1 - u) ** 3 * pos[1] + 3 * (1 - u) ** 2 * u * p1[1] + 3 * (1 - u) * u * u * p2[1] + u ** 3 * p3[1]
                cur.append(pt(x, y))
            pos = p3
            i += 7
        elif t == "Z":
            if cur:
                polys.append(cur)
            cur = []
            i += 1
        else:
            i += 1
    if cur:
        polys.append(cur)
    return polys


def flash_bolt():
    """Bolt 'flash (compact)' 83:1866 (vector 83:1865, 6.6 x 9.7, white) in a 7 x 10 image."""
    v = node("I83:1947;83:285;83:876;83:1189;83:1865")
    color = rgb(v["fills"][0]["color"])
    w, h = 7, 10
    big = Image.new("L", (w * 16, h * 16), 0)
    d = ImageDraw.Draw(big)
    for g in v["fillGeometry"]:
        for poly in svg_polys(g["path"], 0, 0, 16):
            d.polygon(poly, fill=255)
    mask = big.resize((w, h), Image.BOX)
    im = Image.new("RGBA", (w, h), color + (0,))
    im.putalpha(mask)
    return {"flash-bolt": im}


def fill_image(nid):
    f = FILLS / f"{nid.replace(':', '-')}.png"
    if not f.exists():
        sys.exit(f"{f.relative_to(ROOT)} missing (run tools/figma_pull.py)")
    return Image.open(f).convert("RGBA")


def overlays():
    out = {}
    # Saving (83:2228, all four variants share the image; the layer opacity is 25/50/75/100 % for
    # progress k of 4, applied at runtime). STRETCH with imageTransform: the source crop is
    # x = tx .. tx + sx, y = ty .. ty + sy (fractions of the image), stretched to 320 x 240.
    n = node("83:2240")
    src = fill_image("83:2240")
    (sx, _, tx), (_, sy, ty) = n["fills"][0]["imageTransform"]
    W, H = src.size
    box = (tx * W, ty * H, min(W, (tx + sx) * W), min(H, (ty + sy) * H))
    img = src.resize((320, 240), Image.LANCZOS, box=box)
    img.putalpha(255)
    out["overlay-saving"] = img
    # Error (83:2286, both variants): FILL (same 4:3 aspect), layer opacity 90 % baked into alpha.
    n = node("83:2288")
    img = fill_image("83:2288").resize((320, 240), Image.LANCZOS)
    op = round(n.get("opacity", 1.0), 3)  # stored as 0.8999999761
    a = img.getchannel("A").point(lambda v: round(v * op))
    img.putalpha(a)
    out["overlay-error"] = img
    # Dither strip (style=dithered 83:2217): 80 x 200 image FILLed into 40 x 100. BOX matches the
    # Figma render of 83:2216 best (mean premultiplied error 1.6/255, vs 13.9 bilinear).
    out["dither-strip"] = fill_image("83:2217").resize((40, 100), Image.BOX)
    return out


def build():
    out = {}
    out.update(corner_glows())
    out.update(rec_glow())
    out.update(flash_bolt())
    out.update(overlays())
    return out


def same(a, b, tol=1):
    if a.size != b.size:
        return False
    return all(abs(x - y) <= tol for x, y in zip(a.tobytes(), b.tobytes()))


def main():
    check = "--check" in sys.argv
    stale = []
    for name, im in build().items():
        f = OUT / f"{name}.png"
        if f.exists() and same(Image.open(f).convert("RGBA"), im):
            continue
        stale.append(f)
        if not check:
            im.save(f, optimize=True)
            print(f"wrote {f.relative_to(ROOT)} {im.size[0]}x{im.size[1]}")
    if check and stale:
        print("figma assets out of date: run python tools/gen_figma_assets.py "
              + " ".join(str(f.relative_to(ROOT)) for f in stale), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
