"""run_benchmark.py - Run MRTKLIB kinematic positioning benchmark on PPC-Dataset.

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

    --dataset-dir DIR           PPC-Dataset root (default: data/benchmark)
    --l6-dir DIR                L6 file cache (default: data/benchmark/l6)
    --out-dir DIR               Results directory (default: data/benchmark/results)
    --mode single|clas|madoca|rtk|both|all  (both=clas+madoca, all=clas+madoca+rtk, default: all)
                                single = SPP baseline (rover.obs + base.nav only)
    --case ID[,ID...]           Run subset (default: all 6 runs)
    --rnx2rtkp PATH             rnx2rtkp binary path (default: auto-detect)
    --skip-download             Skip L6 download step
    --skip-epochs N             Epochs to skip for metrics (default: 60)
    --plot                      Save per-case ENU PNG to --out-dir
    -v / --verbose              Show rnx2rtkp stdout/stderr
"""

import argparse
import math
import os
import subprocess
import sys
import time
from pathlib import Path

from cases import CASES, case_by_id, tow_to_gpst
from compare_ppc import _match_epochs, compute_metrics, parse_nmea, parse_reference, plot_results
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
        newest_in = max(p.stat().st_mtime for p in group)
        if not out.exists() or out.stat().st_mtime <= newest_in:
            merge_dir.mkdir(parents=True, exist_ok=True)
            with open(out, "wb") as fo:
                for p in group:
                    fo.write(p.read_bytes())
            print(f"  merged {len(group)} L6 sessions -> {out.name}")
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
def _run_rnx2rtkp(
    rnx2rtkp: str,
    conf: str,
    city_conf: str,
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
        city_conf: Path to city override configuration file (applied after conf).
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
            "-k",
            city_conf,
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

    rnx2rtkp = _find_rnx2rtkp(args.rnx2rtkp)
    print(f"rnx2rtkp  : {rnx2rtkp}")
    print(f"Dataset   : {dataset_dir}")
    print(f"L6 cache  : {l6_dir}")
    print(f"Results   : {out_dir}")
    print(f"Mode      : {args.mode}")
    print()

    # Filter cases
    cases = CASES
    if args.case:
        ids = {c.strip() for c in args.case.split(",")}
        try:
            cases = [case_by_id(cid) for cid in ids]
        except KeyError as exc:
            sys.exit(f"FAIL: {exc}")

    # Validate dataset presence (rover.obs, base.nav, reference.csv)
    for case in cases:
        case_dir = dataset_dir / case["city"] / case["run"]
        for fname in ("rover.obs", "base.nav", "reference.csv"):
            fpath = case_dir / fname
            if not fpath.exists():
                sys.exit(
                    f"FAIL: PPC-Dataset file not found: {fpath}\n"
                    f"  Dataset root: {dataset_dir}\n"
                    "  Download it manually — see docs/benchmark.md for instructions."
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

    for case in cases:
        city, run = case["city"], case["run"]
        obs = dataset_dir / city / run / "rover.obs"
        nav = dataset_dir / city / run / "base.nav"
        base_obs = dataset_dir / city / run / "base.obs"
        ref = dataset_dir / city / run / "reference.csv"

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

            if not extra_files and mode != "single":
                print(f"  WARNING: no input files found for {mode}; skipping.")
                results.append(
                    {"case_id": case["id"], "mode": mode, "metrics": None, "status": "skip"}
                )
                continue

            # Skip if output is newer than all inputs
            if out.exists() and not args.force:
                in_mtimes = [obs.stat().st_mtime, nav.stat().st_mtime]
                in_mtimes += [f.stat().st_mtime for f in extra_files]
                newest_in = max(in_mtimes)
                if out.stat().st_mtime > newest_in:
                    print(f"  [cached]  {out.name}")
                    skip_run = True
                else:
                    skip_run = False
            else:
                skip_run = False

            if not skip_run:
                t0 = time.monotonic()
                city_conf = str(conf_dir / f"{case['city']}.toml")
                ok = _run_rnx2rtkp(
                    rnx2rtkp,
                    conf,
                    city_conf,
                    case["gps_week"],
                    case["tow_start"] - 60,
                    case["tow_end"] + 60,
                    out,
                    obs,
                    nav,
                    extra_files,
                    cwd=str(root),
                    verbose=args.verbose,
                )
                elapsed = time.monotonic() - t0
                if not ok:
                    print(f"  FAIL: rnx2rtkp returned non-zero (elapsed {elapsed:.1f}s)")
                    results.append(
                        {"case_id": case["id"], "mode": mode, "metrics": None, "status": "fail"}
                    )
                    continue
                print(f"  rnx2rtkp completed in {elapsed:.1f}s → {out.name}")

            if not out.exists():
                print("  FAIL: output file not produced")
                results.append(
                    {"case_id": case["id"], "mode": mode, "metrics": None, "status": "fail"}
                )
                continue

            # Compare against reference
            ref_rows = parse_reference(str(ref))
            nmea_rows = parse_nmea(str(out))
            pairs = _match_epochs(ref_rows, nmea_rows)
            m = compute_metrics(pairs, args.skip_epochs, threshold_2d=threshold)

            if m is None:
                print("  FAIL: no matching epochs")
                results.append(
                    {"case_id": case["id"], "mode": mode, "metrics": None, "status": "fail"}
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
            results.append({"case_id": case["id"], "mode": mode, "metrics": m, "status": "ok"})

            if args.plot:
                png = out_dir / f"{case['id']}_{mode}.png"
                plot_results(m, title=f"{case['id']} / {mode.upper()}", output_path=str(png))

    print_summary(results)
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
        "--mode",
        choices=["single", "clas", "madoca", "rtk", "both", "all"],
        default="all",
        help="Positioning mode (default: all = clas+madoca+rtk; "
        "single = SPP baseline, rover.obs + base.nav only)",
    )
    p.add_argument("--case", default="", help="Comma-separated case IDs (default: all)")
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
