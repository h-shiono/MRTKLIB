#!/usr/bin/env python3
"""Generate a .tag file for L6 (CLAS/MADOCA) raw data files.

L6 data is transmitted at 2000 bps = 250 bytes/s (one 250-byte frame per second).
This script creates a time-tag file so that rtkrcv can replay L6 data at the
correct rate, synchronized with observation data.

The base time is derived from the L6 filename convention:
    {year}{doy}{session}.l6         (CLAS L6D)
    {year}{doy}{session}.{prn}.l6   (MADOCA L6E)

Session letters map to UTC start hours:
    A=00, B=01, C=02, ..., X=23

When --sync-tag is specified, tick_f is adjusted so that replay timing aligns
with the master stream's tag file.

Usage:
    python gen_l6_tag.py <l6_file> --sync-tag <master.tag>

Examples:
    python gen_l6_tag.py tests/data/malib/data/2024235L.l6 \\
        --sync-tag tests/data/malib/data/MALIB_OSS_data_obsnav_240822-1100.sbf.tag
"""

import argparse
import os
import re
import struct
import sys
from datetime import datetime, timedelta, timezone


L6_FRAME_SIZE = 250  # bytes per frame
L6_FRAME_INTERVAL = 1.0  # seconds between frames
GPS_EPOCH_UNIX = 315964800  # Unix timestamp of GPS epoch (1980-01-06T00:00:00Z)

# GPS-UTC leap seconds table: (UTC datetime, cumulative leap seconds)
# Only entries from GPS epoch onward; kept in sync with IERS Bulletin C.
_LEAP_SECONDS = [
    (datetime(1981, 7, 1, tzinfo=timezone.utc), 1),
    (datetime(1982, 7, 1, tzinfo=timezone.utc), 2),
    (datetime(1983, 7, 1, tzinfo=timezone.utc), 3),
    (datetime(1985, 7, 1, tzinfo=timezone.utc), 4),
    (datetime(1988, 1, 1, tzinfo=timezone.utc), 5),
    (datetime(1990, 1, 1, tzinfo=timezone.utc), 6),
    (datetime(1991, 1, 1, tzinfo=timezone.utc), 7),
    (datetime(1992, 7, 1, tzinfo=timezone.utc), 8),
    (datetime(1993, 7, 1, tzinfo=timezone.utc), 9),
    (datetime(1994, 7, 1, tzinfo=timezone.utc), 10),
    (datetime(1996, 1, 1, tzinfo=timezone.utc), 11),
    (datetime(1997, 7, 1, tzinfo=timezone.utc), 12),
    (datetime(1999, 1, 1, tzinfo=timezone.utc), 13),
    (datetime(2006, 1, 1, tzinfo=timezone.utc), 14),
    (datetime(2009, 1, 1, tzinfo=timezone.utc), 15),
    (datetime(2012, 7, 1, tzinfo=timezone.utc), 16),
    (datetime(2015, 7, 1, tzinfo=timezone.utc), 17),
    (datetime(2017, 1, 1, tzinfo=timezone.utc), 18),
]


def utc2gpst_offset(utc_dt: datetime) -> int:
    """Return GPS-UTC leap second offset for a given UTC datetime."""
    offset = 0
    for dt, ls in _LEAP_SECONDS:
        if utc_dt >= dt:
            offset = ls
        else:
            break
    return offset


def parse_l6_filename(path: str) -> datetime:
    """Extract base time (UTC) from L6 filename.

    Returns datetime in UTC (no leap second conversion — tag stores GPST
    internally via utc2gpst in RTKLIB, but the time_t value in the tag header
    is the same epoch as the system clock at recording time).
    """
    basename = os.path.basename(path)
    m = re.match(r"(\d{4})(\d{3})([A-X])", basename)
    if not m:
        raise ValueError(f"Cannot parse L6 filename: {basename}")

    year = int(m.group(1))
    doy = int(m.group(2))
    session = m.group(3)
    hour = ord(session) - ord("A")

    dt = datetime(year, 1, 1, hour, 0, 0, tzinfo=timezone.utc) + timedelta(
        days=doy - 1
    )
    return dt


def read_tag_header(tag_path: str) -> tuple:
    """Read tick_f, time_time, time_sec from an existing .tag file.

    Returns (tick_f, base_time_unix).
    """
    with open(tag_path, "rb") as f:
        hdr = f.read(64)
        tick_f = struct.unpack_from("<I", hdr, 60)[0]
        time_time = struct.unpack("<I", f.read(4))[0]
        time_sec = struct.unpack("<d", f.read(8))[0]
    return tick_f, time_time + time_sec


def read_tag_time_range(tag_path: str) -> int:
    """Read the last tick_n entry from a tag file.

    Returns the maximum tick_n value (ms), representing the wall-clock
    span of the recording.  This is needed to match L6 tick_n scaling
    to the master tag's time scale (e.g. when the master was recorded
    at high speed rather than real-time).
    """
    file_size = os.path.getsize(tag_path)
    header_size = 64 + 4 + 8  # TIMETAGH + time_time + time_sec
    entry_size = 8  # tick_n(4) + fpos(4)
    n_entries = (file_size - header_size) // entry_size
    if n_entries < 1:
        return 0
    with open(tag_path, "rb") as f:
        # Seek to the last entry
        f.seek(header_size + (n_entries - 1) * entry_size)
        last_tick_n = struct.unpack("<I", f.read(4))[0]
    return last_tick_n


def gen_tag(l6_path: str, sync_tag: str = None) -> str:
    """Generate .tag file for L6 data.

    Args:
        l6_path:  Path to L6 data file.
        sync_tag: Path to master stream's .tag file for synchronization.

    Returns the output path.
    """
    file_size = os.path.getsize(l6_path)
    n_frames = file_size // L6_FRAME_SIZE
    remainder = file_size % L6_FRAME_SIZE

    if remainder != 0:
        print(
            f"Warning: file size {file_size} is not a multiple of "
            f"{L6_FRAME_SIZE} ({remainder} bytes extra)",
            file=sys.stderr,
        )

    # Parse base time from filename (UTC)
    base_utc = parse_l6_filename(l6_path)
    # Convert UTC to GPST basis to match gen_bnx_tag.py and RTKLIB convention.
    # RTKLIB stores wtime=utc2gpst(timeget()).time in the tag header,
    # so time_time = UTC_unix_timestamp + leap_seconds.
    leap = utc2gpst_offset(base_utc)
    time_time = int(base_utc.timestamp()) + leap
    time_sec = 0.0

    # Determine tick_f for sync
    # Determine tick_n scaling (ms per GNSS second)
    # Default: real-time (1 GNSS second = 1000 ms of tag time)
    tick_scale = 1000.0  # ms per GNSS second

    if sync_tag:
        master_tick_f, master_base = read_tag_header(sync_tag)
        master_tick_range = read_tag_time_range(sync_tag)

        # Time difference between master start and L6 start (seconds)
        dt_sec = time_time + time_sec - master_base
        # tick_f should be: master_tick_f + dt_sec * 1000
        # Because offset = master.tick_f - slave.tick_f
        # We want offset = -(dt_sec * 1000) so slave replays dt_sec later
        # => slave.tick_f = master.tick_f + dt_sec * 1000
        tick_f = int(master_tick_f + dt_sec * 1000)
        if tick_f < 0:
            tick_f = 0

        # Scale tick_n to match master's recording speed.
        # The master tag spans master_tick_range ms for some GNSS duration.
        # L6 must use the same ms-per-GNSS-second scale so that strsync
        # delivers L6 data at the same rate as the master.
        if master_tick_range > 0:
            # Approximate the master's GNSS duration (seconds) as
            # dt_sec (offset from master start to L6 start) + n_frames
            # (L6 session length).  This assumes the master covers at
            # least the full L6 session.
            master_gnss_dur = dt_sec + n_frames
            if master_gnss_dur > 0:
                tick_scale = master_tick_range / master_gnss_dur
                # Recompute tick_f with the matched scale
                tick_f = int(master_tick_f + dt_sec * tick_scale)
                if tick_f < 0:
                    tick_f = 0
                print(f"  Master tick range: {master_tick_range} ms for "
                      f"~{master_gnss_dur:.0f}s GNSS data")
                print(f"  Tick scale: {tick_scale:.3f} ms/s "
                      f"(vs 1000 ms/s real-time)")

        master_dt = datetime.fromtimestamp(master_base, tz=timezone.utc)
        print(f"Sync with master tag: {sync_tag}")
        print(f"  Master base time: {master_dt.strftime('%Y-%m-%d %H:%M:%S.%f')}")
        print(f"  Master tick_f: {master_tick_f}")
        print(f"  Time delta: {dt_sec:.1f} s")
        print(f"  L6 tick_f: {tick_f}")
        print(f"  Offset (master-slave): {master_tick_f - tick_f} ms")
    else:
        tick_f = 0
        print("Warning: no --sync-tag specified, tick_f=0 (no sync)")

    print(f"L6 file: {l6_path}")
    print(f"  Size: {file_size} bytes, {n_frames} frames")
    print(f"  Base time: {base_utc.strftime('%Y-%m-%d %H:%M:%S')} UTC")
    print(f"  Duration: {n_frames} seconds ({n_frames/60:.1f} min)")

    # Build tag file
    tag_path = l6_path + ".tag"

    with open(tag_path, "wb") as f:
        # Header: 64 bytes
        hdr = bytearray(64)
        label = b"TIMETAG MRTKLIB gen_l6_tag"
        hdr[: len(label)] = label
        struct.pack_into("<I", hdr, 60, tick_f & 0xFFFFFFFF)
        f.write(hdr)

        # time_time (uint32) + time_sec (double)
        f.write(struct.pack("<I", time_time & 0xFFFFFFFF))
        f.write(struct.pack("<d", time_sec))

        # Records: one per frame (1 frame = 250 bytes = 1 second)
        for i in range(n_frames):
            tick_ms = int(i * L6_FRAME_INTERVAL * tick_scale)
            fpos = i * L6_FRAME_SIZE
            f.write(struct.pack("<II", tick_ms, fpos))

        # Final record for EOF
        if remainder > 0:
            tick_ms = int(n_frames * L6_FRAME_INTERVAL * tick_scale)
            fpos = file_size
            f.write(struct.pack("<II", tick_ms, fpos))

    tag_size = os.path.getsize(tag_path)
    n_records = (tag_size - 76) // 8
    print(f"  Tag file: {tag_path} ({n_records} records)")

    return tag_path


def main():
    parser = argparse.ArgumentParser(
        description="Generate .tag file for L6 data replay"
    )
    parser.add_argument("l6_file", help="Path to L6 data file")
    parser.add_argument(
        "--sync-tag",
        help="Master stream .tag file to synchronize with",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.l6_file):
        print(f"Error: file not found: {args.l6_file}", file=sys.stderr)
        sys.exit(1)

    gen_tag(args.l6_file, args.sync_tag)


if __name__ == "__main__":
    main()
