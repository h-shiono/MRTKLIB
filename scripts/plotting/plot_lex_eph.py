#!/usr/bin/env python3
"""Plot LEX ephemeris and clock errors.

Reads a whitespace-delimited ephemeris difference file (e.g. ``diffeph.out``)
and produces two figures:

1. Time series of radial, along-track, cross-track, and clock errors.
2. Bar chart of per-satellite RMS errors.

Ported from MATLAB ``plotlexeph.m`` (2010/12/09 0.1).

Usage::

    python plot_lex_eph.py diffeph.out --sats 1 2 3 --trange 0 24
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def load_data(filepath: str | Path) -> np.ndarray:
    """Load whitespace-delimited numeric data from *filepath*.

    Args:
        filepath: Path to the input data file.

    Returns:
        2-D numpy array with one row per data record.
    """
    return np.loadtxt(filepath)


def plot_lex_eph(
    filepath: str | Path,
    sats: list[int] | None = None,
    trange: tuple[float, float] = (0.0, 24.0),
) -> None:
    """Plot LEX ephemeris / clock error time series and per-satellite bar chart.

    Args:
        filepath: Path to the ephemeris difference file.
        sats: List of satellite PRN numbers to process (default 1..32).
        trange: Time range ``(t_start_hr, t_end_hr)`` for display and RMS
            computation.
    """
    if sats is None:
        sats = list(range(1, 33))

    v = load_data(filepath)

    # --- reference clock: PRN 2 ---
    ref_prn = 2
    mask_ref = v[:, 2] == ref_prn
    day_offset = np.floor(v[0, 1] / 86400.0) * 86400.0
    tclk = v[mask_ref, 1] - day_offset
    clk0 = v[mask_ref, 4]  # column 5 in MATLAB (1-indexed) -> column 4

    sat_labels: list[str] = []
    rms_errors: list[np.ndarray] = []
    y_range = 3.0

    header = (
        f"{'SAT':5s}: {'NE':>4s} {'NC':>4s} "
        f"{'3D':>11s} {'Radial':>11s} {'AlongTrk':>11s} "
        f"{'CrossTrk':>11s} {'Clock':>11s} (m)"
    )
    print(header)

    # --- Figure 1: time series ---
    fig1, ax1 = plt.subplots(figsize=(10, 5))
    ax1.set_position([0.08, 0.1, 0.86, 0.8])

    for sat in sats:
        mask_sat = v[:, 2] == sat
        if not np.any(mask_sat):
            continue

        timep = v[mask_sat, 1] - day_offset
        # 3-D position error = sqrt(radial^2 + along^2 + cross^2)
        pos_components = v[mask_sat, 3:6]  # columns 4-6 (MATLAB) -> 3:6
        pos3d = np.sqrt(np.sum(pos_components**2, axis=1))
        poserr = np.column_stack([pos3d, pos_components])

        # clock error relative to reference PRN
        common_times, idx_p, idx_c = np.intersect1d(timep, tclk, return_indices=True)
        clkerr = v[mask_sat, 4][idx_p] - clk0[idx_c]

        # apply time range filter
        mask_t_pos = (trange[0] * 3600 <= timep) & (timep < trange[1] * 3600)
        mask_t_clk = (trange[0] * 3600 <= common_times) & (
            common_times < trange[1] * 3600
        )

        timep = timep[mask_t_pos]
        poserr = poserr[mask_t_pos, :]
        common_times = common_times[mask_t_clk]
        clkerr = clkerr[mask_t_clk]

        # plot radial, along-track, cross-track
        ax1.plot(timep / 3600.0, poserr[:, 1], "-")  # radial
        ax1.plot(timep / 3600.0, poserr[:, 2], "-")  # along-track
        ax1.plot(timep / 3600.0, poserr[:, 3], "-")  # cross-track
        ax1.plot(common_times / 3600.0, clkerr, "c-")  # clock

        sat_labels.append(f"GPS{sat:02d}")
        rms_pos = np.sqrt(np.mean(poserr**2, axis=0))  # [3D, R, A, C]
        rms_clk = np.sqrt(np.mean(clkerr**2)) if clkerr.size > 0 else 0.0
        rms_row = np.append(rms_pos, rms_clk)
        rms_errors.append(rms_row)

        print(
            f"GPS{sat:02d}: {len(timep):4d} {len(common_times):4d} "
            f"{rms_row[0]:11.4f} {rms_row[1]:11.4f} {rms_row[2]:11.4f} "
            f"{rms_row[3]:11.4f} {rms_row[4]:11.4f} m"
        )

    rms_all = np.array(rms_errors) if rms_errors else np.zeros((1, 5))
    rms_mean = np.sqrt(np.mean(rms_all**2, axis=0))

    ax1.set_xlim(trange)
    ax1.set_ylim(-y_range, y_range)
    xticks = np.arange(trange[0], trange[1] + 1, 3)
    ax1.set_xticks(xticks)
    ax1.set_xlabel("Time (hr)")
    ax1.set_ylabel("Error (m)")
    ax1.set_title(f"Ephemeris/Clock Error : {filepath}")
    ax1.legend(["Radial", "AlongTrk", "CrossTrk", "Clock"])
    ax1.grid(True)
    ax1.text(
        0.02,
        0.98,
        (
            f"RMS 3D:{rms_mean[0]:6.3f}m R:{rms_mean[1]:6.3f}m,"
            f"A:{rms_mean[2]:6.3f}m,C:{rms_mean[3]:6.3f}m,"
            f"CLK:{rms_mean[4]:6.3f}m"
        ),
        transform=ax1.transAxes,
        ha="left",
        va="top",
        fontfamily="serif",
    )

    # --- Figure 2: bar chart ---
    fig2, ax2 = plt.subplots(figsize=(10, 5))
    ax2.set_position([0.08, 0.12, 0.89, 0.81])

    if rms_errors:
        x = np.arange(len(sat_labels))
        width = 0.15
        labels_bar = ["3D", "Radial", "Along-Track", "Cross-Track", "Clock"]
        for col_idx, label in enumerate(labels_bar):
            ax2.bar(x + col_idx * width, rms_all[:, col_idx], width, label=label)
        ax2.set_xticks(x + width * 2)
        ax2.set_xticklabels(sat_labels, rotation=45, ha="right", fontsize=7)
        ax2.set_ylim(0, y_range)

    ax2.legend()
    ax2.set_ylabel("RMS Error (m)")
    ax2.set_title(f"Ephemeris/Clock Error: {filepath}")
    ax2.text(
        0.02,
        0.98,
        (
            f"RMS 3D:{rms_mean[0]:6.3f}m R:{rms_mean[1]:6.3f}m,"
            f"A:{rms_mean[2]:6.3f}m,C:{rms_mean[3]:6.3f}m,"
            f"CLK:{rms_mean[4]:6.3f}m"
        ),
        transform=ax2.transAxes,
        ha="left",
        va="top",
        fontfamily="serif",
    )

    plt.show()


def main(argv: list[str] | None = None) -> None:
    """Entry point for CLI invocation.

    Args:
        argv: Command-line arguments (default: ``sys.argv[1:]``).
    """
    parser = argparse.ArgumentParser(
        description="Plot LEX ephemeris and clock errors.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "file",
        nargs="?",
        default="diffeph.out",
        help="Input ephemeris difference file (default: diffeph.out).",
    )
    parser.add_argument(
        "--sats",
        nargs="+",
        type=int,
        default=None,
        help="Satellite PRN numbers to plot (default: 1..32).",
    )
    parser.add_argument(
        "--trange",
        nargs=2,
        type=float,
        default=[0.0, 24.0],
        metavar=("START", "END"),
        help="Time range in hours (default: 0 24).",
    )
    args = parser.parse_args(argv)
    plot_lex_eph(args.file, sats=args.sats, trange=tuple(args.trange))


if __name__ == "__main__":
    main()
