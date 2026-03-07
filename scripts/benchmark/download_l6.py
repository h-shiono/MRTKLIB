"""download_l6.py - Download QZSS L6D (CLAS) and L6E (MADOCA) archive files.

Downloads only the L6 sessions needed for the configured PPC-Dataset runs.
Files that already exist are skipped.  Requires only the Python standard library.

Archive URLs
------------
L6D (CLAS):
    https://sys.qzss.go.jp/archives/l6/{year}/{year}{doy}{session}.l6

L6E (MADOCA):
    https://l6msg.go.gnss.go.jp/archives/{year}/{doy}/{year}{doy}{session}.{prn}.l6
    PRN candidates tried in order: 209, 193, 194, 195, 196, 199

Usage
-----
    python download_l6.py [--l6-dir DIR] [--mode clas|madoca|both]
                          [--case ID[,ID...]] [--dry-run]
"""

import argparse
import sys
import urllib.error
import urllib.request
from pathlib import Path

from cases import CASES, l6_sessions

# ---------------------------------------------------------------------------
# URL templates
# ---------------------------------------------------------------------------
_L6D_URL = "https://sys.qzss.go.jp/archives/l6/{year}/{year}{doy:03d}{session}.l6"
_L6E_URL = (
    "https://l6msg.go.gnss.go.jp/archives/{year}/{doy:03d}/"
    "{year}{doy:03d}{session}.{prn}.l6"
)
MADOCA_PRNS = [209, 193, 194, 195, 196, 199]


# ---------------------------------------------------------------------------
# Low-level helpers
# ---------------------------------------------------------------------------
def _l6d_url(year: int, doy: int, session: str) -> str:
    """Build L6D (CLAS) archive URL."""
    return _L6D_URL.format(year=year, doy=doy, session=session)


def _l6e_url(year: int, doy: int, session: str, prn: int) -> str:
    """Build L6E (MADOCA) archive URL for a specific PRN."""
    return _L6E_URL.format(year=year, doy=doy, session=session, prn=prn)


def _probe_l6e_prn(year: int, doy: int, session: str) -> int | None:
    """Find the first available MADOCA PRN for a given session.

    Args:
        year: Calendar year.
        doy: Day-of-year.
        session: Session letter (A–X).

    Returns:
        First PRN that returns HTTP 200, or ``None`` if none found.
    """
    for prn in MADOCA_PRNS:
        url = _l6e_url(year, doy, session, prn)
        try:
            req = urllib.request.Request(url, method="HEAD")
            with urllib.request.urlopen(req, timeout=10):
                return prn
        except (urllib.error.HTTPError, urllib.error.URLError, OSError):
            continue
    return None


def _download(url: str, dest: Path, dry_run: bool = False) -> bool:
    """Download a URL to dest, printing progress.

    Args:
        url: Source URL.
        dest: Destination file path.
        dry_run: If True, print URL without downloading.

    Returns:
        True on success (or dry-run), False on failure.
    """
    if dest.exists():
        print(f"  [skip]     {dest.name} (already exists)")
        return True
    if dry_run:
        print(f"  [dry-run]  {url}")
        print(f"             → {dest}")
        return True
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + ".tmp")
    try:
        print(f"  [download] {url}")
        print(f"             → {dest.name} ", end="", flush=True)
        urllib.request.urlretrieve(url, tmp)
        tmp.rename(dest)
        size_kb = dest.stat().st_size // 1024
        print(f"({size_kb} KB)")
        return True
    except (urllib.error.HTTPError, urllib.error.URLError, OSError) as exc:
        print(f"FAIL: {exc}")
        if tmp.exists():
            tmp.unlink()
        return False


# ---------------------------------------------------------------------------
# Public interface
# ---------------------------------------------------------------------------
def download_l6d_session(
    year: int, doy: int, session: str, l6_dir: Path, dry_run: bool = False
) -> Path | None:
    """Download one L6D (CLAS) session file.

    Args:
        year: Calendar year.
        doy: Day-of-year (1-based).
        session: Session letter (A–X).
        l6_dir: Local directory to store the file.
        dry_run: Print URL without downloading.

    Returns:
        Path to the local file, or ``None`` on failure.
    """
    fname = f"{year}{doy:03d}{session}.l6"
    dest = l6_dir / fname
    url = _l6d_url(year, doy, session)
    return dest if _download(url, dest, dry_run) else None


def download_l6e_session(
    year: int, doy: int, session: str, l6_dir: Path, dry_run: bool = False
) -> Path | None:
    """Download one L6E (MADOCA) session file.

    Probes PRN candidates in order and downloads the first available file.

    Args:
        year: Calendar year.
        doy: Day-of-year (1-based).
        session: Session letter (A–X).
        l6_dir: Local directory to store the file.
        dry_run: Print URL without downloading.

    Returns:
        Path to the local file, or ``None`` if no PRN found.
    """
    if dry_run:
        print(f"  [dry-run]  probing MADOCA PRNs for {year}/{doy:03d}/{session}:")
        for prn in MADOCA_PRNS:
            print(f"             {_l6e_url(year, doy, session, prn)}")
        return l6_dir / f"{year}{doy:03d}{session}.{MADOCA_PRNS[0]}.l6"

    prn = _probe_l6e_prn(year, doy, session)
    if prn is None:
        print(f"  [FAIL]     no MADOCA PRN found for {year}/{doy:03d}/{session}")
        return None
    fname = f"{year}{doy:03d}{session}.{prn}.l6"
    dest = l6_dir / fname
    url = _l6e_url(year, doy, session, prn)
    return dest if _download(url, dest, dry_run) else None


def ensure_case_l6(
    case: dict, l6_dir: Path, mode: str = "both", dry_run: bool = False
) -> dict[str, list[Path]]:
    """Ensure all L6 files needed for a run are present.

    Args:
        case: Case metadata dict from ``cases.CASES``.
        l6_dir: Directory to store L6 files.
        mode: ``"clas"``, ``"madoca"``, or ``"both"``.
        dry_run: Print URLs without downloading.

    Returns:
        Dict ``{"clas": [...], "madoca": [...]}`` with local file paths.
        Missing files are omitted.
    """
    sessions = l6_sessions(case["gps_week"], case["tow_start"], case["tow_end"])
    result: dict[str, list[Path]] = {"clas": [], "madoca": []}

    for year, doy, session in sessions:
        if mode in ("clas", "both"):
            p = download_l6d_session(year, doy, session, l6_dir, dry_run)
            if p:
                result["clas"].append(p)
        if mode in ("madoca", "both"):
            p = download_l6e_session(year, doy, session, l6_dir, dry_run)
            if p:
                result["madoca"].append(p)
    return result


def ensure_all(
    cases: list[dict], l6_dir: Path, mode: str = "both", dry_run: bool = False
) -> dict[str, dict[str, list[Path]]]:
    """Download all L6 files for a list of cases.

    Args:
        cases: List of case metadata dicts.
        l6_dir: Directory to store L6 files.
        mode: ``"clas"``, ``"madoca"``, or ``"both"``.
        dry_run: Print URLs without downloading.

    Returns:
        Dict keyed by case id → ``{"clas": [...], "madoca": [...]}`` paths.
    """
    all_results = {}
    for case in cases:
        print(f"\n--- {case['id']} ---")
        all_results[case["id"]] = ensure_case_l6(case, l6_dir, mode, dry_run)
    return all_results


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main() -> int:
    """Download L6 files for the configured PPC-Dataset runs."""
    p = argparse.ArgumentParser(
        description="Download CLAS L6D and MADOCA L6E files for PPC-Dataset benchmark"
    )
    p.add_argument("--l6-dir", default="data/benchmark/l6",
                   help="Directory to store L6 files (default: data/benchmark/l6)")
    p.add_argument("--mode", choices=["clas", "madoca", "both"], default="both",
                   help="Which L6 type to download (default: both)")
    p.add_argument("--case", default="",
                   help="Comma-separated case IDs (default: all)")
    p.add_argument("--dry-run", action="store_true",
                   help="Print URLs without downloading")
    args = p.parse_args()

    l6_dir = Path(args.l6_dir)
    if not args.dry_run:
        l6_dir.mkdir(parents=True, exist_ok=True)

    # Filter cases
    cases = CASES
    if args.case:
        ids = {c.strip() for c in args.case.split(",")}
        cases = [c for c in CASES if c["id"] in ids]
        if not cases:
            print(f"FAIL: no matching cases for: {args.case}", file=sys.stderr)
            return 1

    ensure_all(cases, l6_dir, args.mode, args.dry_run)
    print("\nDone.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
