#!/usr/bin/env python3
"""Build a test dataset release asset for h-shiono/mrtklib-data (#386).

Packs the given files into ``<name-vN>.tar.gz`` reproducibly: members are
added flat (no directories), sorted by name, with mtime 0, mode 0644,
uid/gid 0 and no extended attributes or macOS metadata (no ``._*`` files),
and the gzip header carries no timestamp. The same input files therefore give
the same SHA256 on any machine.

Prints the SHA256 and size for ``tests/data/datasets.cmake`` and the commands
that publish the asset as an immutable release (draft -> upload -> publish).

Usage:
    make_dataset_asset.py NAME-vN OUTDIR FILE [FILE ...]
"""

import gzip
import hashlib
import io
import os
import re
import sys
import tarfile

REPO = "h-shiono/mrtklib-data"


def build(name, outdir, files):
    if not re.fullmatch(r"[a-z0-9][a-z0-9_]*-v[0-9]+", name):
        raise SystemExit(f"bad dataset name {name!r}: expected <name>-v<N>, lower case")
    bases = [os.path.basename(f) for f in files]
    if len(set(bases)) != len(bases):
        raise SystemExit("duplicate file names: members are stored without directories")

    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.GNU_FORMAT) as tar:
        for path, base in sorted(zip(files, bases), key=lambda x: x[1]):
            with open(path, "rb") as fp:
                data = fp.read()
            info = tarfile.TarInfo(base)
            info.size = len(data)
            info.mtime = 0
            info.mode = 0o644
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            tar.addfile(info, io.BytesIO(data))

    asset = os.path.join(outdir, f"{name}.tar.gz")
    with open(asset, "wb") as fp:
        with gzip.GzipFile(filename="", mode="wb", fileobj=fp, mtime=0, compresslevel=9) as gz:
            gz.write(raw.getvalue())
    return asset


def main():
    if len(sys.argv) < 4:
        print(__doc__, file=sys.stderr)
        return 2
    name, outdir, files = sys.argv[1], sys.argv[2], sys.argv[3:]
    os.makedirs(outdir, exist_ok=True)
    asset = build(name, outdir, files)

    with open(asset, "rb") as fp:
        data = fp.read()
    sha = hashlib.sha256(data).hexdigest()
    with tarfile.open(asset) as tar:
        members = [(m.name, m.size) for m in tar.getmembers()]
    bad = [n for n, _ in members if os.path.basename(n).startswith("._")]
    if bad:
        raise SystemExit(f"macOS metadata in the asset: {bad}")

    print(f"asset   {asset}")
    print(f"size    {len(data)} bytes")
    print(f"sha256  {sha}")
    for n, s in members:
        print(f"member  {n} ({s} bytes)")
    print("\ntests/data/datasets.cmake:")
    print(f"mrtk_dataset({name} SHA256 {sha})")
    print("\npublish (maintainer; the release is immutable once published):")
    print(f"gh release create {name} --repo {REPO} --draft --title {name} --notes-file NOTES.md")
    print(f"gh release upload {name} {asset} --repo {REPO}")
    print(f"gh release edit {name} --repo {REPO} --draft=false")
    return 0


if __name__ == "__main__":
    sys.exit(main())
