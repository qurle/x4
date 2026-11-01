#!/usr/bin/env python3
"""Pull design data from the x4 Figma file through the REST API into ui/figma/.

    FIGMA_TOKEN=... python tools/figma_pull.py                 # default nodes
    FIGMA_TOKEN=... python tools/figma_pull.py 83:2147 73:368  # specific nodes
    FIGMA_TOKEN=... python tools/figma_pull.py --components    # list published components only
    FIGMA_TOKEN=... python tools/figma_pull.py --no-renders    # node JSON only, keep existing PNGs

Writes (all committed, so design changes show up as diffs):
    ui/figma/components.json        every component / component set in the file (id, name, page)
    ui/figma/nodes/<id>.json        raw node trees: geometry, fills, strokes, effects, text styles
    ui/figma/fills/<id>.png         source image fills of FILL_NODES (input of gen_figma_assets.py)
    ui/figma/export/<id>.png        1x renders of each requested node and each component variant
                                    (reference only; assets for the firmware still go through
                                    tools/gen_assets.py)

The token is a Figma personal access token with the file_content:read scope. It is read from the
environment only and never written anywhere.
"""
import json
import os
import pathlib
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

FILE_KEY = "fgEvP6Ms0bOAbWxfkFki4R"
API = "https://api.figma.com/v1"
ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "ui" / "figma"
# 73:368 = "states" section (viewfinder, menu, settings, saving, error)
# 83:2147 = "settings states" (tile components with their variants)
# The rest are component sets used by "states" that live elsewhere in the file:
# 83:1905 Footer States, 83:2228 saving overlay, 83:2286 error overlay, 83:866 Rec Timer,
# 18:587 Overlay Element (footer corners)
DEFAULT_NODES = ["73:368", "83:2147", "83:1905", "83:2228", "83:2286", "83:866", "18:587"]
# Nodes whose image fills tools/gen_figma_assets.py needs (saved as ui/figma/fills/<id>.png):
# saving overlay image, error overlay image, dither strip; and the "Photo Example" photo (15:70)
# that tools/ui_sim/ puts under the UI so its screens compare 1:1 with the Figma frames.
FILL_NODES = ["83:2240", "83:2288", "83:2217", "15:70"]


def token():
    t = os.environ.get("FIGMA_TOKEN", "").strip()
    if not t:
        sys.exit("FIGMA_TOKEN is not set (Figma personal access token, scope file_content:read)")
    return t


MAX_WAIT = 120  # s; Figma sometimes answers 429 with Retry-After of days (seen 2026-11-01 on /images)


class RateLimited(Exception):
    pass


def get(path, params=None, tries=4):
    url = f"{API}{path}"
    if params:
        url += "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"X-Figma-Token": token()})
    for attempt in range(tries):
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code == 429 and attempt < tries - 1:  # rate limited: back off
                wait = int(e.headers.get("Retry-After", 2 ** (attempt + 2)))
                if wait > MAX_WAIT:
                    raise RateLimited(f"GET {path}: rate limited, Retry-After {wait} s ({wait / 3600:.1f} h)")
                print(f"rate limited, waiting {wait} s", file=sys.stderr)
                time.sleep(wait)
                continue
            body = e.read().decode(errors="replace")[:300]
            sys.exit(f"GET {path} failed: HTTP {e.code} {body}")


def download(url, dest):
    with urllib.request.urlopen(url, timeout=120) as r:
        dest.write_bytes(r.read())


def safe(node_id):
    return re.sub(r"[^0-9A-Za-z_-]", "-", node_id)


def collect_variants(node, acc):
    """Component sets and components inside a node tree (for per-variant renders)."""
    if node.get("type") in ("COMPONENT", "COMPONENT_SET", "INSTANCE"):
        acc.append((node["id"], node.get("name", "")))
    for ch in node.get("children", []):
        collect_variants(ch, acc)


def find(n, nid):
    if n.get("id") == nid:
        return n
    for ch in n.get("children", []):
        r = find(ch, nid)
        if r:
            return r
    return None


def pull_fills(data):
    refs = {}
    for nid in FILL_NODES:
        for entry in data.get("nodes", {}).values():
            n = find(entry["document"], nid)
            if n:
                img = [f for f in n.get("fills", []) if f.get("type") == "IMAGE" and f.get("visible", True)]
                if img:
                    refs[nid] = img[0]["imageRef"]
                break
    if not refs:
        return
    try:
        urls = get(f"/files/{FILE_KEY}/images").get("meta", {}).get("images", {})
    except RateLimited as e:
        print(f"{e}; image fills skipped", file=sys.stderr)
        return
    (OUT / "fills").mkdir(exist_ok=True)
    for nid, ref in refs.items():
        if ref in urls:
            download(urls[ref], OUT / "fills" / f"{safe(nid)}.png")
            print(f"fill {nid} saved")


def main(argv):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "nodes").mkdir(exist_ok=True)
    (OUT / "export").mkdir(exist_ok=True)

    # Published components only (empty while the file has no published library). Local components
    # are taken from the /nodes responses below and merged in.
    comps = get(f"/files/{FILE_KEY}/components").get("meta", {}).get("components", [])
    sets = get(f"/files/{FILE_KEY}/component_sets").get("meta", {}).get("component_sets", [])
    listing = {
        "components": {
            c["node_id"]: {"name": c["name"], "published": True,
                           "set": (c.get("containing_frame") or {}).get("containingComponentSet", {}).get("name")}
            for c in comps
        },
        "component_sets": {s["node_id"]: {"name": s["name"], "published": True} for s in sets},
    }

    def write_listing():
        out = {
            "components": [{"id": k, **v} for k, v in sorted(listing["components"].items())],
            "component_sets": [{"id": k, **v} for k, v in sorted(listing["component_sets"].items())],
        }
        (OUT / "components.json").write_text(json.dumps(out, indent=2, ensure_ascii=False) + "\n")
        print(f"components: {len(out['components'])}, sets: {len(out['component_sets'])}")

    if "--components" in argv:
        write_listing()
        return

    ids = [a for a in argv if re.fullmatch(r"\d+[:-]\d+", a)] or DEFAULT_NODES
    ids = [i.replace("-", ":") for i in ids]
    data = get(f"/files/{FILE_KEY}/nodes", {"ids": ",".join(ids), "geometry": "paths"})
    render = []
    for nid, entry in data.get("nodes", {}).items():
        doc = entry["document"]
        (OUT / "nodes" / f"{safe(nid)}.json").write_text(json.dumps(entry, indent=1, ensure_ascii=False) + "\n")
        print(f"node {nid} '{doc.get('name')}' saved")
        render.append((nid, doc.get("name", "")))
        set_names = {k: v.get("name") for k, v in entry.get("componentSets", {}).items()}
        for k, v in entry.get("componentSets", {}).items():
            listing["component_sets"].setdefault(k, {"name": v.get("name"), "published": False})
        for k, v in entry.get("components", {}).items():
            listing["components"].setdefault(
                k, {"name": v.get("name"), "published": False, "set": set_names.get(v.get("componentSetId"))})
        collect_variants(doc, render)

    write_listing()
    pull_fills(data)

    # Renders: requested nodes + every component/variant inside them, 1x (device pixels).
    seen, todo = set(), []
    for nid, name in render:
        if nid not in seen:
            seen.add(nid)
            todo.append((nid, name))
    # Existing PNGs are kept when the images endpoint is rate limited (they are reference only).
    if "--no-renders" not in argv:
        try:
            for i in range(0, len(todo), 50):
                chunk = todo[i : i + 50]
                imgs = get(f"/images/{FILE_KEY}", {"ids": ",".join(n for n, _ in chunk), "format": "png", "scale": 1})
                for nid, name in chunk:
                    url = (imgs.get("images") or {}).get(nid)
                    if url:
                        download(url, OUT / "export" / f"{safe(nid)}.png")
        except RateLimited as e:
            print(f"{e}; renders skipped, existing PNGs kept", file=sys.stderr)
    index = [{"id": n, "name": nm, "png": f"export/{safe(n)}.png"} for n, nm in todo
             if (OUT / "export" / f"{safe(n)}.png").exists()]
    (OUT / "export" / "index.json").write_text(json.dumps(index, indent=2, ensure_ascii=False) + "\n")
    print(f"renders: {len(index)} of {len(todo)} in ui/figma/export/")


if __name__ == "__main__":
    main(sys.argv[1:])
