#!/usr/bin/env python3
"""1:1 cutting templates for the v1 foam-board body (docs/BODY.md).

    python tools/enclosure_templates.py          # writes docs/assets/body/sheet-1..4.svg

Every number is in millimetres and comes from DIMENSIONS below; change a measurement there and
re-run. The SVGs are A4 at real size (1 SVG unit = 1 mm): print at 100 % / "actual size" and
check the 50 mm scale bar before cutting. docs/assets/body/templates.pdf is the same four sheets as
one A4 PDF, printed from the SVGs with Chromium (Playwright page.pdf, A4, zero margins); remake it
when the SVGs change.

Board: 3 mm foam board. Box: front and back panels cover everything; top, bottom and sides sit
between them; top and bottom span the full width over the sides.
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "assets" / "body"

# ---- measured / published dimensions (see docs/BODY.md "Dimensions" for sources and tags) ----
T = 3.0                       # foam board thickness
CAM_W, CAM_H, CAM_BODY = 24.1, 40.1, 9.0   # CamS3-5MP 24 x 40 (+0.1 fit), body 9 + ~2 mm lens boss
CAM_LENS_FROM_GROVE = 26.5    # lens centre from the Grove-port end (M5 STL, +-0.5)
LENS_HOLE = 14.5              # lens boss is ~14 mm across (M5 STL)
FL_W, FL_H = 24.1, 32.1       # FlashLight 24 x 32 (+0.1)
FL_LED_FROM_GROVE = 23.5      # LED window centre from the Grove end (M5 STL)
FL_WINDOW = (8.0, 6.0)        # LED window ~6 x 4 mm, cut a little bigger
CORE = 54.4                   # CoreS3-SE 54 x 54 (+0.4)
CORE_WINDOW = 50.0
GROVE_NOTCH = 12.0            # room for the Grove plug under each unit

# ---- layout ----
N_CAM = 4
W, H, D = 140.0, 97.0, 46.0   # outer box
IW, IH, ID = W - 2 * T, H - 2 * T, D - 2 * T   # interior 134 x 91 x 40
FRAME_W = IW                  # camera frames span the interior width
BAR_L, BAR_TOP, DIVIDER = 4.0, 4.0, 4.0
CAM_X0 = BAR_L                                # frame coords
FL_X0 = CAM_X0 + N_CAM * CAM_W + DIVIDER
FRAME_H = BAR_TOP + CAM_H + 8.0               # 8 mm bar under the cameras
SHELF_Y = 65.0                                # box y (from top outer face) of the shelf's top
CORE_TOP = 7.0                                # box y of the Core's top edge (on the door)
COLLAR_SIDE, COLLAR_TB = 6.0, 4.0             # collar rim: 6 mm left/right, 4 mm top/bottom
COLLAR_DEPTH = 3 * T                          # three layers glued to the door
SHELF_D = ID - COLLAR_DEPTH - 1.0             # shelf stops short of the collar; the gap is the cable pass

# derived positions on the FRONT panel (front view, from its top-left corner)
LENS_Y = T + BAR_TOP + CAM_H - CAM_LENS_FROM_GROVE
LENS_X = [T + CAM_X0 + CAM_W / 2 + k * CAM_W for k in range(N_CAM)]
LED_X = T + FL_X0 + FL_W / 2
LED_Y = T + BAR_TOP + FL_H - FL_LED_FROM_GROVE

PAGE_W, PAGE_H, MARGIN = 210.0, 297.0, 10.0


class Sheet:
    def __init__(self, title):
        self.title = title
        self.items = []

    def add(self, s):
        self.items.append(s)

    def svg(self, n, total):
        head = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{PAGE_W}mm" height="{PAGE_H}mm" '
                f'viewBox="0 0 {PAGE_W} {PAGE_H}" font-family="Helvetica, Arial, sans-serif">\n'
                '<style>.cut{fill:none;stroke:#000;stroke-width:0.3}'
                '.mark{fill:none;stroke:#000;stroke-width:0.25;stroke-dasharray:1.5 1}'
                '.ctr{stroke:#000;stroke-width:0.15}'
                '.lbl{font-size:3.6px}.sm{font-size:2.6px}.big{font-size:5px;font-weight:bold}</style>\n')
        title = (f'<text x="{MARGIN}" y="{MARGIN - 3}" class="big">x4cam body v1 — {self.title}</text>'
                 f'<text x="{MARGIN}" y="{MARGIN + 1.5}" class="sm">sheet {n}/{total} · '
                 '3 mm foam board · print at 100 % (actual size)</text>\n')
        return head + title + scale_bar() + "\n".join(self.items) + "\n</svg>\n"


def scale_bar():
    x, y = MARGIN, PAGE_H - 9
    s = [f'<rect x="{x}" y="{y}" width="50" height="2" fill="#000"/>',
         f'<text x="{x}" y="{y - 1.2}" class="sm">50 mm — measure this before cutting</text>',
         f'<rect x="{x + 60}" y="{y - 8}" width="10" height="10" class="cut"/>',
         f'<text x="{x + 72}" y="{y}" class="sm">10 mm square</text>',
         f'<text x="{PAGE_W - MARGIN}" y="{y + 2}" class="sm" text-anchor="end">'
         'solid = cut through · dashed = mark from your real part, then cut · + = hole centre</text>']
    return "\n".join(s)


def rect(x, y, w, h, cls="cut"):
    return f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" class="{cls}"/>'


def circle(cx, cy, r, cls="cut"):
    return f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}" class="{cls}"/>' + cross(cx, cy)


def cross(cx, cy, s=2.0):
    return (f'<line x1="{cx - s:.2f}" y1="{cy:.2f}" x2="{cx + s:.2f}" y2="{cy:.2f}" class="ctr"/>'
            f'<line x1="{cx:.2f}" y1="{cy - s:.2f}" x2="{cx:.2f}" y2="{cy + s:.2f}" class="ctr"/>')


def text(x, y, s, cls="lbl", anchor="start"):
    return f'<text x="{x:.2f}" y="{y:.2f}" class="{cls}" text-anchor="{anchor}">{s}</text>'


def polygon(pts, cls="cut"):
    return f'<polygon points="{" ".join(f"{x:.2f},{y:.2f}" for x, y in pts)}" class="{cls}"/>'


def front_panel(ox, oy):
    s = [rect(ox, oy, W, H)]
    for x in LENS_X:
        s.append(circle(ox + x, oy + LENS_Y, LENS_HOLE / 2))
    fw, fh = FL_WINDOW
    s.append(rect(ox + LED_X - fw / 2, oy + LED_Y - fh / 2, fw, fh) + cross(ox + LED_X, oy + LED_Y))
    s.append(text(ox + 3, oy + H - 3, "A · FRONT (outside face) — lenses Ø14.5, 24.1 mm apart; flash window", "lbl"))
    s.append(text(ox + 3, oy + H - 8, "cameras: Grove ends DOWN · cut the holes, then sand to a snug fit", "sm"))
    return "\n".join(s)


def back_panel(ox, oy):
    s = [rect(ox, oy, W, H)]
    cx = W / 2
    cy = CORE_TOP + CORE / 2
    s.append(rect(ox + cx - CORE_WINDOW / 2, oy + cy - CORE_WINDOW / 2, CORE_WINDOW, CORE_WINDOW))
    s.append(rect(ox + cx - CORE / 2, oy + cy - CORE / 2, CORE, CORE, "mark"))
    s.append(text(ox + cx, oy + cy + CORE / 2 + 4, "Core outline: collar E1–E3 goes around it on the inside face", "sm", "middle"))
    s.append(text(ox + 3, oy + H - 3, "D · BACK = removable DOOR (outside face) — Core screen window 50 × 50", "lbl"))
    return "\n".join(s)


def camera_frame(ox, oy, label):
    """Frame ring: openings for 4 cameras + flash, Grove notches open at the bottom edge."""
    w, h = FRAME_W, FRAME_H
    # outline as a polygon walking the bottom edge in and out of each notch
    notches = []
    for k in range(N_CAM):
        c = CAM_X0 + CAM_W / 2 + k * CAM_W
        notches.append((c - GROVE_NOTCH / 2, c + GROVE_NOTCH / 2, BAR_TOP + CAM_H))
    c = FL_X0 + FL_W / 2
    notches.append((c - GROVE_NOTCH / 2, c + GROVE_NOTCH / 2, BAR_TOP + FL_H))
    s = [rect(ox, oy, w, h)]
    s.append(rect(ox + CAM_X0, oy + BAR_TOP, N_CAM * CAM_W, CAM_H))
    s.append(rect(ox + FL_X0, oy + BAR_TOP, FL_W, FL_H))
    for a, b, top in notches:
        s.append(rect(ox + a, oy + top, b - a, h - top))
    for k in range(1, N_CAM):  # light guides for camera positions (not cut)
        x = ox + CAM_X0 + k * CAM_W
        s.append(f'<line x1="{x:.2f}" y1="{oy + BAR_TOP:.2f}" x2="{x:.2f}" y2="{oy + BAR_TOP + 4:.2f}" class="ctr"/>')
    s.append(text(ox + w / 2, oy + h + 4, label, "lbl", "middle"))
    s.append(text(ox + w / 2, oy + h + 7.5, "cut the big openings and the notches; the notches open the bottom edge (Grove plugs)", "sm", "middle"))
    return "\n".join(s)


def retainer(ox, oy):
    w, h = FRAME_W, FRAME_H
    s = [rect(ox, oy, w, h)]
    win = 14.0
    for k in range(N_CAM):
        cx = CAM_X0 + CAM_W / 2 + k * CAM_W
        cy = BAR_TOP + CAM_H - 21.5  # camera back recess centre (M5 STL)
        s.append(rect(ox + cx - win / 2, oy + cy - win / 2, win, win))
    cx, cy = FL_X0 + FL_W / 2, BAR_TOP + FL_H / 2
    s.append(rect(ox + cx - 5, oy + cy - 5, 10, 10))
    s.append(text(ox + w / 2, oy + h + 4, "C · RETAINER (behind the cameras, held with hook-and-loop) — vent windows", "lbl", "middle"))
    return "\n".join(s)


def top_panel(ox, oy):
    s = [rect(ox, oy, W, ID)]
    # exhaust slots over the air channel between the retainer (box depth 15) and the Core (27.5)
    for row_y in (13.0, 19.0):
        for k in range(4):
            x = 30 + k * 21
            s.append(rect(ox + x, oy + row_y, 18, 3))
    s.append(circle(ox + 15, oy + ID / 2, 6, "mark"))
    s.append(text(ox + 15, oy + ID - 2.5, "shutter", "sm", "middle"))
    s.append(rect(ox + W - 26, oy + ID / 2 - 5, 16, 10, "mark") + cross(ox + W - 18, oy + ID / 2))
    s.append(text(ox + W - 18, oy + ID - 2.5, "switch", "sm", "middle"))
    s.append(text(ox + 3, oy - 1.5, "F · TOP (outside face; front edge = bottom of this drawing) — exhaust slots", "lbl"))
    return "\n".join(s)


def bottom_panel(ox, oy):
    s = [rect(ox, oy, W, ID)]
    s.append(circle(ox + W / 2, oy + ID / 2, 4, "mark"))
    s.append(text(ox + W / 2, oy + ID / 2 + 9, "optional 1/4\"-20 tripod T-nut: mark from your nut", "sm", "middle"))
    s.append(text(ox + 3, oy - 1.5, "G · BOTTOM", "lbl"))
    return "\n".join(s)


def side_panel(ox, oy, label):
    s = [rect(ox, oy, ID, IH)]
    # intake slots low in the cable zone (box y ~50-62 -> side y ~47-59), over the air channel
    for y in (47.0, 52.5, 58.0):
        s.append(rect(ox + 10, oy + y, 18, 3))
    shelf = SHELF_Y - T
    s.append(f'<line x1="{ox:.2f}" y1="{oy + shelf:.2f}" x2="{ox + SHELF_D:.2f}" y2="{oy + shelf:.2f}" class="mark"/>')
    s.append(text(ox + SHELF_D / 2, oy + shelf + 3.5, "shelf top (line)", "sm", "middle"))
    s.append(text(ox + ID / 2, oy + IH + 4, label, "lbl", "middle"))
    s.append(text(ox + 1, oy + 4, "← this edge = front", "sm"))
    return "\n".join(s)


def shelf(ox, oy):
    s = [rect(ox, oy, IW, SHELF_D)]
    s.append(text(ox + IW / 2, oy + SHELF_D / 2 + 1.5, f"glued to the front block and both sides; the {ID - SHELF_D:.0f} mm gap at the back is the cable pass", "sm", "middle"))
    s.append(text(ox + 3, oy - 1.5, "I · SHELF (front edge = top of this drawing)", "lbl"))
    return "\n".join(s)


def collar(ox, oy, label):
    w, h = CORE + 2 * COLLAR_SIDE, CORE + 2 * COLLAR_TB
    notch = 30.0
    s = [rect(ox, oy, w, h), rect(ox + COLLAR_SIDE, oy + COLLAR_TB, CORE, CORE)]
    a, b = (w - notch) / 2, (h - notch) / 2
    s.append(rect(ox + a, oy, notch, COLLAR_TB))                 # top
    s.append(rect(ox + a, oy + h - COLLAR_TB, notch, COLLAR_TB))  # bottom
    s.append(rect(ox, oy + b, COLLAR_SIDE, notch))               # left
    s.append(rect(ox + w - COLLAR_SIDE, oy + b, COLLAR_SIDE, notch))  # right
    s.append(text(ox + w / 2, oy + h + 4, label, "lbl", "middle"))
    return "\n".join(s)


def build():
    sheets = []
    s1 = Sheet("front and back")
    s1.add(front_panel(MARGIN + (PAGE_W - 2 * MARGIN - W) / 2, MARGIN + 6))
    s1.add(back_panel(MARGIN + (PAGE_W - 2 * MARGIN - W) / 2, MARGIN + 6 + H + 14))
    sheets.append(s1)

    s2 = Sheet("camera block (3 frames + retainer)")
    x = MARGIN + (PAGE_W - 2 * MARGIN - FRAME_W) / 2
    y = MARGIN + 6
    for i in range(3):
        s2.add(camera_frame(x, y, f"B{i + 1} · CAMERA FRAME {i + 1} of 3 (glued behind the front panel)"))
        y += FRAME_H + 12
    s2.add(retainer(x, y))
    sheets.append(s2)

    s3 = Sheet("top, bottom, shelf, sides")
    x = MARGIN + (PAGE_W - 2 * MARGIN - W) / 2
    y = MARGIN + 10
    s3.add(top_panel(x, y)); y += ID + 10
    s3.add(bottom_panel(x, y)); y += ID + 10
    s3.add(shelf(x + T, y)); y += ID + 8
    s3.add(side_panel(x + 10, y, "H1 · SIDE (left)"))
    s3.add(side_panel(x + 10 + ID + 20, y, "H2 · SIDE (right)"))
    sheets.append(s3)

    s4 = Sheet("Core collar ×3")
    x = MARGIN + 5
    y = MARGIN + 6
    s4.add(collar(x, y, "E1 · CORE COLLAR"))
    s4.add(collar(x + CORE + 2 * COLLAR_SIDE + 14, y, "E2 · CORE COLLAR"))
    s4.add(collar(x, y + CORE + 2 * COLLAR_TB + 12, "E3 · CORE COLLAR"))
    cw, ch = CORE + 2 * COLLAR_SIDE, CORE + 2 * COLLAR_TB
    for i, line in enumerate(("glue E1, E2, E3 on top of each other (9 mm), then onto",
                              "the door's inside face around the dashed Core outline:",
                              "6 mm rim left/right, 4 mm top/bottom.",
                              "The notches leave room for the Core's ports and cables.")):
        s4.add(text(x + cw + 14, y + ch + 20 + 4 * i, line, "sm"))
    sheets.append(s4)
    return sheets


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sheets = build()
    for i, sh in enumerate(sheets, 1):
        (OUT / f"sheet-{i}.svg").write_text(sh.svg(i, len(sheets)))
    print(f"wrote {len(sheets)} sheets to {OUT.relative_to(ROOT)}/")
    print(f"box {W:.0f} x {H:.0f} x {D:.0f} mm; lens centres x={', '.join(f'{x:.2f}' for x in LENS_X)} "
          f"y={LENS_Y:.2f} (front view, from the top-left corner); flash window centre ({LED_X:.2f}, {LED_Y:.2f})")


if __name__ == "__main__":
    main()
