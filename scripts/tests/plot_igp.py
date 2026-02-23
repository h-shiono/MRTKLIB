#!/usr/bin/env python3
"""Plot Ionospheric Grid Point (IGP) mesh positions.

Reads an IGP mesh data file and plots the grid points on a 2D scatter plot.
Optionally overlays the ionospheric pierce point (IPP) footprint for a given
receiver position and elevation mask.

The original MATLAB script used gmt() for map projection. This version uses
plain matplotlib scatter/line plots instead.

Ported from: tests/utest/plotigp.m
"""

import argparse

import matplotlib.pyplot as plt
import numpy as np


def read_mesh(filepath: str) -> np.ndarray:
    """Read IGP mesh data from a VTEC grid file.

    Parses lines matching the pattern ``Mesh N: (lon, lat)`` and returns
    the extracted coordinates.  The MATLAB original passes the first value
    to GMT as the x (longitude) argument, so the file order is (lon, lat).

    Args:
        filepath: Path to the mesh data file (e.g. ``001.txt``).

    Returns:
        An (N, 2) array where each row is [longitude, latitude] in degrees.
    """
    mesh = []
    with open(filepath) as f:
        for line in f:
            line = line.strip()
            if not line.startswith("Mesh"):
                continue
            # Expected format: "Mesh N: (lon, lat)"
            try:
                paren_start = line.index("(")
                paren_end = line.index(")")
                coords = line[paren_start + 1 : paren_end]
                parts = coords.split(",")
                if len(parts) >= 2:
                    lon = float(parts[0].strip())
                    lat = float(parts[1].strip())
                    mesh.append([lon, lat])
            except (ValueError, IndexError):
                continue

    if not mesh:
        raise ValueError(f"No mesh data found in {filepath}")

    return np.array(mesh)


def igp_pos(pos: np.ndarray, azel: np.ndarray) -> np.ndarray:
    """Compute ionospheric pierce point position.

    Calculates the sub-ionospheric point for a given receiver position
    and satellite azimuth/elevation using a single-layer ionosphere model.

    Args:
        pos: Receiver position [lat, lon] in radians.
        azel: Satellite [azimuth, elevation] in radians.

    Returns:
        Pierce point position [lat, lon] in radians.
    """
    re = 6380.0  # Earth radius (km)
    hion = 350.0  # Ionospheric shell height (km)

    rp = re / (re + hion) * np.cos(azel[1])
    ap = np.pi / 2 - azel[1] - np.arcsin(rp)

    sinap = np.sin(ap)
    cosaz = np.cos(azel[0])

    posp_lat = np.arcsin(np.sin(pos[0]) * np.cos(ap) + np.cos(pos[0]) * sinap * cosaz)
    posp_lon = pos[1] + np.arcsin(sinap * np.sin(azel[0]) / np.cos(posp_lat))

    return np.array([posp_lat, posp_lon])


def compute_ipp_footprint(
    pos_deg: np.ndarray, elmask_deg: float, az_step: float = 3.0
) -> np.ndarray:
    """Compute the IPP footprint ring for a receiver position.

    Args:
        pos_deg: Receiver position [lat, lon] in degrees.
        elmask_deg: Elevation mask angle in degrees.
        az_step: Azimuth step size in degrees (default: 3.0).

    Returns:
        An (N, 2) array of [lat, lon] in degrees tracing the IPP footprint.
    """
    pos_rad = np.deg2rad(pos_deg)
    elmask_rad = np.deg2rad(elmask_deg)

    azimuths = np.arange(0, 360 + az_step, az_step)
    footprint = []
    for az in azimuths:
        azel = np.array([np.deg2rad(az), elmask_rad])
        pp = igp_pos(pos_rad, azel)
        footprint.append(np.rad2deg(pp))

    return np.array(footprint)


def main() -> None:
    """Parse arguments and generate the IGP mesh plot."""
    parser = argparse.ArgumentParser(description="Plot IGP mesh points.")
    parser.add_argument(
        "input",
        nargs="?",
        default="../../nicttec/vtec/2011/001.txt",
        help="Input mesh data file (default: ../../nicttec/vtec/2011/001.txt)",
    )
    parser.add_argument(
        "--pos",
        nargs=2,
        type=float,
        default=[36.0, 138.0],
        metavar=("LAT", "LON"),
        help="Receiver position [lat, lon] in degrees (default: 36 138)",
    )
    parser.add_argument(
        "--elmask",
        type=float,
        default=15.0,
        help="Elevation mask angle in degrees (default: 15)",
    )
    parser.add_argument(
        "-o",
        "--output",
        default=None,
        help="Output image file (default: display interactively)",
    )
    args = parser.parse_args()

    mesh = read_mesh(args.input)
    pos = np.array(args.pos)
    footprint = compute_ipp_footprint(pos, args.elmask)

    fig, ax = plt.subplots(figsize=(10, 8))

    # Plot mesh points (mesh columns: [lon, lat]).
    ax.scatter(mesh[:, 0], mesh[:, 1], c="red", s=10, marker=".", label="IGP mesh")

    # Plot receiver position.
    ax.scatter(pos[1], pos[0], c="blue", s=30, marker=".", label="Receiver", zorder=5)

    # Plot IPP footprint.
    ax.plot(footprint[:, 1], footprint[:, 0], "b-", linewidth=1.0, label="IPP footprint")

    ax.set_xlabel("Longitude (deg)")
    ax.set_ylabel("Latitude (deg)")
    ax.set_title("IGP Mesh Points")
    ax.legend(loc="upper right")
    ax.grid(True, color="0.5", linewidth=0.5)
    ax.set_aspect("equal")

    plt.tight_layout()
    if args.output:
        fig.savefig(args.output, dpi=150)
        print(f"Saved plot to {args.output}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
