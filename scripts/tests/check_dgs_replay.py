#!/usr/bin/env python3
"""Check the real-time CLAS replay through the DGS input (#570).

Reads a .pos solution (llh, GPS week / TOW) of ``rtkrcv_rt_dgs_clas`` and
gates on behaviour only, not on accuracy: the test covers the wiring of
``format = "dgs"`` in mrtk run (frames reach the CLAS engine), while the CLAS
real-time accuracy is guarded by the rtkrcv_rt_clas* tests.

- solutions: at least --min-epochs epochs with a solution;
- first fix: within --first-fix s of the first solution (no corrections would
  leave the engine Single, broken routing would never fix);
- fix: at least --min-fix fixed epochs.

Usage:
    check_dgs_replay.py POS [--min-epochs N] [--first-fix S] [--min-fix N] [--report]
"""

import argparse
import sys


def read_pos(path):
    """[(tow, quality)] of the solution lines of an llh .pos file (TOW time format)."""
    rows = []
    with open(path) as fp:
        for line in fp:
            if line.startswith("%") or not line.strip():
                continue
            f = line.split()
            rows.append((float(f[1]), int(f[5])))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("pos")
    ap.add_argument("--min-epochs", type=int, default=270)
    ap.add_argument("--first-fix", type=float, default=60.0)
    ap.add_argument("--min-fix", type=int, default=200)
    ap.add_argument("--report", action="store_true", help="print the metrics without gating")
    args = ap.parse_args()

    rows = read_pos(args.pos)
    fixed = [t for t, q in rows if q == 1]
    first = fixed[0] - rows[0][0] if rows and fixed else None
    count = {q: sum(1 for _, x in rows if x == q) for q in (1, 2, 5)}
    print(
        f"epochs {len(rows)}, fix/float/single {count[1]}/{count[2]}/{count[5]}, "
        f"first fix {first if first is not None else 'none'} s"
    )
    if args.report:
        return 0

    errors = []
    if len(rows) < args.min_epochs:
        errors.append(f"{len(rows)} epochs < {args.min_epochs}")
    if first is None or first > args.first_fix:
        errors.append(f"first fix {first} s > {args.first_fix} s")
    if len(fixed) < args.min_fix:
        errors.append(f"{len(fixed)} fixed epochs < {args.min_fix}")
    for e in errors:
        print(f"FAIL: {e}", file=sys.stderr)
    if not errors:
        print("OK")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
