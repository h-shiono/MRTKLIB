#!/usr/bin/env python3
"""parse_pvt.py — Extract time-series position data from NMEA and SBF files.

Returns lists of dicts with keys: time_gpst, lat, lon, hgt, mode, ns.
Coordinates are in degrees (lat/lon) and meters (hgt).

Usage as library:
    from parse_pvt import parse_nmea, parse_sbf, to_enu

    pts_nmea = parse_nmea("clas_rt.nmea")
    pts_sbf  = parse_sbf("G5P3076d.sbf")

    # Convert to ENU relative to first point (or explicit ref)
    enu_nmea = to_enu(pts_nmea)
    enu_sbf  = to_enu(pts_sbf, ref=(35.3362, 139.5203))

Usage as CLI:
    python3 scripts/plotting/parse_pvt.py clas_rt.nmea G5P3076d.sbf
"""

from __future__ import annotations

import math
import struct
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path

import numpy as np
import numpy.typing as npt

# ---------------------------------------------------------------------------
# WGS84 constants
# ---------------------------------------------------------------------------
WGS84_A = 6378137.0
WGS84_F = 1.0 / 298.257223563
WGS84_B = WGS84_A * (1.0 - WGS84_F)
WGS84_E2 = 2 * WGS84_F - WGS84_F**2


# ---------------------------------------------------------------------------
# NMEA parser
# ---------------------------------------------------------------------------
def _nmea_dm_to_deg(dm_str, hemi):
    """Convert NMEA DDMM.MMMMM to decimal degrees."""
    if not dm_str:
        return None
    dot = dm_str.index(".")
    deg = int(dm_str[: dot - 2])
    minutes = float(dm_str[dot - 2 :])
    val = deg + minutes / 60.0
    if hemi in ("S", "W"):
        val = -val
    return val


def parse_nmea(path):
    """Parse GGA sentences from an NMEA file.

    Returns list of dicts: time_gpst (str HH:MM:SS.SS), lat, lon, hgt (deg/m),
    mode (1=SPP,2=DGPS,4=Fix,5=Float), ns (satellite count).
    """
    results = []
    # GGA quality → mode mapping (approximate)
    gga_mode = {0: 0, 1: 1, 2: 2, 4: 4, 5: 5, 6: 6}

    with open(path, "r", errors="replace") as f:
        ddmmyy = None
        for line in f:
            line = line.strip()
            sentence = line[3:6] if len(line) > 6 else ""
            if not line.startswith("$") or sentence not in ["GGA", "RMC"]:
                continue
            # Strip checksum
            if "*" in line:
                line = line[: line.index("*")]
            parts = line.split(",")
            # RMC
            if sentence == "RMC":
                if len(parts) < 14:
                    continue
                try:
                    ddmmyy = parts[9]
                except (ValueError, IndexError):
                    continue
            # GGA
            elif sentence == "GGA":
                if len(parts) < 15:
                    continue
                try:
                    t_utc = datetime.strptime(ddmmyy + parts[1], "%d%m%y%H%M%S.%f")
                    t_utc = t_utc.replace(tzinfo=timezone.utc)
                    # Convert UTC to GPST (add leap seconds)
                    t = t_utc + timedelta(seconds=18)
                    lat = _nmea_dm_to_deg(parts[2], parts[3])
                    lon = _nmea_dm_to_deg(parts[4], parts[5])
                    if lat is None or lon is None:
                        continue
                    quality = int(parts[6]) if parts[6] else 0
                    ns = int(parts[7]) if parts[7] else 0
                    hdop = float(parts[8]) if parts[8] else 0.0
                    hgt = float(parts[9]) if parts[9] else 0.0
                    geoid = float(parts[11]) if parts[11] else 0.0
                    age = float(parts[13]) if parts[13] else 0.0
                    results.append(
                        {
                            "time_gpst": t,
                            "lat": lat,
                            "lon": lon,
                            "hgt": hgt + geoid,  # Convert to ellipsoidal height
                            "mode": gga_mode.get(quality, quality),
                            "ns": ns,
                            "hdop": hdop,
                            "age": age,
                        }
                    )
                except (ValueError, IndexError):
                    continue
    return results


# ---------------------------------------------------------------------------
# SBF parser
# ---------------------------------------------------------------------------
SBF_SYNC = b"\x24\x40"
SBF_PVTGEODETIC = 4007

# GPS epoch: 1980-01-06 00:00:00 UTC
_GPS_EPOCH = datetime(1980, 1, 6, tzinfo=timezone.utc)
# PVT mode bits[0:3]
_SBF_MODE = {0: 0, 1: 1, 2: 2, 3: 5, 4: 4, 5: 5, 6: 6, 10: 1}


def _gpst_to_datetime(wn, tow_ms):
    """Convert GPS week + TOW (ms) to datetime in GPST."""
    return _GPS_EPOCH + timedelta(weeks=wn, milliseconds=tow_ms)


def parse_sbf(path):
    """Parse PVTGeodetic (4007) blocks from an SBF file.

    Returns list of dicts: time_gpst (datetime, GPST), lat, lon, hgt (deg/m),
    mode (1=SPP,4=Fix,5=Float,...), ns (satellite count), hdop, age.

    Note: time_gpst is in GPS Time (not UTC). To convert to UTC, subtract
    the current leap seconds (18s as of 2025).
    """
    results = []
    data = Path(path).read_bytes()
    pos = 0

    while pos < len(data) - 8:
        idx = data.find(SBF_SYNC, pos)
        if idx < 0:
            break
        if idx + 8 > len(data):
            break
        blk_len = struct.unpack_from("<H", data, idx + 6)[0]
        if blk_len < 8 or blk_len > 65535:
            pos = idx + 2
            continue
        if idx + blk_len > len(data):
            break
        blk_id = struct.unpack_from("<H", data, idx + 4)[0] & 0x1FFF

        if blk_id == SBF_PVTGEODETIC and blk_len >= 44:
            p = data[idx + 8 :]
            tow_ms = struct.unpack_from("<I", p, 0)[0]
            wn = struct.unpack_from("<H", p, 4)[0]
            mode_raw = p[6] & 0x0F
            error = p[7]

            if mode_raw != 0 and error == 0 and tow_ms != 0xFFFFFFFF and wn != 0xFFFF:
                lat_rad, lon_rad, hgt = struct.unpack_from("<ddd", p, 8)
                if lat_rad > -2.0e10 and lon_rad > -2.0e10 and hgt > -2.0e10:
                    t = _gpst_to_datetime(wn, tow_ms)

                    # NrSV at offset 66, MeanCorrAge at offset 70 (u2, 0.01s)
                    ns = p[66] if len(p) > 66 else 0
                    age_raw = struct.unpack_from("<H", p, 70)[0] if len(p) > 72 else 0xFFFF
                    age = age_raw * 0.01 if age_raw != 0xFFFF else -1.0
                    # PVTGeodetic doesn't have DOP; use -1 as placeholder
                    hdop = -1.0

                    results.append(
                        {
                            "time_gpst": t,
                            "lat": math.degrees(lat_rad),
                            "lon": math.degrees(lon_rad),
                            "hgt": hgt,
                            "mode": _SBF_MODE.get(mode_raw, mode_raw),
                            "ns": ns,
                            "hdop": hdop,
                            "age": age,
                        }
                    )
        pos = idx + max(blk_len, 2)

    return results


# ---------------------------------------------------------------------------
# ENU conversion
# ---------------------------------------------------------------------------
def to_enu(pts, ref=None):
    """Convert lat/lon/hgt to ENU (meters) relative to a reference.

    Args:
        pts: list of dicts from parse_nmea/parse_sbf
        ref: (lat_deg, lon_deg) or None (use first point)

    Returns list of dicts with added keys: e, n, u (meters).
    """
    if not pts:
        return pts
    if ref is None:
        ref = (pts[0]["lat"], pts[0]["lon"])

    lat0 = math.radians(ref[0])
    lon0 = math.radians(ref[1])
    slat = math.sin(lat0)
    clat = math.cos(lat0)
    rn = WGS84_A / math.sqrt(1.0 - WGS84_E2 * slat * slat)
    rm = rn * (1.0 - WGS84_E2) / (1.0 - WGS84_E2 * slat * slat)
    hgt0 = pts[0]["hgt"]

    result = []
    for p in pts:
        dlat = math.radians(p["lat"]) - lat0
        dlon = math.radians(p["lon"]) - lon0
        e = dlon * rn * clat
        n = dlat * rm
        u = p["hgt"] - hgt0
        result.append({**p, "e": e, "n": n, "u": u})
    return result


def xyz2blh(posxyz: npt.NDArray) -> npt.NDArray:
    """xyz(ecef) to blh"""
    posblh = np.array([0.0, 0.0, -WGS84_A])

    if posblh[0] == 0.0 and posblh[1] == 0.0 and posblh[2] == 0.0:
        return posblh

    h = WGS84_A**2 - WGS84_B**2
    p = np.sqrt(posxyz[0] ** 2 + posxyz[1] ** 2)
    t = np.arctan2(posxyz[2] * WGS84_A, p * WGS84_B)

    posblh[0] = np.arctan2(
        posxyz[2] + h / WGS84_B * np.sin(t) ** 3, p - h / WGS84_A * np.cos(t) ** 3
    )
    n = WGS84_A / np.sqrt(1.0 - WGS84_E2 * np.sin(posblh[0]) ** 2)
    posblh[1] = np.arctan2(posxyz[1], posxyz[0])
    posblh[2] = (p / np.cos(posblh[0])) - n

    return posblh


def blh2xyz(posblh: npt.NDArray) -> npt.NDArray:
    """Blh to xyz(ecef)"""
    n = WGS84_A / np.sqrt(1.0 - WGS84_E2 * np.sin(posblh[0]) ** 2)
    x = (n + posblh[2]) * np.cos(posblh[0]) * np.cos(posblh[1])
    y = (n + posblh[2]) * np.cos(posblh[0]) * np.sin(posblh[1])
    z = (n * (1.0 - WGS84_E2) + posblh[2]) * np.sin(posblh[0])

    return np.array([x, y, z])


def xyz2enu(posxyz_rover: npt.NDArray, posxyz_base: npt.NDArray) -> npt.NDArray:
    """xyz(ecef) to enu"""
    posxyz_rover = posxyz_rover - posxyz_base

    posblh = xyz2blh(posxyz_base)
    s1 = np.sin(posblh[1])
    c1 = np.cos(posblh[1])
    s2 = np.sin(posblh[0])
    c2 = np.cos(posblh[0])

    e = -posxyz_rover[0] * s1 + posxyz_rover[1] * c1
    n = -posxyz_rover[0] * c1 * s2 - posxyz_rover[1] * s1 * s2 + posxyz_rover[2] * c2
    u = posxyz_rover[0] * c1 * c2 + posxyz_rover[1] * s1 * c2 + posxyz_rover[2] * s2

    return np.array([e, n, u])


def enu2xyz(posenu_rover: npt.NDArray, posxyz_base: npt.NDArray) -> npt.NDArray:
    """Enu to xyz(ecef)"""
    posblh = xyz2blh(posxyz_base)
    s1 = np.sin(posblh[1])
    c1 = np.cos(posblh[1])
    s2 = np.sin(posblh[0])
    c2 = np.cos(posblh[0])

    posxyz_rover = np.zeros(3)
    posxyz_rover[0] = -posenu_rover[0] * s1 - posenu_rover[1] * c1 * s2 + posenu_rover[2] * c1 * c2
    posxyz_rover[1] = posenu_rover[0] * c1 - posenu_rover[1] * s1 * s2 + posenu_rover[2] * s1 * c2
    posxyz_rover[2] = posenu_rover[1] * c2 + posenu_rover[2] * s2

    posxyz_rover = posxyz_rover + posxyz_base

    return posxyz_rover


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def _detect_format(path):
    """Detect file format from extension or content."""
    p = Path(path)
    ext = p.suffix.lower()
    if ext in (".nmea", ".pos"):
        return "nmea"
    if ext == ".sbf":
        return "sbf"
    # Peek at content
    with open(path, "rb") as f:
        head = f.read(4)
    if head[:2] == SBF_SYNC:
        return "sbf"
    return "nmea"


def main():
    if len(sys.argv) < 2:
        print("Usage: parse_pvt.py <file1> [file2] ...")
        print("Supported: .nmea, .sbf")
        sys.exit(1)

    for path in sys.argv[1:]:
        fmt = _detect_format(path)
        if fmt == "sbf":
            pts = parse_sbf(path)
        else:
            pts = parse_nmea(path)

        print("=== %s (%s) ===" % (path, fmt))
        print("  Points: %d" % len(pts))

        if pts:
            modes = {}
            for p in pts:
                modes[p["mode"]] = modes.get(p["mode"], 0) + 1
            mode_names = {0: "NoFix", 1: "SPP", 2: "DGPS", 4: "Fix", 5: "Float", 6: "PPP"}
            for k, v in sorted(modes.items()):
                print("  %s: %d" % (mode_names.get(k, "mode%d" % k), v))

            enu = to_enu(pts)
            es = [p["e"] for p in enu]
            ns = [p["n"] for p in enu]
            print("  E range: %.3f .. %.3f m (span=%.3f)" % (min(es), max(es), max(es) - min(es)))
            print("  N range: %.3f .. %.3f m (span=%.3f)" % (min(ns), max(ns), max(ns) - min(ns)))
            print("  Time: %s .. %s" % (pts[0]["time_gpst"], pts[-1]["time_gpst"]))


if __name__ == "__main__":
    main()
