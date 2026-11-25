"""prepare_gici.py - Build the RTK benchmark inputs of a GICI open-dataset sequence.

GICI (https://github.com/chichengcn/gici-open-dataset) is downloaded by hand
from its OneDrive / BaiduCloud folders, one zip per sequence (``3.1.zip``,
...), almost all of it camera images.  This script reads the four files the
benchmark uses straight from the zip (or from an extracted sequence folder):

    <seq>/gnss_rover.bin      Tersus David30 rover, NovAtel OEM binary, 10 Hz
    <seq>/gnss_reference.bin  Qianxun network VRS base, RTCM3 MSM4 + 1005, 1 Hz
    <seq>/gnss_ephemeris.bin  broadcast ephemerides, RTCM3
    <seq>/ground_truth.txt    Inertial Explorer GNSS / FOG-IMU solution, 100 Hz

and writes ``<dataset-dir>/gici/<seq>/{rover.obs, base.obs, base.nav,
reference.csv}``.  The ``.bin`` files are converted with ``mrtk convert``,
after BeiDou 1042 messages whose toe lies outside the week are rewritten
(``fix_bds_ephemeris``; a defect of the recorded stream, not of the decoder).

The ground truth is the position of the IMU, not of the GNSS antenna.
``reference.csv`` holds the antenna position: the IMU position plus
``LEVER_RFU`` (right, forward, up) turned by the reference heading, on the
rover's 0.1 s grid and clipped to the span of the rover observations (the
ground truth runs on after the GNSS recordings of some sequences end).  Its
first five columns follow the PPC-Dataset ``reference.csv``; the sixth is the
heading.  The data are not redistributed by MRTKLIB.

Usage
-----
    python prepare_gici.py [--dataset-dir data/benchmark] [--mrtk PATH] SEQ.zip|SEQ_DIR ...
"""

import argparse
import math
import shutil
import subprocess
import sys
import tempfile
import zipfile
from datetime import datetime, timedelta, timezone
from pathlib import Path

MEMBERS = ("gnss_rover.bin", "gnss_reference.bin", "gnss_ephemeris.bin", "ground_truth.txt")
OUTPUTS = ("rover.obs", "base.obs", "base.nav")

# GNSS antenna relative to the IMU in the vehicle frame (right, forward, up),
# metres.  Measured: the mean of fixed RTK solutions minus ground truth,
# turned into the vehicle frame by the reference heading, is
# (0.361, 0.274, -0.060) on sequence 1.1 (open sky, 1106 fixes of rtk.toml,
# std 4 mm), (0.364, 0.259, -0.061) on 3.1 (2997 fixes of rtk.toml, std
# 12 mm) and (0.361, 0.273, -0.060) on 5.2 (15891 fixes of demo5's
# trimble_urban settings, std 36 / 19 mm); the value is their mean.  The
# ground_truth.txt header gives x = -0.356, y = 0.278, z = 0.049 m
# "x-right, y-fwd, z-up" in the IMU frame of the solution, which is rolled by
# 180 degrees (left-forward-down, roll ~181 degrees in every sequence); turned
# by the full attitude it gives (0.355, 0.277, -0.057 ... -0.061), within
# about 1 cm of the measured value.  Only the heading is applied here: the
# pitch and roll terms change the offset by 1.2 cm on average.
LEVER_RFU = (0.362, 0.269, -0.060)

GRID = 0.1  # rover observation interval, s

_GPS_EPOCH = datetime(1980, 1, 6, tzinfo=timezone.utc)
_A = 6378137.0
_F = 1.0 / 298.257223563
_E2 = _F * (2.0 - _F)


def _llh2ecef(lat: float, lon: float, h: float) -> tuple[float, float, float]:
    la, lo = math.radians(lat), math.radians(lon)
    n = _A / math.sqrt(1.0 - _E2 * math.sin(la) ** 2)
    return (
        (n + h) * math.cos(la) * math.cos(lo),
        (n + h) * math.cos(la) * math.sin(lo),
        (n * (1.0 - _E2) + h) * math.sin(la),
    )


def _ecef2llh(x: float, y: float, z: float) -> tuple[float, float, float]:
    p = math.hypot(x, y)
    lat = math.atan2(z, p * (1.0 - _E2))
    for _ in range(10):
        n = _A / math.sqrt(1.0 - _E2 * math.sin(lat) ** 2)
        lat = math.atan2(z + _E2 * n * math.sin(lat), p)
    n = _A / math.sqrt(1.0 - _E2 * math.sin(lat) ** 2)
    return math.degrees(lat), math.degrees(math.atan2(y, x)), p / math.cos(lat) - n


def antenna_position(
    lat: float, lon: float, h: float, heading: float, lever=LEVER_RFU
) -> tuple[float, float, float]:
    """Shift an IMU position to the GNSS antenna.

    Args:
        lat: IMU latitude, degrees.
        lon: IMU longitude, degrees.
        h: IMU ellipsoidal height, m.
        heading: Vehicle heading, degrees clockwise from north.
        lever: Antenna offset (right, forward, up), m.

    Returns:
        ``(lat, lon, h)`` of the antenna.
    """
    r, f, u = lever
    s, c = math.sin(math.radians(heading)), math.cos(math.radians(heading))
    e, n = r * c + f * s, -r * s + f * c
    la, lo = math.radians(lat), math.radians(lon)
    x, y, z = _llh2ecef(lat, lon, h)
    x += -math.sin(lo) * e - math.sin(la) * math.cos(lo) * n + math.cos(la) * math.cos(lo) * u
    y += math.cos(lo) * e - math.sin(la) * math.sin(lo) * n + math.cos(la) * math.sin(lo) * u
    z += math.cos(la) * n + math.sin(la) * u
    return _ecef2llh(x, y, z)


def read_ground_truth(path: Path) -> list[tuple[int, float, float, float, float, float]]:
    """Read ``ground_truth.txt`` epochs on the rover grid.

    Returns:
        ``(week, tow, lat, lon, h, heading)`` per epoch whose time is a
        multiple of ``GRID``.
    """
    rows = []
    with open(path) as fh:
        for line in fh:
            p = line.split()
            if len(p) < 9 or not p[0].isdigit():
                continue
            tow = float(p[1])
            if abs(tow / GRID - round(tow / GRID)) > 1e-6:
                continue
            rows.append((int(p[0]), tow, float(p[4]), float(p[3]), float(p[5]), float(p[6])))
    return rows


def write_reference(rows, path: Path) -> None:
    """Write antenna positions in the PPC-Dataset ``reference.csv`` layout."""
    with open(path, "w") as fh:
        fh.write(
            "GPS TOW (s),GPS Week,Latitude (deg),Longitude (deg),"
            "Ellipsoid Height (m),Heading (deg)\n"
        )
        for week, tow, lat, lon, h, heading in rows:
            a_lat, a_lon, a_h = antenna_position(lat, lon, h, heading)
            fh.write(f"{tow:.1f},{week},{a_lat:.10f},{a_lon:.10f},{a_h:.4f},{heading:.4f}\n")


def _crc24q(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 16
        for _ in range(8):
            crc <<= 1
            if crc & 0x1000000:
                crc ^= 0x1864CFB
    return crc & 0xFFFFFF


def _getbits(buf: bytearray, pos: int, n: int) -> int:
    v = 0
    for k in range(pos, pos + n):
        v = (v << 1) | ((buf[k >> 3] >> (7 - (k & 7))) & 1)
    return v


def _setbits(buf: bytearray, pos: int, n: int, v: int) -> None:
    for k in range(pos + n - 1, pos - 1, -1):
        mask = 1 << (7 - (k & 7))
        buf[k >> 3] = (buf[k >> 3] | mask) if v & 1 else (buf[k >> 3] & ~mask)
        v >>= 1


# Bit offsets in a 1042 message body (after the 12-bit message number), and
# the body length the decoder requires.
_B1042_BITS = 12 + 499
_B1042_WEEK = 12 + 6
_B1042_TOC = _B1042_WEEK + 13 + 4 + 14 + 5
_B1042_TOE = _B1042_TOC + 17 + 11 + 22 + 24 + 5 + 18 + 16 + 32 + 18 + 32 + 18 + 32


def fix_bds_ephemeris(data: bytes) -> tuple[bytes, int]:
    """Bring RTCM3 1042 toe / toc that lie one week ahead back into the week.

    The Qianxun streams of sequences 4.1 and 5.2 (2023-05-21) send BeiDou
    ephemerides as BDT week 906 with toe = toc = 626400 or 630000 s instead of
    week 907, 21600 or 25200 s.  The 1042 field
    is 17 bits x 8 s, so the value fits the bits, but it is outside the range
    the standard allows (0-604792 s).  A decoder keeps it, and the orbit
    computation, which subtracts omega_e * toe, then turns every satellite by
    6.9 degrees (omega_e * 604800 s = 7.02 turns).  Rewrite such messages as
    week + 1, toe - 604800, toc - 604800 and recompute the CRC.

    Returns:
        The rewritten stream, the number of messages changed, and the number of
        messages left unchanged with only one of toe / toc out of range.
    """
    buf = bytearray(data)
    n_fixed = n_odd = i = 0
    while i + 6 <= len(buf):
        if buf[i] != 0xD3 or buf[i + 1] & 0xFC:
            i += 1
            continue
        length = ((buf[i + 1] & 3) << 8) | buf[i + 2]
        end = i + 3 + length
        if end + 3 > len(buf) or _crc24q(buf[i:end]) != int.from_bytes(buf[end : end + 3], "big"):
            i += 1
            continue
        body = i + 3
        if length * 8 >= _B1042_BITS and _getbits(buf, body * 8, 12) == 1042:
            toe = _getbits(buf, body * 8 + _B1042_TOE, 17) * 8
            toc = _getbits(buf, body * 8 + _B1042_TOC, 17) * 8
            if toe >= 604800 and toc >= 604800:
                week = _getbits(buf, body * 8 + _B1042_WEEK, 13)
                _setbits(buf, body * 8 + _B1042_WEEK, 13, (week + 1) % 8192)
                _setbits(buf, body * 8 + _B1042_TOE, 17, (toe - 604800) // 8)
                _setbits(buf, body * 8 + _B1042_TOC, 17, (toc - 604800) // 8)
                buf[end : end + 3] = _crc24q(buf[i:end]).to_bytes(3, "big")
                n_fixed += 1
            elif toe >= 604800 or toc >= 604800:
                n_odd += 1
        i = end + 3
    return bytes(buf), n_fixed, n_odd


def _has_records(path: Path) -> bool:
    """Return True if a RINEX file exists and has a line after its header."""
    if not path.is_file():
        return False
    with open(path) as fh:
        for line in fh:
            if "END OF HEADER" in line:
                return any(rec.strip() for rec in fh)
    return False


def _obs_span(path: Path) -> tuple[float, float]:
    """Return the GPS TOW of the first and last epoch of a RINEX 3 OBS file."""
    tows = []
    with open(path) as fh:
        for line in fh:
            if line.startswith("> "):
                y, mo, d, h, mi = (int(v) for v in line[2:].split()[:5])
                sec = float(line[2:].split()[5])
                t = datetime(y, mo, d, h, mi, tzinfo=timezone.utc) - _GPS_EPOCH
                tows.append((t.total_seconds() + sec) % 604800.0)
    return round(tows[0], 1), round(tows[-1], 1)


def _run(argv: list[str]) -> None:
    r = subprocess.run(argv, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode != 0:
        sys.exit(f"FAIL: {' '.join(argv)}\n{r.stderr[-2000:]}")


def _gpst_string(week: int, tow: float) -> str:
    t = _GPS_EPOCH + timedelta(weeks=week, seconds=tow)
    return t.strftime("%Y/%m/%d %H:%M:%S")


def prepare(src: Path, dataset_dir: Path, mrtk: str) -> Path:
    """Build one sequence's benchmark inputs from its zip or folder.

    Args:
        src: ``<seq>.zip`` or an extracted ``<seq>`` folder.
        dataset_dir: Benchmark data root.
        mrtk: Path to the ``mrtk`` binary.

    Returns:
        The output folder ``<dataset_dir>/gici/<seq>``.
    """
    seq = src.stem if src.suffix == ".zip" else src.name
    out = dataset_dir / "gici" / seq
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        if src.suffix == ".zip":
            with zipfile.ZipFile(src) as zf:
                for name in MEMBERS:
                    if f"{seq}/{name}" not in zf.namelist():
                        sys.exit(f"FAIL: {src} has no {seq}/{name} (keep the zip's sequence name)")
                    with zf.open(f"{seq}/{name}") as fin, open(work / name, "wb") as fout:
                        shutil.copyfileobj(fin, fout)
        else:
            for name in MEMBERS:
                shutil.copy2(src / name, work / name)

        eph = work / "gnss_ephemeris.bin"
        fixed, n_fixed, n_odd = fix_bds_ephemeris(eph.read_bytes())
        if n_fixed:
            eph.write_bytes(fixed)
            print(f"{seq}: {n_fixed} BeiDou 1042 messages with toe beyond the week rewritten")
        if n_odd:
            print(
                f"{seq}: WARNING {n_odd} BeiDou 1042 messages with only toe or toc beyond the week"
            )

        gt = read_ground_truth(work / "ground_truth.txt")
        if not gt:
            sys.exit(f"FAIL: no ground-truth epochs in {src}")
        # RTCM3 MSM carries no week number; -tr resolves it from an approximate time.
        tr = _gpst_string(gt[0][0], gt[0][1]).split()
        conv = [mrtk, "convert"]
        _run(conv + ["-r", "nov", "-o", str(work / "rover.obs"), str(work / "gnss_rover.bin")])
        _run(
            conv
            + ["-r", "rtcm3", "-tr", *tr, "-o", str(work / "base.obs")]
            + [str(work / "gnss_reference.bin")]
        )
        _run(
            conv
            + ["-r", "rtcm3", "-tr", *tr, "-n", str(work / "base.nav")]
            + ["-o", str(work / "eph.obs"), str(work / "gnss_ephemeris.bin")]
        )
        # mrtk convert exits 0 and removes an output file it wrote no record to,
        # so check every input the benchmark needs before replacing the old ones.
        for name in OUTPUTS:
            if not _has_records(work / name):
                sys.exit(f"FAIL: {seq}: mrtk convert wrote no records to {name}")
        t_first, t_last = _obs_span(work / "rover.obs")
        gt = [r for r in gt if t_first - 1e-6 <= r[1] <= t_last + 1e-6]
        if not gt:
            sys.exit(f"FAIL: {seq}: no ground-truth epoch within the rover observations")
        write_reference(gt, work / "reference.csv")
        out.mkdir(parents=True, exist_ok=True)
        for name in (*OUTPUTS, "reference.csv"):
            shutil.move(work / name, out / name)
    week, t0, t1 = gt[0][0], gt[0][1], gt[-1][1]
    print(f"{seq}: {len(gt)} reference epochs, GPS week {week} TOW {t0:.1f}-{t1:.1f} -> {out}")
    return out


def main() -> int:
    """Entry point."""
    p = argparse.ArgumentParser(description="Build GICI sequence inputs for the RTK benchmark")
    p.add_argument("src", nargs="+", type=Path, help="Sequence zip (e.g. 3.1.zip) or folder")
    p.add_argument("--dataset-dir", default="data/benchmark", type=Path)
    p.add_argument("--mrtk", default="", help="mrtk binary (default: build/mrtk, then PATH)")
    args = p.parse_args()

    root = Path(__file__).resolve().parent.parent.parent
    mrtk = args.mrtk or (str(root / "build/mrtk") if (root / "build/mrtk").is_file() else "")
    mrtk = mrtk or shutil.which("mrtk") or ""
    if not mrtk:
        sys.exit("FAIL: mrtk binary not found; build it or pass --mrtk PATH")
    dataset_dir = args.dataset_dir if args.dataset_dir.is_absolute() else root / args.dataset_dir
    for src in args.src:
        prepare(src, dataset_dir, mrtk)
    return 0


if __name__ == "__main__":
    sys.exit(main())
