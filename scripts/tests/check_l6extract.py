#!/usr/bin/env python3
"""Check ``mrtk l6extract`` output names and frame counts (#362).

Runs ``mrtk l6extract`` on the L6 routing fixtures in ``tests/utest`` and
asserts the exact set of ``{prefix}.{PRN}.l6d`` / ``.l6e`` files, the number
of 250-byte frames in each, and that every frame's L6 header PRN matches the
file name. The signal and PRN must come from the L6 frame header, not from
the receiver container:

    l6route_mcla.sbf      SBF 4069 (mosaic-CLAS) carries L6D, not L6E
    l6route_sbf_211.sbf   L6E PRN 211 (QZS7) is kept
    l6route_ubx_l6e.ubx   UBX L6E is named by the header PRN, not the svId

Usage:
    check_l6extract.py MRTK FIXTURE_DIR WORK_DIR
"""

import os
import subprocess
import sys
import tempfile

RAW_LEN = 250

# fixture -> {output file suffix: frame count}
EXPECTED = {
    "l6route_g5.sbf": {"195.l6d": 30, "199.l6d": 30, "205.l6e": 30, "209.l6e": 30},
    "l6route_iono.sbf": {"200.l6d": 30},
    "l6route_mcla.sbf": {"199.l6d": 30},
    "l6route_sbf_197.sbf": {"197.l6d": 30},
    "l6route_sbf_src0.sbf": {"199.l6d": 30},
    "l6route_sbf_211.sbf": {"211.l6e": 30},
    "l6route_d9c.ubx": {"194.l6d": 30, "199.l6d": 30},
    "l6route_ubx_iono.ubx": {"200.l6d": 30},
    "l6route_ubx_l6e.ubx": {"205.l6e": 30},
}


def check(mrtk, fixture, expected, workdir):
    errors = []
    outdir = tempfile.mkdtemp(prefix="l6extract_", dir=workdir)
    prefix = os.path.join(outdir, "x")
    res = subprocess.run(
        [mrtk, "l6extract", "-no-has", "-o", prefix, fixture],
        capture_output=True,
        text=True,
    )
    if res.returncode != 0:
        return [f"{fixture}: exit {res.returncode}\n{res.stderr}"]

    got = sorted(os.listdir(outdir))
    want = sorted(f"x.{s}" for s in expected)
    if got != want:
        errors.append(f"{fixture}: files {got}, expected {want}")

    for suffix, nframe in expected.items():
        path = f"{prefix}.{suffix}"
        if not os.path.exists(path):
            continue
        with open(path, "rb") as fp:
            data = fp.read()
        if len(data) != nframe * RAW_LEN:
            errors.append(
                f"{path}: {len(data)} bytes, expected {nframe} frames ({nframe * RAW_LEN} bytes)"
            )
        prn = int(suffix.split(".")[0])
        for k in range(0, len(data), RAW_LEN):
            frame = data[k : k + RAW_LEN]
            if frame[:4] != b"\x1a\xcf\xfc\x1d" or frame[4] != prn:
                errors.append(
                    f"{path}: frame {k // RAW_LEN} has preamble {frame[:4].hex()} PRN {frame[4]}"
                )
                break
    return errors


def main():
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    mrtk, fixdir, workdir = sys.argv[1:]
    os.makedirs(workdir, exist_ok=True)

    errors = []
    for name, expected in EXPECTED.items():
        errs = check(mrtk, os.path.join(fixdir, name), expected, workdir)
        print(f"{'FAIL' if errs else 'ok  '} {name}")
        errors += errs
    for e in errors:
        print(f"  {e}", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
