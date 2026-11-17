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

    # A subframe-indicator frame whose data part happens to start with the
    # ST1 bit pattern would be a false anchor: keep an anchor only when its
    # epoch - index agrees with a neighbour.
    keys = [tow - i for i, tow in anchors]
    if len(anchors) < 2:
        good = anchors
    else:
        good = [
            a
            for k, a in enumerate(anchors)
            if (k > 0 and keys[k] == keys[k - 1]) or (k + 1 < len(keys) and keys[k] == keys[k + 1])
        ]
    if not good:
        raise ValueError("no two ST1 frames agree on the frame timing")
    if len(good) != len(anchors):
        print(
            f"Warning: ignored {len(anchors) - len(good)} inconsistent ST1 frame(s)",
            file=sys.stderr,
        )
    anchors = good

    gps_ref = ref_gpst - GPS_EPOCH_UNIX
    week = round((gps_ref - anchors[0][1]) / WEEK_SEC)

    def to_gpst(i_anchor, tow, i):
        return GPS_EPOCH_UNIX + week * WEEK_SEC + tow + (i - i_anchor)

    times = []
    a = 0
    prev = None
    for i in range(len(data) // L6_FRAME_SIZE):
        while a + 1 < len(anchors) and anchors[a + 1][0] <= i:
            a += 1
        t = to_gpst(anchors[a][0], anchors[a][1], i)
        if prev is not None and t - prev < -WEEK_SEC / 2:  # TOW rolled over the week
            week += 1
            t += WEEK_SEC
        times.append(t)
        prev = t
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
        # A record (tick_n, fpos) releases the file up to fpos at tick_n.
        for i, tick in enumerate(ticks):
            f.write(struct.pack("<II", tick, (i + 1) * L6_FRAME_SIZE))
        if remainder:
            f.write(struct.pack("<II", ticks[-1] if ticks else 0, len(data)))

    def fmt(t):
        gps = t - GPS_EPOCH_UNIX
        return f"week {int(gps // WEEK_SEC)} tow {gps % WEEK_SEC:.1f}"

    print(f"L6 file: {l6_path} ({n_frames} frames)")
    print(f"  first frame epoch: {fmt(times[0])}, last: {fmt(times[-1])}")
    print(f"  master start: {fmt(master_start)} (tick_f {tick_f})")
    print(f"  frames released at replay start: {n_early}")
    print(f"  tag file: {tag_path} ({len(ticks) + (1 if remainder else 0)} records)")
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
