#!/usr/bin/env python3
"""Update the rig's firmware over WiFi (OTA): the cameras one by one, then the Core.

Run it from the repo root. Needs PlatformIO (`pio`) on PATH. Only the full firmware (core,
cam1..cam4) has OTA; steps/ envs need USB.

  python tools/ota.py              # every online camera, then the Core
  python tools/ota.py cams         # every online camera
  python tools/ota.py cam2 cam4    # just these
  python tools/ota.py core         # just the Core
  python tools/ota.py --dry-run    # print what it would do

On a Mac it switches WiFi for you: it builds first (while you still have internet), joins the
rig's WiFi if the Core isn't reachable, and rejoins your home WiFi at the end, also after a
failure or Ctrl-C. Home WiFi name: --home, or X4_HOME_WIFI (password, if macOS asks for one:
X4_HOME_WIFI_PASS); otherwise the script reads it or asks. --stay keeps you on x4cam,
--no-wifi never touches WiFi. Elsewhere: join x4cam yourself first.

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
IS_MAC = sys.platform == "darwin"


def secret(board, name, default=""):
    """A string #define from <board>/include/secrets.h, else secrets.h.example (as the build)."""
    inc = os.path.join(ROOT, board, "include")
    for fn in ("secrets.h", "secrets.h.example"):
        p = os.path.join(inc, fn)
        if os.path.exists(p):
            m = re.search(r'#define\s+' + name + r'\s+"([^"]*)"', open(p, encoding="utf-8").read())
            if m:
                return m.group(1)
            return default  # defined some other way (e.g. X4_AP_SSID = X4_PROJECT_NAME)
    return default


def ota_pass(board):
    return secret(board, "X4_OTA_PASS")


# ---- Mac WiFi switching (networksetup ships with macOS) ----------------------------------------

def sh(*cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return (r.stdout + r.stderr).strip()


def wifi_device():
    out = sh("networksetup", "-listallhardwareports")
    m = re.search(r"Hardware Port: (?:Wi-Fi|AirPort)\s*\nDevice: (\S+)", out)
    return m.group(1) if m else "en0"


def current_ssid(dev):
    """The joined network's name, or None. Since macOS 15 this is often hidden from scripts
    (location privacy), so callers must cope with None."""
    m = re.match(r"Current Wi-Fi Network: (.+)", sh("networksetup", "-getairportnetwork", dev))
    name = m.group(1).strip() if m else None
    return None if not name or "redacted" in name.lower() else name


def join(dev, ssid, password):
    """Join a network. networksetup prints nothing on success and an error text otherwise."""
    out = sh("networksetup", "-setairportnetwork", dev, ssid, *([password] if password else []))
    return not re.search(r"error|could not|failed|unable", out, re.I), out


def core_reachable(core_ip, timeout_s=0):
    t0 = time.time()
    while True:
        try:
            status(core_ip)
            return True
        except Exception:
            if time.time() - t0 >= timeout_s:
                return False
            time.sleep(2)


def home_network(a, dev):
    if a.home:
        return a.home
    if os.environ.get("X4_HOME_WIFI"):
        return os.environ["X4_HOME_WIFI"]
    name = current_ssid(dev)
    if name:
        return name
    if sys.stdin.isatty():
        name = input("Which WiFi should I return to afterwards? (macOS hides it from scripts; "
                     "Enter = stay on the rig's WiFi) ").strip()
        return name or None
    return None


def prebuild(cams, do_core, dry):
    """Build while the Mac still has internet (a first build downloads packages)."""
    jobs = []
    if cams:
        jobs.append(["pio", "run", "-d", os.path.join(ROOT, "camera")] + sum((["-e", f"cam{c}"] for c in cams), []))
    if do_core:
        jobs.append(["pio", "run", "-d", os.path.join(ROOT, "core"), "-e", "core"])
    for cmd in jobs:
        print("  $", " ".join(cmd))
        if not dry and subprocess.call(cmd) != 0:
            sys.exit("Build failed: nothing was uploaded, WiFi untouched.")


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
    ap.add_argument("--home", help="Mac: WiFi to rejoin at the end (default: X4_HOME_WIFI, or the current one)")
    ap.add_argument("--stay", action="store_true", help="Mac: stay on the rig's WiFi at the end")
    ap.add_argument("--no-wifi", action="store_true", help="Mac: never switch WiFi")
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

    switch = IS_MAC and not a.no_wifi
    if switch:
        prebuild(cams, do_core, a.dry_run)

    dev = wifi_device() if switch else None
    home = None
    joining = switch and not core_reachable(a.core_ip)
    if joining:
        home = None if a.stay else home_network(a, dev)
    try:
        if joining:
            ssid, pw = secret("core", "X4_AP_SSID", "x4cam"), secret("core", "X4_AP_PASS")
            print(f"WiFi ({dev}): joining {ssid}" + (f", back to {home} at the end" if home else ""))
            if not a.dry_run:
                ok, out = join(dev, ssid, pw)
                if not ok or not core_reachable(a.core_ip, 30):
                    sys.exit(f"Couldn't reach the Core on {ssid} ({out or 'no answer'}). Is the rig on?")
        failed = update(a, cams, do_core)
    finally:
        if home and not a.dry_run:
            pw = os.environ.get("X4_HOME_WIFI_PASS", "")
            ok, out = join(dev, home, pw)
            print(f"WiFi: back on {home}" if ok else
                  f"WiFi: couldn't rejoin {home} ({out}). Pick it from the WiFi menu, or set X4_HOME_WIFI_PASS.")
        elif home:
            print(f"WiFi: would rejoin {home}")
        elif joining and not a.stay:
            print("WiFi: staying on the rig's WiFi (set X4_HOME_WIFI=<name> to switch back next time)")

    if failed:
        sys.exit("Not done: " + ", ".join(failed))
    print("Done.")


def update(a, cams, do_core):
    """Cameras one by one, then the Core. Returns what didn't get updated."""
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
            print(f"core {a.core_ip}: uploading (its WiFi drops while it reboots)")
            if not run_upload("core", "core", a.core_ip, ota_pass("core"), a.dry_run):
                failed.append("core")

    return failed


if __name__ == "__main__":
    main()
