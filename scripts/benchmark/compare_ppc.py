"""compare_ppc.py - Compare rnx2rtkp NMEA output against PPC-Dataset reference.csv.

Reads rnx2rtkp NMEA GGA output and the PPC-Dataset ground-truth reference.csv,
matches epochs by UTC time-of-day, computes ENU positioning errors, and reports
2D/3D RMS, fix rate, and convergence time.  It also reports the PPC official
score (distance-weighted share of 3D error <= 0.5 m, see ``ppc_score()``).
``--skip-epochs`` does not apply to the PPC score.

Usage
-----
    compare_ppc.py --ref <reference.csv> <result.nmea>
                   [--skip-epochs N] [--plot] [--plot-out FILE]

Output
------
    Per-epoch statistics printed to stdout.  Exit code 0.
"""

import argparse
import bisect
import math
import os
import sys
from pathlib import Path

import numpy as np

# ---------------------------------------------------------------------------
# Locate scripts/tests/ to import shared geodetic helpers
# ---------------------------------------------------------------------------
_TESTS_DIR = Path(__file__).resolve().parent.parent / "tests"
sys.path.insert(0, str(_TESTS_DIR))
from _geo import blh2xyz, nmea_to_deg, xyz2blh, xyz2enu  # noqa: E402
from cases import GPS_LEAP  # noqa: E402

# ---------------------------------------------------------------------------
# Parsers
# ---------------------------------------------------------------------------
_GGA_TAGS = ("$GPGGA", "$GNGGA")


def parse_reference(csv_path: str) -> list[tuple[float, float, float, float]]:
    """Parse PPC-Dataset reference.csv into a list of timed position records.

    Each record holds ``(utc_sod, lat_deg, lon_deg, h_ell)`` where
    ``utc_sod`` is UTC seconds-of-day derived from the GPS TOW + Week fields.

    Args:
        csv_path: Path to ``reference.csv``.

    Returns:
        List of ``(utc_sod, lat_deg, lon_deg, h_ell)`` tuples, sorted by
        ``utc_sod``.
    """
    rows = []
    with open(csv_path) as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#") or line.startswith("GPS"):
                continue
            fields = line.split(",")
            if len(fields) < 5:
                continue
            try:
                tow = float(fields[0])
                lat = float(fields[2])
                lon = float(fields[3])
                h = float(fields[4])
            except ValueError:
                continue
            # GPS TOW → UTC seconds-of-day
            utc_sod = (tow - GPS_LEAP) % 86400.0
            rows.append((utc_sod, lat, lon, h))
    rows.sort(key=lambda r: r[0])
    return rows


def parse_nmea(nmea_path: str) -> list[tuple[float, float, float, float, int, int]]:
    """Parse rnx2rtkp NMEA GGA output into a list of timed position records.

    Each record holds ``(utc_sod, lat_deg, lon_deg, h_ell, quality, n_sv)``
    where ``utc_sod`` is UTC seconds-of-day from the GGA time field (HHMMSS.ss).
    Ellipsoidal height is recovered as ``MSL + geoid_separation``.

    Args:
        nmea_path: Path to the NMEA file.

    Returns:
        List of ``(utc_sod, lat_deg, lon_deg, h_ell, quality, n_sv)`` tuples,
        sorted by ``utc_sod``.
    """
    rows = []
    with open(nmea_path) as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue
            if "*" in line:
                line = line[: line.index("*")]
            fields = line.split(",")
            if len(fields) < 10:
                continue
            if fields[0] not in _GGA_TAGS:
                continue
            try:
                quality = int(fields[6])
                if quality == 0 or not fields[2] or not fields[4]:
                    continue
                # GGA time field: HHMMSS.ss
                t = fields[1]
                hh, mm, ss = int(t[0:2]), int(t[2:4]), float(t[4:])
                utc_sod = hh * 3600.0 + mm * 60.0 + ss

                lat = nmea_to_deg(fields[2], fields[3])
                lon = nmea_to_deg(fields[4], fields[5])
                alt_msl = float(fields[9])
                geoid_sep = 0.0
                if len(fields) > 11 and fields[11]:
                    try:
                        geoid_sep = float(fields[11])
                    except ValueError:
                        pass
                h_ell = alt_msl + geoid_sep
                n_sv = 0
                if len(fields) > 7 and fields[7]:
                    try:
                        n_sv = int(fields[7])
                    except ValueError:
                        pass
                rows.append((utc_sod, lat, lon, h_ell, quality, n_sv))
            except (ValueError, IndexError):
                continue
    rows.sort(key=lambda r: r[0])
    return rows


# ---------------------------------------------------------------------------
# Time matching
# ---------------------------------------------------------------------------
def _match_epochs(
    ref_rows: list[tuple[float, float, float, float]],
    nmea_rows: list[tuple[float, float, float, float, int]],
    tol: float = 0.15,
) -> list[tuple[tuple, tuple]]:
    """Pair reference and NMEA epochs by UTC seconds-of-day.

    Handles runs that start near midnight by correcting |Δt| > 43200 s with ±86400 s.

    Args:
        ref_rows: Sorted list from ``parse_reference()``.
        nmea_rows: Sorted list from ``parse_nmea()``.
        tol: Maximum time difference in seconds to accept a match.

    Returns:
        List of ``(ref_row, nmea_row)`` pairs.
    """
    ref_sods = [r[0] for r in ref_rows]
    pairs = []
    for nrow in nmea_rows:
        ns = nrow[0]
        i = bisect.bisect_left(ref_sods, ns)
        for j in (i - 1, i):
            if 0 <= j < len(ref_rows):
                dt = ns - ref_rows[j][0]
                # Midnight wrap: if |dt| > half day, add/subtract 86400
                if dt > 43200.0:
                    dt -= 86400.0
                elif dt < -43200.0:
                    dt += 86400.0
                if abs(dt) <= tol:
                    pairs.append((ref_rows[j], nrow))
                    break
    return pairs


# ---------------------------------------------------------------------------
# PPC official score
# ---------------------------------------------------------------------------
PPC_SCORE_THRESHOLD_3D = 0.50


def continuous_epochs(ref_rows: list[tuple], sol_rows: list[tuple]) -> tuple[list, list]:
    """Put reference and solution records on one continuous time axis.

    Both parsers key records by UTC (or GPS) seconds-of-day and sort on it, so
    a run across midnight comes back out of order.  Times are taken relative to
    the first reference record and unwrapped into (-12 h, +12 h], then both
    lists are re-sorted on that time.  Records with the same time keep their
    file order (the sort key is the time only), so a hold takes the last one.

    Args:
        ref_rows: Records whose first element is seconds-of-day.
        sol_rows: Records whose first element is seconds-of-day.

    Returns:
        ``(ref, sol)``: lists of ``(t, record)`` sorted by ``t`` (seconds).
    """
    if not ref_rows:
        return [], []
    t0 = ref_rows[0][0]

    def rel(t: float) -> float:
        return (t - t0 + 43200.0) % 86400.0 - 43200.0

    ref = sorted(((rel(r[0]), r) for r in ref_rows), key=lambda x: x[0])
    sol = sorted(((rel(n[0]), n) for n in sol_rows), key=lambda x: x[0])
    return ref, sol


def observation_grid(
    ref: list[tuple[float, tuple]], interval: float | None, phase: float | None
) -> tuple[list[tuple[float, tuple]], float, float]:
    """Reference epochs on the rover observation grid.

    Args:
        ref: ``(t, record)`` list from ``continuous_epochs()``; ``record[0]``
            is the UTC seconds-of-day.
        interval: Rover observation interval in seconds.  None, or not longer
            than the reference interval, keeps every reference epoch.
        phase: UTC time of day of an observation epoch modulo ``interval``
            (0 if None).

    Returns:
        ``(grid, dt_grid, tol)``: the kept ``(t, record)`` pairs, the grid
        interval, and the time tolerance (a quarter of the reference
        interval) used to decide that an epoch lies on the grid or that a
        solution belongs to an epoch.
    """
    dt_ref = _median_interval([t for t, _ in ref])
    tol = 0.25 * dt_ref if not math.isnan(dt_ref) else 1e-3
    if interval is None or math.isnan(dt_ref) or interval <= dt_ref + tol:
        return ref, dt_ref, tol
    ph = phase or 0.0
    grid = [
        (t, r)
        for t, r in ref
        if abs((r[0] - ph) / interval - round((r[0] - ph) / interval)) * interval <= tol
    ]
    return grid, interval, tol


def shift_reference(rows: list[tuple], offset_enu: tuple[float, float, float]) -> list[tuple]:
    """Move reference positions by a constant local ENU offset.

    Used for datasets whose reference trajectory has a documented constant
    offset from the solutions (UrbanNav Tokyo, see ``cases.URBANNAV_OFFSET_ENU``).
    Shifting the reference, not the solutions, keeps solution records intact.

    Args:
        rows: ``(t, lat_deg, lon_deg, h, ...)`` records.
        offset_enu: ``(east, north, up)`` in metres, added at each position.

    Returns:
        Records with the shifted position and the remaining fields unchanged.
    """
    out = []
    de, dn, du = offset_enu
    for r in rows:
        la, lo = math.radians(r[1]), math.radians(r[2])
        dx = np.array(
            [
                -math.sin(lo) * de
                - math.sin(la) * math.cos(lo) * dn
                + math.cos(la) * math.cos(lo) * du,
                math.cos(lo) * de
                - math.sin(la) * math.sin(lo) * dn
                + math.cos(la) * math.sin(lo) * du,
                math.cos(la) * dn + math.sin(la) * du,
            ]
        )
        lat, lon, h = xyz2blh(*(blh2xyz(r[1], r[2], r[3]) + dx))
        out.append((r[0], lat, lon, h) + tuple(r[4:]))
    return out


def ppc_score(
    ref_rows: list[tuple[float, float, float, float]],
    nmea_rows: list[tuple],
    threshold_3d: float = PPC_SCORE_THRESHOLD_3D,
    interval: float | None = None,
    phase: float | None = None,
) -> tuple[float, float]:
    """Score a solution with the PPC-Dataset official metric.

    The score is the share of the travelled reference distance on which the
    3D error is at most ``threshold_3d``.  At every reference epoch the latest
    solution at or before that epoch is held (any quality flag), so epochs
    with no solution yet count as failures.  The distance between consecutive
    reference epochs is credited to the later epoch.  This matches the PPC
    rules (distance per epoch, hold, 0.5 m) and rtklibexplorer's
    ``score_ppc_sol.py`` (RTKLIB-EX util/ppc-dataset).

    When the rover observes more slowly than the reference (UrbanNav F9P:
    5 Hz against 10 Hz), pass ``interval`` / ``phase``: the score is then taken
    on the reference epochs of the observation grid, with distances between
    those epochs.  Scoring a 5 Hz solver at 10 Hz would hold every other
    position 0.1 s late, which at driving speed alone exceeds 0.5 m.

    Args:
        ref_rows: Sorted list from ``parse_reference()``.
        nmea_rows: Sorted list from ``parse_nmea()``.
        threshold_3d: 3D error bound in metres (PPC rules: 0.5 m).
        interval: Rover observation interval in seconds (see
            ``observation_grid()``); None scores every reference epoch.
        phase: Observation epoch phase for ``interval``.

    Returns:
        ``(good_m, total_m)``: distance within the bound and total distance.
        Runs can be combined two ways (``run_benchmark.print_ppc_scores()``
        prints both): ``sum(good_m) / sum(total_m)``, one long drive, as
        RTKLIB-EX ``score_ppc_sol.py`` does; or the mean of the per-run
        shares, as the PPC2024 organizers describe.  The organizers take the
        distance as reference speed x time; the position-difference distance
        used here changes the per-run shares by <= 0.1 pt on the six runs.
    """
    if not ref_rows:
        return 0.0, 0.0
    ref_c, sol_c = continuous_epochs(ref_rows, nmea_rows)
    ref_c, _, _ = observation_grid(ref_c, interval, phase)
    ref = [(t, blh2xyz(r[1], r[2], r[3])) for t, r in ref_c]
    sol = [(t, blh2xyz(n[1], n[2], n[3])) for t, n in sol_c]

    good = total = 0.0
    j = 0
    held = None
    for i, (t, xyz) in enumerate(ref):
        while j < len(sol) and sol[j][0] <= t + 1e-3:
            held = sol[j][1]
            j += 1
        if i == 0:
            continue
        w = float(np.linalg.norm(xyz - ref[i - 1][1]))
        total += w
        if held is not None and float(np.linalg.norm(held - xyz)) <= threshold_3d:
            good += w
    return good, total


def rinex_obs_timing(obs_path: str) -> tuple[float | None, float | None]:
    """Observation interval and epoch phase of a RINEX 3 OBS file.

    The interval is the ``INTERVAL`` header record, else the median spacing of
    the first observation records (event records are ignored).  The phase is
    the UTC time of day of the first observation epoch modulo the interval, so
    a 5 Hz rover observing at .1, .3, ... s is not gridded at .0, .2, ... s,
    and a 5 s GPST grid is mapped onto the UTC reference.  The epoch time
    system comes from ``TIME OF FIRST OBS`` (GPS when absent).

    Args:
        obs_path: Path to a RINEX 3.xx observation file.

    Returns:
        ``(interval_s, phase_s)``; either is None when it cannot be determined.
    """
    times = []
    interval = None
    utc_minus = float(GPS_LEAP)  # epoch time minus UTC, by TIME OF FIRST OBS system
    with open(obs_path, errors="replace") as fh:
        for line in fh:
            if line[60:].startswith("TIME OF FIRST OBS"):
                # GLO epochs are UTC(SU) = UTC + 3 h (a whole number of any
                # interval); BDT = GPST - 14 s; GAL / QZS follow GPST.
                utc_minus = {"GLO": 0.0, "BDT": GPS_LEAP - 14.0}.get(line[48:51], float(GPS_LEAP))
            if line[60:].startswith("INTERVAL"):
                try:
                    interval = float(line[:10])
                except ValueError:
                    pass
            if line[60:].startswith("END OF HEADER"):
                break
        for line in fh:
            f = line[1:].split()
            # Observation records only (epoch flag 0 / 1); events do not set
            # the grid.
            if line.startswith(">") and len(f) > 6 and f[6] in ("0", "1"):
                try:
                    times.append(int(f[3]) * 3600 + int(f[4]) * 60 + float(f[5]))
                except (IndexError, ValueError):
                    continue
                if len(times) >= 50:
                    break
    if interval is None:
        dt = _median_interval(times)
        interval = None if math.isnan(dt) else dt
    # The references and NMEA are UTC.
    phase = (times[0] - utc_minus) % interval if interval and times else None
    return interval, phase


def _median_interval(times: list[float]) -> float:
    """Median positive spacing of sorted epoch times, or NaN for fewer than two."""
    d = np.diff(np.asarray(times, dtype=float))
    d = d[d > 1e-6]
    return float(np.median(d)) if d.size else math.nan


def reliability_metrics(
    ref_rows: list[tuple[float, float, float, float]],
    nmea_rows: list[tuple],
    threshold_3d: float = PPC_SCORE_THRESHOLD_3D,
    fix_quality: int = 4,
    interval: float | None = None,
    phase: float | None = None,
) -> dict:
    """Fix-reliability and solution-gap metrics on the expected-solution grid.

    The PPC score holds the latest solution, so it cannot tell a solver that
    stopped from one that kept solving.  These metrics are evaluated on the
    epochs at which a new solution was expected instead: the reference epochs
    that fall on the observation interval (e.g. every other epoch for 5 Hz
    observations against a 10 Hz reference).  The interval comes from the
    caller (the rover observation interval), never from the solutions, which
    would hide gaps.  Without it the grid is every reference epoch.  A grid
    epoch has a solution when one lies within a quarter of the reference
    interval.

    Args:
        ref_rows: Sorted list from ``parse_reference()``.
        nmea_rows: Sorted list from ``parse_nmea()``.
        threshold_3d: 3D error bound in metres for a correct fix.
        fix_quality: Quality flag of an integer-fixed solution (NMEA GGA: 4).
        interval: Expected solution interval in seconds (rover observation
            interval); None or not longer than the reference interval uses
            every reference epoch.
        phase: UTC time of day of an observation epoch modulo ``interval`` (0 if
            None), which anchors the decimated grid.

    Returns:
        Dict with ``n_expected``, ``missing_rate`` (% of grid epochs without a
        solution), ``n_fix3d`` (fixed grid epochs), ``misfix3d_rate`` (% of
        those with 3D error > ``threshold_3d``, NaN without fixes) and
        ``misfix3d_longest_s`` (longest run of consecutive misfixed grid
        epochs in seconds; a missing or non-misfixed epoch ends a run).
    """
    out = {
        "n_expected": 0,
        "missing_rate": math.nan,
        "n_fix3d": 0,
        "misfix3d_rate": math.nan,
        "misfix3d_longest_s": math.nan,
    }
    if len(ref_rows) < 2:
        return out
    ref, sol = continuous_epochs(ref_rows, nmea_rows)
    grid, dt_grid, tol = observation_grid(ref, interval, phase)

    sol_t = [t for t, _ in sol]
    n_missing = n_fix = n_misfix = 0
    run = longest = 0
    prev_t = None
    for t, r in grid:
        # A gap in the reference itself also ends a misfix run.
        if prev_t is not None and t - prev_t > 1.5 * dt_grid:
            run = 0
        prev_t = t
        i = bisect.bisect_left(sol_t, t - tol)
        s = None
        if i < len(sol) and abs(sol_t[i] - t) <= tol:
            # Same-time records: use the last one, as the hold of ppc_score() does.
            i = bisect.bisect_right(sol_t, sol_t[i], lo=i) - 1
            s = sol[i][1]
        bad_fix = False
        if s is None:
            n_missing += 1
        elif s[4] == fix_quality:
            n_fix += 1
            err = float(np.linalg.norm(blh2xyz(s[1], s[2], s[3]) - blh2xyz(r[1], r[2], r[3])))
            if err > threshold_3d:
                n_misfix += 1
                bad_fix = True
        run = run + 1 if bad_fix else 0
        longest = max(longest, run)

    out["n_expected"] = len(grid)
    out["missing_rate"] = 100.0 * n_missing / len(grid) if grid else math.nan
    out["n_fix3d"] = n_fix
    if n_fix:
        out["misfix3d_rate"] = 100.0 * n_misfix / n_fix
        out["misfix3d_longest_s"] = longest * dt_grid
    return out


def format_official(ppc: tuple[float, float], rel: dict) -> list[str]:
    """Format the PPC score and the reliability metrics as report lines.

    Args:
        ppc: ``(good_m, total_m)`` from ``ppc_score()``.
        rel: Dict from ``reliability_metrics()``.

    Returns:
        Lines without trailing newlines.
    """
    good_m, total_m = ppc
    thr = f"{PPC_SCORE_THRESHOLD_3D * 100:.0f} cm"
    lines = []
    if total_m > 0:
        lines.append(
            f"PPC score         : {100.0 * good_m / total_m:.1f}%  "
            f"({good_m:.1f} / {total_m:.1f} m within 3D {thr}; official metric, all epochs)"
        )
    else:
        lines.append("PPC score         : n/a (reference has no travelled distance)")
    lines.append(
        f"missing_rate      : {_pct(rel['missing_rate'])}  "
        f"(of {rel['n_expected']} expected-solution epochs)"
    )
    lines.append(
        f"misfix3d_rate     : {_pct(rel['misfix3d_rate'])}  "
        f"(of {rel['n_fix3d']} fixed epochs, 3D > {thr})"
    )
    longest = rel["misfix3d_longest_s"]
    lines.append(f"misfix3d_longest  : {'nan' if math.isnan(longest) else f'{longest:.1f} s'}")
    return lines


def _pct(v: float) -> str:
    return "nan" if math.isnan(v) else f"{v:.1f}%"


# ---------------------------------------------------------------------------
# Metrics
# ---------------------------------------------------------------------------
def compute_metrics(
    pairs: list[tuple[tuple, tuple]],
    skip_epochs: int = 0,
    threshold_2d: float = float("nan"),
    acc_threshold_2d: float = 0.30,
) -> dict | None:
    """Compute positioning error statistics from matched epoch pairs.

    Args:
        pairs: Matched ``(ref_row, nmea_row)`` pairs from ``_match_epochs()``.
        skip_epochs: Number of initial epochs to discard (convergence exclusion).
        threshold_2d: Horizontal error threshold in metres for threshold-based
            fix rate and convergence time (e.g. 0.30 for PPP modes where Q=4
            is never set).  When ``nan``, only Q=4-based metrics are computed.
        acc_threshold_2d: Accuracy threshold in metres, applied *within* each
            tier (``acc_rate_fix`` / ``acc_rate_ff`` / ``acc_rate_all``).  On
            the fix tier its complement is the misfix rate: a Q=4 epoch beyond
            this bound is a wrong integer fix, not a degraded one.  Always
            computed, including for the AR modes where ``threshold_2d`` is nan.

    Returns:
        Dict of statistics, or ``None`` if no usable pairs remain.
    """
    pairs = pairs[skip_epochs:]
    if not pairs:
        return None

    # Per-epoch ENU projection: compute ECEF error vector and project to local
    # ENU using the ground-truth lat/lon as the origin for that epoch.
    enu_errors = []
    q_list = []
    sv_list = []
    abs_tows = []

    for ref_row, nrow in pairs:
        r_lat, r_lon, r_h = ref_row[1], ref_row[2], ref_row[3]
        n_lat, n_lon, n_h = nrow[1], nrow[2], nrow[3]
        q = nrow[4]
        n_sv = nrow[5] if len(nrow) > 5 else 0

        true_xyz = blh2xyz(r_lat, r_lon, r_h)
        test_xyz = blh2xyz(n_lat, n_lon, n_h)
        dx = test_xyz - true_xyz
        enu = xyz2enu(dx, r_lat, r_lon)
        enu_errors.append(enu)
        q_list.append(q)
        sv_list.append(n_sv)
        abs_tows.append(ref_row[0])  # utc_sod for convergence calculation

    en = np.array(enu_errors)
    sv = np.array(sv_list, dtype=float)
    n = len(en)

    horiz = np.sqrt(en[:, 0] ** 2 + en[:, 1] ** 2)
    e3d = np.sqrt(en[:, 0] ** 2 + en[:, 1] ** 2 + en[:, 2] ** 2)

    fix_mask = np.array([q == 4 for q in q_list])
    n_fix = int(fix_mask.sum())

    # Mean satellite count per tier
    mean_sv_fix = float(np.mean(sv[fix_mask])) if n_fix > 0 else math.nan

    # Fix-only subset (Q=4)
    if n_fix > 0:
        h_fix = horiz[fix_mask]
        e_fix = e3d[fix_mask]
        rms_2d_fix = float(np.sqrt(np.mean(h_fix**2)))
        rms_3d_fix = float(np.sqrt(np.mean(e_fix**2)))
        p68_2d_fix = float(np.percentile(h_fix, 68))
        p95_2d_fix = float(np.percentile(h_fix, 95))
    else:
        rms_2d_fix = rms_3d_fix = p68_2d_fix = p95_2d_fix = math.nan

    # Fix+Float subset: Q=4 (fix), Q=5 (RTK float), Q=3 (PPP float/MADOCA)
    # Excludes Q=1 (SPP fallback) and Q=2 (DGPS).
    ff_mask = np.array([q in (3, 4, 5) for q in q_list])
    n_ff = int(ff_mask.sum())
    mean_sv_ff = float(np.mean(sv[ff_mask])) if n_ff > 0 else math.nan
    mean_sv_all = float(np.mean(sv))
    if n_ff > 0:
        h_ff = horiz[ff_mask]
        e_ff = e3d[ff_mask]
        rms_2d_ff = float(np.sqrt(np.mean(h_ff**2)))
        rms_3d_ff = float(np.sqrt(np.mean(e_ff**2)))
        p68_2d_ff = float(np.percentile(h_ff, 68))
        p95_2d_ff = float(np.percentile(h_ff, 95))
    else:
        rms_2d_ff = rms_3d_ff = p68_2d_ff = p95_2d_ff = math.nan

    # Per-tier accuracy rate: fraction of that tier's epochs inside the bound.
    # On the fix tier, 100% minus this is the misfix rate.
    below = horiz < acc_threshold_2d
    acc_rate_fix = float(np.mean(below[fix_mask]) * 100.0) if n_fix > 0 else math.nan
    acc_rate_ff = float(np.mean(below[ff_mask]) * 100.0) if n_ff > 0 else math.nan
    acc_rate_all = float(np.mean(below) * 100.0)

    # Q=4-based convergence: first run of ≥30 consecutive Q=4 epochs
    conv_time_s = math.nan
    run = 0
    run_start_idx = 0
    for i, q in enumerate(q_list):
        if q == 4:
            if run == 0:
                run_start_idx = i
            run += 1
            if run >= 30:
                conv_time_s = abs_tows[run_start_idx] - abs_tows[0]
                break
        else:
            run = 0

    # Threshold-based metrics (for PPP modes where Q=4 is not produced)
    thr_rate = math.nan
    conv_thr_s = math.nan
    if not math.isnan(threshold_2d):
        thr_mask = horiz < threshold_2d
        n_thr = int(thr_mask.sum())
        thr_rate = n_thr / n * 100.0
        # TTFF: first run of ≥30 consecutive epochs below threshold
        run = 0
        run_start_idx = 0
        for i, below in enumerate(thr_mask):
            if below:
                if run == 0:
                    run_start_idx = i
                run += 1
                if run >= 30:
                    conv_thr_s = abs_tows[run_start_idx] - abs_tows[0]
                    break
            else:
                run = 0

    return {
        "n_matched": n,
        # Q=4 fix
        "n_fix": n_fix,
        "fix_rate": n_fix / n * 100.0,
        "acc_rate_fix": acc_rate_fix,
        "mean_sv_fix": mean_sv_fix,
        "rms_2d_fix": rms_2d_fix,
        "rms_3d_fix": rms_3d_fix,
        "p68_2d_fix": p68_2d_fix,
        "p95_2d_fix": p95_2d_fix,
        "conv_time_s": conv_time_s,
        # Q=4 or Q=5 (fix + float)
        "n_ff": n_ff,
        "ff_rate": n_ff / n * 100.0,
        "acc_rate_ff": acc_rate_ff,
        "mean_sv_ff": mean_sv_ff,
        "rms_2d_ff": rms_2d_ff,
        "rms_3d_ff": rms_3d_ff,
        "p68_2d_ff": p68_2d_ff,
        "p95_2d_ff": p95_2d_ff,
        # All epochs
        "acc_threshold_2d": acc_threshold_2d,
        "acc_rate_all": acc_rate_all,
        "mean_sv_all": mean_sv_all,
        "rms_2d_all": float(np.sqrt(np.mean(horiz**2))),
        "rms_3d_all": float(np.sqrt(np.mean(e3d**2))),
        "p50_2d_all": float(np.percentile(horiz, 50)),
        "p68_2d_all": float(np.percentile(horiz, 68)),
        "p95_2d_all": float(np.percentile(horiz, 95)),
        "max_2d_all": float(np.max(horiz)),
        "rms_e": float(np.sqrt(np.mean(en[:, 0] ** 2))),
        "rms_n": float(np.sqrt(np.mean(en[:, 1] ** 2))),
        "rms_u": float(np.sqrt(np.mean(en[:, 2] ** 2))),
        # Raw arrays (for plotting)
        "enu_errors": en,
        "q_list": q_list,
        "abs_tows": abs_tows,
        # Threshold-based (for PPP modes)
        "threshold_2d": threshold_2d,
        "thr_rate": thr_rate,
        "conv_thr_s": conv_thr_s,
    }


# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------
def plot_results(m: dict, title: str = "", output_path: str = "ppc_compare.png") -> None:
    """Generate ENU time-series and Q-flag plot.

    Args:
        m: Metrics dict from ``compute_metrics()``.
        title: Plot title prefix.
        output_path: Output PNG file path.
    """
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    en = m["enu_errors"] * 100  # m → cm
    idx = np.arange(m["n_matched"])

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
    ax1.plot(idx, en[:, 0], label="East", alpha=0.8, linewidth=0.6)
    ax1.plot(idx, en[:, 1], label="North", alpha=0.8, linewidth=0.6)
    ax1.plot(idx, en[:, 2], label="Up", alpha=0.5, linewidth=0.6, linestyle="--")
    ax1.axhline(0, color="k", linewidth=0.4)
    ax1.set_ylabel("Position error [cm]")
    ax1.set_title(
        f"{title}  2D RMS {m['rms_2d_all'] * 100:.2f} cm  |  "
        f"Fix {m['fix_rate']:.1f}%  |  "
        f"2D RMS(fix) {m['rms_2d_fix'] * 100:.2f} cm"
        if not math.isnan(m["rms_2d_fix"])
        else f"{title}  2D RMS {m['rms_2d_all'] * 100:.2f} cm  |  No fix"
    )
    ax1.legend(loc="upper right")
    ax1.grid(True, alpha=0.3)

    ax2.scatter(idx, m["q_list"], s=4, alpha=0.5)
    ax2.set_ylabel("GGA quality")
    ax2.set_xlabel("Matched epoch")
    ax2.set_title("GGA quality  (Q=4: fix,  Q=5: float,  Q=1: SPP)")
    ax2.set_yticks([1, 2, 4, 5])
    ax2.set_yticklabels(["1:SPP", "2:DGPS", "4:Fix", "5:Float"])
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"  Plot saved: {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def _fmt(v: float, unit: str = "m", digits: int = 3) -> str:
    """Format a metric value, showing 'nan' for math.nan."""
    return "nan" if math.isnan(v) else f"{v:.{digits}f} {unit}"


def main() -> int:
    """Run standalone comparison of one NMEA file against one reference.csv."""
    p = argparse.ArgumentParser(
        description="Compare rnx2rtkp NMEA output against PPC-Dataset reference.csv"
    )
    p.add_argument("--ref", required=True, metavar="CSV", help="PPC-Dataset reference.csv")
    p.add_argument("result", metavar="NMEA", help="rnx2rtkp NMEA output file")
    p.add_argument(
        "--skip-epochs",
        type=int,
        default=0,
        help="Initial epochs to skip (convergence exclusion, default: 0)",
    )
    p.add_argument(
        "--interval",
        type=float,
        default=None,
        help="Expected solution interval in seconds for the missing / misfix3d"
        " metrics (default: the reference interval; see --obs)",
    )
    p.add_argument(
        "--obs",
        default="",
        help="Rover RINEX OBS; its interval and epoch phase are used unless --interval is given",
    )
    p.add_argument(
        "--phase",
        type=float,
        default=None,
        help="Epoch phase for --interval: UTC time of day of an observation epoch modulo the"
        " interval (default 0)",
    )
    p.add_argument("--plot", action="store_true", help="Generate ENU time-series PNG")
    p.add_argument("--plot-out", default="", help="Output path for plot (default: <result>.png)")
    args = p.parse_args()

    if not os.path.isfile(args.ref):
        print(f"FAIL: reference not found: {args.ref}", file=sys.stderr)
        return 1
    if not os.path.isfile(args.result):
        print(f"FAIL: result not found: {args.result}", file=sys.stderr)
        return 1

    ref_rows = parse_reference(args.ref)
    nmea_rows = parse_nmea(args.result)
    if not ref_rows:
        print("FAIL: no data in reference.csv", file=sys.stderr)
        return 1

    print(f"Reference : {args.ref}")
    print(f"Result    : {args.result}")
    if args.skip_epochs:
        print(f"Skip      : {args.skip_epochs} epochs")
    print()
    interval, phase = args.interval, args.phase
    if interval is None and args.obs:
        interval, phase = rinex_obs_timing(args.obs)
    # The official score and the reliability metrics cover every reference
    # epoch, so they are reported even when no epoch matches.
    for line in format_official(
        ppc_score(ref_rows, nmea_rows),
        reliability_metrics(ref_rows, nmea_rows, interval=interval, phase=phase),
    ):
        print(line)
    print()

    if not nmea_rows:
        print("FAIL: no GGA sentences in result", file=sys.stderr)
        return 1

    pairs = _match_epochs(ref_rows, nmea_rows)
    if not pairs:
        print("FAIL: no matching epochs (check GPS week / time range)", file=sys.stderr)
        return 1

    m = compute_metrics(pairs, args.skip_epochs)
    if m is None:
        print("FAIL: no usable epochs after skip", file=sys.stderr)
        return 1

    print(f"Matched epochs : {m['n_matched']}")
    print(f"Fix epochs     : {m['n_fix']}  ({m['fix_rate']:.1f}%)")
    print()
    print("  ENU RMS (all epochs):")
    print(f"    East   : {m['rms_e'] * 100:8.3f} cm")
    print(f"    North  : {m['rms_n'] * 100:8.3f} cm")
    print(f"    Up     : {m['rms_u'] * 100:8.3f} cm")
    print()
    print("  2D horizontal (all epochs):")
    print(f"    RMS    : {_fmt(m['rms_2d_all'], 'm')}")
    print(f"    1σ     : {_fmt(m['p68_2d_all'], 'm')}  (68th pctile)")
    print(f"    95%    : {_fmt(m['p95_2d_all'], 'm')}  (95th pctile)")
    print(f"    Max    : {_fmt(m['max_2d_all'], 'm')}")
    print()
    print("  2D horizontal (Q=4 fix only):")
    print(f"    RMS    : {_fmt(m['rms_2d_fix'], 'm')}")
    print(f"    1σ     : {_fmt(m['p68_2d_fix'], 'm')}")
    print(f"    95%    : {_fmt(m['p95_2d_fix'], 'm')}")
    thr_cm = m["acc_threshold_2d"] * 100
    acc_fix = m["acc_rate_fix"]
    if math.isnan(acc_fix):
        print(f"    <{thr_cm:.0f}cm  : nan")
    else:
        print(f"    <{thr_cm:.0f}cm  : {acc_fix:.1f}%   (misfix {100.0 - acc_fix:.1f}%)")
    print()
    conv = m["conv_time_s"]
    conv_str = _fmt(conv, "s", 0) if not math.isnan(conv) else "never (no 30-epoch fix run)"
    print(f"  Convergence: {conv_str}")

    if args.plot:
        out = args.plot_out or (os.path.splitext(args.result)[0] + ".png")
        plot_results(m, title=os.path.basename(args.result), output_path=out)

    return 0


if __name__ == "__main__":
    sys.exit(main())
