"""run_benchmark.py - Run MRTKLIB kinematic positioning benchmark on PPC-Dataset / UrbanNav / GICI.

For each configured driving run, calls rnx2rtkp in PPP-RTK (CLAS), PPP
(MADOCA), and/or kinematic RTK mode, then compares the NMEA output against the
PPC-Dataset ground truth reference.csv.  Results are summarised in a
fixed-width ASCII table.

Requirements
------------
- PPC-Dataset must be present at --dataset-dir (manual download required).
  See docs/benchmark.md for download instructions.
- CLAS L6D and MADOCA L6E files are downloaded automatically to --l6-dir
  unless --skip-download is given.
- rnx2rtkp binary is located automatically under build/ or can be specified
  with --rnx2rtkp.
- Python packages: numpy (always); matplotlib (only with --plot).

Usage
-----
    python run_benchmark.py [options]

    --dataset ppc|urbannav|gici Data set (default: ppc); urbannav = UrbanNav Tokyo, RTK only
                                (fetch with download_urbannav.py); gici = GICI Shanghai,
                                RTK only (build with prepare_gici.py)
    --dataset-dir DIR           Benchmark data root (default: data/benchmark)
    --l6-dir DIR                L6 file cache (default: data/benchmark/l6)
    --out-dir DIR               Results directory (default: data/benchmark/results)
    --mode single|clas|madoca|rtk|both|all  (both=clas+madoca, all=clas+madoca+rtk;
                                default: all for ppc, rtk for urbannav / gici)
                                single = SPP baseline (rover.obs + base.nav only)
    --case ID[,ID...]           Run subset (default: every case of the data set)
    --held-out                  Run the held-out cases only (GICI); left out otherwise
    --rnx2rtkp PATH             rnx2rtkp binary path (default: auto-detect)
    --skip-download             Skip L6 download step
    --skip-epochs N             Epochs to skip for metrics (default: 60)
    --plot                      Save per-case ENU PNG to --out-dir
    -v / --verbose              Show rnx2rtkp stdout/stderr

Besides the text report, every run writes two JSON files to --out-dir,
<stem>_manifest.json (code, configuration and input hashes, every case with its
status) and <stem>_results.json (all metrics per case and mode), <stem> being
<dataset>_<mode>, plus "_subset" for a --case run.  Each solution gets a
<solution>.run.json record (solver, configurations and the files they name,
inputs, time span); a cached solution is reused only when that record matches
the current run.
"""

import argparse
import datetime
import hashlib
import json
import math
import os
import subprocess
import sys
import time
import tomllib
from pathlib import Path

from cases import DATASETS, case_by_id, case_files, tow_to_gpst
from compare_ppc import (
    PPC_SCORE_THRESHOLD_3D,
    _match_epochs,
    compute_metrics,
    parse_nmea,
    parse_reference,
    plot_results,
    ppc_score,
    reliability_metrics,
    rinex_obs_timing,
    shift_reference,
)
from download_l6 import ensure_case_l6

# ---------------------------------------------------------------------------
# Auto-detect the positioning binary.
# Since v0.6.0 post-processing lives in the unified `mrtk` binary as
# `mrtk post`; the legacy standalone `rnx2rtkp` is kept as a fallback.
# ---------------------------------------------------------------------------
_CANDIDATE_BINS = [
    "build/mrtk",
    "build/Release/mrtk",
    "build/Debug/mrtk",
    "build/rnx2rtkp",
    "build/Release/rnx2rtkp",
    "build/Debug/rnx2rtkp",
]


def _post_argv(binary: str) -> list[str]:
    """Build the argv prefix for a post-processing run.

    The unified `mrtk` binary needs the `post` subcommand; the legacy
    standalone `rnx2rtkp` is invoked directly.
    """
    return [binary, "post"] if os.path.basename(binary).startswith("mrtk") else [binary]


def _merge_l6_sessions(paths: list[Path], merge_dir: Path) -> list[Path]:
    """Concatenate the hourly archive sessions of each L6 stream into one file.

    A run that spans a UTC hour boundary needs two hourly archive files per
    stream, but `mrtk post` cannot consume them as a sequence: it keeps only
    the *first* L6E file it sees, and it assigns each CLAS `.l6` file to a
    separate transmit-pattern channel — so the second hour is either dropped
    or misread as a second channel, and corrections die at the boundary.
    Concatenating the sessions of one stream into a single file sidesteps both
    limits; the decoders resynchronise on the L6 preamble of every 250-byte
    record, so a joined stream decodes exactly like a continuous one.

    Interim workaround for the plumbing; see issue #316 for the fix in postpos.

    `download_l6.py` probes for the first available L6E PRN per session, so two
    hours of one run can carry different PRNs.  They are still merged into one
    stream — postpos would keep a single file either way, so merging loses
    nothing and recovers the second hour — but the merged name can only carry
    one suffix, so every merge reports its source files by name.

    Args:
        paths: L6 file paths for one mode, in session order.
        merge_dir: Directory for the merged files (created on demand).

    Returns:
        Path list with each multi-session stream replaced by its merged file.
        Single-session streams are passed through untouched.
    """
    # Group the way postpos itself classifies the files: ".200.l6" / ".201.l6"
    # are MADOCA-PPP L6D iono streams keyed per PRN, everything else is the one
    # stream postpos keeps (MADOCA L6E, or the single CLAS channel of this
    # dataset — the PPC rover records one L6 PRN, never two channels).
    streams: dict[str, list[Path]] = {}
    for p in paths:
        suffix = p.name.split(".", 1)[1]  # "2024205B.204.l6" -> "204.l6"
        streams.setdefault(suffix if suffix[:3] in ("200", "201") else "l6e", []).append(p)

    merged = []
    for group in streams.values():
        if len(group) == 1:
            merged.append(group[0])
            continue
        # "2024205B.204.l6" + "2024205C.204.l6" -> "2024205B-C.204.l6"
        session, suffix = group[0].name.split(".", 1)
        out = merge_dir / f"{session}-{group[-1].name.split('.', 1)[0][-1]}.{suffix}"
        if len({p.name.split(".", 1)[1] for p in group}) > 1:
            print(f"  NOTE: sessions carry different PRNs; {out.name} names only the first")
        newest_in = max(p.stat().st_mtime for p in group)
        if not out.exists() or out.stat().st_mtime <= newest_in:
            merge_dir.mkdir(parents=True, exist_ok=True)
            with open(out, "wb") as fo:
                for p in group:
                    fo.write(p.read_bytes())
            print(f"  merged {' + '.join(p.name for p in group)} -> {out.name}")
        merged.append(out)
    return merged


def _find_rnx2rtkp(hint: str = "") -> str:
    """Locate the positioning binary (`mrtk` preferred, `rnx2rtkp` fallback).

    Args:
        hint: Explicit path supplied via --rnx2rtkp.  If non-empty, return
              as-is after verifying the file exists.

    Returns:
        Path to the positioning binary.

    Raises:
        SystemExit: If no binary can be found.
    """
    if hint:
        if not os.path.isfile(hint):
            sys.exit(f"FAIL: binary not found at {hint!r}")
        return hint
    # Search relative to MRTKLIB root (parent of scripts/)
    root = Path(__file__).resolve().parent.parent.parent
    for rel in _CANDIDATE_BINS:
        p = root / rel
        if p.is_file():
            return str(p)
    # Fall back to PATH
    import shutil

    for name in ("mrtk", "rnx2rtkp"):
        p = shutil.which(name)
        if p:
            return p
    sys.exit(
        "FAIL: positioning binary not found. "
        "Build first with 'cmake --build build', or pass --rnx2rtkp PATH."
    )


# ---------------------------------------------------------------------------
# rnx2rtkp invocation
# ---------------------------------------------------------------------------


def _temp_output(out: Path) -> Path:
    """Per-invocation temporary path next to ``out`` (safe for parallel runs)."""
    return out.with_name(f"{out.name}.{os.getpid()}.tmp")


def _publish_output(tmp: Path, out: Path) -> None:
    """Move a solver output and its sibling diagnostics (``<tmp>.stat`` ...) to ``out``."""
    for f in tmp.parent.glob(tmp.name + ".*"):
        f.replace(out.with_name(out.name + f.name[len(tmp.name) :]))
    tmp.replace(out)


def _discard_output(tmp: Path) -> None:
    """Remove a temporary solver output and its sibling diagnostics."""
    for f in tmp.parent.glob(tmp.name + ".*"):
        f.unlink(missing_ok=True)
    tmp.unlink(missing_ok=True)


def _run_rnx2rtkp(
    rnx2rtkp: str,
    conf: str,
    site_confs: list[str],
    week: int,
    tow_start: float,
    tow_end: float,
    output: Path,
    obs: Path,
    nav: Path,
    l6_files: list[Path],
    cwd: str = "",
    verbose: bool = False,
) -> bool:
    """Run rnx2rtkp for one case/mode combination.

    Args:
        rnx2rtkp: Path to rnx2rtkp binary.
        conf: Path to mode configuration file.
        site_confs: Site / receiver override configuration files, applied in
            order after conf.
        week: GPS week of tow_start / tow_end.
        tow_start: Run start TOW with margin already subtracted.
        tow_end: Run end TOW with margin already added.
        output: Destination NMEA file path.
        obs: rover.obs path.
        nav: base.nav path.
        l6_files: List of L6 file paths.
        cwd: Working directory for subprocess (conf relative paths resolve here).
        verbose: Pass stdout/stderr through; otherwise suppress.

    Returns:
        True if rnx2rtkp exited with code 0.
    """
    # rnx2rtkp parses -ts/-te as "y/m/d" "h:m:s" in GPST; a week/TOW pair is
    # silently discarded (epoch2time() rejects month 0 and returns t0).
    ts = tow_to_gpst(week, tow_start)
    te = tow_to_gpst(week, tow_end)

    # Resolve all file paths to absolute so they work regardless of cwd.
    output = output.resolve()
    obs = obs.resolve()
    nav = nav.resolve()
    l6_files = [f.resolve() for f in l6_files]
    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = (
        _post_argv(rnx2rtkp)
        + [
            "-k",
            conf,
        ]
        + [arg for c in site_confs for arg in ("-k", c)]
        + [
            "-ts",
            ts.strftime("%Y/%m/%d"),
            ts.strftime("%H:%M:%S"),
            "-te",
            te.strftime("%Y/%m/%d"),
            te.strftime("%H:%M:%S"),
            "-o",
            str(output),
            str(obs),
            str(nav),
        ]
        + [str(f) for f in l6_files]
    )

    if verbose:
        print("  $", " ".join(cmd))
    result = subprocess.run(
        cmd,
        cwd=cwd or None,
        stdout=None if verbose else subprocess.DEVNULL,
        stderr=None if verbose else subprocess.DEVNULL,
    )
    return result.returncode == 0


# ---------------------------------------------------------------------------
# Summary table
# ---------------------------------------------------------------------------
#  Three rows per case/mode: FIX (Q=4), FF (Q=4|Q=5), ALL (every epoch)
#  Columns: Case  Mode  Tier  N  Rate%  <30cm  RMS_2D  1σ  95%  TTFF_s
_HDR = (
    f"{'Case':<20} {'Mode':<7} {'Tier':<5} {'N':>6} {'nSV':>5} {'Rate%':>7} {'<30cm':>7} "
    f"{'RMS_2D':>10} {'1σ':>10} {'95%':>10} {'TTFF_s':>8}"
)
_SEP = "-" * len(_HDR)
_BLANK_CASE_MODE = " " * (20 + 1 + 7 + 1)  # pad for case+mode when repeated


def _fmt_m(v: float) -> str:
    """Format metres with 3 decimal places, or 'nan'."""
    return "nan" if math.isnan(v) else f"{v:.3f}m"


def _fmt_s(v: float) -> str:
    """Format seconds as integer, or '—'."""
    return "—" if math.isnan(v) else f"{v:.0f}"


def _fmt_sv(v: float) -> str:
    """Format mean satellite count with 1 decimal place, or '—'."""
    return "—" if math.isnan(v) else f"{v:.1f}"


def _fmt_pct(v: float) -> str:
    """Format a percentage, or '—' when the tier has no epochs."""
    return "—" if math.isnan(v) else f"{v:.1f}%"


def _row(
    case_id: str,
    mode: str,
    tier: str,
    n: int,
    n_sv: str,
    rate: str,
    acc: str,
    rms2: str,
    p68: str,
    p95: str,
    ttff: str,
    first: bool = True,
) -> str:
    """Format one tier row."""
    if first:
        prefix = f"{case_id:<20} {mode:<7}"
    else:
        prefix = _BLANK_CASE_MODE
    return (
        f"{prefix} {tier:<5} {n:>6} {n_sv:>5} {rate:>7} {acc:>7} "
        f"{rms2:>10} {p68:>10} {p95:>10} {ttff:>8}"
    )


def print_summary(rows: list[dict]) -> None:
    """Print a fixed-width summary table.

    CLAS and RTK produce three tier rows per case; MADOCA produces one PPP row.

    Tiers (CLAS / RTK):
      FIX  — Q=4 integer-fix epochs only
      FF   — Q=4 or Q=5 (fix + float), excludes SPP blunders
      ALL  — every matched epoch (fix + float + SPP)

    Tier (MADOCA):
      PPP  — all valid PPP-float epochs (Q=3)

    Rate% column:
      FIX row : Q=4 fix rate (clas/rtk)
      FF  row : Q=4|Q=5 rate (clas/rtk)
      PPP row : <30cm threshold rate (madoca)

    <30cm column:
      Fraction of that row's own N epochs within 30 cm 2D — so on the FIX row
      it answers "of the epochs that claimed an integer fix, how many were
      actually right", and its complement is the misfix rate.

    TTFF column:
      FIX row : first ≥30-consecutive-Q=4 run (clas/rtk)
      PPP row : first ≥30-consecutive-<30cm run (madoca)

    Args:
        rows: List of result dicts from the benchmark loop.
    """
    print()
    print(_SEP)
    print(_HDR)
    print(_SEP)
    for i, r in enumerate(rows):
        m = r["metrics"]
        if i > 0 and rows[i - 1]["case_id"] != r["case_id"]:
            print()  # blank line between cases

        if m is None:
            tag = " [FAILED]" if r["status"] == "fail" else " [skipped]"
            print(f"{r['case_id']:<20} {r['mode']:<7} {'—':<5} {'—':>6}{tag}")
            continue

        use_threshold = not math.isnan(m.get("threshold_2d", float("nan")))

        if use_threshold:
            # Threshold modes: single row; no integer fix.
            #   MADOCA → PPP float; SINGLE → SPP (Q=1). Metrics span every epoch.
            tier_label = "SPP" if r["mode"] == "single" else "PPP"
            ppp_rate = f"{m['thr_rate']:.1f}%"
            ppp_ttff = _fmt_s(m["conv_thr_s"])
            print(
                _row(
                    r["case_id"],
                    r["mode"],
                    tier_label,
                    m["n_matched"],
                    _fmt_sv(m["mean_sv_all"]),
                    ppp_rate,
                    _fmt_pct(m["acc_rate_all"]),
                    _fmt_m(m["rms_2d_all"]),
                    _fmt_m(m["p68_2d_all"]),
                    _fmt_m(m["p95_2d_all"]),
                    ppp_ttff,
                    first=True,
                )
            )
        else:
            # AR modes (CLAS / RTK): three rows
            # FIX row (Q=4)
            print(
                _row(
                    r["case_id"],
                    r["mode"],
                    "FIX",
                    m["n_fix"],
                    _fmt_sv(m["mean_sv_fix"]),
                    f"{m['fix_rate']:.1f}%",
                    _fmt_pct(m["acc_rate_fix"]),
                    _fmt_m(m["rms_2d_fix"]),
                    _fmt_m(m["p68_2d_fix"]),
                    _fmt_m(m["p95_2d_fix"]),
                    _fmt_s(m["conv_time_s"]),
                    first=True,
                )
            )
            # FF row (Q=3,4,5 — fix + float, excludes SPP)
            print(
                _row(
                    r["case_id"],
                    r["mode"],
                    "FF",
                    m["n_ff"],
                    _fmt_sv(m["mean_sv_ff"]),
                    f"{m['ff_rate']:.1f}%",
                    _fmt_pct(m["acc_rate_ff"]),
                    _fmt_m(m["rms_2d_ff"]),
                    _fmt_m(m["p68_2d_ff"]),
                    _fmt_m(m["p95_2d_ff"]),
                    "—",
                    first=False,
                )
            )
            # ALL row (every epoch including SPP)
            print(
                _row(
                    r["case_id"],
                    r["mode"],
                    "ALL",
                    m["n_matched"],
                    _fmt_sv(m["mean_sv_all"]),
                    "—",
                    _fmt_pct(m["acc_rate_all"]),
                    _fmt_m(m["rms_2d_all"]),
                    _fmt_m(m["p68_2d_all"]),
                    _fmt_m(m["p95_2d_all"]),
                    "—",
                    first=False,
                )
            )

    print(_SEP)
    print("  CLAS/RTK tiers: FIX=Q=4 | FF=Q=4+5 (excl SPP) | ALL=every epoch")
    print("  MADOCA tier:    PPP=all valid PPP-float epochs (Q=3); Rate%=<30cm fraction")
    print("  SINGLE tier:    SPP=all valid SPP epochs (Q=1); Rate%=<2m fraction")
    print("  <30cm column:   fraction of THAT row's N epochs within 30 cm 2D.")
    print("                  On the FIX row, 100% minus it is the misfix rate.")


def print_ppc_scores(rows: list[dict], dataset: str = "ppc") -> None:
    """Print the PPC-metric score and the reliability metrics per case and mode.

    The score is the distance-weighted share of the reference trajectory with
    3D error <= 0.5 m (``compare_ppc.ppc_score()``).  Two per-mode overalls
    are printed: ``sum`` divides summed distances, treating the runs as one
    long drive (RTKLIB-EX ``score_ppc_sol.py``, used by published comparison
    tables); ``mean`` averages the per-run scores (PPC2024 organizers' rule).
    ``--skip-epochs`` does not apply.

    For UrbanNav and GICI, which have no official scoring rule, the score is
    the same metric at the rover observation grid.  UrbanNav is scored on the
    offset-corrected reference (``cases.URBANNAV_OFFSET_ENU``); two reference
    columns show it without the offset correction (``no-offset``) and on every
    10 Hz reference epoch (``all-ref``, which costs the 5 Hz F9P every other
    epoch).  The GICI reference is already at the antenna (prepare_gici.py).

    A run that completed without a usable solution still counts, with no good
    distance.  A run that did not complete (solver failure, no output, or
    skipped for missing inputs) has no score, and the overall of its mode is
    reported as incomplete rather than computed over the remaining runs.

    Args:
        rows: List of result dicts from the benchmark loop.
        dataset: ``"ppc"``, ``"urbannav"`` or ``"gici"`` (selects the title and
            columns).
    """
    if not rows:
        return
    thr = f"{PPC_SCORE_THRESHOLD_3D * 100:.0f} cm"
    keys = [("ppc", "score")]
    if dataset == "urbannav":
        keys += [("ppc_raw", "no-offset"), ("ppc_allref", "all-ref")]
    print()
    if dataset == "urbannav":
        print(
            f"UrbanNav score (PPC metric, 3D <= {thr}; offset-corrected reference;"
            " F9P 5 Hz / NetR9 10 Hz grid)"
        )
    elif dataset == "gici":
        print(f"GICI score (PPC metric, 3D <= {thr}; antenna reference; 10 Hz grid)")
    else:
        print(f"PPC official score (distance-weighted, 3D <= {thr}, all epochs)")
    head = "".join(f" {label:>9}" for _, label in keys)
    print(f"  {'case':<20} {'mode':<7}{head} {'missing':>8} {'misfix3d':>9} {'longest':>8}")

    def pct(v: float) -> str:
        return "nan" if math.isnan(v) else f"{v:.1f}%"

    totals: dict[str, dict] = {}
    for r in rows:
        acc = totals.setdefault(r["mode"], {"bad": 0, **{k: [0.0, 0.0, []] for k, _ in keys}})
        ppc = r.get("ppc")
        if ppc is None or ppc[1] <= 0:
            acc["bad"] += 1
            tag = "skipped" if r["status"] == "skip" else "FAILED"
            print(f"  {r['case_id']:<20} {r['mode']:<7} {tag:>9}")
            continue
        cols = ""
        for k, _ in keys:
            good, total = r[k]
            acc[k][0] += good
            acc[k][1] += total
            acc[k][2].append(100.0 * good / total)
            cols += f" {100.0 * good / total:8.1f}%"
        rel = r["rel"]
        longest = rel["misfix3d_longest_s"]
        print(
            f"  {r['case_id']:<20} {r['mode']:<7}{cols} "
            f"{pct(rel['missing_rate']):>8} {pct(rel['misfix3d_rate']):>9} "
            f"{'nan' if math.isnan(longest) else f'{longest:.1f}s':>8}"
        )
    for mode, acc in totals.items():
        n_ok = len(acc["ppc"][2])
        if acc["bad"]:
            label = f"overall ({n_ok + acc['bad']} runs)"
            print(f"  {label:<20} {mode:<7} incomplete ({acc['bad']} run(s) not scored)")
            continue
        s_cols = "".join(f" {100.0 * acc[k][0] / acc[k][1]:8.1f}%" for k, _ in keys)
        m_cols = "".join(f" {sum(acc[k][2]) / n_ok:8.1f}%" for k, _ in keys)
        print(f"  {'overall, sum':<20} {mode:<7}{s_cols}  (summed distance)")
        print(f"  {'overall, mean':<20} {mode:<7}{m_cols}  (mean of {n_ok} runs)")
    print(
        f"  missing  = expected-solution epochs without a solution;"
        f" misfix3d = fixed (Q=4) epochs with 3D error > {thr};"
    )
    print("  longest  = longest consecutive misfix3d span.")


def print_misfix_events(rows: list[dict]) -> None:
    """Print the correct-fix rate and the misfix-event metrics per case and mode.

    All columns come from ``compare_ppc.reliability_metrics()`` on the
    expected-solution grid.  An epoch rate counts one long misfix as many
    failures; an event counts it once, from its first misfix to the next
    correct fix, with its span and recovery time.  Only the integer-fixing
    modes (RTK, CLAS) are shown.  The overall row pools the scored runs,
    weighting epochs by the grid interval; as in the score table, it is
    reported as incomplete if a run of the mode was not scored.

    Args:
        rows: List of result dicts from the benchmark loop.
    """
    rows = [r for r in rows if r["mode"] in ("rtk", "clas")]
    if not rows:
        return

    def pct(v: float) -> str:
        return "nan" if math.isnan(v) else f"{v:.1f}%"

    def sec(v: float) -> str:
        return "-" if math.isnan(v) else f"{v:.1f}s"

    thr = f"{PPC_SCORE_THRESHOLD_3D * 100:.0f} cm"
    print()
    print(f"Fix reliability (expected-solution grid, 3D <= {thr})")
    print(
        f"  {'case':<20} {'mode':<7} {'cfix':>7} {'mfixdst':>8} {'events':>6} {'ev/h':>6}"
        f" {'span50':>7} {'span95':>7} {'spanmax':>8} {'rec50':>7} {'recmax':>7}"
        f" {'cens':>4} {'unrec':>5}"
    )
    totals: dict[str, dict] = {}
    for r in rows:
        acc = totals.setdefault(
            r["mode"],
            {"bad": 0, "ok_s": 0.0, "exp_s": 0.0, "ev": 0, "h": 0.0, "cens": 0, "unrec": 0},
        )
        if r.get("ppc") is None or r["ppc"][1] <= 0:
            acc["bad"] += 1
            tag = "skipped" if r["status"] == "skip" else "FAILED"
            print(f"  {r['case_id']:<20} {r['mode']:<7} {tag:>7}")
            continue
        rel = r["rel"]
        dt = rel["dt_grid_s"]
        acc["ok_s"] += rel["n_fix3d_correct"] * dt
        acc["exp_s"] += rel["n_expected"] * dt
        acc["ev"] += rel["misfix_events"]
        acc["h"] += rel["evaluated_h"]
        acc["cens"] += rel["n_censored"]
        acc["unrec"] += rel["n_unrecovered"]
        evh = rel["misfix_events_per_h"]
        print(
            f"  {r['case_id']:<20} {r['mode']:<7} {pct(rel['correct_fix_rate']):>7}"
            f" {pct(rel['misfix3d_dist_rate']):>8} {rel['misfix_events']:>6}"
            f" {'nan' if math.isnan(evh) else f'{evh:.1f}':>6}"
            f" {sec(rel['misfix_span_p50_s']):>7} {sec(rel['misfix_span_p95_s']):>7}"
            f" {sec(rel['misfix_span_max_s']):>8} {sec(rel['recovery_p50_s']):>7}"
            f" {sec(rel['recovery_max_s']):>7} {rel['n_censored']:>4} {rel['n_unrecovered']:>5}"
        )
    for mode, acc in totals.items():
        if acc["bad"]:
            print(f"  {'overall':<20} {mode:<7} incomplete ({acc['bad']} run(s) not scored)")
            continue
        cfix = 100.0 * acc["ok_s"] / acc["exp_s"] if acc["exp_s"] > 0 else math.nan
        evh = acc["ev"] / acc["h"] if acc["h"] > 0 else math.nan
        print(
            f"  {'overall (pooled)':<20} {mode:<7} {pct(cfix):>7} {'':>8} {acc['ev']:>6}"
            f" {'nan' if math.isnan(evh) else f'{evh:.1f}':>6} {'':>7} {'':>7} {'':>8}"
            f" {'':>7} {'':>7} {acc['cens']:>4} {acc['unrec']:>5}"
        )
    print(
        "  cfix    = correctly fixed epochs / all expected epochs;"
        " mfixdst = share of the trajectory length on misfixed epochs;"
    )
    print(
        "  events  = misfix events, each from a misfix to the next correct fix;"
        " span = first to last misfix of an event;"
    )
    print(
        "  rec     = last misfix to the next correct fix; cens = ended by a reference gap;"
        " unrec = still open at the end of the run."
    )


def _sha256(path: Path, cache: dict) -> str | None:
    """SHA-256 of a file (cached per path), or None if it does not exist."""
    key = str(path)
    if key not in cache:
        if not path.is_file():
            cache[key] = None
        else:
            h = hashlib.sha256()
            with open(path, "rb") as f:
                for chunk in iter(lambda: f.read(1 << 20), b""):
                    h.update(chunk)
            cache[key] = h.hexdigest()
    return cache[key]


def _fileinfo(path, cache: dict) -> dict:
    """``{"path", "sha256"}`` of a file (sha256 None if it does not exist)."""
    return {"path": str(path), "sha256": _sha256(Path(path), cache)}


def _git(root: Path, *argv: str) -> str | None:
    """Output of a git command in ``root``, or None when git fails."""
    try:
        res = subprocess.run(
            ["git", *argv], cwd=root, capture_output=True, text=True, check=True, timeout=30
        )
    except (OSError, subprocess.SubprocessError):
        return None
    # Keep leading spaces: they are part of `git status --porcelain` lines.
    return res.stdout.rstrip()


def _git_state(root: Path) -> dict:
    """MRTKLIB commit, ``git describe`` and the modified tracked files, at the start of a run."""
    status = _git(root, "status", "--porcelain", "--untracked-files=no")
    modified = None if status is None else [line[3:] for line in status.splitlines()]
    return {
        "commit": _git(root, "rev-parse", "HEAD"),
        "describe": _git(root, "describe", "--always", "--dirty", "--tags"),
        "dirty": None if modified is None else bool(modified),
        "modified": modified,
    }


def _binary_info(binary: str) -> dict:
    """Path, ``--version`` (None unless it exits 0) and SHA-256 of the solver binary."""
    version = None
    try:
        res = subprocess.run(
            [binary, "--version"], capture_output=True, text=True, timeout=30, check=False
        )
        lines = res.stdout.strip().splitlines()
        if res.returncode == 0 and lines:
            version = lines[0]
    except (OSError, subprocess.SubprocessError):
        pass
    return {
        "path": str(Path(binary).resolve()),
        "version": version,
        "sha256": _sha256(Path(binary), {}),
    }


def _referenced_files(conf: str, root: Path, cache: dict) -> list[dict]:
    """Files named inside a TOML configuration, with their SHA-256.

    Every string value that names an existing file (relative paths resolve
    against the repository root, the solver's working directory) is listed, so
    a changed antenna, grid or table file shows up although the TOML did not
    change.  A configuration that is not TOML gives an empty list.
    """
    try:
        with open(conf, "rb") as f:
            data = tomllib.load(f)
    except (OSError, tomllib.TOMLDecodeError):
        return []
    found = []

    def walk(node, key):
        if isinstance(node, dict):
            for k, v in node.items():
                walk(v, f"{key}.{k}" if key else k)
        elif isinstance(node, list):
            for v in node:
                walk(v, key)
        elif isinstance(node, str) and node and len(node) < 4096:
            path = Path(node) if Path(node).is_absolute() else root / node
            if path.is_file():
                found.append({"key": key, **_fileinfo(path, cache)})

    walk(data, "")
    return found


def _solver_record(binary_info: dict, ctx: dict, span: tuple, root: Path, cache: dict) -> dict:
    """What produces a solution: solver, configurations with their files, inputs, time span.

    Written next to each solution at solve time (``<solution>.run.json``) and
    compared on the next run: a cached solution is reused only when the two
    records are equal.
    """
    inputs = {}
    for k, v in ctx["inputs"].items():
        inputs[k] = [_fileinfo(f, cache) for f in v] if isinstance(v, list) else _fileinfo(v, cache)
    return {
        "binary": {"version": binary_info["version"], "sha256": binary_info["sha256"]},
        "configs": [
            {**_fileinfo(c, cache), "referenced": _referenced_files(c, root, cache)}
            for c in ctx["configs"]
        ],
        "inputs": inputs,
        "span": {"gps_week": span[0], "tow_start": span[1], "tow_end": span[2]},
    }


def _sidecar_path(out: Path) -> Path:
    """Path of the solver record written next to a solution file."""
    return out.with_name(out.name + ".run.json")


def _read_json(path: Path):
    """Parsed JSON file, or None if it is missing or unreadable."""
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError):
        return None


def _jsonable(v):
    """Convert a value to strict JSON (NaN / inf -> None, numpy scalars -> Python, Path -> str).

    Raises:
        TypeError: For any other type, so that nothing is dropped silently.
    """
    if isinstance(v, dict):
        return {k: _jsonable(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [_jsonable(x) for x in v]
    if isinstance(v, Path):
        return str(v)
    if hasattr(v, "item") and getattr(v, "ndim", None) == 0:
        v = v.item()
    if isinstance(v, float) and not math.isfinite(v):
        return None
    if isinstance(v, (str, int, float, bool)) or v is None:
        return v
    raise TypeError(f"not JSON-serialisable: {type(v).__name__}")


def records_stem(args: argparse.Namespace) -> str:
    """File-name stem of the run records: data set, mode, ``heldout``, ``subset`` for ``--case``.

    A run of a different mode, or of a few cases, does not overwrite the
    records of a full run.
    """
    stem = f"{args.dataset}_{args.mode}"
    if getattr(args, "held_out", False):
        stem += "_heldout"
    return stem + ("_subset" if args.case else "")


def write_run_records(
    out_dir: Path,
    args: argparse.Namespace,
    git_state: dict,
    binary_info: dict,
    rows: list[dict],
    started: str,
    complete: bool,
    cache: dict,
) -> None:
    """Write ``<stem>_manifest.json`` and ``<stem>_results.json`` to ``out_dir``.

    The manifest records what produced the numbers: the MRTKLIB state at the
    start of the run (commit, ``git describe``, modified tracked files), the
    solver binary of this run, the SHA-256 of the benchmark scripts, the
    command-line options, whether the run completed, and per case and mode the
    status (``ok`` / ``fail`` / ``skip``, with the reason and missing inputs of
    a skip), whether the solution came from the cache, the solver record of
    the solution (binary, configurations and the files they name, inputs, time
    span: for a cached solution, the record written when it was solved), the
    reference with its offset, and the solution file.  A case that failed or
    was skipped is listed, never dropped.  The results file holds every scalar
    metric per case and mode.  ``<stem>`` is ``records_stem(args)``.

    Args:
        out_dir: Results directory.
        args: Parsed CLI arguments.
        git_state: From ``_git_state()`` at the start of the run.
        binary_info: From ``_binary_info()``.
        rows: List of result dicts from the benchmark loop.
        started: Start time of the run (UTC, ISO 8601).
        complete: False when the run stopped before the last case.
        cache: SHA-256 cache shared with the run.
    """
    here = Path(__file__).resolve().parent
    scripts = [here / "run_benchmark.py", here / "compare_ppc.py", here / "cases.py"]
    scripts.append(here.parent / "tests" / "_geo.py")

    cases = []
    results = []
    for r in rows:
        ctx = r.get("ctx", {})
        cases.append(
            {
                "case_id": r["case_id"],
                "mode": r["mode"],
                "status": r["status"],
                "skip_reason": ctx.get("skip_reason"),
                "missing_inputs": ctx.get("missing_inputs", []),
                "cached": ctx.get("cached", False),
                "solver_record": ctx.get("solver_record"),
                "reference": _fileinfo(ctx["ref"], cache) if ctx.get("ref") else None,
                "reference_offset_enu": ctx.get("ref_offset_enu"),
                "solution": _fileinfo(r["solution"], cache) if r.get("solution") else None,
            }
        )
        res = {"case_id": r["case_id"], "mode": r["mode"], "status": r["status"]}
        if r.get("grid") is not None:
            res["grid"] = {"interval_s": r["grid"][0], "phase_s": r["grid"][1]}
        for key in ("ppc", "ppc_raw", "ppc_allref"):
            if r.get(key) is not None:
                good, total = r[key]
                res[key] = {
                    "good_m": good,
                    "total_m": total,
                    "score_pct": 100.0 * good / total if total > 0 else None,
                }
        if r.get("rel") is not None:
            res["reliability"] = r["rel"]
        m = r.get("metrics")
        if m is not None:
            res["metrics"] = {
                k: v
                for k, v in m.items()
                if not isinstance(v, (list, tuple)) and getattr(v, "ndim", 0) == 0
            }
        results.append(res)

    manifest = {
        "schema": 1,
        "dataset": args.dataset,
        "complete": complete,
        "started_utc": started,
        "finished_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "mrtklib": git_state,
        "binary": binary_info,
        "scripts": [_fileinfo(p, cache) for p in scripts],
        "options": {
            "mode": args.mode,
            "case": args.case,
            "held_out": args.held_out,
            "force": args.force,
            "skip_epochs": args.skip_epochs,
            "dataset_dir": str(args.dataset_dir),
            "threshold_3d_m": PPC_SCORE_THRESHOLD_3D,
        },
        "cases": cases,
    }
    out_dir.mkdir(parents=True, exist_ok=True)
    stem = records_stem(args)
    for name, data in (("manifest", manifest), ("results", {"schema": 1, "results": results})):
        path = out_dir / f"{stem}_{name}.json"
        path.write_text(json.dumps(_jsonable(data), indent=1, allow_nan=False) + "\n")
        print(f"Wrote {path}")


# ---------------------------------------------------------------------------
# Main benchmark loop
# ---------------------------------------------------------------------------
def run_benchmark(args: argparse.Namespace) -> int:
    """Execute the full benchmark loop.

    Args:
        args: Parsed CLI arguments.

    Returns:
        Exit code.
    """
    # Resolve paths (relative paths are resolved against the repo root so the
    # script works correctly regardless of the current working directory)
    root = Path(__file__).resolve().parent.parent.parent
    dataset_dir = Path(args.dataset_dir)
    if not dataset_dir.is_absolute():
        dataset_dir = root / dataset_dir
    l6_dir = Path(args.l6_dir)
    if not l6_dir.is_absolute():
        l6_dir = root / l6_dir
    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute():
        out_dir = root / out_dir
    conf_dir = root / "conf" / "benchmark"
    started = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")

    rnx2rtkp = _find_rnx2rtkp(args.rnx2rtkp)
    print(f"rnx2rtkp  : {rnx2rtkp}")
    print(f"Dataset   : {dataset_dir}")
    print(f"L6 cache  : {l6_dir}")
    print(f"Results   : {out_dir}")
    # UrbanNav Tokyo (2018) predates the L6 archives, and GICI has no L6
    # recording: RTK / SPP only.
    if args.mode is None:
        args.mode = "all" if args.dataset == "ppc" else "rtk"
    if args.dataset != "ppc" and args.mode not in ("rtk", "single"):
        sys.exit(f"FAIL: --dataset {args.dataset} supports --mode rtk or single only")
    print(f"Data set  : {args.dataset}")
    print(f"Mode      : {args.mode}")
    print()

    # Filter cases.  Held-out cases run only with --held-out, and then alone.
    cases = [c for c in DATASETS[args.dataset] if c.get("held_out", False) == args.held_out]
    if args.held_out and not cases:
        sys.exit(f"FAIL: --dataset {args.dataset} has no held-out case")
    if args.case:
        ids = list(dict.fromkeys(c.strip() for c in args.case.split(",") if c.strip()))
        try:
            cases = [case_by_id(cid, DATASETS[args.dataset]) for cid in ids]
        except KeyError as exc:
            sys.exit(f"FAIL: {exc}")
        wrong = [c["id"] for c in cases if c.get("held_out", False) != args.held_out]
        if wrong:
            sys.exit(
                f"FAIL: {', '.join(wrong)}: "
                + ("not a held-out case" if args.held_out else "held-out case; pass --held-out")
            )

    # Validate dataset presence (rover, nav and reference files)
    hint = {
        "urbannav": "  Fetch it with scripts/benchmark/download_urbannav.py",
        "gici": "  Download the sequence zips and run scripts/benchmark/prepare_gici.py"
        " — see docs/reference/benchmark.md.",
    }.get(
        args.dataset, "  Download it manually — see docs/reference/benchmark.md for instructions."
    )
    for case in cases:
        files = case_files(dataset_dir, case)
        for key in ("rover", "nav", "ref"):
            if not files[key].exists():
                sys.exit(
                    f"FAIL: {args.dataset} file not found: {files[key]}\n"
                    f"  Dataset root: {dataset_dir}\n{hint}"
                )

    # Expand mode aliases
    if args.mode == "all":
        modes = ["clas", "madoca", "rtk"]
    elif args.mode == "both":
        modes = ["clas", "madoca"]
    else:
        modes = [args.mode]

    # PPP modes use a 2D error threshold instead of Q=4 for fix/convergence
    _PPP_THRESHOLD = 0.30  # metres
    # SPP has no integer fix; report the fraction of epochs within this 2D bound
    _SPP_THRESHOLD = 2.0  # metres

    results = []
    git_state = _git_state(root)
    binary_info = _binary_info(rnx2rtkp)
    hashes: dict = {}
    complete = False
    try:
        for case in cases:
            files = case_files(dataset_dir, case)
            obs, nav, base_obs, ref = files["rover"], files["nav"], files["base"], files["ref"]

            # Download L6 files (only needed for clas/madoca)
            l6_modes = [m for m in modes if m in ("clas", "madoca")]
            l6_paths: dict[str, list[Path]] = {"clas": [], "madoca": []}
            if l6_modes:
                l6_mode_arg = "both" if set(l6_modes) == {"clas", "madoca"} else l6_modes[0]
                if not args.skip_download:
                    print(f"Downloading L6 for {case['id']} ...")
                    l6_paths = ensure_case_l6(case, l6_dir, l6_mode_arg)
                else:
                    # Build expected paths without downloading
                    from cases import l6_sessions
                    from download_l6 import MADOCA_L6D_PRNS, MADOCA_PRNS

                    sessions = l6_sessions(case["gps_week"], case["tow_start"], case["tow_end"])
                    for year, doy, session in sessions:
                        l6d = l6_dir / f"{year}{doy:03d}{session}.l6"
                        if l6d.exists():
                            l6_paths["clas"].append(l6d)
                        for prn in MADOCA_PRNS:
                            l6e = l6_dir / f"{year}{doy:03d}{session}.{prn}.l6"
                            if l6e.exists():
                                l6_paths["madoca"].append(l6e)
                                break
                        # MADOCA-PPP L6D iono: every available PRN, as ensure_case_l6 does
                        for prn in MADOCA_L6D_PRNS:
                            mdc_l6d = l6_dir / f"{year}{doy:03d}{session}.{prn}.l6"
                            if mdc_l6d.exists():
                                l6_paths["madoca"].append(mdc_l6d)

                # Join multi-session streams so corrections survive the hour boundary
                for key in list(l6_paths):
                    l6_paths[key] = _merge_l6_sessions(l6_paths[key], l6_dir / "merged")

            for mode in modes:
                conf = str(conf_dir / f"{mode}.toml")
                out = out_dir / f"{case['id']}_{mode}.nmea"

                # Determine extra input files and convergence threshold
                if mode == "rtk":
                    extra_files = [base_obs]
                    threshold = math.nan  # RTK uses Q=4
                elif mode == "madoca":
                    extra_files = l6_paths.get("madoca", [])
                    threshold = _PPP_THRESHOLD  # PPP: use <30cm threshold
                elif mode == "single":
                    extra_files = []  # SPP: rover.obs + base.nav (broadcast eph) only
                    threshold = _SPP_THRESHOLD
                else:  # clas
                    extra_files = l6_paths.get("clas", [])
                    threshold = math.nan  # PPP-RTK uses Q=4

                print(f"\n[{case['id']}  /  {mode.upper()}]")

                site_confs = [
                    str(conf_dir / f"{c}.toml") for c in case.get("confs", [case["city"]])
                ]
                # Run context for the manifest, attached to every result of this case/mode.
                ctx = {
                    "configs": [conf, *site_confs],
                    "inputs": {"rover": obs, "nav": nav},
                    "ref": ref,
                    "ref_offset_enu": case.get("ref_offset_enu"),
                    "cached": False,
                }
                if mode == "rtk":
                    ctx["inputs"]["base"] = base_obs
                elif mode in ("clas", "madoca"):
                    ctx["inputs"]["l6"] = list(extra_files)

                missing = [str(f) for f in extra_files if not f.exists()]
                if (not extra_files and mode != "single") or missing:
                    print(f"  WARNING: input files missing for {mode}; skipping.")
                    ctx["skip_reason"] = "no L6 files" if not extra_files else "input files missing"
                    ctx["missing_inputs"] = missing
                    results.append(
                        {
                            "case_id": case["id"],
                            "mode": mode,
                            "metrics": None,
                            "status": "skip",
                            "ctx": ctx,
                        }
                    )
                    continue

                # What produces the solution: solver, configurations (and the files
                # they name), inputs and time span.  A cached solution is reused only
                # when the record written next to it at solve time matches.
                span = (case["gps_week"], case["tow_start"] - 60, case["tow_end"] + 60)
                solver_rec = _solver_record(binary_info, ctx, span, root, hashes)
                sidecar = _sidecar_path(out)
                skip_run = False
                if out.exists() and not args.force:
                    if _read_json(sidecar) == solver_rec:
                        print(f"  [cached]  {out.name}")
                        skip_run = True
                        ctx["cached"] = True
                    else:
                        print(f"  [stale]   {out.name}: solver, configuration or inputs changed")
                ctx["solver_record"] = solver_rec

                if not skip_run:
                    # Write to a temporary path and publish only what this run
                    # produced: a solver that exits 0 without writing must neither
                    # be scored from an old output nor erase it.
                    tmp = _temp_output(out)
                    t0 = time.monotonic()
                    ok = _run_rnx2rtkp(
                        rnx2rtkp,
                        conf,
                        site_confs,
                        case["gps_week"],
                        case["tow_start"] - 60,
                        case["tow_end"] + 60,
                        tmp,
                        obs,
                        nav,
                        extra_files,
                        cwd=str(root),
                        verbose=args.verbose,
                    )
                    elapsed = time.monotonic() - t0
                    if not ok:
                        _discard_output(tmp)
                        print(f"  FAIL: rnx2rtkp returned non-zero (elapsed {elapsed:.1f}s)")
                        results.append(
                            {
                                "case_id": case["id"],
                                "mode": mode,
                                "metrics": None,
                                "status": "fail",
                                "ctx": ctx,
                            }
                        )
                        continue
                    if not tmp.exists():
                        _discard_output(tmp)
                        print("  FAIL: output file not produced")
                        results.append(
                            {
                                "case_id": case["id"],
                                "mode": mode,
                                "metrics": None,
                                "status": "fail",
                                "ctx": ctx,
                            }
                        )
                        continue
                    _publish_output(tmp, out)
                    sidecar.write_text(json.dumps(solver_rec, indent=1) + "\n")
                    print(f"  rnx2rtkp completed in {elapsed:.1f}s → {out.name}")

                if not out.exists():
                    print("  FAIL: output file not produced")
                    results.append(
                        {
                            "case_id": case["id"],
                            "mode": mode,
                            "metrics": None,
                            "status": "fail",
                            "ctx": ctx,
                        }
                    )
                    continue

                # Compare against reference
                ref_raw = parse_reference(str(ref))
                offset = case.get("ref_offset_enu")
                ref_rows = shift_reference(ref_raw, offset) if offset else ref_raw
                nmea_rows = parse_nmea(str(out))
                pairs = _match_epochs(ref_rows, nmea_rows)
                m = compute_metrics(pairs, args.skip_epochs, threshold_2d=threshold)
                # The official score covers the whole reference, so a run that
                # completed keeps it even with no matching epoch (good = 0).  The
                # PPC rule scores every reference epoch; UrbanNav and GICI, which
                # have no official rule, are scored on the rover observation grid
                # (UrbanNav: 5 Hz F9P against a 10 Hz reference; GICI: both 10 Hz,
                # so the grid keeps every epoch).  The reliability metrics use the
                # grid for all.
                interval, phase = rinex_obs_timing(str(obs))
                if args.dataset != "ppc":
                    ppc = ppc_score(ref_rows, nmea_rows, interval=interval, phase=phase)
                else:
                    ppc = ppc_score(ref_rows, nmea_rows)
                rel = reliability_metrics(ref_rows, nmea_rows, interval=interval, phase=phase)
                # Reference variants: without the offset correction, and on every
                # reference epoch.
                extra = {}
                if offset:
                    extra["ppc_raw"] = ppc_score(ref_raw, nmea_rows, interval=interval, phase=phase)
                    extra["ppc_allref"] = ppc_score(ref_rows, nmea_rows)

                if m is None:
                    print("  FAIL: no matching epochs")
                    results.append(
                        {
                            "case_id": case["id"],
                            "mode": mode,
                            "metrics": None,
                            "status": "fail",
                            "ppc": ppc,
                            "rel": rel,
                            **extra,
                            "ctx": ctx,
                            "solution": out,
                            "grid": (interval, phase),
                        }
                    )
                    continue

                # Progress line
                if not math.isnan(threshold):
                    thr_label = (
                        f"<{threshold:.1f}m" if threshold >= 1.0 else f"<{threshold * 100:.0f}cm"
                    )
                    rate_str = f"{thr_label}={m['thr_rate']:.1f}%"
                    conv_str = _fmt_s(m["conv_thr_s"])
                else:
                    rate_str = (
                        f"Fix={m['fix_rate']:.1f}%  FF={m['ff_rate']:.1f}%  "
                        f"misfix={_fmt_pct(100.0 - m['acc_rate_fix'])}"
                    )
                    conv_str = _fmt_s(m["conv_time_s"])
                print(
                    f"  N={m['n_matched']}  {rate_str}  "
                    f"RMS(fix)={_fmt_m(m['rms_2d_fix'])}  "
                    f"RMS(ff)={_fmt_m(m['rms_2d_ff'])}  "
                    f"RMS(all)={m['rms_2d_all']:.3f}m  "
                    f"TTFF={conv_str}s"
                )
                results.append(
                    {
                        "case_id": case["id"],
                        "mode": mode,
                        "metrics": m,
                        "status": "ok",
                        "ppc": ppc,
                        "rel": rel,
                        **extra,
                        "ctx": ctx,
                        "solution": out,
                        "grid": (interval, phase),
                    }
                )

                if args.plot:
                    png = out_dir / f"{case['id']}_{mode}.png"
                    plot_results(m, title=f"{case['id']} / {mode.upper()}", output_path=str(png))

        complete = True
    finally:
        # A run that stops early still leaves its records, marked incomplete.
        if not complete:
            write_run_records(
                out_dir, args, git_state, binary_info, results, started, False, hashes
            )

    print_summary(results)
    print_ppc_scores(results, args.dataset)
    print_misfix_events(results)
    print()
    write_run_records(out_dir, args, git_state, binary_info, results, started, True, hashes)
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main() -> int:
    """Entry point for the PPC-Dataset benchmark runner."""
    p = argparse.ArgumentParser(description="Run MRTKLIB kinematic benchmark on PPC-Dataset")
    p.add_argument(
        "--dataset-dir",
        default="data/benchmark",
        help="PPC-Dataset root directory (default: data/benchmark)",
    )
    p.add_argument(
        "--l6-dir",
        default="data/benchmark/l6",
        help="L6 file cache directory (default: data/benchmark/l6)",
    )
    p.add_argument(
        "--out-dir",
        default="data/benchmark/results",
        help="Output directory for NMEA results (default: data/benchmark/results)",
    )
    p.add_argument(
        "--dataset",
        choices=sorted(DATASETS),
        default="ppc",
        help="Benchmark data set: ppc (PPC-Dataset, default), urbannav (UrbanNav Tokyo, RTK)"
        " or gici (GICI Shanghai, RTK)",
    )
    p.add_argument(
        "--mode",
        choices=["single", "clas", "madoca", "rtk", "both", "all"],
        default=None,
        help="Positioning mode (default: all = clas+madoca+rtk for ppc, rtk for urbannav / gici; "
        "single = SPP baseline, rover.obs + base.nav only)",
    )
    p.add_argument("--case", default="", help="Comma-separated case IDs (default: all)")
    p.add_argument(
        "--held-out",
        action="store_true",
        help="Run the held-out cases only (GICI); they are left out otherwise."
        " For the release decision on a benchmark default, not for choosing one.",
    )
    p.add_argument(
        "--rnx2rtkp", default="", help="Path to rnx2rtkp binary (default: auto-detect under build/)"
    )
    p.add_argument(
        "--skip-download", action="store_true", help="Skip L6 download step (use existing files)"
    )
    p.add_argument(
        "--force", action="store_true", help="Re-run rnx2rtkp even if cached output exists"
    )
    p.add_argument(
        "--skip-epochs",
        type=int,
        default=60,
        help="Epochs to exclude from metrics for convergence (default: 60)",
    )
    p.add_argument("--plot", action="store_true", help="Generate ENU time-series PNG per case")
    p.add_argument("-v", "--verbose", action="store_true", help="Show rnx2rtkp stdout/stderr")
    args = p.parse_args()
    return run_benchmark(args)


if __name__ == "__main__":
    sys.exit(main())
