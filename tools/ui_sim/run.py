#!/usr/bin/env python3
"""x4cam UI simulator driver: builds tools/ui_sim, renders every UI scene with the real UI code,
and compares each one with the Figma frame it should match.

    python tools/ui_sim/run.py                 build, render all scenes, compare, write the report
    python tools/ui_sim/run.py preview_photo   only the scenes named (prefixes work: settings)
    python tools/ui_sim/run.py --live          open a window you can click (needs SDL2)
    python tools/ui_sim/run.py --list          scene names

Output: tools/ui_sim/out/ (git-ignored): <scene>.png (simulator), <scene>.figma.png, <scene>.diff.png,
index.html (side by side) and report.md (numbers). Needs cmake, a C/C++ compiler, git and
Pillow (13.0.0, like CI). Guide: docs/DEVELOP.md.

The comparison is informational: differences the docs already explain (photo mocks, dithering,
hidden menu items) are listed per scene. The script fails only if the simulator does not build
or a scene does not render.
"""
import argparse
import json
import pathlib
import shutil
import subprocess
import sys

from PIL import Image, ImageChops

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
BUILD = HERE / "build"
OUT = HERE / "out"
FIGMA = ROOT / "ui" / "figma"
SECTION = "73:368"  # "states": the frames the scenes are compared with
W, H = 320, 240

# Settings tiles compared one by one with their components (section 83:2147). Tile origins in
# the settings frame: grid padding 16, tiles 140x100, gap 8.
TILES = {"t1": (16, 16), "t2": (164, 16), "t3": (16, 124), "t4": (164, 124)}
TILE_CHECKS = {
    "settings": {"t1": "83:2134", "t2": "83:2132", "t3": "83:2131", "t4": "83:2133"},
    "settings_b": {"t1": "83:2169", "t2": "83:2179", "t3": "83:2216", "t4": "83:2220"},
    "settings_c": {"t2": "83:2174"},
    "settings_d": {"t2": "83:2212"},
    "settings_tap": {"t1": "83:2169"},  # tapped horzntl -> vertcl
}
SETTINGS_BG = (0xD1, 0xD1, 0xD1)


def run(cmd, **kw):
    print("$", " ".join(str(c) for c in cmd), flush=True)
    return subprocess.run(cmd, check=True, **kw)


def build():
    gen = ["-G", "Ninja"] if shutil.which("ninja") else []
    if not (BUILD / "CMakeCache.txt").exists():
        run(["cmake", "-S", HERE, "-B", BUILD, *gen])
    run(["cmake", "--build", BUILD, "--parallel"])
    exe = BUILD / ("ui_sim.exe" if sys.platform == "win32" else "ui_sim")
    if not exe.exists():
        sys.exit(f"build finished but {exe} is missing")
    return exe


def photo_file():
    """Figma's 'Photo Example' (fill of 15:70, scale mode FILL) as big-endian RGB565 320x240."""
    src = FIGMA / "fills" / "15-70.png"
    dst = BUILD / "photo.rgb565be"
    if not src.exists():
        print(f"note: {src.relative_to(ROOT)} missing (run tools/figma_pull.py): no photo", file=sys.stderr)
        return None
    im = Image.open(src).convert("RGB")
    s = max(W / im.width, H / im.height)  # FILL = cover, centred
    w, h = round(im.width * s), round(im.height * s)
    im = im.resize((w, h), Image.LANCZOS).crop(((w - W) // 2, (h - H) // 2, (w - W) // 2 + W, (h - H) // 2 + H))
    out = bytearray()
    raw = im.tobytes()
    for i in range(0, len(raw), 3):
        r, g, b = raw[i], raw[i + 1], raw[i + 2]
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out += bytes((v >> 8, v & 0xFF))
    dst.write_bytes(out)
    return dst


def scenes(exe):
    rows = subprocess.run([exe, "--list"], check=True, capture_output=True, text=True).stdout.splitlines()
    return [dict(zip(("name", "figma", "what"), r.split("\t"))) for r in rows if r]


def node_index():
    idx = {}
    for f in (FIGMA / "nodes").glob("*.json"):
        stack = [json.loads(f.read_text())["document"]]
        while stack:
            n = stack.pop()
            idx.setdefault(n["id"], n)
            stack.extend(n.get("children", []))
    return idx


def export(nid):
    p = FIGMA / "export" / f"{nid.replace(':', '-')}.png"
    return Image.open(p).convert("RGBA") if p.exists() else None


def figma_frame(idx, section_png, nid):
    """Crop a frame out of the 1x render of the 'states' section."""
    if nid not in idx or section_png is None:
        return None
    sb, fb = idx[SECTION]["absoluteBoundingBox"], idx[nid]["absoluteBoundingBox"]
    x, y = round(fb["x"] - sb["x"]), round(fb["y"] - sb["y"])
    return section_png.crop((x, y, x + W, y + H)).convert("RGB")


def compare(a, b):
    """Mean absolute difference (0-255) and share of pixels that differ by more than 24 levels."""
    d = ImageChops.difference(a.convert("RGB"), b.convert("RGB"))
    raw = d.tobytes()
    n = len(raw) // 3
    mean = sum(raw) / len(raw)
    big = sum(1 for i in range(0, len(raw), 3) if max(raw[i], raw[i + 1], raw[i + 2]) > 24) / n
    heat = d.point(lambda v: min(255, v * 4))
    return mean, big, heat


def flatten(rgba, bg):
    base = Image.new("RGBA", rgba.size, bg + (255,))
    return Image.alpha_composite(base, rgba).convert("RGB")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("only", nargs="*", help="scene names or prefixes")
    ap.add_argument("--live", action="store_true", help="interactive window (needs SDL2)")
    ap.add_argument("--list", action="store_true", help="list scenes")
    args = ap.parse_args()

    exe = build()
    photo = photo_file()
    if args.live:
        return subprocess.call([exe, "--live"] + (["--photo", photo] if photo else []))
    table = scenes(exe)
    if args.list:
        for s in table:
            print(f"{s['name']:24} {s['figma']:8} {s['what']}")
        return 0
    if args.only:
        table = [s for s in table if any(s["name"].startswith(o) for o in args.only)]
        if not table:
            sys.exit("no scene matches")

    OUT.mkdir(exist_ok=True)
    idx = node_index()
    section = export(SECTION)
    report = ["# UI simulator vs Figma", "",
              "Mean = average difference per channel (0-255). Changed = pixels off by more than 24 levels.", "",
              "| scene | Figma | mean | changed | what / expected differences |", "|---|---|---|---|---|"]
    html = ["<!doctype html><meta charset=utf-8><title>x4cam UI sim</title>",
            "<style>body{font:14px system-ui;background:#222;color:#eee}img{image-rendering:pixelated;"
            "width:480px;margin:2px}td{vertical-align:top;padding:4px}</style>",
            "<h1>x4cam UI simulator vs Figma</h1><p>simulator · Figma · difference ×4</p><table>"]
    failed = []
    for s in table:
        name = s["name"]
        bmp = BUILD / f"{name}.bmp"
        cmd = [exe, "--scene", name, "--out", bmp] + (["--photo", photo] if photo else [])
        if subprocess.call(cmd) != 0 or not bmp.exists():
            failed.append(name)
            continue
        sim = Image.open(bmp).convert("RGB")
        sim.save(OUT / f"{name}.png")
        row, cells = f"| `{name}` | {s['figma']} | – | – | {s['what']} |", [f"{name}.png"]
        ref = figma_frame(idx, section, s["figma"]) if s["figma"] != "-" else None
        if ref is not None:
            ref.save(OUT / f"{name}.figma.png")
            mean, big, heat = compare(sim, ref)
            heat.save(OUT / f"{name}.diff.png")
            row = f"| `{name}` | {s['figma']} | {mean:.1f} | {big:.1%} | {s['what']} |"
            cells += [f"{name}.figma.png", f"{name}.diff.png"]
        report.append(row)
        html.append(f"<tr><td><b>{name}</b><br>{s['what']}<br>Figma {s['figma']}</td><td>"
                    + "".join(f"<img src='{c}'>" for c in cells) + "</td></tr>")
        for tile, comp in TILE_CHECKS.get(name, {}).items():
            ref_tile = export(comp)
            if ref_tile is None:
                continue
            x, y = TILES[tile]
            mine = sim.crop((x, y, x + ref_tile.width, y + ref_tile.height))
            theirs = flatten(ref_tile, SETTINGS_BG)
            mean, big, heat = compare(mine, theirs)
            stem = f"{name}.{tile}"
            mine.save(OUT / f"{stem}.png")
            theirs.save(OUT / f"{stem}.figma.png")
            heat.save(OUT / f"{stem}.diff.png")
            report.append(f"| `{name}` tile {tile} | {comp} | {mean:.1f} | {big:.1%} | {idx.get(comp, {}).get('name', '')} |")
            html.append(f"<tr><td>{name} {tile}<br>{comp}</td><td>"
                        + "".join(f"<img src='{stem}{e}.png' style='width:280px'>" for e in ("", ".figma", ".diff"))
                        + "</td></tr>")
    html.append("</table>")
    (OUT / "index.html").write_text("\n".join(html) + "\n")
    (OUT / "report.md").write_text("\n".join(report) + "\n")
    print("\n".join(report))
    print(f"\nopen {(OUT / 'index.html').relative_to(ROOT)} in a browser")
    if failed:
        print("scenes that did not render: " + ", ".join(failed), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
