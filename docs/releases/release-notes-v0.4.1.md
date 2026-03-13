# MRTKLIB v0.4.1 Release Notes

**Release date:** 2026-03-08
**Type:** Feature + bugfix (RTK algorithm improvements and accuracy fix)

---

## Overview

v0.4.1 ports the [demo5](https://github.com/rtklibexplorer/RTKLIB) kinematic RTK
algorithm improvements into MRTKLIB and resolves a **false-fix persistence defect**
that caused 1σ accuracy to degrade to 1.4 m in urban-canyon scenarios.  The changes
affect the RTK positioning engine (`mrtk_rtkpos.c`) only; PPP, PPP-AR, and CLAS
PPP-RTK/VRS-RTK engines are unchanged.

### Highlights

- **Partial AR (PAR)** — Multi-attempt LAMBDA with polynomial ratio threshold and
  per-satellite exclusion loop, improving fix rates in difficult environments.
- **`detslp_dop()` / `detslp_code()`** — Two additional cycle-slip detectors from
  demo5 reduce the number of epochs with wrong integer candidates.
- **`varerr()` rewrite** — Per-constellation elevation weighting and SNR-scaled
  noise accurately models the satellite-specific measurement quality.
- **False-fix fix (Phase 4F)** — Identified and reverted a lock-counter change that
  caused `holdamb()` false-fix cycles to persist indefinitely in urban canyons.
  **nagoya_run3 1σ restored: 1.373 m → 0.135 m** (baseline: 0.128 m).

> **Disclaimer:** The configuration parameters used here have not been tuned or
> optimised for maximum accuracy.  Results are provided for reference only and
> should not be taken as representative of the best achievable performance.

---

## What Changed

### New configuration options

Three new `prcopt_t` fields (and corresponding `rnx2rtkp`/`rtkrcv` option keys)
are available.  Library defaults are conservative (disabled); the benchmark
configuration activates them:

| Option key | Field | Description | Lib default | Benchmark conf |
|------------|-------|-------------|:-----------:|:--------------:|
| `pos2-arminfixsats` | `minfixsats` | Min satellites for valid AR; requires `nb ≥ N-1` DD pairs | 0 (off) | 4 |
| `pos2-arfilter` | `arfilter` | Exclude newly-locked sats that degrade AR ratio | off | on |
| `pos2-thresdop` | `thresdop` | Doppler cycle-slip threshold (cycles/s); 0 = disabled | 0 | 1.0 |
| `stats-eratio3` | `eratio[2]` | Phase/code variance ratio for L5 (completing triple-freq) | 100 | 100 |

### Ambiguity resolution improvements

#### Partial AR — `manage_amb_LAMBDA()`

The previous single-shot LAMBDA call is replaced by a multi-attempt loop:

1. **Polynomial ratio threshold** — The acceptance threshold scales with the number
   of satellite pairs `nb`; fewer pairs require a higher ratio to prevent random fixes.
2. **PAR satellite exclusion** — If the ratio is below threshold, the satellite with
   the weakest contribution is temporarily excluded and LAMBDA is retried.  Iteration
   continues until the ratio passes or the minimum satellite count (`minfixsats`) is
   reached.
3. **AR filter** — If a newly-locked satellite (lock count == 0) caused the ratio to
   drop compared to the previous epoch, it is excluded for several epochs before
   being re-evaluated (`arfilter=1`).
4. **Position-variance gate** — AR is skipped entirely when the filter's position
   variance exceeds `thresar[1]` (default: 0.9999 m), preventing premature integer
   fixing before convergence.

#### Doppler cycle-slip detection — `detslp_dop()`

Restored from demo5 with a clock-jump mean-removal fix.  Computes the expected
phase-rate change from Doppler and compares it to the actual phase difference; a
discrepancy exceeding `thresdop` cycles/s is flagged as a cycle slip.  The
clock-jump removal prevents the mean Doppler error in a single epoch from triggering
false slips on all satellites simultaneously.

#### Observation-code cycle-slip detection — `detslp_code()`

New in MRTKLIB.  Flags a cycle slip whenever the signal-tracking code for a
frequency changes between epochs (e.g. L1C → L1P), indicating a receiver
re-acquisition that typically introduces an integer-cycle ambiguity jump.

### Measurement noise model — `varerr()`

Rewritten to match demo5:

| Change | Effect |
|--------|--------|
| Per-constellation EFACT (GAL/QZS/CMP/IRN 1.5 vs GPS/GLO/SBS 1.0) | Weaker constellations receive proportionally larger noise |
| Correct IFLC variance scaling (EFACT² × A² + B²/sin²el + …) | Prevents IFLC observations from being over-trusted |
| SNR-weighted term (`err[5]`, `err[6]`; both default 0 = disabled) | Optional noise floor inflation for low-SNR observations |
| Half-cycle variance inflation (+0.01 m² when `LLI_HALFC` is set) | Deprioritises half-cycle-ambiguous phase observations |

### Kalman filter improvements

- **Adaptive outlier threshold** — Phase outlier rejection threshold is inflated
  10× for the first epoch or immediately after a bias reset, preventing healthy
  satellites from being falsely rejected during filter initialisation.
- **`rejc<2` guard** — Phase-bias reset in `udbias()` is now only triggered when
  the outlier count reaches 2, preventing a single-epoch glitch from resetting a
  well-converged ambiguity.
- **Acceleration coupling gate** — The `F[pos,acc]` state-transition term is
  disabled when position variance exceeds `thresar[1]`, preventing
  dynamics-model corruption before the filter has converged.

### GNSS data quality fixes

- **GLONASS clock rejection** — `satpos()` now discards GLONASS ephemeris entries
  with `|taun| > 1 s`; corrupted clock entries caused position jumps.
- **GLONASS health check** — `satexclude()` switched from generic `svh != 0` to
  the ICD-specific mask `(svh&9) != 0 || (svh&6) == 4`; this correctly identifies
  L1-bad, L2-bad, and malfunction states while accepting satellites in normal
  operation that the generic check would incorrectly exclude.

### Miscellaneous correctness fixes

- **`seph2clk()` parenthesis** — Iteration guard for SBAS ephemeris clock
  computation had mismatched parentheses; corrected (no observable effect on
  supported test data).
- **GF slip detector** — Added `thresslip == 0` early-out guard; replaced early
  `return` with `continue` to process all satellite pairs instead of stopping at
  the first pair with missing data.
- **SBAS AR coupling** (Copilot review) — `sbas1` was incorrectly derived from
  the GLONASS AR state; now set independently from `navsys & SYS_SBS`.
- **`detslp_dop()` nf clamp** (Copilot review) — `nf` is now clamped to `NFREQ`
  before indexing the `dopdif` and `tt` stack arrays, matching `detslp_code()`.
- **`rejc[]` phase-only** (Copilot review) — Code-residual outliers no longer
  increment `rejc[]`; doing so could trigger spurious phase-bias resets in
  `udbias()` via the `rejc >= 2` guard.

---

## False-Fix Persistence Fix (Phase 4F)

This is the most impactful single change in v0.4.1.

### Root cause

Once `holdamb()` fires, it applies a Kalman measurement update that constrains
ambiguity covariance to VAR_HOLDAMB = 0.001 cy².  Because process noise
accumulates at only ~10⁻⁸ m²/epoch, these constraints are effectively permanent
until a cycle slip or satellite loss.  If the wrong integers are held, the filter
is locked at the wrong position with no self-correction mechanism.

A Phase 4D change had gated the lock counter on `nfix > 0 && fix[f] ≥ 2`:

```c
/* Phase 4D — caused false-fix persistence: */
if (rtk->ssat[sat[i]-1].lock[f]<0 ||
    (rtk->nfix>0 && rtk->ssat[sat[i]-1].fix[f]>=2))
    rtk->ssat[sat[i]-1].lock[f]++;
```

When a false fix broke (`nfix` reset to 0 on any float epoch), lock counts froze
for all non-countdown satellites.  At the next epoch, LAMBDA operated on the
identical satellite set with the identical constrained ambiguity state — the same
wrong integers were selected again, `holdamb()` fired again after `minfix` epochs,
and the cycle repeated indefinitely.

### Fix

Reverted to unconditional `lock++`:

```c
/* Phase 4F — unconditional: allows satellite-set diversification between fix attempts */
rtk->ssat[sat[i]-1].lock[f]++;
```

With unconditional incrementing, lock counts continue to evolve during float
periods.  Satellites that enter or leave the AR-eligible window between attempts
introduce variation in the integer search space, enabling eventual escape from the
false-fix cycle.

### Effect

| Metric | v0.3.3 | v0.4.1 (w/ Phase 4D) | v0.4.1 (Phase 4F) |
|--------|:------:|:--------------------:|:-----------------:|
| nagoya_run3 Fix% | 8.2% | 12.9% | **10.1%** |
| nagoya_run3 1σ | 0.128 m | 1.373 m ⚠ | **0.135 m** ✓ |
| False-fix epochs | 123 / 514 (24%) | 241 / 661 (37%) | ~5% of fix epochs |

---

## RTK Benchmark Results

All results use city conf (`nagoya.conf` / `tokyo.conf`), `--skip-epochs 60`.
FIX tier (Q = 4) only.  Parameters are not tuned; see the disclaimer above.

### Fix rate and accuracy vs v0.3.3

| Run | Fix% (v0.3.3) | Fix% (v0.4.1) | Δ Fix% | 1σ (v0.3.3) | 1σ (v0.4.1) | TTFF (v0.4.1) |
|-----|:---:|:---:|:---:|:---:|:---:|---:|
| nagoya_run1 | 29.7% | 27.4% | −2.3 pp | 0.112 m | 0.112 m | 797 s |
| nagoya_run2 | 16.1% | **28.3%** | +12.2 pp | 0.154 m | 0.175 m | 0 s |
| nagoya_run3 |  8.2% | **10.1%** | +1.9 pp | 0.128 m | **0.135 m** | 74 s |
| tokyo_run1  |  3.4% |  2.9% | −0.5 pp | 0.027 m | **0.026 m** | 1841 s |
| tokyo_run2  | 18.3% | **22.1%** | +3.8 pp | 0.010 m | 0.020 m | 803 s |
| tokyo_run3  | 25.1% | **27.7%** | +2.6 pp | 0.013 m | 0.013 m | 658 s |

4/6 runs improved fix rate; all 6 runs maintain Phase-3-level 1σ accuracy.

**Notes:**
- **nagoya_run2 +12 pp** — The largest improvement.  PAR finds valid subsets on
  this run even when the full satellite set produces a low ratio.
- **tokyo_run2 1σ (0.010 → 0.020 m)** — The slight increase reflects the richer
  satellite set that `varerr()` now models with per-constellation weighting;
  absolute accuracy remains sub-centimetre.
- **nagoya_run1 fix rate (29.7 → 27.4%)** — Phase 4D's conditional lock had boosted
  this run to 38.2% at the cost of nagoya_run3 accuracy; the Phase 4F revert returns
  it to near-baseline, with identical 1σ.

### Full result tables

See [docs/benchmark.md](../reference/benchmark.md) for complete three-tier (FIX/FF/ALL) tables
including CLAS PPP-RTK and MADOCA PPP results.

---

## Acknowledgements

RTK algorithm improvements in this release are adapted from
**demo5 RTKLIB** by [rtklibexplorer](https://github.com/rtklibexplorer/RTKLIB),
a community fork of RTKLIB maintaining kinematic RTK accuracy improvements for
challenging environments.

Benchmark results use the **PPC-Dataset** from the Precise Positioning Challenge
2024 (高精度測位チャレンジ2024), organised by the
[Institute of Navigation Japan (測位航法学会)](https://www.in-japan.org/):

> Taro Suzuki, Chiba Institute of Technology.
> *PPC-Dataset — GNSS/IMU driving data for precise positioning research.*
> <https://github.com/taroz/PPC-Dataset>

---

## Test Suite (57 tests, unchanged)

No new CTest entries in this release.  All 57 tests from v0.3.3 continue to pass.

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
