#!/usr/bin/env python3
"""Extend the cumulative ``igu00p01.erp`` with IGS ultra-rapid ERP files.

The public cumulative file stopped at MJD 59992.25 (2023-02-17 06:00): the copies
on CDDIS and SOPAC are frozen there. The newer ``igu00p01.erp`` shipped with
CLASLIB 0.9.1 is that file extended by hand from the 6-hourly ultra-rapid issues
``IGS0OPSULT_YYYYDDDHHMM_02D_01D_ERP.ERP.gz``. This tool applies the same rule:

* each issue has two rows, at issue + 0.5 d and issue + 1.5 d;
* issues are applied oldest first, and a later issue overwrites an existing MJD,
  so the rows up to the newest issue + 0.5 d are the +0.5 d values and the
  remaining tail rows are +1.5 d predictions until the next issues arrive;
* rows are copied verbatim, and the header of the input file is kept.

The newest applied issue is derived from the input (last MJD - 1.5 d). A run goes
back over the last 14 days as well, so an issue that reached the mirrors late still
overwrites its row, and ``--full`` re-applies every issue since the frozen file ended.
A replay never replaces a row the input already has from a newer issue.

Issues are fetched from BKG over HTTPS. ``--allow-http`` adds SOPAC as a fallback;
it has no HTTPS, so its files are not protected in transit. BKG lacks some issues
(49 of 2023-02-16 .. 2026-10-19, all on SOPAC), so rebuilding from the frozen file
needs ``--allow-http``; extending the CLASLIB file keeps its rows where BKG has a gap.

CLASLIB 0.9.1 has five rows (MJD 60013.50, 61256.25-61257.00) that differ from what
this rule gives with today's mirrors; ``--full`` overwrites them. Background and the
record of those rows: ``docs/design/clas-support-files.md``.

Usage:
    python update_igu_erp.py -i igu00p01.erp -o igu00p01_new.erp
    python update_igu_erp.py -i igu00p01.erp -o new.erp --full --cache-dir ~/.cache/igu
    python update_igu_erp.py -i igu00p01.erp -o new.erp --until 2026-10-19T18:00
"""

import argparse
import concurrent.futures
import datetime as dt
import gzip
import os
import re
import sys
import time
import urllib.error
import urllib.request

SOURCES = ("https://igs.bkg.bund.de/root_ftp/IGS/products/{week}/{name}",)
# SOPAC has no HTTPS: used only with --allow-http.
HTTP_SOURCES = ("http://garner.ucsd.edu/pub/products/{week}/{name}",)
MJD_EPOCH = dt.datetime(1858, 11, 17)
GPS_EPOCH = dt.datetime(1980, 1, 6)
STEP = dt.timedelta(hours=6)
# First issue after the frozen cumulative file ended.
FIRST_ISSUE = dt.datetime(2023, 2, 16)
# Mirrors have taken over a day to publish an issue.
LOOKBACK = dt.timedelta(days=14)
# Mirrors reset connections under load; retry a transient error a few times.
RETRIES = 4
ROW = re.compile(r"^\d{5}\.\d{2}(\s+-?\d+){15}\s*$")


def mjd_of(t):
    """Return the MJD of a datetime."""
    return (t - MJD_EPOCH).total_seconds() / 86400.0


def read_cumulative(path):
    """Return (header lines, {mjd: row line}) of a cumulative ERP file."""
    with open(path, encoding="latin-1") as f:
        lines = f.read().split("\n")
    if lines[-1] == "":
        lines.pop()
    first = next((i for i, line in enumerate(lines) if ROW.match(line)), None)
    if first is None:
        sys.exit(f"error: no ERP rows in {path}")
    rows = {}
    for line in lines[first:]:
        if not ROW.match(line):
            sys.exit(f"error: unexpected line in {path}: {line!r}")
        rows[float(line.split()[0])] = line
    return lines[:first], rows


def issue_name(t):
    """Return the IGS long file name of the ultra-rapid ERP issued at t."""
    return f"IGS0OPSULT_{t:%Y%j%H%M}_02D_01D_ERP.ERP.gz"


def download(t, cache_dir, sources):
    """Return the compressed ERP file issued at t, or None if no mirror has it."""
    name = issue_name(t)
    cached = os.path.join(cache_dir, name) if cache_dir else None
    if cached and os.path.exists(cached):
        with open(cached, "rb") as f:
            return f.read()
    week = (t - GPS_EPOCH).days // 7
    error = None
    for source in sources:
        url = source.format(week=week, name=name)
        data = None
        for attempt in range(RETRIES):
            try:
                with urllib.request.urlopen(url, timeout=60) as resp:
                    data = resp.read()
                break
            except urllib.error.HTTPError as e:
                if e.code == 404:
                    break
                error = e
            except OSError as e:
                error = e
            time.sleep(2**attempt)
        if data is None:
            continue
        if cached:
            tmp = f"{cached}.tmp{os.getpid()}"
            with open(tmp, "wb") as f:
                f.write(data)
            os.replace(tmp, cached)
        return data
    if error is not None:
        raise RuntimeError(f"{name}: {error}")
    return None


def issue_rows(t, data):
    """Return the {mjd: row line} of the ERP file issued at t, checking its epochs."""
    name = issue_name(t)
    rows = {}
    for line in gzip.decompress(data).decode("latin-1").split("\n"):
        if ROW.match(line):
            rows[float(line.split()[0])] = line
    expected = {mjd_of(t) + 0.5, mjd_of(t) + 1.5}
    if set(rows) != expected:
        raise RuntimeError(f"{name}: rows {sorted(rows)}, expected {sorted(expected)}")
    return rows


def merge(rows, fetched, last_issue):
    """Apply the fetched issues {issue mjd: rows} to rows, newest value per MJD.

    A +0.5 d row always wins: it can only come from one issue. A +1.5 d prediction
    is applied only when the issue one day later, which would replace it, was not
    fetched and is newer than the input, so a replay never puts a prediction back
    over a value the input already has from a newer issue.
    """
    last = mjd_of(last_issue)
    for m0, issue in fetched.items():
        for m, line in issue.items():
            if m - m0 > 1.0 and (m - 0.5 in fetched or m - 0.5 <= last):
                continue
            rows[m] = line


def main():
    """Parse the command line and write the extended cumulative ERP."""
    parser = argparse.ArgumentParser(
        description="Extend igu00p01.erp with IGS ultra-rapid ERP issues."
    )
    parser.add_argument("-i", "--input", required=True, help="cumulative ERP file to extend")
    parser.add_argument("-o", "--output", required=True, help="output cumulative ERP file")
    parser.add_argument("--until", help="newest issue to apply, UTC (ISO format, default: now)")
    parser.add_argument("--full", action="store_true", help="re-apply every issue since 2023-02-16")
    parser.add_argument(
        "--allow-http", action="store_true", help="also try SOPAC, which has no HTTPS"
    )
    parser.add_argument("--cache-dir", help="keep the downloaded issues in this directory")
    parser.add_argument("--jobs", type=int, default=4, help="parallel downloads (default: 4)")
    args = parser.parse_args()

    header, rows = read_cumulative(args.input)
    last_issue = MJD_EPOCH + dt.timedelta(days=max(rows) - 1.5)
    until = dt.datetime.fromisoformat(args.until) if args.until else dt.datetime.now(dt.UTC)
    if until.tzinfo is not None:
        until = until.astimezone(dt.UTC).replace(tzinfo=None)

    issues = []
    t = FIRST_ISSUE if args.full else max(FIRST_ISSUE, last_issue - LOOKBACK + STEP)
    while t <= until:
        issues.append(t)
        t += STEP
    if not issues:
        print(f"nothing to apply up to {until}")
        return
    if args.cache_dir:
        os.makedirs(args.cache_dir, exist_ok=True)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        sources = SOURCES + (HTTP_SOURCES if args.allow_http else ())
        files = list(pool.map(lambda t: download(t, args.cache_dir, sources), issues))

    applied = [t for t, data in zip(issues, files) if data is not None]
    if not applied:
        print(f"no issue on the mirrors in {issue_name(issues[0])} .. {issue_name(issues[-1])}")
        return
    fetched = {mjd_of(t): issue_rows(t, data) for t, data in zip(issues, files) if data}
    merge(rows, fetched, last_issue)
    # The newest issues are not always on the mirrors yet: only a hole before the
    # last applied issue is a real gap.
    missing = [t for t, data in zip(issues, files) if data is None and t < applied[-1]]
    for t in missing:
        print(f"warning: missing issue {issue_name(t)}")
    if missing:
        hint = "" if args.allow_http else "; --allow-http also tries SOPAC"
        print(f"warning: {len(missing)} issues missing from the mirrors{hint}")

    mjds = sorted(rows)
    tmp = f"{args.output}.tmp"
    with open(tmp, "w", encoding="latin-1", newline="") as f:
        f.write("\n".join(header + [rows[m] for m in mjds]) + "\n")
    os.replace(tmp, args.output)
    print(f"applied {len(applied)} issues, {issue_name(applied[0])} .. {issue_name(applied[-1])}")
    print(f"rows {len(mjds)}, last MJD {mjds[-1]:.2f}: {args.output}")


if __name__ == "__main__":
    main()
