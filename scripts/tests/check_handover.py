#!/usr/bin/env python3
"""Check a CLAS solution through a QZSS L6 handover (#372).

Reads an NMEA solution (GGA) and the case's window and handover time from
``handover_manifest.json``. RT replay is not bit-reproducible (a few cm and
occasional fix/float flips between runs), so the gates are about behaviour,
not RMS:

- freeze (#205 / #218): no gap between solutions longer than --max-gap s, and
  solutions continue to the end of the window (within --max-gap);
- fix before: fix rate over the --pre s before the handover >= --pre-fix-rate
  (otherwise the test proves nothing about the handover);
- recovery: the last non-fixed epoch after the handover ends within
  --recover s of it (fix loss is often delayed by ~30 s, when the old
  satellite's corrections age out, so "first fix again" is not enough);
- outage: at most --max-nonfix non-fixed epochs after the handover;
  epochs are taken on the 1 Hz timeline of the window, so an epoch without
  a solution (nothing is written for SOLQ_NONE) counts as not fixed;
- satellites: median satellite count of the fixed epochs after the
  handover >= --min-nsat.

--report prints the metrics without gating (used to size the thresholds);
--selftest checks the gates on synthetic runs.

Usage:
    check_handover.py NMEA MANIFEST CASE [thresholds] [--report]
"""

import argparse
import json
import math
import statistics
import sys

LEAP_S = 18  # GPST - UTC in 2026


def read_gga(path, day):
    """[(gpst_tow_s, quality, nsat)] from GGA sentences; day = GPS day of week."""
    rows = []
    with open(path, errors="replace") as fp:
        for line in fp:
            f = line.strip().split(",")
            if len(f) < 8 or not f[0].endswith("GGA") or not f[1]:
                continue
            h, m, s = int(f[1][:2]), int(f[1][2:4]), float(f[1][4:])
            rows.append(
                (day * 86400 + h * 3600 + m * 60 + s + LEAP_S, int(f[6] or 0), int(f[7] or 0))
            )
    return rows


def metrics(rows, start, end, ho, pre=120.0):
    m = {"epochs": len(rows)}
    if not rows:
        return m
    t = [r[0] for r in rows]
    m["first"], m["last"] = t[0] - start, t[-1] - end
    m["max_gap"] = max([b - a for a, b in zip(t, t[1:])] + [t[0] - start, end - 1 - t[-1]])
    # 1 Hz timeline of the window: an epoch without a solution (no output is
    # written for SOLQ_NONE) counts as not fixed, so dropped epochs cannot hide
    # an outage behind a small gap
    sol = {round(tt): (q, n) for tt, q, n in rows}
    line = [(s_, *sol.get(s_, (0, 0))) for s_ in range(math.ceil(start), math.ceil(end))]
    before = [q for s_, q, _ in line if ho - pre <= s_ < ho]
    m["pre_fix_rate"] = sum(q == 4 for q in before) / len(before) if before else 0.0
    # recovery: the fix loss caused by a handover is often delayed (~30 s,
    # when the old satellite's corrections age out), so recovery is measured
    # to the end of the last non-fixed run after the handover: 0 if fix is
    # held throughout, else (last non-fixed epoch + 1 s) - handover
    after = [r for r in line if r[0] >= ho]
    nonfix = [s_ for s_, q, _ in after if q != 4]
    m["nonfix_epochs"] = len(nonfix)
    m["missing_after"] = sum(s_ not in sol for s_, _, _ in after)
    m["recovery"] = (nonfix[-1] + 1.0 - ho) if nonfix else 0.0
    m["fix_rate_after"] = sum(q == 4 for _, q, _ in after) / len(after) if after else 0.0
    nsat = [n for _, q, n in after if q == 4]
    m["nsat_after"] = statistics.median(nsat) if nsat else 0
    return m


def gate(m, args):
    """List of failed gates for metrics m."""
    if m["epochs"] == 0:
        return ["no solutions"]
    fails = []
    if m["max_gap"] > args.max_gap:
        fails.append(f"solution gap {m['max_gap']:.1f} s > {args.max_gap} s (freeze)")
    if m["pre_fix_rate"] < args.pre_fix_rate:
        fails.append(f"fix rate before the handover {m['pre_fix_rate']:.3f} < {args.pre_fix_rate}")
    if m["recovery"] > args.recover:
        fails.append(
            f"fix not held again until {m['recovery']:.1f} s after the handover > {args.recover} s"
        )
    if m["nonfix_epochs"] > args.max_nonfix:
        fails.append(
            f"{m['nonfix_epochs']} non-fixed epochs after the handover > {args.max_nonfix}"
        )
    if m["nsat_after"] < args.min_nsat:
        fails.append(
            f"median satellites of fixed epochs after the handover {m['nsat_after']} < {args.min_nsat}"
        )
    return fails


def selftest(args):
    """Synthetic 900 s run, handover at 600 s: each defect must trip its gate."""
    start, end, ho = 0.0, 900.0, 600.0

    def run(q=lambda t: 4, n=15, drop=(0, 0), drop_odd=(0, 0)):
        return [
            (float(t), q(t), n)
            for t in range(900)
            if not drop[0] <= t < drop[1] and not (drop_odd[0] <= t < drop_odd[1] and t % 2)
        ]

    cases = [
        (
            "clean, 30 s outage after the handover",
            run(q=lambda t: 5 if 630 <= t < 660 else 4),
            None,
        ),
        ("60 s gap (freeze)", run(drop=(700, 760)), "freeze"),
        ("solutions stop 100 s early", run(drop=(800, 900)), "freeze"),
        ("long outage", run(q=lambda t: 5 if 630 <= t < 800 else 4), "non-fixed"),
        ("short but late outage", run(q=lambda t: 5 if 750 <= t < 755 else 4), "not held again"),
        ("every other epoch missing, +40 .. +160 s", run(drop_odd=(640, 760)), "not held again"),
        ("no fix before the handover", run(q=lambda t: 1 if t < 600 else 4), "before the handover"),
        ("too few satellites", run(n=6), "satellites"),
    ]
    bad = 0
    for label, rows, want in cases:
        fails = gate(metrics(rows, start, end, ho, args.pre), args)
        ok = (not fails) if want is None else any(want in f for f in fails)
        print(f"{'ok  ' if ok else 'FAIL'} {label}: {fails or 'pass'}")
        bad += not ok
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("nmea", nargs="?")
    ap.add_argument("manifest", nargs="?")
    ap.add_argument("case", nargs="?")
    ap.add_argument("--max-gap", type=float, default=5.0)
    ap.add_argument("--pre", type=float, default=120.0)
    ap.add_argument("--pre-fix-rate", type=float, default=0.9)
    ap.add_argument("--recover", type=float, default=120.0)
    ap.add_argument("--max-nonfix", type=int, default=60)
    ap.add_argument("--min-nsat", type=float, default=10)
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--selftest", action="store_true", help="check the gates on synthetic runs")
    args = ap.parse_args()
    if args.selftest:
        return selftest(args)
    if not args.case:
        ap.error("NMEA MANIFEST CASE are required")

    with open(args.manifest) as fp:
        case = next(c for c in json.load(fp)["cases"] if c["case"] == args.case)
    start, end = (x / 1000.0 for x in case["window_tow_ms"])
    ho = case["measured_handover_tow_ms"] / 1000.0
    rows = read_gga(args.nmea, int(start // 86400))
    m = metrics(rows, start, end, ho, args.pre)
    print(
        f"{args.case}: "
        + " ".join(f"{k}={v:.3f}" if isinstance(v, float) else f"{k}={v}" for k, v in m.items())
    )
    if args.report:
        return 0
    fails = gate(m, args)
    for f in fails:
        print(f"FAIL: {f}")
    if not fails:
        print("ok")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
