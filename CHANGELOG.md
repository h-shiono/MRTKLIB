# Changelog

All notable changes to MRTKLIB are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v0.4.2] - 2026-03-08

**PPP-RTK / PPP accuracy release** — Extends the [demo5](https://github.com/rtklibexplorer/RTKLIB)
kinematic algorithm improvements from the RTK engine (v0.4.1) to the CLAS PPP-RTK
and MADOCA PPP engines.  RTK and VRS-RTK engines are unchanged.

### Added

- **`detslp_dop()` for PPP-RTK and PPP** — Doppler-based cycle-slip detection with
  clock-jump mean removal, identical to the RTK implementation ported in v0.4.1.
  Activated by `pos2-thresdop > 0.0` (library default: 0 = disabled).
- **`detslp_code()` for PPP-RTK and PPP** — Observation-code-change cycle-slip
  detector; flags slips whenever a satellite's tracked signal code changes between
  epochs, indicating a receiver re-acquisition that shifts the integer ambiguity.
- **`ph[0][f]` / `pt[0][f]` update in PPP `update_stat()`** — PPP did not previously
  record phase observations needed by `detslp_dop()`; the update was added to enable
  Doppler slip detection on the next epoch.

### Changed

- **`ephpos()` GLONASS clock rejection** — Added `if (fabs(geph->taun) > 1.0) return 0`
  guard, matching the protection already present in `ephclk()`.  Corrupted GLONASS
  clock entries (`|taun| > 1 s`) could propagate incorrect `dts[0]` into PPP-RTK and
  PPP via `satpos_ssr()`.
- **PPP-RTK PAR — position variance gate (B2)** — `ppp_rtk_pos()` now skips AR when
  the mean diagonal of the 3 × 3 position covariance block exceeds `thresar[1]`,
  preventing premature fixing before filter convergence.
- **PPP-RTK PAR — arfilter (B3)** — After PAR fails, newly-locked satellites
  (lock count = 0) are backed off and LAMBDA is retried, matching the RTK `arfilter`
  behaviour (activated by `pos2-arfilter`).
- **Full per-constellation EFACT in `varerr()`** — Both `mrtk_ppp_rtk.c` and
  `mrtk_ppp.c` now expand the system error factor to all seven constellations
  (GAL/QZS/CMP/IRN) via `mrtk_const.h` constants; previously GPS/GLO/SBS only.
- **Adaptive outlier threshold in `residual_test()` (PPP-RTK only)** — The
  sigma-normalised rejection threshold is inflated 10× for phase residuals whose
  corresponding bias state was just initialised (`P[IB,IB] == SQR(std[0])`).

### Fixed

- **PPP adaptive outlier threshold regression** — An equivalent D2 threshold inflation
  applied to PPP's absolute-metres check (`|v| > maxinno`) caused a severe accuracy
  regression (tokyo_run1 MADOCA RMS: 1.8 m → 199 m).  Undifferenced PPP residuals can
  legitimately exceed tens of metres at initialisation; inflating the threshold by 10×
  admitted extreme outliers.  The change was reverted; PPP retains the original
  fixed-threshold check.

### CLAS benchmark summary (6-run PPC-Dataset, FIX tier, `--skip-epochs 60`)

| Run | Fix% (v0.3.3) | Fix% (v0.4.2) | RMS_2D fix (v0.3.3) | RMS_2D fix (v0.4.2) | 1σ (v0.4.2) |
|-----|:---:|:---:|:---:|:---:|:---:|
| nagoya_run1 | 17.0% | 17.0% | 1.105 m | 1.105 m | 0.402 m |
| nagoya_run2 | 26.9% | 23.4% | 1.088 m | 1.119 m | **0.461 m** |
| nagoya_run3 |  6.3% |  6.3% | 0.318 m | 0.318 m | 0.339 m |
| tokyo_run1  |  5.2% |  4.9% | 0.868 m | **0.747 m** | 0.244 m |
| tokyo_run2  | 21.7% | 21.7% | 0.590 m | **0.514 m** | 0.120 m |
| tokyo_run3  |  7.4% |  7.4% | 0.801 m | 0.801 m | 0.075 m |

2/6 Tokyo runs: FIX RMS_2D improved (−13–14%).
MADOCA results unchanged across all 6 runs.

### Test Results

All 57 tests pass (unchanged from v0.4.1).

---

## [v0.4.1] - 2026-03-07

**RTK accuracy release** — Ports the [demo5](https://github.com/rtklibexplorer/RTKLIB)
kinematic RTK algorithm improvements into MRTKLIB and resolves a false-fix persistence
defect that caused 1σ accuracy to degrade to 1.4 m in urban-canyon scenarios.

### Added

- **`pos2-arminfixsats`** — Minimum satellites required for AR (`nb >= minfixsats-1`
  DD pairs); guards against rank-deficient LAMBDA decompositions (library default: 0 = disabled;
  benchmark conf: 4).
- **`pos2-arfilter`** — AR candidate filter: newly-locked satellites that degrade
  the ambiguity ratio are temporarily excluded and re-introduced epoch by epoch
  (library default: off; benchmark conf: on).
- **`pos2-thresdop`** — Doppler-based cycle-slip threshold in cycles/s; 0 = disabled
  (library default: 0; benchmark conf: 1.0).
- **`stats-eratio3`** — Phase/code variance ratio for L5 frequency, completing
  independent eratio configuration for triple-frequency processing.
- **Partial AR (`manage_amb_LAMBDA`)** — Multi-attempt LAMBDA with PAR satellite
  exclusion loop and polynomial ratio threshold scaled to number of satellite pairs.
- **`detslp_code()`** — Observation-code-change-based cycle-slip detector for
  receiver signal-tracking transitions.
- **`detslp_dop()`** restored — Doppler-based slip detection with clock-jump
  mean-removal fix that prevented false triggers.
- **RTK benchmark results** — Full six-run PPC-Dataset results (Phase 4A–4F) added
  to `docs/benchmark.md` with Fix%, RMS_2D, 1σ, 95%, and TTFF.

### Changed

- **`varerr()`** — Rewritten with per-constellation elevation factors (GAL/QZS/CMP/IRN),
  SNR-weighted noise term, and correct IFLC variance scaling.
- **Outlier rejection threshold** — Adaptive 10× inflation for first-epoch or
  recently-reset biases; added `rejc<2` guard to prevent premature bias reset.
- **`seliflc()`** — Carrier-smoothed pseudo-range selection included in observation
  pre-processing.
- **Acceleration coupling gate** — State-transition acceleration term (`F[pos,acc]`)
  is disabled when position variance exceeds `thresar[1]`, preventing premature
  dynamics coupling before the filter has converged.
- **Half-cycle variance inflation** — Adds +0.01 m² to phase measurement variance
  when the LLI half-cycle ambiguity flag is set.
- **`seph2clk()` parenthesis fix** — Corrected iteration guard for SBAS ephemeris
  clock computation.
- **GF slip detector** — Added `thresslip==0` early-out guard; replaced early
  `return` with `continue` to prevent skipping subsequent satellite pairs.

### Fixed

- **False-fix persistence in urban canyons** (Phase 4F) — `holdamb()` constrains
  the Kalman ambiguity covariance to VAR_HOLDAMB = 0.001 cy², making wrong integer
  solutions effectively permanent until a cycle-slip or satellite-loss event.  A
  Phase 4D change had made the lock counter conditional (`lock++` only when
  `nfix>0 && fix[f]≥2`), which froze lock counts during `nfix=0` epochs and
  forced re-selection of the same wrong integers immediately after every false-fix
  break.  Reverting to unconditional `lock++` allows the eligible satellite set to
  diversify between fix attempts, enabling escape from the false-fix cycle.
  **Effect**: nagoya_run3 1σ restored from 1.373 m → 0.135 m (baseline: 0.128 m).

- **GLONASS clock rejection** — Ephemeris entries with `|taun| > 1 s` are now
  discarded; corrupted GLO clock data was propagating into positioning.

- **GLONASS health check** — Switched from generic `svh!=0` to ICD-specific bit
  mask `(svh&9)!=0 || (svh&6)==4`; GPS/Galileo/QZSS retain the `svh!=0` check.

- **vsat set by phase DD** (Phase 4E revert) — A demo5 change that set `vsat=1`
  only from phase DD residuals caused AR bootstrap deadlock in urban canyons where
  satellites have valid code DD but noisy phase.  Reverted to code-DD-based
  `vsat` (original behaviour).

- **Conditional lock increment** (Phase 4E revert) — A demo5 change that gated
  `lock++` on `nfix>0` prevented bootstrap from `nfix=0` at session start, blocking
  AR entirely until the first integer fix was achieved.

### RTK benchmark summary (6-run PPC-Dataset, FIX tier)

All results use city conf (`nagoya.conf` / `tokyo.conf`) for precise base-station
coordinates.  `--skip-epochs 60`.

| Run | Fix% (v0.3.3) | Fix% (v0.4.1) | 1σ (v0.3.3) | 1σ (v0.4.1) | TTFF (v0.4.1) |
|-----|:---:|:---:|:---:|:---:|---:|
| nagoya_run1 | 29.7% | 27.4% | 0.112 m | 0.112 m | 797 s |
| nagoya_run2 | 16.1% | **28.3%** | 0.154 m | 0.175 m | 0 s |
| nagoya_run3 |  8.2% | **10.1%** | 0.128 m | **0.135 m** | 74 s |
| tokyo_run1  |  3.4% |  2.9% | 0.027 m | **0.026 m** | 1841 s |
| tokyo_run2  | 18.3% | **22.1%** | 0.010 m | 0.020 m | 803 s |
| tokyo_run3  | 25.1% | **27.7%** | 0.013 m | 0.013 m | 658 s |

4/6 runs show improved fix rate vs v0.3.3; all runs maintain Phase-3-level 1σ accuracy.

### Test Results

All 57 tests pass (unchanged from v0.3.3):

| Test Suite | Tests |
|------------|-------|
| Unit tests | 12 |
| SPP / receiver bias / rtkrcv | 5 |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 |
| CLAS PPP-RTK + VRS-RTK | 19 |
| ssr2obs / ssr2osr / BINEX | 4 |
| Tier 2 absolute accuracy | 2 |
| Tier 3 position scatter | 2 |
| Fixtures | 3 |

---

## [v0.3.3] - 2026-03-07

Minor release — kinematic positioning benchmark for urban driving evaluation.
No functional changes to the library.

### Added

- **Kinematic benchmark infrastructure** (`scripts/benchmark/`) — End-to-end
  pipeline evaluating CLAS PPP-RTK, MADOCA PPP, and kinematic RTK against the
  PPC-Dataset urban driving data:
  - `cases.py` — Metadata for 6 PPC-Dataset runs (GPS week/TOW, city/run IDs)
  - `download_l6.py` — Auto-download QZSS L6D (CLAS) and L6E (MADOCA) archive
    files; MADOCA PRN auto-probed from candidates `[209, 193, 194, 195, 196, 199]`
  - `compare_ppc.py` — NMEA vs `reference.csv` comparison; three-tier accuracy
    breakdown (FIX/FF/ALL for CLAS & RTK; PPP for MADOCA); computes 2D/3D RMS,
    1σ, 95%, TTFF, mean satellite count; optional PNG plots
  - `run_benchmark.py` — Orchestrator with result caching, layered `-k` conf
    support, and summary table; `--mode clas|madoca|rtk|both|all`
    (`both`=clas+madoca, `all`=clas+madoca+rtk, default `all`)
- **Benchmark configurations** (`conf/benchmark/`):
  - `clas.conf` — CLAS PPP-RTK: `ant1-anttype=*`, `pos2-isb=off`, NMEA output
  - `madoca.conf` — MADOCA PPP: `pos1-dynamics=on`, `ant1-postype=single`, NMEA
  - `rtk.conf` — Kinematic RTK: `pos1-frequency=l1+2+3`, `pos1-ionoopt=off`,
    `pos1-snrmask_r=on` (enables genuine triple-frequency AR)
  - `nagoya.conf` — City overrides: precise base LLH, antenna types, `ant2-antdelu`
  - `tokyo.conf` — City overrides: precise base LLH, antenna types, `ant2-antdelu`
- **Benchmark documentation** ([docs/benchmark.md](docs/benchmark.md)) —
  Dataset download instructions, L6 auto-download, three-mode execution walkthrough,
  three-tier metric definitions, result tables, and known limitations.

### Changed

- `.gitignore` — Added `data/benchmark/` exclusion (large L6/NMEA files).
- `ruff.toml` — Added `scripts/benchmark/*.py` to `D103` per-file-ignores.
- **PPC-Dataset attribution** — Corrected to credit the contest organiser:
  Precise Positioning Challenge 2024 (高精度測位チャレンジ2024) by the
  Institute of Navigation Japan (測位航法学会); data published by Prof. Taro
  Suzuki (Chiba Institute of Technology).
- **Benchmark disclaimer** — Added note that parameters are not tuned and
  results are for reference only.

### Fixed

- **`rnx2rtkp` multi-`-k` conf loading** — `resetsysopts()` was called inside
  the `-k` processing loop, resetting values already loaded by earlier conf files.
  Moved to before the loop so layered city overrides (`nagoya.conf`, `tokyo.conf`)
  take effect correctly.
- **`loadopts()` leading whitespace** — Values after the `=` separator now have
  leading whitespace stripped, preventing key lookup failures for entries like
  `ant1-anttype       = *`.
- **MADOCA `misc-timeinterp`** — Was inadvertently `off` in the benchmark conf;
  restored to `on`, matching upstream MADOCALIB behaviour.

### Dataset

The benchmark uses the **PPC-Dataset** from the Precise Positioning Challenge 2024
(高精度測位チャレンジ2024), organised by the Institute of Navigation Japan
(測位航法学会). Data published by Prof. Taro Suzuki (Chiba Institute of Technology):
<https://github.com/taroz/PPC-Dataset>

Six urban vehicle runs (Nagoya × 3, Tokyo × 3) with 5 Hz triple-frequency
multi-GNSS, 100 Hz IMU, and sub-centimetre Applanix POS LV 220 ground truth.

---

## [v0.3.2] - 2026-03-06

Patch release — three-tier test methodology with absolute accuracy and position
scatter checks. No functional changes to the library.

### Added

- **Three-tier test methodology** ([docs/test-accuracy-methodology.md](docs/test-accuracy-methodology.md))
  — Formalises Tier 1 (relative porting correctness), Tier 2 (absolute geodetic
  accuracy vs SINEX / GSI F5), and Tier 3 (position scatter / precision) layers.
- **Tier 2 absolute accuracy tests** (2 new CTest entries):
  - `madocalib_pppar_abs_check` — MADOCA PPP-AR vs IGS SINEX MIZU (GPS week 2383), 2D horizontal ≤ 0.100 m
  - `claslib_ppp_rtk_2ch_abs_check` — CLAS 2CH vs GSI F5 TSUKUBA3 2025/06/06, 2D horizontal ≤ 0.300 m
- **Tier 3 position scatter tests** (2 new CTest entries):
  - `madocalib_ppp_scatter` — MADOCA-PPP 2D scatter ≤ 0.150 m (1σ=7.4 cm, 95%=11.2 cm, skip=30)
  - `claslib_ppp_rtk_2ch_scatter` — CLAS 2CH scatter ≤ 0.400 m (1σ=5.8 cm, 95%=25.2 cm, skip=20)
- **New comparison scripts**: `compare_pos_abs.py`, `compare_nmea_abs.py` (Tier 2);
  `check_pos_scatter.py` (Tier 3).
- **`scripts/tests/_geo.py`** — Shared geodetic utilities (`blh2xyz`, `xyz2blh`,
  `xyz2enu`, `nmea_to_deg`) replacing 4× inline duplication across test scripts.
- **`ruff.toml`** at project root — Ruff linting for all Python scripts
  (`E`, `F`, `W`, `I`, `D` rules, Google docstring convention).
- **`.vscode/settings.json`** — Ruff format-on-save and `scripts/.venv` interpreter.

### Changed

- `scripts/pyproject.toml` — Reduced to package metadata only; ruff config moved
  to root `ruff.toml`.
- Three debug plot scripts moved from `scripts/tests/` to `scripts/plotting/`
  (`plot_gloeph.py`, `plot_igp.py`, `plot_peph.py`).

### Removed

- `scripts/fix_trace_fwd_decls.py`, `scripts/migrate_trace_ctx.py` — One-time
  migration tools no longer needed.
- `scripts/plotting/plot_lex_{eph,ion,ure}.py` — LEX service retired 2020.

### Fixed

- **NMEA ellipsoidal height** — `compare_nmea_abs.py` now correctly computes
  `h_ell = MSL + geoid_separation` from GGA fields [9] and [11]; previously
  field[11] was not summed, causing ~40 m Up errors.
- **Absolute check 2D mode** — `--use-2d` flag in `compare_pos_abs.py` now
  consistently governs both the reported metric and the pass/fail criterion.

### Test Results

All 57 tests pass (53 carried from v0.3.1 + 4 new):

| Test Suite | Tests |
|------------|-------|
| Unit tests | 12 |
| SPP / receiver bias / rtkrcv | 5 |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 |
| CLAS PPP-RTK + VRS-RTK | 19 |
| ssr2obs / ssr2osr / BINEX | 4 |
| Tier 2 absolute accuracy | 2 ← new |
| Tier 3 position scatter | 2 ← new |
| Fixtures | 3 |

---

## [v0.3.1] - 2026-03-06

Patch release — MADOCALIB PPP-AR regression test quality improvement.
No functional changes.

### Changed

- **MADOCALIB PPP-AR reference data** — Replaced `pppar` and `pppar_ion`
  reference `.pos` files with outputs from upstream MADOCALIB built with
  `-DLAPACK` (Accelerate framework). The previous LU-solver reference caused
  artificial 1.5–3.8 cm offsets unrelated to porting correctness.
- **Test tolerances tightened** (when `LAPACK_FOUND` is true):
  - `madocalib_pppar_check`: 0.020 m → **0.008 m**
  - `madocalib_pppar_ion_check`: 0.040 m → **0.005 m**
  - Fallback to original thresholds when LAPACK is unavailable (internal LU vs LAPACK reference diverges ~1.5–3.8 cm)

### Background

Investigation confirmed that upstream MADOCALIB already supports LAPACK via
`#ifdef LAPACK` in `rtkcmn.c`. Building upstream with `-DLAPACK -framework
Accelerate` reveals the true implementation difference: MRTKLIB vs upstream
LAPACK is **0.41 cm** (pppar) and **0.25 cm** (pppar_ion), confirming the
porting is correct. The 1.5–3.8 cm difference seen in v0.3.0 was entirely
attributable to LU vs LAPACK numerical divergence within the upstream itself.

### Test Results

All 53 tests pass. Tolerances when built with LAPACK (tight) vs without (wide):

| Test | 3D RMS | Tolerance (LAPACK) | Tolerance (no LAPACK) |
|------|--------|-------------------|----------------------|
| madocalib_pppar | 0.41 cm | 0.008 m (~50% margin) | 0.020 m |
| madocalib_pppar_ion | 0.25 cm | 0.005 m (~50% margin) | 0.040 m |

---

## [v0.3.0] - 2026-03-06

CLASLIB integration — adds QZSS CLAS L6D augmentation (PPP-RTK and VRS-RTK)
via [CLASLIB](https://github.com/QZSS-Strategy-Office/claslib) ver.0.8.2 (`9e714b9`).
Includes the complete CSSR decoder, grid correction engine, PPP-RTK positioning,
VRS double-differenced RTK, dual-channel L6 support, and SSR→OSR utilities.

### Added

- **CLAS CSSR decoder** (`mrtk_clas.c`) — Full ST1–ST12 Compact SSR message
  decoder supporting IS-QZSS-L6-007 two-channel L6 input.
- **CLAS grid correction engine** (`mrtk_clas_grid.c`) — Tropospheric and STEC
  grid interpolation derived from GSILIB v1.0.3 algorithms.
- **PPP-RTK positioning engine** (`mrtk_ppp_rtk.c`) — CLAS centimeter-level
  augmentation using double-differenced carrier-phase OSR corrections.
  Accuracy: **5.9 cm 3D RMS, 99.86% fix** (single-channel, DOY 239/2019).
- **VRS DD-RTK positioning engine** (`mrtk_vrs.c`, ~2050 lines) — Virtual
  Reference Station double-differenced RTK using CLAS OSR corrections.
  Accuracy: **3.3 cm 3D RMS, 99.86% fix** (DOY 239/2019).
- **Multi-L6E dual-channel support** — `nav->ssr_ch[SSR_CH_NUM][MAXSAT]` and
  `_mcssr[SSR_CH_NUM]` decoder array support two independent L6 channels
  (ST12 mode, IS-QZSS-L6-007). ST12 accuracy: **10.8 cm 3D RMS, 98.75% fix**.
- **BINEX file reading** — `readbnxt()` enables post-processing from Septentrio
  BINEX raw observation files.
- **Per-system frequency selection** (`posopt[10–12]`) — Independent L1/L2/L5
  signal selection for GPS, GLONASS, Galileo, and QZSS.
- **`ssr2obs` utility** — Standalone tool converting CLAS L6 corrections to
  RINEX3 VRS pseudo-observations; also outputs OSR CSV and RTCM3 MSM4 format.
- **`ssr2osr` utility** — Post-processing OSR verification tool (SSR→OSR via
  `postpos()` with `PMODE_SSR2OSR`).
- **`dumpcssr` utility** — CSSR message dump tool for L6 stream inspection.
- **`clas_ssr2osr()` wrapper** — Public API for SSR→OSR conversion callable
  from within `rtkpos()`.
- **23 new regression tests** — PPP-RTK (10), VRS-RTK (9), ssr2obs/ssr2osr (3),
  BINEX (1); total test count raised from 30 to 53.
- **NMEA comparison infrastructure** (`scripts/tests/compare_nmea.py`) —
  GGA-based accuracy comparison for VRS-RTK regression testing.

### Changed

- **`rtkpos()` dispatch** — Added `PMODE_PPP_RTK`, `PMODE_VRS_RTK`, and
  `PMODE_SSR2OSR` branches; CLAS context initialised in `postpos()`.
- **`prcopt_t`** — Extended `err[5]→err[8]` (CLAS measurement noise model) and
  `posopt[6]→posopt[13]` (per-system frequency and PPP-RTK knobs).
- **`sat_lambda()` in PPP-RTK** — Switched from obsdef table lookup to direct
  obs-code→frequency mapping, fixing QZS L2 wavelength (0 → correct value)
  and raising fix rate from 97% → 99.86%.
- **`clas_osr_zdres()`** — When `y==NULL` (VRS mode), writes OSR directly to
  the caller's buffer without an intermediate `osrtmp` redirect.
- **`tidedisp()` call** — Corrected bitmask routing (1=solid, 2=OTL, 4=pole)
  for grid OTL displacement.
- **Unified copyright headers** — All source files (lib + apps, ~112 files)
  updated with full 9-party contributor lineage, SPDX identifier, and
  Doxygen `@file` blocks separated from license comments.
- **`LICENSE.txt`** — Added Mitsubishi Electric Corp. (2015–) and Geospatial
  Information Authority of Japan (2014–) copyright lines.
- **`README.md`** — CLASLIB marked Integrated (was Planned); License &
  Attributions table added.

### Fixed

- **`set_ssr_ch_idx()` / `set_mcssr_ch()`** — Added `[0, SSR_CH_NUM)` bounds
  guard to prevent out-of-bounds access on `nav->ssr_ch` and `_mcssr` arrays.
- **`memset` sizeof targets** — `biaosb->spb` and `biaosb->vspb` now use
  `sizeof` of their own member (previously used sibling member's size).
- **SIS/IODE boundary detection** — `clas_osr_satcorr_update()` uses
  `prev_orb_tow` to detect orbit-epoch advancement; removed incorrect
  `orb_tow == clk_tow` requirement that broke ST12 (orbit always 5 s ahead
  of clock in ST12 streams).
- **`clas_mode` gate in `postpos()`** — `PMODE_SSR2OSR` included in CLAS
  context initialisation check.
- **`compensatedisp()` SSR crash** — Fixed null pointer dereference when SSR
  corrections are not yet available.

### Known Limitations

- **Real-time CLAS**: `rtkrcv` supports a single CLAS stream (channel 0).
  Real-time dual-L6 channel switching is not yet implemented.
- **Real-time L6D**: Ionospheric STEC correction (L6D) is not available in
  real-time mode. PPP-AR+iono requires post-processing (`rnx2rtkp`).
- **Static mode**: PPP-RTK in static receiver mode is not yet validated
  (test data unavailable).

### Test Results

All 53 tests pass:

| Test Suite | Tests | Key Accuracy |
|------------|-------|-------------|
| Unit tests | 12 | — |
| SPP regression | 2 | — |
| Receiver bias | 2 | — |
| rtkrcv real-time | 1 | — |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 | <0.5 cm / ~1.6 cm / ~3.8 cm 3D RMS |
| CLAS PPP-RTK (single-ch / dual-ch / ST12 / L1CL5) | 10 | 5.9 cm / — / 10.8 cm / — |
| CLAS VRS-RTK (single-ch / ST12 / dual-ch) | 9 | 3.3 cm / — / — |
| ssr2obs / ssr2osr | 3 | — |
| BINEX reading | 1 | — |
| Fixtures (setup/cleanup/download) | 3 | — |

### Upstream References

- **CLASLIB**: [claslib](https://github.com/QZSS-Strategy-Office/claslib) ver.0.8.2 (`9e714b9`)

---

## [v0.2.0] - 2026-03-02

MADOCALIB PPP engine integration — replaces the MALIB PPP engine with
[MADOCALIB](https://github.com/QZSS-Strategy-Office/madocalib) ver.2.0 (`8091004`)
for higher-accuracy PPP/PPP-AR processing with L6E and L6D correction support.

### Added

- **MADOCALIB PPP/PPP-AR engine** — Full integration of the MADOCALIB positioning
  algorithms (PPP, PPP-AR, PPP-AR+iono) from upstream ver.2.0, replacing the
  previous MALIB PPP stub.
- **L6E SSR decoder** (`mrtk_madoca.c`) — QZSS L6E orbit/clock/bias correction
  stream decoder (MCSSR format).
- **L6D ionospheric decoder** (`mrtk_madoca_iono.c`) — QZSS L6D wide-area STEC
  ionospheric correction decoder for PPP-AR+iono post-processing.
- **Real-time PPP** — `rtkrcv` now runs MADOCALIB PPP engine with L6E corrections
  via SBF file stream replay; verified with `rtkrcv_rt` regression test.
- **MADOCALIB regression tests** (8 tests) — PPP, PPP-AR (mdc-004/mdc-003),
  PPP-AR+iono accuracy checks against upstream reference data (MIZU station).
- **Python-based `.pos` comparison tool** (`tests/cmake/compare_pos.py`) —
  Configurable tolerance comparison for regression testing.
- **Unified copyright headers** — All 104 source files updated with full
  7-party contributor lineage and SPDX-License-Identifier.

### Changed

- **PPP engine** — `pppos()` in `mrtk_ppp.c` is now the MADOCALIB implementation
  (previously MALIB). Includes PPP-AR (`mrtk_ppp_ar.c`) and ionospheric
  correction (`mrtk_ppp_iono.c`) modules.
- **Signal selection** — Unified to obsdef tables aligned with upstream MADOCALIB
  (`code2idx()` frequency index mapping).
- **SPP ISB model** — Aligned with upstream MADOCALIB observation selection.
- **BLAS guard** — Added zero-dimension guard in `matmul()` to prevent BLAS
  `dgemm` LDC=0 errors when filter state count is zero.
- **`prev_qr[]` state management** — Zero-initialized in `rtkinit()` and updated
  after `rtkpos()` in both `procpos()` and `rtksvrthread()` to ensure correct
  `const_iono_corr()` convergence detection.
- **File naming** — Renamed for consistency with `mrtk_madoca_*` convention:
  - `mrtk_rtcm3lcl.c` → `mrtk_rtcm3_local_corr.c`
  - `mrtk_mdciono.c` → `mrtk_madoca_iono.c`
- **CI workflow** — Updated accuracy analysis to use MADOCALIB PPP output
  (`out_madocalib_ppp.pos`) with MIZU station reference coordinates.
- **`prn[6]` documentation** — Corrected field comment from `dcb` to `ifb`
  (Inter-Frequency Bias); `stats-prndcb` retained as legacy alias.
- **`prcopt_t` cleanup** — Removed unused `ppp_engine` field and related
  constants/configuration (`pos1-ppp-engine`).

### Removed

- **`mrtk_ppp_madoca.c`** — Dead stub file (SOLQ_NONE output only); the actual
  MADOCALIB engine is integrated directly into `mrtk_ppp.c`.
- **`MRTK_PPP_ENGINE_*` constants** — Engine selection mechanism removed as
  MADOCALIB is now the sole PPP engine.

### Known Limitations

- **Real-time L6D**: Ionospheric STEC correction input is not available in
  real-time mode (`rtkrcv`). PPP-AR+iono requires post-processing (`rnx2rtkp`).
- **Single correction stream**: `rtkrcv` supports one correction input
  (`inpstr3`). Multiple L6E channels require receiver-side multiplexing.
- **LAPACK numerical difference**: MRTKLIB uses system LAPACK while upstream
  MADOCALIB uses an embedded LU solver, causing ~1.5–3.8 cm 3D RMS differences
  in PPP-AR solutions. Test tolerances are adjusted accordingly.

### Test Results

All 30 tests pass (12 unit + 18 regression):

| Test Suite | Tests | Status |
|------------|-------|--------|
| Unit tests | 12 | PASS |
| SPP regression | 2 | PASS |
| Receiver bias | 2 | PASS |
| rtkrcv real-time | 1 | PASS |
| MADOCA PPP | 2 | PASS |
| MADOCA PPP-AR (mdc-004) | 2 | PASS |
| MADOCA PPP-AR (mdc-003) | 2 | PASS |
| MADOCA PPP-AR+iono | 2 | PASS |
| Fixtures (setup/cleanup) | 5 | PASS |

### Upstream References

- **MALIB**: [JAXA-SNU/MALIB](https://github.com/JAXA-SNU/MALIB) feature/1.2.0 (`f006a34`)
- **MADOCALIB**: [madocalib](https://github.com/QZSS-Strategy-Office/madocalib) ver.2.0 (`8091004`)

## [v0.1.0] - 2026-02-23

Initial release — MALIB structural migration complete.

### Added

- **MRTKLIB core architecture** — Thread-safe `mrtk_ctx_t` context, POSIX/C11
  pure implementation, CMake build system.
- **Domain-driven directory structure** — `src/core/`, `src/pos/`, `src/data/`,
  `src/rtcm/`, `src/models/`, `src/stream/`, `src/madoca/`.
- **Applications** — `rnx2rtkp` (post-processing), `rtkrcv` (real-time),
  `recvbias` (receiver bias estimation).
- **Regression test framework** — CTest-based SPP, receiver bias, and real-time
  PPP tests with reference data comparison.
- **MALIB integration** — Structural base from JAXA MALIB feature/1.2.0
  (directory layout, threading, stream I/O).

[v0.4.2]: https://github.com/h-shiono/MRTKLIB/compare/v0.4.1...v0.4.2
[v0.4.1]: https://github.com/h-shiono/MRTKLIB/compare/v0.3.3...v0.4.1
[v0.3.3]: https://github.com/h-shiono/MRTKLIB/compare/v0.3.2...v0.3.3
[v0.3.2]: https://github.com/h-shiono/MRTKLIB/compare/v0.3.1...v0.3.2
[v0.3.1]: https://github.com/h-shiono/MRTKLIB/compare/v0.3.0...v0.3.1
[v0.3.0]: https://github.com/h-shiono/MRTKLIB/compare/v0.2.0...v0.3.0
[v0.2.0]: https://github.com/h-shiono/MRTKLIB/compare/v0.1.0...v0.2.0
[v0.1.0]: https://github.com/h-shiono/MRTKLIB/releases/tag/v0.1.0
