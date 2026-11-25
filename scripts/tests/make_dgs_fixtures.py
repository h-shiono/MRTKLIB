#!/usr/bin/env python3
"""Build the DGS (DATAGNSS QZS-6C / MA-10P) L6 fixtures in ``tests/utest``.

A DGS frame is the Allystar binary message group 0x02 / id 0x10 with one
250-byte QZSS L6 frame, see ``src/data/rcv/mrtk_rcv_dgs.c``.

    l6route_ma10p.dgs    real slice of the L6 port of an MA-10P (2026-11-24,
                         GPS week 2446, TOW 197986..198016 s), copied verbatim:
                         L6D PRN 195 and 199 (CLAS, one frame per second each)
                         with the RTCM3 1058 messages the receiver interleaves.
                         The two frames of TOW 198000 s carry the Reed-Solomon
                         failure flag.
    l6route_dgs_l6e.dgs  L6E frames (PRN 205: MADOCA, QZNMA) of
                         l6route_ubx_l6e.ubx re-wrapped as DGS (the container
                         is synthesised, the L6 frame is real), mixed with
                         cases the extractor must survive: a sync sequence
                         followed by too few bytes right before a frame, a
                         frame with a corrupted checksum, and frames flagged
                         week / time of week not confirmed.

The script prints the frame counts that scripts/tests/check_l6extract.py and
tests/utest/utest_dgs.c assert.

Usage (from the repository root):
    python3 scripts/tests/make_dgs_fixtures.py --ma10p 20261124_065850_aux_115200.bin
"""

import argparse
import os
import struct
from collections import Counter

OUTDIR = "tests/utest"
SYNC = b"\xf1\xd9\x02\x10"
DGS_LEN = 272
RAW_LEN = 250
SLICE_TOW = (197986, 198016)  # [first, last) time of week of the real slice (s)
N_SYNTH = 30  # frames of the re-wrapped fixture
SYNTH_WEEK = 2446
SYNTH_TOW = 200000


def fletcher(body):
    a = b = 0
    for x in body:
        a = (a + x) & 0xFF
        b = (b + a) & 0xFF
    return bytes((a, b))


def dgs_frames(data):
    """Yield (offset, prn, week, tow_ms, flag, l6) of every valid DGS L6 frame."""
    i = 0
    while i + DGS_LEN <= len(data):
        if data[i : i + 4] == SYNC and fletcher(data[i + 2 : i + 270]) == data[i + 270 : i + 272]:
            (svn,) = struct.unpack_from("<H", data, i + 6)
            week, tow = struct.unpack_from(">HI", data, i + 10)
            yield i, svn - 700, week, tow, data[i + 17], data[i + 18 : i + 268]
            i += DGS_LEN
        else:
            i += 1


def wrap_dgs(l6, week, tow_ms, flag=0, snr=40):
    """Wrap one 250-byte L6 frame as a DGS frame; the SVN is 700 + L6 header PRN."""
    body = b"\x02\x10" + struct.pack("<HHBB", 264, 700 + l6[4], 0, 65)
    body += struct.pack(">HI", week, tow_ms) + bytes((snr, flag)) + l6 + b"\x00\x00"
    return b"\xf1\xd9" + body + fletcher(body)


def ubx_l6_frames(data):
    """Yield the 250-byte L6 frame of every UBX-RXM-QZSSL6 message."""
    i = 0
    while i + 8 <= len(data):
        if data[i : i + 4] == b"\xb5\x62\x02\x73":
            (n,) = struct.unpack_from("<H", data, i + 4)
            yield data[i + 6 + 14 : i + 6 + 14 + RAW_LEN]
            i += 8 + n
        else:
            i += 1


def report(name, data):
    ok = Counter()
    skipped = 0
    for _, _, _, _, flag, l6 in dgs_frames(data):
        if flag & 0x07:
            skipped += 1
        else:
            ok[l6[4]] += 1
    print(
        f"{name}: {len(data)} bytes, frames by L6 PRN {dict(sorted(ok.items()))}, {skipped} flagged"
    )


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument(
        "--ma10p", required=True, help="MA-10P L6 port capture (20261124_065850_aux_115200.bin)"
    )
    args = ap.parse_args()

    with open(args.ma10p, "rb") as fp:
        cap = fp.read()
    offs = [
        o for o, _, _, tow, _, _ in dgs_frames(cap) if SLICE_TOW[0] <= tow // 1000 < SLICE_TOW[1]
    ]
    real = cap[offs[0] : offs[-1] + DGS_LEN]

    with open(os.path.join(OUTDIR, "l6route_ubx_l6e.ubx"), "rb") as fp:
        l6e = list(ubx_l6_frames(fp.read()))[:N_SYNTH]
    synth = b""
    for k, l6 in enumerate(l6e):
        tow = (SYNTH_TOW + k) * 1000
        if k == 3:
            # sync sequence inside other data, 10 bytes before a frame
            synth += SYNC + bytes(range(1, 11))
        if k == 7:
            bad = bytearray(wrap_dgs(l6, SYNTH_WEEK, tow))
            bad[100] ^= 0xFF
            synth += bytes(bad)
        if k == 11:
            synth += wrap_dgs(l6, SYNTH_WEEK, tow, flag=0x02)
        if k == 12:
            synth += wrap_dgs(l6, SYNTH_WEEK, tow, flag=0x04)
        synth += wrap_dgs(l6, SYNTH_WEEK, tow)
        synth += b"$GPZDA,000000.000,01,01,2026,00,00*5E\r\n"

    for name, data in (("l6route_ma10p.dgs", real), ("l6route_dgs_l6e.dgs", synth)):
        with open(os.path.join(OUTDIR, name), "wb") as fp:
            fp.write(data)
        report(name, data)


if __name__ == "__main__":
    main()
