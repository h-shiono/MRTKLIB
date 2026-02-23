#!/usr/bin/env python3
"""Plot LEX User Range Error (URE) from ephemeris and clock data.

Reads a whitespace-delimited ephemeris difference file (e.g. ``diffeph.out``)
and produces two figures:

1. Time series of per-satellite URE (mean-removed per epoch, filtered by
   minimum elevation).
2. Bar chart of per-satellite RMS URE.

Ported from MATLAB ``plotlexure.m`` (2010/12/09 0.1).

Usage::

    python plot_lex_ure.py diffeph.out --sats 1 2 3 --trange 0 24
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


def plot_lex_ure(
    filepath: str | Path,
    sats: list[int] | None = None,
    trange: tuple[float, float] = (0.0, 24.0),
) -> None:
    """Plot LEX URE time series and per-satellite bar chart.

    The URE column is the second-to-last column of the input file, and the
    elevation angle is the last column.  Records with elevation below
    ``minel`` (15 deg) have their URE set to NaN.  After filtering, the
    per-epoch mean is subtracted from all satellites at that epoch.

    Args:
        filepath: Path to the ephemeris difference file.
        sats: List of satellite PRN numbers to process (default 1..32).
        trange: Time range ``(t_start_hr, t_end_hr)`` for display.
    """
    if sats is None:
        sats = list(range(1, 33))

    v = load_data(filepath)

    y_range = 3.0
    minel = 15.0

    # Time column (second column, 0-indexed col 1), normalised to seconds of day
    timv = v[:, 1] - np.floor(v[:, 1] / 86400.0) * 86400.0
    time_unique = np.unique(timv)

    # Mask low-elevation records: set URE (second-to-last col) to NaN
    # where elevation (last col) < minel
    v[v[:, -1] < minel, -2] = np.nan

    # Build URE matrix: rows = unique epochs, cols = satellites
    ure = np.full((len(time_unique), len(sats)), np.nan)
    for i, sat in enumerate(sats):
        mask_sat = v[:, 2] == sat
        if not np.any(mask_sat):
            continue
        sat_times = timv[mask_sat]
        _, idx_time, idx_sat = np.intersect1d(
            time_unique, sat_times, return_indices=True
        )
        ure[idx_time, i] = v[mask_sat, -2][idx_sat]

    # Remove per-epoch mean (over available satellites)
    for i in range(len(time_unique)):
        valid = ~np.isnan(ure[i, :])
        if np.any(valid):
            ure[i, :] -= np.nanmean(ure[i, valid])

    # --- Figure 1: time series ---
    fig1, ax1 = plt.subplots(figsize=(10, 5))

    sat_labels: list[str] = []
    rms_errors: list[float] = []

    for i, sat in enumerate(sats):
        valid = ~np.isnan(ure[:, i])
        ax1.plot(time_unique / 3600.0, ure[:, i], "-")

        sat_labels.append(f"GPS{sat:02d}")
        n_valid = int(np.sum(valid))
        rms = np.sqrt(np.mean(ure[valid, i] ** 2)) if n_valid > 0 else 0.0
        rms_errors.append(rms)

        print(f"GPS{sat:02d}: {n_valid:4d} {rms:8.4f} m")

    rms_arr = np.array(rms_errors)

    ax1.set_xlim(trange)
    ax1.set_ylim(-y_range, y_range)
    ax1.set_xlabel("Time (hr)")
    ax1.set_ylabel("Error (m)")
    ax1.set_title(f"URE Error : {filepath}")
    ax1.legend(["URE"])
    ax1.grid(True)

    # --- Figure 2: bar chart ---
    fig2, ax2 = plt.subplots(figsize=(10, 5))
    ax2.set_position([0.08, 0.12, 0.89, 0.81])

    if rms_errors:
        x = np.arange(len(sat_labels))
        ax2.bar(x, rms_arr, label="URE")
        ax2.set_xticks(x)
        ax2.set_xticklabels(sat_labels, rotation=45, ha="right", fontsize=7)
        ax2.set_ylim(0, y_range)

    ax2.legend()
    ax2.set_ylabel("URE RMS Error (m)")
    ax2.set_title(f"QZSS LEX Ephemeris/Clock URE Error: {filepath}")

    plt.show()


def main(argv: list[str] | None = None) -> None:
    """Entry point for CLI invocation.

    Args:
        argv: Command-line arguments (default: ``sys.argv[1:]``).
    """
    parser = argparse.ArgumentParser(
        description="Plot LEX User Range Error (URE).",
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
    plot_lex_ure(args.file, sats=args.sats, trange=tuple(args.trange))


if __name__ == "__main__":
    main()
