#!/usr/bin/env python3
"""Update the rig's firmware over WiFi (OTA): the cameras one by one, then the Core.

Run it from the repo root with the laptop joined to the rig's WiFi (x4cam). Needs PlatformIO
(`pio`) on PATH. Only the full firmware (core, cam1..cam4) has OTA; steps/ envs need USB.

  python tools/ota.py              # every online camera, then the Core
  python tools/ota.py cams         # every online camera
  python tools/ota.py cam2 cam4    # just these
  python tools/ota.py core         # just the Core
  python tools/ota.py --dry-run    # print what it would do

Camera IPs come from the Core (GET /api/status). The OTA password is X4_OTA_PASS from
core/include/secrets.h (or camera/include/secrets.h for cameras), else from secrets.h.example,
the same way the build picks it. docs/UPDATING.md has the details.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORE_IP = "192.168.4.1"
CAMS = 4


def ota_pass(board):
    inc = os.path.join(ROOT, board, "include")
    for name in ("secrets.h", "secrets.h.example"):
        p = os.path.join(inc, name)
        if os.path.exists(p):
            m = re.search(r'#define\s+X4_OTA_PASS\s+"([^"]*)"', open(p, encoding="utf-8").read())
            if m:
                return m.group(1)
    return ""


def get_json(url, timeout=3):
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.load(r)


def status(core_ip):
    return get_json(f"http://{core_ip}/api/status")


def cam_entry(st, cam_id):
    for c in st.get("cams", []):
        if c.get("id") == cam_id:
            return c
    return None


def run_upload(board, env, ip, password, dry):
    cmd = ["pio", "run", "-d", os.path.join(ROOT, board), "-e", env, "-t", "upload", "--upload-port", ip]
    print("  $", " ".join(cmd))
    if dry:
        return True
    e = dict(os.environ)
    e["PLATFORMIO_UPLOAD_FLAGS"] = f"--auth={password}"
    return subprocess.call(cmd, env=e) == 0


def wait_back(core_ip, cam_id, ip, timeout_s=120):
    """After an upload: wait until the camera has rebooted and registered with the Core again."""
    t0 = time.time()
    rebooted = False
    while time.time() - t0 < timeout_s:
        time.sleep(2)
        if not rebooted:
            try:
                rebooted = get_json(f"http://{ip}/info", timeout=2).get("uptime_ms", 1 << 31) < 60000
            except Exception:
                pass  # rebooting: not answering yet
            continue
        try:
            c = cam_entry(status(core_ip), cam_id)
            if c and c.get("online"):
                return c
        except Exception:
            pass
    return None


def main():
    ap = argparse.ArgumentParser(description="OTA update: cameras one by one, then the Core.")
    ap.add_argument("targets", nargs="*", help="core, cams, cam1..cam4 (default: all)")
    ap.add_argument("--core-ip", default=CORE_IP)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    targets = a.targets or ["cams", "core"]
    cams = []
    for t in targets:
        if t == "cams":
            cams += range(1, CAMS + 1)
        elif re.fullmatch(r"cam[1-4]", t):
            cams.append(int(t[3]))
        elif t != "core":
            ap.error(f"unknown target {t!r} (core, cams, cam1..cam4)")
    cams = sorted(set(cams))
    do_core = "core" in targets

    try:
        st = status(a.core_ip)
    except Exception as e:
        sys.exit(f"Can't reach the Core at {a.core_ip} ({e}). Is the laptop on the x4cam WiFi?")
    print(f"Core {a.core_ip}: fw {st.get('fw', '?')}")

    failed = []
    for cid in cams:
        c = cam_entry(st, cid)
        if not c or not c.get("online") or not c.get("ip"):
            print(f"cam{cid}: offline, skipped")
            failed.append(f"cam{cid} (offline)")
            continue
        ip = c["ip"]
        print(f"cam{cid} {ip}: fw {c.get('fw', '?')} -> uploading")
        if not run_upload("camera", f"cam{cid}", ip, ota_pass("camera"), a.dry_run):
            print(f"cam{cid}: upload FAILED. Stopping here: check docs/UPDATING.md before the next one.")
            failed.append(f"cam{cid}")
            break
        if a.dry_run:
            continue
        back = wait_back(a.core_ip, cid, ip)
        if not back:
            print(f"cam{cid}: didn't come back within 2 min. Stopping: it may need one USB flash.")
            failed.append(f"cam{cid} (not back)")
            break
        print(f"cam{cid}: back online, fw {back.get('fw', '?')}")
    else:
        if do_core:
            print(f"core {a.core_ip}: uploading (the WiFi drops while it reboots; rejoin x4cam after)")
            if not run_upload("core", "core", a.core_ip, ota_pass("core"), a.dry_run):
                failed.append("core")

    if failed:
        sys.exit("Not done: " + ", ".join(failed))
    print("Done.")


if __name__ == "__main__":
    main()
