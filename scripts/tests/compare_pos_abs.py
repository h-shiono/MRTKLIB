#!/usr/bin/env python3
"""compare_pos_abs.py - Absolute accuracy check against geodetic truth.

Compares a solution file — RTKLIB .pos or NMEA GGA — against a true reference
coordinate derived from either an IGS SINEX file (for IGS/CORS stations such as
MIZU) or a GSI F5 daily coordinate file (for GEONET stations).

Input formats
-------------
The format is sniffed from the file content (a leading ``$`` sentence means
NMEA); ``--format pos|nmea`` overrides the detection.  NMEA input is normalised
onto the .pos convention so every metric below is format-independent:

  Position  ellipsoidal height is recovered as GGA field[9] (MSL) + field[11]
            (geoid separation N).  When field[11] is absent or zero the Up
            component cannot be recovered — the run then requires --use-2d.
  Q flag    GGA quality is mapped to the RTKLIB solution status (the inverse
            of nmea_solq[] in src/pos/mrtk_sol.c), so Q=1 is an integer fix and
            Q=6 is PPP for both formats.

Reference derivation
--------------------
SINEX:
    Station coordinate at the SINEX reference epoch, optionally propagated to
    a target date using velocity estimates (VELX/VELY/VELZ) in the same file.
    Formal 3D uncertainty  =  sqrt(σ_X² + σ_Y² + σ_Z²).

GSI F5:
    Median of daily ECEF coordinates over the 15-day window centred on the
    evaluation date (D−7 … D+7).
    Reference precision  =  68th-percentile of 3D scatter within that window.

Accuracy metrics
----------------
For each epoch in the test .pos file, the 3D position error against the fixed
true coordinate is computed (ECEF difference projected to ENU).

Reported statistics
-------------------
  1σ   — 68th percentile of per-epoch 3D errors  (≈ 1 standard deviation)
  95%  — 95th percentile of per-epoch 3D errors
  Satellite count (mean / min / max) over the epochs in a precise solution
  state — Fix, Float or PPP (Q = 1, 2, 6); Single and DGPS epochs are excluded
  so the figure describes the epochs the accuracy statistics come from.
  TTFF — first epoch that is integer-fixed (Q=1, i.e. GGA quality 4) *and*
  within 30 cm horizontally and 50 cm vertically (--ttff-h / --ttff-v).
  Measured from the first epoch of the file: --skip-epochs discards the
  convergence transient, which is what TTFF is about, so it is not applied.
  PPP conv — the same bounds applied to the PPP state (Q=6, GGA quality 3),
  reported as a reference convergence time.  Printed only when the solution
  actually contains PPP epochs.

Pass / Fail
-----------
For each metric (1σ, 95%) the test passes when at least ONE of the following
holds:

  A.  metric < tolerance            (algorithm meets the required accuracy)
  B.  metric < reference precision  (algorithm is at least as good as the truth)

Criterion B acts as a safety valve: when the reference itself is noisy (e.g.
a sparsely sampled F5 file), the test cannot fairly demand better accuracy than
the truth provides.

Overall result is PASS only when both the 1σ criterion and the 95% criterion
individually pass.

Usage
-----
    compare_pos_abs.py --sinex FILE.SNX[.gz] --station CODE [--epoch YYYY/MM/DD]
                       [options] test.pos|test.nmea
    compare_pos_abs.py --f5 FILE --date YYYY/MM/DD
                       [options] test.pos|test.nmea
    compare_pos_abs.py --llh LAT,LON,H [--ref-precision FLOAT]
                       [options] test.pos|test.nmea
    compare_pos_abs.py --ecef X,Y,Z [--ref-precision FLOAT]
                       [options] test.pos|test.nmea

Options
-------
    --format FMT        Input format: auto (default), pos, or nmea
    --tolerance FLOAT   Tolerance for criterion A in metres (default 0.030)
    --ttff-h FLOAT      TTFF horizontal bound in metres (default 0.30)
    --ttff-v FLOAT      TTFF vertical bound in metres (default 0.50)
    --skip-epochs INT   Skip N initial epochs (convergence transient)
    --use-2d            Evaluate pass/fail on 2D horizontal error (default: 3D)
    --plot              Generate ENU error time-series plot

Note on PPP vertical accuracy
------------------------------
PPP Up errors are dominated by tropospheric delay model residuals and are
typically 3–5× larger than horizontal errors.  Use --use-2d for tests where
the purpose is to validate horizontal positioning performance.
"""

import argparse
import gzip
import math
import sys
from datetime import datetime, timedelta

import numpy as np
from _geo import blh2xyz, nmea_to_deg, xyz2blh, xyz2enu  # noqa: E402


# ---------------------------------------------------------------------------
# SINEX parser
# ---------------------------------------------------------------------------
def _sinex_epoch(s):
    """Parse SINEX epoch string 'YY:DOY:SOD' → datetime."""
    yy, doy, sod = (int(t) for t in s.split(":"))
    year = 2000 + yy if yy < 79 else 1900 + yy
    return datetime(year, 1, 1) + timedelta(days=doy - 1, seconds=sod)


def parse_sinex(filepath, station_code):
    """Extract station coordinate and formal sigma from an IGS SINEX file.

    Args:
        filepath: Path to .SNX or .SNX.gz file.
        station_code: 4-character station code (case-insensitive, e.g. 'MIZU').

    Returns:
        dict with keys:
            xyz       – np.array([X, Y, Z]) in metres (ITRF)
            sigma_3d  – formal 3D uncertainty sqrt(σX²+σY²+σZ²) in metres
            epoch     – datetime of reference epoch
            estimates – raw dict mapping type→(value, sigma, epoch)
    """
    code = station_code.upper()[:4]
    opener = gzip.open if str(filepath).endswith(".gz") else open
    estimates = {}
    in_est = False

    with opener(filepath, "rt", encoding="ascii", errors="replace") as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            if line.startswith("+SOLUTION/ESTIMATE"):
                in_est = True
                continue
            if line.startswith("-SOLUTION/ESTIMATE"):
                break
            if not in_est or line.startswith("*"):
                continue
            parts = line.split()
            if len(parts) < 10:
                continue
            # INDEX TYPE CODE PT SOLN REF_EPOCH UNIT S VALUE STDDEV
            ptype, pcode = parts[1], parts[2].upper()
            if pcode != code:
                continue
            epoch = _sinex_epoch(parts[5])
            value, sigma = float(parts[8]), float(parts[9])
            estimates[ptype] = (value, sigma, epoch)

    for req in ("STAX", "STAY", "STAZ"):
        if req not in estimates:
            raise ValueError(f"Station '{code}' not found in SINEX (missing {req})")

    x, sx, epoch = estimates["STAX"]
    y, sy, _ = estimates["STAY"]
    z, sz, _ = estimates["STAZ"]
    sigma_3d = math.sqrt(sx * sx + sy * sy + sz * sz)
    return {
        "xyz": np.array([x, y, z]),
        "sigma_3d": sigma_3d,
        "epoch": epoch,
        "estimates": estimates,
    }


def sinex_propagate(sinex, target_epoch):
    """Apply linear velocity model to propagate SINEX coordinate.

    Uses VELX/VELY/VELZ entries if present; otherwise returns the coordinate
    unchanged (appropriate when target epoch ≈ reference epoch).

    Args:
        sinex: dict returned by parse_sinex().
        target_epoch: datetime to propagate to.

    Returns:
        np.array([X, Y, Z]) propagated to target_epoch.
    """
    xyz = sinex["xyz"].copy()
    dt_yr = (target_epoch - sinex["epoch"]).total_seconds() / (365.25 * 86400)
    for i, vtype in enumerate(("VELX", "VELY", "VELZ")):
        if vtype in sinex["estimates"]:
            xyz[i] += sinex["estimates"][vtype][0] * dt_yr
    return xyz


# ---------------------------------------------------------------------------
# GSI F5 daily coordinate parser
# ---------------------------------------------------------------------------
def parse_f5(filepath, eval_date_str):
    """Compute 15-day median reference coordinate from a GSI F5 file.

    The reference coordinate is the element-wise median of ECEF (X, Y, Z)
    over the window [eval_date − 7 days, eval_date + 7 days].

    Reference precision is the 68th-percentile of the 3D scatter of daily
    values within the window, relative to their median.

    Args:
        filepath: Path to a GSI F5 daily .pos file.
        eval_date_str: Evaluation date as 'YYYY/MM/DD'.

    Returns:
        dict with keys:
            xyz       – np.array([X, Y, Z]) median in metres (ITRF2014/GRS80)
            lat, lon, h – geodetic coordinates of median
            sigma_3d  – reference precision (68th-pctile of 3D scatter) [m]
            n_days    – number of days actually used
    """
    eval_dt = datetime.strptime(eval_date_str, "%Y/%m/%d")
    lo, hi = eval_dt - timedelta(days=7), eval_dt + timedelta(days=7)

    xs, ys, zs = [], [], []
    in_data = False

    # GSI F5 files carry a Japanese J_NAME field in legacy encodings (EUC-JP /
    # Shift-JIS).  Only the ASCII numeric data lines matter, so decode loosely.
    with open(filepath, encoding="ascii", errors="replace") as fh:
        for raw in fh:
            line = raw.strip()
            if line.startswith("+DATA"):
                in_data = True
                continue
            if line.startswith("-DATA"):
                break
            if not in_data or line.startswith("*") or not line:
                continue
            parts = line.split()
            if len(parts) < 7:
                continue
            try:
                dt = datetime(int(parts[0]), int(parts[1]), int(parts[2]))
            except ValueError:
                continue
            if lo <= dt <= hi:
                xs.append(float(parts[4]))
                ys.append(float(parts[5]))
                zs.append(float(parts[6]))

    if len(xs) < 7:
        raise ValueError(
            f"GSI F5: only {len(xs)} day(s) in ±7-day window around {eval_date_str} — need ≥7"
        )

    xs, ys, zs = np.array(xs), np.array(ys), np.array(zs)
    mx, my, mz = float(np.median(xs)), float(np.median(ys)), float(np.median(zs))
    median_xyz = np.array([mx, my, mz])
    lat, lon, h = xyz2blh(mx, my, mz)

    # 3D scatter of daily values relative to their median → reference precision
    scatter = [
        np.linalg.norm(xyz2enu(np.array([xi - mx, yi - my, zi - mz]), lat, lon))
        for xi, yi, zi in zip(xs, ys, zs)
    ]
    sigma_3d = float(np.percentile(scatter, 68))

    return {
        "xyz": median_xyz,
        "lat": lat,
        "lon": lon,
        "h": h,
        "sigma_3d": sigma_3d,
        "n_days": len(xs),
    }


# ---------------------------------------------------------------------------
# Solution file parsers  (.pos — same logic as compare_pos.py — and NMEA GGA)
# ---------------------------------------------------------------------------
# NMEA GGA quality indicator → RTKLIB solution status Q.  Inverse of
# nmea_solq[] in src/pos/mrtk_sol.c: 1=single, 2=DGPS, 3=PPP, 4=fix, 5=float,
# 6=dead reckoning.  Note that PPP rides on quality 3, not 6.
_GGA_TO_SOLQ = {1: 5, 2: 4, 3: 6, 4: 1, 5: 2, 6: 7}

_UNIX_EPOCH = datetime(1970, 1, 1)


def _pos_seconds(date_field, time_field):
    """Convert a .pos time stamp to seconds on a monotonic scale.

    Handles both output formats: calendar ``YYYY/MM/DD HH:MM:SS.sss`` and GPS
    ``week tow``.  Only differences are used downstream, so the origin of the
    calendar scale (Unix epoch, naive) does not matter.

    Args:
        date_field: First whitespace field (date or GPS week).
        time_field: Second field (time of day or time of week).

    Returns:
        Seconds as float, or None if the stamp is not parsable.
    """
    try:
        if "/" in date_field:
            day = datetime.strptime(date_field, "%Y/%m/%d")
            hh, mm, ss = time_field.split(":")
            return (day - _UNIX_EPOCH).total_seconds() + int(hh) * 3600 + int(mm) * 60 + float(ss)
        return int(date_field) * 604800 + float(time_field)
    except ValueError:
        return None


def _gga_seconds(hhmmss):
    """Convert a GGA HHMMSS.ss field to seconds of day.

    Args:
        hhmmss: GGA field[1].

    Returns:
        Seconds of day as float, or None if the field is malformed.
    """
    try:
        return int(hhmmss[0:2]) * 3600 + int(hhmmss[2:4]) * 60 + float(hhmmss[4:])
    except (ValueError, IndexError):
        return None


def parse_pos(filepath):
    """Parse an RTKLIB .pos file.

    Returns:
        dict mapping time-key string to (lat, lon, h, Q, ns, t), where ns is the
        satellite count of the epoch (0 when the file has no ns column) and t is
        the epoch time in seconds (None if the time stamp is not parsable).
    """
    data = {}
    with open(filepath) as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("%"):
                continue
            parts = line.split()
            if len(parts) < 6:
                continue
            key = parts[0] + " " + parts[1]
            data[key] = (
                float(parts[2]),
                float(parts[3]),
                float(parts[4]),
                int(parts[5]),
                int(parts[6]) if len(parts) > 6 else 0,
                _pos_seconds(parts[0], parts[1]),
            )
    return data


def parse_nmea(filepath):
    """Parse NMEA GGA sentences.

    Recovers ellipsoidal height from the two GGA height fields:
      field[9]  — MSL altitude (orthometric height)
      field[11] — Geoid separation N
      h_ell     = field[9] + field[11]

    Args:
        filepath: Path to NMEA file.

    Returns:
        Tuple (rows, geoid_ok) where:
          rows     — list of (lat_deg, lon_deg, h_ell, quality, nsat, t) tuples
                     in file order.  h_ell is the ellipsoidal height in metres,
                     quality is the raw GGA quality indicator, nsat is field[7]
                     (satellites used) and t is the epoch time in seconds,
                     unwrapped across midnight (GGA carries no date).
          geoid_ok — True if geoid separation was successfully read from
                     at least one sentence; False if field[11] was absent
                     or zero for all sentences (Up comparison unreliable).
    """
    geoid_ok = False
    rows = []
    day_offset, prev_sod = 0.0, None
    with open(filepath) as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue
            if "*" in line:
                line = line[: line.index("*")]
            fields = line.split(",")
            if len(fields) < 10:
                continue
            if fields[0] not in ("$GPGGA", "$GNGGA"):
                continue
            try:
                quality = int(fields[6])
                if quality == 0 or not fields[2] or not fields[4]:
                    continue
                lat = nmea_to_deg(fields[2], fields[3])
                lon = nmea_to_deg(fields[4], fields[5])
                nsat = int(fields[7]) if fields[7] else 0
                alt_msl = float(fields[9])
                # Recover ellipsoidal height: h_ell = MSL + geoid_separation
                geoid_sep = 0.0
                if len(fields) > 11 and fields[11]:
                    try:
                        geoid_sep = float(fields[11])
                    except ValueError:
                        pass
                if geoid_sep != 0.0:
                    geoid_ok = True
                sod = _gga_seconds(fields[1])
                if sod is None:
                    t = None
                else:
                    if prev_sod is not None and sod < prev_sod:
                        day_offset += 86400.0  # GGA has no date: unwrap midnight
                    prev_sod = sod
                    t = sod + day_offset
                rows.append((lat, lon, alt_msl + geoid_sep, quality, nsat, t))
            except (ValueError, IndexError):
                continue
    return rows, geoid_ok


def detect_format(filepath):
    """Sniff whether a solution file is NMEA or an RTKLIB .pos.

    The extension is not trusted — only the content is inspected.

    Args:
        filepath: Path to the solution file.

    Returns:
        ``"nmea"`` if the first payload line is a NMEA sentence, else ``"pos"``.
    """
    with open(filepath, errors="replace") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("%"):
                continue
            return "nmea" if line.startswith("$") else "pos"
    return "pos"


def load_solution(filepath, fmt="auto"):
    """Load a solution file as position rows in epoch order.

    Args:
        filepath: RTKLIB .pos or NMEA GGA file.
        fmt: ``"pos"``, ``"nmea"``, or ``"auto"`` to sniff the content.

    Returns:
        Tuple (rows, fmt, geoid_ok) where:
          rows     — list of (lat_deg, lon_deg, h_ell_m, Q, nsat, t) tuples.  Q
                     is on the RTKLIB .pos scale for both input formats and t is
                     the epoch time in seconds.
          fmt      — format actually used.
          geoid_ok — False only for NMEA input carrying no geoid separation,
                     i.e. the ellipsoidal height (hence Up) is not recoverable.
    """
    if fmt == "auto":
        fmt = detect_format(filepath)

    if fmt == "nmea":
        rows, geoid_ok = parse_nmea(filepath)
        rows = [(lat, lon, h, _GGA_TO_SOLQ.get(q, 0), ns, t) for lat, lon, h, q, ns, t in rows]
        return rows, fmt, geoid_ok

    # .pos epochs are keyed by timestamp, so the dict also de-duplicates them.
    # Order by the parsed epoch time, not the key: the GPS week/TOW output
    # format would sort wrong as a string ("2320 99000" after "2320 100000").
    data = parse_pos(filepath)
    rows = sorted(data.values(), key=lambda r: (r[5] is None, r[5] or 0.0))
    return rows, fmt, True


# ---------------------------------------------------------------------------
# Absolute accuracy metrics
# ---------------------------------------------------------------------------
def compute_abs_metrics(true_xyz, rows, skip_epochs=0):
    """Compute per-epoch 3D errors against a fixed true coordinate.

    Args:
        true_xyz: np.array([X, Y, Z]) — true ECEF coordinate in metres.
        rows: list of (lat, lon, h, Q, ns, t) tuples from load_solution(), in
            epoch order.  Q follows the RTKLIB .pos convention.
        skip_epochs: number of initial epochs to discard.

    Returns:
        dict of statistics, or None if no usable epochs.
    """
    true_lat, true_lon, _ = xyz2blh(*true_xyz)
    rows = rows[skip_epochs:]
    if not rows:
        return None

    errors_3d, enu_errors, q_list, ns_list, t_list = [], [], [], [], []
    for lat, lon, h, q, ns, t in rows:
        dx = blh2xyz(lat, lon, h) - true_xyz
        enu = xyz2enu(dx, true_lat, true_lon)
        enu_errors.append(enu)
        errors_3d.append(float(np.linalg.norm(enu)))
        q_list.append(q)
        ns_list.append(ns)
        t_list.append(t)

    e3 = np.array(errors_3d)
    en = np.array(enu_errors)
    n = len(e3)

    horiz = np.sqrt(en[:, 0] ** 2 + en[:, 1] ** 2)

    # Satellite count over precise-solution epochs only (Q=1 fix, 2 float,
    # 6 PPP); Single/DGPS epochs would dilute the figure the user cares about.
    ns_precise = [ns for ns, q in zip(ns_list, q_list) if q in (1, 2, 6)]

    return {
        "n": n,
        "errors_3d": e3,
        "enu_errors": en,
        "q_list": q_list,
        "ns_list": ns_list,
        "t_list": t_list,
        "true_lat": true_lat,
        "true_lon": true_lon,
        # ENU components
        "rms_e": float(np.sqrt(np.mean(en[:, 0] ** 2))),
        "rms_n": float(np.sqrt(np.mean(en[:, 1] ** 2))),
        "rms_u": float(np.sqrt(np.mean(en[:, 2] ** 2))),
        # 2D horizontal
        "mean_2d": float(np.mean(horiz)),
        "rms_2d": float(np.sqrt(np.mean(horiz**2))),
        "p68_2d": float(np.percentile(horiz, 68)),
        "p95_2d": float(np.percentile(horiz, 95)),
        "max_2d": float(np.max(horiz)),
        # 3D
        "mean_3d": float(np.mean(e3)),
        "rms_3d": float(np.sqrt(np.mean(e3**2))),
        "p68_3d": float(np.percentile(e3, 68)),
        "p95_3d": float(np.percentile(e3, 95)),
        "max_3d": float(np.max(e3)),
        "fix_rate": sum(1 for q in q_list if q in (1, 6)) / n * 100.0,
        # integer-fixed only (Q=1 = narrow-lane/RTK fix); excludes Q=6 PPP float.
        # Used by --min-fix-rate to assert PPP-AR actually resolves ambiguities.
        "fix_rate_int": sum(1 for q in q_list if q == 1) / n * 100.0,
        # Satellite count over Fix/Float/PPP epochs
        "n_precise": len(ns_precise),
        "ns_mean": float(np.mean(ns_precise)) if ns_precise else 0.0,
        "ns_min": int(np.min(ns_precise)) if ns_precise else 0,
        "ns_max": int(np.max(ns_precise)) if ns_precise else 0,
    }


def compute_ttff(true_xyz, rows, max_h=0.30, max_v=0.50, fix_q=1):
    """Time to first fix: first epoch in the given state that is *also* accurate.

    An epoch qualifies when its solution status equals ``fix_q`` and both the
    horizontal error and the absolute Up error are within the given bounds.
    The search always starts at the first epoch of the file — skip_epochs
    exists to drop the convergence transient, which is exactly what TTFF
    measures.

    Args:
        true_xyz: np.array([X, Y, Z]) — true ECEF coordinate in metres.
        rows: list of (lat, lon, h, q, ns, t) tuples in epoch order.
        max_h: horizontal error bound in metres.
        max_v: absolute vertical (Up) error bound in metres.
        fix_q: quality value that means integer fix — 1 on the .pos scale,
            4 for raw GGA quality.

    Returns:
        dict with keys ``ttff`` (seconds from the first epoch, None when the
        epoch times are unusable), ``index`` (0-based epoch index) and
        ``n`` (epochs scanned), or None if no epoch ever qualifies.
    """
    if not rows:
        return None
    true_lat, true_lon, _ = xyz2blh(*true_xyz)

    for i, (lat, lon, h, q, _ns, t) in enumerate(rows):
        if q != fix_q:
            continue
        enu = xyz2enu(blh2xyz(lat, lon, h) - true_xyz, true_lat, true_lon)
        if math.hypot(enu[0], enu[1]) > max_h or abs(enu[2]) > max_v:
            continue
        t0 = rows[0][5]
        return {
            "ttff": (t - t0) if (t is not None and t0 is not None) else None,
            "index": i,
            "n": len(rows),
        }
    return None


def print_ttff(true_xyz, rows, max_h=0.30, max_v=0.50, geoid_ok=True, fix_q=1, ppp_q=6):
    """Print the TTFF line, and the PPP convergence time when PPP epochs exist.

    The PPP line applies the same accuracy bounds to the PPP solution state
    instead of the integer fix.  It is a reference figure — a run that never
    reports a PPP epoch (plain RTK) simply does not get the line.

    Args:
        true_xyz: np.array([X, Y, Z]) — true ECEF coordinate in metres.
        rows: list of (lat, lon, h, q, ns, t) tuples in epoch order.
        max_h: horizontal error bound in metres.
        max_v: absolute vertical (Up) error bound in metres.
        geoid_ok: False when the input is NMEA without geoid separation, in
            which case the vertical criterion cannot be evaluated.
        fix_q: quality value that means integer fix.
        ppp_q: quality value that means PPP.
    """

    def line(label, state, q):
        crit = f"{state} & 2D<={max_h * 100:g}cm & Up<={max_v * 100:g}cm"
        if not geoid_ok:
            print(f"  {label} : n/a  (no geoid separation, Up unevaluable; {crit})")
            return
        r = compute_ttff(true_xyz, rows, max_h, max_v, q)
        if r is None:
            print(f"  {label} : not reached in {len(rows)} epochs  ({crit})")
        elif r["ttff"] is None:
            print(f"  {label} : epoch {r['index'] + 1}/{r['n']}, time unknown  ({crit})")
        else:
            print(f"  {label} : {r['ttff']:.1f} s  (epoch {r['index'] + 1}/{r['n']}; {crit})")

    line("TTFF    ", "fix", fix_q)
    if any(r[3] == ppp_q for r in rows):
        line("PPP conv", "PPP", ppp_q)


# ---------------------------------------------------------------------------
# Plot
# ---------------------------------------------------------------------------
def _time_axis(m):
    """Build the shared x axis of the plots.

    Args:
        m: metrics dict from compute_abs_metrics().

    Returns:
        Tuple (x, label, formatter, ticks, xlim): elapsed seconds from the first
        epoch with an HH:MM tick formatter and ticks on round clock times when
        every epoch carries a time stamp, else the epoch index with none of
        them.  Elapsed seconds keep the axis monotonic across midnight, which a
        seconds-of-day axis would not.
    """
    t_list = m.get("t_list") or []
    if len(t_list) != m["n"] or any(t is None for t in t_list):
        return np.arange(m["n"]), "Epoch", None, None, None

    t0, t1 = t_list[0], t_list[-1]

    def hhmm(x, _pos):
        sod = (t0 + x) % 86400
        return f"{int(sod // 3600):02d}:{int(sod % 3600 // 60):02d}"

    # Tick on round clock times rather than on round elapsed seconds: pick the
    # smallest step that keeps the count under ~10.
    span = max(t1 - t0, 1.0)
    step = next(
        (s for s in (60, 120, 300, 600, 900, 1800, 3600, 7200, 21600, 43200) if span / s <= 9),
        86400,
    )
    # Pad the view so a tick landing on the first or last epoch still has room
    # for its label instead of being clipped against the frame.
    pad = 0.02 * span
    first = math.ceil((t0 - pad) / step) * step
    ticks = np.arange(first, t1 + pad + step, step) - t0
    ticks = ticks[(ticks >= -pad) & (ticks <= span + pad)]

    return np.array(t_list) - t0, "Time [HH:MM]", hhmm, ticks, (-pad, span + pad)


def plot_results(m, ref_label, output_path="abs_compare.png"):
    """Generate ENU error time-series and Q-flag plot."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import FuncFormatter

    en = m["enu_errors"] * 100  # m → cm
    idx, xlabel, formatter, ticks, xlim = _time_axis(m)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    for col, lbl in zip(range(3), ("East", "North", "Up")):
        ax1.plot(idx, en[:, col], label=lbl, alpha=0.8, linewidth=0.8)
    ax1.axhline(0, color="k", linewidth=0.5)
    ax1.set_ylabel("Position error [cm]")
    ax1.set_title(
        f"Absolute error vs {ref_label} — "
        f"RMS {m['rms_3d'] * 100:.2f} cm  |  "
        f"1σ {m['p68_3d'] * 100:.2f} cm  |  "
        f"95% {m['p95_3d'] * 100:.2f} cm"
    )
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Satellite count on the left axis, Q flag on the right.  The count starts
    # at 0 so the bar-like trace is read against an absolute scale, which also
    # keeps it clear of the Q markers anchored at the bottom of the right scale.
    ax2.plot(idx, m["ns_list"], color="C0", linewidth=0.8, alpha=0.9)
    ax2.set_ylim(0, max(m["ns_list"]) + 1)
    ax2.set_ylabel("Satellites", color="C0")
    ax2.tick_params(axis="y", labelcolor="C0")
    ax2.set_xlabel(xlabel)
    ax2.set_title(
        f"Satellites  (mean {m['ns_mean']:.1f} over Fix/Float/PPP)"
        f"   |   Q flag  (fix rate {m['fix_rate']:.1f}%)"
    )
    ax2.grid(True, alpha=0.3)

    ax2q = ax2.twinx()
    ax2q.scatter(idx, m["q_list"], s=8, alpha=0.5, color="C3")
    ax2q.set_ylabel("Q flag", color="C3")
    ax2q.set_yticks([1, 2, 3, 4, 5, 6])
    ax2q.set_yticklabels(["1:Fix", "2:Float", "3:SBAS", "4:DGPS", "5:Single", "6:PPP"])
    ax2q.set_ylim(0.5, 6.5)
    ax2q.tick_params(axis="y", labelcolor="C3")
    if formatter:
        # ax1 shares this axis
        ax2.set_xticks(ticks)
        ax2.set_xlim(*xlim)
        ax2.xaxis.set_major_formatter(FuncFormatter(formatter))

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"  Plot saved: {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def _parse_triplet(text, label):
    """Parse a comma-separated coordinate triplet."""
    try:
        values = [float(v) for v in text.split(",")]
    except ValueError as exc:
        raise ValueError(f"{label} must contain numeric values: {text!r}") from exc
    if len(values) != 3:
        raise ValueError(f"{label} must be three comma-separated values")
    return values


def _criterion(label, value_m, tolerance_m, ref_precision_m):
    """Evaluate and print one pass/fail criterion.

    Passes when  value < tolerance  OR  value < ref_precision.

    Returns True if the criterion passes.
    """
    a = value_m < tolerance_m
    b = value_m < ref_precision_m
    ok = a or b
    tag = "PASS" if ok else "FAIL"
    reasons = []
    if a:
        reasons.append(f"< tol {tolerance_m * 100:.1f} cm")
    if b:
        reasons.append(f"< ref prec {ref_precision_m * 100:.2f} cm")
    if not reasons:
        reasons = [
            f">= tol {tolerance_m * 100:.1f} cm",
            f">= ref prec {ref_precision_m * 100:.2f} cm",
        ]
    print(f"{tag} [{label}]: {value_m * 100:.3f} cm  ({', '.join(reasons)})")
    return ok


def main():  # noqa: D103
    p = argparse.ArgumentParser(
        description="Absolute accuracy check of RTKLIB .pos vs geodetic truth"
    )
    ref = p.add_mutually_exclusive_group(required=True)
    ref.add_argument("--sinex", metavar="FILE", help="IGS SINEX file (.SNX or .SNX.gz)")
    ref.add_argument("--f5", metavar="FILE", help="GSI F5 daily coordinate file")
    ref.add_argument("--llh", metavar="LAT,LON,H", help="Fixed reference lat/lon/h (deg, deg, m)")
    ref.add_argument("--ecef", metavar="X,Y,Z", help="Fixed reference ECEF coordinate (m)")
    p.add_argument("--station", metavar="CODE", help="4-char station code (required with --sinex)")
    p.add_argument(
        "--date",
        metavar="YYYY/MM/DD",
        help="Evaluation date for F5 15-day window (required with --f5)",
    )
    p.add_argument(
        "--epoch",
        metavar="YYYY/MM/DD",
        help="Target epoch for SINEX propagation (default: SINEX ref epoch)",
    )
    p.add_argument(
        "--ref-precision",
        type=float,
        default=0.0,
        help="Reference precision in metres for --llh/--ecef (default 0)",
    )
    p.add_argument(
        "--tolerance",
        type=float,
        default=0.030,
        help="Tolerance for criterion A in metres (default 0.030)",
    )
    p.add_argument("--skip-epochs", type=int, default=0, help="Initial epochs to discard")
    p.add_argument(
        "--min-fix-rate",
        type=float,
        default=None,
        help="Require integer-fix rate (Q=1) >= this percent (PPP-AR test)",
    )
    p.add_argument(
        "--use-2d",
        action="store_true",
        help="Evaluate pass/fail on 2D horizontal error (default: 3D)",
    )
    p.add_argument(
        "--ttff-h",
        type=float,
        default=0.30,
        help="TTFF horizontal error bound in metres (default 0.30)",
    )
    p.add_argument(
        "--ttff-v",
        type=float,
        default=0.50,
        help="TTFF vertical error bound in metres (default 0.50)",
    )
    p.add_argument(
        "--format",
        choices=["auto", "pos", "nmea"],
        default="auto",
        help="Input format of the test file (default: auto-detect from content)",
    )
    p.add_argument("--plot", action="store_true", help="Generate ENU error time-series plot")
    p.add_argument("test", help="RTKLIB .pos or NMEA GGA file to evaluate")
    args = p.parse_args()

    import os

    # ── Resolve true reference coordinate ───────────────────────────────────
    if args.sinex:
        if not args.station:
            print("FAIL: --station is required with --sinex", file=sys.stderr)
            return 1
        if not os.path.isfile(args.sinex):
            print(f"FAIL: SINEX file not found: {args.sinex}", file=sys.stderr)
            return 1

        sinex = parse_sinex(args.sinex, args.station)
        if args.epoch:
            target_dt = datetime.strptime(args.epoch, "%Y/%m/%d")
            true_xyz = sinex_propagate(sinex, target_dt)
            epoch_note = f"propagated to {args.epoch}"
        else:
            true_xyz = sinex["xyz"]
            epoch_note = f"at SINEX ref epoch {sinex['epoch'].strftime('%Y/%m/%d %H:%M')}"

        ref_precision = sinex["sigma_3d"]
        ref_label = f"SINEX/{args.station.upper()}"

        print(f"Reference : {args.sinex}")
        print(f"Station   : {args.station.upper()} ({epoch_note})")
        print(f"Ref prec  : {ref_precision * 1000:.2f} mm (SINEX formal 3D σ)")
    elif args.f5:
        if not args.date:
            print("FAIL: --date is required with --f5", file=sys.stderr)
            return 1
        if not os.path.isfile(args.f5):
            print(f"FAIL: F5 file not found: {args.f5}", file=sys.stderr)
            return 1

        f5 = parse_f5(args.f5, args.date)
        true_xyz = f5["xyz"]
        ref_precision = f5["sigma_3d"]
        ref_label = f"GSI-F5/{args.date}"

        print(f"Reference : {args.f5}")
        print(f"Eval date : {args.date}  (±7-day median, {f5['n_days']} days)")
        print(f"Ref coord : {f5['lat']:.8f}°N  {f5['lon']:.8f}°E  {f5['h']:.4f} m")
        print(f"Ref prec  : {ref_precision * 1000:.2f} mm (68th-pctile of F5 daily scatter)")
    elif args.llh:
        try:
            lat, lon, h = _parse_triplet(args.llh, "--llh")
        except ValueError as exc:
            print(f"FAIL: {exc}", file=sys.stderr)
            return 1
        true_xyz = blh2xyz(lat, lon, h)
        ref_precision = args.ref_precision
        ref_label = "fixed LLH"

        print("Reference : fixed LLH")
        print(f"Ref coord : {lat:.8f}°N  {lon:.8f}°E  {h:.4f} m")
        print(f"Ref prec  : {ref_precision * 1000:.2f} mm")
    else:
        try:
            x, y, z = _parse_triplet(args.ecef, "--ecef")
        except ValueError as exc:
            print(f"FAIL: {exc}", file=sys.stderr)
            return 1
        true_xyz = np.array([x, y, z])
        ref_precision = args.ref_precision
        ref_label = "fixed ECEF"

        lat, lon, h = xyz2blh(x, y, z)
        print("Reference : fixed ECEF")
        print(f"Ref coord : {lat:.8f}°N  {lon:.8f}°E  {h:.4f} m")
        print(f"Ref prec  : {ref_precision * 1000:.2f} mm")

    # ── Parse test solution (.pos or NMEA) ───────────────────────────────────
    if not os.path.isfile(args.test):
        print(f"FAIL: test file not found: {args.test}", file=sys.stderr)
        return 1

    rows, fmt, geoid_ok = load_solution(args.test, args.format)

    metric_label = "2D horizontal" if args.use_2d else "3D"
    fmt_note = f"{fmt.upper()}, auto-detected" if args.format == "auto" else fmt.upper()
    print(f"Test      : {args.test}  ({fmt_note})")
    print(f"Tolerance : {args.tolerance * 100:.1f} cm  (evaluated on {metric_label} error)")
    if args.skip_epochs:
        print(f"Skip      : {args.skip_epochs} initial epochs")
    print()

    if not rows:
        label = "GGA" if fmt == "nmea" else "position"
        print(f"FAIL: no {label} data in test file", file=sys.stderr)
        return 1

    if not geoid_ok:
        # h_ell = MSL + N is unrecoverable, so Up is off by the undulation
        # (~30-40 m in Japan).  Horizontal error is unaffected.
        if not args.use_2d:
            print(
                "FAIL: GGA field[11] (geoid separation) is absent or zero in all epochs,\n"
                "      so the ellipsoidal height cannot be recovered.  Re-run with\n"
                "      --use-2d to evaluate horizontal accuracy only.",
                file=sys.stderr,
            )
            return 1
        print("WARNING: GGA field[11] (geoid separation) absent or zero in all epochs.")
        print("         Up errors are meaningless; 2D horizontal pass/fail is used.")
        print()

    # ── Compute metrics ──────────────────────────────────────────────────────
    m = compute_abs_metrics(true_xyz, rows, args.skip_epochs)
    if m is None:
        print("FAIL: no usable epochs", file=sys.stderr)
        return 1

    tl, tn, th = xyz2blh(*true_xyz)
    print(f"True pos  : {tl:.8f}°N  {tn:.8f}°E  {th:.4f} m")
    print(f"Epochs    : {m['n']}")
    print()
    print("  ENU RMS (absolute):")
    print(f"    East  : {m['rms_e'] * 100:8.3f} cm")
    print(f"    North : {m['rms_n'] * 100:8.3f} cm")
    print(f"    Up    : {m['rms_u'] * 100:8.3f} cm")
    print()
    print("  2D horizontal error distribution:")
    print(f"    Bias  : {m['mean_2d'] * 100:8.3f} cm  (mean)")
    print(f"    RMS   : {m['rms_2d'] * 100:8.3f} cm")
    print(f"    1σ    : {m['p68_2d'] * 100:8.3f} cm  (68th percentile)")
    print(f"    95%   : {m['p95_2d'] * 100:8.3f} cm  (95th percentile)")
    print(f"    Max   : {m['max_2d'] * 100:8.3f} cm")
    print()
    print("  3D error distribution (vs geodetic truth):")
    print(f"    Bias  : {m['mean_3d'] * 100:8.3f} cm  (mean)")
    print(f"    RMS   : {m['rms_3d'] * 100:8.3f} cm")
    print(f"    1σ    : {m['p68_3d'] * 100:8.3f} cm  (68th percentile)")
    print(f"    95%   : {m['p95_3d'] * 100:8.3f} cm  (95th percentile)")
    print(f"    Max   : {m['max_3d'] * 100:8.3f} cm")
    print()
    print(f"  Sol rate : {m['fix_rate']:.2f}%  (Q=1/6: fixed or PPP-float)")
    print(f"  Fix rate : {m['fix_rate_int']:.2f}%  (Q=1: integer-fixed)")
    print(f"  Ref prec : {ref_precision * 100:.3f} cm  ({ref_label})")
    print()
    print(f"  Satellites over Fix/Float/PPP epochs  ({m['n_precise']} epochs):")
    if m["n_precise"]:
        print(f"    Mean  : {m['ns_mean']:8.2f}")
        print(f"    Min   : {m['ns_min']:8d}")
        print(f"    Max   : {m['ns_max']:8d}")
    else:
        print("    (none)")
    print()
    print_ttff(true_xyz, rows, args.ttff_h, args.ttff_v, geoid_ok)
    print()

    if args.plot:
        plot_results(m, ref_label)

    # ── Pass / Fail ──────────────────────────────────────────────────────────
    # Each metric (1σ, 95%) passes when:
    #   A. metric < tolerance        (meets the required accuracy target)  OR
    #   B. metric < ref_precision    (at least as good as the truth itself)
    #
    # ref_precision is always a 3D quantity (SINEX σ3D or F5 3D scatter).
    # When evaluating 2D horizontal metrics, scale it to horizontal precision
    # assuming isotropic errors: σ_2D = σ_3D * sqrt(2/3).
    if args.use_2d:
        ref_prec_2d = ref_precision * math.sqrt(2.0 / 3.0)
        ok_1s = _criterion("1σ  (2D)", m["p68_2d"], args.tolerance, ref_prec_2d)
        ok_95 = _criterion("95% (2D)", m["p95_2d"], args.tolerance, ref_prec_2d)
    else:
        ok_1s = _criterion("1σ  (3D)", m["p68_3d"], args.tolerance, ref_precision)
        ok_95 = _criterion("95% (3D)", m["p95_3d"], args.tolerance, ref_precision)

    passed = ok_1s and ok_95

    if args.min_fix_rate is not None:
        ok_fix = m["fix_rate_int"] >= args.min_fix_rate
        tag = "PASS" if ok_fix else "FAIL"
        rel = ">=" if ok_fix else "<"
        print(f"{tag} [fix-rate]: {m['fix_rate_int']:.2f}% {rel} min {args.min_fix_rate:.2f}%")
        passed = passed and ok_fix

    print()
    print("RESULT: PASS" if passed else "RESULT: FAIL")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
