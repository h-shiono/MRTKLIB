#!/usr/bin/env python3
"""compare_pos_abs.py - Absolute accuracy check against geodetic truth.

Compares a RTKLIB .pos file against a true reference coordinate derived from
either an IGS SINEX file (for IGS/CORS stations such as MIZU) or a GSI F5
daily coordinate file (for GEONET stations).

Reference derivation
--------------------
SINEX:
    Station coordinate at the SINEX reference epoch, optionally propagated to
    a target date using velocity estimates (VELX/VELY/VELZ) in the same file.
    Formal 3D uncertainty  =  sqrt(σ_X² + σ_Y² + σ_Z²).

GSI F5:
    Median of daily ECEF coordinates over the 15-day window centred on the
    evaluation date (D−7 … D+7).
    Reference precision  =  68th-percentile of 3D scatter within that window.

Accuracy metrics
----------------
For each epoch in the test .pos file, the 3D position error against the fixed
true coordinate is computed (ECEF difference projected to ENU).

Reported statistics
-------------------
  1σ   — 68th percentile of per-epoch 3D errors  (≈ 1 standard deviation)
  95%  — 95th percentile of per-epoch 3D errors

Pass / Fail
-----------
For each metric (1σ, 95%) the test passes when at least ONE of the following
holds:

  A.  metric < tolerance            (algorithm meets the required accuracy)
  B.  metric < reference precision  (algorithm is at least as good as the truth)

Criterion B acts as a safety valve: when the reference itself is noisy (e.g.
a sparsely sampled F5 file), the test cannot fairly demand better accuracy than
the truth provides.

Overall result is PASS only when both the 1σ criterion and the 95% criterion
individually pass.

Usage
-----
    compare_pos_abs.py --sinex FILE.SNX[.gz] --station CODE [--epoch YYYY/MM/DD]
                       [options] test.pos
    compare_pos_abs.py --f5 FILE --date YYYY/MM/DD
                       [options] test.pos

Options
-------
    --tolerance FLOAT   Tolerance for criterion A in metres (default 0.030)
    --skip-epochs INT   Skip N initial epochs (convergence transient)
    --use-2d            Evaluate pass/fail on 2D horizontal error (default: 3D)
    --plot              Generate ENU error time-series plot

Note on PPP vertical accuracy
------------------------------
PPP Up errors are dominated by tropospheric delay model residuals and are
typically 3–5× larger than horizontal errors.  Use --use-2d for tests where
the purpose is to validate horizontal positioning performance.
"""

import argparse
import gzip
import math
import sys
from datetime import datetime, timedelta

import numpy as np
from _geo import blh2xyz, xyz2blh, xyz2enu  # noqa: E402


# ---------------------------------------------------------------------------
# SINEX parser
# ---------------------------------------------------------------------------
def _sinex_epoch(s):
    """Parse SINEX epoch string 'YY:DOY:SOD' → datetime."""
    yy, doy, sod = (int(t) for t in s.split(":"))
    year = 2000 + yy if yy < 79 else 1900 + yy
    return datetime(year, 1, 1) + timedelta(days=doy - 1, seconds=sod)


def parse_sinex(filepath, station_code):
    """Extract station coordinate and formal sigma from an IGS SINEX file.

    Args:
        filepath: Path to .SNX or .SNX.gz file.
        station_code: 4-character station code (case-insensitive, e.g. 'MIZU').

    Returns:
        dict with keys:
            xyz       – np.array([X, Y, Z]) in metres (ITRF)
            sigma_3d  – formal 3D uncertainty sqrt(σX²+σY²+σZ²) in metres
            epoch     – datetime of reference epoch
            estimates – raw dict mapping type→(value, sigma, epoch)
    """
    code = station_code.upper()[:4]
    opener = gzip.open if str(filepath).endswith(".gz") else open
    estimates = {}
    in_est = False

    with opener(filepath, "rt", encoding="ascii", errors="replace") as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            if line.startswith("+SOLUTION/ESTIMATE"):
                in_est = True
                continue
            if line.startswith("-SOLUTION/ESTIMATE"):
                break
            if not in_est or line.startswith("*"):
                continue
            parts = line.split()
            if len(parts) < 10:
                continue
            # INDEX TYPE CODE PT SOLN REF_EPOCH UNIT S VALUE STDDEV
            ptype, pcode = parts[1], parts[2].upper()
            if pcode != code:
                continue
            epoch = _sinex_epoch(parts[5])
            value, sigma = float(parts[8]), float(parts[9])
            estimates[ptype] = (value, sigma, epoch)

    for req in ("STAX", "STAY", "STAZ"):
        if req not in estimates:
            raise ValueError(f"Station '{code}' not found in SINEX (missing {req})")

    x, sx, epoch = estimates["STAX"]
    y, sy, _     = estimates["STAY"]
    z, sz, _     = estimates["STAZ"]
    sigma_3d = math.sqrt(sx * sx + sy * sy + sz * sz)
    return {
        "xyz": np.array([x, y, z]),
        "sigma_3d": sigma_3d,
        "epoch": epoch,
        "estimates": estimates,
    }


def sinex_propagate(sinex, target_epoch):
    """Apply linear velocity model to propagate SINEX coordinate.

    Uses VELX/VELY/VELZ entries if present; otherwise returns the coordinate
    unchanged (appropriate when target epoch ≈ reference epoch).

    Args:
        sinex: dict returned by parse_sinex().
        target_epoch: datetime to propagate to.

    Returns:
        np.array([X, Y, Z]) propagated to target_epoch.
    """
    xyz = sinex["xyz"].copy()
    dt_yr = (target_epoch - sinex["epoch"]).total_seconds() / (365.25 * 86400)
    for i, vtype in enumerate(("VELX", "VELY", "VELZ")):
        if vtype in sinex["estimates"]:
            xyz[i] += sinex["estimates"][vtype][0] * dt_yr
    return xyz


# ---------------------------------------------------------------------------
# GSI F5 daily coordinate parser
# ---------------------------------------------------------------------------
def parse_f5(filepath, eval_date_str):
    """Compute 15-day median reference coordinate from a GSI F5 file.

    The reference coordinate is the element-wise median of ECEF (X, Y, Z)
    over the window [eval_date − 7 days, eval_date + 7 days].

    Reference precision is the 68th-percentile of the 3D scatter of daily
    values within the window, relative to their median.

    Args:
        filepath: Path to a GSI F5 daily .pos file.
        eval_date_str: Evaluation date as 'YYYY/MM/DD'.

    Returns:
        dict with keys:
            xyz       – np.array([X, Y, Z]) median in metres (ITRF2014/GRS80)
            lat, lon, h – geodetic coordinates of median
            sigma_3d  – reference precision (68th-pctile of 3D scatter) [m]
            n_days    – number of days actually used
    """
    eval_dt = datetime.strptime(eval_date_str, "%Y/%m/%d")
    lo, hi = eval_dt - timedelta(days=7), eval_dt + timedelta(days=7)

    xs, ys, zs = [], [], []
    in_data = False

    with open(filepath, encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if line.startswith("+DATA"):
                in_data = True
                continue
            if line.startswith("-DATA"):
                break
            if not in_data or line.startswith("*") or not line:
                continue
            parts = line.split()
            if len(parts) < 7:
                continue
            try:
                dt = datetime(int(parts[0]), int(parts[1]), int(parts[2]))
            except ValueError:
                continue
            if lo <= dt <= hi:
                xs.append(float(parts[4]))
                ys.append(float(parts[5]))
                zs.append(float(parts[6]))

    if len(xs) < 7:
        raise ValueError(
            f"GSI F5: only {len(xs)} day(s) in ±7-day window around "
            f"{eval_date_str} — need ≥7"
        )

    xs, ys, zs = np.array(xs), np.array(ys), np.array(zs)
    mx, my, mz = float(np.median(xs)), float(np.median(ys)), float(np.median(zs))
    median_xyz = np.array([mx, my, mz])
    lat, lon, h = xyz2blh(mx, my, mz)

    # 3D scatter of daily values relative to their median → reference precision
    scatter = [
        np.linalg.norm(xyz2enu(np.array([xi - mx, yi - my, zi - mz]), lat, lon))
        for xi, yi, zi in zip(xs, ys, zs)
    ]
    sigma_3d = float(np.percentile(scatter, 68))

    return {
        "xyz": median_xyz,
        "lat": lat, "lon": lon, "h": h,
        "sigma_3d": sigma_3d,
        "n_days": len(xs),
    }


# ---------------------------------------------------------------------------
# .pos file parser  (same logic as compare_pos.py)
# ---------------------------------------------------------------------------
def parse_pos(filepath):
    """Parse an RTKLIB .pos file.

    Returns:
        dict mapping time-key string to (lat, lon, h, Q).
    """
    data = {}
    with open(filepath) as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("%"):
                continue
            parts = line.split()
            if len(parts) < 6:
                continue
            key = parts[0] + " " + parts[1]
            data[key] = (float(parts[2]), float(parts[3]),
                         float(parts[4]), int(parts[5]))
    return data


# ---------------------------------------------------------------------------
# Absolute accuracy metrics
# ---------------------------------------------------------------------------
def compute_abs_metrics(true_xyz, test_data, skip_epochs=0):
    """Compute per-epoch 3D errors against a fixed true coordinate.

    Args:
        true_xyz: np.array([X, Y, Z]) — true ECEF coordinate in metres.
        test_data: dict from parse_pos().
        skip_epochs: number of initial epochs to discard.

    Returns:
        dict of statistics, or None if no usable epochs.
    """
    true_lat, true_lon, _ = xyz2blh(*true_xyz)
    epochs = sorted(test_data.keys())[skip_epochs:]
    if not epochs:
        return None

    errors_3d, enu_errors, q_list = [], [], []
    for key in epochs:
        lat, lon, h, q = test_data[key]
        dx = blh2xyz(lat, lon, h) - true_xyz
        enu = xyz2enu(dx, true_lat, true_lon)
        enu_errors.append(enu)
        errors_3d.append(float(np.linalg.norm(enu)))
        q_list.append(q)

    e3 = np.array(errors_3d)
    en = np.array(enu_errors)
    n = len(e3)

    horiz = np.sqrt(en[:, 0] ** 2 + en[:, 1] ** 2)

    return {
        "n": n,
        "errors_3d": e3,
        "enu_errors": en,
        "q_list": q_list,
        "true_lat": true_lat,
        "true_lon": true_lon,
        # ENU components
        "rms_e":    float(np.sqrt(np.mean(en[:, 0] ** 2))),
        "rms_n":    float(np.sqrt(np.mean(en[:, 1] ** 2))),
        "rms_u":    float(np.sqrt(np.mean(en[:, 2] ** 2))),
        # 2D horizontal
        "mean_2d":  float(np.mean(horiz)),
        "rms_2d":   float(np.sqrt(np.mean(horiz ** 2))),
        "p68_2d":   float(np.percentile(horiz, 68)),
        "p95_2d":   float(np.percentile(horiz, 95)),
        "max_2d":   float(np.max(horiz)),
        # 3D
        "mean_3d":  float(np.mean(e3)),
        "rms_3d":   float(np.sqrt(np.mean(e3 ** 2))),
        "p68_3d":   float(np.percentile(e3, 68)),
        "p95_3d":   float(np.percentile(e3, 95)),
        "max_3d":   float(np.max(e3)),
        "fix_rate": sum(1 for q in q_list if q in (1, 6)) / n * 100.0,
    }


# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------
def plot_results(m, ref_label, output_path="abs_compare.png"):
    """Generate ENU error time-series and Q-flag plot."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    en = m["enu_errors"] * 100      # m → cm
    idx = np.arange(m["n"])

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    for col, lbl in zip(range(3), ("East", "North", "Up")):
        ax1.plot(idx, en[:, col], label=lbl, alpha=0.8, linewidth=0.8)
    ax1.axhline(0, color="k", linewidth=0.5)
    ax1.set_ylabel("Position error [cm]")
    ax1.set_title(
        f"Absolute error vs {ref_label} — "
        f"RMS {m['rms_3d']*100:.2f} cm  |  "
        f"1σ {m['p68_3d']*100:.2f} cm  |  "
        f"95% {m['p95_3d']*100:.2f} cm"
    )
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    ax2.scatter(idx, m["q_list"], s=8, alpha=0.6)
    ax2.set_ylabel("Q flag")
    ax2.set_xlabel("Epoch")
    ax2.set_title(f"Q flag  (fix rate {m['fix_rate']:.1f}%)")
    ax2.set_yticks([1, 2, 3, 4, 5, 6])
    ax2.set_yticklabels(["1:Fix", "2:Float", "3:SBAS", "4:DGPS", "5:Single", "6:PPP"])
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"  Plot saved: {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def _criterion(label, value_m, tolerance_m, ref_precision_m):
    """Evaluate and print one pass/fail criterion.

    Passes when  value < tolerance  OR  value < ref_precision.

    Returns True if the criterion passes.
    """
    a = value_m < tolerance_m
    b = value_m < ref_precision_m
    ok = a or b
    tag = "PASS" if ok else "FAIL"
    reasons = []
    if a:
        reasons.append(f"< tol {tolerance_m*100:.1f} cm")
    if b:
        reasons.append(f"< ref prec {ref_precision_m*100:.2f} cm")
    if not reasons:
        reasons = [
            f">= tol {tolerance_m*100:.1f} cm",
            f">= ref prec {ref_precision_m*100:.2f} cm",
        ]
    print(f"{tag} [{label}]: {value_m*100:.3f} cm  ({', '.join(reasons)})")
    return ok


def main():  # noqa: D103
    p = argparse.ArgumentParser(
        description="Absolute accuracy check of RTKLIB .pos vs geodetic truth"
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
                   help="Target epoch for SINEX propagation (default: SINEX ref epoch)")
    p.add_argument("--tolerance", type=float, default=0.030,
                   help="Tolerance for criterion A in metres (default 0.030)")
    p.add_argument("--skip-epochs", type=int, default=0,
                   help="Initial epochs to discard")
    p.add_argument("--use-2d", action="store_true",
                   help="Evaluate pass/fail on 2D horizontal error (default: 3D)")
    p.add_argument("--plot", action="store_true",
                   help="Generate ENU error time-series plot")
    p.add_argument("test", help="RTKLIB .pos file to evaluate")
    args = p.parse_args()

    import os

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

    # ── Parse test .pos ──────────────────────────────────────────────────────
    if not os.path.isfile(args.test):
        print(f"FAIL: test file not found: {args.test}", file=sys.stderr)
        return 1

    metric_label = "2D horizontal" if args.use_2d else "3D"
    print(f"Test      : {args.test}")
    print(f"Tolerance : {args.tolerance*100:.1f} cm  (evaluated on {metric_label} error)")
    if args.skip_epochs:
        print(f"Skip      : {args.skip_epochs} initial epochs")
    print()

    test_data = parse_pos(args.test)
    if not test_data:
        print("FAIL: no data in test file", file=sys.stderr)
        return 1

    # ── Compute metrics ──────────────────────────────────────────────────────
    m = compute_abs_metrics(true_xyz, test_data, args.skip_epochs)
    if m is None:
        print("FAIL: no usable epochs", file=sys.stderr)
        return 1

    tl, tn, th = xyz2blh(*true_xyz)
    print(f"True pos  : {tl:.8f}°N  {tn:.8f}°E  {th:.4f} m")
    print(f"Epochs    : {m['n']}")
    print()
    print("  ENU RMS (absolute):")
    print(f"    East  : {m['rms_e']*100:8.3f} cm")
    print(f"    North : {m['rms_n']*100:8.3f} cm")
    print(f"    Up    : {m['rms_u']*100:8.3f} cm")
    print()
    print("  2D horizontal error distribution:")
    print(f"    Bias  : {m['mean_2d']*100:8.3f} cm  (mean)")
    print(f"    RMS   : {m['rms_2d']*100:8.3f} cm")
    print(f"    1σ    : {m['p68_2d']*100:8.3f} cm  (68th percentile)")
    print(f"    95%   : {m['p95_2d']*100:8.3f} cm  (95th percentile)")
    print(f"    Max   : {m['max_2d']*100:8.3f} cm")
    print()
    print("  3D error distribution (vs geodetic truth):")
    print(f"    Bias  : {m['mean_3d']*100:8.3f} cm  (mean)")
    print(f"    RMS   : {m['rms_3d']*100:8.3f} cm")
    print(f"    1σ    : {m['p68_3d']*100:8.3f} cm  (68th percentile)")
    print(f"    95%   : {m['p95_3d']*100:8.3f} cm  (95th percentile)")
    print(f"    Max   : {m['max_3d']*100:8.3f} cm")
    print()
    print(f"  Fix rate : {m['fix_rate']:.2f}%")
    print(f"  Ref prec : {ref_precision*100:.3f} cm  ({ref_label})")
    print()

    if args.plot:
        plot_results(m, ref_label)

    # ── Pass / Fail ──────────────────────────────────────────────────────────
    # Each metric (1σ, 95%) passes when:
    #   A. metric < tolerance        (meets the required accuracy target)  OR
    #   B. metric < ref_precision    (at least as good as the truth itself)
    if args.use_2d:
        ok_1s = _criterion("1σ  (2D)", m["p68_2d"], args.tolerance, ref_precision)
        ok_95 = _criterion("95% (2D)", m["p95_2d"], args.tolerance, ref_precision)
    else:
        ok_1s = _criterion("1σ  (3D)", m["p68_3d"], args.tolerance, ref_precision)
        ok_95 = _criterion("95% (3D)", m["p95_3d"], args.tolerance, ref_precision)

    passed = ok_1s and ok_95
    print()
    print("RESULT: PASS" if passed else "RESULT: FAIL")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
