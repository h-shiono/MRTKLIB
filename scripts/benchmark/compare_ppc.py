"""compare_ppc.py - Compare rnx2rtkp NMEA output against PPC-Dataset reference.csv.

Reads rnx2rtkp NMEA GGA output and the PPC-Dataset ground-truth reference.csv,
matches epochs by UTC time-of-day, computes ENU positioning errors, and reports
2D/3D RMS, fix rate, and convergence time.

Usage
-----
    compare_ppc.py --ref <reference.csv> <result.nmea>
                   [--skip-epochs N] [--plot] [--plot-out FILE]

Output
------
    Per-epoch statistics printed to stdout.  Exit code 0.
"""

import argparse
import bisect
import math
import os
import sys
from pathlib import Path

import numpy as np

# ---------------------------------------------------------------------------
# Locate scripts/tests/ to import shared geodetic helpers
# ---------------------------------------------------------------------------
_TESTS_DIR = Path(__file__).resolve().parent.parent / "tests"
sys.path.insert(0, str(_TESTS_DIR))
from _geo import blh2xyz, nmea_to_deg, xyz2enu  # noqa: E402
from cases import GPS_LEAP  # noqa: E402

# ---------------------------------------------------------------------------
# Parsers
# ---------------------------------------------------------------------------
_GGA_TAGS = ("$GPGGA", "$GNGGA")


def parse_reference(csv_path: str) -> list[tuple[float, float, float, float]]:
    """Parse PPC-Dataset reference.csv into a list of timed position records.

    Each record holds ``(utc_sod, lat_deg, lon_deg, h_ell)`` where
    ``utc_sod`` is UTC seconds-of-day derived from the GPS TOW + Week fields.

    Args:
        csv_path: Path to ``reference.csv``.

    Returns:
        List of ``(utc_sod, lat_deg, lon_deg, h_ell)`` tuples, sorted by
        ``utc_sod``.
    """
    rows = []
    with open(csv_path) as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#") or line.startswith("GPS"):
                continue
            fields = line.split(",")
            if len(fields) < 5:
                continue
            try:
                tow = float(fields[0])
                lat = float(fields[2])
                lon = float(fields[3])
                h = float(fields[4])
            except ValueError:
                continue
            # GPS TOW → UTC seconds-of-day
            utc_sod = (tow - GPS_LEAP) % 86400.0
            rows.append((utc_sod, lat, lon, h))
    rows.sort(key=lambda r: r[0])
    return rows


def parse_nmea(nmea_path: str) -> list[tuple[float, float, float, float, int, int]]:
    """Parse rnx2rtkp NMEA GGA output into a list of timed position records.

    Each record holds ``(utc_sod, lat_deg, lon_deg, h_ell, quality, n_sv)``
    where ``utc_sod`` is UTC seconds-of-day from the GGA time field (HHMMSS.ss).
    Ellipsoidal height is recovered as ``MSL + geoid_separation``.

    Args:
        nmea_path: Path to the NMEA file.

    Returns:
        List of ``(utc_sod, lat_deg, lon_deg, h_ell, quality, n_sv)`` tuples,
        sorted by ``utc_sod``.
    """
    rows = []
    with open(nmea_path) as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue
            if "*" in line:
                line = line[: line.index("*")]
            fields = line.split(",")
            if len(fields) < 10:
                continue
            if fields[0] not in _GGA_TAGS:
                continue
            try:
                quality = int(fields[6])
                if quality == 0 or not fields[2] or not fields[4]:
                    continue
                # GGA time field: HHMMSS.ss
                t = fields[1]
                hh, mm, ss = int(t[0:2]), int(t[2:4]), float(t[4:])
                utc_sod = hh * 3600.0 + mm * 60.0 + ss

                lat = nmea_to_deg(fields[2], fields[3])
                lon = nmea_to_deg(fields[4], fields[5])
                alt_msl = float(fields[9])
                geoid_sep = 0.0
                if len(fields) > 11 and fields[11]:
                    try:
                        geoid_sep = float(fields[11])
                    except ValueError:
                        pass
                h_ell = alt_msl + geoid_sep
                n_sv = 0
                if len(fields) > 7 and fields[7]:
                    try:
                        n_sv = int(fields[7])
                    except ValueError:
                        pass
                rows.append((utc_sod, lat, lon, h_ell, quality, n_sv))
            except (ValueError, IndexError):
                continue
    rows.sort(key=lambda r: r[0])
    return rows


# ---------------------------------------------------------------------------
# Time matching
# ---------------------------------------------------------------------------
def _match_epochs(
    ref_rows: list[tuple[float, float, float, float]],
    nmea_rows: list[tuple[float, float, float, float, int]],
    tol: float = 0.15,
) -> list[tuple[tuple, tuple]]:
    """Pair reference and NMEA epochs by UTC seconds-of-day.

    Handles runs that start near midnight by correcting |Δt| > 43200 s with ±86400 s.

    Args:
        ref_rows: Sorted list from ``parse_reference()``.
        nmea_rows: Sorted list from ``parse_nmea()``.
        tol: Maximum time difference in seconds to accept a match.

    Returns:
        List of ``(ref_row, nmea_row)`` pairs.
    """
    ref_sods = [r[0] for r in ref_rows]
    pairs = []
    for nrow in nmea_rows:
        ns = nrow[0]
        i = bisect.bisect_left(ref_sods, ns)
        for j in (i - 1, i):
            if 0 <= j < len(ref_rows):
                dt = ns - ref_rows[j][0]
                # Midnight wrap: if |dt| > half day, add/subtract 86400
                if dt > 43200.0:
                    dt -= 86400.0
                elif dt < -43200.0:
                    dt += 86400.0
                if abs(dt) <= tol:
                    pairs.append((ref_rows[j], nrow))
                    break
    return pairs


# ---------------------------------------------------------------------------
# Metrics
# ---------------------------------------------------------------------------
def compute_metrics(
    pairs: list[tuple[tuple, tuple]],
    skip_epochs: int = 0,
    threshold_2d: float = float("nan"),
) -> dict | None:
    """Compute positioning error statistics from matched epoch pairs.

    Args:
        pairs: Matched ``(ref_row, nmea_row)`` pairs from ``_match_epochs()``.
        skip_epochs: Number of initial epochs to discard (convergence exclusion).
        threshold_2d: Horizontal error threshold in metres for threshold-based
            fix rate and convergence time (e.g. 0.30 for PPP modes where Q=4
            is never set).  When ``nan``, only Q=4-based metrics are computed.

    Returns:
        Dict of statistics, or ``None`` if no usable pairs remain.
    """
    pairs = pairs[skip_epochs:]
    if not pairs:
        return None

    # Per-epoch ENU projection: compute ECEF error vector and project to local
    # ENU using the ground-truth lat/lon as the origin for that epoch.
    enu_errors = []
    q_list = []
    sv_list = []
    abs_tows = []

    for ref_row, nrow in pairs:
        r_lat, r_lon, r_h = ref_row[1], ref_row[2], ref_row[3]
        n_lat, n_lon, n_h = nrow[1], nrow[2], nrow[3]
        q = nrow[4]
        n_sv = nrow[5] if len(nrow) > 5 else 0

        true_xyz = blh2xyz(r_lat, r_lon, r_h)
        test_xyz = blh2xyz(n_lat, n_lon, n_h)
        dx = test_xyz - true_xyz
        enu = xyz2enu(dx, r_lat, r_lon)
        enu_errors.append(enu)
        q_list.append(q)
        sv_list.append(n_sv)
        abs_tows.append(ref_row[0])  # utc_sod for convergence calculation

    en = np.array(enu_errors)
    sv = np.array(sv_list, dtype=float)
    n = len(en)

    horiz = np.sqrt(en[:, 0] ** 2 + en[:, 1] ** 2)
    e3d = np.sqrt(en[:, 0] ** 2 + en[:, 1] ** 2 + en[:, 2] ** 2)

    fix_mask = np.array([q == 4 for q in q_list])
    n_fix = int(fix_mask.sum())

    # Mean satellite count per tier
    mean_sv_fix = float(np.mean(sv[fix_mask])) if n_fix > 0 else math.nan

    # Fix-only subset (Q=4)
    if n_fix > 0:
        h_fix = horiz[fix_mask]
        e_fix = e3d[fix_mask]
        rms_2d_fix = float(np.sqrt(np.mean(h_fix**2)))
        rms_3d_fix = float(np.sqrt(np.mean(e_fix**2)))
        p68_2d_fix = float(np.percentile(h_fix, 68))
        p95_2d_fix = float(np.percentile(h_fix, 95))
    else:
        rms_2d_fix = rms_3d_fix = p68_2d_fix = p95_2d_fix = math.nan

    # Fix+Float subset: Q=4 (fix), Q=5 (RTK float), Q=3 (PPP float/MADOCA)
    # Excludes Q=1 (SPP fallback) and Q=2 (DGPS).
    ff_mask = np.array([q in (3, 4, 5) for q in q_list])
    n_ff = int(ff_mask.sum())
    mean_sv_ff = float(np.mean(sv[ff_mask])) if n_ff > 0 else math.nan
    mean_sv_all = float(np.mean(sv))
    if n_ff > 0:
        h_ff = horiz[ff_mask]
        e_ff = e3d[ff_mask]
        rms_2d_ff = float(np.sqrt(np.mean(h_ff**2)))
        rms_3d_ff = float(np.sqrt(np.mean(e_ff**2)))
        p68_2d_ff = float(np.percentile(h_ff, 68))
        p95_2d_ff = float(np.percentile(h_ff, 95))
    else:
        rms_2d_ff = rms_3d_ff = p68_2d_ff = p95_2d_ff = math.nan

    # Q=4-based convergence: first run of ≥30 consecutive Q=4 epochs
    conv_time_s = math.nan
    run = 0
    run_start_idx = 0
    for i, q in enumerate(q_list):
        if q == 4:
            if run == 0:
                run_start_idx = i
            run += 1
            if run >= 30:
                conv_time_s = abs_tows[run_start_idx] - abs_tows[0]
                break
        else:
            run = 0

    # Threshold-based metrics (for PPP modes where Q=4 is not produced)
    thr_rate = math.nan
    conv_thr_s = math.nan
    if not math.isnan(threshold_2d):
        thr_mask = horiz < threshold_2d
        n_thr = int(thr_mask.sum())
        thr_rate = n_thr / n * 100.0
        # TTFF: first run of ≥30 consecutive epochs below threshold
        run = 0
        run_start_idx = 0
        for i, below in enumerate(thr_mask):
            if below:
                if run == 0:
                    run_start_idx = i
                run += 1
                if run >= 30:
                    conv_thr_s = abs_tows[run_start_idx] - abs_tows[0]
                    break
            else:
                run = 0

    return {
        "n_matched": n,
        # Q=4 fix
        "n_fix": n_fix,
        "fix_rate": n_fix / n * 100.0,
        "mean_sv_fix": mean_sv_fix,
        "rms_2d_fix": rms_2d_fix,
        "rms_3d_fix": rms_3d_fix,
        "p68_2d_fix": p68_2d_fix,
        "p95_2d_fix": p95_2d_fix,
        "conv_time_s": conv_time_s,
        # Q=4 or Q=5 (fix + float)
        "n_ff": n_ff,
        "ff_rate": n_ff / n * 100.0,
        "mean_sv_ff": mean_sv_ff,
        "rms_2d_ff": rms_2d_ff,
        "rms_3d_ff": rms_3d_ff,
        "p68_2d_ff": p68_2d_ff,
        "p95_2d_ff": p95_2d_ff,
        # All epochs
        "mean_sv_all": mean_sv_all,
        "rms_2d_all": float(np.sqrt(np.mean(horiz**2))),
        "rms_3d_all": float(np.sqrt(np.mean(e3d**2))),
        "p68_2d_all": float(np.percentile(horiz, 68)),
        "p95_2d_all": float(np.percentile(horiz, 95)),
        "max_2d_all": float(np.max(horiz)),
        "rms_e": float(np.sqrt(np.mean(en[:, 0] ** 2))),
        "rms_n": float(np.sqrt(np.mean(en[:, 1] ** 2))),
        "rms_u": float(np.sqrt(np.mean(en[:, 2] ** 2))),
        # Raw arrays (for plotting)
        "enu_errors": en,
        "q_list": q_list,
        "abs_tows": abs_tows,
        # Threshold-based (for PPP modes)
        "threshold_2d": threshold_2d,
        "thr_rate": thr_rate,
        "conv_thr_s": conv_thr_s,
    }


# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------
def plot_results(m: dict, title: str = "", output_path: str = "ppc_compare.png") -> None:
    """Generate ENU time-series and Q-flag plot.

    Args:
        m: Metrics dict from ``compute_metrics()``.
        title: Plot title prefix.
        output_path: Output PNG file path.
    """
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    en = m["enu_errors"] * 100  # m → cm
    idx = np.arange(m["n_matched"])

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
    ax1.plot(idx, en[:, 0], label="East", alpha=0.8, linewidth=0.6)
    ax1.plot(idx, en[:, 1], label="North", alpha=0.8, linewidth=0.6)
    ax1.plot(idx, en[:, 2], label="Up", alpha=0.5, linewidth=0.6, linestyle="--")
    ax1.axhline(0, color="k", linewidth=0.4)
    ax1.set_ylabel("Position error [cm]")
    ax1.set_title(
        f"{title}  2D RMS {m['rms_2d_all']*100:.2f} cm  |  "
        f"Fix {m['fix_rate']:.1f}%  |  "
        f"2D RMS(fix) {m['rms_2d_fix']*100:.2f} cm"
        if not math.isnan(m["rms_2d_fix"])
        else f"{title}  2D RMS {m['rms_2d_all']*100:.2f} cm  |  No fix"
    )
    ax1.legend(loc="upper right")
    ax1.grid(True, alpha=0.3)

    ax2.scatter(idx, m["q_list"], s=4, alpha=0.5)
    ax2.set_ylabel("GGA quality")
    ax2.set_xlabel("Matched epoch")
    ax2.set_title("GGA quality  (Q=4: fix,  Q=5: float,  Q=1: SPP)")
    ax2.set_yticks([1, 2, 4, 5])
    ax2.set_yticklabels(["1:SPP", "2:DGPS", "4:Fix", "5:Float"])
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"  Plot saved: {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def _fmt(v: float, unit: str = "m", digits: int = 3) -> str:
    """Format a metric value, showing 'nan' for math.nan."""
    return "nan" if math.isnan(v) else f"{v:.{digits}f} {unit}"


def main() -> int:
    """Run standalone comparison of one NMEA file against one reference.csv."""
    p = argparse.ArgumentParser(
        description="Compare rnx2rtkp NMEA output against PPC-Dataset reference.csv"
    )
    p.add_argument("--ref", required=True, metavar="CSV",
                   help="PPC-Dataset reference.csv")
    p.add_argument("result", metavar="NMEA",
                   help="rnx2rtkp NMEA output file")
    p.add_argument("--skip-epochs", type=int, default=0,
                   help="Initial epochs to skip (convergence exclusion, default: 0)")
    p.add_argument("--plot", action="store_true",
                   help="Generate ENU time-series PNG")
    p.add_argument("--plot-out", default="",
                   help="Output path for plot (default: <result>.png)")
    args = p.parse_args()

    if not os.path.isfile(args.ref):
        print(f"FAIL: reference not found: {args.ref}", file=sys.stderr)
        return 1
    if not os.path.isfile(args.result):
        print(f"FAIL: result not found: {args.result}", file=sys.stderr)
        return 1

    ref_rows = parse_reference(args.ref)
    nmea_rows = parse_nmea(args.result)
    if not ref_rows:
        print("FAIL: no data in reference.csv", file=sys.stderr)
        return 1
    if not nmea_rows:
        print("FAIL: no GGA sentences in result", file=sys.stderr)
        return 1

    pairs = _match_epochs(ref_rows, nmea_rows)
    if not pairs:
        print("FAIL: no matching epochs (check GPS week / time range)", file=sys.stderr)
        return 1

    m = compute_metrics(pairs, args.skip_epochs)
    if m is None:
        print("FAIL: no usable epochs after skip", file=sys.stderr)
        return 1

    print(f"Reference : {args.ref}")
    print(f"Result    : {args.result}")
    if args.skip_epochs:
        print(f"Skip      : {args.skip_epochs} epochs")
    print()
    print(f"Matched epochs : {m['n_matched']}")
    print(f"Fix epochs     : {m['n_fix']}  ({m['fix_rate']:.1f}%)")
    print()
    print("  ENU RMS (all epochs):")
    print(f"    East   : {m['rms_e']*100:8.3f} cm")
    print(f"    North  : {m['rms_n']*100:8.3f} cm")
    print(f"    Up     : {m['rms_u']*100:8.3f} cm")
    print()
    print("  2D horizontal (all epochs):")
    print(f"    RMS    : {_fmt(m['rms_2d_all'], 'm')}")
    print(f"    1σ     : {_fmt(m['p68_2d_all'], 'm')}  (68th pctile)")
    print(f"    95%    : {_fmt(m['p95_2d_all'], 'm')}  (95th pctile)")
    print(f"    Max    : {_fmt(m['max_2d_all'], 'm')}")
    print()
    print("  2D horizontal (Q=4 fix only):")
    print(f"    RMS    : {_fmt(m['rms_2d_fix'], 'm')}")
    print(f"    1σ     : {_fmt(m['p68_2d_fix'], 'm')}")
    print(f"    95%    : {_fmt(m['p95_2d_fix'], 'm')}")
    print()
    conv = m["conv_time_s"]
    conv_str = _fmt(conv, "s", 0) if not math.isnan(conv) else "never (no 30-epoch fix run)"
    print(f"  Convergence: {conv_str}")

    if args.plot:
        out = args.plot_out or (os.path.splitext(args.result)[0] + ".png")
        plot_results(m, title=os.path.basename(args.result), output_path=out)

    return 0


if __name__ == "__main__":
    sys.exit(main())
