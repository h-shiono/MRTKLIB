#!/usr/bin/env python3
"""Convert a SouthPAN DAS (SISNeT) message log to an RTKLIB SBAS message file.

Input: the DS2DC data lines of a SouthPAN Data Access Service L5 stream
(SBAS-STN-0002 Rev 02 8-9, App. A), one message per line:
    *MSG,<GPS week>,<GPS TOW>,<SINCA payload>*<checksum>
The SINCA payload is hex with run-length tokens: "X|n" is the hex digit X
repeated n times (n one hex digit) and "X/nn" is X repeated nn times (nn two
hex digits). Decompressed, it is 64 hex digits: the 250-bit L5 DFMC frame
(4-bit preamble, 6-bit type, 216 data bits, CRC-24Q) and 6 pad bits. The
"*<checksum>" algorithm is not given in the SDD and is not checked; the
frame CRC-24Q is checked instead.

Output (.sbs, RTKLIB sbsoutmsg form, as MRTKLIB writes L5 frames):
    week tow prn type : <58 hex = 226 bits without CRC, padded with 0>
The type column is the L5 message type, which MRTKLIB's reader uses to tell
L5 frames from L1 frames.

Usage: das2sbs.py DAS.txt OUT.sbs [--prn 122]
"""

import argparse
import re
import sys


def crc24q(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 16
        for _ in range(8):
            crc <<= 1
            if crc & 0x1000000:
                crc ^= 0x1864CFB
    return crc & 0xFFFFFF


def getbits(buf: bytes, pos: int, n: int) -> int:
    v = 0
    for k in range(pos, pos + n):
        v = (v << 1) | ((buf[k // 8] >> (7 - k % 8)) & 1)
    return v


def sinca_expand(s: str) -> str:
    def rep(m: re.Match) -> str:
        return m.group(1) * int(m.group(3), 16)

    s = re.sub(r"([0-9A-Fa-f])(/)([0-9A-Fa-f]{2})", rep, s)
    return re.sub(r"([0-9A-Fa-f])(\|)([0-9A-Fa-f])", rep, s)


def frame_crc_ok(frame: bytes) -> bool:
    # CRC-24Q over the 226 message bits, right-aligned to whole bytes (6 zero
    # bits in front), as RTKLIB sbsdecodemsg() does for L1
    msg = getbits(frame, 0, 226)
    return crc24q(msg.to_bytes(29, "big")) == getbits(frame, 226, 24)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("das")
    ap.add_argument("out")
    ap.add_argument("--prn", type=int, default=122, help="SBAS PRN of the stream")
    a = ap.parse_args()

    nin = nbad = 0
    with open(a.das) as fi, open(a.out, "w") as fo:
        for line in fi:
            if not line.startswith("*MSG,"):
                continue
            v = line.strip().split(",")
            if len(v) < 4:
                continue
            nin += 1
            week, tow = int(v[1]), int(float(v[2]))
            hexs = sinca_expand(v[3].split("*")[0])
            if len(hexs) != 64:
                nbad += 1
                continue
            frame = bytes.fromhex(hexs)
            if not frame_crc_ok(frame):
                nbad += 1
                continue
            msg = bytearray(frame[:29])
            msg[28] &= 0xC0
            mt = getbits(frame, 4, 6)
            fo.write(f"{week:4d} {tow:6d} {a.prn:3d} {mt:2d} : {msg.hex().upper()}\n")
    print(f"{a.das}: {nin} messages, {nbad} rejected (length/CRC)", file=sys.stderr)
    return 0 if nin > nbad else 1


if __name__ == "__main__":
    sys.exit(main())
