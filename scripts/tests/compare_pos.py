#!/usr/bin/env python3
"""compare_pos.py - Statistical comparison of RTKLIB .pos files.

Compares a test .pos file against a reference .pos file using ENU error
metrics with configurable tolerance, replacing exact-match comparisons.

Usage:
    python compare_pos.py ref.pos test.pos
    python compare_pos.py ref.pos test.pos --tolerance 0.01
    python compare_pos.py ref.pos test.pos --tolerance 0.005 --plot
    python compare_pos.py ref.pos test.pos --fix-q 1 --max-fix-drop 5.0
"""

import argparse
import sys

import numpy as np
from _geo import blh2xyz, xyz2enu  # noqa: E402


def _parse_gpst_key(fields):
    """Extract a time key from data fields.

    Supports two RTKLIB .pos time formats:
      - Date format:  YYYY/MM/DD HH:MM:SS.sss  (fields[0], fields[1])
      - Week format:  WWWW SSSSSS.sss           (fields[0], fields[1])

    Returns a string key suitable for inner-join matching.
    """
    if "/" in fields[0]:
        # Date format: "2024/08/22 11:00:05.000"
        return fields[0] + " " + fields[1]
    else:
        # GPS week format: "2328 385200.000"
        return fields[0] + " " + fields[1]


def parse_pos(filepath):
    """Parse an RTKLIB .pos file.

    Args:
        filepath: Path to the .pos file.

    Returns:
        Mapping from time-key string to (lat, lon, height, Q) tuple.
    """
    data = {}
    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("%"):
                continue
            fields = line.split()
            if len(fields) < 6:
                continue

            key = _parse_gpst_key(fields)

            # Determine column offsets based on time format
            if "/" in fields[0]:
                # Date format: YYYY/MM/DD HH:MM:SS.sss lat lon h Q ns ...
                lat = float(fields[2])
                lon = float(fields[3])
                h = float(fields[4])
                q = int(fields[5])
            else:
                # Week format: WWWW SSSSSS.sss lat lon h Q ns ...
                lat = float(fields[2])
                lon = float(fields[3])
                h = float(fields[4])
                q = int(fields[5])

            data[key] = (lat, lon, h, q)
    return data


def compute_metrics(ref_data, test_data, skip_epochs=0, fix_q=(1, 6)):
    """Compute ENU error metrics between matched epochs.

    Args:
        ref_data: Parsed .pos data from parse_pos() for the reference file.
        test_data: Parsed .pos data from parse_pos() for the test file.
        skip_epochs: Number of initial epochs to skip (convergence transient).
        fix_q: Q values counted towards the fix rate.

    Returns:
        Dict of metrics (ENU errors, 3D RMS, fix rates, etc.), or None if
        there are no common epochs.
    """
    common_keys = sorted(set(ref_data.keys()) & set(test_data.keys()))
    if skip_epochs > 0:
        common_keys = common_keys[skip_epochs:]
    if not common_keys:
        return None

    enu_errors = []
    errors_3d = []
    ref_q_list = []
    test_q_list = []
    time_keys = []

    for key in common_keys:
        rlat, rlon, rh, rq = ref_data[key]
        tlat, tlon, th, tq = test_data[key]

        ref_xyz = blh2xyz(rlat, rlon, rh)
        test_xyz = blh2xyz(tlat, tlon, th)
        dx = test_xyz - ref_xyz
        enu = xyz2enu(dx, rlat, rlon)

        enu_errors.append(enu)
        errors_3d.append(np.linalg.norm(enu))
        ref_q_list.append(rq)
        test_q_list.append(tq)
        time_keys.append(key)

    enu_errors = np.array(enu_errors)
    errors_3d = np.array(errors_3d)

    n = len(common_keys)
    rms_3d = np.sqrt(np.mean(errors_3d**2))
    max_3d = np.max(errors_3d)

    rms_e = np.sqrt(np.mean(enu_errors[:, 0] ** 2))
    rms_n = np.sqrt(np.mean(enu_errors[:, 1] ** 2))
    rms_u = np.sqrt(np.mean(enu_errors[:, 2] ** 2))

    ref_fix = sum(1 for q in ref_q_list if q in fix_q)
    test_fix = sum(1 for q in test_q_list if q in fix_q)
    ref_fix_rate = ref_fix / n * 100.0 if n > 0 else 0.0
    test_fix_rate = test_fix / n * 100.0 if n > 0 else 0.0

    return {
        "n_common": n,
        "n_ref_total": len(ref_data),
        "n_test_total": len(test_data),
        "rms_3d": rms_3d,
        "max_3d": max_3d,
        "rms_e": rms_e,
        "rms_n": rms_n,
        "rms_u": rms_u,
        "ref_fix_rate": ref_fix_rate,
        "test_fix_rate": test_fix_rate,
        "time_keys": time_keys,
        "enu_errors": enu_errors,
        "ref_q": ref_q_list,
        "test_q": test_q_list,
    }


def plot_results(metrics, output_path="compare_result.png"):
    """Generate ENU error time-series and Q-flag comparison plot.

    Parameters
    ----------
    metrics : dict
        Output from compute_metrics().
    output_path : str
        Output PNG file path.
    """
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    n = metrics["n_common"]
    epochs = np.arange(n)
    enu = metrics["enu_errors"]

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 7), sharex=True)

    # ENU error time series
    ax1.plot(epochs, enu[:, 0] * 100, label="East", alpha=0.8, linewidth=0.8)
    ax1.plot(epochs, enu[:, 1] * 100, label="North", alpha=0.8, linewidth=0.8)
    ax1.plot(epochs, enu[:, 2] * 100, label="Up", alpha=0.8, linewidth=0.8)
    ax1.set_ylabel("ENU Error [cm]")
    ax1.set_title(
        f"ENU Error (3D RMS: {metrics['rms_3d'] * 100:.2f} cm, "
        f"Max: {metrics['max_3d'] * 100:.2f} cm)"
    )
    ax1.legend(loc="upper right")
    ax1.grid(True, alpha=0.3)
    ax1.axhline(y=0, color="k", linewidth=0.5)

    # Q-flag comparison
    ax2.scatter(epochs, metrics["ref_q"], s=8, label="Reference", alpha=0.6, marker="o")
    ax2.scatter(epochs, metrics["test_q"], s=8, label="Test", alpha=0.6, marker="x")
    ax2.set_ylabel("Q flag")
    ax2.set_xlabel("Epoch")
    ax2.set_title(
        f"Q Flag (Ref fix rate: {metrics['ref_fix_rate']:.1f}%, "
        f"Test fix rate: {metrics['test_fix_rate']:.1f}%)"
    )
    ax2.legend(loc="upper right")
    ax2.grid(True, alpha=0.3)
    ax2.set_yticks([1, 2, 3, 4, 5, 6])
    ax2.set_yticklabels(["1:Fix", "2:Float", "3:SBAS", "4:DGPS", "5:Single", "6:PPP"])

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"  Plot saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Statistical comparison of RTKLIB .pos files")
    parser.add_argument("ref", help="Reference .pos file path")
    parser.add_argument("test", help="Test .pos file path")
    parser.add_argument(
        "--tolerance",
        type=float,
        default=0.005,
        help="Maximum allowed 3D RMS error in metres (default: 0.005)",
    )
    parser.add_argument(
        "--skip-epochs",
        type=int,
        default=0,
        help="Number of initial epochs to skip for convergence transient",
    )
    parser.add_argument(
        "--fix-q",
        default="1,6",
        help="Comma-separated Q values counted as fixed (default: 1,6 = any valid "
        "PPP solution; use 1 to measure PPP-AR integer fixes)",
    )
    parser.add_argument(
        "--max-fix-drop",
        type=float,
        default=1.0,
        help="Maximum allowed fix-rate drop vs reference in percent (default: 1.0)",
    )
    parser.add_argument(
        "--plot", action="store_true", help="Generate comparison plot (compare_result.png)"
    )
    args = parser.parse_args()
    fix_q = tuple(int(q) for q in args.fix_q.split(","))

    # Parse files
    print(f"Reference : {args.ref}")
    print(f"Test      : {args.test}")
    print(f"Tolerance : {args.tolerance:.4f} m")
    if args.skip_epochs > 0:
        print(f"Skip      : {args.skip_epochs} initial epochs")
    print()

    import os

    if not os.path.isfile(args.ref):
        print(f"FAIL: Reference file not found: {args.ref}", file=sys.stderr)
        return 1
    if not os.path.isfile(args.test):
        print(f"FAIL: Test file not found: {args.test}", file=sys.stderr)
        return 1

    ref_data = parse_pos(args.ref)
    test_data = parse_pos(args.test)

    if not ref_data:
        print("FAIL: Reference file has no data lines", file=sys.stderr)
        return 1
    if not test_data:
        print("FAIL: Test file has no data lines", file=sys.stderr)
        return 1

    # Compute metrics
    metrics = compute_metrics(ref_data, test_data, skip_epochs=args.skip_epochs, fix_q=fix_q)
    if metrics is None:
        print("FAIL: No common epochs between reference and test", file=sys.stderr)
        return 1

    # Report
    print(
        f"Epochs    : {metrics['n_common']} common "
        f"(ref={metrics['n_ref_total']}, test={metrics['n_test_total']})"
    )
    print()
    print("  ENU RMS Error:")
    print(f"    East  : {metrics['rms_e'] * 100:8.3f} cm")
    print(f"    North : {metrics['rms_n'] * 100:8.3f} cm")
    print(f"    Up    : {metrics['rms_u'] * 100:8.3f} cm")
    print(f"    3D    : {metrics['rms_3d'] * 100:8.3f} cm")
    print(f"    3D Max: {metrics['max_3d'] * 100:8.3f} cm")
    print()
    print(f"  Fix Rate (Q in {{{args.fix_q}}}):")
    print(f"    Reference : {metrics['ref_fix_rate']:6.2f}%")
    print(f"    Test      : {metrics['test_fix_rate']:6.2f}%")
    fix_delta = metrics["test_fix_rate"] - metrics["ref_fix_rate"]
    print(f"    Delta     : {fix_delta:+6.2f}%")
    print()

    # Generate plot if requested
    if args.plot:
        plot_results(metrics)

    # Pass/Fail judgment
    passed = True

    # Criterion 1: 3D RMS under tolerance
    if metrics["rms_3d"] >= args.tolerance:
        print(f"FAIL: 3D RMS ({metrics['rms_3d']:.6f} m) >= tolerance ({args.tolerance:.6f} m)")
        passed = False
    else:
        print(f"PASS: 3D RMS ({metrics['rms_3d']:.6f} m) < tolerance ({args.tolerance:.6f} m)")

    # Criterion 2: fix rate not degraded beyond --max-fix-drop (one-sided, so
    # an implementation that fixes more often than the reference still passes)
    if fix_delta < -args.max_fix_drop:
        print(f"FAIL: Fix rate degraded by {fix_delta:.2f}% (threshold: -{args.max_fix_drop:.1f}%)")
        passed = False
    else:
        print(f"PASS: Fix rate delta ({fix_delta:+.2f}%) within threshold")

    if passed:
        print("\nRESULT: PASS")
        return 0
    else:
        print("\nRESULT: FAIL")
        return 1


if __name__ == "__main__":
    sys.exit(main())
