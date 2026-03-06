"""run_benchmark.py - Run MRTKLIB kinematic positioning benchmark on PPC-Dataset.

For each configured driving run, calls rnx2rtkp in PPP-RTK (CLAS) and/or PPP
(MADOCA) mode, then compares the NMEA output against the PPC-Dataset ground
truth reference.csv.  Results are summarised in a fixed-width ASCII table.

Requirements
------------
- PPC-Dataset must be present at --dataset-dir (manual download required).
  See docs/benchmark.md for download instructions.
- CLAS L6D and MADOCA L6E files are downloaded automatically to --l6-dir
  unless --skip-download is given.
- rnx2rtkp binary is located automatically under build/ or can be specified
  with --rnx2rtkp.

Usage
-----
    python run_benchmark.py [options]

    --dataset-dir DIR     PPC-Dataset root (default: data/benchmark)
    --l6-dir DIR          L6 file cache (default: data/benchmark/l6)
    --out-dir DIR         Results directory (default: data/benchmark/results)
    --mode clas|madoca|both  (default: both)
    --case ID[,ID...]     Run subset (default: all 6 runs)
    --rnx2rtkp PATH       rnx2rtkp binary path (default: auto-detect)
    --skip-download       Skip L6 download step
    --skip-epochs N       Epochs to skip for metrics (default: 60)
    --plot                Save per-case ENU PNG to --out-dir
    -v / --verbose        Show rnx2rtkp stdout/stderr
"""

import argparse
import math
import os
import subprocess
import sys
import time
from pathlib import Path

from cases import CASES, case_by_id
from compare_ppc import _match_epochs, compute_metrics, parse_nmea, parse_reference, plot_results
from download_l6 import ensure_case_l6

# ---------------------------------------------------------------------------
# Auto-detect rnx2rtkp
# ---------------------------------------------------------------------------
_CANDIDATE_BINS = [
    "build/rnx2rtkp",
    "build/Release/rnx2rtkp",
    "build/Debug/rnx2rtkp",
]


def _find_rnx2rtkp(hint: str = "") -> str:
    """Locate the rnx2rtkp binary.

    Args:
        hint: Explicit path supplied via --rnx2rtkp.  If non-empty, return
              as-is after verifying the file exists.

    Returns:
        Path to rnx2rtkp binary.

    Raises:
        SystemExit: If no binary can be found.
    """
    if hint:
        if not os.path.isfile(hint):
            sys.exit(f"FAIL: rnx2rtkp not found at {hint!r}")
        return hint
    # Search relative to MRTKLIB root (parent of scripts/)
    root = Path(__file__).resolve().parent.parent.parent
    for rel in _CANDIDATE_BINS:
        p = root / rel
        if p.is_file():
            return str(p)
    # Fall back to PATH
    import shutil

    p = shutil.which("rnx2rtkp")
    if p:
        return p
    sys.exit(
        "FAIL: rnx2rtkp binary not found. "
        "Build first with 'cmake --build build', or pass --rnx2rtkp PATH."
    )


# ---------------------------------------------------------------------------
# rnx2rtkp invocation
# ---------------------------------------------------------------------------
def _run_rnx2rtkp(
    rnx2rtkp: str,
    conf: str,
    week: int,
    tow_start: float,
    tow_end: float,
    output: Path,
    obs: Path,
    nav: Path,
    l6_files: list[Path],
    verbose: bool = False,
) -> bool:
    """Run rnx2rtkp for one case/mode combination.

    Args:
        rnx2rtkp: Path to rnx2rtkp binary.
        conf: Path to configuration file.
        week: GPS week (for -ts/-te flags).
        tow_start: Run start TOW with margin already subtracted.
        tow_end: Run end TOW with margin already added.
        output: Destination NMEA file path.
        obs: rover.obs path.
        nav: base.nav path.
        l6_files: List of L6 file paths.
        verbose: Pass stdout/stderr through; otherwise suppress.

    Returns:
        True if rnx2rtkp exited with code 0.
    """
    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        rnx2rtkp,
        "-k", conf,
        "-ts", str(week), f"{tow_start:.3f}",
        "-te", str(week), f"{tow_end:.3f}",
        "-o", str(output),
        str(obs),
        str(nav),
    ] + [str(f) for f in l6_files]

    if verbose:
        print("  $", " ".join(cmd))
    result = subprocess.run(
        cmd,
        stdout=None if verbose else subprocess.DEVNULL,
        stderr=None if verbose else subprocess.DEVNULL,
    )
    return result.returncode == 0


# ---------------------------------------------------------------------------
# Summary table
# ---------------------------------------------------------------------------
_HDR = (
    f"{'Case':<20} {'Mode':<7} {'N':>6} {'Fix%':>6} "
    f"{'RMS_2D(all)':>12} {'RMS_3D(all)':>12} "
    f"{'RMS_2D(fix)':>12} {'Conv_s':>8}"
)
_SEP = "-" * len(_HDR)


def _fmt_m(v: float) -> str:
    """Format metres with 2 decimal places, or 'nan'."""
    return "nan" if math.isnan(v) else f"{v:.3f} m"


def _fmt_s(v: float) -> str:
    """Format seconds as integer, or 'nan'."""
    return "nan" if math.isnan(v) else f"{v:.0f}"


def print_summary(rows: list[dict]) -> None:
    """Print a fixed-width summary table.

    Args:
        rows: List of result dicts from the benchmark loop.
    """
    print()
    print(_SEP)
    print(_HDR)
    print(_SEP)
    for r in rows:
        m = r["metrics"]
        n = m["n_matched"] if m else 0
        fix_pct = f"{m['fix_rate']:.1f}%" if m else "--"
        rms2_all = _fmt_m(m["rms_2d_all"]) if m else "--"
        rms3_all = _fmt_m(m["rms_3d_all"]) if m else "--"
        rms2_fix = _fmt_m(m["rms_2d_fix"]) if m else "--"
        conv = _fmt_s(m["conv_time_s"]) if m else "--"
        if r["status"] == "skip":
            status_tag = " [skipped]"
        elif r["status"] == "fail":
            status_tag = " [FAILED]"
        else:
            status_tag = ""
        print(
            f"{r['case_id']:<20} {r['mode']:<7} {n:>6} {fix_pct:>6} "
            f"{rms2_all:>12} {rms3_all:>12} {rms2_fix:>12} {conv:>8}"
            f"{status_tag}"
        )
    print(_SEP)


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
    # Resolve paths
    root = Path(__file__).resolve().parent.parent.parent
    dataset_dir = Path(args.dataset_dir)
    l6_dir = Path(args.l6_dir)
    out_dir = Path(args.out_dir)
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

    # Validate dataset presence
    for case in cases:
        obs = dataset_dir / case["city"] / case["run"] / "rover.obs"
        if not obs.exists():
            sys.exit(
                f"FAIL: PPC-Dataset not found at {dataset_dir!r}.\n"
                "  Download it manually and place it so that e.g.\n"
                f"  {obs} exists.\n"
                "  See docs/benchmark.md for instructions."
            )

    modes = ["clas", "madoca"] if args.mode == "both" else [args.mode]
    results = []

    for case in cases:
        city, run = case["city"], case["run"]
        obs = dataset_dir / city / run / "rover.obs"
        nav = dataset_dir / city / run / "base.nav"
        ref = dataset_dir / city / run / "reference.csv"

        # Download L6 files
        if not args.skip_download:
            print(f"Downloading L6 for {case['id']} ...")
            l6_paths = ensure_case_l6(case, l6_dir, args.mode)
        else:
            # Build expected paths without downloading
            from cases import l6_sessions
            from download_l6 import _MADOCA_PRNS

            sessions = l6_sessions(case["gps_week"], case["tow_start"], case["tow_end"])
            l6_paths: dict[str, list[Path]] = {"clas": [], "madoca": []}
            for year, doy, session in sessions:
                l6d = l6_dir / f"{year}{doy:03d}{session}.l6"
                if l6d.exists():
                    l6_paths["clas"].append(l6d)
                for prn in _MADOCA_PRNS:
                    l6e = l6_dir / f"{year}{doy:03d}{session}.{prn}.l6"
                    if l6e.exists():
                        l6_paths["madoca"].append(l6e)
                        break

        for mode in modes:
            conf = str(conf_dir / f"{mode}.conf")
            out = out_dir / f"{case['id']}_{mode}.nmea"
            l6_files = l6_paths.get(mode, [])

            print(f"\n[{case['id']}  /  {mode.upper()}]")

            if not l6_files:
                print(f"  WARNING: no L6 files found for {mode}; skipping.")
                results.append({"case_id": case["id"], "mode": mode,
                                 "metrics": None, "status": "skip"})
                continue

            # Skip if output is newer than all inputs
            if out.exists() and not args.force:
                newest_in = max(
                    obs.stat().st_mtime,
                    nav.stat().st_mtime,
                    max(f.stat().st_mtime for f in l6_files),
                )
                if out.stat().st_mtime > newest_in:
                    print(f"  [cached]  {out.name}")
                    skip_run = True
                else:
                    skip_run = False
            else:
                skip_run = False

            if not skip_run:
                t0 = time.monotonic()
                ok = _run_rnx2rtkp(
                    rnx2rtkp, conf,
                    case["gps_week"],
                    case["tow_start"] - 60, case["tow_end"] + 60,
                    out, obs, nav, l6_files,
                    verbose=args.verbose,
                )
                elapsed = time.monotonic() - t0
                if not ok:
                    print(f"  FAIL: rnx2rtkp returned non-zero (elapsed {elapsed:.1f}s)")
                    results.append({"case_id": case["id"], "mode": mode,
                                     "metrics": None, "status": "fail"})
                    continue
                print(f"  rnx2rtkp completed in {elapsed:.1f}s → {out.name}")

            if not out.exists():
                print("  FAIL: output file not produced")
                results.append({"case_id": case["id"], "mode": mode,
                                 "metrics": None, "status": "fail"})
                continue

            # Compare against reference
            ref_rows = parse_reference(str(ref))
            nmea_rows = parse_nmea(str(out))
            pairs = _match_epochs(ref_rows, nmea_rows)
            m = compute_metrics(pairs, args.skip_epochs)

            if m is None:
                print("  FAIL: no matching epochs")
                results.append({"case_id": case["id"], "mode": mode,
                                 "metrics": None, "status": "fail"})
                continue

            print(
                f"  N={m['n_matched']}  Fix={m['fix_rate']:.1f}%  "
                f"RMS_2D={m['rms_2d_all']:.3f}m  "
                f"RMS_2D(fix)={_fmt_m(m['rms_2d_fix'])}  "
                f"Conv={_fmt_s(m['conv_time_s'])}s"
            )
            results.append({"case_id": case["id"], "mode": mode,
                             "metrics": m, "status": "ok"})

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
    p = argparse.ArgumentParser(
        description="Run MRTKLIB kinematic benchmark on PPC-Dataset"
    )
    p.add_argument("--dataset-dir", default="data/benchmark",
                   help="PPC-Dataset root directory (default: data/benchmark)")
    p.add_argument("--l6-dir", default="data/benchmark/l6",
                   help="L6 file cache directory (default: data/benchmark/l6)")
    p.add_argument("--out-dir", default="data/benchmark/results",
                   help="Output directory for NMEA results (default: data/benchmark/results)")
    p.add_argument("--mode", choices=["clas", "madoca", "both"], default="both",
                   help="Positioning mode to run (default: both)")
    p.add_argument("--case", default="",
                   help="Comma-separated case IDs (default: all)")
    p.add_argument("--rnx2rtkp", default="",
                   help="Path to rnx2rtkp binary (default: auto-detect under build/)")
    p.add_argument("--skip-download", action="store_true",
                   help="Skip L6 download step (use existing files)")
    p.add_argument("--force", action="store_true",
                   help="Re-run rnx2rtkp even if cached output exists")
    p.add_argument("--skip-epochs", type=int, default=60,
                   help="Epochs to exclude from metrics for convergence (default: 60)")
    p.add_argument("--plot", action="store_true",
                   help="Generate ENU time-series PNG per case")
    p.add_argument("-v", "--verbose", action="store_true",
                   help="Show rnx2rtkp stdout/stderr")
    args = p.parse_args()
    return run_benchmark(args)


if __name__ == "__main__":
    sys.exit(main())
