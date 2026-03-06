#!/usr/bin/env python3
"""check_pos_scatter.py - Position scatter (precision) check for a static receiver.

Evaluates the spread of position solutions around the session centroid.
No external reference coordinate is required — the centroid is computed
from the solutions themselves.

This is a Tier 3 cross-validation test: it runs a different correction
algorithm on existing observation data and checks that the position scatter
meets a reasonable precision threshold.

Supported input formats (auto-detected from file extension):
  .pos  — RTKLIB .pos file (lat/lon/h/Q per epoch)
  .nmea — NMEA GGA sentences (field[9] = MSL, field[11] = geoid separation)

Usage
-----
    check_pos_scatter.py [options] test.pos
    check_pos_scatter.py [options] test.nmea

Options
-------
    --tolerance FLOAT   Maximum scatter 1σ and 95% in metres (default 0.200)
    --skip-epochs INT   Skip N initial epochs (convergence transient)
    --use-3d            Evaluate 3D scatter instead of 2D horizontal
    --fix-only          Restrict to fix/PPP epochs (Q=1,4 for NMEA; Q=1,6 for .pos)
    --plot              Save scatter time-series plot
"""

import argparse
import os
import sys

import numpy as np
from _geo import blh2xyz, nmea_to_deg, xyz2blh, xyz2enu  # noqa: E402


# ---------------------------------------------------------------------------
# Parsers
# ---------------------------------------------------------------------------
def _parse_gpst_key(fields):
    return fields[0] + " " + fields[1]


def parse_pos(filepath, fix_only=False):
    """Parse RTKLIB .pos file.

    Returns list of (lat_deg, lon_deg, h, q) tuples in time order.
    """
    rows = []
    with open(filepath) as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("%"):
                continue
            fields = line.split()
            if len(fields) < 6:
                continue
            try:
                lat = float(fields[2])
                lon = float(fields[3])
                h   = float(fields[4])
                q   = int(fields[5])
            except (ValueError, IndexError):
                continue
            if fix_only and q not in (1, 6):
                continue
            rows.append((lat, lon, h, q))
    return rows


def parse_nmea(filepath, fix_only=False):
    """Parse NMEA GGA sentences.

    Recovers ellipsoidal height as field[9] + field[11] (MSL + geoid sep).
    Returns list of (lat_deg, lon_deg, h_ell, q) tuples in time order.
    Also returns geoid_ok flag.
    """
    rows = []
    geoid_ok = False
    with open(filepath) as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue
            if "*" in line:
                line = line[: line.index("*")]
            fields = line.split(",")
            if len(fields) < 10:
                continue
            if fields[0] not in ("$GPGGA", "$GNGGA"):
                continue
            try:
                q = int(fields[6])
                if q == 0 or not fields[2] or not fields[4]:
                    continue
                if fix_only and q not in (1, 4):
                    continue
                lat = nmea_to_deg(fields[2], fields[3])
                lon = nmea_to_deg(fields[4], fields[5])
                alt_msl = float(fields[9])
                geoid_sep = 0.0
                if len(fields) > 11 and fields[11]:
                    try:
                        geoid_sep = float(fields[11])
                    except ValueError:
                        pass
                if geoid_sep != 0.0:
                    geoid_ok = True
                rows.append((lat, lon, alt_msl + geoid_sep, q))
            except (ValueError, IndexError):
                continue
    return rows, geoid_ok


# ---------------------------------------------------------------------------
# Scatter computation
# ---------------------------------------------------------------------------
def compute_scatter(rows, skip_epochs=0):
    """Compute position scatter around the session centroid.

    Args:
        rows: list of (lat_deg, lon_deg, h, q) after quality filtering.
        skip_epochs: discard the first N rows.

    Returns:
        dict of statistics, or None if fewer than 2 usable rows.
    """
    rows = rows[skip_epochs:]
    if len(rows) < 2:
        return None

    # Compute ECEF centroid
    ecef = np.array([blh2xyz(lat, lon, h) for lat, lon, h, _ in rows])
    centroid = np.mean(ecef, axis=0)
    c_lat, c_lon, c_h = xyz2blh(*centroid)

    # Per-epoch deviation from centroid in ENU
    enu_devs = np.array([xyz2enu(e - centroid, c_lat, c_lon) for e in ecef])
    q_list   = [r[3] for r in rows]
    n = len(rows)

    horiz = np.sqrt(enu_devs[:, 0] ** 2 + enu_devs[:, 1] ** 2)
    e3d   = np.sqrt(enu_devs[:, 0] ** 2 + enu_devs[:, 1] ** 2 + enu_devs[:, 2] ** 2)

    return {
        "n":          n,
        "enu_devs":   enu_devs,
        "q_list":     q_list,
        "centroid_lat": c_lat,
        "centroid_lon": c_lon,
        "centroid_h":   c_h,
        # ENU standard deviations
        "std_e":   float(np.std(enu_devs[:, 0])),
        "std_n":   float(np.std(enu_devs[:, 1])),
        "std_u":   float(np.std(enu_devs[:, 2])),
        # 2D horizontal scatter
        "mean_2d": float(np.mean(horiz)),
        "rms_2d":  float(np.sqrt(np.mean(horiz ** 2))),
        "p68_2d":  float(np.percentile(horiz, 68)),
        "p95_2d":  float(np.percentile(horiz, 95)),
        "max_2d":  float(np.max(horiz)),
        # 3D scatter
        "mean_3d": float(np.mean(e3d)),
        "rms_3d":  float(np.sqrt(np.mean(e3d ** 2))),
        "p68_3d":  float(np.percentile(e3d, 68)),
        "p95_3d":  float(np.percentile(e3d, 95)),
        "max_3d":  float(np.max(e3d)),
    }


# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------
def plot_results(m, output_path="scatter_result.png"):
    """Save ENU deviation time-series plot."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    en  = m["enu_devs"] * 100   # m → cm
    idx = np.arange(m["n"])

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    ax1.plot(idx, en[:, 0], label="East",  alpha=0.8, linewidth=0.8)
    ax1.plot(idx, en[:, 1], label="North", alpha=0.8, linewidth=0.8)
    ax1.plot(idx, en[:, 2], label="Up",    alpha=0.5, linewidth=0.8, linestyle="--")
    ax1.axhline(0, color="k", linewidth=0.5)
    ax1.set_ylabel("Deviation from centroid [cm]")
    ax1.set_title(
        f"Position scatter — "
        f"2D RMS {m['rms_2d']*100:.2f} cm  |  "
        f"1σ(2D) {m['p68_2d']*100:.2f} cm  |  "
        f"95%(2D) {m['p95_2d']*100:.2f} cm"
    )
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    ax2.scatter(idx, m["q_list"], s=8, alpha=0.6)
    ax2.set_ylabel("Q flag")
    ax2.set_xlabel("Epoch")
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"  Plot saved: {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main():
    p = argparse.ArgumentParser(
        description="Position scatter (precision) check for a static receiver"
    )
    p.add_argument("test", help=".pos or .nmea file to evaluate")
    p.add_argument("--tolerance", type=float, default=0.200,
                   help="Maximum 1σ and 95%% scatter in metres (default 0.200)")
    p.add_argument("--skip-epochs", type=int, default=0,
                   help="Initial epochs to discard (convergence transient)")
    p.add_argument("--use-3d", action="store_true",
                   help="Evaluate 3D scatter (default: 2D horizontal)")
    p.add_argument("--fix-only", action="store_true",
                   help="Restrict to fix/PPP epochs only")
    p.add_argument("--plot", action="store_true",
                   help="Save scatter time-series plot")
    args = p.parse_args()

    if not os.path.isfile(args.test):
        print(f"FAIL: file not found: {args.test}", file=sys.stderr)
        return 1

    metric_label = "3D" if args.use_3d else "2D horizontal"
    print(f"Test      : {args.test}")
    print(f"Tolerance : {args.tolerance*100:.1f} cm  (evaluated on {metric_label} scatter)")
    if args.skip_epochs:
        print(f"Skip      : {args.skip_epochs} initial epochs")
    if args.fix_only:
        print("Fix-only  : yes")
    print()

    # ── Parse ────────────────────────────────────────────────────────────────
    ext = os.path.splitext(args.test)[1].lower()
    if ext == ".nmea":
        rows, geoid_ok = parse_nmea(args.test, fix_only=args.fix_only)
        if not geoid_ok:
            print("WARNING: GGA field[11] (geoid separation) absent or zero; "
                  "Up scatter may be unreliable.")
        fmt = "NMEA GGA"
    else:
        rows = parse_pos(args.test, fix_only=args.fix_only)
        fmt = "RTKLIB .pos"

    if not rows:
        print("FAIL: no usable epochs in test file", file=sys.stderr)
        return 1

    # ── Compute scatter ──────────────────────────────────────────────────────
    m = compute_scatter(rows, skip_epochs=args.skip_epochs)
    if m is None:
        print("FAIL: fewer than 2 usable epochs after skip", file=sys.stderr)
        return 1

    print(f"Format    : {fmt}")
    print(f"Centroid  : {m['centroid_lat']:.8f}°N  "
          f"{m['centroid_lon']:.8f}°E  {m['centroid_h']:.4f} m")
    print(f"Epochs    : {m['n']}")
    print()
    print("  ENU standard deviation (scatter around centroid):")
    print(f"    East  : {m['std_e']*100:8.3f} cm")
    print(f"    North : {m['std_n']*100:8.3f} cm")
    print(f"    Up    : {m['std_u']*100:8.3f} cm")
    print()
    print("  2D horizontal scatter distribution:")
    print(f"    Mean  : {m['mean_2d']*100:8.3f} cm")
    print(f"    RMS   : {m['rms_2d']*100:8.3f} cm")
    print(f"    1σ    : {m['p68_2d']*100:8.3f} cm  (68th percentile)")
    print(f"    95%   : {m['p95_2d']*100:8.3f} cm  (95th percentile)")
    print(f"    Max   : {m['max_2d']*100:8.3f} cm")
    print()
    print("  3D scatter distribution:")
    print(f"    RMS   : {m['rms_3d']*100:8.3f} cm")
    print(f"    1σ    : {m['p68_3d']*100:8.3f} cm")
    print(f"    95%   : {m['p95_3d']*100:8.3f} cm")
    print()

    if args.plot:
        plot_results(m)

    # ── Pass / Fail ──────────────────────────────────────────────────────────
    if args.use_3d:
        v1s, v95 = m["p68_3d"], m["p95_3d"]
        lbl1s, lbl95 = "1σ  (3D)", "95% (3D)"
    else:
        v1s, v95 = m["p68_2d"], m["p95_2d"]
        lbl1s, lbl95 = "1σ  (2D)", "95% (2D)"

    tol = args.tolerance

    def criterion(label, value):
        ok = value < tol
        tag = "PASS" if ok else "FAIL"
        if ok:
            print(f"{tag} [{label}]: {value*100:.3f} cm  (< tol {tol*100:.1f} cm)")
        else:
            print(f"{tag} [{label}]: {value*100:.3f} cm  (>= tol {tol*100:.1f} cm)")
        return ok

    ok_1s = criterion(lbl1s, v1s)
    ok_95 = criterion(lbl95, v95)

    passed = ok_1s and ok_95
    print()
    print("RESULT: PASS" if passed else "RESULT: FAIL")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
