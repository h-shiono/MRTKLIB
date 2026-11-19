#!/usr/bin/env python3
"""Check which frequencies a system's satellites use in a solution (#346).

Counts the valid ($SAT vsat = 1) records per frequency for one system in a
solution status file. The VRS-RTK engine took the frequency pair of every
system from gps_frequency, so qzs_frequency = "l1+l5" was ignored and QZS
stayed on frequencies 1+2 (2019/239 16:00-16:10: 1349 valid records on
frequency 2, none on 3). With the claslib 0.9.0 selection QZS runs on 1+3
(1343 records on frequency 3) while GPS stays on 1+2.
"""

import argparse
import sys
from collections import Counter
from pathlib import Path


def valid_per_freq(path: Path, system: str) -> Counter:
    """Return {frequency number: valid $SAT records} for one system letter."""
    count = Counter()
    for line in path.read_text(encoding="ascii", errors="replace").splitlines():
        f = line.split(",")
        if len(f) > 9 and f[0] == "$SAT" and f[3].startswith(system) and f[9] == "1":
            count[int(f[4])] += 1
    return count


def main() -> int:
    """Compare the frequency usage with the expected pair and report."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stat", type=Path, help="solution status file (.stat)")
    parser.add_argument("--system", default="J", help="satellite system letter")
    parser.add_argument("--second", type=int, default=3, help="expected second frequency (2 or 3)")
    parser.add_argument("--min-records", type=int, default=1000, help="required valid L1 records")
    parser.add_argument(
        "--min-ratio", type=float, default=0.95, help="required second-frequency / L1 record ratio"
    )
    args = parser.parse_args()

    count = valid_per_freq(args.stat, args.system)
    other = 5 - args.second
    first, second, unused = count[1], count[args.second], count[other]
    ratio = second / first if first else 0.0
    ok = first >= args.min_records and ratio >= args.min_ratio and unused == 0
    print(
        f"{'PASS' if ok else 'FAIL'}: {args.system} valid records "
        f"L1 {first} (>= {args.min_records}), frequency {args.second} {second} "
        f"(ratio {ratio:.3f}, >= {args.min_ratio:.2f}), frequency {other} {unused} (must be 0)"
    )
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
