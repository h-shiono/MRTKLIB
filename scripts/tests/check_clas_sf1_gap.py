#!/usr/bin/env python3
"""Check recovery after a lost CLAS subframe 1 in an RT replay (#406).

For every frame epoch T (GPST, multiple of --period) whose subframe 1 was
removed by make_clas_sf1_gap.py and that the solution entered in Fix, count
the Single epochs in [T, T+70 s] and require Fix again by T+70 s. Before #406
the orbit of frame T+30 could not be paired with a code bias until the next
network phase bias arrived, and the solution stayed Single for that whole
interval; now it is Single only while the previous orbit is past its 60 s
validity and the next one has not arrived.

Usage:
    check_clas_sf1_gap.py OUT.nmea [--period 300] [--max-single 25] [--min-events 2]
"""

import argparse
import sys

GPST_UTC = 18  # leap seconds, 2019+
WINDOW = 70


def load_gga(path: str) -> dict:
    """GPST second of day -> GGA quality."""
    q = {}
    for line in open(path):
        if not line.startswith(("$GPGGA", "$GNGGA")):
            continue
        f = line.split(",")
        if len(f) < 7 or not f[1] or not f[6]:
            continue
        hms = f[1]
        sod = int(hms[:2]) * 3600 + int(hms[2:4]) * 60 + int(float(hms[4:]))
        q[(sod + GPST_UTC) % 86400] = f[6]
    return q


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("nmea")
    ap.add_argument("--period", type=int, default=300)
    ap.add_argument("--max-single", type=int, default=25)
    ap.add_argument("--min-events", type=int, default=2)
    args = ap.parse_args()

    q = load_gga(args.nmea)
    if not q:
        print(f"ERROR: no GGA in {args.nmea}")
        return 1
    first, last = min(q), max(q)
    nev = nfail = 0
    for t in range((first // args.period + 1) * args.period, last - WINDOW, args.period):
        if q.get(t - 1) != "4":
            continue  # not in Fix before the loss
        win = [q.get(t + k, "-") for k in range(WINDOW + 1)]
        nsingle = win.count("1")
        refix = "4" in win[31:]
        ok = nsingle <= args.max_single and refix
        nev += 1
        nfail += not ok
        print(
            f"T={t:5d} single={nsingle:2d} refix={'yes' if refix else 'no '} {'ok' if ok else 'FAIL'}  {''.join(win)}"
        )
    if nev < args.min_events:
        print(f"FAIL: only {nev} gap events evaluated (need {args.min_events})")
        return 1
    print(
        f"{nev} events, {nfail} failed (max single {args.max_single} s, Fix again by T+{WINDOW} s)"
    )
    return 1 if nfail else 0


if __name__ == "__main__":
    sys.exit(main())
