#!/usr/bin/env python3
"""Check ``mrtk cssr2rtcm3`` output continuity across lost L6 frames (#442).

Runs ``mrtk cssr2rtcm3`` on the ``clas_l6gap-v1`` SBF window, in which the
receiver dropped whole SBF blocks for 1-2 s every few minutes. A subframe with
a frame lost inside it is discarded (#363); the OSR path then bridges with the
last good correction set (BackupCSSR) instead of skipping epochs. Without the
bridge the window gives 1067 epochs and outages of up to 33 s; with it, 1164
epochs and one 11 s interval.

The converter reports one ``Epoch N: week=W tow=T ...`` line per RTCM3 epoch
on stderr (not always at the start of a line); the epoch count and the
longest interval between consecutive epochs are taken from those lines.

Usage:
    check_cssr2rtcm3_gap.py MRTK CONF SBF OUT [--min-epochs N] [--max-gap S]

Run from the source tree root (the converter configuration uses relative
paths to the CLAS grid and antenna files).
"""

import argparse
import os
import re
import subprocess
import sys

EPOCH = re.compile(r"Epoch \d+: week=(\d+) tow=(\d+)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mrtk")
    ap.add_argument("conf")
    ap.add_argument("sbf")
    ap.add_argument("out")
    ap.add_argument("--min-epochs", type=int, default=1150)
    ap.add_argument("--max-gap", type=int, default=15)
    a = ap.parse_args()

    if os.path.exists(a.out):
        os.remove(a.out)
    res = subprocess.run(
        [
            a.mrtk,
            "cssr2rtcm3",
            "-k",
            a.conf,
            "-in",
            "sbf://file://" + os.path.abspath(a.sbf),
            "-out",
            "file://" + os.path.abspath(a.out),
        ],
        capture_output=True,
        text=True,
        errors="replace",
        timeout=120,
    )
    if res.returncode != 0:
        print(f"FAIL: exit {res.returncode}\n{res.stderr[-2000:]}")
        return 1

    t = [int(w) * 604800 + int(s) for w, s in EPOCH.findall(res.stderr)]
    if len(t) < 2:
        print(f"FAIL: {len(t)} epochs\n{res.stderr[-2000:]}")
        return 1
    gap, at = max((b - a_, a_) for a_, b in zip(t, t[1:]))
    size = os.path.getsize(a.out) if os.path.exists(a.out) else 0
    print(
        f"epochs={len(t)} span={t[-1] - t[0] + 1} s max_gap={gap} s (after tow {at % 604800}) rtcm3={size} bytes"
    )

    ok = True
    if len(t) < a.min_epochs:
        print(f"FAIL: epochs {len(t)} < {a.min_epochs}")
        ok = False
    if gap > a.max_gap:
        print(f"FAIL: max gap {gap} s > {a.max_gap} s")
        ok = False
    if size == 0:
        print("FAIL: empty RTCM3 output")
        ok = False
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
