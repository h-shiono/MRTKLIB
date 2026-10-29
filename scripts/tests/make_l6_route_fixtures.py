#!/usr/bin/env python3
"""Build the L6 routing fixtures for ``tests/utest/utest_l6_route.c`` (#351).

Each fixture is a short receiver stream (SBF or UBX) of QZSS L6 frames. The
script classifies every frame from its L6 header (vendor ID + MADOCA CSID,
mirroring ``l6_parse_header()``) and prints the per-service counts that the
unit test asserts.

Real slices (first N L6 frames, CRC/checksum-valid blocks copied verbatim):

    l6route_g5.sbf     mosaic-G5 G5P3287a, blocks 4270/4271: CLAS, MADOCA, QZNMA
    l6route_iono.sbf   mosaic-G5 G5P3186k, block 4270 PRN 200: MADOCA iono
    l6route_mcla.sbf   mosaic-CLAS MCLA286e, block 4069 Source=1: CLAS
    l6route_d9c.ubx    NEO-D9C D9C1286m, UBX-RXM-QZSSL6 (2 L6D channels): CLAS

Re-wrapped real frames (the container is synthesised, the 218-byte frame is
real; frame RS parity is not re-encoded because no decoder checks it):

    l6route_ubx_iono.ubx  PRN 200 iono frames as UBX-RXM-QZSSL6 on an L6D
                          channel (NEO-D9C tuned to QZS6): must route as
                          MADOCA iono, not CLAS (D1)
    l6route_sbf_197.sbf   G5P3287a CLAS frames with L6 PRN and SBF SVID set
                          to QZS5 / PRN 197: must route as CLAS, not iono (D2)
    l6route_sbf_src0.sbf  MCLA286e 4069 CLAS blocks with SBF Source = 0
                          (unknown): must route as CLAS and still set ephsat
    l6route_sbf_211.sbf   G5P3287a 4271 MADOCA frames with L6 PRN set to 211
                          (QZS7 L6E, SBF SVID of the L6D satellite 201):
                          l6extract must keep PRNs above 209 (#362)
    l6route_ubx_l6e.ubx   G5P3287a L6E frames (PRN 205: MADOCA, QZNMA) as
                          UBX-RXM-QZSSL6 on an L6E channel with the satellite
                          svId (195): l6extract must name them by the header
                          PRN 205, not the svId (#362)

The last two are consumed by the l6extract ctests (scripts/tests/
check_l6extract.py), not by utest_l6_route.

Usage (from the repository root):
    python3 scripts/tests/make_l6_route_fixtures.py --g5 G5P3287a.sbf \\
        --iono G5P3186k.sbf --mcla MCLA0286e.sbf --d9c D9C1286m.ubx
"""

import argparse
import os
import struct
from collections import Counter

OUTDIR = "tests/utest"
N_SLICE = 120  # L6 frames per real slice
N_SYNTH = 30  # frames per re-wrapped fixture
FRAME_LEN = 218
RAW_LEN = 250


def crc16_ccitt(data):
    """CRC-16-CCITT (poly 0x1021, init 0), as rtk_crc16() for SBF blocks."""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def ubx_checksum(body):
    a = b = 0
    for x in body:
        a = (a + x) & 0xFF
        b = (b + a) & 0xFF
    return bytes((a, b))


def classify(f):
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


def sbf_blocks(data):
    """Yield (block_id, block_bytes) for CRC-valid SBF blocks."""
    i, n = 0, len(data)
    while True:
        i = data.find(b"$@", i)
        if i < 0 or i + 8 > n:
            return
        bid, length = struct.unpack_from("<HH", data, i + 4)
        if length < 8 or length % 4 or i + length > n:
            i += 2
            continue
        blk = data[i : i + length]
        if crc16_ccitt(blk[4:]) == struct.unpack_from("<H", blk, 2)[0]:
            yield bid & 0x1FFF, blk
            i += length
        else:
            i += 2


def sbf_frame(blk):
    words = struct.unpack_from("<63I", blk, 20)
    return b"".join(struct.pack(">I", w) for w in words)


def ubx_messages(data, cls, mid):
    """Yield checksum-valid UBX messages of one class/id."""
    i, n = 0, len(data)
    while True:
        i = data.find(b"\xb5\x62", i)
        if i < 0 or i + 8 > n:
            return
        length = struct.unpack_from("<H", data, i + 4)[0]
        end = i + 8 + length
        if end > n:
            return
        msg = data[i:end]
        if ubx_checksum(msg[2:-2]) == msg[-2:]:
            if msg[2] == cls and msg[3] == mid:
                yield msg
            i = end
        else:
            i += 2


def slice_sbf(path, ids, n, want=None):
    """First n parity-OK L6 blocks of the given IDs (optionally one service)."""
    out, counts = bytearray(), Counter()
    with open(path, "rb") as fp:
        data = fp.read()
    for bid, blk in sbf_blocks(data):
        if bid not in ids or len(blk) < 272 or not blk[15]:
            continue
        svc = classify(sbf_frame(blk))
        if want and svc != want:
            continue
        out += blk
        counts[svc] += 1
        if sum(counts.values()) >= n:
            break
    return bytes(out), counts


def slice_ubx(path, n):
    out, counts = bytearray(), Counter()
    with open(path, "rb") as fp:
        data = fp.read()
    for msg in ubx_messages(data, 0x02, 0x73):
        if (struct.unpack_from("<H", msg, 16)[0] >> 12) & 3 != 1:
            continue  # not error-free: the decoder drops it
        out += msg
        counts[classify(msg[20 : 20 + FRAME_LEN])] += 1
        if sum(counts.values()) >= n:
            break
    return bytes(out), counts


def wrap_ubx_l6(frames, svid, l6e=0):
    """UBX-RXM-QZSSL6 (version 0x00, channel 0, error-free) per frame."""
    out = bytearray()
    for k, f in enumerate(frames):
        chinfo = (1 << 12) | (l6e << 10) | (0 << 8)  # errStatus=1, msgName, ch0
        payload = struct.pack("<BBHIBBH", 0, svid, 45 * 256, 1000 * k, 0, 0, chinfo)
        payload += b"\x00\x00" + f[:RAW_LEN]
        body = b"\x02\x73" + struct.pack("<H", len(payload)) + payload
        out += b"\xb5\x62" + body + ubx_checksum(body)
    return bytes(out)


def rewrap_sbf_prn(blocks, prn, sat_prn=None):
    """Set L6 header PRN (frame byte 4) to `prn` and SBF SVID to the satellite
    `sat_prn` (default `prn`); recompute CRC."""
    out = bytearray()
    for blk in blocks:
        b = bytearray(blk)
        b[14] = (sat_prn or prn) - 192 + 180  # SVID (decode_qzsrawl6: prn = svid - 180 + 192)
        word0 = bytearray(struct.pack(">I", struct.unpack_from("<I", b, 24)[0]))
        word0[0] = prn  # frame byte 4 = first byte of NAVBits word 1
        b[24:28] = struct.pack("<I", struct.unpack(">I", bytes(word0))[0])
        struct.pack_into("<H", b, 2, crc16_ccitt(bytes(b[4:])))
        out += b
    return bytes(out)


def rewrap_sbf_source(blocks, source):
    """Set the SBF QZSRawL6 Source field; recompute CRC."""
    out = bytearray()
    for blk in blocks:
        b = bytearray(blk)
        b[17] = source
        struct.pack_into("<H", b, 2, crc16_ccitt(bytes(b[4:])))
        out += b
    return bytes(out)


def split_blocks(data):
    return [blk for _, blk in sbf_blocks(data)]


def report(name, counts):
    print(f"{name:22s} " + " ".join(f"{k}={v}" for k, v in sorted(counts.items())))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--g5", required=True, help="mosaic-G5 SBF (G5P3287a)")
    ap.add_argument("--iono", required=True, help="mosaic-G5 SBF with L6D PRN 200 (G5P3186k)")
    ap.add_argument("--mcla", required=True, help="mosaic-CLAS SBF (MCLA286e)")
    ap.add_argument("--d9c", required=True, help="NEO-D9C UBX (D9C1286m)")
    args = ap.parse_args()

    fixtures = {}
    fixtures["l6route_g5.sbf"] = slice_sbf(args.g5, {4270, 4271}, N_SLICE)
    fixtures["l6route_iono.sbf"] = slice_sbf(args.iono, {4270}, N_SYNTH, want="MADOCA-IONO")
    fixtures["l6route_mcla.sbf"] = slice_sbf(args.mcla, {4069}, N_SYNTH)
    fixtures["l6route_d9c.ubx"] = slice_ubx(args.d9c, 2 * N_SYNTH)

    iono_frames = [sbf_frame(b) for b in split_blocks(fixtures["l6route_iono.sbf"][0])]
    fixtures["l6route_ubx_iono.ubx"] = (
        wrap_ubx_l6(iono_frames, 200 - 192),
        Counter({"MADOCA-IONO": len(iono_frames)}),
    )
    clas, _ = slice_sbf(args.g5, {4270}, N_SYNTH, want="CLAS")
    fixtures["l6route_sbf_197.sbf"] = (
        rewrap_sbf_prn(split_blocks(clas), 197),
        Counter({"CLAS": N_SYNTH}),
    )

    mcla, mcla_counts = fixtures["l6route_mcla.sbf"]
    fixtures["l6route_sbf_src0.sbf"] = (rewrap_sbf_source(split_blocks(mcla), 0), mcla_counts)

    madoca, _ = slice_sbf(args.g5, {4271}, N_SYNTH, want="MADOCA")
    fixtures["l6route_sbf_211.sbf"] = (
        rewrap_sbf_prn(split_blocks(madoca), 211, sat_prn=201),
        Counter({"MADOCA": N_SYNTH}),
    )
    l6e_blocks = [
        b
        for b in split_blocks(slice_sbf(args.g5, {4271}, 4 * N_SYNTH)[0])
        if sbf_frame(b)[4] == 205
    ]
    l6e_frames = [sbf_frame(b) for b in l6e_blocks[:N_SYNTH]]
    fixtures["l6route_ubx_l6e.ubx"] = (
        wrap_ubx_l6(l6e_frames, 195 - 192, l6e=1),
        Counter(classify(f) for f in l6e_frames),
    )

    for name, (data, counts) in fixtures.items():
        with open(os.path.join(OUTDIR, name), "wb") as fp:
            fp.write(data)
        report(f"{name} ({len(data)} B)", counts)


if __name__ == "__main__":
    main()
