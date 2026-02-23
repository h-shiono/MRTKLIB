#!/usr/bin/env python3
"""Plot precise ephemeris errors from test output.

Reads a precise ephemeris test output file (e.g. ``testpeph1.out`` or
``testpeph2.out``) and plots position and clock interpolation errors over time.

This script handles both testpeph1 and testpeph2 scenarios:
    - testpeph1: interpolation error (cols 2-5 are computed ephemeris, cols 6-9
      are reference; error = computed - reference). Y-axis range: [-0.05, 0.05].
    - testpeph2: broadcast-vs-precise (cols 2-5 minus cols 6-9).
      Y-axis range: [-10, 10].

The ``--mode`` argument selects the scenario. By default it is inferred from
the filename (``testpeph1`` -> interp, ``testpeph2`` -> brdc).

The original MATLAB scripts also called readeph()/readclk() to load SP3
reference ephemerides for testpeph1. In this Python port, the comparison data
is assumed to already be present in the output file columns. If the output
file only contains computed columns (no reference columns 6-9), the script
plots the raw computed values directly.

Ported from: tests/utest/testpeph1.m and tests/utest/testpeph2.m
"""

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def load_peph_data(
    filepath: str, mode: str
) -> tuple[str, np.ndarray, np.ndarray, str]:
    """Load precise ephemeris error data from a test output file.

    Args:
        filepath: Path to the test output file.
        mode: Error mode -- ``"interp"`` for interpolation error (testpeph1)
            or ``"brdc"`` for broadcast-vs-precise (testpeph2).

    Returns:
        A tuple of (sat_label, time_hours, dpos, title_desc) where:
            - sat_label: e.g. "GPS01"
            - time_hours: 1D array of time in hours
            - dpos: (N, 4) array of [dx, dy, dz, dclk] errors in meters
            - title_desc: description string for the plot title
    """
    data = np.loadtxt(filepath)

    # Replace exact zeros with NaN (matches MATLAB: ephs(ephs==0)=nan).
    data[data == 0.0] = np.nan

    sat_num = int(np.nanmin(data[:, 0]))  # First column is satellite PRN.
    sat_label = f"GPS{sat_num:02d}"

    # Build time vector: 0 to 2 days at 30 s intervals.
    nrows = data.shape[0]
    time_s = np.arange(nrows) * 30.0
    time_h = time_s / 3600.0

    ncols = data.shape[1]

    if ncols >= 10:
        # Columns: [prn, (aux), pos_x, pos_y, pos_z, clk, ref_x, ref_y, ref_z, ref_clk]
        # testpeph2 mode: dpos = cols[2:6] - cols[6:10]
        # testpeph1 mode: same structure, difference is the error type
        computed = data[:, 2:6]
        reference = data[:, 6:10]
        dpos = computed - reference
        title_desc = "brdc-prec ephemeris" if mode == "brdc" else "interpolation error"
    elif ncols >= 6:
        # Only computed columns available; plot them directly.
        dpos = data[:, 2:6]
        title_desc = "interpolation error" if mode == "interp" else "brdc-prec ephemeris"
    else:
        raise ValueError(
            f"Expected at least 6 columns in {filepath}, got {ncols}"
        )

    return sat_label, time_h, dpos, title_desc


def plot_peph(
    sat_label: str,
    time_h: np.ndarray,
    dpos: np.ndarray,
    title_desc: str,
    ylimits: tuple[float, float],
    output: str | None,
) -> None:
    """Create the precise ephemeris error plot.

    Args:
        sat_label: Satellite identifier string (e.g. "GPS01").
        time_h: Time array in hours.
        dpos: (N, 4) error array [x, y, z, clk] in meters.
        title_desc: Description string for the plot title.
        ylimits: Y-axis limits (min, max).
        output: Output file path, or None to display interactively.
    """
    labels = ["x", "y", "z", "clk"]

    fig, ax = plt.subplots(figsize=(12, 6), facecolor="white")

    for i, label in enumerate(labels):
        ax.plot(time_h, dpos[:, i], label=label)

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
    ax.set_ylim(*ylimits)
    ax.legend()
    ax.set_title(f"testpeph: {title_desc} {sat_label}")
    ax.grid(True)

    plt.tight_layout()
    if output:
        fig.savefig(output, dpi=150)
        print(f"Saved plot to {output}")
    else:
        plt.show()


def main() -> None:
    """Parse arguments and generate the precise ephemeris error plot."""
    parser = argparse.ArgumentParser(
        description=(
            "Plot precise ephemeris errors from test output. "
            "Handles both testpeph1 (interpolation) and testpeph2 (brdc-prec) scenarios."
        )
    )
    parser.add_argument(
        "input",
        nargs="?",
        default="testpeph1.out",
        help="Input test output file (default: testpeph1.out)",
    )
    parser.add_argument(
        "--mode",
        choices=["interp", "brdc"],
        default=None,
        help=(
            "Error mode: 'interp' for interpolation error (testpeph1), "
            "'brdc' for broadcast-vs-precise (testpeph2). "
            "Default: inferred from filename."
        ),
    )
    parser.add_argument(
        "--ylim",
        nargs=2,
        type=float,
        default=None,
        metavar=("YMIN", "YMAX"),
        help="Y-axis limits (default: auto based on mode)",
    )
    parser.add_argument(
        "-o",
        "--output",
        default=None,
        help="Output image file (default: display interactively)",
    )
    args = parser.parse_args()

    # Infer mode from filename if not explicitly set.
    if args.mode is None:
        stem = Path(args.input).stem.lower()
        if "peph2" in stem:
            mode = "brdc"
        else:
            mode = "interp"
    else:
        mode = args.mode

    # Set y-axis limits based on mode if not explicitly set.
    if args.ylim is not None:
        ylimits = (args.ylim[0], args.ylim[1])
    elif mode == "interp":
        ylimits = (-0.05, 0.05)
    else:
        ylimits = (-10.0, 10.0)

    sat_label, time_h, dpos, title_desc = load_peph_data(args.input, mode)
    plot_peph(sat_label, time_h, dpos, title_desc, ylimits, args.output)


if __name__ == "__main__":
    main()
