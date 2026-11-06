#!/usr/bin/env python3
"""Check ``mrtk cssr2rtcm3`` output continuity and timing (#442).

Runs ``mrtk cssr2rtcm3`` on the ``clas_l6gap-v1`` SBF window, in which the
receiver dropped whole SBF blocks for 1-2 s every few minutes.

- Continuity: a subframe with a frame lost inside it is discarded (#363); the
  OSR path bridges with the last good correction set (BackupCSSR) instead of
  skipping epochs. Without the bridge the window gives 1067 epochs and
  outages of up to 33 s.
- Timing: each output epoch is the latest valid PVT time on the ``-t`` grid.
  The converter used to step one interval per PVT, so after an input gap it
  output past epochs 2-10 s behind the receiver and jumped forward once the
  lag passed 10 s.

The converter reports one ``Epoch N: week=W tow=T pvt=P ...`` line per RTCM3
epoch on stderr (not always at the start of a line). The epoch times are also
decoded from the MSM messages of the RTCM3 output (GPS / Galileo / QZSS MSM,
whose epoch field is the time of week in ms) and must match the log, increase
strictly, and lie within one interval before the PVT time that triggered them.

Usage:
    check_cssr2rtcm3_gap.py MRTK CONF SBF OUT [--interval S] [--min-epochs N]
                            [--max-gap S]

Run from the source tree root (the converter configuration uses relative
paths to the CLAS grid and antenna files).
"""

import argparse
import os
import re
import subprocess
import sys

EPOCH = re.compile(r"Epoch \d+: week=(\d+) tow=([\d.]+) pvt=([\d.]+)")
# MSM1-7 of GPS, Galileo, QZSS: 30-bit epoch time = time of week in ms
MSM_TOW_MS = set(range(1071, 1078)) | set(range(1091, 1098)) | set(range(1111, 1118))


def crc24q(data):
    crc = 0
    for b in data:
        crc ^= b << 16
        for _ in range(8):
            crc <<= 1
            if crc & 0x1000000:
                crc ^= 0x1864CFB
    return crc & 0xFFFFFF


def rtcm_msm_epochs(path):
    """Distinct MSM epoch times (ms of week) in output order, and a CRC error count."""
    d = open(path, "rb").read()
    epochs, bad, i = [], 0, 0
    while i + 6 <= len(d):
        if d[i] != 0xD3:
            i += 1
            continue
        n = ((d[i + 1] & 0x03) << 8) | d[i + 2]
        if i + 6 + n > len(d):
            break
        frame = d[i : i + 3 + n]
        if crc24q(frame) != int.from_bytes(d[i + 3 + n : i + 6 + n], "big"):
            bad += 1
            i += 1
            continue
        body = int.from_bytes(frame[3:], "big")
        bits = 8 * n
        mtype = body >> (bits - 12)
        if mtype in MSM_TOW_MS:
            ms = (body >> (bits - 54)) & ((1 << 30) - 1)
            if not epochs or epochs[-1] != ms:
                epochs.append(ms)
        i += 6 + n
    return epochs, bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mrtk")
    ap.add_argument("conf")
    ap.add_argument("sbf")
    ap.add_argument("out")
    ap.add_argument("--interval", type=float, default=1.0, help="converter -t (s)")
    ap.add_argument("--min-epochs", type=int, default=1150)
    ap.add_argument("--max-gap", type=float, default=15)
    a = ap.parse_args()

    if os.path.exists(a.out):
        os.remove(a.out)
    res = subprocess.run(
        [
            a.mrtk,
            "cssr2rtcm3",
            "-k",
            a.conf,
            "-t",
            str(a.interval),
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

    log = [(int(w) * 604800 + float(s), float(p)) for w, s, p in EPOCH.findall(res.stderr)]
    if len(log) < 2:
        print(f"FAIL: {len(log)} epochs\n{res.stderr[-2000:]}")
        return 1
    t = [e for e, _ in log]
    gap, at = max((b - a_, a_) for a_, b in zip(t, t[1:]))
    # PVT time minus output epoch, within the week
    lags = [(p - e % 604800 + 302400) % 604800 - 302400 for e, p in log]
    rtcm, bad = rtcm_msm_epochs(a.out) if os.path.exists(a.out) else ([], 0)
    log_ms = [round((e % 604800) * 1000) for e in t]
    print(
        f"epochs={len(t)} span={t[-1] - t[0] + a.interval:g} s max_gap={gap:g} s (after tow {at % 604800:g}) "
        f"lag=[{min(lags):.3f}, {max(lags):.3f}] s rtcm_epochs={len(rtcm)} crc_errors={bad}"
    )

    ok = True

    def fail(msg):
        nonlocal ok
        print("FAIL: " + msg)
        ok = False

    if len(t) < a.min_epochs:
        fail(f"epochs {len(t)} < {a.min_epochs}")
    if gap > a.max_gap:
        fail(f"max gap {gap:g} s > {a.max_gap:g} s")
    if min(lags) < 0.0 or max(lags) >= a.interval:
        fail(f"output epoch not within one interval ({a.interval} s) before its PVT time")
    if any(b <= x for x, b in zip(t, t[1:])):
        fail("output epochs do not increase strictly")
    if bad:
        fail(f"{bad} RTCM3 CRC errors")
    if rtcm != log_ms:
        first = next(
            (k for k, (x, y) in enumerate(zip(rtcm, log_ms)) if x != y), min(len(rtcm), len(log_ms))
        )
        fail(
            f"RTCM3 MSM epochs ({len(rtcm)}) differ from the log ({len(log_ms)}) from epoch {first}"
        )
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
