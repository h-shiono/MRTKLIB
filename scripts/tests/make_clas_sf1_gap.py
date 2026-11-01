#!/usr/bin/env python3
"""Make a CLAS L6 replay in which subframe 1 of some 30 s frames is lost (#406).

CLAS sends the mask (ST1), orbit (ST2), clock (ST3) and global code bias (ST4)
in subframe 1 (the first five 1 s frames) of each 30 s frame. Losing it — the
receiver dropping its output buffer while the host stalls — left the engine
without a usable correction set for ~17 s after the next frame epoch.

This script zero-fills subframe 1 of every frame whose ST1 epoch is a multiple
of ``--period`` seconds, keeping the file size so the original ``.tag`` time
tags stay valid, and writes an ``mrtk run`` configuration that replays the
gapped L6 file from ``--offset`` seconds into the capture.

Usage:
    make_clas_sf1_gap.py --l6 IN.l6 --out-dir DIR --conf IN.toml \\
        [--period 300] [--offset 2400]
"""

import argparse
import shutil
import sys
from pathlib import Path

PREAMBLE = b"\x1a\xcf\xfc\x1d"
FRAME_LEN = 250
SUBFRAME_FRAMES = 5
CSSR_MSG_NUM = 4073
ST_MASK = 1


def getbits(buf: bytes, pos: int, n: int) -> int:
    v = 0
    for i in range(pos, pos + n):
        v = (v << 1) | ((buf[i >> 3] >> (7 - (i & 7))) & 1)
    return v


def blank_subframe1(data: bytearray, period: int) -> list:
    """Zero-fill subframe 1 of frames whose ST1 epoch % period == 0."""
    events = []
    pending = {}
    i = 0
    while True:
        i = data.find(PREAMBLE, i)
        if i < 0 or i + FRAME_LEN > len(data):
            break
        frame = data[i : i + FRAME_LEN]
        prn, sf_ind = frame[4], frame[5] & 1
        # CSSR data starts after the 49-bit L6 header (preamble, PRN, type, alert)
        if sf_ind and getbits(frame, 49, 12) == CSSR_MSG_NUM and getbits(frame, 61, 4) == ST_MASK:
            tow = getbits(frame, 65, 20)
            if tow % period == 0:
                pending[prn] = SUBFRAME_FRAMES
                events.append((prn, tow))
        if pending.get(prn, 0) > 0:
            data[i : i + FRAME_LEN] = bytes(FRAME_LEN)
            pending[prn] -= 1
        i += FRAME_LEN
    return events


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--l6", required=True, type=Path)
    ap.add_argument("--out-dir", required=True, type=Path)
    ap.add_argument("--conf", required=True, type=Path)
    ap.add_argument("--period", type=int, default=300)
    ap.add_argument("--offset", type=int, default=2400)
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    out_l6 = args.out_dir / (args.l6.stem + "_sf1gap.l6")
    data = bytearray(args.l6.read_bytes())
    events = blank_subframe1(data, args.period)
    if not events:
        print(f"ERROR: no subframe 1 blanked in {args.l6}", file=sys.stderr)
        return 1
    out_l6.write_bytes(data)
    shutil.copyfile(str(args.l6) + ".tag", str(out_l6) + ".tag")

    conf = args.conf.read_text()
    l6_path = f'"./tests/data/claslib/{args.l6.name}::T::x10"'
    if l6_path not in conf:
        print(f"ERROR: {l6_path} not in {args.conf}", file=sys.stderr)
        return 1
    conf = conf.replace(l6_path, f'"{out_l6}::T::x10"')
    conf = conf.replace("::T::x10", f"::T::+{args.offset}::x10")
    out_conf = args.out_dir / "rtkrcv_sf1gap.toml"
    out_conf.write_text(conf)

    print(f"blanked {len(events)} subframe-1 frames (PRN, ST1 tow): {events}")
    print(f"wrote {out_l6} and {out_conf}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
