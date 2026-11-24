"""compare_gsdc.py - Compare mrtk NMEA output against a GSDC ground_truth.csv.

The GSDC-2023 reference track is ``ground_truth.csv`` (NovAtel SPAN, post-
processed).  This module parses it into the same timed-position record shape the
PPC comparator uses, then reuses ``compare_ppc``'s epoch matching and metric
computation so the two benchmarks report identical statistics.

ground_truth.csv columns (subset used)
--------------------------------------
    LatitudeDegrees, LongitudeDegrees, AltitudeMeters (WGS84 ellipsoidal),
    SpeedMps, BearingDegrees, UnixTimeMillis (UTC)

Usage
-----
    compare_gsdc.py --ref <ground_truth.csv> <result.nmea>
                    [--skip-epochs N] [--threshold M] [--plot]
"""

import argparse
import csv
import math
import os
import sys
from pathlib import Path

import numpy as np

# Reuse the PPC comparator's NMEA parser, epoch matcher and metric computation.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare_ppc import (  # noqa: E402
    _match_epochs,
    blh2xyz,
    compute_metrics,
    continuous_epochs,
    parse_nmea,
    plot_results,
    xyz2enu,
)

# A solution within this many seconds of a ground-truth epoch is its own
# solution there; an older one is a held (gap-filled) value.
GSDC_MATCH_TOL = 0.15


def parse_ground_truth(csv_path: str) -> list[tuple[float, float, float, float]]:
    """Parse a GSDC ground_truth.csv into timed position records.

    Args:
        csv_path: Path to ``ground_truth.csv``.

    Returns:
        List of ``(utc_sod, lat_deg, lon_deg, h_ell)`` tuples sorted by
        ``utc_sod`` (UTC seconds-of-day from ``UnixTimeMillis``).
    """
    rows = []
    with open(csv_path, newline="") as fh:
        for r in csv.DictReader(fh):
            try:
                t = int(r["UnixTimeMillis"])
                lat = float(r["LatitudeDegrees"])
                lon = float(r["LongitudeDegrees"])
                h = float(r["AltitudeMeters"])
            except (KeyError, ValueError):
                continue
            rows.append(((t / 1000.0) % 86400.0, lat, lon, h))
    rows.sort(key=lambda x: x[0])
    return rows


def parse_ground_truth_velocity(csv_path: str) -> dict[float, tuple[float, float]]:
    """Parse ground-truth horizontal velocity, keyed by UTC seconds-of-day.

    Provided for the deferred TDCP velocity validation (#165 task 6); the
    position comparison does not use it.

    Returns:
        ``{utc_sod: (speed_mps, bearing_deg)}``.
    """
    out = {}
    with open(csv_path, newline="") as fh:
        for r in csv.DictReader(fh):
            try:
                t = int(r["UnixTimeMillis"])
                speed = float(r["SpeedMps"])
                bearing = float(r["BearingDegrees"])
            except (KeyError, ValueError):
                continue
            out[(t / 1000.0) % 86400.0] = (speed, bearing)
    return out


def gsdc_official(
    ref_rows: list[tuple[float, float, float, float]],
    nmea_rows: list[tuple],
    tol: float = GSDC_MATCH_TOL,
) -> dict | None:
    """Score one trip/device with the GSDC-2023 metric over every truth epoch.

    The competition scored a prediction at every ground-truth epoch, so the
    score must not be computed on matched epochs only (that drops the hard
    stretches where the solver produced nothing).  Gaps are filled the way a
    real-time user would see them: each epoch takes the latest solution at or
    before it (``tol`` allows for NMEA time rounding), and epochs before the
    first solution take the first solution.  The fill rule is this benchmark's
    choice; scoring every truth epoch is the competition's.  The competition
    scored latitude / longitude only, so the error is the horizontal distance
    with the solution put at the truth height (the solution height does not
    enter), and the score is the mean of its 50th and 95th percentiles.

    Args:
        ref_rows: Sorted list from ``parse_ground_truth()``.
        nmea_rows: Sorted list from ``parse_nmea()``.
        tol: Seconds within which a solution counts as the epoch's own.

    Returns:
        Dict with ``score``, ``p50``, ``p95`` (metres), ``n_epochs`` and
        ``filled_rate`` (% of epochs scored with a held or back-filled value),
        or None when there is no solution to fill with.
    """
    if not ref_rows or not nmea_rows:
        return None
    ref, sol = continuous_epochs(ref_rows, nmea_rows)
    err = []
    n_filled = 0
    j = 0
    held_t, held = sol[0]
    for t, r in ref:
        while j < len(sol) and sol[j][0] <= t + tol:
            held_t, held = sol[j]
            j += 1
        if abs(t - held_t) > tol:
            n_filled += 1
        dx = blh2xyz(held[1], held[2], r[3]) - blh2xyz(r[1], r[2], r[3])
        enu = xyz2enu(dx, r[1], r[2])
        err.append(float(np.hypot(enu[0], enu[1])))
    p50 = float(np.percentile(err, 50))
    p95 = float(np.percentile(err, 95))
    return {
        "score": (p50 + p95) / 2.0,
        "p50": p50,
        "p95": p95,
        "n_epochs": len(err),
        "filled_rate": 100.0 * n_filled / len(err),
    }


def _fmt(v: float, unit: str = "m", digits: int = 3) -> str:
    """Format a metric value, showing 'nan' for math.nan."""
    return "nan" if math.isnan(v) else f"{v:.{digits}f} {unit}"


def main() -> int:
    """Compare one NMEA result against one GSDC ground_truth.csv."""
    p = argparse.ArgumentParser(
        description="Compare mrtk NMEA output against a GSDC ground_truth.csv"
    )
    p.add_argument("--ref", required=True, metavar="CSV", help="GSDC ground_truth.csv")
    p.add_argument("result", metavar="NMEA", help="mrtk NMEA output file")
    p.add_argument(
        "--skip-epochs",
        type=int,
        default=0,
        help="initial epochs to skip (convergence exclusion, default 0)",
    )
    p.add_argument(
        "--threshold",
        type=float,
        default=2.0,
        help="2D horizontal error threshold in metres (default 2.0)",
    )
    p.add_argument("--plot", action="store_true", help="generate ENU time-series PNG")
    p.add_argument("--plot-out", default="", help="output path for plot")
    args = p.parse_args()

    if not os.path.isfile(args.ref):
        print(f"FAIL: ground truth not found: {args.ref}", file=sys.stderr)
        return 1
    if not os.path.isfile(args.result):
        print(f"FAIL: result not found: {args.result}", file=sys.stderr)
        return 1

    ref_rows = parse_ground_truth(args.ref)
    nmea_rows = parse_nmea(args.result)
    if not ref_rows:
        print("FAIL: no data in ground_truth.csv", file=sys.stderr)
        return 1
    if not nmea_rows:
        print("FAIL: no GGA sentences in result", file=sys.stderr)
        return 1

    # Every truth epoch is scored, so the official score is independent of
    # matching and --skip-epochs and is printed before either can fail.
    print(f"Reference : {args.ref}")
    print(f"Result    : {args.result}")
    print()
    off = gsdc_official(ref_rows, nmea_rows)
    print(
        f"GSDC score (official, every truth epoch) : {_fmt(off['score'])}  "
        f"(p50 {_fmt(off['p50'])}, p95 {_fmt(off['p95'])}, "
        f"{off['filled_rate']:.1f}% of epochs gap-filled)"
    )
    print()

    pairs = _match_epochs(ref_rows, nmea_rows)
    if not pairs:
        print("FAIL: no matching epochs (check date / time range)", file=sys.stderr)
        return 1

    m = compute_metrics(pairs, args.skip_epochs, threshold_2d=args.threshold)
    if m is None:
        print("FAIL: no usable epochs after skip", file=sys.stderr)
        return 1

    thr = args.threshold
    thr_label = f"<{thr:.1f}m" if thr >= 1.0 else f"<{thr * 100:.0f}cm"
    score = (m["p50_2d_all"] + m["p95_2d_all"]) / 2.0
    ref_usable = max(0, len(ref_rows) - args.skip_epochs)
    coverage = m["n_matched"] / ref_usable * 100.0 if ref_usable else math.nan
    print(f"Reference epochs : {ref_usable}")
    print(f"Matched epochs : {m['n_matched']}")
    print(f"Coverage       : {coverage:.1f}%")
    print(f"{thr_label} rate    : {m['thr_rate']:.1f}%")
    print(f"mean nSV       : {m['mean_sv_all']:.1f}")
    print()
    print("  2D horizontal error (all epochs):")
    print(f"    p50    : {_fmt(m['p50_2d_all'])}")
    print(f"    p95    : {_fmt(m['p95_2d_all'])}")
    print(f"    RMS    : {_fmt(m['rms_2d_all'])}")
    print(f"    Max    : {_fmt(m['max_2d_all'])}")
    print()
    print(f"  GSDC score (matched epochs only) : {_fmt(score)}")
    print()
    print("  ENU RMS (all epochs):")
    print(f"    East/North/Up : {m['rms_e']:.3f} / {m['rms_n']:.3f} / {m['rms_u']:.3f} m")

    if args.plot:
        out = args.plot_out or (os.path.splitext(args.result)[0] + ".png")
        plot_results(m, title=os.path.basename(args.result), output_path=out)

    return 0


if __name__ == "__main__":
    sys.exit(main())
