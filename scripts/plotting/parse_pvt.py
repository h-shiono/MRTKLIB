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
from pathlib import Path

# ---------------------------------------------------------------------------
# WGS84 constants
# ---------------------------------------------------------------------------
WGS84_A = 6378137.0
WGS84_F = 1.0 / 298.257223563
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
        for line in f:
            line = line.strip()
            if not line.startswith("$") or "GGA" not in line:
                continue
            # Strip checksum
            if "*" in line:
                line = line[: line.index("*")]
            parts = line.split(",")
            if len(parts) < 15:
                continue
            try:
                t = parts[1]
                if len(t) >= 6:
                    time_str = "%s:%s:%s" % (t[0:2], t[2:4], t[4:])
                else:
                    time_str = t
                lat = _nmea_dm_to_deg(parts[2], parts[3])
                lon = _nmea_dm_to_deg(parts[4], parts[5])
                if lat is None or lon is None:
                    continue
                quality = int(parts[6]) if parts[6] else 0
                ns = int(parts[7]) if parts[7] else 0
                hgt = float(parts[9]) if parts[9] else 0.0
                results.append(
                    {
                        "time_gpst": time_str,
                        "lat": lat,
                        "lon": lon,
                        "hgt": hgt,
                        "mode": gga_mode.get(quality, quality),
                        "ns": ns,
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

# PVT mode bits[0:3]
_SBF_MODE = {0: 0, 1: 1, 2: 2, 3: 5, 4: 4, 5: 5, 6: 6, 10: 1}


def parse_sbf(path):
    """Parse PVTGeodetic (4007) blocks from an SBF file.

    Returns list of dicts: time_gpst (str HH:MM:SS.S), lat, lon, hgt (deg/m),
    mode (1=SPP,4=Fix,5=Float,...), ns (satellite count).
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

            if mode_raw != 0 and error == 0:
                lat_rad, lon_rad, hgt = struct.unpack_from("<ddd", p, 8)
                if lat_rad > -2.0e10 and lon_rad > -2.0e10 and hgt > -2.0e10:
                    tow_s = tow_ms * 0.001
                    h = int(tow_s // 3600) % 24
                    m = int((tow_s % 3600) // 60)
                    s = tow_s % 60
                    time_str = "%02d:%02d:%04.1f" % (h, m, s)

                    # NrSV at offset 66 if block is long enough
                    ns = p[66] if len(p) > 66 else 0

                    results.append(
                        {
                            "time_gpst": time_str,
                            "lat": math.degrees(lat_rad),
                            "lon": math.degrees(lon_rad),
                            "hgt": hgt,
                            "mode": _SBF_MODE.get(mode_raw, mode_raw),
                            "ns": ns,
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
            print(
                "  E range: %.3f .. %.3f m (span=%.3f)"
                % (min(es), max(es), max(es) - min(es))
            )
            print(
                "  N range: %.3f .. %.3f m (span=%.3f)"
                % (min(ns), max(ns), max(ns) - min(ns))
            )
            print("  Time: %s .. %s" % (pts[0]["time_gpst"], pts[-1]["time_gpst"]))


if __name__ == "__main__":
    main()
