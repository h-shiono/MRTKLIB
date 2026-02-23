"""Plot ionospheric delay from STEC estimation files.

Converted from MATLAB: util/geniono/plotiono.m

Reads a ``$STEC`` formatted file with 14 data columns and plots ionospheric
delay versus time for each requested GPS satellite.  Cycle-slip epochs are
highlighted in red.

Column layout (after the ``$STEC`` tag)::

    week  tow  sat  slip  iono  sig  ionor  sigr  bias  sigb  az  el  PG  LG
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

# GPS L1 / L2 frequencies (Hz)
_F1 = 1.57542e9
_F2 = 1.22760e9
_C_IONO = 1.0 - _F1**2 / _F2**2  # noqa: F841 – kept for reference (used in commented logic)

# Column indices inside the data matrix (0-based, after skipping the $STEC tag)
_COL_W = 0
_COL_T = 1
_COL_S = 2
_COL_SLIP = 3
_COL_IONO = 4
_COL_SIG = 5  # noqa: F841
_COL_IONOR = 6  # noqa: F841
_COL_SIGR = 7  # noqa: F841
_COL_BIAS = 8  # noqa: F841
_COL_SIGB = 9  # noqa: F841
_COL_AZ = 10  # noqa: F841
_COL_EL = 11  # noqa: F841
_COL_PG = 12  # noqa: F841
_COL_LG = 13  # noqa: F841


def _load_stec(filepath: str | Path) -> np.ndarray:
    """Load a ``$STEC`` file, stripping the leading tag and satellite prefix.

    The file is expected to have one header line followed by data lines that
    start with ``$STEC``.  The satellite column contains entries like ``G01``
    and is converted to a plain integer (``1``).

    Args:
        filepath: Path to the STEC data file.

    Returns:
        A 2-D NumPy array of shape ``(N, 14)`` with dtype ``float64``.
    """
    rows: list[list[float]] = []
    with open(filepath) as fh:
        # Skip the first header line.
        next(fh)
        for line in fh:
            line = line.strip()
            if not line or not line.startswith("$STEC"):
                continue
            parts = line.split()
            # parts[0] = "$STEC"
            # parts[1] = week, parts[2] = tow, parts[3] = "Gnn", parts[4..] = numeric
            week = float(parts[1])
            tow = float(parts[2])
            sat = float(parts[3][1:])  # strip leading 'G'
            numeric = [float(x) for x in parts[4:]]
            rows.append([week, tow, sat] + numeric)
    return np.array(rows)


def plot_iono(
    filepath: str | Path,
    satellites: list[int] | None = None,
) -> plt.Figure:
    """Create an ionospheric-delay plot from a STEC file.

    Args:
        filepath: Path to the ``$STEC`` formatted input file (14 data columns).
        satellites: List of GPS PRN numbers to plot.  ``None`` plots PRN 1-32.

    Returns:
        The matplotlib ``Figure`` object.
    """
    if satellites is None:
        satellites = list(range(1, 33))

    data = _load_stec(filepath)

    t = data[:, _COL_T]
    s = data[:, _COL_S]
    slip = data[:, _COL_SLIP]
    iono = data[:, _COL_IONO]

    fig, ax = plt.subplots(facecolor="w")
    ax.grid(True)

    for sat in satellites:
        # All points for this satellite.
        mask = s == sat
        ax.plot(t[mask] / 3600.0, iono[mask], "b.", markersize=2)

        # Cycle-slip points highlighted in red.
        mask_slip = mask & (slip == 1)
        ax.plot(t[mask_slip] / 3600.0, iono[mask_slip], "r.", markersize=2)

    ax.set_xlabel("TIME (H)")
    ax.set_ylabel("IONO DELAY (m)")
    ax.set_xlim(t[0] / 3600.0, t[-1] / 3600.0)
    ax.set_ylim(-2, 18)

    fig.tight_layout()
    return fig


def _parse_args() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        Parsed argument namespace.
    """
    parser = argparse.ArgumentParser(
        description="Plot ionospheric delay from a $STEC file (14-column format).",
    )
    parser.add_argument(
        "file",
        nargs="?",
        default="iono_0225.stec",
        help="Path to the $STEC data file (default: %(default)s).",
    )
    parser.add_argument(
        "--sat",
        type=int,
        nargs="+",
        default=None,
        help="GPS PRN numbers to plot (default: 1-32).",
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
    fig = plot_iono(args.file, satellites=args.sat)
    if args.output:
        fig.savefig(args.output, dpi=150)
    else:
        plt.show()
