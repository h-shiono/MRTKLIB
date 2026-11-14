#!/usr/bin/env python3
"""Check that a high SNR mask removes satellites from a solution (#479).

Compares the mean GGA satellite count of a run with a high SNR mask against a
reference run without it, over the epochs both have. Before #479 the CLAS
PPP-RTK and VRS paths scaled the C/N0 by claslib's 0.25 dBHz unit instead of
0.001 dBHz, so the mask never rejected anything and both counts were equal.
"""

import argparse
import sys
from pathlib import Path


def gga_nsat(path: Path) -> dict:
    """Return {time: satellite count} from the GGA sentences of an NMEA file."""
    nsat = {}
    for line in path.read_text(encoding="ascii", errors="replace").splitlines():
        f = line.split(",")
        if len(f) > 7 and f[0].endswith("GGA") and f[6] not in ("", "0") and f[7]:
            nsat[f[1]] = int(f[7])
    return nsat


def main() -> int:
    """Compare the satellite counts and report the result."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("masked", type=Path, help="NMEA output with the high SNR mask")
    parser.add_argument("reference", type=Path, help="NMEA output without it")
    parser.add_argument(
        "--min-drop", type=float, default=1.0, help="required drop of the mean satellite count"
    )
    parser.add_argument("--min-epochs", type=int, default=300, help="required common epochs")
    args = parser.parse_args()

    masked = gga_nsat(args.masked)
    ref = gga_nsat(args.reference)
    common = sorted(set(masked) & set(ref))
    if len(common) < args.min_epochs:
        print(f"FAIL: {len(common)} common epochs (< {args.min_epochs})")
        return 1
    mean_m = sum(masked[t] for t in common) / len(common)
    mean_r = sum(ref[t] for t in common) / len(common)
    drop = mean_r - mean_m
    ok = drop >= args.min_drop
    print(
        f"{'PASS' if ok else 'FAIL'}: {len(common)} epochs, mean nsat {mean_r:.2f} -> {mean_m:.2f} "
        f"(drop {drop:.2f}, need >= {args.min_drop:.2f})"
    )
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
