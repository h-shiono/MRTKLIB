"""download_urbannav.py - Fetch the GNSS files of the UrbanNav Tokyo dataset.

UrbanNav (https://github.com/IPNL-POLYU/UrbanNavDataset, MIT licence) ships
the Tokyo drives as one ``Tokyo_Data.zip`` of ~6.4 GB, almost all of it LiDAR
bags.  This script reads the zip over HTTP range requests and extracts only
the files the RTK benchmark uses (~300 MB):

    Tokyo_Data/{Odaiba,Shinjuku}/{base.nav, base_trimble.obs, reference.csv,
                                  rover_trimble.obs, rover_ublox.obs}

into ``<dataset-dir>/urbannav/``.  Files already present with the archived
size are skipped.  ``zipfile`` checks each member's CRC while extracting.
The data are not redistributed by MRTKLIB; keep the UrbanNav licence notice
with any copy.

Usage
-----
    python download_urbannav.py [--dataset-dir data/benchmark] [--url URL] [--list]
"""

import argparse
import io
import os
import sys
import urllib.request
import zipfile
from pathlib import Path

# Dropbox link of Tokyo_Data.zip from the UrbanNav README (dl=1: direct file).
DEFAULT_URL = "https://www.dropbox.com/s/isu1bugpgr8dpas/Tokyo_Data.zip?dl=1"
SITES = ("Odaiba", "Shinjuku")
FILES = ("base.nav", "base_trimble.obs", "reference.csv", "rover_trimble.obs", "rover_ublox.obs")
WANTED = {f"Tokyo_Data/{site}/{name}" for site in SITES for name in FILES}
_UA = {"User-Agent": "Mozilla/5.0 (compatible; mrtklib-benchmark)"}


class _HttpRangeFile(io.RawIOBase):
    """Seekable read-only view of a remote file through HTTP range requests."""

    def __init__(self, url: str):
        # Keep the published URL: Dropbox's redirect target is short-lived and
        # refuses range requests made outside the redirect.
        self.url = url
        req = urllib.request.Request(url, method="HEAD", headers=_UA)
        with urllib.request.urlopen(req) as r:
            self.size = int(r.headers["Content-Length"])
        self.pos = 0

    def seekable(self) -> bool:
        return True

    def readable(self) -> bool:
        return True

    def tell(self) -> int:
        return self.pos

    def seek(self, off: int, whence: int = 0) -> int:
        self.pos = {0: off, 1: self.pos + off, 2: self.size + off}[whence]
        return self.pos

    def readinto(self, b) -> int:
        n = min(len(b), self.size - self.pos)
        if n <= 0:
            return 0
        h = dict(_UA, Range=f"bytes={self.pos}-{self.pos + n - 1}")
        with urllib.request.urlopen(urllib.request.Request(self.url, headers=h)) as r:
            data = r.read()
        b[: len(data)] = data
        self.pos += len(data)
        return len(data)


def fetch(url: str, out_dir: Path, list_only: bool = False) -> int:
    """Extract the benchmark members of the remote zip into ``out_dir``.

    Args:
        url: URL of ``Tokyo_Data.zip``.
        out_dir: Destination; members keep their ``Tokyo_Data/...`` paths.
        list_only: Only print what would be fetched.

    Returns:
        Number of members missing from the archive (0 on success).
    """
    z = zipfile.ZipFile(io.BufferedReader(_HttpRangeFile(url), buffer_size=8 << 20))
    found = set()
    root = out_dir.resolve()
    for zi in z.infolist():
        if zi.filename not in WANTED:
            continue
        found.add(zi.filename)
        dst = (out_dir / zi.filename).resolve()
        if root not in dst.parents:
            sys.exit(f"FAIL: unsafe member path {zi.filename!r}")
        if dst.is_file() and dst.stat().st_size == zi.file_size:
            print(f"  have {zi.file_size:>11} {zi.filename}")
            continue
        print(f"  get  {zi.file_size:>11} {zi.filename}", flush=True)
        if list_only:
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        tmp = dst.with_name(dst.name + ".part")
        with z.open(zi) as src, open(tmp, "wb") as fh:
            while chunk := src.read(8 << 20):
                fh.write(chunk)
        os.replace(tmp, dst)
    missing = sorted(WANTED - found)
    for name in missing:
        print(f"FAIL: {name} not in the archive", file=sys.stderr)
    return len(missing)


def main() -> int:
    """Entry point for the UrbanNav Tokyo downloader."""
    p = argparse.ArgumentParser(description="Fetch the GNSS files of UrbanNav Tokyo")
    p.add_argument(
        "--dataset-dir",
        default="data/benchmark",
        help="benchmark data root; files go to <dir>/urbannav/Tokyo_Data (default: data/benchmark)",
    )
    p.add_argument("--url", default=DEFAULT_URL, help="URL of Tokyo_Data.zip")
    p.add_argument("--list", action="store_true", help="list what would be fetched")
    args = p.parse_args()
    out_dir = Path(args.dataset_dir)
    if not out_dir.is_absolute():
        out_dir = Path(__file__).resolve().parent.parent.parent / out_dir
    return 1 if fetch(args.url, out_dir / "urbannav", args.list) else 0


if __name__ == "__main__":
    sys.exit(main())
