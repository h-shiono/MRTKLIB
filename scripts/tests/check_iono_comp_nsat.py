#!/usr/bin/env python3
"""Check that SSR ionosphere compensation keeps every satellite (#345).

Compares the mean GGA satellite count of a CLAS PPP-RTK run with
iono_compensation = "ssr" against the same data with "meas", over the epochs
both have. When a satellite's CSSR signals hold two L1-band codes (QZS L1C/A
+ L1C), compensatedisp() divided by 1 - fi^2 = 0; the 0/0 coefficient made
the compensation NaN and dropped the satellite (J03 on 2025/022: mean nsat
13.17 "meas" vs 12.17 "ssr" before the fix, equal after).
"""

import argparse
import sys
from pathlib import Path

from check_snrmask_nsat import gga_nsat


def main() -> int:
    """Compare the satellite counts and report the result."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ssr", type=Path, help='NMEA output with iono_compensation = "ssr"')
    parser.add_argument("meas", type=Path, help='NMEA output with iono_compensation = "meas"')
    parser.add_argument(
        "--max-drop", type=float, default=0.5, help="allowed drop of the mean satellite count"
    )
    parser.add_argument("--min-epochs", type=int, default=300, help="required common epochs")
    args = parser.parse_args()

    ssr = gga_nsat(args.ssr)
    meas = gga_nsat(args.meas)
    common = sorted(set(ssr) & set(meas))
    if len(common) < args.min_epochs:
        print(f"FAIL: {len(common)} common epochs (< {args.min_epochs})")
        return 1
    mean_s = sum(ssr[t] for t in common) / len(common)
    mean_m = sum(meas[t] for t in common) / len(common)
    drop = mean_m - mean_s
    ok = drop <= args.max_drop
    print(
        f"{'PASS' if ok else 'FAIL'}: {len(common)} epochs, "
        f"mean nsat meas {mean_m:.2f} / ssr {mean_s:.2f} "
        f"(drop {drop:.2f}, allowed <= {args.max_drop:.2f})"
    )
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
