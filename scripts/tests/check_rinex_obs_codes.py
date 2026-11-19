#!/usr/bin/env python3
"""Check which observation types carry data in a RINEX 3 observation file (#349).

Counts the non-blank values per system and observation type and compares them
with the types that must (--present) or must not (--absent) carry data, e.g.

    check_rinex_obs_codes.py vrs.obs --present J:L1X,L2X,L5X --absent J:L1C

The VRS observations of `mrtk ssr2obs` took the first three CSSR signals of a
satellite, kept both QZS L1 signals and wrote a fixed list of codes: on the
2025/022 data QZS came out as L1 C/A + L2C with no L5, and the carrier phase
was NaN in ~400 epochs per satellite (ionosphere compensation over two L1
signals). claslib 0.9.0 outputs one L1 signal, L1C with -JL1X.
"""

import argparse
import math
import sys
from collections import Counter
from pathlib import Path


def count_values(path: Path) -> tuple[Counter, int]:
    """Return ({(system, obs type): values}, number of NaN / Inf values)."""
    types: dict[str, list[str]] = {}
    count: Counter = Counter()
    bad = 0
    header = True
    cont = ""
    for line in path.read_text(encoding="ascii", errors="replace").splitlines():
        if header:
            if "SYS / # / OBS TYPES" in line:
                if line[0] != " ":
                    cont = line[0]
                    types[cont] = []
                types[cont] += line[7:60].split()
            elif "END OF HEADER" in line:
                header = False
            continue
        if not line or line[0] == ">":
            continue
        for i, name in enumerate(types.get(line[0], [])):
            field = line[3 + 16 * i : 3 + 16 * i + 14].strip()
            if not field:
                continue
            try:
                value = float(field)
            except ValueError:
                bad += 1
                continue
            if math.isfinite(value):
                count[(line[0], name)] += 1
            else:
                bad += 1
    return count, bad


def parse_spec(specs: list[str]) -> list[tuple[str, str]]:
    """Expand ["J:L1X,L5X", ...] into [("J", "L1X"), ("J", "L5X"), ...]."""
    out = []
    for spec in specs:
        system, names = spec.split(":")
        out += [(system, name) for name in names.split(",")]
    return out


def main() -> int:
    """Check the observation types and report the result."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("obs", type=Path, help="RINEX 3 observation file")
    parser.add_argument("--present", action="append", default=[], help="SYS:TYPE[,TYPE...]")
    parser.add_argument("--absent", action="append", default=[], help="SYS:TYPE[,TYPE...]")
    parser.add_argument("--min-records", type=int, default=1000, help="values per present type")
    args = parser.parse_args()

    count, bad = count_values(args.obs)
    ok = bad == 0
    parts = [f"{bad} NaN/Inf values (must be 0)"]
    for key in parse_spec(args.present):
        ok = ok and count[key] >= args.min_records
        parts.append(f"{key[0]} {key[1]} {count[key]} (>= {args.min_records})")
    for key in parse_spec(args.absent):
        ok = ok and count[key] == 0
        parts.append(f"{key[0]} {key[1]} {count[key]} (must be 0)")
    print(f"{'PASS' if ok else 'FAIL'}: {', '.join(parts)}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
