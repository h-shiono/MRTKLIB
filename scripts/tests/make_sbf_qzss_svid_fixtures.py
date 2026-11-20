#!/usr/bin/env python3
"""Build the QZSS SVID fixtures for ``tests/utest/utest_sbf_qzss_svid.c`` (#471).

SBF numbers QZSS J01-J10 as SVID 181-190. The fixtures are real blocks copied
verbatim (SBF CRC valid; QZSRawL1CA also with the receiver's CRCPassed set)
from two captures:

    sbfqzs_g5.sbf     mosaic-G5 G5P3296u_20m.sbf (release asset clas_l6gap-v1,
                      SHA256 pinned in tests/data/datasets.cmake):
                      - the first MeasEpoch (4027) with SVID 187 and 188
                      - for SVID 187 and 188, the first QZSRawL1CA (4066)
                        subframes 1, 2, 3 received in a row with one IODE
    sbfqzs_malib.sbf  MALIB_OSS_data_obsnav_240822-1100.sbf from
                      tests/data/malib/MALIB_OSS_data.tar.gz:
                      - the first QZSNav (4095) of SVID 187

The unit test checks SVID 187 (the last one decoded before #471) and the ends
of the new range, 188 and 190, on each block. No capture holds SVID 190 or a
QZSNav of SVID 188-190; the unit test makes those cases by rewriting the SVID
of these blocks in memory.

Inputs the committed fixtures were built from (SHA256):

    G5P3296u_20m.sbf
        703019f8633d4fd5179a51444205adb9304ae1ba32222dc0d8b2249c4625f8ef
    MALIB_OSS_data_obsnav_240822-1100.sbf
        d904ef65737301a4bd9edbae95bc2d13aec7aa47eee1a0f1bc8ec4762e067eef

Usage (from the repository root):
    python3 scripts/tests/make_sbf_qzss_svid_fixtures.py \\
        --g5 G5P3296u_20m.sbf --malib MALIB_OSS_data_obsnav_240822-1100.sbf
"""

import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_l6_route_fixtures import sbf_blocks  # noqa: E402

OUTDIR = "tests/utest"
ID_MEASEPOCH = 4027
ID_QZSRAWL1CA = 4066
ID_QZSNAV = 4095
SVIDS = (187, 188)  # SVIDs taken from the G5 capture; 190 is made by the test


def measepoch_svids(blk):
    """SVIDs of the main-antenna Type-1 sub-blocks (layout as decode_measepoch)."""
    n1, len1, len2 = blk[14], blk[15], blk[16]
    svids, p = set(), 20
    for _ in range(n1):
        if p + 20 > len(blk):
            break
        if blk[p + 1] >> 5 == 0:
            svids.add(blk[p + 2])
        p += len1 + len2 * blk[p + 19]
    return svids


def rawca_bits(blk):
    """The 10 words of 24 bits without parity, as decode_rawca() packs them."""
    bits = 0
    for i in range(10):
        bits = (bits << 24) | (struct.unpack_from("<I", blk, 20 + 4 * i)[0] >> 6) & 0xFFFFFF
    return bits


def getbits(bits, pos, n):
    return (bits >> (240 - pos - n)) & ((1 << n) - 1)


def rawca_id_iode(blk):
    """Subframe ID and IODE (subframe 1: IODC low 8 bits) per IS-QZSS-PNT."""
    bits = rawca_bits(blk)
    sfid = getbits(bits, 43, 3)
    pos = {1: 168, 2: 48, 3: 216}.get(sfid)
    return sfid, getbits(bits, pos, 8) if pos is not None else None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--g5", required=True, help="G5P3296u_20m.sbf")
    ap.add_argument("--malib", required=True, help="MALIB_OSS_data_obsnav_240822-1100.sbf")
    args = ap.parse_args()

    with open(args.g5, "rb") as fp:
        g5 = fp.read()
    meas, run = None, {svid: [] for svid in SVIDS}
    done = dict.fromkeys(SVIDS)
    for bid, blk in sbf_blocks(g5):
        tow = struct.unpack_from("<I", blk, 8)[0]
        if bid == ID_MEASEPOCH and meas is None and set(SVIDS) <= measepoch_svids(blk):
            meas = (tow, blk)
        if bid != ID_QZSRAWL1CA or blk[14] not in run or done[blk[14]]:
            continue
        svid, seq = blk[14], run[blk[14]]
        if not blk[15]:  # receiver CRCPassed
            seq.clear()
            continue
        sfid, iode = rawca_id_iode(blk)
        if sfid == 1:
            seq[:] = [(tow, iode, blk)]
        elif seq and sfid == len(seq) + 1 and iode == seq[0][1]:
            seq.append((tow, iode, blk))
            if sfid == 3:
                done[svid] = list(seq)
        else:
            seq.clear()
    if meas is None or not all(done.values()):
        sys.exit("make_sbf_qzss_svid_fixtures: required blocks not found in --g5")

    items = [meas] + [(t, b) for svid in SVIDS for t, _, b in done[svid]]
    items.sort(key=lambda x: x[0])
    with open(os.path.join(OUTDIR, "sbfqzs_g5.sbf"), "wb") as fp:
        fp.write(b"".join(b for _, b in items))
    print(f"sbfqzs_g5.sbf: MeasEpoch TOW {meas[0]} ms")
    for svid in SVIDS:
        tows = ", ".join(str(t) for t, _, _ in done[svid])
        print(f"  4066 SVID {svid}: IODE {done[svid][0][1]}, subframes 1-3 TOW {tows} ms")

    with open(args.malib, "rb") as fp:
        malib = fp.read()
    nav = None
    for bid, blk in sbf_blocks(malib):
        if bid == ID_QZSNAV and blk[14] == 187:
            nav = (struct.unpack_from("<I", blk, 8)[0], blk)
            break
    if nav is None:
        sys.exit("make_sbf_qzss_svid_fixtures: no QZSNav of SVID 187 in --malib")
    with open(os.path.join(OUTDIR, "sbfqzs_malib.sbf"), "wb") as fp:
        fp.write(nav[1])
    print(f"sbfqzs_malib.sbf: QZSNav SVID 187 TOW {nav[0]} ms")


if __name__ == "__main__":
    main()
