#!/usr/bin/env python3
"""Read a geoid model file and generate a C source file with static arrays.

Reads a geoid grid file (e.g. WW15MGH.GRD in EGM96 format), extracts height
data for a specified geographic range, and writes a C source file containing
the geoid heights as static const arrays.

The output C file includes:
    - DLON/DLAT increment defines
    - A range array {W, E, S, N} in degrees
    - A 2D float array of geoid heights (lon x lat)

Ported from: util/geoid/gengeoid.m
"""

import argparse
from pathlib import Path

import numpy as np


def read_geoid_grid(filepath: str) -> tuple[np.ndarray, float, float, float, float, float, float]:
    """Read a geoid grid file in EGM96 WW15MGH.GRD format.

    The first line of the file contains six header values:
        lat1 lat2 lon1 lon2 dlat dlon

    The remaining lines contain the geoid height data in row-major order,
    8 values per line, starting from the northernmost latitude.

    Args:
        filepath: Path to the geoid grid file.

    Returns:
        A tuple of (data, lat1, lat2, lon1, lon2, dlat, dlon) where data is
        a 2D numpy array of shape (nlat, nlon) with geoid heights in meters,
        and the other values describe the grid extent and spacing.
    """
    with open(filepath) as f:
        header = f.readline().split()

    lat1, lat2, lon1, lon2, dlat, dlon = (float(v) for v in header[:6])

    nlon = int((lon2 - lon1) / dlon) + 1
    nlat = int((lat2 - lat1) / dlat) + 1

    # Read all data after the header line as a flat stream of values.
    # The GRD format has variable numbers of values per line (typically 8,
    # with fewer on the last line of each latitude block), so np.loadtxt
    # cannot be used directly.
    with open(filepath) as f:
        next(f)  # skip header
        raw_flat = np.array([float(x) for line in f for x in line.split()])

    # Each latitude row has nlon values spread across ceil(nlon/8) lines of 8
    # values each, so each block is ceil(nlon/8)*8 values long.
    vals_per_block = int(np.ceil(nlon / 8)) * 8
    data = np.empty((nlat, nlon))
    for i in range(nlat):
        start = i * vals_per_block
        data[i, :] = raw_flat[start : start + nlon]

    # Flip so that latitude index 0 is the southernmost row.
    data = np.flipud(data)

    return data, lat1, lat2, lon1, lon2, dlat, dlon


def generate_c_source(
    data: np.ndarray,
    lat1: float,
    lat2: float,
    lon1: float,
    lon2: float,
    dlat: float,
    dlon: float,
    geo_range: list[float],
    output_path: str,
    source_filename: str,
) -> None:
    """Generate a C source file containing geoid height data.

    Args:
        data: 2D array of geoid heights (nlat x nlon), south-to-north.
        lat1: Southern latitude bound of the full grid (deg).
        lat2: Northern latitude bound of the full grid (deg).
        lon1: Western longitude bound of the full grid (deg).
        lon2: Eastern longitude bound of the full grid (deg).
        dlat: Latitude grid spacing (deg).
        dlon: Longitude grid spacing (deg).
        geo_range: Geographic subset [lon_west, lon_east, lat_south, lat_north].
        output_path: Path for the output C file.
        source_filename: Name of the source geoid file (for the comment).
    """
    lons = np.arange(lon1, lon2 + dlon / 2, dlon)
    lats = np.arange(lat1, lat2 + dlat / 2, dlat)

    # MATLAB flipud was applied, so lats[0] corresponds to lat1 (south).
    lat_idx = np.where((geo_range[2] <= lats) & (lats <= geo_range[3]))[0]
    lon_idx = np.where((geo_range[0] <= lons) & (lons <= geo_range[1]))[0]

    nj = len(lon_idx)
    ni = len(lat_idx)

    with open(output_path, "w") as f:
        f.write(f"#define DLON   {dlon:.2f}                 /* longitude increment (deg) */ \n")
        f.write(f"#define DLAT   {dlat:.2f}                 /* latitude increment (deg) */ \n")
        f.write(
            "static const double range[4];       "
            "/* geoid area range {W,E,S,N} (deg) */\n"
        )
        f.write(
            f"static const float geoid[{nj}][{ni}]; "
            f"/* geoid heights (m) (lon x lat) */\n"
        )
        f.write("\n\n")
        f.write(
            "/*----------------------------------------------"
            "--------------------------------\n"
        )
        f.write(f"* geoid heights (derived from {source_filename})\n")
        f.write(
            "*----------------------------------------------"
            "-------------------------------*/\n"
        )
        f.write(
            f"static const double range[]="
            f"{{{geo_range[0]:.2f},{geo_range[1]:.2f},"
            f"{geo_range[2]:.2f},{geo_range[3]:.2f}}};\n\n"
        )
        f.write(f"static const float geoid[{nj}][{ni}]={{")

        for m_idx, m in enumerate(lon_idx):
            f.write("{\n")
            for n_idx, n in enumerate(lat_idx):
                f.write(f"{data[n, m]:7.3f}f")
                if n_idx != ni - 1:
                    f.write(",")
                if (n_idx + 1) % 10 == 0:
                    f.write("\n")
            if m_idx == nj - 1:
                f.write("\n}")
            else:
                f.write("\n},")

        f.write("};\n")


def main() -> None:
    """Parse arguments and run geoid C source generation."""
    parser = argparse.ArgumentParser(
        description="Read a geoid model file and generate a C source file."
    )
    parser.add_argument(
        "input",
        nargs="?",
        default="WW15MGH.GRD",
        help="Input geoid grid file (default: WW15MGH.GRD)",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="geoid.c",
        help="Output C source file (default: geoid.c)",
    )
    parser.add_argument(
        "-r",
        "--range",
        nargs=4,
        type=float,
        default=[120.0, 155.0, 20.0, 50.0],
        metavar=("LON_W", "LON_E", "LAT_S", "LAT_N"),
        help="Geographic range [lon_west lon_east lat_south lat_north] (default: 120 155 20 50)",
    )
    args = parser.parse_args()

    source_name = Path(args.input).name
    data, lat1, lat2, lon1, lon2, dlat, dlon = read_geoid_grid(args.input)
    generate_c_source(
        data, lat1, lat2, lon1, lon2, dlat, dlon, args.range, args.output, source_name
    )
    print(f"Generated {args.output} from {args.input} (range: {args.range})")


if __name__ == "__main__":
    main()
