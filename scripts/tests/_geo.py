"""_geo.py - WGS84/GRS80 geodetic helpers shared by MRTKLIB test scripts.

Functions
---------
blh2xyz     -- geodetic (lat, lon, h) → ECEF [X, Y, Z]
xyz2blh     -- ECEF [X, Y, Z] → geodetic (lat_deg, lon_deg, h)
xyz2enu     -- ECEF difference vector → local ENU
nmea_to_deg -- NMEA DDmm.mmmm / DDDmm.mmmm → decimal degrees
"""

import math

import numpy as np

# ---------------------------------------------------------------------------
# WGS84 / GRS80 constants (numerically identical for both datums)
# ---------------------------------------------------------------------------
_A = 6_378_137.0            # semi-major axis [m]
_F = 1.0 / 298.257_223_563  # flattening
_E2 = _F * (2.0 - _F)       # first eccentricity squared


def blh2xyz(lat_deg: float, lon_deg: float, h: float) -> np.ndarray:
    """Convert geodetic (lat, lon, h) to ECEF [X, Y, Z].

    Args:
        lat_deg: Latitude in decimal degrees.
        lon_deg: Longitude in decimal degrees.
        h: Ellipsoidal height in metres.

    Returns:
        ECEF coordinates [X, Y, Z] in metres.
    """
    lat = math.radians(lat_deg)
    lon = math.radians(lon_deg)
    sl, cl = math.sin(lat), math.cos(lat)
    sp, cp = math.sin(lon), math.cos(lon)
    N = _A / math.sqrt(1.0 - _E2 * sl * sl)
    return np.array([(N + h) * cl * cp, (N + h) * cl * sp, (N * (1.0 - _E2) + h) * sl])


def xyz2blh(x: float, y: float, z: float) -> tuple[float, float, float]:
    """Convert ECEF [X, Y, Z] to geodetic (lat_deg, lon_deg, h).

    Args:
        x: ECEF X coordinate in metres.
        y: ECEF Y coordinate in metres.
        z: ECEF Z coordinate in metres.

    Returns:
        Tuple (lat_deg, lon_deg, h) — latitude and longitude in decimal
        degrees, ellipsoidal height in metres.
    """
    lon_deg = math.degrees(math.atan2(y, x))
    p = math.sqrt(x * x + y * y)
    lat = math.atan2(z, p * (1.0 - _E2))
    for _ in range(10):
        N = _A / math.sqrt(1.0 - _E2 * math.sin(lat) ** 2)
        lat_new = math.atan2(z + _E2 * N * math.sin(lat), p)
        if abs(lat_new - lat) < 1e-12:
            break
        lat = lat_new
    N = _A / math.sqrt(1.0 - _E2 * math.sin(lat) ** 2)
    cl = math.cos(lat)
    h = p / cl - N if abs(cl) > 1e-9 else abs(z) / math.sin(lat) - N * (1.0 - _E2)
    return math.degrees(lat), lon_deg, h


def xyz2enu(dx: np.ndarray, lat_deg: float, lon_deg: float) -> np.ndarray:
    """Convert ECEF difference vector to local ENU at a reference point.

    Args:
        dx: ECEF difference vector [dX, dY, dZ] in metres.
        lat_deg: Reference point latitude in decimal degrees.
        lon_deg: Reference point longitude in decimal degrees.

    Returns:
        ENU vector [East, North, Up] in metres.
    """
    lat = math.radians(lat_deg)
    lon = math.radians(lon_deg)
    sl, cl = math.sin(lat), math.cos(lat)
    sp, cp = math.sin(lon), math.cos(lon)
    e = -sp * dx[0] + cp * dx[1]
    n = -sl * cp * dx[0] - sl * sp * dx[1] + cl * dx[2]
    u = cl * cp * dx[0] + cl * sp * dx[1] + sl * dx[2]
    return np.array([e, n, u])


def nmea_to_deg(val_str: str, hemi: str) -> float:
    """Convert NMEA DDmm.mmmm / DDDmm.mmmm to decimal degrees.

    Args:
        val_str: NMEA coordinate string (e.g., ``"3606.2180069"``).
        hemi: Hemisphere indicator (``'N'``, ``'S'``, ``'E'``, or ``'W'``).

    Returns:
        Decimal degrees (negative for S/W hemispheres).
    """
    val = float(val_str)
    deg = int(val / 100)
    result = deg + (val - deg * 100) / 60.0
    return -result if hemi in ("S", "W") else result
