#!/usr/bin/env python3
"""Build the CLAS L6 frame-gap regression dataset (#363).

Slices 20 minutes of a live mosaic-G5 SBF capture (DOY 2026-296, hour u) in
which the receiver dropped its output buffer every few minutes: whole SBF
blocks, QZSRawL6D frames included, are missing for 1-2 s. A frame lost inside
a CLAS subframe shifted the bits of the following frames and the decoder read
garbage corrections from them (#363), so the window has the metre-level Float
excursions of the live run. The slice keeps the observation / navigation
blocks of the handover dataset plus QZSRawL6D (4270, all tracked PRNs), and
gets a synthetic ``.tag`` (``make_handover_fixtures.write_tag``) for ``mrtk
run`` file replay. Both are packed into ``clas_l6gap-v<N>.tar.gz`` (#386)
with ``make_dataset_asset.py``; nothing large is committed.

Prints, per L6 PRN, the frames kept, the seconds without a frame and the
subframes with a frame missing inside them, for tests/data/clas_l6gap/README.md.

Usage (from the repository root):
    make_clas_l6gap_fixture.py CAPTURE.sbf OUTDIR [--version N]
"""

import argparse
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_dataset_asset  # noqa: E402
import make_handover_fixtures as mh  # noqa: E402
import sbf_filter  # noqa: E402

WINDOW_TOW_S = (504300, 505500)  # GPST, week 2441 (2026-10-23 20:05..20:25)
SLICE = "G5P3296u_20m.sbf"
L6D_BLOCK = 4270


def l6_frames(data):
    """{header PRN: [(tow_s, sf_ind)]} of QZSRawL6D blocks the decoder accepts
    (valid block CRC, L6 parity flag set). The 250-byte frame is stored as
    little-endian words from block offset 20, so frame byte 4 (PRN) is block
    byte 27 and frame byte 5 (subframe indicator in its LSB) is block byte 26."""
    out, i, n = {}, 0, len(data)
    while True:
        i = data.find(b"$@", i)
        if i < 0 or i + 8 > n:
            return out
        bid = struct.unpack_from("<H", data, i + 4)[0] & 0x1FFF
        length = struct.unpack_from("<H", data, i + 6)[0]
        if length < 8 or length % 4 or i + length > n:
            i += 1
            continue
        blk = data[i : i + length]
        if bid == L6D_BLOCK and length >= 272 and blk[15] and mh.sbf_crc_ok(blk):
            tow = struct.unpack_from("<I", blk, 8)[0] / 1000.0
            out.setdefault(blk[27], []).append((tow, blk[26] & 1))
        i += length


def gap_stats(frames):
    """(seconds without a frame, subframes with a frame missing inside them)."""
    t = [f[0] for f in frames]
    missing = int(round(t[-1] - t[0])) + 1 - len(t)
    gapped, head, k = 0, None, 0
    for j, (tow, sf) in enumerate(frames):
        if sf:
            head, k = tow, 1
            continue
        if head is None:
            continue
        k += 1
        if abs(tow - frames[j - 1][0] - 1.0) > 0.5 or tow - head > 4.5:
            gapped += 1
            head = None
    return missing, gapped


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("capture", help="live capture of DOY 296 hour u (rovr_296u.sbf)")
    ap.add_argument("outdir", help="output directory for the slice, its tag and the asset")
    ap.add_argument("--version", type=int, default=1)
    args = ap.parse_args()

    with open(args.capture, "rb") as fp:
        raw = fp.read()
    blocks = sbf_filter.parse_blocks(f"{mh.OBS_NAV_BLOCKS},{L6D_BLOCK}")
    start, end = (t * 1000 for t in WINDOW_TOW_S)
    data, st = sbf_filter.filter_sbf(raw, blocks, start, end)

    slices = os.path.join(args.outdir, "slices")
    os.makedirs(slices, exist_ok=True)
    path = os.path.join(slices, SLICE)
    with open(path, "wb") as fp:
        fp.write(data)
    mh.write_tag(path + ".tag", st["records"], st["wnc"])

    print(f"{SLICE}: {len(data)} bytes, week {st['wnc']}, TOW {WINDOW_TOW_S[0]}-{WINDOW_TOW_S[1]}")
    print(f"kept blocks: {dict(sorted(st['kept'].items()))}")
    for prn, frames in sorted(l6_frames(data).items()):
        missing, gapped = gap_stats(frames)
        print(
            f"L6D PRN {prn}: {len(frames)} frames, {missing} s without a frame, {gapped} subframes gapped"
        )

    name = f"clas_l6gap-v{args.version}"
    asset = make_dataset_asset.build(name, args.outdir, [path, path + ".tag"])
    with open(asset, "rb") as fp:
        sha = hashlib.sha256(fp.read()).hexdigest()
    print(f"asset {asset} ({os.path.getsize(asset)} bytes) sha256 {sha}")
    print(f"mrtk_dataset({name} SHA256 {sha})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
