#!/usr/bin/env python3
"""Filter a u-blox UBX binary file by message type and a NAV-PVT time window.

Used to build small regression fixtures from a long capture, like
``sbf_filter.py`` for SBF. UBX-RXM-QZSSL6 carries no GPST (its ``timeTag`` is
a receiver-internal counter), so the window is defined by NAV-PVT iTOW:
a message belongs to the iTOW of the latest NAV-PVT before it. Messages before
the first NAV-PVT have no time and are dropped.

UBX framing: sync 0xB5 0x62, class, id, length (u2 LE), payload, CK_A, CK_B.
Messages are copied verbatim (checksums stay valid); messages with a bad
checksum are skipped.
"""

import argparse
import struct
import sys

UBX_SYNC = b"\xb5\x62"
NAV_PVT = (0x01, 0x07)
RXM_QZSSL6 = (0x02, 0x73)
DEFAULT_MSGS = "01-07,02-73"  # NAV-PVT (time anchor) + RXM-QZSSL6


def checksum(body):
    a = b = 0
    for x in body:
        a = (a + x) & 0xFF
        b = (b + a) & 0xFF
    return bytes((a, b))


def messages(data):
    """Yield (cls, id, msg_bytes) for checksum-valid UBX messages."""
    i, n = 0, len(data)
    while True:
        i = data.find(UBX_SYNC, i)
        if i < 0 or i + 8 > n:
            return
        length = struct.unpack_from("<H", data, i + 4)[0]
        end = i + 8 + length
        if end > n:
            return
        msg = data[i:end]
        if checksum(msg[2:-2]) == msg[-2:]:
            yield msg[2], msg[3], msg
            i = end
        else:
            i += 2


def timed_messages(data):
    """Yield (itow_ms, cls, id, msg) with the iTOW of the latest NAV-PVT
    (the NAV-PVT itself carries its own iTOW); untimed messages are dropped."""
    itow = None
    for cls, mid, msg in messages(data):
        if (cls, mid) == NAV_PVT and len(msg) >= 8 + 4:
            itow = struct.unpack_from("<I", msg, 6)[0]
        if itow is not None:
            yield itow, cls, mid, msg


def parse_msgs(spec):
    """Parse "cc-ii,cc-ii" (hex class-id pairs) into a set of tuples."""
    out = set()
    for tok in spec.split(","):
        tok = tok.strip()
        if tok:
            c, m = tok.split("-")
            out.add((int(c, 16), int(m, 16)))
    return out


def filter_ubx(data, keep, start_ms, end_ms):
    """Return (output_bytes, records, stats); records = [(itow_ms, end_offset)]
    per kept message, for time-tag synthesis."""
    out = bytearray()
    records = []
    kept = {}
    for itow, cls, mid, msg in timed_messages(data):
        if (cls, mid) in keep and start_ms <= itow < end_ms:
            out += msg
            records.append((itow, len(out)))
            kept[(cls, mid)] = kept.get((cls, mid), 0) + 1
    return bytes(out), records, kept


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Filter a UBX file by message type and NAV-PVT iTOW window."
    )
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument(
        "--msgs", default=DEFAULT_MSGS, help=f"class-id pairs in hex (default {DEFAULT_MSGS})"
    )
    ap.add_argument(
        "--start-tow", type=int, required=True, help="window start iTOW [ms], inclusive"
    )
    ap.add_argument("--duration", type=int, required=True, help="window duration [s]")
    args = ap.parse_args(argv)

    with open(args.input, "rb") as fp:
        data = fp.read()
    end_ms = args.start_tow + args.duration * 1000
    out, _, kept = filter_ubx(data, parse_msgs(args.msgs), args.start_tow, end_ms)
    with open(args.output, "wb") as fp:
        fp.write(out)
    print(f"input  : {args.input} ({len(data)} bytes)", file=sys.stderr)
    print(f"output : {args.output} ({len(out)} bytes)", file=sys.stderr)
    print(f"window : iTOW [{args.start_tow}, {end_ms}) ms", file=sys.stderr)
    for (c, m), k in sorted(kept.items()):
        print(f"  {c:02x}-{m:02x} {k:8d}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
