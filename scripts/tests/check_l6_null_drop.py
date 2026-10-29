#!/usr/bin/env python3
"""Check that CLAS decoding ignores L6 null messages (#344).

No capture contains a null message (IS-QZSS-L6-008 4.1.2.3: message type ID 0,
alert flag 1, data "0101010" followed by repeated "10101010", i.e. frame bytes
6..217 all 0xAA). This script inserts synthetic null frames into a real CLAS
.l6 file -- one before the first frame and one after every 7th frame, so over
the file they land at every position inside the 5-frame subframes -- and
requires ``mrtk dump`` (CSSR decoded through clas_input_cssrf()) to produce
byte-identical output for the clean and the injected file.

Usage:
    check_l6_null_drop.py MRTK L6FILE NAVFILE WORK_DIR
"""

import os
import subprocess
import sys
import tempfile

RAW_LEN = 250  # L6 frame incl. Reed-Solomon parity, as stored in .l6 files
FRAME_LEN = 218  # preamble .. data
EVERY = 7  # insert a null frame after every 7th frame (coprime to 5)


def null_frame(prn):
    """Null message for `prn`, Reed-Solomon parity zeroed (no decoder checks it)."""
    return (
        b"\x1a\xcf\xfc\x1d"
        + bytes((prn, 0))
        + b"\xaa" * (FRAME_LEN - 6)
        + b"\x00" * (RAW_LEN - FRAME_LEN)
    )


def inject(data):
    frames = [data[k : k + RAW_LEN] for k in range(0, len(data) - RAW_LEN + 1, RAW_LEN)]
    if not frames or any(f[:4] != b"\x1a\xcf\xfc\x1d" for f in frames):
        raise SystemExit("input is not a sequence of 250-byte L6 frames")
    out = bytearray(null_frame(frames[0][4]))
    nnull = 1
    for k, f in enumerate(frames, 1):
        out += f
        if k % EVERY == 0:
            out += null_frame(f[4])
            nnull += 1
    return bytes(out), len(frames), nnull


def dump(mrtk, l6, nav, out):
    res = subprocess.run([mrtk, "dump", "-o", out, l6, nav], capture_output=True, text=True)
    if res.returncode != 0:
        raise SystemExit(f"mrtk dump {l6}: exit {res.returncode}\n{res.stderr}")
    with open(out, "rb") as fp:
        return fp.read()


def main():
    if len(sys.argv) != 5:
        print(__doc__, file=sys.stderr)
        return 2
    mrtk, l6, nav, workdir = sys.argv[1:]
    os.makedirs(workdir, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="l6null_", dir=workdir)

    with open(l6, "rb") as fp:
        injected, nframe, nnull = inject(fp.read())
    l6_null = os.path.join(tmp, "null.l6")
    with open(l6_null, "wb") as fp:
        fp.write(injected)

    clean = dump(mrtk, l6, nav, os.path.join(tmp, "clean.csv"))
    dirty = dump(mrtk, l6_null, nav, os.path.join(tmp, "null.csv"))
    nrec = clean.count(b"\n") - 1
    print(f"{os.path.basename(l6)}: {nframe} frames + {nnull} null frames, {nrec} records")
    if nrec <= 0:
        print("FAIL: clean dump is empty", file=sys.stderr)
        return 1
    if clean != dirty:
        a, b = clean.splitlines(), dirty.splitlines()
        k = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
        print(
            f"FAIL: dump differs ({len(a)} vs {len(b)} lines), first at line {k + 1}",
            file=sys.stderr,
        )
        return 1
    print("ok: dump identical with null frames injected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
