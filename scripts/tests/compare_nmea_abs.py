#!/usr/bin/env python3
"""compare_nmea_abs.py - Absolute accuracy check of NMEA GGA output vs geodetic truth.

Compares a NMEA GGA file against a true reference coordinate derived from
either an IGS SINEX file or a GSI F5 daily coordinate file.  Shares all
reference-parsing and pass/fail logic with compare_pos_abs.py.

Reference derivation and pass/fail criteria are identical to compare_pos_abs.py.
See that script's docstring for details.

Height recovery from GGA
------------------------
NMEA GGA contains two height fields:
  Field 9  — MSL altitude (orthometric height above geoid)  [metres]
  Field 11 — Geoid separation (undulation N)                [metres]

Ellipsoidal height is recovered as:
  h_ell = field[9] + field[11]   (= MSL + N)

MRTKLIB's outnmea_gga() always populates both fields via geoidh(), so 3D
comparison against SINEX/F5 ellipsoidal height is valid.

If field[11] is absent or zero (some third-party receivers omit it), the
script falls back to the reference ellipsoidal height for Up computation and
emits a warning.  In that case 2D horizontal comparison is more reliable.

Usage
-----
    compare_nmea_abs.py --sinex FILE.SNX[.gz] --station CODE [--epoch YYYY/MM/DD]
                        [options] test.nmea
    compare_nmea_abs.py --f5 FILE --date YYYY/MM/DD
                        [options] test.nmea

Options
-------
    --tolerance FLOAT   Tolerance for criterion A in metres (default 0.100)
    --skip-epochs INT   Skip N initial epochs
    --use-3d            Evaluate pass/fail on 3D error instead of 2D horizontal
    --plot              Generate ENU error time-series plot
"""

import argparse
import math
import os
import sys

import numpy as np
from _geo import blh2xyz, nmea_to_deg, xyz2blh, xyz2enu  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from compare_pos_abs import _criterion, parse_f5, parse_sinex, sinex_propagate  # noqa: E402


def parse_nmea(filepath):
    """Parse NMEA GGA sentences.

    Recovers ellipsoidal height from the two GGA height fields:
      field[9]  — MSL altitude (orthometric height)
      field[11] — Geoid separation N
      h_ell     = field[9] + field[11]

    Args:
        filepath: Path to NMEA file.

    Returns:
        Tuple (rows, geoid_ok) where:
          rows     — list of (lat_deg, lon_deg, h_ell, quality) tuples in
                     file order.  h_ell is the ellipsoidal height in metres.
          geoid_ok — True if geoid separation was successfully read from
                     at least one sentence; False if field[11] was absent
                     or zero for all sentences (Up comparison unreliable).
    """
    geoid_ok = False
    rows = []
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
                quality = int(fields[6])
                if quality == 0 or not fields[2] or not fields[4]:
                    continue
                lat = nmea_to_deg(fields[2], fields[3])
                lon = nmea_to_deg(fields[4], fields[5])
                alt_msl = float(fields[9])
                # Recover ellipsoidal height: h_ell = MSL + geoid_separation
                geoid_sep = 0.0
                if len(fields) > 11 and fields[11]:
                    try:
                        geoid_sep = float(fields[11])
                    except ValueError:
                        pass
                if geoid_sep != 0.0:
                    geoid_ok = True
                rows.append((lat, lon, alt_msl + geoid_sep, quality))
            except (ValueError, IndexError):
                continue
    return rows, geoid_ok


# ---------------------------------------------------------------------------
# Absolute accuracy metrics  (horizontal + 3D)
# ---------------------------------------------------------------------------
def compute_abs_metrics(true_xyz, rows, skip_epochs=0):
    """Compute per-epoch position errors against a fixed true coordinate.

    Uses the ellipsoidal height recovered from GGA fields 9+11 directly.
    If parse_nmea() could not read geoid separation (geoid_ok=False), the
    caller should warn the user that Up errors may be unreliable.

    Args:
        true_xyz: np.array([X, Y, Z]) — true ECEF coordinate in metres.
        rows: list of (lat, lon, h_ell, q) tuples from parse_nmea(), in
            file order. Storing epochs as a list avoids silent overwrites
            when HHMMSS.ss timestamps repeat across midnight boundaries.
        skip_epochs: initial epochs to discard.

    Returns:
        dict of statistics, or None if no usable epochs.
    """
    true_lat, true_lon, _true_h = xyz2blh(*true_xyz)
    rows = rows[skip_epochs:]
    if not rows:
        return None

    enu_errors, q_list = [], []
    for lat, lon, h_ell, q in rows:
        test_xyz = blh2xyz(lat, lon, h_ell)
        dx = test_xyz - true_xyz
        enu = xyz2enu(dx, true_lat, true_lon)
        enu_errors.append(enu)
        q_list.append(q)

    en = np.array(enu_errors)
    n = len(en)

    horiz = np.sqrt(en[:, 0] ** 2 + en[:, 1] ** 2)
    e3d   = np.sqrt(en[:, 0] ** 2 + en[:, 1] ** 2 + en[:, 2] ** 2)

    return {
        "n": n,
        "enu_errors": en,
        "q_list": q_list,
        "true_lat": true_lat,
        "true_lon": true_lon,
        # Horizontal (2D)
        "mean_2d":  float(np.mean(horiz)),
        "rms_2d":   float(np.sqrt(np.mean(horiz ** 2))),
        "p68_2d":   float(np.percentile(horiz, 68)),
        "p95_2d":   float(np.percentile(horiz, 95)),
        "max_2d":   float(np.max(horiz)),
        "rms_e":    float(np.sqrt(np.mean(en[:, 0] ** 2))),
        "rms_n":    float(np.sqrt(np.mean(en[:, 1] ** 2))),
        # 3D (valid when geoid separation is present in GGA)
        "mean_3d":  float(np.mean(e3d)),
        "rms_3d":   float(np.sqrt(np.mean(e3d ** 2))),
        "p68_3d":   float(np.percentile(e3d, 68)),
        "p95_3d":   float(np.percentile(e3d, 95)),
        "max_3d":   float(np.max(e3d)),
        "rms_u":    float(np.sqrt(np.mean(en[:, 2] ** 2))),
        "fix_rate": sum(1 for q in q_list if q in (1, 4)) / n * 100.0,
    }


# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------
def plot_results(m, ref_label, output_path="abs_nmea_compare.png"):
    """Generate ENU error time-series and Q-flag plot."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    en = m["enu_errors"] * 100      # m → cm
    idx = np.arange(m["n"])

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    ax1.plot(idx, en[:, 0], label="East",  alpha=0.8, linewidth=0.8)
    ax1.plot(idx, en[:, 1], label="North", alpha=0.8, linewidth=0.8)
    ax1.plot(idx, en[:, 2], label="Up",
             alpha=0.5, linewidth=0.8, linestyle="--")
    ax1.axhline(0, color="k", linewidth=0.5)
    ax1.set_ylabel("Position error [cm]")
    ax1.set_title(
        f"Absolute error vs {ref_label} — "
        f"2D RMS {m['rms_2d']*100:.2f} cm  |  "
        f"1σ(2D) {m['p68_2d']*100:.2f} cm  |  "
        f"95%(2D) {m['p95_2d']*100:.2f} cm"
    )
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    ax2.scatter(idx, m["q_list"], s=8, alpha=0.6)
    ax2.set_ylabel("GGA quality")
    ax2.set_xlabel("Epoch")
    ax2.set_title(f"GGA quality  (fix rate {m['fix_rate']:.1f}%,  Q=1 or Q=4)")
    ax2.set_yticks([1, 2, 4, 5])
    ax2.set_yticklabels(["1:GPS", "2:DGPS", "4:Fix", "5:Float"])
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"  Plot saved: {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main():  # noqa: D103
    p = argparse.ArgumentParser(
        description="Absolute accuracy check of NMEA GGA output vs geodetic truth"
    )
    ref = p.add_mutually_exclusive_group(required=True)
    ref.add_argument("--sinex", metavar="FILE",
                     help="IGS SINEX file (.SNX or .SNX.gz)")
    ref.add_argument("--f5", metavar="FILE",
                     help="GSI F5 daily coordinate file")
    p.add_argument("--station", metavar="CODE",
                   help="4-char station code (required with --sinex)")
    p.add_argument("--date", metavar="YYYY/MM/DD",
                   help="Evaluation date for F5 15-day window (required with --f5)")
    p.add_argument("--epoch", metavar="YYYY/MM/DD",
                   help="Target epoch for SINEX propagation")
    p.add_argument("--tolerance", type=float, default=0.100,
                   help="Tolerance for criterion A in metres (default 0.100)")
    p.add_argument("--skip-epochs", type=int, default=0,
                   help="Initial epochs to discard")
    p.add_argument("--use-3d", action="store_true",
                   help="Evaluate pass/fail on 3D error (default: 2D horizontal)")
    p.add_argument("--plot", action="store_true",
                   help="Generate ENU error time-series plot")
    p.add_argument("test", help="NMEA GGA file to evaluate")
    args = p.parse_args()

    from datetime import datetime

    # ── Resolve true reference coordinate ───────────────────────────────────
    if args.sinex:
        if not args.station:
            print("FAIL: --station is required with --sinex", file=sys.stderr)
            return 1
        if not os.path.isfile(args.sinex):
            print(f"FAIL: SINEX file not found: {args.sinex}", file=sys.stderr)
            return 1
        sinex = parse_sinex(args.sinex, args.station)
        if args.epoch:
            target_dt = datetime.strptime(args.epoch, "%Y/%m/%d")
            true_xyz = sinex_propagate(sinex, target_dt)
            epoch_note = f"propagated to {args.epoch}"
        else:
            true_xyz = sinex["xyz"]
            epoch_note = f"at SINEX ref epoch {sinex['epoch'].strftime('%Y/%m/%d %H:%M')}"
        ref_precision = sinex["sigma_3d"]
        ref_label = f"SINEX/{args.station.upper()}"
        print(f"Reference : {args.sinex}")
        print(f"Station   : {args.station.upper()} ({epoch_note})")
        print(f"Ref prec  : {ref_precision*1000:.2f} mm (SINEX formal 3D σ)")
    else:
        if not args.date:
            print("FAIL: --date is required with --f5", file=sys.stderr)
            return 1
        if not os.path.isfile(args.f5):
            print(f"FAIL: F5 file not found: {args.f5}", file=sys.stderr)
            return 1
        f5 = parse_f5(args.f5, args.date)
        true_xyz = f5["xyz"]
        ref_precision = f5["sigma_3d"]
        ref_label = f"GSI-F5/{args.date}"
        print(f"Reference : {args.f5}")
        print(f"Eval date : {args.date}  (±7-day median, {f5['n_days']} days)")
        print(f"Ref coord : {f5['lat']:.8f}°N  {f5['lon']:.8f}°E  {f5['h']:.4f} m")
        print(f"Ref prec  : {ref_precision*1000:.2f} mm (68th-pctile of F5 daily scatter)")

    # ── Parse test NMEA ──────────────────────────────────────────────────────
    if not os.path.isfile(args.test):
        print(f"FAIL: test file not found: {args.test}", file=sys.stderr)
        return 1

    metric_label = "3D" if args.use_3d else "2D horizontal"
    print(f"Test      : {args.test}")
    print(f"Tolerance : {args.tolerance*100:.1f} cm  (evaluated on {metric_label} error)")
    if args.skip_epochs:
        print(f"Skip      : {args.skip_epochs} initial epochs")
    print()

    test_data, geoid_ok = parse_nmea(args.test)
    if not test_data:
        print("FAIL: no GGA data in test file", file=sys.stderr)
        return 1
    if not geoid_ok:
        if args.use_3d:
            print("FAIL: --use-3d requested but GGA field[11] (geoid separation) is absent",
                  file=sys.stderr)
            print("      or zero in all epochs. Ellipsoidal height cannot be recovered.",
                  file=sys.stderr)
            print("      Re-run without --use-3d to evaluate 2D horizontal accuracy only.",
                  file=sys.stderr)
            return 1
        print("WARNING: GGA field[11] (geoid separation) absent or zero in all epochs.")
        print("         Up errors may be unreliable; 2D horizontal pass/fail is used.")

    # ── Compute metrics ──────────────────────────────────────────────────────
    m = compute_abs_metrics(true_xyz, test_data, args.skip_epochs)
    if m is None:
        print("FAIL: no usable epochs", file=sys.stderr)
        return 1

    tl, tn, th = xyz2blh(*true_xyz)
    print(f"True pos  : {tl:.8f}°N  {tn:.8f}°E  {th:.4f} m (ellipsoidal)")
    print(f"Epochs    : {m['n']}")
    print()
    print("  ENU RMS:")
    print(f"    East   : {m['rms_e']*100:8.3f} cm")
    print(f"    North  : {m['rms_n']*100:8.3f} cm")
    print(f"    Up     : {m['rms_u']*100:8.3f} cm")
    print()
    print("  2D horizontal error distribution:")
    print(f"    Bias   : {m['mean_2d']*100:8.3f} cm  (mean)")
    print(f"    RMS    : {m['rms_2d']*100:8.3f} cm")
    print(f"    1σ     : {m['p68_2d']*100:8.3f} cm  (68th percentile)")
    print(f"    95%    : {m['p95_2d']*100:8.3f} cm  (95th percentile)")
    print(f"    Max    : {m['max_2d']*100:8.3f} cm")
    print()
    print("  3D error distribution:")
    print(f"    RMS    : {m['rms_3d']*100:8.3f} cm")
    print(f"    1σ     : {m['p68_3d']*100:8.3f} cm")
    print(f"    95%    : {m['p95_3d']*100:8.3f} cm")
    print()
    print(f"  Fix rate : {m['fix_rate']:.2f}%  (GGA quality = 1 or 4)")
    print(f"  Ref prec : {ref_precision*100:.3f} cm  ({ref_label})")
    print()

    if args.plot:
        plot_results(m, ref_label)

    # ── Pass / Fail ──────────────────────────────────────────────────────────
    # ref_precision is always a 3D quantity (SINEX σ3D or F5 3D scatter).
    # When evaluating 2D horizontal metrics, scale to horizontal precision
    # assuming isotropic errors: σ_2D = σ_3D * sqrt(2/3).
    if args.use_3d:
        ok_1s = _criterion("1σ  (3D)", m["p68_3d"], args.tolerance, ref_precision)
        ok_95 = _criterion("95% (3D)", m["p95_3d"], args.tolerance, ref_precision)
    else:
        ref_prec_2d = ref_precision * math.sqrt(2.0 / 3.0)
        ok_1s = _criterion("1σ  (2D)", m["p68_2d"], args.tolerance, ref_prec_2d)
        ok_95 = _criterion("95% (2D)", m["p95_2d"], args.tolerance, ref_prec_2d)

    passed = ok_1s and ok_95
    print()
    print("RESULT: PASS" if passed else "RESULT: FAIL")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
