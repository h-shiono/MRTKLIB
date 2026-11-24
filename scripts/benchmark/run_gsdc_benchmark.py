"""run_gsdc_benchmark.py - Run MRTKLIB SPP on the GSDC-2023 smartphone dataset.

For each selected ``trip/device`` case this:
  1. converts ``device_gnss.csv`` to RINEX 3.04 OBS (``gsdc_to_rinex``),
  2. fetches the daily merged broadcast nav for the UTC day(s) it spans
     (``download_brdc``),
  3. runs ``mrtk post`` single-point positioning with a SPP config, and
  4. compares the NMEA output against ``ground_truth.csv`` (``compare_gsdc``).

Results are summarised in a fixed-width table.  This is the smartphone analogue
of ``run_benchmark.py`` (PPC) and is the venue where the #116 SPP work's P5
(clock-jump) and P6 (position EKF) stages are validated - see issue #165 and
``docs/design/spp-accuracy.md`` 4.6.

Requirements
------------
- GSDC-2023 extracted under --dataset-dir (manual download; see
  docs/reference/benchmark-gsdc.md).
- mrtk binary built under build/ (auto-detected) or passed via --mrtk.
- Network access for broadcast-nav download (unless --skip-download and cached).
- Python: numpy.

Usage
-----
    python run_gsdc_benchmark.py [--case curated|all|<id,...>] [--conf PATH]...
"""

import argparse
import math
import os
import subprocess
import sys
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare_gsdc import gsdc_official, parse_ground_truth  # noqa: E402
from compare_ppc import _match_epochs, compute_metrics, parse_nmea  # noqa: E402
from download_brdc import ensure_brdc  # noqa: E402
from gsdc_cases import safe_name, select_cases  # noqa: E402
from gsdc_to_rinex import _approx_xyz, parse_csv, write_rinex  # noqa: E402

# mrtk binary auto-detection (mirrors run_benchmark.py).
_CANDIDATE_BINS = [
    "build/mrtk",
    "build/Release/mrtk",
    "build/Debug/mrtk",
]


def _find_mrtk(hint: str = "") -> str:
    """Locate the mrtk binary (build/ first, then PATH)."""
    if hint:
        if not os.path.isfile(hint):
            sys.exit(f"FAIL: mrtk not found at {hint!r}")
        return hint
    root = Path(__file__).resolve().parent.parent.parent
    for rel in _CANDIDATE_BINS:
        p = root / rel
        if p.is_file():
            return str(p)
    import shutil

    p = shutil.which("mrtk")
    if p:
        return p
    sys.exit("FAIL: mrtk not found. Build with 'cmake --build build' or pass --mrtk.")


def _utc_days(epoch_millis) -> list[tuple[int, int]]:
    """Return the sorted set of (year, doy) UTC days an epoch span covers."""
    lo, hi = min(epoch_millis), max(epoch_millis)
    days = set()
    t = datetime.fromtimestamp(lo / 1000.0, tz=timezone.utc).date()
    end = datetime.fromtimestamp(hi / 1000.0, tz=timezone.utc).date()
    while t <= end:
        days.add((t.year, t.timetuple().tm_yday))
        t += timedelta(days=1)
    return sorted(days)


def _convert(case: dict, out_dir: Path, force: bool) -> tuple[Path, list]:
    """Convert a case's device_gnss.csv to RINEX OBS (cached); return (obs, days)."""
    obs = out_dir / f"{safe_name(case['id'])}.obs"
    epochs, sys_bands = parse_csv(str(case["gnss_csv"]))
    if not epochs:
        raise RuntimeError("no usable measurements")
    days = _utc_days(epochs.keys())
    if force or not obs.exists() or obs.stat().st_mtime <= case["gnss_csv"].stat().st_mtime:
        approx = _approx_xyz(str(case["gnss_csv"]))
        write_rinex(epochs, sys_bands, str(obs), safe_name(case["id"])[:60], approx)
    return obs, days


def _run_mrtk(
    mrtk: str, confs: list[str], obs: Path, navs: list[Path], out: Path, verbose: bool
) -> bool:
    """Invoke ``mrtk post`` for one case; return True on success."""
    cmd = [mrtk, "post"]
    for c in confs:
        cmd += ["-k", c]
    cmd += ["-o", str(out.resolve()), str(obs.resolve())]
    cmd += [str(n.resolve()) for n in navs]
    if verbose:
        print("  $", " ".join(cmd))
    r = subprocess.run(
        cmd,
        stdout=None if verbose else subprocess.DEVNULL,
        stderr=None if verbose else subprocess.DEVNULL,
    )
    return r.returncode == 0


# ---------------------------------------------------------------------------
# Summary table
# ---------------------------------------------------------------------------
_HDR = (
    f"{'Case':<42} {'N':>5} {'Cov%':>6} {'nSV':>5} {'<2m%':>6} "
    f"{'RMS2D':>8} {'p50':>8} {'p95':>8} {'matched':>8} {'official':>8} {'fill%':>6}"
)
_SEP = "-" * len(_HDR)


def _m(v: float) -> str:
    return "nan" if math.isnan(v) else f"{v:.2f}"


def gsdc_score(m: dict) -> float:
    """GSDC metric over the matched epochs only.

    Mean of the 50th and 95th percentile 2D horizontal error (metres).  The
    official score, over every truth epoch, is ``compare_gsdc.gsdc_official()``.
    """
    return (m["p50_2d_all"] + m["p95_2d_all"]) / 2.0


def _off_cols(r: dict) -> str:
    off = r.get("official")
    if off is None:
        return f"{'—':>8} {'—':>6}"
    return f"{_m(off['score']):>8} {off['filled_rate']:>5.1f}%"


def print_summary(rows: list[dict]) -> None:
    """Print the fixed-width GSDC SPP summary table.

    ``official`` is the Google Smartphone Decimeter Challenge 2023 metric (mean
    of the 50th and 95th percentile horizontal error) over every ground-truth
    epoch, gaps filled with the latest solution (``compare_gsdc.gsdc_official``);
    ``fill%`` is the share of epochs scored with a filled value.  ``matched`` is
    the same statistic over the matched epochs only, which leaves out the
    stretches without a solution.  Scores are on the train split, whose ground
    truth is public; they are not leaderboard (test split) numbers.

    The official MEAN is over every case with ground truth.  If any such case
    has no official score (conversion, navigation or solver failure, or no
    solution at all), the MEAN is reported as incomplete instead of averaging
    the rest.
    """
    print()
    print(_SEP)
    print(_HDR)
    print(_SEP)
    for r in rows:
        m = r["metrics"]
        cid = r["id"][:42]
        if m is None:
            d = "—"
            print(
                f"{cid:<42} {d:>5} {d:>6} {d:>5} {d:>6} "
                f"{d:>8} {d:>8} {d:>8} {d:>8} {_off_cols(r)}  [{r['status']}]"
            )
            continue
        print(
            f"{cid:<42} {m['n_matched']:>5} {r['coverage']:>5.1f}% "
            f"{m['mean_sv_all']:>5.1f} "
            f"{m['thr_rate']:>5.1f}% {_m(m['rms_2d_all']):>8} "
            f"{_m(m['p50_2d_all']):>8} {_m(m['p95_2d_all']):>8} "
            f"{_m(gsdc_score(m)):>8} {_off_cols(r)}"
        )
    print(_SEP)
    ok = [r["metrics"] for r in rows if r["metrics"]]
    truth = [r for r in rows if r["status"] != "no-truth"]
    unscored = [r for r in truth if r.get("official") is None]
    if unscored:
        official_mean = f"incomplete ({len(unscored)} of {len(truth)} cases not scored)"
    elif truth:
        mean_score = sum(r["official"]["score"] for r in truth) / len(truth)
        mean_fill = sum(r["official"]["filled_rate"] for r in truth) / len(truth)
        official_mean = f"{_m(mean_score):>8} {mean_fill:>5.1f}%"
    else:
        official_mean = "—"
    if ok:
        import numpy as np

        print(
            f"{'MEAN':<42} {int(np.mean([m['n_matched'] for m in ok])):>5} "
            f"{np.mean([r['coverage'] for r in rows if r['metrics']]):>5.1f}% "
            f"{np.mean([m['mean_sv_all'] for m in ok]):>5.1f} "
            f"{np.mean([m['thr_rate'] for m in ok]):>5.1f}% "
            f"{np.mean([m['rms_2d_all'] for m in ok]):>8.2f} "
            f"{np.mean([m['p50_2d_all'] for m in ok]):>8.2f} "
            f"{np.mean([m['p95_2d_all'] for m in ok]):>8.2f} "
            f"{np.mean([gsdc_score(m) for m in ok]):>8.2f} {official_mean}"
        )
    else:
        print(f"{'MEAN':<42} official {official_mean}")
    print(
        "  official = mean(p50, p95) 2D horizontal [m] over every truth epoch, gaps filled"
        " with the latest solution (GSDC-2023 metric, train split)"
    )
    print(
        "  matched  = the same over matched epochs only; fill% = epochs scored with a filled value"
    )


def run(args: argparse.Namespace) -> int:
    """Execute the GSDC benchmark loop."""
    root = Path(__file__).resolve().parent.parent.parent
    dataset_dir = Path(args.dataset_dir)
    if not dataset_dir.is_absolute():
        dataset_dir = root / dataset_dir
    brdc_dir = Path(args.brdc_dir)
    if not brdc_dir.is_absolute():
        brdc_dir = root / brdc_dir
    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute():
        out_dir = root / out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    confs = args.conf or [str(root / "conf" / "benchmark" / "single.toml")]
    mrtk = _find_mrtk(args.mrtk)

    cases = select_cases(dataset_dir, args.case)
    if not any(not c.get("missing") for c in cases):
        sys.exit(
            f"FAIL: no cases under {dataset_dir} (extract the dataset; "
            "see docs/reference/benchmark-gsdc.md)"
        )

    print(f"mrtk     : {mrtk}")
    print(f"dataset  : {dataset_dir}")
    print(f"conf     : {' + '.join(confs)}")
    print(f"cases    : {len(cases)} ({args.case})")

    rows = []
    for case in cases:
        print(f"\n[{case['id']}]")
        if case.get("missing"):
            print("  FAIL: requested case not found (no device_gnss.csv)")
            rows.append({"id": case["id"], "metrics": None, "status": "missing-data"})
            continue
        # Check for truth first, so a missing nav cache does not decide whether
        # a case without truth counts against the MEAN.
        if case["ground_truth"] is None:
            print("  skip: no ground truth (test split)")
            rows.append({"id": case["id"], "metrics": None, "status": "no-truth"})
            continue
        try:
            obs, days = _convert(case, out_dir, args.force)
        except Exception as exc:  # noqa: BLE001
            print(f"  FAIL convert: {exc}")
            rows.append({"id": case["id"], "metrics": None, "status": "convert-fail"})
            continue

        navs = []
        for year, doy in days:
            nav = (
                ensure_brdc(year, doy, brdc_dir, dry_run=False)
                if not args.skip_download
                else _cached_nav(brdc_dir, year, doy)
            )
            if nav:
                navs.append(nav)
        if not navs:
            print("  FAIL: no broadcast nav available")
            rows.append({"id": case["id"], "metrics": None, "status": "no-nav"})
            continue

        out = out_dir / f"{safe_name(case['id'])}.nmea"
        # Write to a temporary path and publish only what this run produced: a
        # solver that exits 0 without writing must neither be scored from an old
        # output nor erase it.
        tmp = out.with_name(out.name + ".tmp")
        tmp.unlink(missing_ok=True)
        t0 = time.monotonic()
        ok = _run_mrtk(mrtk, confs, obs, navs, tmp, args.verbose)
        dt = time.monotonic() - t0
        if not ok or not tmp.exists():
            tmp.unlink(missing_ok=True)
            print(f"  FAIL mrtk post ({dt:.1f}s)")
            rows.append({"id": case["id"], "metrics": None, "status": "mrtk-fail"})
            continue
        tmp.replace(out)

        ref = parse_ground_truth(str(case["ground_truth"]))
        nmea = parse_nmea(str(out))
        # Every truth epoch is scored, so the official score does not depend
        # on matching or on --skip-epochs.
        off = gsdc_official(ref, nmea)
        pairs = _match_epochs(ref, nmea)
        m = compute_metrics(pairs, args.skip_epochs, threshold_2d=args.threshold)
        if m is None:
            print("  FAIL: no matching epochs")
            rows.append({"id": case["id"], "metrics": None, "status": "no-match", "official": off})
            continue
        ref_usable = max(0, len(ref) - args.skip_epochs)
        coverage = m["n_matched"] / ref_usable * 100.0 if ref_usable else math.nan
        print(
            f"  N={m['n_matched']}  <2m={m['thr_rate']:.1f}%  "
            f"p50={m['p50_2d_all']:.2f}m  p95={m['p95_2d_all']:.2f}m  "
            f"matched={gsdc_score(m):.2f}m  official={_m(off['score'])}m  "
            f"cov={coverage:.1f}%  ({dt:.1f}s)"
        )
        rows.append(
            {
                "id": case["id"],
                "metrics": m,
                "coverage": coverage,
                "status": "ok",
                "official": off,
            }
        )

    print_summary(rows)
    return 0


def _cached_nav(brdc_dir: Path, year: int, doy: int):
    """Return a cached nav file path for the day, or None (no download)."""
    from download_brdc import find_cached

    return find_cached(brdc_dir, year, doy)


def main() -> int:
    """Entry point for the GSDC smartphone SPP benchmark runner."""
    p = argparse.ArgumentParser(description="Run MRTKLIB SPP benchmark on GSDC-2023")
    p.add_argument("--dataset-dir", default="data/gsdc", help="GSDC root (has train/)")
    p.add_argument("--brdc-dir", default="data/gsdc/brdc", help="broadcast-nav cache")
    p.add_argument("--out-dir", default="data/gsdc/results", help="OBS/NMEA output dir")
    p.add_argument(
        "--case",
        default="curated",
        help="'curated' (default), 'all', or comma-separated trip/device ids",
    )
    p.add_argument(
        "--conf",
        action="append",
        default=[],
        help="config file(s), layered; default conf/benchmark/single.toml",
    )
    p.add_argument("--mrtk", default="", help="mrtk binary path (default: auto)")
    p.add_argument("--threshold", type=float, default=2.0, help="2D <Nm rate threshold")
    p.add_argument("--skip-epochs", type=int, default=0, help="initial epochs to skip")
    p.add_argument("--skip-download", action="store_true", help="use cached nav only")
    p.add_argument("--force", action="store_true", help="re-convert even if cached")
    p.add_argument("-v", "--verbose", action="store_true", help="show mrtk output")
    return run(p.parse_args())


if __name__ == "__main__":
    sys.exit(main())
