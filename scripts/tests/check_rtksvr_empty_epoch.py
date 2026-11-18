#!/usr/bin/env python3
"""Gate `mrtk run` output across rover epochs with no observations (#382).

The rtkrcv_rt_empty_epoch replay feeds a mosaic-G5 SBF slice whose rover
epochs decode with no observations at TOW 21399-21448 (signal blockage).
Before the fix each empty epoch was passed to rtkpos() and the solution was
written again under the time of the last valid epoch (51 GGA at 05:56:20 UTC).

Checks on the NMEA GGA times:
  * strictly increasing (no repeated or backward time stamp),
  * no solution inside the empty window,
  * output on both sides of the window (the replay ran through the gap).
"""

import argparse
import sys


def gga_seconds(path):
    """Return the GGA UTC times of day [s], in file order."""
    secs = []
    with open(path) as f:
        for line in f:
            if not line.startswith(("$GPGGA", "$GNGGA")):
                continue
            hms = line.split(",")[1]
            if len(hms) < 6:
                continue
            secs.append(int(hms[0:2]) * 3600 + int(hms[2:4]) * 60 + float(hms[4:]))
    return secs


def hms(sec):
    """Format a UTC time of day [s] as HH:MM:SS.ss."""
    return f"{int(sec // 3600):02d}:{int(sec % 3600 // 60):02d}:{sec % 60:05.2f}"


def main(argv=None):
    """CLI entry point."""
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("nmea", help="mrtk run NMEA output")
    ap.add_argument(
        "--gap-start", type=float, required=True, help="first empty epoch, UTC time of day [s]"
    )
    ap.add_argument(
        "--gap-end", type=float, required=True, help="last empty epoch, UTC time of day [s]"
    )
    ap.add_argument("--min-before", type=int, default=30, help="minimum epochs before the gap")
    ap.add_argument("--min-after", type=int, default=30, help="minimum epochs after the gap")
    args = ap.parse_args(argv)

    secs = gga_seconds(args.nmea)
    fail = []

    repeats = [(a, b) for a, b in zip(secs, secs[1:]) if b <= a]
    if repeats:
        fail.append(
            f"{len(repeats)} GGA not after the previous one (first {hms(repeats[0][0])} -> {hms(repeats[0][1])})"
        )
    inside = [s for s in secs if args.gap_start - 0.005 <= s <= args.gap_end + 0.005]
    if inside:
        fail.append(f"{len(inside)} GGA inside the empty window (first {hms(inside[0])})")
    before = sum(1 for s in secs if s < args.gap_start)
    after = sum(1 for s in secs if s > args.gap_end)
    if before < args.min_before:
        fail.append(f"only {before} GGA before the gap (expected >= {args.min_before})")
    if after < args.min_after:
        fail.append(f"only {after} GGA after the gap (expected >= {args.min_after})")

    print(
        f"GGA {len(secs)}: before gap {before}, after gap {after}, "
        f"inside {len(inside)}, not increasing {len(repeats)}"
    )
    for msg in fail:
        print(f"FAIL: {msg}")
    if fail:
        return 1
    print("OK: no solution written for empty rover epochs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
