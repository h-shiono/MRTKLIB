#!/usr/bin/env python3
"""Check the extracted handover dataset against its manifest (#372).

For every file in ``handover_manifest.json``: the size matches; the ``.tag``
is well formed (60-byte header text, header tick = first record on the GPST
ms-of-week axis, monotonic ticks, monotonic file offsets ending at the file
size); and the per-channel L6 PRN runs recomputed from the data (no PRN
interleaving, one handover per case) equal the manifest.

Usage:
    check_handover_dataset.py DATASET_DIR
"""

import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_handover_fixtures as mhf  # noqa: E402


def check_tag(path, size, t0_expect):
    with open(path, "rb") as fp:
        tag = fp.read()
    errs = []
    if not tag.startswith(b"TIMETAG"):
        errs.append("header text")
    t0 = struct.unpack_from("<I", tag, 60)[0]
    if t0 != t0_expect:
        errs.append(f"header tick {t0} != manifest {t0_expect}")
    recs = [struct.unpack_from("<II", tag, k) for k in range(76, len(tag), 8)]
    if (len(tag) - 76) % 8 or not recs:
        errs.append("record layout")
        return errs
    ticks = [r[0] for r in recs]
    pos = [r[1] for r in recs]
    if ticks[0] != 0 or any(b < a for a, b in zip(ticks, ticks[1:])):
        errs.append("ticks not monotonic from 0")
    if any(b <= a for a, b in zip(pos, pos[1:])) or pos[-1] != size:
        errs.append(f"offsets not increasing to file size ({pos[-1]} vs {size})")
    return errs


def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    d = sys.argv[1]
    with open(os.path.join(d, "handover_manifest.json")) as fp:
        manifest = json.load(fp)

    fails = 0
    for e in manifest["cases"]:
        errs = []
        for name, f in e["files"].items():
            path = os.path.join(d, name)
            size = os.path.getsize(path)
            if size != f["bytes"]:
                errs.append(f"{name}: {size} bytes, manifest {f['bytes']}")
            errs += [
                f"{name}.tag: {x}" for x in check_tag(path + ".tag", size, f["tag_start_tow_ms"])
            ]
            if f["l6"]:
                with open(path, "rb") as fp:
                    data = fp.read()
                if name.endswith(".ubx"):
                    seqs = mhf.ubx_l6_prns(data)
                else:
                    seqs = {0: mhf.sbf_l6_prns(data)}
                got = mhf.describe({ch: mhf.runs(seq) for ch, seq in seqs.items()})
                if got != e["l6_channels"]:
                    errs.append(f"{name}: L6 channel runs differ from the manifest")
        print(f"{'FAIL' if errs else 'ok  '} {e['case']} ({', '.join(e['files'])})")
        for x in errs:
            print(f"  {x}")
        fails += bool(errs)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
