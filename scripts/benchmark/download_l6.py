"""download_l6.py - Download QZSS CLAS and MADOCA-PPP L6 archive files.

Downloads the L6 sessions needed for the configured PPC-Dataset runs, or the
sessions of an explicit UTC date.  Files that already exist are skipped.
Requires only the Python standard library.

Archive URLs
------------
CLAS (L6D):
    https://sys.qzss.go.jp/archives/l6/{year}/{year}{doy}{session}.l6

MADOCA-PPP SSR (L6E) and wide-area ionospheric augmentation (L6D):
    https://l6msg.go.gnss.go.jp/archives/{year}/{doy}/{year}{doy}{session}.{prn}.l6
    L6E PRN candidates — first available is used: 204, 205, 206, 207, 209, 210, 211
    L6D PRN candidates — all available are used:  200, 201

``mrtk post`` consumes a single L6E file but up to ``MIONO_MAX_PRN`` L6D files,
which it discriminates by the ``.200.l6`` / ``.201.l6`` name suffix.  Session
letters must be upper case: the MADOCA archive answers 403 for lower case.

Usage
-----
    python download_l6.py [--l6-dir DIR] [--mode clas|madoca|both]
                          [--case ID[,ID...]] [--datetime YYYY-MM-DD[S]]
                          [--dry-run]
"""

import argparse
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

from cases import CASES, date_sessions, l6_sessions

# ---------------------------------------------------------------------------
# URL templates
# ---------------------------------------------------------------------------
_L6D_URL = "https://sys.qzss.go.jp/archives/l6/{year}/{year}{doy:03d}{session}.l6"
_L6E_URL = "https://l6msg.go.gnss.go.jp/archives/{year}/{doy:03d}/{year}{doy:03d}{session}.{prn}.l6"
MADOCA_PRNS = [204, 205, 206, 207, 209, 210, 211]
MADOCA_L6D_PRNS = [200, 201]


# ---------------------------------------------------------------------------
# Low-level helpers
# ---------------------------------------------------------------------------
def _l6d_url(year: int, doy: int, session: str) -> str:
    """Build L6D (CLAS) archive URL."""
    return _L6D_URL.format(year=year, doy=doy, session=session)


def _l6e_url(year: int, doy: int, session: str, prn: int) -> str:
    """Build L6E (MADOCA) archive URL for a specific PRN."""
    return _L6E_URL.format(year=year, doy=doy, session=session, prn=prn)


def _probe_madoca_prns(
    year: int, doy: int, session: str, prns: list[int], first_only: bool = True
) -> list[int]:
    """Find the MADOCA PRNs that have an archive file for a given session.

    Args:
        year: Calendar year.
        doy: Day-of-year.
        session: Session letter (A–X).
        prns: PRN candidates, tried in order.
        first_only: Stop at the first hit instead of probing every candidate.

    Returns:
        PRNs that returned HTTP 200, in candidate order (empty if none).
    """
    found = []
    for prn in prns:
        url = _l6e_url(year, doy, session, prn)
        try:
            req = urllib.request.Request(url, method="HEAD")
            with urllib.request.urlopen(req, timeout=10):
                found.append(prn)
                if first_only:
                    break
        except (urllib.error.HTTPError, urllib.error.URLError, OSError):
            continue
    return found


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


def _download_madoca(
    year: int,
    doy: int,
    session: str,
    l6_dir: Path,
    prns: list[int],
    first_only: bool,
    label: str,
    dry_run: bool = False,
) -> list[Path]:
    """Download the MADOCA session files matching a PRN candidate set.

    Args:
        year: Calendar year.
        doy: Day-of-year (1-based).
        session: Session letter (A–X).
        l6_dir: Local directory to store the files.
        prns: PRN candidates, tried in order.
        first_only: Take only the first available PRN.
        label: Signal label used in messages (``"L6E"`` / ``"L6D"``).
        dry_run: Print URLs without downloading.

    Returns:
        Paths to the local files (empty if no PRN is available).  In dry-run
        the first candidate PRN is assumed.
    """
    if dry_run:
        print(f"  [dry-run]  probing MADOCA {label} PRNs for {year}/{doy:03d}/{session}:")
        for prn in prns:
            print(f"             {_l6e_url(year, doy, session, prn)}")
        return [l6_dir / f"{year}{doy:03d}{session}.{prns[0]}.l6"]

    found = _probe_madoca_prns(year, doy, session, prns, first_only)
    if not found:
        print(f"  [FAIL]     no MADOCA {label} PRN found for {year}/{doy:03d}/{session}")
        return []

    paths = []
    for prn in found:
        dest = l6_dir / f"{year}{doy:03d}{session}.{prn}.l6"
        if _download(_l6e_url(year, doy, session, prn), dest):
            paths.append(dest)
    return paths


def download_l6e_session(
    year: int, doy: int, session: str, l6_dir: Path, dry_run: bool = False
) -> list[Path]:
    """Download the MADOCA-PPP SSR (L6E) file of one session.

    Probes PRN candidates in order and downloads the first available file —
    ``mrtk post`` consumes a single L6E file.

    Args:
        year: Calendar year.
        doy: Day-of-year (1-based).
        session: Session letter (A–X).
        l6_dir: Local directory to store the file.
        dry_run: Print URLs without downloading.

    Returns:
        Single-element list with the local path, or empty if no PRN is available.
    """
    return _download_madoca(
        year, doy, session, l6_dir, MADOCA_PRNS, first_only=True, label="L6E", dry_run=dry_run
    )


def download_l6d_madoca_session(
    year: int, doy: int, session: str, l6_dir: Path, dry_run: bool = False
) -> list[Path]:
    """Download the MADOCA-PPP ionospheric augmentation (L6D) files of one session.

    Every available PRN is downloaded: the engine feeds each ``.200.l6`` /
    ``.201.l6`` file into its own ``pppiono`` region slot.

    Args:
        year: Calendar year.
        doy: Day-of-year (1-based).
        session: Session letter (A–X).
        l6_dir: Local directory to store the files.
        dry_run: Print URLs without downloading.

    Returns:
        Local paths, or empty if no PRN is available.
    """
    return _download_madoca(
        year,
        doy,
        session,
        l6_dir,
        MADOCA_L6D_PRNS,
        first_only=False,
        label="L6D",
        dry_run=dry_run,
    )


def ensure_sessions(
    sessions: list[tuple[int, int, str]],
    l6_dir: Path,
    mode: str = "both",
    dry_run: bool = False,
) -> dict[str, list[Path]]:
    """Ensure the L6 files of the given archive sessions are present.

    Args:
        sessions: (year, doy, session_letter) tuples.
        l6_dir: Directory to store L6 files.
        mode: ``"clas"``, ``"madoca"``, or ``"both"``.
        dry_run: Print URLs without downloading.

    Returns:
        Dict ``{"clas": [...], "madoca": [...]}`` with local file paths.
        Missing files are omitted.
    """
    result: dict[str, list[Path]] = {"clas": [], "madoca": []}

    for year, doy, session in sessions:
        if mode in ("clas", "both"):
            p = download_l6d_session(year, doy, session, l6_dir, dry_run)
            if p:
                result["clas"].append(p)
        if mode in ("madoca", "both"):
            result["madoca"] += download_l6e_session(year, doy, session, l6_dir, dry_run)
            result["madoca"] += download_l6d_madoca_session(year, doy, session, l6_dir, dry_run)
    return result


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
    return ensure_sessions(sessions, l6_dir, mode, dry_run)


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
def _parse_datetime_arg(spec: str) -> list[tuple[int, int, str]]:
    """Expand a ``YYYY-MM-DD[S]`` CLI argument into archive sessions.

    Args:
        spec: UTC date, optionally suffixed with a session letter A–X
            (case-insensitive).  Without a letter the whole day is expanded.

    Returns:
        List of (year, doy, session_letter) tuples.

    Raises:
        ValueError: If the date or the session letter is malformed.
    """
    try:
        date = datetime.strptime(spec[:10], "%Y-%m-%d").replace(tzinfo=timezone.utc)
    except ValueError:
        raise ValueError(f"invalid --datetime {spec!r} (expected YYYY-MM-DD[S])") from None
    return date_sessions(date, spec[10:] or None)


def main() -> int:
    """Download L6 files for the configured PPC-Dataset runs."""
    p = argparse.ArgumentParser(
        description="Download CLAS L6D and MADOCA L6E files for PPC-Dataset benchmark"
    )
    p.add_argument(
        "--l6-dir",
        default="data/benchmark/l6",
        help="Directory to store L6 files (default: data/benchmark/l6)",
    )
    p.add_argument(
        "--mode",
        choices=["clas", "madoca", "both"],
        default="both",
        help="Which L6 type to download (default: both)",
    )
    p.add_argument("--case", default="", help="Comma-separated case IDs (default: all)")
    p.add_argument(
        "--datetime",
        default=None,
        help=(
            "UTC date, optionally with a session letter A–X: '2026-07-30A' for a "
            "single hour, '2026-07-30' for the whole day (mutually exclusive with --case)"
        ),
    )
    p.add_argument("--dry-run", action="store_true", help="Print URLs without downloading")
    args = p.parse_args()

    if args.datetime and args.case:
        print("FAIL: --datetime and --case are mutually exclusive", file=sys.stderr)
        return 1

    l6_dir = Path(args.l6_dir)
    if not args.dry_run:
        l6_dir.mkdir(parents=True, exist_ok=True)

    if args.datetime:
        try:
            sessions = _parse_datetime_arg(args.datetime)
        except ValueError as exc:
            print(f"FAIL: {exc}", file=sys.stderr)
            return 1
        print(f"\n--- {args.datetime} ---")
        ensure_sessions(sessions, l6_dir, args.mode, args.dry_run)
    else:
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
