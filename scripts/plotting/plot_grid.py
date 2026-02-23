"""Plot ionospheric grid point positions on a lat/lon scatter chart.

Converted from MATLAB: util/geniono/plotgrid.m

The original MATLAB script used GMT (Generic Mapping Tools) via a MATLAB
wrapper to render a Miller-projection map.  This Python version replaces
the GMT calls with a plain matplotlib scatter plot of latitude versus
longitude, keeping the same data-reading logic.

Input file format (whitespace-delimited, no header)::

    tag  week  tow  lat  lon
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

# Column indices (0-based)
_COL_TAG = 0  # noqa: F841 – string tag, skipped during numeric load
_COL_W = 1  # noqa: F841
_COL_T = 2  # noqa: F841
_COL_LAT = 3
_COL_LON = 4


def _load_grid(filepath: str | Path) -> tuple[np.ndarray, np.ndarray]:
    """Load grid-point positions from an ionospheric position file.

    The first column is a string tag and is skipped; the remaining four
    columns (week, tow, lat, lon) are read as floats.

    Args:
        filepath: Path to the grid position file.

    Returns:
        A tuple ``(lat, lon)`` of 1-D NumPy arrays.
    """
    lats: list[float] = []
    lons: list[float] = []
    with open(filepath) as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            # parts[0] = tag (string), parts[1] = week, parts[2] = tow,
            # parts[3] = lat, parts[4] = lon
            lats.append(float(parts[3]))
            lons.append(float(parts[4]))
    return np.array(lats), np.array(lons)


def plot_grid(filepath: str | Path) -> plt.Figure:
    """Create a scatter plot of ionospheric grid point positions.

    The original MATLAB code rendered a Miller-projection map centred at
    (137 E, 38 N) using GMT.  This version produces a simple
    longitude-vs-latitude scatter plot with matching aesthetics.

    Args:
        filepath: Path to the grid position file (5-column format).

    Returns:
        The matplotlib ``Figure`` object.
    """
    lat, lon = _load_grid(filepath)

    fig, ax = plt.subplots(facecolor="w")
    ax.plot(lon, lat, "r.", markersize=3)
    ax.set_xlabel("Longitude (deg)")
    ax.set_ylabel("Latitude (deg)")
    ax.set_aspect("equal")
    ax.grid(True, color="0.5", linewidth=0.5)

    fig.tight_layout()
    return fig


def _parse_args() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        Parsed argument namespace.
    """
    parser = argparse.ArgumentParser(
        description="Plot ionospheric grid point positions (lat/lon scatter).",
    )
    parser.add_argument(
        "file",
        nargs="?",
        default="stec/iono.pos",
        help="Path to the grid position file (default: %(default)s).",
    )
    parser.add_argument(
        "-o",
        "--output",
        default=None,
        help="Save figure to file instead of displaying interactively.",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = _parse_args()
    fig = plot_grid(args.file)
    if args.output:
        fig.savefig(args.output, dpi=150)
    else:
        plt.show()
