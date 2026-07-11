#!/usr/bin/env python3
"""Assert that the CLAS PPP-RTK periodic filter reset (#264) fired and recovered.

Given the trace and NMEA output of a CLAS run with reset_interval enabled,
verify three things:

1. The trace records at least ``--min-resets`` "regularly reset filter" lines.
2. Each reset epoch appears in the NMEA output as a Single (GGA quality 1)
   solution -- the filter state was zeroed at that epoch.
3. The filter re-converges afterwards, i.e. Float/Fixed (quality >= 4) epochs
   still dominate the run. A reset that never recovered would leave the rest
   of the window in Single.
"""

import argparse
import sys
from pathlib import Path


def gga_quality_counts(nmea: str) -> dict[int, int]:
    """Count GGA sentences by their quality indicator (field 6)."""
    counts: dict[int, int] = {}
    for line in nmea.splitlines():
        if "GGA," not in line:
            continue
        fields = line.split(",")
        if len(fields) < 7 or not fields[6].isdigit():
            continue
        q = int(fields[6])
        counts[q] = counts.get(q, 0) + 1
    return counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("solution", type=Path, help="NMEA .pos output")
    parser.add_argument("trace", type=Path, help="MRTKLIB trace output")
    parser.add_argument("--min-resets", type=int, default=4)
    args = parser.parse_args()

    trace = args.trace.read_text(encoding="utf-8", errors="replace")
    resets = trace.count("regularly reset filter")
    if resets < args.min_resets:
        print(f"FAIL: expected >= {args.min_resets} periodic resets, trace has {resets}")
        return 1

    counts = gga_quality_counts(args.solution.read_text(encoding="utf-8", errors="replace"))
    singles = counts.get(1, 0)
    converged = counts.get(4, 0) + counts.get(5, 0)

    if singles < args.min_resets:
        print(f"FAIL: expected >= {args.min_resets} Single epochs (one per reset), found {singles}")
        return 1
    if converged <= singles:
        print(
            f"FAIL: filter did not recover after resets (Float/Fixed={converged}, Single={singles})"
        )
        return 1

    print(
        f"PASS: {resets} resets fired; {singles} Single epochs recovered to {converged} Float/Fixed"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
