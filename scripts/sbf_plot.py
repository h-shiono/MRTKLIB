#!/usr/bin/env python3
"""sbf_plot.py — Real-time SBF position plotter.

Connects to a TCP server streaming Septentrio SBF data (e.g. via
``mrtk relay``), decodes PVTGeodetic (block 4007), and plots
position/quality in real time using matplotlib.

Usage:
    python3 scripts/sbf_plot.py [--host HOST] [--port PORT]
                                [--ref LAT,LON] [--max-points N]

Example:
    # Terminal 1: relay SBF from serial to TCP
    mrtk relay -in serial://ttyUSB0:115200 -out tcpsvr://:30001

    # Terminal 2: plot
    python3 scripts/sbf_plot.py --port 30001 --ref 35.3231,139.5221
"""

from __future__ import annotations

import argparse
import math
import queue
import socket
import struct
import sys
import threading
import time
from collections import deque

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# ---------------------------------------------------------------------------
# SBF constants
# ---------------------------------------------------------------------------
SBF_SYNC = b"\x24\x40"  # '$@'
SBF_HDR_LEN = 8  # sync(2) + CRC(2) + ID(2) + LEN(2)
SBF_PVTGEODETIC = 4007

# PVT mode bits[0:3] → label & colour
PVT_MODES = {
    0: ("NoFix", "gray"),
    1: ("SPP", "red"),
    2: ("DGPS", "orange"),
    3: ("Float", "orange"),
    4: ("Fix", "green"),
    5: ("Float", "orange"),
}


# ---------------------------------------------------------------------------
# Geodetic helpers
# ---------------------------------------------------------------------------
WGS84_A = 6378137.0
WGS84_F = 1.0 / 298.257223563
WGS84_E2 = 2 * WGS84_F - WGS84_F**2


def geodetic_to_enu(lat, lon, lat0, lon0):
    """Convert geodetic offset to local ENU (meters), ignoring height."""
    dlat = lat - lat0
    dlon = lon - lon0
    slat = math.sin(lat0)
    clat = math.cos(lat0)
    rn = WGS84_A / math.sqrt(1.0 - WGS84_E2 * slat * slat)
    rm = rn * (1.0 - WGS84_E2) / (1.0 - WGS84_E2 * slat * slat)
    east = dlon * rn * clat
    north = dlat * rm
    return east, north


# ---------------------------------------------------------------------------
# SBF decoder
# ---------------------------------------------------------------------------
def decode_pvtgeodetic(block: bytes):
    """Decode PVTGeodetic (4007) block.  Returns dict or None."""
    if len(block) < 44:
        return None
    p = block[8:]  # skip SBF header to TOW
    # p+6 = Mode, p+7 = Error, p+8 = Lat(R8), p+16 = Lon(R8), p+24 = Hgt(R8)
    mode = p[6]
    error = p[7]
    pvt_type = mode & 0x0F
    if pvt_type == 0 or error != 0:
        return None
    lat, lon, hgt = struct.unpack_from("<ddd", p, 8)
    if lat < -2.0e10 or lon < -2.0e10 or hgt < -2.0e10:
        return None
    # TOW and week
    tow_ms = struct.unpack_from("<I", p, 0)[0]
    week = struct.unpack_from("<H", p, 4)[0]
    return {
        "tow": tow_ms * 0.001,
        "week": week,
        "lat": math.degrees(lat),
        "lon": math.degrees(lon),
        "hgt": hgt,
        "mode": pvt_type,
        "label": PVT_MODES.get(pvt_type, ("Other", "blue"))[0],
        "color": PVT_MODES.get(pvt_type, ("Other", "blue"))[1],
    }


# ---------------------------------------------------------------------------
# TCP SBF receiver thread
# ---------------------------------------------------------------------------
class SbfReceiver(threading.Thread):
    """Connect to TCP SBF server and push decoded PVT dicts to a queue."""

    def __init__(self, host: str, port: int, q: queue.Queue):
        super().__init__(daemon=True)
        self.host = host
        self.port = port
        self.q = q
        self._stop_evt = threading.Event()

    def stop(self):
        self._stop_evt.set()

    def run(self):
        while not self._stop_evt.is_set():
            try:
                self._connect_and_read()
            except (OSError, ConnectionError) as e:
                print(f"[sbf_plot] connection error: {e}, retrying in 2s...")
                time.sleep(2)

    def _connect_and_read(self):
        with socket.create_connection((self.host, self.port), timeout=5) as sock:
            sock.settimeout(2.0)
            print(f"[sbf_plot] connected to {self.host}:{self.port}")
            buf = bytearray()
            while not self._stop_evt.is_set():
                try:
                    data = sock.recv(4096)
                except socket.timeout:
                    continue
                if not data:
                    break
                buf.extend(data)
                self._parse(buf)

    def _parse(self, buf: bytearray):
        while len(buf) >= SBF_HDR_LEN:
            # find sync
            idx = buf.find(SBF_SYNC)
            if idx < 0:
                buf.clear()
                return
            if idx > 0:
                del buf[:idx]
            if len(buf) < SBF_HDR_LEN:
                return
            # read block length
            blk_len = struct.unpack_from("<H", buf, 6)[0]
            if blk_len < 8 or blk_len > 65535:
                del buf[:2]
                continue
            if len(buf) < blk_len:
                return  # wait for more data
            # extract block
            block = bytes(buf[:blk_len])
            del buf[:blk_len]
            # check block ID
            blk_id = struct.unpack_from("<H", block, 4)[0] & 0x1FFF
            if blk_id == SBF_PVTGEODETIC:
                pvt = decode_pvtgeodetic(block)
                if pvt:
                    self.q.put(pvt)


# ---------------------------------------------------------------------------
# Real-time plotter
# ---------------------------------------------------------------------------
class SbfPlotter:
    def __init__(self, ref=None, max_points=3600):
        self.ref = ref  # (lat_deg, lon_deg) or None
        self.auto_ref = None  # auto-set from first point if ref not given
        self.max_points = max_points
        self.pts = deque(maxlen=max_points)  # list of (x, y, color, mode)
        self.q = queue.Queue()

        self.fig, self.ax = plt.subplots(figsize=(8, 8))
        self.ax.set_title("SBF PVT — waiting for data...")

    def start(self, host, port):
        self.receiver = SbfReceiver(host, port, self.q)
        self.receiver.start()
        self.ani = FuncAnimation(self.fig, self._update, interval=200, cache_frame_data=False)
        plt.show()
        self.receiver.stop()

    def _get_ref(self):
        """Return reference (lat_deg, lon_deg) — explicit or auto."""
        return self.ref or self.auto_ref

    def _update(self, _frame):
        changed = False
        while not self.q.empty():
            try:
                pvt = self.q.get_nowait()
            except queue.Empty:
                break
            # auto-set reference from first point
            if not self.ref and not self.auto_ref:
                self.auto_ref = (pvt["lat"], pvt["lon"])
                self.ax.plot(0, 0, "k+", markersize=12)
            ref = self._get_ref()
            x, y = geodetic_to_enu(
                math.radians(pvt["lat"]),
                math.radians(pvt["lon"]),
                math.radians(ref[0]),
                math.radians(ref[1]),
            )
            self.pts.append((x, y, pvt["color"], pvt["mode"]))
            changed = True

        if not changed or not self.pts:
            return []

        xs = [p[0] for p in self.pts]
        ys = [p[1] for p in self.pts]
        cs = [p[2] for p in self.pts]

        # redraw scatter each frame (robust across matplotlib versions)
        self.ax.clear()
        self.ax.scatter(xs, ys, s=8, c=cs, edgecolors="none")
        if self._get_ref():
            self.ax.plot(0, 0, "k+", markersize=12, markeredgewidth=2)
        self.ax.set_xlabel("East (m)")
        self.ax.set_ylabel("North (m)")
        self.ax.set_aspect("equal")
        self.ax.grid(True, alpha=0.3)

        # auto-scale with margin
        xmin, xmax = min(xs), max(xs)
        ymin, ymax = min(ys), max(ys)
        dx = max(xmax - xmin, 0.1) * 0.1
        dy = max(ymax - ymin, 0.1) * 0.1
        margin = max(dx, dy, 0.5)
        self.ax.set_xlim(xmin - margin, xmax + margin)
        self.ax.set_ylim(ymin - margin, ymax + margin)

        # stats
        modes = [p[3] for p in self.pts]
        n_fix = sum(1 for m in modes if m == 4)
        n_float = sum(1 for m in modes if m in (3, 5))
        n_spp = sum(1 for m in modes if m == 1)
        total = len(modes)
        fix_pct = 100.0 * n_fix / total if total > 0 else 0.0
        last = self.pts[-1]
        self.ax.set_title(
            f"SBF PVT — {total} pts | "
            f"Fix:{n_fix}({fix_pct:.1f}%) Float:{n_float} SPP:{n_spp} | "
            f"Last: {PVT_MODES.get(last[3], ('?', ''))[0]}"
        )

        return []


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Real-time SBF position plotter")
    parser.add_argument("--host", default="localhost", help="TCP host")
    parser.add_argument("--port", type=int, default=30001, help="TCP port")
    parser.add_argument(
        "--ref",
        type=str,
        default=None,
        help="Reference position LAT,LON in degrees (e.g. 35.3231,139.5221)",
    )
    parser.add_argument(
        "--max-points",
        type=int,
        default=3600,
        help="Max points to display (default: 3600)",
    )
    args = parser.parse_args()

    ref = None
    if args.ref:
        parts = args.ref.split(",")
        if len(parts) != 2:
            print("Error: --ref must be LAT,LON (e.g. 35.3231,139.5221)")
            sys.exit(1)
        ref = (float(parts[0]), float(parts[1]))

    plotter = SbfPlotter(ref=ref, max_points=args.max_points)
    plotter.start(args.host, args.port)


if __name__ == "__main__":
    main()
