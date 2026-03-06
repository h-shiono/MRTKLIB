#!/usr/bin/env python3
"""Plot GLONASS ephemeris errors from test output.

Reads a GLONASS ephemeris test output file (e.g. ``testgloeph.out``) and
plots the broadcast-vs-precise ephemeris position and clock errors over time.
The clock bias column is de-meaned before plotting.

Columns in the input file are expected as:
    col 0: (unused, or satellite system id)
    col 1: time offset (seconds)
    col 2: satellite number
    col 3-6: position errors x, y, z (m) and clock error (m)

Ported from: tests/utest/testgloeph.m
"""

import argparse

import matplotlib.pyplot as plt
import numpy as np


def load_gloeph_data(filepath: str) -> tuple[str, np.ndarray, np.ndarray]:
    """Load GLONASS ephemeris error data from a test output file.

    Args:
        filepath: Path to the test output file.

    Returns:
        A tuple of (satellite_label, time_hours, dpos) where:
            - satellite_label: e.g. "SAT01"
            - time_hours: 1D array of time in hours
            - dpos: (N, 4) array of [dx, dy, dz, dclk] errors in meters
    """
    data = np.loadtxt(filepath)

    sat_num = int(data[0, 2])
    sat_label = f"SAT{sat_num:02d}"

    time_s = data[:, 1]
    time_h = time_s / 3600.0

    dpos = data[:, 3:7].copy()
    # De-mean the clock column.
    clk_valid = dpos[:, 3][~np.isnan(dpos[:, 3])]
    if clk_valid.size > 0:
        dpos[:, 3] -= np.mean(clk_valid)

    return sat_label, time_h, dpos


def plot_gloeph(sat_label: str, time_h: np.ndarray, dpos: np.ndarray, output: str | None) -> None:
    """Create the GLONASS ephemeris error plot.

    Args:
        sat_label: Satellite identifier string (e.g. "SAT01").
        time_h: Time array in hours.
        dpos: (N, 4) error array [x, y, z, clk] in meters.
        output: Output file path, or None to display interactively.
    """
    labels = ["x", "y", "z", "clk"]

    fig, ax = plt.subplots(figsize=(12, 6), facecolor="white")

    for i, label in enumerate(labels):
        ax.plot(time_h, dpos[:, i], label=label)

    # Overlay markers at 30-minute intervals (every 60 samples at 30 s cadence,
    # offset by 30 samples to match MATLAB's 31:60:end indexing).
    marker_idx = np.arange(30, len(time_h), 60)
    for i, label in enumerate(labels):
        ax.plot(time_h[marker_idx], dpos[marker_idx, i], ".", markersize=4)

    # Compute standard deviations for valid (non-NaN) data.
    valid_xyz = ~np.isnan(dpos[:, 0])
    valid_clk = ~np.isnan(dpos[:, 3])
    std_xyz = np.std(dpos[valid_xyz, :3], axis=0) if np.any(valid_xyz) else np.zeros(3)
    std_clk = np.std(dpos[valid_clk, 3]) if np.any(valid_clk) else 0.0

    ax.text(
        0.02,
        0.95,
        f"STD: X={std_xyz[0]:.4f} Y={std_xyz[1]:.4f} Z={std_xyz[2]:.4f} CLK={std_clk:.4f}m",
        transform=ax.transAxes,
        verticalalignment="top",
        fontsize=9,
    )

    ax.set_xlabel("time (hr)")
    ax.set_ylabel("error (m)")
    ax.set_xlim(time_h[0], time_h[-1])
    ax.set_ylim(-10, 10)
    ax.legend()
    ax.set_title(f"testgloeph: brdc-prec ephemeris {sat_label}")
    ax.grid(True)

    plt.tight_layout()
    if output:
        fig.savefig(output, dpi=150)
        print(f"Saved plot to {output}")
    else:
        plt.show()


def main() -> None:
    """Parse arguments and generate the GLONASS ephemeris error plot."""
    parser = argparse.ArgumentParser(
        description="Plot GLONASS ephemeris errors from test output."
    )
    parser.add_argument(
        "input",
        nargs="?",
        default="testgloeph.out",
        help="Input test output file (default: testgloeph.out)",
    )
    parser.add_argument(
        "-o",
        "--output",
        default=None,
        help="Output image file (default: display interactively)",
    )
    args = parser.parse_args()

    sat_label, time_h, dpos = load_gloeph_data(args.input)
    plot_gloeph(sat_label, time_h, dpos, args.output)


if __name__ == "__main__":
    main()
