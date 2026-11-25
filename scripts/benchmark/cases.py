"""cases.py - PPC-Dataset benchmark run metadata.

Defines the six PPC2024 driving runs (nagoya × 3, tokyo × 3) and helper
functions for converting GPS time to UTC and computing the L6 archive
session letters needed by download_l6.py.
"""

from datetime import datetime, timedelta, timezone

# GPS epoch and leap seconds
_GPS_EPOCH = datetime(1980, 1, 6, tzinfo=timezone.utc)
GPS_LEAP = 18  # GPS - UTC leap seconds (correct as of 2024)
SEC_WEEK = 86400 * 7
# L6 archive session letters, one per UTC hour.  Upper case is mandatory: the
# MADOCA archive returns 403 for a lower-case letter.
SESSIONS = "ABCDEFGHIJKLMNOPQRSTUVWX"

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


# ---------------------------------------------------------------------------
# UrbanNav Tokyo (RTK only)
# ---------------------------------------------------------------------------
# Constant offset of the UrbanNav Tokyo reference trajectories, as local
# (east, north, up) metres, solution minus reference.  The benchmark adds it to
# the reference positions before scoring ("offset-corrected" score).
#
# How the value was obtained: from RTKLIB-EX 2.5.1 (demo5) solutions of the
# four cases, run with the f9p_urban / trimble_urban configurations published
# with rtklibexplorer's UrbanNav benchmark (as first published).  For every
# reference epoch with a solution at the same time (any quality) and a 3D
# difference below 2 m, the solution-minus-reference vector was taken in the
# local ENU frame of the reference point; the value is the per-component
# median over all four cases pooled (42,217 epochs).  Per-case medians lie
# within E -0.703..-0.734, N -0.009..-0.034, U -0.053..-0.086 m.
#
# The value therefore comes from another solver's solutions, not from an
# independent survey, and is not a property of MRTKLIB.  Two cross-checks
# support treating it as a dataset-level constant:
# - Estimated from one area's demo5 solutions only and used to score the
#   other area, it changes no case score of the MRTKLIB benchmark run
#   (develop 9a5bc66) by more than 0.1 pt (overall 59.1 % either way).
# - In ECEF it is (+0.503, +0.514, -0.062) m.  The per-case offsets in
#   rtklibexplorer's post of 2026-08-31 ("Updates to urban RTK benchmarking
#   with RTKLIB") are 0.49-0.51, 0.50-0.52 and -0.06..-0.07 m, within 0.02 m
#   of these ECEF components, although the post labels them East / North / Up
#   and states no sign convention.
# The cause is not known (the post does not identify it either).  The four
# cases share one base station and its RINEX-header coordinate, so an error
# there would produce a common offset of this kind, but that has not been
# verified.
URBANNAV_OFFSET_ENU = (-0.717, -0.020, -0.079)

_URBANNAV_SITES = {
    "odaiba": ("Odaiba", 2032, 273375.1, 274616.0),
    "shinjuku": ("Shinjuku", 2032, 281275.1, 283370.0),
}
# (rover file, site configurations layered on the mode configuration)
_URBANNAV_ROVERS = {
    "f9p": ("rover_ublox.obs", ["urbannav", "urbannav_f9p"]),
    "trimble": ("rover_trimble.obs", ["urbannav"]),
}

URBANNAV_CASES: list[dict] = [
    {
        "id": f"{site}_{rx}",
        "city": "urbannav",
        "dir": f"urbannav/Tokyo_Data/{name}",
        "rover": rover,
        "confs": confs,
        "base": "base_trimble.obs",
        "nav": "base.nav",
        "gps_week": week,
        "tow_start": t0,
        "tow_end": t1,
        "ref_offset_enu": URBANNAV_OFFSET_ENU,
    }
    for site, (name, week, t0, t1) in _URBANNAV_SITES.items()
    for rx, (rover, confs) in _URBANNAV_ROVERS.items()
]

# ---------------------------------------------------------------------------
# GICI open dataset, Shanghai (RTK only; held out, not used for tuning)
# ---------------------------------------------------------------------------
# Built by prepare_gici.py from the hand-downloaded sequence zips.  The rover
# is 10 Hz; the base is a 1 Hz Qianxun network VRS, so the baseline is short.
# reference.csv already holds the antenna position (prepare_gici.LEVER_RFU).
# (GPS week, first and last reference TOW)
_GICI_SEQUENCES = {
    "3.1": (2255, 114317.0, 114789.0),
    "3.2": (2255, 111852.0, 112281.0),
    "3.3": (2255, 107155.0, 107708.0),
    "4.1": (2263, 24983.1, 25369.0),
    "4.2": (2255, 115567.0, 115841.0),
    "4.3": (2255, 104100.0, 104556.0),
    "5.2": (2263, 25485.1, 27172.0),
}

GICI_CASES: list[dict] = [
    {
        "id": f"gici_{seq}",
        "city": "gici",
        "dir": f"gici/{seq}",
        "confs": [],
        "gps_week": week,
        "tow_start": t0,
        "tow_end": t1,
    }
    for seq, (week, t0, t1) in _GICI_SEQUENCES.items()
]

DATASETS = {"ppc": CASES, "urbannav": URBANNAV_CASES, "gici": GICI_CASES}


def case_files(dataset_dir, case: dict) -> dict:
    """Input paths of a case: rover, base, nav and reference files.

    Args:
        dataset_dir: Benchmark data root (``pathlib.Path``).
        case: Case metadata dict from ``CASES`` or ``URBANNAV_CASES``.

    Returns:
        Dict with ``rover``, ``base``, ``nav`` and ``ref`` paths.
    """
    d = dataset_dir / case.get("dir", f"{case.get('city')}/{case.get('run')}")
    return {
        "rover": d / case.get("rover", "rover.obs"),
        "base": d / case.get("base", "base.obs"),
        "nav": d / case.get("nav", "base.nav"),
        "ref": d / "reference.csv",
    }


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


def tow_to_gpst(week: int, tow: float) -> datetime:
    """Convert GPS week + time-of-week to a GPST datetime.

    Unlike :func:`tow_to_utc` no leap-second offset is removed: the result is
    GPS time expressed as a datetime, which is what rnx2rtkp's ``-ts``/``-te``
    epochs are compared against.

    Args:
        week: GPS week number.
        tow: Time of week in seconds.

    Returns:
        GPST as a timezone-aware datetime.
    """
    return _GPS_EPOCH + timedelta(weeks=week, seconds=tow)


def utc_to_tow(dt: datetime) -> tuple[int, float]:
    """Convert UTC datetime to GPS week + time-of-week.

    Inverse of :func:`tow_to_utc`: the leap-second offset is added back.

    Args:
        dt: UTC datetime (timezone-aware).

    Returns:
        Corresponding GPS week number and time of week in seconds.
    """
    gps_t = (dt + timedelta(seconds=GPS_LEAP) - _GPS_EPOCH).total_seconds()
    return int(gps_t // SEC_WEEK), gps_t % SEC_WEEK


def ses_to_hour(ses: str) -> int:
    """Convert an L6 archive session letter to its UTC hour.

    Args:
        ses: Session letter A–X (case-insensitive).

    Returns:
        UTC hour, 0–23.

    Raises:
        ValueError: If ``ses`` is not a single letter in A–X.
    """
    hour = SESSIONS.find(ses.upper()) if len(ses) == 1 else -1
    if hour < 0:
        raise ValueError(f"invalid session letter: {ses!r} (expected A–X)")
    return hour


def date_sessions(date: datetime, session: str | None = None) -> list[tuple[int, int, str]]:
    """Return the L6 archive sessions of a UTC calendar date.

    The archive buckets data by UTC hour, so the date is used directly — no
    GPS-time round trip, hence no leap-second or hour-boundary ambiguity.

    Args:
        date: UTC date (time-of-day is ignored).
        session: Session letter A–X (case-insensitive), or ``None`` for the
            whole day.

    Returns:
        List of (year, doy, session_letter) tuples: one entry for a given
        session letter, 24 entries for a whole day.

    Raises:
        ValueError: If ``session`` is not a valid session letter.
    """
    doy = date.timetuple().tm_yday
    letters = SESSIONS if session is None else SESSIONS[ses_to_hour(session)]
    return [(date.year, doy, s) for s in letters]


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
        entry = (t.year, doy, SESSIONS[t.hour])
        if entry not in sessions:
            sessions.append(entry)
        t += timedelta(hours=1)
    return sessions


def case_by_id(case_id: str, cases: list[dict] = CASES) -> dict:
    """Look up a case by its id string.

    Args:
        case_id: Case identifier (e.g. ``"nagoya_run1"``).
        cases: Case list to search (default: the PPC cases).

    Returns:
        Case metadata dict.

    Raises:
        KeyError: If no case with that id exists.
    """
    for c in cases:
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
        assert utc_to_tow(t0) == (c["gps_week"], c["tow_start"]), "tow/utc round trip"
        dur = (c["tow_end"] - c["tow_start"]) / 60
        sess = l6_sessions(c["gps_week"], c["tow_start"], c["tow_end"])
        print(
            f"{c['id']:18s}  {t0.strftime('%Y-%m-%d %H:%M')}"
            f"–{t1.strftime('%H:%M')} UTC  {dur:.0f} min  "
            f"sessions={[f'{y}-{d:03d}{s}' for y, d, s in sess]}"
        )
