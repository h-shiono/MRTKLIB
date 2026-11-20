#!/usr/bin/env python3
r"""Generate a .tag file for a CLAS L6D raw frame file.

L6 is broadcast at 2000 bps: one 250-byte frame per second.  The tag lets
``mrtk run`` replay the L6 file in step with an observation file whose tag
(``--sync-tag``, e.g. from ``gen_bnx_tag.py``) is the master stream.

Frame times come from the content, not from the file name: every subframe
starts with a Compact SSR ST1 (message 4073, subtype 1) whose 20-bit GNSS
epoch time is the GPS time of week of that frame.  Frames between two ST1
frames are one second apart.  A receiver outputs the frame one second after
that epoch (end of reception; SBF QZSRawL6D TOW = ST1 epoch + 1 s), so frame
``i`` is released at ``epoch_i + 1 s``.

CLAS only: in MADOCA-PPP L6E files the ST1 epoch is not the frame time
(2025157U.204.l6, which starts at the hour, opens with epoch hh:00:05).

The L6 tag is written on the master's timeline: same header time and same
``tick_f`` as the master, so ``strsync()`` applies no offset, and
``tick_n = (release time - master start) * 1000``.  Frames released before
the master starts get ``tick_n = 0`` and are delivered at once when the replay
starts, as a receiver that was already running would have them.  This assumes
the master tag runs at 1000 ms per GNSS second, which holds for
``gen_bnx_tag.py`` output and for tags recorded in real time.

Usage:
    python gen_l6_tag.py <l6_file> --sync-tag <master.tag>

Example:
    python gen_l6_tag.py tests/data/claslib/2019239Q.l6 \
        --sync-tag tests/data/claslib/0627239Q.bnx.tag
"""

import argparse
import os
import struct
import sys

L6_FRAME_SIZE = 250  # bytes per frame
L6_PREAMBLE = b"\x1a\xcf\xfc\x1d"
L6_RELEASE_DELAY = 1.0  # s from the frame's GNSS epoch to its output
GPS_EPOCH_UNIX = 315964800  # tag header times are GPST seconds on the Unix scale
WEEK_SEC = 604800
MAX_GAP_SEC = 86400  # longest run of missing frames accepted between two ST1
TAG_HEADER_LEN = 64 + 4 + 8  # TIMETAGH + time_time + time_sec


def _bits(buf: bytes, pos: int, n: int) -> int:
    v = 0
    for k in range(pos, pos + n):
        v = (v << 1) | ((buf[k >> 3] >> (7 - (k & 7))) & 1)
    return v


def st1_anchors(data: bytes) -> list:
    """Return (frame index, ST1 epoch TOW) of every frame that opens a subframe.

    The data part starts at bit 49 (preamble 32, PRN 8, message type 8,
    alert 1); bit 0 of the message type is the subframe indicator.
    """
    anchors = []
    for i in range(len(data) // L6_FRAME_SIZE):
        fr = data[i * L6_FRAME_SIZE : (i + 1) * L6_FRAME_SIZE]
        if fr[:4] != L6_PREAMBLE or not (fr[5] & 1):
            continue
        if _bits(fr, 49, 12) == 4073 and _bits(fr, 61, 4) == 1:
            anchors.append((i, _bits(fr, 65, 20)))
    return anchors


def l6_frame_times(data: bytes, ref_gpst: float) -> list:
    """GPST (seconds on the tag-header scale) of each frame's GNSS epoch.

    Each frame is timed from the latest ST1 frame at or before it (frames
    before the first ST1 from the first one), so a frame dropped from the file
    does not shift the rest.  ``ref_gpst`` resolves the GPS week: the week that
    puts the first ST1 epoch nearest to it is used.
    """
    anchors = st1_anchors(data)
    if not anchors:
        raise ValueError("no CSSR ST1 frame found: cannot time the L6 file")

    # Full GPST of each anchor, unwrapping the 20-bit TOW across week rollovers.
    # Frames can be missing from the file (more seconds than frames between two
    # anchors), but never fewer seconds than frames: such an anchor is not a
    # real ST1 and is skipped. The modulo also turns a backward TOW into a jump
    # of almost a week, so more than MAX_GAP_SEC of missing frames is skipped
    # too: a stale or damaged ST1 must not delay every later frame by a week.
    week = round((ref_gpst - GPS_EPOCH_UNIX - anchors[0][1]) / WEEK_SEC)
    timed = [(anchors[0][0], GPS_EPOCH_UNIX + week * WEEK_SEC + anchors[0][1])]
    prev_tow = anchors[0][1]
    for i, tow in anchors[1:]:
        elapsed = (tow - prev_tow) % WEEK_SEC
        frames = i - timed[-1][0]
        t = timed[-1][1] + elapsed
        if elapsed < frames or elapsed > frames + MAX_GAP_SEC:
            print(
                f"Warning: ignored ST1 frame {i} (epoch {tow}): inconsistent timing",
                file=sys.stderr,
            )
            continue
        timed.append((i, t))
        prev_tow = tow

    times = []
    a = 0
    for i in range(len(data) // L6_FRAME_SIZE):
        while a + 1 < len(timed) and timed[a + 1][0] <= i:
            a += 1
        times.append(timed[a][1] + (i - timed[a][0]))
    return times


def read_tag_header(tag_path: str) -> tuple:
    """Return (tick_f, time_time, time_sec) of a .tag file."""
    with open(tag_path, "rb") as f:
        hdr = f.read(TAG_HEADER_LEN)
    tick_f = struct.unpack_from("<I", hdr, 60)[0]
    time_time = struct.unpack_from("<I", hdr, 64)[0]
    time_sec = struct.unpack_from("<d", hdr, 68)[0]
    return tick_f, time_time, time_sec


def gen_tag(l6_path: str, sync_tag: str) -> str:
    """Write ``<l6_path>.tag`` on the timeline of the master tag ``sync_tag``."""
    with open(l6_path, "rb") as f:
        data = f.read()
    n_frames = len(data) // L6_FRAME_SIZE
    remainder = len(data) % L6_FRAME_SIZE
    if remainder:
        print(
            f"Warning: file size {len(data)} is not a multiple of {L6_FRAME_SIZE} "
            f"({remainder} bytes extra)",
            file=sys.stderr,
        )

    tick_f, time_time, time_sec = read_tag_header(sync_tag)
    master_start = time_time + time_sec
    times = l6_frame_times(data, master_start)

    ticks = [max(0, round((t + L6_RELEASE_DELAY - master_start) * 1000.0)) for t in times]
    n_early = sum(1 for t in ticks if t == 0)

    tag_path = l6_path + ".tag"
    with open(tag_path, "wb") as f:
        hdr = bytearray(64)
        label = b"TIMETAG MRTKLIB gen_l6_tag"
        hdr[: len(label)] = label
        struct.pack_into("<I", hdr, 60, tick_f)
        f.write(hdr)
        f.write(struct.pack("<I", time_time))
        f.write(struct.pack("<d", time_sec))
        # readfile() makes the file readable up to the fpos of the first record
        # whose tick is still ahead, so record (r_i, start of frame i) releases
        # frame i at r_i; the last record releases the final frame.
        for i, tick in enumerate(ticks):
            f.write(struct.pack("<II", tick, i * L6_FRAME_SIZE))
        f.write(struct.pack("<II", (ticks[-1] if ticks else 0) + 1000, len(data)))

    def fmt(t):
        gps = t - GPS_EPOCH_UNIX
        return f"week {int(gps // WEEK_SEC)} tow {gps % WEEK_SEC:.1f}"

    print(f"L6 file: {l6_path} ({n_frames} frames)")
    print(f"  first frame epoch: {fmt(times[0])}, last: {fmt(times[-1])}")
    print(f"  master start: {fmt(master_start)} (tick_f {tick_f})")
    print(f"  frames released at replay start: {n_early}")
    print(f"  tag file: {tag_path} ({len(ticks) + 1} records)")
    return tag_path


def main():
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description="Generate a .tag file for L6 data replay")
    parser.add_argument("l6_file", help="L6 raw frame file (250-byte frames)")
    parser.add_argument(
        "--sync-tag", required=True, help="master stream .tag file (e.g. the BINEX obs tag)"
    )
    args = parser.parse_args()
    for p in (args.l6_file, args.sync_tag):
        if not os.path.isfile(p):
            sys.exit(f"Error: file not found: {p}")
    gen_tag(args.l6_file, args.sync_tag)


if __name__ == "__main__":
    main()
