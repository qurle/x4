#!/usr/bin/env python3
"""x4cam foam-board body (docs/ENCLOSURE.md): one geometry -> 1:1 SVG templates + 3D scene.

    cd tools/enclosure && npm ci
    python3 gen.py      # docs/assets/enclosure/sheet-1..3.svg + out/scene.json
    node build.mjs      # docs/assets/enclosure/x4cam-body.html (3D steps, works offline)
    node render.cjs     # docs/assets/enclosure/step-*.jpg + x4cam-body.pdf (needs Playwright + Chromium)

Change a measurement below and run all three again.

Layout (owner's render): bank in a sleeve on the left (front view), cameras touching in a row,
Core and flash in the back layer, the flash behind camera 4 shining over its top edge through a
light tunnel, WAGOs at the bottom.

World (mm): X along the length (front view, left -> right, 0 = open end of the bank sleeve),
Y up (0 = bottom outer face), Z depth (0 = back outer face, D = front outer face).
Box interior: u = X - X_IN (from the middle wall), v = down from the top inner face,
z = back from the front inner face.
"""
import json, math, pathlib

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
DOCS = ROOT / "docs" / "assets" / "enclosure"   # sheet-1..3.svg (committed)
OUT = HERE / "out"                              # scene.json for build.mjs (not committed)
DOCS.mkdir(parents=True, exist_ok=True)
OUT.mkdir(exist_ok=True)

T = 3.0
# parts (mm)
CAM_W, CAM_H, CAM_D = 24.1, 40.1, 9.0          # CamS3-5MP 24 x 40 x 11 (9 body + 2 lens boss)
LENS_FROM_GROVE, LENS_HOLE = 26.5, 14.5
FL_W, FL_H, FL_D = 24.1, 32.1, 8.0             # FlashLight 24 x 32 x 8
LED_FROM_GROVE = 23.5                           # from M5's STL, +-0.5
CORE, CORE_D = 54.4, 16.5                       # CoreS3-SE 54 x 54 x 15.5 (M5 page) / 16.5 (M5 STL)
BANK = (37.0, 77.0, 25.0)                       # A1653 ~77 x 37 x 25 (listings differ)
WAGO = (30.0, 8.4, 18.6)                        # 221-415: 30 x 8.4 x 18.6
BTN = (32.0, 24.0, 8.0)                         # Unit (Mini) Button 32 x 24 x 8
CAP = dict(d=10.0, l=20.0)                      # typical 2200 uF 10 V; Not sure, measure yours
GAP = 3.0                                       # cooling gap = spacer strips
CAM_TOP = 19.0                                  # v of the camera tops = height of the light tunnel
FL_TOP = 0.5
PLUG = 6.5                                      # Grove plug below a unit (Not sure: measure)
NOTCH = 12.0
POST_H = 3.5
POST_LAYERS = 2
TUN_HALF = 11.5                                 # tunnel half-width (~37 deg each side at 15 mm)
LIP = 2.0

IW = 114.0                                      # cameras touch M; ~18 mm free to the right of camera 4 (owner's render)
ID = BANK[2] + 1.0                              # 26
IH = BANK[1] + 0.5                              # 77.5: the box is exactly as tall as the bank sleeve
WAGO_Z0 = ID - WAGO[2]                          # 7.4: WAGOs pushed against the door, holes facing forward
SLEEVE_W = BANK[0] + 0.5                        # 37.5
RISER = 0
L = SLEEVE_W + T + IW + T                       # 140
H = IH + 2 * T                                  # 90.5
D = ID + 2 * T                                  # 32
X_IN, Y_TOP, Z_FRONT = SLEEVE_W + T, H - T, D - T


def X(u): return X_IN + u
def Y(v): return Y_TOP - v
def Z(z): return Z_FRONT - z


cam_c = [CAM_W / 2 + k * CAM_W for k in range(4)]
LENS_V = CAM_TOP + CAM_H - LENS_FROM_GROVE      # 30.6
FL_U = 4 * CAM_W + 4.6 - FL_W / 2               # LED just right of camera 4's edge, flash half behind camera 4
fl_c = FL_U + FL_W / 2
LED_V = FL_TOP + FL_H - LED_FROM_GROVE          # 9.1
TUN = (fl_c - TUN_HALF, min(IW - 1.0, fl_c + TUN_HALF))
CORE_U = 25.0                                   # left of it: breakout + switch
FL_Z = CAM_D + GAP                              # flash front face rests on camera 4's strips
REC_Z, REC_V = 15.0, 40.0                       # socket centre (Not sure: from your bank's plug)
SW = dict(u=7.5, v=12.0, w=8.0, h=13.5, depth=12.0)

# ---------------------------------------------------------------- panels (2D, y down)
def rect_pts(x, y, w, h): return [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]


def arc(cx, cy, r, a0, a1, n=12):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


panels = []


def place(O, U, V, N): return dict(O=O, U=U, V=V, N=N)


def panel(pid, name, w, h, pl, step, outline=None, holes=(), marks=(), notes=()):
    p = dict(id=pid, name=name, w=w, h=h, outline=outline or rect_pts(0, 0, w, h), holes=list(holes),
             marks=list(marks), notes=list(notes), place=pl, step=step)
    panels.append(p)
    return p


NOTCH_R = 10.0
# A: front (outside face)
a_out = None
a_holes = [dict(circle=(X(c), T + LENS_V, LENS_HOLE / 2)) for c in cam_c]
WIN = (X(TUN[0]), T + 1.5, TUN[1] - TUN[0], CAM_TOP - 3.0, 3.0)
a_holes.append(dict(rrect=WIN))
panel("A", "Front", L, H, place([0, H, D], [1, 0, 0], [0, -1, 0], [0, 0, -1]), 3, a_out, a_holes,
      notes=[(3, H - 3, "A · FRONT (outside face) · 4 lens holes Ø14.5, 24.1 apart · flash window over camera 4"),
             (3, H - 8, "left 37.5 mm = bank sleeve")])

# D: door, seen from the back (X runs right-to-left)
DOOR_W = L - SLEEVE_W
def dx(u): return L - X(u)
d_holes = [dict(rect=(dx(CORE_U + CORE), T, CORE, CORE))]
d_marks = [dict(rect=(dx(SW["u"] + SW["w"] / 2), T + SW["v"] - SW["h"] / 2, SW["w"], SW["h"])),
           dict(cross=(dx(SW["u"]), T + SW["v"])),
           dict(rect=(dx(fl_c + 8), T + 6, 16, 20))]
panel("D", "Door", DOOR_W, H, place([L, H, 0], [-1, 0, 0], [0, -1, 0], [0, 0, 1]), 7, None, d_holes, d_marks,
      notes=[(3, H - 3, "D · DOOR (outside face, seen from the back) · window = full Core outline"),
             (3, H - 8, "dashed, mark from your parts: switch (top right) · flash pad P1+P2 (inside face, top left)"),
             (dx(fl_c + 8) + 8, T + 6 + 24, "flash pad P (inside)", "middle")])

# K: sleeve back, seen from the back
k_out = None
panel("K", "Sleeve back", SLEEVE_W, H, place([SLEEVE_W, H, 0], [-1, 0, 0], [0, -1, 0], [0, 0, 1]), 4, k_out,
      notes=[(2, H - 3, "K · SLEEVE BACK")])

# B1..B3: top bar of the camera block (seen from the front). B3 keeps a 2 mm lip over camera 4.
b_short = rect_pts(0, 0, TUN[0], CAM_TOP)
b3 = [(0, 0), (TUN[0], 0), (TUN[0], CAM_TOP - LIP), (IW, CAM_TOP - LIP), (IW, CAM_TOP), (0, CAM_TOP)]
for i in range(3):
    pl = place([X_IN, Y_TOP, Z_FRONT - T * i], [1, 0, 0], [0, -1, 0], [0, 0, -1])
    if i < 2:
        panel(f"B{i + 1}", f"Top bar {i + 1}", TUN[0], CAM_TOP, pl, 2, b_short,
              notes=[(TUN[0] / 2, CAM_TOP + 4, f"B{i + 1} · TOP BAR (front layers)", "middle")])
    else:
        panel("B3", "Top bar 3", IW, CAM_TOP, pl, 2, b3,
              notes=[(0, CAM_TOP + 4, "B3 · TOP BAR (back layer, with the 2 mm lip over camera 4)")])

# T: top (outside face; back edge at the top of the drawing)
def ty(z): return Z(z) - T
t_holes = [dict(rect=(X(max(k * CAM_W + 4, 12)), ty(CAM_D + GAP), CAM_W - 4 - max(4, 12 - k * CAM_W), GAP)) for k in range(4)]
t_holes.append(dict(rect=(X(0.5), ty(CAM_D + GAP + 2), 9, 5)))
t_marks = [dict(rect=(X(CORE_U + 8), ty(24), CORE - 16, 10)),
           dict(rect=((SLEEVE_W - BTN[0]) / 2, (ID - BTN[1]) / 2, BTN[0], BTN[1]))]
panel("T", "Top", L, ID, place([0, H, T], [1, 0, 0], [0, 0, 1], [0, -1, 0]), 4, None, t_holes, t_marks,
      notes=[(3, -1.5, "T · TOP (outside face; FRONT edge = bottom of this drawing) · 4 exhaust slots over the gap"),
             (X(CORE_U + 8) + 1, ty(24) + 6, "Core SD / button: mark from Core"),
             (5, ID / 2 + 1, "Mini Button sits here"),
             (X(0.5), ty(CAM_D + GAP + 2) + 8, "button cable")])
panel("G", "Bottom", L, ID, place([0, T, T], [1, 0, 0], [0, 0, 1], [0, -1, 0]), 4,
      notes=[(3, -1.5, "G · BOTTOM (front edge = bottom of this drawing)")])

# M: middle wall, seen from the sleeve side (back on the left, front on the right)
panel("M", "Middle wall", ID, IH, place([SLEEVE_W, Y_TOP, T], [0, 0, 1], [0, -1, 0], [1, 0, 0]), 4, None,
      [dict(rect=(Z(REC_Z) - T - 2.25, REC_V - 5, 4.5, 10))], [dict(cross=(Z(REC_Z) - T, REC_V))],
      notes=[(ID / 2, IH + 4, "M · MIDDLE WALL", "middle"), (ID / 2, IH + 7.5, "socket hole: check vs your bank", "middle"),
             (1, 4, "← back"), (ID - 1, 4, "front →", "end")])
R_SLOTS = [CAM_TOP + CAM_H + 2.5, CAM_TOP + CAM_H + 6.5]
panel("R", "Right wall", ID, IH, place([L, Y_TOP, Z_FRONT], [0, 0, -1], [0, -1, 0], [-1, 0, 0]), 4, None,
      [dict(rect=(4, v, 18, 3)) for v in R_SLOTS],
      notes=[(ID / 2, IH + 4, "R · RIGHT WALL", "middle"), (ID / 2, IH + 7.5, "2 intake slots", "middle"),
             (1, 4, "← front"), (ID - 1, 4, "back →", "end")])
for i in range(RISER):
    panel(f"S{i + 1}", f"Bank riser {i + 1}", SLEEVE_W, ID, place([0, T + T * (i + 1), T], [1, 0, 0], [0, 0, 1], [0, -1, 0]), 4,
          notes=[(SLEEVE_W / 2, ID + 4, f"S{i + 1} · RISER", "middle")])
for i in range(2):
    panel(f"P{i + 1}", f"Flash pad {i + 1}", 16, 20,
          place([X(fl_c - 8), Y(6), T + T * i], [1, 0, 0], [0, -1, 0], [0, 0, 1]), 7,
          notes=[(8, 24, f"P{i + 1}", "middle")])

# ---------------------------------------------------------------- foam boxes + components (3D)
comps = []
FOAM = "foam"


def box(cid, name, x0, x1, y0, y1, z0, z1, color, step, group=None):
    comps.append(dict(kind="box", id=cid, name=name, min=[min(x0, x1), min(y0, y1), min(z0, z1)],
                      max=[max(x0, x1), max(y0, y1), max(z0, z1)], color=color, step=step, group=group or cid))


def cyl(cid, name, c, r, axis, length, color, step, group=None):
    comps.append(dict(kind="cyl", id=cid, name=name, c=c, r=r, axis=axis, len=length, color=color, step=step,
                      group=group or cid))


# posts under the cameras (laminated strip, cut into 5 pieces)
edges = [0.0] + sum(([c - NOTCH / 2, c + NOTCH / 2] for c in cam_c), []) + [IW]
for a, b in zip(edges[0::2], edges[1::2]):
    box(f"post{a:.0f}", "post", X(a), X(b), Y(CAM_TOP + CAM_H), Y(CAM_TOP + CAM_H + POST_H), Z(0), Z(POST_LAYERS * T), FOAM, 3, "posts")
# spacer strips on the camera backs
for k in range(4):
    for du in (0.0, CAM_W - 4):
        box(f"strip{k}{du:.0f}", "spacer strip", X(k * CAM_W + du), X(k * CAM_W + du + 4), Y(CAM_TOP), Y(CAM_TOP + CAM_H),
            Z(CAM_D), Z(CAM_D + GAP), "#ffb070", 5, "strips")
for k in range(4):
    u0 = k * CAM_W
    box(f"cam{k}", f"Camera {k + 1}", X(u0), X(u0 + CAM_W), Y(CAM_TOP), Y(CAM_TOP + CAM_H), Z(0), Z(CAM_D), "#ececec", 5, "cams")
    cyl(f"boss{k}", "lens", [X(cam_c[k]), Y(LENS_V), Z_FRONT + 1], 7.0, "z", 2.0, "#f7f7f7", 5, "cams")
    cyl(f"lens{k}", "lens", [X(cam_c[k]), Y(LENS_V), Z_FRONT + 2.05], 3.0, "z", 0.2, "#1b1b1b", 5, "cams")
    box(f"plug{k}", "Grove plug", X(cam_c[k] - 4.25), X(cam_c[k] + 4.25), Y(CAM_TOP + CAM_H), Y(CAM_TOP + CAM_H + PLUG),
        Z(2), Z(7), "#f2f2f2", 5, "cams")
box("flash", "Flashlight", X(FL_U), X(FL_U + FL_W), Y(FL_TOP), Y(FL_TOP + FL_H), Z(FL_Z), Z(FL_Z + FL_D), "#f4f4f4", 6, "flash")
box("led", "LED", X(fl_c - 3), X(fl_c + 3), Y(LED_V - 2), Y(LED_V + 2), Z(FL_Z), Z(FL_Z) + 0.15, "#ffcf3f", 6, "flash")
box("fplug", "Grove plug", X(fl_c - 4.25), X(fl_c + 4.25), Y(FL_TOP + FL_H), Y(FL_TOP + FL_H + PLUG),
    Z(FL_Z + 1), Z(FL_Z + 6), "#f2f2f2", 6, "flash")
cyl("cap", "capacitor", [X(fl_c), Y(FL_TOP + FL_H + PLUG + 2 + CAP["d"] / 2 + 3), Z(19)], CAP["d"] / 2, "x", CAP["l"], "#2a2a33", 6)
box("brk", "USB-C breakout", X(1), X(14), Y(REC_V - 11), Y(REC_V + 11), Z(REC_Z + 1.6), Z(REC_Z + 3.2), "#9b1c1c", 6, "breakout")
box("rec", "USB-C socket", SLEEVE_W, X(4.5), Y(REC_V - 5), Y(REC_V + 5), Z(REC_Z - 1.6), Z(REC_Z + 1.6), "#9aa0a6", 6, "breakout")
wago_u0 = (IW - 3 * WAGO[0]) / 2
wagos = [(wago_u0 + k * WAGO[0], 0) for k in range(3)] + [(wago_u0 + WAGO[0], 1)]
for i, (u, j) in enumerate(wagos):
    y0 = T + j * WAGO[1]
    box(f"wago{i}", "WAGO 221-415", X(u), X(u + WAGO[0]), y0, y0 + WAGO[1], Z(WAGO_Z0), Z(ID), "#c9c9c9", 6, "wagos")
    for q in range(5):
        lx = X(u) + 1.5 + q * 5.6
        box(f"lever{i}{q}", "lever", lx, lx + 4.6, y0 + WAGO[1], y0 + WAGO[1] + 1.0, Z(ID - 3), Z(WAGO_Z0 + 2), "#f0a020", 6, "wagos")
        box(f"hole{i}{q}", "wire hole", lx + 0.8, lx + 3.8, y0 + 2.7, y0 + 5.7, Z(WAGO_Z0) + 0.1, Z(WAGO_Z0) - 0.05, "#3a3a3a", 6, "wagos")
box("core", "CoreS3-SE", X(CORE_U), X(CORE_U + CORE), Y(0), Y(CORE), Z(CAM_D + GAP), Z(CAM_D + GAP + CORE_D), "#d6d6d6", 7, "core")
box("screen", "screen", X(CORE_U + 5), X(CORE_U + CORE - 5), Y(8), Y(CORE - 12), Z(CAM_D + GAP + CORE_D) - 0.1,
    Z(CAM_D + GAP + CORE_D), "#15171a", 7, "core")
box("sw", "power switch", X(SW["u"] - SW["w"] / 2), X(SW["u"] + SW["w"] / 2), Y(SW["v"] - SW["h"] / 2), Y(SW["v"] + SW["h"] / 2),
    -1.5, T + SW["depth"], "#111111", 7, "switch")
bx0, by0, bz0 = (SLEEVE_W - BANK[0]) / 2, T + RISER * T + 0.25, (D - BANK[2]) / 2
box("bank", "Power bank", bx0, bx0 + BANK[0], by0, by0 + BANK[1], bz0, bz0 + BANK[2], "#1d1d1f", 8, "bank")
box("btn", "Mini Button", (SLEEVE_W - BTN[0]) / 2, (SLEEVE_W + BTN[0]) / 2, H, H + BTN[2], (D - BTN[1]) / 2, (D + BTN[1]) / 2,
    "#f2f2f2", 8, "button")
cyl("btncap", "button cap", [SLEEVE_W / 2 - 4, H + BTN[2] + 0.6, D / 2], 4.0, "y", 1.2, "#2c2c2c", 8, "button")

# ---------------------------------------------------------------- templates (SVG)
PAGE_W, PAGE_H, MARGIN = 210.0, 297.0, 10.0
cx = lambda w: (PAGE_W - w) / 2
ROW2 = 24 + CAM_TOP + 14
sheet_pos = {
    "A": (1, cx(L), 22), "D": (1, 14, 22 + H + 20), "K": (1, 14 + DOOR_W + 12, 22 + H + 20),
    "B1": (2, 14, 24), "B2": (2, 14, 24 + CAM_TOP + 12), "B3": (2, 14, 24 + 2 * (CAM_TOP + 12)),
    "P1": (2, 14 + IW + 14, 24), "P2": (2, 14 + IW + 14 + 16 + 10, 24),
    "T": (3, cx(L), 24), "G": (3, cx(L), 24 + ID + 14),
    "M": (3, cx(L), 24 + 2 * (ID + 14)), "R": (3, cx(L) + ID + 16, 24 + 2 * (ID + 14)),
}
POSTS_AT = (2, 14, 24 + 3 * (CAM_TOP + 12) + 10)
STRIPS_AT = (2, 14, POSTS_AT[2] + 3 * (POST_H + 6) + 14)
TITLES = {1: "front, door, sleeve back", 2: "camera block, posts, spacer strips, flash pad", 3: "top, bottom, walls"}


def fm(v): return f"{v:.2f}"


def svg_poly(pts, ox, oy, cls="cut"):
    return f'<polygon points="{" ".join(f"{fm(ox + x)},{fm(oy + y)}" for x, y in pts)}" class="{cls}"/>'


def svg_cross(cx_, cy_, ox, oy, s=2.0):
    x, y = ox + cx_, oy + cy_
    return (f'<line x1="{fm(x - s)}" y1="{fm(y)}" x2="{fm(x + s)}" y2="{fm(y)}" class="ctr"/>'
            f'<line x1="{fm(x)}" y1="{fm(y - s)}" x2="{fm(x)}" y2="{fm(y + s)}" class="ctr"/>')


def svg_shape(s, ox, oy, cls):
    if "circle" in s:
        a, b, r = s["circle"]
        return f'<circle cx="{fm(ox + a)}" cy="{fm(oy + b)}" r="{fm(r)}" class="{cls}"/>' + svg_cross(a, b, ox, oy)
    if "rect" in s:
        x, y, w, h = s["rect"]
        return f'<rect x="{fm(ox + x)}" y="{fm(oy + y)}" width="{fm(w)}" height="{fm(h)}" class="{cls}"/>'
    if "cross" in s:
        return svg_cross(*s["cross"], ox, oy)
    if "rrect" in s:
        x, y, w, h, r = s["rrect"]
        return f'<rect x="{fm(ox + x)}" y="{fm(oy + y)}" width="{fm(w)}" height="{fm(h)}" rx="{fm(r)}" class="{cls}"/>'
    return ""


def svg_text(x, y, s, cls="sm", anchor="start"):
    s = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    return f'<text x="{fm(x)}" y="{fm(y)}" class="{cls}" text-anchor="{anchor}">{s}</text>'


def sheet_svg(n, total):
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{PAGE_W}mm" height="{PAGE_H}mm" viewBox="0 0 {PAGE_W} {PAGE_H}" '
           'font-family="Helvetica, Arial, sans-serif">',
           '<style>.cut{fill:none;stroke:#000;stroke-width:0.3}.mark{fill:none;stroke:#000;stroke-width:0.25;stroke-dasharray:1.5 1}'
           '.ctr{stroke:#000;stroke-width:0.15}.sm{font-size:2.6px}.lbl{font-size:3.3px}.big{font-size:5px;font-weight:bold}</style>',
           svg_text(MARGIN, MARGIN + 1, f"x4cam body — {TITLES[n]}", "big"),
           svg_text(MARGIN, MARGIN + 6, f"sheet {n}/{total} · 3 mm foam board · print at 100 % (actual size), check the 50 mm bar", "sm")]
    for p in panels:
        sp = sheet_pos.get(p["id"])
        if not sp or sp[0] != n:
            continue
        _, ox, oy = sp
        out.append(svg_poly(p["outline"], ox, oy))
        out += [svg_shape(h, ox, oy, "cut") for h in p["holes"]]
        out += [svg_shape(m, ox, oy, "mark") for m in p["marks"]]
        for note in p["notes"]:
            x, y, s = note[:3]
            out.append(svg_text(ox + x, oy + y, s, "lbl" if s.startswith(p["id"] + " ") else "sm",
                                note[3] if len(note) > 3 else "start"))
    if n == 2:
        _, ox, oy = POSTS_AT
        out.append(svg_text(ox, oy - 2, "F1–F2 · POST STRIPS: laminate, then cut the 5 dashed notches", "lbl"))
        for i in range(POST_LAYERS):
            y = oy + i * (POST_H + 6)
            out.append(f'<rect x="{fm(ox)}" y="{fm(y)}" width="{fm(IW)}" height="{fm(POST_H)}" class="cut"/>')
            for c in cam_c:
                out.append(f'<rect x="{fm(ox + c - NOTCH / 2)}" y="{fm(y)}" width="{fm(NOTCH)}" height="{fm(POST_H)}" class="mark"/>')
            out.append(svg_text(ox + IW + 2, y + 3, f"F{i + 1}"))
        _, ox, oy = STRIPS_AT
        out.append(svg_text(ox, oy - 2, "C · SPACER STRIPS × 8: two per camera", "lbl"))
        x, y = ox, oy
        for i in range(8):
            out.append(f'<rect x="{fm(x)}" y="{fm(y)}" width="{fm(CAM_H)}" height="4" class="cut"/>')
            x += CAM_H + 3
            if x + CAM_H > ox + IW:
                x, y = ox, y + 7
    yb = PAGE_H - 18
    out += [f'<rect x="{MARGIN}" y="{yb}" width="50" height="2" fill="#000"/>', svg_text(MARGIN, yb - 1.5, "50 mm: measure before cutting"),
            f'<rect x="{MARGIN + 60}" y="{yb - 6}" width="10" height="10" class="cut"/>', svg_text(MARGIN + 72, yb + 1, "10 mm"),
            svg_text(PAGE_W - MARGIN, yb + 1, "solid = cut through · dashed = mark from your real part · + = centre", "sm", "end"),
            "</svg>"]
    return "\n".join(out)


for n in (1, 2, 3):
    (DOCS / f"sheet-{n}.svg").write_text(sheet_svg(n, 3))

# step 1: everything lying on the table, laid out like the sheets
SX, SZ = -150, -280
flat = {p["id"]: place([SX + (sheet_pos[p["id"]][0] - 1) * 225 + sheet_pos[p["id"]][1], 0, SZ + sheet_pos[p["id"]][2]],
                       [1, 0, 0], [0, 0, 1], [0, 1, 0]) for p in panels}
flat_boxes = []
_, ox, oy = POSTS_AT
for i in range(POST_LAYERS):
    flat_boxes.append(dict(min=[SX + 225 + ox, 0, SZ + oy + i * (POST_H + 6)], max=[SX + 225 + ox + IW, T, SZ + oy + i * (POST_H + 6) + POST_H], label="F1–F2"))
_, ox, oy = STRIPS_AT
x, y = ox, oy
for i in range(8):
    flat_boxes.append(dict(min=[SX + 225 + x, 0, SZ + y], max=[SX + 225 + x + CAM_H, T, SZ + y + 4], label="C ×8"))
    x += CAM_H + 3
    if x + CAM_H > ox + IW:
        x, y = ox, y + 7

beam_up = math.degrees(math.atan((LED_V - 1.5) / (FL_Z + T)))
beam_dn = math.degrees(math.atan((CAM_TOP - 1.5 - LED_V) / (FL_Z + T)))
facts = dict(L=L, H=H, D=D, IW=IW, IH=IH, ID=ID, lens_x=[round(X(c), 2) for c in cam_c], lens_y=round(T + LENS_V, 2),
             led_x=round(X(fl_c), 2), led_y=round(T + LED_V, 2), beam_up=round(beam_up), beam_dn=round(beam_dn),
             tunnel=[round(TUN[1] - TUN[0], 1), CAM_TOP - 3.0])
scene = dict(T=T, size=[L, H, D], panels=[{k: v for k, v in p.items() if k != "notes"} for p in panels],
             comps=comps, flat=flat, flat_boxes=flat_boxes, facts=facts)
(OUT / "scene.json").write_text(json.dumps(scene))
print(json.dumps(facts))
