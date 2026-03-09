#!/usr/bin/env python3
"""Generate a .tag file for BINEX observation files.

Parses the BINEX file to find observation record (0x7F-05) epoch boundaries
and creates a RTKLIB-compatible time-tag file for file stream replay.

The tag file enables rtkrcv to replay the BINEX data at controlled speed
using the ::T::xN suffix (e.g., file.bnx::T::x10 for 10x playback).

Usage:
    python gen_bnx_tag.py <bnx_file>

Example:
    python gen_bnx_tag.py tests/data/claslib/0627239Q.bnx
"""

import argparse
import os
import struct
import sys
from datetime import datetime, timezone

# GPS epoch: 1980-01-06 00:00:00 UTC
GPS_EPOCH_UNIX = 315964800  # Unix timestamp of GPS epoch
GPS_LEAP_2019 = 18  # GPS-UTC leap seconds in 2019


def getbnxi(data, offset):
    """Decode BINEX variable-length big-endian integer.

    Returns (value, bytes_consumed).
    """
    val = 0
    for i in range(3):
        if offset + i >= len(data):
            return None, 0
        b = data[offset + i]
        val = (val << 7) | (b & 0x7F)
        if not (b & 0x80):
            return val, i + 1
    if offset + 3 >= len(data):
        return None, 0
    val = (val << 8) | data[offset + 3]
    return val, 4


def scan_bnx_epochs(bnx_path):
    """Scan BINEX file for observation epoch records.

    Returns list of (file_offset, gps_time_seconds) tuples.
    gps_time_seconds is seconds since GPS epoch (1980-01-06).
    """
    with open(bnx_path, "rb") as f:
        data = f.read()

    epochs = []
    i = 0
    while i < len(data) - 8:
        # Look for sync byte 0xE2
        if data[i] != 0xE2:
            i += 1
            continue

        fpos = i

        # Record ID at byte 1
        if i + 1 >= len(data):
            break
        rec_id = data[i + 1]

        # Only interested in 0x7F (GNSS data)
        if rec_id != 0x7F:
            i += 2
            continue

        # Record length (variable-length integer at byte 2+)
        rec_len, len_bytes = getbnxi(data, i + 2)
        if rec_len is None or rec_len == 0:
            i += 2
            continue

        payload_start = i + 2 + len_bytes

        # Check we have enough data
        if payload_start + 7 > len(data):
            break

        # Subrecord ID (first byte of payload)
        srec = data[payload_start]
        if srec != 0x05:  # Only Trimble NetR8/R9 observations
            # Skip this record
            i = payload_start + rec_len + (2 if rec_len >= 128 else 1)
            continue

        # Extract timestamp: 4-byte minutes + 2-byte milliseconds (big-endian)
        minutes = struct.unpack(">I", data[payload_start + 1 : payload_start + 5])[0]
        msec = struct.unpack(">H", data[payload_start + 5 : payload_start + 7])[0]

        gps_seconds = minutes * 60.0 + msec * 0.001

        epochs.append((fpos, gps_seconds))

        # Advance past this record
        # Record: sync(1) + id(1) + len_header(len_bytes) + payload(rec_len) + checksum(1 or 2)
        checksum_size = 2 if rec_len >= 128 else 1
        i = payload_start + rec_len + checksum_size

    return epochs


def gen_tag(bnx_path):
    """Generate .tag file for BINEX data.

    Returns the output path.
    """
    print(f"Scanning BINEX file: {bnx_path}")
    epochs = scan_bnx_epochs(bnx_path)

    if not epochs:
        print("ERROR: no observation epochs found", file=sys.stderr)
        sys.exit(1)

    print(f"  Found {len(epochs)} observation epochs")

    # First and last epoch timestamps (GPS seconds since GPS epoch)
    t0_gps = epochs[0][1]
    t1_gps = epochs[-1][1]
    duration = t1_gps - t0_gps
    print(f"  GPS time range: {t0_gps:.3f} - {t1_gps:.3f} ({duration:.1f}s)")

    # Convert GPS seconds to Unix timestamp for tag header
    # tag stores time_time as Unix-like timestamp
    # RTKLIB: wtime = utc2gpst(timeget()), stores wtime.time
    # So tag base_time = GPS_EPOCH_UNIX + gps_seconds_since_epoch
    # But actually RTKLIB stores the GPS time as-if it were Unix time_t
    # i.e., time_time = GPS_EPOCH_UNIX + gps_seconds (including leap seconds)
    time_time = int(GPS_EPOCH_UNIX + t0_gps)
    time_sec = (GPS_EPOCH_UNIX + t0_gps) - time_time

    # tick_f: initial tick count (ms) — set to 0 for master stream
    tick_f = 0

    print(f"  Base time_time: {time_time}")
    print(f"  Duration: {duration:.1f}s ({duration/60:.1f} min)")

    # Build tag file
    tag_path = bnx_path + ".tag"
    file_size = os.path.getsize(bnx_path)

    with open(tag_path, "wb") as f:
        # Header: 64 bytes
        hdr = bytearray(64)
        label = b"TIMETAG MRTKLIB gen_bnx_tag"
        hdr[: len(label)] = label
        struct.pack_into("<I", hdr, 60, tick_f & 0xFFFFFFFF)
        f.write(hdr)

        # time_time (uint32) + time_sec (double)
        f.write(struct.pack("<I", time_time & 0xFFFFFFFF))
        f.write(struct.pack("<d", time_sec))

        # Records: one per epoch
        for fpos, gps_sec in epochs:
            tick_ms = int((gps_sec - t0_gps) * 1000)
            f.write(struct.pack("<II", tick_ms, fpos))

        # Final EOF record
        tick_ms = int(duration * 1000)
        f.write(struct.pack("<II", tick_ms, file_size))

    tag_size = os.path.getsize(tag_path)
    n_records = (tag_size - 76) // 8
    print(f"  Tag file: {tag_path} ({n_records} records)")

    return tag_path


def main():
    parser = argparse.ArgumentParser(
        description="Generate .tag file for BINEX observation data replay"
    )
    parser.add_argument("bnx_file", help="Path to BINEX observation file")
    args = parser.parse_args()

    if not os.path.isfile(args.bnx_file):
        print(f"Error: file not found: {args.bnx_file}", file=sys.stderr)
        sys.exit(1)

    gen_tag(args.bnx_file)


if __name__ == "__main__":
    main()
