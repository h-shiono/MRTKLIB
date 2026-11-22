#!/usr/bin/env python3
"""check_has_ppp.py - Galileo HAS float-PPP regression check.

Validates a ``mrtk post`` Galileo HAS solution (.pos) against:

  1. Solution availability  : epochs >= min_ratio * expected.
  2. Solution quality       : every solution epoch has Q == 6 (PPP float).
  3. Absolute accuracy      : over the last N minutes, the mean position and/or
                              the 95th percentile of the per-epoch horizontal and
                              vertical errors lie within the thresholds, against
                              a fixed reference ECEF coordinate.
  4. System usage (optional): from the solution status file (``-y 2``), the
                              mean number of satellites used per epoch of each
                              listed system reaches its minimum, so a system
                              that silently drops out fails (#473).

The reference coordinate is the surveyed/derived ITRF position of the receiver.
It is passed on the command line (the CMake test records its provenance).

Reference provenance (for the bundled G5P3162a HAS fixture)
-----------------------------------------------------------
  Receiver static position from MADOCA-PPP (static + combined PPP-AR) over the
  full hour, ITRF current epoch (2026-06-12), ~1-2 cm.
    XYZ = -3961905.2046 3348992.8024 3698212.4790
    LLH = 35.666341868 deg, 139.792211087 deg, 59.837 m
  HAS float accuracy against this reference: docs/design/gal-has.md §11.2.

Usage
-----
    check_has_ppp.py SOLUTION.pos --ref-xyz X Y Z [options]

Options
-------
    --ref-xyz X Y Z      Reference ECEF coordinate [m] (required)
    --expected-epochs N  Expected solution-epoch count (for the ratio gate)
    --min-ratio R        Minimum (solutions / expected), default 0.95
    --last-min M         Averaging window at the end of the run [min], default 5
    --interval SEC       Solution interval [s], default 1
    --max-horiz M        Max horizontal offset of the window mean [m]
    --max-3d M           Max 3D offset of the window mean [m]
    --max-h95 M          Max 95th percentile horizontal error in the window [m]
    --max-v95 M          Max 95th percentile vertical error in the window [m]
    --stat FILE          Solution status file ($SAT records) for --min-sats
    --min-sats S=N ...   Min mean satellites used per epoch, per system letter
                         (e.g. G=4 E=3)
"""

import argparse
import sys
from collections import defaultdict

import numpy as np
from _geo import blh2xyz, xyz2blh, xyz2enu  # noqa: E402


def read_pos(path):
    """Read an RTKLIB llh .pos file -> list of (lat, lon, h, Q)."""
    rows = []
    with open(path) as f:
        for line in f:
            if line.startswith("%") or not line.strip():
                continue
            p = line.split()
            try:
                lat, lon, h, q = float(p[2]), float(p[3]), float(p[4]), int(p[5])
            except (IndexError, ValueError):
                continue
            rows.append((lat, lon, h, q))
    return rows


def read_sat_usage(path):
    """Read $SAT records -> (epoch count, {system letter: used satellite count})."""
    epochs = set()
    used = defaultdict(int)
    with open(path) as f:
        for line in f:
            if not line.startswith("$SAT"):
                continue
            p = line.split(",")
            if p[4] != "1":
                continue
            epochs.add(p[2])
            if p[9] == "1":
                used[p[3][0]] += 1
    return len(epochs), used


def main(argv=None):
    """CLI entry point for the HAS float-PPP regression check."""
    ap = argparse.ArgumentParser(description="Galileo HAS float-PPP regression check.")
    ap.add_argument("solution", help="mrtk post .pos solution file")
    ap.add_argument("--ref-xyz", type=float, nargs=3, required=True, metavar=("X", "Y", "Z"))
    ap.add_argument("--expected-epochs", type=int, default=900)
    ap.add_argument("--min-ratio", type=float, default=0.95)
    ap.add_argument("--last-min", type=float, default=5.0)
    ap.add_argument("--interval", type=float, default=1.0)
    ap.add_argument("--max-horiz", type=float, default=None)
    ap.add_argument("--max-3d", type=float, default=None)
    ap.add_argument("--max-h95", type=float, default=None)
    ap.add_argument("--max-v95", type=float, default=None)
    ap.add_argument("--stat", default=None)
    ap.add_argument("--min-sats", nargs="+", default=[], metavar="S=N")
    args = ap.parse_args(argv)

    rows = read_pos(args.solution)
    n = len(rows)
    ref = np.array(args.ref_xyz)
    rlat, rlon, _ = xyz2blh(*ref)

    ok = True

    # (1) availability
    ratio = n / args.expected_epochs if args.expected_epochs else 0.0
    avail_ok = ratio >= args.min_ratio
    ok &= avail_ok
    tag = "OK" if avail_ok else "FAIL"
    print(f"epochs       : {n} / {args.expected_epochs} = {ratio:.3f}  (>= {args.min_ratio}) {tag}")

    if n == 0:
        print("FAIL: no solution epochs")
        return 1

    # (2) quality: all Q == 6
    qbad = [q for *_, q in rows if q != 6]
    q_ok = len(qbad) == 0
    ok &= q_ok
    qset = sorted({q for *_, q in rows})
    print(f"quality      : Q values {qset}, non-Q6 epochs={len(qbad)} {'OK' if q_ok else 'FAIL'}")

    # (3) absolute accuracy of the last-N-min mean
    nlast = int(round(args.last_min * 60.0 / args.interval))
    nlast = min(nlast, n)
    xyz = np.array([blh2xyz(lat, lon, h) for lat, lon, h, _ in rows[-nlast:]])
    mean_xyz = xyz.mean(axis=0)
    enu = xyz2enu(mean_xyz - ref, rlat, rlon)
    horiz = float(np.hypot(enu[0], enu[1]))
    d3 = float(np.linalg.norm(enu))
    errs = np.array([xyz2enu(x - ref, rlat, rlon) for x in xyz])
    h95 = float(np.percentile(np.hypot(errs[:, 0], errs[:, 1]), 95))
    v95 = float(np.percentile(np.abs(errs[:, 2]), 95))
    print(
        f"accuracy     : last {args.last_min:.0f} min ({nlast} ep) mean "
        f"dENU=[{enu[0]:+.3f} {enu[1]:+.3f} {enu[2]:+.3f}] m"
    )
    for name, val, lim in (
        ("horiz", horiz, args.max_horiz),
        ("3D   ", d3, args.max_3d),
        ("H95  ", h95, args.max_h95),
        ("V95  ", v95, args.max_v95),
    ):
        if lim is None:
            print(f"               {name}={val:.3f} m")
            continue
        g_ok = val <= lim
        ok &= g_ok
        print(f"               {name}={val:.3f} m (<= {lim}) {'OK' if g_ok else 'FAIL'}")

    # (4) satellites used per system
    if args.min_sats:
        if not args.stat:
            print("FAIL: --min-sats needs --stat")
            return 1
        nep, used = read_sat_usage(args.stat)
        for spec in args.min_sats:
            sysc, nmin = spec.split("=")
            mean = used.get(sysc, 0) / nep if nep else 0.0
            s_ok = mean >= float(nmin)
            ok &= s_ok
            print(
                f"system usage : {sysc} mean {mean:.2f} sats/epoch (>= {nmin}) {'OK' if s_ok else 'FAIL'}"
            )

    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
