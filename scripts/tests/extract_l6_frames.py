#!/usr/bin/env python3
"""Extract one real QZSS L6 frame per service from SBF captures.

Builds the fixture for ``tests/utest/utest_l6_header.c`` (issue #351): a binary
file of concatenated 218-byte L6 frames (preamble .. data, no Reed-Solomon
parity), one per requested service, in the order given on the command line.

For each ``SERVICE=FILE`` argument the first frame of that service whose
subframe indicator is set is taken from SBF blocks 4069 (QZSRawL6),
4270 (QZSRawL6D) or 4271 (QZSRawL6E). The service is classified from the L6
header exactly as ``l6_parse_header()`` does (vendor ID + MADOCA CSID).
A block is used only if its SBF CRC is valid (same check as the MRTKLIB SBF
decoder) and the receiver reports a successful Reed-Solomon decode (Parity).

SBF QZSRawL6 body (little-endian, offsets from block start): 8 TOW, 12 WNc,
14 SVID, 15 Parity (0 = RS failure), 16 RSCnt, 17 Source, 20.. NAVBits as
63 little-endian u32 words whose bytes are MSB-first frame bytes.

Example (the committed fixture):
    python3 scripts/tests/extract_l6_frames.py \\
        -o tests/utest/l6_frames.bin \\
        CLAS=G5P3287a.sbf MADOCA=G5P3287a.sbf \\
        MADOCA-IONO=G5P3186k.sbf QZNMA=G5P3287a.sbf
"""

import argparse
import struct
import sys

L6_BLOCKS = (4069, 4270, 4271)
FRAME_LEN = 218
PREAMBLE = b"\x1a\xcf\xfc\x1d"


def crc16_ccitt(data):
    """CRC-16-CCITT (poly 0x1021, init 0), as rtk_crc16() for SBF blocks."""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def classify(f):
    """Service name of a 218-byte frame (mirrors l6_parse_header())."""
    if f[:4] != PREAMBLE:
        return "UNKNOWN"
    if f[5] == 0 and (f[6] & 0x80) and all(x == 0xAA for x in f[6:FRAME_LEN]):
        return "NULL"
    vid = f[5] >> 5
    if vid == 5:
        return "CLAS"
    if vid == 2:
        return "MADOCA-IONO" if (f[5] >> 2) & 1 else "MADOCA"
    if vid == 3:
        return "QZNMA"
    return "UNKNOWN"


def sbf_l6_frames(data):
    """Yield (tow_ms, frame) for every CRC- and parity-OK L6 frame in an SBF file."""
    i, n = 0, len(data)
    while True:
        i = data.find(b"$@", i)
        if i < 0 or i + 8 > n:
            return
        bid, length = struct.unpack_from("<HH", data, i + 4)
        if length < 8 or length % 4 or i + length > n:
            i += 2
            continue
        if (
            (bid & 0x1FFF) in L6_BLOCKS
            and length >= 272
            and data[i + 15]
            and crc16_ccitt(data[i + 4 : i + length]) == struct.unpack_from("<H", data, i + 2)[0]
        ):
            words = struct.unpack_from("<63I", data, i + 20)
            frame = b"".join(struct.pack(">I", w) for w in words)[:FRAME_LEN]
            yield struct.unpack_from("<I", data, i + 8)[0], frame
        i += length


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("-o", "--output", required=True, help="output .bin file")
    ap.add_argument("specs", nargs="+", metavar="SERVICE=FILE")
    args = ap.parse_args()

    cache = {}
    out = bytearray()
    for spec in args.specs:
        svc, path = spec.split("=", 1)
        if path not in cache:
            with open(path, "rb") as fp:
                cache[path] = fp.read()
        for tow, f in sbf_l6_frames(cache[path]):
            if classify(f) == svc and (f[5] & 0x01):
                out += f
                print(
                    f"{svc:12s} {path}: TOW {tow / 1000:.1f} PRN {f[4]} msgid 0x{f[5]:02X}",
                    file=sys.stderr,
                )
                break
        else:
            sys.exit(f"error: no {svc} subframe-head frame in {path}")

    with open(args.output, "wb") as fp:
        fp.write(out)


if __name__ == "__main__":
    main()
