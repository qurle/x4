#!/usr/bin/env python3
"""x4 log tool: turns x4 serial/HTTP log lines (logfmt) into tables and shot summaries.

Examples
    pio device monitor -e core | python tools/x4log.py               # live table
    python tools/x4log.py core.log --lvl WARN                         # warnings and worse
    python tools/x4log.py core.log --mod shoot,xfer --cols t,dev,evt,shot,cam,bytes,ms
    python tools/x4log.py core.log --shots                            # per-shot timing summary
    python tools/x4log.py core.log --csv > events.csv
    curl -s http://192.168.4.1/logs | python tools/x4log.py --errors  # error-code histogram

Lines that are not x4 lines (boot ROM output, panics) are passed through with --raw.
"""
import argparse
import csv
import re
import shlex
import sys
from collections import Counter, defaultdict

LEVELS = ["ERROR", "WARN", "INFO", "DEBUG", "TRACE"]
KV = re.compile(r'(\w+)=("(?:[^"\\]|\\.)*"|\S*)')


def parse(line):
    line = line.strip()
    if not line.startswith("t=") and " t=" not in line[:40]:
        return None
    i = line.find("t=")
    d = {}
    for k, v in KV.findall(line[i:]):
        if v.startswith('"'):
            try:
                v = shlex.split(v)[0]
            except ValueError:
                v = v.strip('"')
        d.setdefault(k, v)
    return d if "lvl" in d and "evt" in d else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", nargs="?", help="log file (default: stdin)")
    ap.add_argument("--lvl", default="TRACE", help="minimum level: ERROR, WARN, INFO, DEBUG, TRACE")
    ap.add_argument("--mod", help="comma list of modules to keep")
    ap.add_argument("--evt", help="comma list of events to keep")
    ap.add_argument("--dev", help="comma list of devices (core,cam1,...)")
    ap.add_argument("--grep", help="substring that must appear in the line")
    ap.add_argument("--cols", help="columns to print (default: t,lvl,dev,mod,evt,+rest)")
    ap.add_argument("--csv", action="store_true", help="CSV output")
    ap.add_argument("--shots", action="store_true", help="per-shot summary (skew, capture, transfer, total)")
    ap.add_argument("--errors", action="store_true", help="count err codes")
    ap.add_argument("--raw", action="store_true", help="also print non-x4 lines")
    a = ap.parse_args()

    maxlvl = LEVELS.index(a.lvl.upper()) if a.lvl.upper() in LEVELS else 4
    keep = lambda s: set(s.split(",")) if s else None
    mods, evts, devs = keep(a.mod), keep(a.evt), keep(a.dev)
    src = open(a.file, errors="replace") if a.file else sys.stdin

    shots = defaultdict(dict)
    errs = Counter()
    writer = csv.writer(sys.stdout) if a.csv else None
    cols = a.cols.split(",") if a.cols else None
    header_done = False

    for raw in src:
        d = parse(raw)
        if d is None:
            if a.raw and not (a.shots or a.errors):
                print(raw.rstrip())
            continue
        if LEVELS.index(d["lvl"]) > maxlvl if d["lvl"] in LEVELS else False:
            continue
        if mods and d.get("mod") not in mods or evts and d.get("evt") not in evts or devs and d.get("dev") not in devs:
            continue
        if a.grep and a.grep not in raw:
            continue
        if "err" in d:
            errs[(d["err"], d.get("err_name", ""))] += 1
        if a.shots and "shot" in d:
            s = shots[d["shot"]]
            e = d["evt"]
            if d.get("mod") == "shoot" and e == "start":
                s["t0"] = int(d["t"])
                s["cams"] = d.get("cams")
            elif e == "captured" and d.get("dev") == "core":
                s["skew_ms"] = int(d.get("skew_us", 0)) / 1000
                s["captured_ms"] = d.get("ms")
            elif e == "stored":
                s["total_ms"] = d.get("total_ms")
                s["stored"] = f'{d.get("stored")}/{d.get("of")}'
            elif e == "done" and d.get("mod") == "xfer":
                s.setdefault("xfer", []).append(f'c{d.get("cam")}:{d.get("ms")}ms/{d.get("kBps")}kB/s')
            elif e == "ack":
                s.setdefault("bytes", []).append(f'c{d.get("cam")}:{int(d.get("bytes", 0)) // 1024}k')
            continue
        if a.shots or a.errors:
            continue
        if cols is None:
            fixed = ["t", "lvl", "dev", "mod", "evt"]
            rest = " ".join(f"{k}={v}" for k, v in d.items() if k not in fixed)
            row = [d.get(c, "") for c in fixed] + [rest]
            if writer:
                if not header_done:
                    writer.writerow(fixed + ["fields"])
                    header_done = True
                writer.writerow(row)
            else:
                print(f"{row[0]:>9} {row[1]:<5} {row[2]:<5} {row[3]:<7} {row[4]:<16} {row[5]}")
        else:
            row = [d.get(c, "") for c in cols]
            if writer:
                if not header_done:
                    writer.writerow(cols)
                    header_done = True
                writer.writerow(row)
            else:
                print("  ".join(f"{v:<10}" for v in row))

    if a.shots:
        print(f"{'shot':>6} {'cams':>4} {'stored':>6} {'captured_ms':>11} {'total_ms':>8} {'skew_ms':>7}  transfers")
        for k in sorted(shots, key=lambda x: int(x)):
            s = shots[k]
            print(f"{k:>6} {s.get('cams', ''):>4} {s.get('stored', ''):>6} {s.get('captured_ms', ''):>11} "
                  f"{s.get('total_ms', ''):>8} {s.get('skew_ms', ''):>7}  {' '.join(s.get('xfer', []))}")
    if a.errors:
        for (code, name), n in errs.most_common():
            print(f"{n:>5}  err={code} {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
