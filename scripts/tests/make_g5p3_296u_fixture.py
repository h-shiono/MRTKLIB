#!/usr/bin/env python3
"""Build the mosaic-G5 open-service SSR dataset of DOY 2026-296 hour u.

One hour (19:59:30-21:00:00 GPST, week 2441) of the maintainer's live kubolab
capture, cut for the BeiDou PPP-B2b tests (#417) and kept usable for SouthPAN
PVS (#418). Same antenna and reference coordinate as the handover and
clas_l6gap datasets (TPSCR.G5 NONE). The window starts just before the
top-of-hour BeiDou B-CNAV1 / GPS ephemeris updates, so a replay has broadcast
ephemeris within the first minute.

SBF blocks kept:
  observations / navigation : 4027 MeasEpoch, 4007 PVTGeodetic,
                              4017 GPSRawCA, 5891 GPSNav, 4002 GALNav,
                              4081 BDSNav (GPS raw subframes because GPSNav is
                              only output when the 2-hourly ephemeris changes;
                              GALNav changes every 10 min, so no Galileo raw)
  PPP-B2b                   : 4242 BDSRawB2b of the GEOs C01-C04 only (the
                              IGSO/MEO frames are B-CNAV3), 4251 BDSCNav1
  SouthPAN PVS              : 4021 GEORawL5 of PRN 122 only
The slice gets a synthetic ``.tag`` (``make_handover_fixtures.write_tag``) for
``mrtk run`` file replay.

Post-processing needs a RINEX 4 navigation file with BeiDou CNV1 records
(``mrtk convert`` writes none), so the IGS merged broadcast file of the day is
cut to the records with an epoch inside the window +- the update interval
(BDS CNV1 and GAL INAV 1 h, GPS LNAV 2 h) and packed as well.

Both are packed into ``g5p3_296u-v<N>.tar.gz`` (#386) with
``make_dataset_asset.py``; nothing large is committed.

Usage (from the repository root):
    make_g5p3_296u_fixture.py rovr_296u.sbf BRD400DLR_S_20262960000_01D_MN.rnx OUTDIR [--version N]
"""

import argparse
import datetime
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_dataset_asset  # noqa: E402
import make_handover_fixtures as mh  # noqa: E402

WINDOW_TOW_S = (503970, 507600)  # GPST, week 2441 (2026-10-23 19:59:30..21:00:00)
SLICE = "G5P3296u.sbf"
NAV_CUT = "BRD4296u.rnx"
KEEP = {4027, 4007, 4017, 5891, 4002, 4081, 4242, 4251, 4021}
SVID_FILTER = {
    4242: set(range(141, 145)),  # BDSRawB2b: C01-C04 (SBF SVID 141-144)
    4021: {122},  # GEORawL5: SouthPAN PRN 122
}
NAV_MARGIN_S = {("C", "CNV1"): 3600, ("G", "LNAV"): 7200, ("E", "INAV"): 3600}


def slice_sbf(data):
    """Kept blocks with a valid CRC inside the window; returns the bytes, the
    tag records [(tow_ms, end offset)], the week and per-block counts."""
    out, records, kept, wnc = bytearray(), [], {}, None
    start, end = (t * 1000 for t in WINDOW_TOW_S)
    i, n = 0, len(data)
    while True:
        i = data.find(b"$@", i)
        if i < 0 or i + 8 > n:
            break
        bid = struct.unpack_from("<H", data, i + 4)[0] & 0x1FFF
        length = struct.unpack_from("<H", data, i + 6)[0]
        if length < 8 or length % 4 or i + length > n:
            i += 1
            continue
        blk = data[i : i + length]
        i += length
        tow = struct.unpack_from("<I", blk, 8)[0]
        if bid not in KEEP or not start <= tow < end or not mh.sbf_crc_ok(blk):
            continue
        if bid in SVID_FILTER and blk[14] not in SVID_FILTER[bid]:
            continue
        out += blk
        records.append((tow, len(out)))
        kept[bid] = kept.get(bid, 0) + 1
        if wnc is None:
            wnc = struct.unpack_from("<H", blk, 12)[0]
    return bytes(out), records, wnc, kept


def cut_nav(lines):
    """Header plus the RINEX 4 records of the NAV_MARGIN_S types whose epoch
    (time of clock) is inside the window +- the margin of their type; returns
    the lines and the record counts."""
    # epoch lines are in the system time (BDT is GPST - 14 s); the margin covers it
    sow0 = 2441 * 604800
    k = next(j for j, l in enumerate(lines) if "END OF HEADER" in l)
    out, counts, j = lines[: k + 1], {}, k + 1
    while j < len(lines):
        if not lines[j].startswith(">"):
            j += 1
            continue
        e = j + 1
        while e < len(lines) and not lines[e].startswith(">"):
            e += 1
        head = lines[j].split()
        key = (head[2][0], head[3]) if head[1] == "EPH" and len(head) > 3 else None
        if key in NAV_MARGIN_S:
            y, mo, d, h, mi, s = (int(float(x)) for x in lines[j + 1][4:23].split())
            sow = gps_seconds(y, mo, d, h, mi, s) - sow0
            if WINDOW_TOW_S[0] - NAV_MARGIN_S[key] <= sow <= WINDOW_TOW_S[1] + NAV_MARGIN_S[key]:
                out += lines[j:e]
                counts[key] = counts.get(key, 0) + 1
        j = e
    return out, counts


def gps_seconds(y, mo, d, h, mi, s):
    """Seconds since the GPS epoch (1980-01-06) of a calendar time."""
    t = datetime.datetime(y, mo, d, h, mi, s) - datetime.datetime(1980, 1, 6)
    return t.days * 86400 + t.seconds


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("capture", help="live capture of DOY 296 hour u (rovr_296u.sbf)")
    ap.add_argument("nav", help="BRD400DLR_S_20262960000_01D_MN.rnx (uncompressed)")
    ap.add_argument("outdir", help="output directory for the slice, the cut nav and the asset")
    ap.add_argument("--version", type=int, default=1)
    args = ap.parse_args()

    with open(args.capture, "rb") as fp:
        data, records, wnc, kept = slice_sbf(fp.read())
    slices = os.path.join(args.outdir, "slices")
    os.makedirs(slices, exist_ok=True)
    path = os.path.join(slices, SLICE)
    with open(path, "wb") as fp:
        fp.write(data)
    mh.write_tag(path + ".tag", records, wnc)
    print(f"{SLICE}: {len(data)} bytes, week {wnc}, TOW {WINDOW_TOW_S[0]}-{WINDOW_TOW_S[1]}")
    print(f"kept blocks: {dict(sorted(kept.items()))}")

    with open(args.nav) as fp:
        nav, counts = cut_nav(fp.read().splitlines(keepends=True))
    nav_path = os.path.join(slices, NAV_CUT)
    with open(nav_path, "w") as fp:
        fp.writelines(nav)
    print(f"{NAV_CUT}: {os.path.getsize(nav_path)} bytes, records {dict(sorted(counts.items()))}")

    name = f"g5p3_296u-v{args.version}"
    asset = make_dataset_asset.build(name, args.outdir, [path, path + ".tag", nav_path])
    with open(asset, "rb") as fp:
        sha = hashlib.sha256(fp.read()).hexdigest()
    print(f"asset {asset} ({os.path.getsize(asset)} bytes) sha256 {sha}")
    print(f"mrtk_dataset({name} SHA256 {sha})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
