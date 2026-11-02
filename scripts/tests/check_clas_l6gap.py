#!/usr/bin/env python3
"""Check a CLAS RT solution through L6 frame losses (#363).

Reads an NMEA solution (GGA) of the clas_l6gap replay and gates, from the
first fixed epoch to the last epoch of the replay (--end, UTC; the last
solution if not given, so a lost tail would go unnoticed):

- fix rate on the 1 Hz timeline >= --min-fix (an epoch without a solution
  counts as not fixed);
- horizontal error of every Fix and Float epoch <= --max-herr m: a subframe
  decoded from shifted bits gave garbage corrections and Float errors of
  6-27 m in this window;
- at most --max-misfix fixed epochs with a horizontal error > 0.30 m;
- no gap between solutions, or between the last one and --end, longer than
  --max-gap s.

RT replay is not bit-reproducible, so there is no RMS gate. --report prints
the metrics without gating; --selftest checks the gates on synthetic runs.

Usage:
    check_clas_l6gap.py NMEA --llh LAT,LON,H [--end HH:MM:SS] [thresholds] [--report]
"""

import argparse
import math
import sys

MISFIX_M = 0.30


def read_gga(path, ref):
    """[(utc_s, quality, horizontal error m, up error m)] from GGA sentences."""
    lat0, lon0, h0 = ref
    rows = []
    with open(path, errors="replace") as fp:
        for line in fp:
            f = line.strip().split(",")
            if len(f) < 12 or not f[0].endswith("GGA") or not f[1] or not f[2]:
                continue
            t = int(f[1][:2]) * 3600 + int(f[1][2:4]) * 60 + float(f[1][4:])
            lat = (int(f[2][:2]) + float(f[2][2:]) / 60) * (1 if f[3] == "N" else -1)
            lon = (int(f[4][:3]) + float(f[4][3:]) / 60) * (1 if f[5] == "E" else -1)
            dn = math.radians(lat - lat0) * 6378137.0
            de = math.radians(lon - lon0) * 6378137.0 * math.cos(math.radians(lat0))
            rows.append((t, int(f[6] or 0), math.hypot(dn, de), float(f[9]) + float(f[11]) - h0))
    return rows


def metrics(rows, end=None):
    m = {"epochs": len(rows)}
    fixed = [r for r in rows if r[1] == 4]
    if not fixed:
        return m
    t0 = fixed[0][0]
    t1 = rows[-1][0] if end is None else max(end, rows[-1][0])
    after = [r for r in rows if r[0] >= t0]
    t = [r[0] for r in after] + [t1]
    m["first_fix"] = t0 - rows[0][0]
    m["max_gap"] = max([b - a for a, b in zip(t, t[1:])], default=0.0)
    m["fix_rate"] = sum(1 for r in after if r[1] == 4) / (round(t1 - t0) + 1)
    precise = [r for r in after if r[1] in (4, 5)]
    m["max_herr"] = max(r[2] for r in precise)
    m["misfix"] = sum(1 for r in after if r[1] == 4 and r[2] > MISFIX_M)
    up = sorted(r[3] for r in after if r[1] == 4)
    m["median_up"] = up[len(up) // 2]
    return m


def gate(m, args):
    if "fix_rate" not in m:
        return ["no fixed solution"]
    fails = []
    if m["fix_rate"] < args.min_fix:
        fails.append(f"fix rate {m['fix_rate']:.4f} < {args.min_fix}")
    if m["max_herr"] > args.max_herr:
        fails.append(f"Fix/Float horizontal error {m['max_herr']:.2f} m > {args.max_herr} m")
    if m["misfix"] > args.max_misfix:
        fails.append(f"{m['misfix']} fixed epochs > {MISFIX_M} m > {args.max_misfix}")
    if m["max_gap"] > args.max_gap:
        fails.append(f"solution gap {m['max_gap']:.1f} s > {args.max_gap} s")
    return fails


def selftest(args):
    """Synthetic 1200 s run: each defect must trip its gate."""

    end = 1199.0

    def run(q=lambda t: 4, herr=lambda t: 0.02, drop=(0, 0)):
        return [(float(t), q(t), herr(t), 0.0) for t in range(1200) if not drop[0] <= t < drop[1]]

    cases = [
        ("clean, 20 s Single", run(q=lambda t: 1 if 500 <= t < 520 else 4), None),
        (
            "20 s Float at 27 m",
            run(
                q=lambda t: 5 if 500 <= t < 520 else 4,
                herr=lambda t: 27.0 if 500 <= t < 520 else 0.02,
            ),
            "horizontal",
        ),
        ("misfix at 0.5 m", run(herr=lambda t: 0.5 if 500 <= t < 503 else 0.02), "fixed epochs"),
        ("long outage", run(q=lambda t: 5 if 300 <= t < 500 else 4), "fix rate"),
        ("missing epochs", run(drop=(500, 520)), "gap"),
        ("lost tail", run(drop=(1140, 1200)), "gap"),
        ("never fixed", run(q=lambda t: 5), "no fixed"),
    ]
    bad = 0
    for label, rows, want in cases:
        fails = gate(metrics(rows, end), args)
        ok = (not fails) if want is None else any(want in f for f in fails)
        print(f"{'ok  ' if ok else 'FAIL'} {label}: {fails or 'pass'}")
        bad += not ok
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("nmea", nargs="?")
    ap.add_argument("--llh", help="reference LAT,LON,H (deg, deg, ellipsoidal m)")
    ap.add_argument("--end", help="last epoch of the replay, UTC HH:MM:SS")
    ap.add_argument("--min-fix", type=float, default=0.95)
    ap.add_argument("--max-herr", type=float, default=1.0)
    ap.add_argument("--max-misfix", type=int, default=0)
    ap.add_argument("--max-gap", type=float, default=5.0)
    ap.add_argument("--report", action="store_true", help="print the metrics, no gating")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        return selftest(args)
    if not args.nmea or not args.llh:
        ap.error("NMEA and --llh are required")
    end = None
    if args.end:
        h, mi, se = args.end.split(":")
        end = int(h) * 3600 + int(mi) * 60 + float(se)
    m = metrics(read_gga(args.nmea, [float(x) for x in args.llh.split(",")]), end)
    print(" ".join(f"{k}={v:.4f}" if isinstance(v, float) else f"{k}={v}" for k, v in m.items()))
    if args.report:
        return 0
    fails = gate(m, args)
    for f in fails:
        print(f"FAIL: {f}")
    print("PASS" if not fails else "FAILED")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
