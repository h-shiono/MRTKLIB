#!/usr/bin/env python3
"""Build a CLASLIB-style ``igs14_L5copy.atx`` antenna file from an IGS ANTEX file.

Despite its name, the file shipped with CLASLIB is an igs20 ANTEX: CLASLIB 0.9.1
is ``igs20_2437.atx`` plus the changes below (and CLASLIB 0.7.4 to 0.9.0 used
``IGS20_2317``, no longer in the IGS archive, with the same antenna list).

Applied to the listed antennas only (``L5_COPY_ANTENNAS``):

* a G05 block is inserted right after the G02 block, with the G02 values copied
  verbatim and only the frequency code changed;
* ``# OF FREQUENCIES`` is left unchanged, as CLASLIB does (readers do not use it);
* the output uses CRLF line endings; no COMMENT line is added.

Five of the antennas already have a calibrated G05 block in igs20. CLASLIB inserts
the copy anyway, ahead of that block: readers overwrite on each block, so the
calibrated values still win for them and the copy matters for the others.

CLASLIB also copies G02 to G05 on the satellite antenna ``BLOCK IIA`` / G01 (valid
from 1992-11-22). That entry is left out here, so the output differs from the
CLASLIB file by that one block. Background: ``docs/design/clas-support-files.md``.

Usage:
    python make_clas_atx.py --week 2437 -o igs14_L5copy.atx
    python make_clas_atx.py --week latest -o igs14_L5copy.atx
    python make_clas_atx.py --base igs20_2437.atx -o igs14_L5copy.atx
"""

import argparse
import gzip
import hashlib
import os
import sys
import urllib.request

PCV_URL = "https://files.igs.org/pub/station/general"

# Ground antennas as (type + radome, serial, valid from), whitespace-normalized.
L5_COPY_ANTENNAS = (
    ("TOP700779A NONE", "", ""),
    ("TPSCR.G5 NONE", "", ""),
    ("TPSCR.G5 TPSH", "", ""),
    ("TPSCR.G5C TPSH", "", ""),
    ("TPSCR4 NONE", "", ""),
    ("TRM159900.00 SCIS", "", ""),
    ("TRM23903.00 NONE", "", ""),
    ("TRM29659.00 NONE", "", ""),
    ("TRM29659.00 SCIS", "", ""),
    ("TRM59800.00 SCIS", "", ""),
    ("TRM59800.80 NONE", "", ""),
    ("TRM59800.80 SCIS", "", ""),
)


def label(line):
    """Return the ANTEX record label (columns 61-80) of a line."""
    return line[60:].strip()


def find_g02(antenna):
    """Return the (start, end) line indexes of the G02 block, or None."""
    start = None
    for i, line in enumerate(antenna):
        if label(line) == "START OF FREQUENCY" and line[3:6] == "G02":
            start = i
        elif label(line) == "END OF FREQUENCY" and start is not None:
            return start, i
    return None


def antenna_key(antenna):
    """Return the whitespace-normalized (type, serial, valid from) of an antenna."""
    ant_type = serial = valid_from = ""
    for line in antenna:
        if label(line) == "TYPE / SERIAL NO":
            ant_type = " ".join(line[:20].split())
            serial = line[20:40].strip()
        elif label(line) == "VALID FROM":
            valid_from = " ".join(line[:43].split())
    return ant_type, serial, valid_from


def copy_l2_to_l5(antenna, key):
    """Return the antenna lines with a G05 block copied from G02."""
    span = find_g02(antenna)
    if span is None:
        sys.exit(f"error: {key} has no G02 block to copy")
    start, end = span
    copy = antenna[start : end + 1]
    copy[0] = copy[0][:3] + "G05" + copy[0][6:]
    copy[-1] = copy[-1][:3] + "G05" + copy[-1][6:]
    return antenna[: end + 1] + copy + antenna[end + 1 :]


def build(base):
    """Apply the CLASLIB L5 copy to base ANTEX bytes; return the output bytes."""
    lines = base.decode("latin-1").replace("\r\n", "\n").split("\n")
    if lines[-1] == "":
        lines.pop()
    if not lines or label(lines[0]) != "ANTEX VERSION / SYST":
        sys.exit("error: base file is not an ANTEX file")

    out = []
    antenna = None
    remaining = set(L5_COPY_ANTENNAS)
    for line in lines:
        if label(line) == "START OF ANTENNA":
            antenna = [line]
        elif antenna is None:
            out.append(line)
        else:
            antenna.append(line)
            if label(line) == "END OF ANTENNA":
                key = antenna_key(antenna)
                if key in remaining:
                    remaining.discard(key)
                    antenna = copy_l2_to_l5(antenna, key)
                    print(f"G02->G05: {key[0]} {key[1]} {key[2]}".rstrip())
                out.extend(antenna)
                antenna = None
    if remaining:
        sys.exit(f"error: antennas not found in the base file: {sorted(remaining)}")
    return ("\r\n".join(out) + "\r\n").encode("latin-1")


def fetch_base(week):
    """Download igs20 for a GPS week ('latest' = the current igs20.atx)."""
    url = (
        f"{PCV_URL}/igs20.atx" if week == "latest" else f"{PCV_URL}/pcv_archive/igs20_{week}.atx.gz"
    )
    print(f"download: {url}")
    with urllib.request.urlopen(url, timeout=300) as resp:
        data = resp.read()
    return gzip.decompress(data) if url.endswith(".gz") else data


def main():
    """Parse the command line and write the L5-copy ANTEX."""
    parser = argparse.ArgumentParser(
        description="Build the CLASLIB L5-copy ANTEX from an IGS igs20 ANTEX."
    )
    src = parser.add_mutually_exclusive_group(required=True)
    src.add_argument("--week", help="igs20 archive GPS week (e.g. 2437), or 'latest'")
    src.add_argument("--base", help="local igs20 ANTEX file")
    parser.add_argument("-o", "--output", required=True, help="output ANTEX file")
    args = parser.parse_args()

    if args.base:
        with open(args.base, "rb") as f:
            base = f.read()
    else:
        base = fetch_base(args.week)
    output = build(base)

    tmp = f"{args.output}.tmp"
    with open(tmp, "wb") as f:
        f.write(output)
    os.replace(tmp, args.output)
    print(f"base   sha256 {hashlib.sha256(base).hexdigest()}")
    print(f"output sha256 {hashlib.sha256(output).hexdigest()}  {args.output}")


if __name__ == "__main__":
    main()
