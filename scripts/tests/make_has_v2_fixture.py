#!/usr/bin/env python3
"""Build the Galileo HAS dataset has-v2 (#473).

has-v2 is a superset of has-v1: it carries the has-v1 member
``G5P3162a_15m.sbf`` byte-identical (2026-06-11 15 min, reference from
MADOCA-PPP, docs/design/gal-has.md §11) and adds an hour of the maintainer's
live kubolab capture of 2026-10-23 (``rovr_296u.sbf``, mosaic-G5, the same
CR-G5 antenna and reference coordinate as the handover, clas_l6gap and
g5p3_296u datasets).

SBF blocks kept from the capture (valid CRC, inside the window):
  4027 MeasEpoch, 4007 PVTGeodetic  observations, receiver PVT
  4017 GPSRawCA                     GPS LNAV subframes (GPSNav is only output
                                    when the 2-hourly ephemeris changes)
  5891 GPSNav, 4002 GALNav          decoded ephemeris
  4022 GALRawFNAV, 4023 GALRawINAV  Galileo raw navigation: without them the
                                    slice lacks ephemeris sets (E13, E26) and
                                    its PPP differs from the full capture's
  4024 GALRawCNAV                   Galileo E6-B C/NAV pages (HAS)
The slice gets a synthetic ``.tag`` (``make_handover_fixtures.write_tag``) for
``mrtk run`` file replay.

Usage (from the repository root):
    make_has_v2_fixture.py rovr_296u.sbf G5P3162a_15m.sbf OUTDIR [--version N]
"""

import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_dataset_asset  # noqa: E402
import make_g5p3_296u_fixture as g5  # noqa: E402
import make_handover_fixtures as mh  # noqa: E402

WINDOW_TOW_S = (503970, 507600)  # GPST, week 2441 (2026-10-23 19:59:30..21:00:00)
SLICE = "G5P3296u_has.sbf"
V1_MEMBER = "G5P3162a_15m.sbf"
KEEP = {4027, 4007, 4017, 5891, 4002, 4022, 4023, 4024}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("capture", help="live capture of DOY 296 hour u (rovr_296u.sbf)")
    ap.add_argument("v1sbf", help=f"{V1_MEMBER} extracted from has-v1.tar.gz")
    ap.add_argument("outdir", help="output directory for the slice and the asset")
    ap.add_argument("--version", type=int, default=2)
    args = ap.parse_args()

    if os.path.basename(args.v1sbf) != V1_MEMBER:
        raise SystemExit(f"{args.v1sbf}: expected the has-v1 member {V1_MEMBER}")

    # reuse the g5p3_296u slicer with this dataset's window and block set
    g5.WINDOW_TOW_S, g5.KEEP, g5.SVID_FILTER = WINDOW_TOW_S, KEEP, {}
    with open(args.capture, "rb") as fp:
        data, records, wnc, kept = g5.slice_sbf(fp.read())
    slices = os.path.join(args.outdir, "slices")
    os.makedirs(slices, exist_ok=True)
    path = os.path.join(slices, SLICE)
    with open(path, "wb") as fp:
        fp.write(data)
    mh.write_tag(path + ".tag", records, wnc)
    print(f"{SLICE}: {len(data)} bytes, week {wnc}, TOW {WINDOW_TOW_S[0]}-{WINDOW_TOW_S[1]}")
    print(f"kept blocks: {dict(sorted(kept.items()))}")

    name = f"has-v{args.version}"
    asset = make_dataset_asset.build(name, args.outdir, [args.v1sbf, path, path + ".tag"])
    with open(asset, "rb") as fp:
        sha = hashlib.sha256(fp.read()).hexdigest()
    print(f"asset {asset} ({os.path.getsize(asset)} bytes) sha256 {sha}")
    print(f"mrtk_dataset({name} SHA256 {sha})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
