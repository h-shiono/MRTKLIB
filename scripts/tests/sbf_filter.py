#!/usr/bin/env python3
"""Filter a Septentrio SBF binary file by block-ID set and a TOW window.

Used to build small, bundled regression fixtures (e.g. the Galileo HAS test
data) from a large field capture: keep only the blocks needed by the test
pipeline (l6extract -> .has -> ``mrtk convert`` -> ``mrtk post``) and clip to a
short time window.

SBF framing (little-endian):

    offset  size  field
    0       2     sync  ($@ = 0x24 0x40)
    2       2     CRC-16-CCITT over bytes [4 .. 4+length)
    4       2     ID    (block ID in low 13 bits; high 3 = revision)
    6       2     length (total block length in bytes, multiple of 4)
    8       4     TOW   (ms of GPS week; 0xFFFFFFFF = not-set) -- payload offset 0
                        for every block this filter cares about

Blocks are copied verbatim, so the stored CRC stays valid -- no re-CRC needed.
A block whose TOW is the 0xFFFFFFFF not-set sentinel is dropped (we cannot place
it in the window; the blocks selected for fixtures always carry a real TOW).
"""

import argparse
import struct
import sys

SBF_SYNC1 = 0x24  # '$'
SBF_SYNC2 = 0x40  # '@'
TOW_NOTSET = 0xFFFFFFFF
ID_MASK = 0x1FFF

# Default block set: the minimal pipeline for Galileo HAS post-processing.
#   4027 MeasEpoch    -> observations
#   4017 GPSRawCA     -> GPS ephemeris
#   4023 GALRawINAV   -> Galileo ephemeris
#   4024 GALRawCNAV   -> Galileo E6-B C/NAV (HAS pages)
#   4271 QZSRawL6E    -> QZSS L6E (MADOCA) frames (l6extract L6E)
DEFAULT_BLOCKS = "4027,4017,4023,4024,4271"


def parse_blocks(spec):
    """Parse a comma-separated block-ID list into a set of ints."""
    return {int(x) for x in spec.split(",") if x.strip()}


def filter_sbf(data, keep_ids, start_tow, end_tow):
    """Return (output_bytes, stats).

    keep_ids   : set of block IDs to retain
    start_tow  : inclusive window start (ms of week)
    end_tow    : exclusive window end (ms of week)
    """
    n = len(data)
    out = bytearray()
    i = 0
    seen = {}  # id -> count seen
    kept = {}  # id -> count kept
    records = []  # (tow_ms, end offset in out) per kept block, for time tags
    wnc = None  # week number of the first kept block
    n_notset = 0
    while i < n - 8:
        if data[i] != SBF_SYNC1 or data[i + 1] != SBF_SYNC2:
            i += 1
            continue
        idfull = struct.unpack_from("<H", data, i + 4)[0]
        bid = idfull & ID_MASK
        length = struct.unpack_from("<H", data, i + 6)[0]
        # Reject implausible framing: length must be a positive multiple of 4
        # and fit in the file. Otherwise treat the sync as a false positive and
        # advance one byte.
        if length < 8 or length % 4 != 0 or i + length > n:
            i += 1
            continue
        tow = struct.unpack_from("<I", data, i + 8)[0]
        seen[bid] = seen.get(bid, 0) + 1
        if bid in keep_ids:
            if tow == TOW_NOTSET:
                n_notset += 1
            elif start_tow <= tow < end_tow:
                out += data[i : i + length]
                kept[bid] = kept.get(bid, 0) + 1
                records.append((tow, len(out)))
                if wnc is None:
                    wnc = struct.unpack_from("<H", data, i + 12)[0]
        i += length
    stats = {"seen": seen, "kept": kept, "notset_dropped": n_notset, "records": records, "wnc": wnc}
    return bytes(out), stats


def main(argv=None):
    """CLI entry point: filter an SBF file by block-ID set and TOW window."""
    ap = argparse.ArgumentParser(description="Filter an SBF file by block ID and TOW window.")
    ap.add_argument("input", help="input SBF file")
    ap.add_argument("output", help="output SBF file")
    ap.add_argument(
        "--blocks",
        default=DEFAULT_BLOCKS,
        help=f"comma-separated block IDs to keep (default: {DEFAULT_BLOCKS})",
    )
    ap.add_argument(
        "--start-tow",
        type=int,
        required=True,
        help="window start TOW [ms of GPS week], inclusive",
    )
    ap.add_argument(
        "--duration",
        type=int,
        required=True,
        help="window duration [s]; end = start + duration*1000 (exclusive)",
    )
    args = ap.parse_args(argv)

    keep_ids = parse_blocks(args.blocks)
    start_tow = args.start_tow
    end_tow = start_tow + args.duration * 1000

    with open(args.input, "rb") as f:
        data = f.read()

    out, stats = filter_sbf(data, keep_ids, start_tow, end_tow)

    with open(args.output, "wb") as f:
        f.write(out)

    print(f"input  : {args.input} ({len(data)} bytes)", file=sys.stderr)
    print(f"output : {args.output} ({len(out)} bytes)", file=sys.stderr)
    print(f"window : TOW [{start_tow}, {end_tow}) ms  ({args.duration} s)", file=sys.stderr)
    print(f"blocks : {sorted(keep_ids)}", file=sys.stderr)
    print(f"  {'id':>6} {'seen':>8} {'kept':>8}", file=sys.stderr)
    for bid in sorted(keep_ids):
        print(
            f"  {bid:6d} {stats['seen'].get(bid, 0):8d} {stats['kept'].get(bid, 0):8d}",
            file=sys.stderr,
        )
    if stats["notset_dropped"]:
        print(
            f"  (dropped {stats['notset_dropped']} kept-id blocks with TOW not-set)",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
