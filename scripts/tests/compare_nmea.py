#!/usr/bin/env python3
"""compare_nmea.py - Statistical comparison of NMEA GGA output files.

Compares a test NMEA file against a reference NMEA file using ENU error
metrics computed from GGA sentences, with configurable tolerance.

Usage:
    python compare_nmea.py ref.nmea test.nmea
    python compare_nmea.py ref.nmea test.nmea --tolerance 0.10
    python compare_nmea.py ref.nmea test.nmea --tolerance 0.10 --skip-epochs 60 --plot
"""
import argparse
import math
import sys

import numpy as np


# WGS84 constants
_WGS84_A = 6378137.0            # semi-major axis [m]
_WGS84_F = 1.0 / 298.257223563  # flattening
_WGS84_E2 = _WGS84_F * (2.0 - _WGS84_F)  # first eccentricity squared


def blh2xyz(lat_deg, lon_deg, h):
    """Convert geodetic (lat, lon, height) to ECEF (X, Y, Z)."""
    lat = math.radians(lat_deg)
    lon = math.radians(lon_deg)
    sinlat = math.sin(lat)
    coslat = math.cos(lat)
    sinlon = math.sin(lon)
    coslon = math.cos(lon)

    N = _WGS84_A / math.sqrt(1.0 - _WGS84_E2 * sinlat * sinlat)
    x = (N + h) * coslat * coslon
    y = (N + h) * coslat * sinlon
    z = (N * (1.0 - _WGS84_E2) + h) * sinlat
    return np.array([x, y, z])


def xyz2enu(dx, lat_deg, lon_deg):
    """Convert ECEF difference vector to local ENU at a reference point."""
    lat = math.radians(lat_deg)
    lon = math.radians(lon_deg)
    sinlat = math.sin(lat)
    coslat = math.cos(lat)
    sinlon = math.sin(lon)
    coslon = math.cos(lon)

    e = -sinlon * dx[0] + coslon * dx[1]
    n = -sinlat * coslon * dx[0] - sinlat * sinlon * dx[1] + coslat * dx[2]
    u = coslat * coslon * dx[0] + coslat * sinlon * dx[1] + sinlat * dx[2]
    return np.array([e, n, u])


def _nmea_to_deg(val_str, hemi):
    """Convert NMEA DDmm.mmmm / DDDmm.mmmm to decimal degrees.

    Parameters
    ----------
    val_str : str
        NMEA coordinate string (e.g., "3606.2180069" for lat, "14005.1791604" for lon).
    hemi : str
        Hemisphere indicator ('N', 'S', 'E', 'W').

    Returns
    -------
    float
        Decimal degrees (negative for S/W).
    """
    val = float(val_str)
    # Determine number of degree digits: 2 for lat, 3 for lon
    if hemi in ('N', 'S'):
        deg = int(val / 100)
        minutes = val - deg * 100
    else:
        deg = int(val / 100)
        minutes = val - deg * 100
    result = deg + minutes / 60.0
    if hemi in ('S', 'W'):
        result = -result
    return result


def parse_nmea(filepath):
    """Parse NMEA file, extracting position data from GGA sentences.

    Parameters
    ----------
    filepath : str
        Path to the NMEA file.

    Returns
    -------
    dict
        Mapping from time key (HHMMSS.ss string) to (lat_deg, lon_deg, alt, quality).
    """
    data = {}
    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            # Remove checksum
            if '*' in line:
                line = line[:line.index('*')]

            fields = line.split(',')
            if len(fields) < 10:
                continue

            # Parse GGA sentences
            if fields[0] in ('$GPGGA', '$GNGGA'):
                try:
                    time_str = fields[1]  # HHMMSS.ss
                    lat_str = fields[2]
                    lat_hemi = fields[3]
                    lon_str = fields[4]
                    lon_hemi = fields[5]
                    quality = int(fields[6])
                    alt = float(fields[9])

                    if quality == 0 or not lat_str or not lon_str:
                        continue

                    lat_deg = _nmea_to_deg(lat_str, lat_hemi)
                    lon_deg = _nmea_to_deg(lon_str, lon_hemi)

                    data[time_str] = (lat_deg, lon_deg, alt, quality)
                except (ValueError, IndexError):
                    continue

    return data


def compute_metrics(ref_data, test_data, skip_epochs=0):
    """Compute ENU error metrics between matched epochs.

    Parameters
    ----------
    ref_data, test_data : dict
        Parsed NMEA data from parse_nmea().
    skip_epochs : int
        Number of initial epochs to skip (for convergence transient).

    Returns
    -------
    dict or None
        Metrics including ENU errors, 3D errors, RMS, fix rates, etc.
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
    rms_3d = np.sqrt(np.mean(errors_3d ** 2))
    max_3d = np.max(errors_3d)

    rms_e = np.sqrt(np.mean(enu_errors[:, 0] ** 2))
    rms_n = np.sqrt(np.mean(enu_errors[:, 1] ** 2))
    rms_u = np.sqrt(np.mean(enu_errors[:, 2] ** 2))

    # Fix rate: NMEA quality 4=RTK fix, 5=RTK float (both are "valid" for PPP-RTK)
    # Also accept 1=GPS fix and 6=estimated(dead reckoning) as "fix"
    ref_fix = sum(1 for q in ref_q_list if q in (1, 4))
    test_fix = sum(1 for q in test_q_list if q in (1, 4))
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


def plot_results(metrics, output_path="compare_nmea_result.png"):
    """Generate ENU error time-series and Q-flag comparison plot."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    n = metrics["n_common"]
    epochs = np.arange(n)
    enu = metrics["enu_errors"]

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 7), sharex=True)

    ax1.plot(epochs, enu[:, 0] * 100, label="East", alpha=0.8, linewidth=0.8)
    ax1.plot(epochs, enu[:, 1] * 100, label="North", alpha=0.8, linewidth=0.8)
    ax1.plot(epochs, enu[:, 2] * 100, label="Up", alpha=0.8, linewidth=0.8)
    ax1.set_ylabel("ENU Error [cm]")
    ax1.set_title(
        f"ENU Error (3D RMS: {metrics['rms_3d']*100:.2f} cm, "
        f"Max: {metrics['max_3d']*100:.2f} cm)"
    )
    ax1.legend(loc="upper right")
    ax1.grid(True, alpha=0.3)
    ax1.axhline(y=0, color="k", linewidth=0.5)

    ax2.scatter(epochs, metrics["ref_q"], s=8, label="Reference", alpha=0.6,
                marker="o")
    ax2.scatter(epochs, metrics["test_q"], s=8, label="Test", alpha=0.6,
                marker="x")
    ax2.set_ylabel("NMEA Quality")
    ax2.set_xlabel("Epoch")
    ax2.set_title(
        f"NMEA Quality (Ref fix rate: {metrics['ref_fix_rate']:.1f}%, "
        f"Test fix rate: {metrics['test_fix_rate']:.1f}%)"
    )
    ax2.legend(loc="upper right")
    ax2.grid(True, alpha=0.3)
    ax2.set_yticks([0, 1, 2, 4, 5])
    ax2.set_yticklabels(["0:Invalid", "1:GPS", "2:DGPS", "4:RTK Fix", "5:Float"])

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"  Plot saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Statistical comparison of NMEA GGA output files"
    )
    parser.add_argument("ref", help="Reference NMEA file path")
    parser.add_argument("test", help="Test NMEA file path")
    parser.add_argument(
        "--tolerance", type=float, default=0.10,
        help="Maximum allowed 3D RMS error in metres (default: 0.10)"
    )
    parser.add_argument(
        "--skip-epochs", type=int, default=0,
        help="Number of initial epochs to skip for convergence transient"
    )
    parser.add_argument(
        "--plot", action="store_true",
        help="Generate comparison plot (compare_nmea_result.png)"
    )
    parser.add_argument(
        "--skip-fixrate", action="store_true",
        help="Skip fix rate degradation check (for float-only solutions)"
    )
    args = parser.parse_args()

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

    ref_data = parse_nmea(args.ref)
    test_data = parse_nmea(args.test)

    if not ref_data:
        print("FAIL: Reference file has no GGA data", file=sys.stderr)
        return 1
    if not test_data:
        print("FAIL: Test file has no GGA data", file=sys.stderr)
        return 1

    print(f"Parsed    : ref={len(ref_data)} epochs, test={len(test_data)} epochs")

    # Compute metrics
    metrics = compute_metrics(ref_data, test_data, skip_epochs=args.skip_epochs)
    if metrics is None:
        print("FAIL: No common epochs between reference and test",
              file=sys.stderr)
        return 1

    # Report
    print(f"Matched   : {metrics['n_common']} common epochs")
    print()
    print("  ENU RMS Error:")
    print(f"    East  : {metrics['rms_e']*100:8.3f} cm")
    print(f"    North : {metrics['rms_n']*100:8.3f} cm")
    print(f"    Up    : {metrics['rms_u']*100:8.3f} cm")
    print(f"    3D    : {metrics['rms_3d']*100:8.3f} cm")
    print(f"    3D Max: {metrics['max_3d']*100:8.3f} cm")
    print()
    print("  Fix Rate (Quality=4: RTK Fix):")
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
        print(f"FAIL: 3D RMS ({metrics['rms_3d']:.6f} m) >= "
              f"tolerance ({args.tolerance:.6f} m)")
        passed = False
    else:
        print(f"PASS: 3D RMS ({metrics['rms_3d']:.6f} m) < "
              f"tolerance ({args.tolerance:.6f} m)")

    # Criterion 2: Fix rate not degraded by more than 5.0% (relaxed for PPP-RTK)
    if args.skip_fixrate:
        print("SKIP: Fix rate check disabled (--skip-fixrate)")
    elif fix_delta < -5.0:
        print(f"FAIL: Fix rate degraded by {fix_delta:.2f}% "
              f"(threshold: -5.0%)")
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
