#!/usr/bin/env python3
"""Convert a BeiDou PPP-B2b text dump to MRTKLIB's binary .b2b record format.

Input format (233h_bdsb2b.txt, one message per line):
    week tow prn type len hexpayload
where
    week        GPS week number
    tow         GPS time-of-week [s]
    prn         BDS GEO PRN (59-62) that broadcast the B2b message
    type        B2b message type (== MesType, redundant with payload)
    len         payload length in bytes (64)
    hexpayload  128 hex chars = 64 bytes, the 486-bit B2b message MSB-first

The 486-bit message is MesType(6) + data(456) + CRC-24Q(24); the CRC is the
RTCM CRC-24Q (polynomial 0x1864CFB) computed bit-serially over the first 462
bits. The 64-byte payload carries those 486 bits MSB-aligned in its first 61
bytes (486 = 60 full bytes + 6 bits), with the remaining bits zero padding.

Output format (.b2b): fixed 72-byte little-endian records
    tow_ms  u4        GPS TOW [ms]
    wnc     u2        GPS week
    prn     u1        BDS PRN
    flags   u1        bit0 = CRC-24Q passed
    msg     u8[61]    first 61 bytes of payload, byte 60 masked to its 6 MSBs
    pad     u8[3]     zero
The 6-bit tail mask zeroes the 2 LSBs of byte 60 (bit 486 onward) so the stored
message contains exactly the 486 broadcast bits and nothing past them.

Only CRC-passing records are written (MT63 null messages included; the C
decoder skips them). cssrlib is NOT used by this script.
"""

import argparse
import os
import struct
from binascii import unhexlify
from collections import Counter

CRC24Q_POLY = 0x1864CFB


def crc24q_bits(payload, nbits):
    """Bit-serial RTCM CRC-24Q over the first ``nbits`` bits of ``payload``."""
    crc = 0
    for k in range(nbits):
        bit = (payload[k >> 3] >> (7 - (k & 7))) & 1
        crc ^= bit << 23
        crc <<= 1
        if crc & 0x1000000:
            crc ^= CRC24Q_POLY
    return crc & 0xFFFFFF


def rx_crc24(payload):
    """Read the transmitted CRC: bits 462..485 of the MSB-first message."""
    val = 0
    for k in range(462, 486):
        bit = (payload[k >> 3] >> (7 - (k & 7))) & 1
        val = (val << 1) | bit
    return val


def main():
    ap = argparse.ArgumentParser(description="BDS PPP-B2b txt -> MRTKLIB .b2b binary.")
    ap.add_argument("--txt", default="233h_bdsb2b.txt", help="input text dump")
    ap.add_argument("--out", default="233h.b2b", help="output .b2b binary")
    args = ap.parse_args()

    n_pass = n_fail = n_written = 0
    mt_hist = Counter()
    out = open(args.out, "wb")
    for line in open(args.txt):
        parts = line.split()
        if len(parts) < 6:
            continue
        week = int(parts[0])
        tow = int(parts[1])
        prn = int(parts[2])
        payload = unhexlify(parts[5])
        if len(payload) != 64:
            continue

        ok = crc24q_bits(payload, 462) == rx_crc24(payload)
        if not ok:
            n_fail += 1
            continue
        n_pass += 1
        mt = payload[0] >> 2
        mt_hist[mt] += 1

        # Keep the 486 message bits: 60 full bytes + the top 6 bits of byte 60.
        msg = bytearray(payload[:61])
        msg[60] &= 0xFC  # zero the 2 LSBs (bits 486, 487)

        rec = struct.pack("<IHBB", tow * 1000, week, prn, 0x01)
        rec += bytes(msg)  # 61 bytes
        rec += b"\x00" * 3  # pad to 72
        assert len(rec) == 72
        out.write(rec)
        n_written += 1
    out.close()

    print(f"input: {args.txt}")
    print(f"CRC pass: {n_pass}  CRC fail: {n_fail}")
    print(f"records written: {n_written}")
    print(f"MT histogram (written): {dict(sorted(mt_hist.items()))}")
    print(f"output: {os.path.abspath(args.out)}")


if __name__ == "__main__":
    main()
