"""cases.py - PPC-Dataset benchmark run metadata.

Defines the six PPC2024 driving runs (nagoya × 3, tokyo × 3) and helper
functions for converting GPS time to UTC and computing the L6 archive
session letters needed by download_l6.py.
"""

from datetime import datetime, timedelta, timezone

# GPS epoch and leap seconds
_GPS_EPOCH = datetime(1980, 1, 6, tzinfo=timezone.utc)
GPS_LEAP = 18  # GPS - UTC leap seconds (correct as of 2024)

# ---------------------------------------------------------------------------
# Run metadata
# ---------------------------------------------------------------------------
CASES: list[dict] = [
    {
        "id": "nagoya_run1",
        "city": "nagoya",
        "run": "run1",
        "gps_week": 2325,
        "tow_start": 550380.0,
        "tow_end": 551910.0,
    },
    {
        "id": "nagoya_run2",
        "city": "nagoya",
        "run": "run2",
        "gps_week": 2323,
        "tow_start": 555720.0,
        "tow_end": 557610.0,
    },
    {
        "id": "nagoya_run3",
        "city": "nagoya",
        "run": "run3",
        "gps_week": 2325,
        "tow_start": 553800.0,
        "tow_end": 554840.0,
    },
    {
        "id": "tokyo_run1",
        "city": "tokyo",
        "run": "run1",
        "gps_week": 2324,
        "tow_start": 187470.0,
        "tow_end": 189860.0,
    },
    {
        "id": "tokyo_run2",
        "city": "tokyo",
        "run": "run2",
        "gps_week": 2324,
        "tow_start": 177000.0,
        "tow_end": 178830.0,
    },
    {
        "id": "tokyo_run3",
        "city": "tokyo",
        "run": "run3",
        "gps_week": 2324,
        "tow_start": 179460.0,
        "tow_end": 182520.0,
    },
]


def tow_to_utc(week: int, tow: float) -> datetime:
    """Convert GPS week + time-of-week to UTC datetime.

    Args:
        week: GPS week number.
        tow: Time of week in seconds.

    Returns:
        Corresponding UTC datetime (timezone-aware).
    """
    gps_t = _GPS_EPOCH + timedelta(weeks=week, seconds=tow)
    return gps_t - timedelta(seconds=GPS_LEAP)


def l6_sessions(week: int, tow_start: float, tow_end: float) -> list[tuple[int, int, str]]:
    """Return the L6 archive sessions (year, doy, letter) covering a run.

    Session letters are A–X, one per UTC hour (A = 00:00–01:00, ...).
    A 30-second margin is added on each side to ensure full coverage.

    Args:
        week: GPS week number.
        tow_start: Run start TOW in seconds.
        tow_end: Run end TOW in seconds.

    Returns:
        List of (year, doy, session_letter) tuples.  Typically 1–2 entries.
    """
    t_start = tow_to_utc(week, tow_start - 30)
    t_end = tow_to_utc(week, tow_end + 30)

    sessions = []
    t = t_start.replace(minute=0, second=0, microsecond=0)
    while t <= t_end:
        doy = t.timetuple().tm_yday
        letter = chr(ord("A") + t.hour)
        entry = (t.year, doy, letter)
        if entry not in sessions:
            sessions.append(entry)
        t += timedelta(hours=1)
    return sessions


def case_by_id(case_id: str) -> dict:
    """Look up a case by its id string.

    Args:
        case_id: Case identifier (e.g. ``"nagoya_run1"``).

    Returns:
        Case metadata dict.

    Raises:
        KeyError: If no case with that id exists.
    """
    for c in CASES:
        if c["id"] == case_id:
            return c
    raise KeyError(f"unknown case: {case_id!r}")


# ---------------------------------------------------------------------------
# Sanity check (python -m scripts.benchmark.cases)
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    for c in CASES:
        t0 = tow_to_utc(c["gps_week"], c["tow_start"])
        t1 = tow_to_utc(c["gps_week"], c["tow_end"])
        dur = (c["tow_end"] - c["tow_start"]) / 60
        sess = l6_sessions(c["gps_week"], c["tow_start"], c["tow_end"])
        print(
            f"{c['id']:18s}  {t0.strftime('%Y-%m-%d %H:%M')}"
            f"–{t1.strftime('%H:%M')} UTC  {dur:.0f} min  "
            f"sessions={[f'{y}-{d:03d}{s}' for y,d,s in sess]}"
        )
