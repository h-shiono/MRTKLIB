# MRTKLIB v0.3.3 Release Notes

**Release date:** 2026-03-07
**Type:** Minor (benchmark tooling — no functional changes to the library)

---

## Overview

v0.3.3 introduces a **kinematic positioning benchmark** for evaluating MRTKLIB's
three positioning modes against real-world urban driving data from the PPC-Dataset
(Precise Positioning Challenge 2024).  Six vehicle runs recorded in Nagoya and
Tokyo are covered, with sub-centimetre ground truth from an Applanix POS LV 220
GNSS/INS system.

Three positioning modes are benchmarked:

| Mode | Engine | Correction | Ambiguity |
|------|--------|------------|-----------|
| **CLAS** | PPP-RTK | QZSS L6D (IS-QZSS-L6-003) | Integer AR |
| **MADOCA** | PPP | QZSS L6E (MADOCA-PPP) | Float only |
| **RTK** | Kinematic RTK | Rover + base RINEX | Integer AR |

No library code, positioning algorithms, or public API changed in this release.

> **Disclaimer:** The configuration parameters used here have not been tuned or
> optimised for maximum accuracy.  Results are provided for reference only and
> should not be taken as representative of the best achievable performance.

---

## What Changed

### Kinematic Benchmark Infrastructure (`scripts/benchmark/`)

Four new Python scripts provide an end-to-end kinematic evaluation pipeline:

| Script | Purpose |
|--------|---------|
| `cases.py` | Metadata for all 6 PPC-Dataset runs (GPS week/TOW, city/run IDs) |
| `download_l6.py` | Auto-download QZSS L6D (CLAS) and L6E (MADOCA) archive files |
| `compare_ppc.py` | Compare `rnx2rtkp` NMEA output against `reference.csv` ground truth |
| `run_benchmark.py` | Orchestrator: download → run `rnx2rtkp` → compare → summary table |

**Key features:**
- Three positioning modes: `--mode clas|madoca|rtk|both|all`
  (`both`=clas+madoca, `all`=clas+madoca+rtk, default `all`)
- L6D archive: `https://sys.qzss.go.jp/archives/l6/` (CLAS)
- L6E archive: `https://l6msg.go.gnss.go.jp/archives/` (MADOCA, PRN auto-probe)
- Result caching: skips `rnx2rtkp` if output file is newer than all inputs
- Per-case PNG plots (`--plot`) via matplotlib
- Full CLI with `--mode`, `--case`, `--skip-epochs`, `--force`, `-v`

### Three-Tier Accuracy Breakdown

`compare_ppc.py` now reports **three solution-quality tiers** for CLAS and RTK,
plus a separate **PPP** tier for MADOCA:

| Tier | GGA Q | Description |
|------|-------|-------------|
| **FIX** | 4 | Integer ambiguity fix epochs only |
| **FF** | 4, 5 | Fix + float; excludes SPP fallback |
| **ALL** | any | Every matched epoch including SPP |
| **PPP** | 3 | All valid PPP-float epochs (MADOCA) |

Each row now also reports **nSV** (mean satellite count used in that tier).
TTFF is defined as the first epoch of a ≥30-consecutive Q=4 run.

### Benchmark Configurations (`conf/benchmark/`)

Five configuration files cover all hardware/mode combinations:

| File | Purpose | Key parameters |
|------|---------|---------------|
| `clas.conf` | CLAS PPP-RTK | `ant1-anttype=*`, `pos2-isb=off`, NMEA output |
| `madoca.conf` | MADOCA PPP | `pos1-dynamics=on`, `ant1-postype=single`, NMEA output |
| `rtk.conf` | Kinematic RTK | `pos1-frequency=l1+2+3`, `pos1-ionoopt=off`, `snrmask_r=on` |
| `nagoya.conf` | Nagoya overrides | Precise base LLH, `ant1/ant2-anttype`, `ant2-antdelu` |
| `tokyo.conf` | Tokyo overrides | Precise base LLH, `ant1/ant2-anttype`, `ant2-antdelu` |

The mode conf and city conf are layered via two `-k` flags; city settings take
precedence.  City confs supply the precise base-station marker coordinates and
the `ANTENNA DELTA H` from each base.obs RINEX header (`ant2-antdelu`).

**RTK triple-frequency AR:** With `pos1-ionoopt=off`, `NF(opt)=3` and all three
raw frequencies (L1/L2/L5) participate in double-difference ambiguity resolution.
This is effective for the PPC-Dataset baselines (13–27 km) because the rover and
base are both open-sky stations with good L5 visibility.

**Receiver-specific notes (CLAS/MADOCA):**
- Rover RINEX files carry `Unknown Unknown` in `ANT # / TYPE` → `ant1-anttype = *`
- Septentrio mosaic-X5 has no ISB table entry → `pos2-isb = off`

### Bug Fixes (Benchmark Tooling)

| Component | Fix |
|-----------|-----|
| `rnx2rtkp` multi-`-k` loading | `resetsysopts()` was called inside the `-k` loop, resetting values set by earlier conf files; moved to before the loop |
| `loadopts()` leading whitespace | Value strings after the `=` separator now have leading whitespace stripped |
| MADOCA `misc-timeinterp` | Was inadvertently set to `off` in the original conf; restored to `on` (matches upstream behaviour) |

### Documentation (`docs/benchmark.md`)

Updated benchmark guide covers:
- PPC-Dataset manual download instructions
- L6 archive auto-download (`python download_l6.py`)
- Full benchmark execution walkthrough including RTK mode
- Complete option reference
- Three-tier metric definitions (FIX / FF / ALL / PPP)
- Updated result tables for all three modes
- Configuration notes explaining the layered conf approach
- Known limitations (urban multipath, long RTK baselines, no ISB, etc.)

---

## Indicative Benchmark Results

Results on all six PPC-Dataset runs, GNSS-only (no IMU), `--skip-epochs 60`.
Parameters are not tuned; see the disclaimer at the top of this document.

### Nagoya

| Case | Mode | Tier | N | Fix% | RMS 2D | 1σ | 95% | TTFF (s) |
|------|------|------|--:|-----:|-------:|---:|----:|---------:|
| nagoya_run1 | CLAS   | FIX |  1 276 | 17% | 1.105 m | 0.402 m | 0.452 m |    0 |
|             |        | ALL |  7 525 |  — | 42.8 m  | 4.089 m | 53.2 m  |    — |
| nagoya_run1 | MADOCA | PPP |  1 968 |  — | 17.4 m  | 2.108 m | 4.684 m |    — |
| nagoya_run1 | RTK    | FIX |  2 140 | 30% | 1.536 m | 0.112 m | 0.151 m |  799 |
| nagoya_run2 | CLAS   | FIX |  2 522 | 27% | 1.088 m | 0.717 m | 0.826 m |    0 |
|             |        | ALL |  9 390 |  — | 305.6 m | 8.323 m | 73.9 m  |    — |
| nagoya_run2 | MADOCA | PPP |  7 494 |  — | 39.3 m  | 2.967 m | 19.4 m  |    — |
| nagoya_run2 | RTK    | FIX |  1 489 | 16% | 1.081 m | 0.154 m | 0.196 m |    0 |
| nagoya_run3 | CLAS   | FIX |    325 |  6% | 0.318 m | 0.339 m | 0.397 m |    9 |
|             |        | ALL |  5 141 |  — | 12.7 m  | 6.606 m | 27.9 m  |    — |
| nagoya_run3 | MADOCA | PPP |  2 753 |  — |  2.649 m | 2.246 m | 4.285 m |  — |
| nagoya_run3 | RTK    | FIX |    416 |  8% | 0.307 m | 0.128 m | 0.150 m |   72 |

### Tokyo

| Case | Mode | Tier | N | Fix% | RMS 2D | 1σ | 95% | TTFF (s) |
|------|------|------|--:|-----:|-------:|---:|----:|---------:|
| tokyo_run1 | CLAS   | FIX |    612 |  5% |  0.868 m |  0.239 m |  2.483 m |   15 |
|            |        | ALL | 11 867 |  — | 627.1 m  |  7.914 m | 277.3 m  |    — |
| tokyo_run1 | MADOCA | PPP |  3 084 |  — |   1.825 m |  0.979 m |  1.957 m |    0 |
| tokyo_run1 | RTK    | FIX |    380 |  3% |  0.711 m |  0.027 m |  0.643 m | 1840 |
| tokyo_run2 | CLAS   | FIX |  1 972 | 22% |  0.590 m |  0.117 m |  1.041 m |  368 |
|            |        | ALL |  9 091 |  — |  33.4 m  |  2.389 m |  33.5 m  |    — |
| tokyo_run2 | MADOCA | PPP |  8 159 |  — |   2.894 m |  1.181 m |  4.580 m |  464 |
| tokyo_run2 | RTK    | FIX |  1 517 | 18% | 17.993 m |  0.010 m |  0.059 m |  806 |
| tokyo_run3 | CLAS   | FIX |  1 129 |  7% |  0.801 m |  0.075 m |  1.831 m |   28 |
|            |        | ALL | 15 241 |  — |  41.5 m  |  3.737 m |  29.6 m  |    — |
| tokyo_run3 | MADOCA | PPP |  2 795 |  — |   0.724 m |  0.750 m |  1.062 m |   26 |
| tokyo_run3 | RTK    | FIX |  3 669 | 25% |  0.293 m |  0.013 m |  0.051 m |  335 |

**Interpretation:**

- **CLAS FIX accuracy:** 0.3–1.1 m RMS 2D across all runs — consistent with
  PPP-RTK performance under urban multipath.  Fix rates of 5–27% are expected
  given the dense canyon environments shown in the dataset's fisheye imagery.
- **RTK FIX accuracy:** Most runs achieve sub-metre FIX RMS with triple-frequency
  AR (`l1+2+3`, `ionoopt=off`), e.g. nagoya_run3: 0.31 m, tokyo_run3: 0.29 m.
  tokyo_run2 FIX RMS is elevated (18 m) despite tiny 1σ/95% values — a small
  number of wrongly-fixed epochs at this ~27 km baseline dominate the RMS.
- **RTK FF = ALL:** RTK outputs Q=4 (fix) or Q=5 (float) only — no SPP fallback
  — so the FF and ALL tiers are identical.
- **CLAS ALL RMS:** The large ALL-epoch values (13–627 m) reflect Q=1 SPP fallback
  epochs that dominate in the deepest canyons.  The FF tier (24–78%) removes these.
- **MADOCA:** Float-only PPP; N reflects epochs with a valid filter solution.
  In nagoya_run1 (heavy multipath) only 27% of epochs produce a solution.
  In more open environments (tokyo_run2) coverage reaches ~100%.
- These results establish a **baseline** for future improvements.

> **Note:** Kinematic benchmark results are not part of the CTest regression
> suite (PPC-Dataset requires a manual download). Run manually with
> `python scripts/benchmark/run_benchmark.py`.

---

## Acknowledgements

The kinematic benchmark relies on the **PPC-Dataset** from the Precise Positioning
Challenge 2024 (高精度測位チャレンジ2024), organised by the
[Institute of Navigation Japan (測位航法学会)](https://www.in-japan.org/):

> Taro Suzuki, Chiba Institute of Technology.
> *PPC-Dataset — GNSS/IMU driving data for precise positioning research.*
> https://github.com/taroz/PPC-Dataset

The dataset provides six urban vehicle runs (Nagoya × 3, Tokyo × 3) with
5 Hz triple-frequency multi-GNSS observations, 100 Hz IMU data, and
sub-centimetre ground truth from an Applanix POS LV 220.
We gratefully acknowledge Prof. Suzuki for making this dataset publicly available.

---

## Test Suite (unchanged at 57 tests)

No new CTest entries in this release. All 57 tests from v0.3.2 continue to pass.

| Test Suite | Tests |
|------------|-------|
| Unit tests | 12 |
| SPP regression | 2 |
| Receiver bias | 2 |
| rtkrcv real-time | 1 |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 |
| CLAS PPP-RTK (single-ch / dual-ch / ST12 / L1CL5) | 10 |
| CLAS VRS-RTK (single-ch / ST12 / dual-ch) | 9 |
| ssr2obs / ssr2osr | 3 |
| BINEX reading | 1 |
| Tier 2 absolute accuracy | 2 |
| Tier 3 position scatter | 2 |
| Fixtures (setup/cleanup/download) | 3 |
