#!/usr/bin/env python3
"""Plot LEX ionosphere correction maps.

Reads a NumPy ``.npz`` data file containing ionosphere TEC grid data and
produces a filled-contour plot of vertical ionosphere delay.

The original MATLAB script (``plotlexion.m``, 2010/12/09 0.1) loaded
variables via ``eval(file)`` from a MATLAB script file.  This Python
version expects a ``.npz`` archive with the following arrays:

- ``epoch`` : shape ``(6,)`` -- calendar epoch ``[Y, M, D, h, m, s]``.
- ``time``  : shape ``(N,)`` -- time offsets in seconds of day.
- ``tec``   : shape ``(n_lat, n_lon, N)`` -- TEC grid values (m of L1 delay).
- ``lons``  : shape ``(n_lon,)`` -- longitude grid points (degrees).
- ``lats``  : shape ``(n_lat,)`` -- latitude grid points (degrees).

The MATLAB version used ``gmt()`` map projections and ``ggt('colorbarv', ...)``
for rendering.  This port uses plain ``matplotlib.contourf`` without geographic
map projection, as instructed.

Usage::

    python plot_lex_ion.py LEXION_20101204.npz --index 2
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def load_npz(filepath: str | Path) -> dict[str, np.ndarray]:
    """Load a ``.npz`` archive and return its contents as a dictionary.

    Args:
        filepath: Path to the ``.npz`` data file.

    Returns:
        Dictionary mapping array names to numpy arrays.  Expected keys are
        ``epoch``, ``time``, ``tec``, ``lons``, ``lats``.
    """
    data = np.load(filepath)
    return {key: data[key] for key in data.files}


def _epoch_to_label(epoch: np.ndarray, time_offset: float) -> str:
    """Build a human-readable timestamp string.

    Mimics the MATLAB logic::

        td = caltomjd(epoch);
        ep = mjdtocal(td + (time + 0.5) / 86400);
        ts = sprintf('%04.0f/%02.0f/%02.0f %02.0f:%02.0f', ep(1:5));

    The ``+0.5`` second offset from the original is preserved.

    Args:
        epoch: Calendar epoch array ``[Y, M, D, h, m, s]``.
        time_offset: Time offset in seconds of day.

    Returns:
        Formatted timestamp string ``YYYY/MM/DD HH:MM``.
    """
    # Convert epoch to a base number of seconds, add offset, recompute
    y, mo, d, h, mi, s = epoch[:6]
    total_seconds = h * 3600.0 + mi * 60.0 + s + time_offset + 0.5
    extra_days = int(total_seconds // 86400)
    remaining = total_seconds - extra_days * 86400.0

    # Simple day rollover (sufficient for display purposes)
    day = int(d) + extra_days
    hour = int(remaining // 3600)
    minute = int((remaining % 3600) // 60)

    return f"{int(y):04d}/{int(mo):02d}/{day:02d} {hour:02d}:{minute:02d}"


def _plot_contour_map(
    ax: plt.Axes,
    tec: np.ndarray,
    lons: np.ndarray,
    lats: np.ndarray,
    title: str,
) -> None:
    """Draw a filled-contour ionosphere map on *ax*.

    Replicates the MATLAB ``plotmap`` sub-function logic using plain
    ``matplotlib.contourf`` (no geographic projection).

    Args:
        ax: Matplotlib axes to draw on.
        tec: 2-D array of TEC values, shape ``(n_lat, n_lon)``.
        lons: 1-D longitude grid (degrees).
        lats: 1-D latitude grid (degrees).
        title: Plot title string.
    """
    levels = np.arange(0.0, 10.01, 0.01)
    lon_grid, lat_grid = np.meshgrid(lons, lats)

    cf = ax.contourf(lon_grid, lat_grid, tec, levels=levels, extend="both")
    cf.set_edgecolor("face")  # suppress contour edges

    # LEX TEC coverage rectangle (from original MATLAB)
    lonr = [141.0, 129.0, 126.7, 146.0, 146.0, 141.0]
    latr = [45.5, 34.7, 26.0, 26.0, 45.5, 45.5]
    ax.plot(lonr, latr, "k-", linewidth=1)

    ax.set_xlabel("Longitude (deg)")
    ax.set_ylabel("Latitude (deg)")
    ax.set_title(title)
    ax.set_aspect("equal", adjustable="box")

    # Colorbar
    cbar = plt.colorbar(cf, ax=ax, fraction=0.03, pad=0.04)
    cbar.set_label("Delay (m)")


def plot_lex_ion(
    filepath: str | Path,
    index: int = 2,
) -> None:
    """Plot LEX vertical ionosphere delay map.

    Args:
        filepath: Path to the ``.npz`` data file containing TEC grids.
        index: Time-step index to plot (1-based, matching MATLAB convention).
            Default is 2.
    """
    data = load_npz(filepath)
    epoch = data["epoch"]
    time_arr = data["time"]
    tec = data["tec"]
    lons = data["lons"]
    lats = data["lats"]

    # MATLAB uses 1-based indexing; convert to 0-based
    idx = index - 1
    if idx < 0 or idx >= time_arr.shape[0]:
        print(
            f"Error: index {index} out of range [1, {time_arr.shape[0]}].",
            file=sys.stderr,
        )
        sys.exit(1)

    time_val = time_arr[idx]
    timestamp = _epoch_to_label(epoch, time_val)

    # --- Figure: LEX ionosphere delay ---
    fig, ax = plt.subplots(figsize=(10, 7))
    _plot_contour_map(
        ax,
        tec[:, :, idx],
        lons,
        lats,
        f"LEX Vertical Ionosphere Delay (L1) (m): {timestamp}",
    )

    plt.tight_layout()
    plt.show()


def main(argv: list[str] | None = None) -> None:
    """Entry point for CLI invocation.

    Args:
        argv: Command-line arguments (default: ``sys.argv[1:]``).
    """
    parser = argparse.ArgumentParser(
        description="Plot LEX ionosphere correction map.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "file",
        nargs="?",
        default="LEXION_20101204.npz",
        help="Input .npz data file (default: LEXION_20101204.npz).",
    )
    parser.add_argument(
        "--index",
        type=int,
        default=2,
        help="1-based time-step index to plot (default: 2).",
    )
    args = parser.parse_args(argv)
    plot_lex_ion(args.file, index=args.index)


if __name__ == "__main__":
    main()
