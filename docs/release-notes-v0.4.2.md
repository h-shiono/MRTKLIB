# MRTKLIB v0.4.2 Release Notes

**Release date:** 2026-03-08
**Type:** Feature (demo5 algorithm improvements ported to PPP-RTK and PPP engines)
**Branch:** `feat/ppp-rtk-demo5-improvements`

---

## Overview

v0.4.2 extends the [demo5](https://github.com/rtklibexplorer/RTKLIB) kinematic accuracy
improvements from v0.4.1 (RTK engine) to the **CLAS PPP-RTK** (`mrtk_ppp_rtk.c`) and
**MADOCA PPP** (`mrtk_ppp.c`) engines.  The **RTK** and **VRS-RTK** engines are
unchanged.

Ported improvements cover four implementation phases:

| Phase | Scope | Summary |
|-------|-------|---------|
| A | PPP-RTK, PPP | GLONASS `ephpos()` corrupted-clock rejection |
| B | PPP-RTK | PAR position-variance gate + arfilter |
| C | PPP-RTK, PPP | Doppler + code-change cycle-slip detectors |
| D | PPP-RTK, PPP | Full per-constellation EFACT; adaptive outlier threshold (PPP-RTK only) |

### Highlights

- **GLONASS clock guard** ported to `ephpos()` — the `|taun| > 1 s` rejection that
  already guarded `ephclk()` (since v0.4.1) now also guards the satellite position
  computation path used by PPP-RTK and PPP.
- **`detslp_code()`** extended to PPP-RTK and PPP — signal-code changes between epochs
  (receiver re-acquisition events) are detected and used to reset phase-bias states
  before false integer candidates can accumulate.
- **`detslp_dop()`** extended to PPP-RTK and PPP — Doppler-based phase-rate slip
  detector with clock-jump mean removal, identical to the RTK implementation.
- **Full EFACT expansion** — `varerr()` in both PPP-RTK and PPP now handles all seven
  constellation EFACT factors (GAL/QZS/CMP/IRN) instead of just GPS/GLO/SBS.
- **Adaptive outlier threshold (PPP-RTK only)** — The `residual_test()` sigma-normalised
  threshold is inflated 10× for newly-initialised biases, matching the demo5 RTK
  behaviour.  An equivalent change to undifferenced PPP caused a severe regression
  (tokyo_run1 MADOCA RMS: 1.8 m → 199 m) and was explicitly reverted.

> **Disclaimer:** The configuration parameters used here have not been tuned or
> optimised for maximum accuracy.  Results are provided for reference only and
> should not be taken as representative of the best achievable performance.

---

## What Changed

### Phase A — GLONASS ephemeris rejection in `ephpos()`

`ephpos()` calls `selgeph()` followed by `geph2pos()` to compute the satellite position
and clock.  Previously, corrupted GLONASS clock data (`|taun| > 1 s`) was only caught
in `ephclk()`.  When SSR corrections are active (PPP-RTK and PPP), `satpos_ssr()` calls
`ephpos()` directly for the base clock; the `dts[0]` contribution from `geph2pos()`
could carry a multi-second `taun` error into the final clock.

**Fix**: added `if (fabs(geph->taun) > 1.0) return 0;` immediately after `selgeph()` in
`ephpos()`, consistent with the guard already present in `ephclk()`.

**Files changed**: `src/data/mrtk_eph.c`

### Phase B — PPP-RTK PAR enhancements

Two AR-quality guards from the RTK engine (v0.4.1) are now also active in the PPP-RTK
engine (`ppp_rtk_pos()`):

#### B2 — Position variance gate

Before invoking `resamb_LAMBDA()`, the mean diagonal of the 3 × 3 position covariance
block is compared to `thresar[1]`.  If `posvar > thresar[1]` the AR step is skipped
entirely (`nb = 0; goto ppprtk_ar_done`), preventing premature integer fixing before
the PPP-RTK filter has converged.

The gate is disabled unless `thresar[1] > 0.0` (library default: 0.9999; off = 0.0).

#### B3 — arfilter

When `resamb_LAMBDA()` returns `nb ≤ 1` (AR failed or insufficient satellite pairs), and
`opt->arfilter` is set, any satellite whose phase-bias lock count equals 0 (newly-locked)
is backed off by `-(minlock + delay)` and LAMBDA is retried.  The delay is staggered
(`+2` per satellite) to prevent all newly-locked satellites from becoming eligible
simultaneously on the next epoch.

**Files changed**: `src/pos/mrtk_ppp_rtk.c`

### Phase C — Cycle-slip detectors for PPP-RTK and PPP

#### `detslp_dop()` — Doppler-based slip detection

Added as a `static` function in both `mrtk_ppp_rtk.c` and `mrtk_ppp.c`.
Implementation is identical to the RTK version in `mrtk_rtkpos.c`:

1. For each observation, compute `dph = (L[f] − ph[0][f]) / tt` (phase rate from
   carrier phase) and `dpt = −D[f]` (Doppler-derived phase rate).
2. Accumulate the mean of `dph − dpt` for all satellites with `|diff| < 3 × thresdop`.
3. Flag any satellite whose deviation from the mean exceeds `thresdop` (cycles/s) as a
   cycle slip.

The mean-removal step is the clock-jump fix from demo5: a large receiver-clock jump
shifts every satellite's Doppler by the same amount, so subtracting the mean prevents
false slip flags on all satellites simultaneously.

Activated by `pos2-thresdop > 0.0` (library default: 0; disabled).

**`ph[0][f]` / `pt[0][f]` update for PPP**: The PPP engine did not previously store
phase observations in `ssat[].ph[]`/`ssat[].pt[]`.  A block was added to `update_stat()`
to record them after each epoch, enabling `detslp_dop()` to compare consecutive epochs.

#### `detslp_code()` — Observation-code-change slip detection

Added as a `static` function in both `mrtk_ppp_rtk.c` and `mrtk_ppp.c`.  Compares
`obs[i].code[f]` against the stored `ssat[sat-1].codeprev[f][0]`; any change between
non-`CODE_NONE` values sets the slip flag and logs the transition.

Call order in `udbias_ppp()`:

```
detslp_ll()    (LLI flag)
detslp_gf()    (geometry-free combination)
detslp_dop()   ← new
detslp_code()  ← new
```

**Files changed**: `src/pos/mrtk_ppp_rtk.c`, `src/pos/mrtk_ppp.c`

### Phase D — Measurement noise model improvements

#### D1 — Full per-constellation EFACT in `varerr()`

Both `mrtk_ppp_rtk.c` and `mrtk_ppp.c` previously used a three-way conditional for the
system error factor:

```c
/* Before */
fact *= sys == SYS_GLO ? EFACT_GLO : (sys == SYS_SBS ? EFACT_SBS : EFACT_GPS);
```

This silently applied `EFACT_GPS = 1.0` to Galileo, QZSS, BeiDou, and IRNSS.  The
corrected expansion uses all seven EFACT constants from `mrtk_const.h`:

```c
/* After */
fact *= sys==SYS_GLO?EFACT_GLO:(sys==SYS_GAL?EFACT_GAL:
        (sys==SYS_QZS?EFACT_QZS:(sys==SYS_CMP?EFACT_CMP:
        (sys==SYS_IRN?EFACT_IRN:(sys==SYS_SBS?EFACT_SBS:EFACT_GPS)))));
```

EFACT values (`mrtk_const.h`):
| Constellation | EFACT |
|---------------|------:|
| GPS | 1.0 |
| GLONASS | 1.5 |
| Galileo | 1.0 |
| QZSS | 1.0 |
| BeiDou | 1.0 |
| IRNSS | 1.5 |
| SBAS | 3.0 |

The only effective change is for IRNSS (1.0 → 1.5).  GPS, GLO, GAL, QZS, CMP, SBS are
unchanged by value.

#### D2 — Adaptive outlier threshold (PPP-RTK only)

In `residual_test()` (PPP-RTK's outlier rejection loop), the per-residual threshold
`inno0` is inflated 10× when the corresponding phase-bias state was just initialised:

```c
double inno0_eff = inno0;
if (type == 0 && rtk->opt.std[0] > 0.0) {
    int ib = IB_RTK(sat2, freq, &rtk->opt);
    if (rtk->P[ib + ib * rtk->nx] == SQR(rtk->opt.std[0])) inno0_eff *= 10.0;
}
```

PPP-RTK uses a sigma-normalised test (`v² < Q × inno0²`) where `Q` already incorporates
the large initial bias variance via the Kalman innovation covariance.  When bias
variance is large, `inno0 × √Q` is already a generous threshold; the 10× inflation is
a belt-and-suspenders guard that has no adverse effect.

**Why D2 was not applied to PPP**: PPP's outlier check compares `|v| > maxinno` in
metres (not sigma-normalised).  A newly-initialised PPP phase bias has undifferenced
residuals that can legitimately reach tens of metres (ionosphere, troposphere, orbit
errors not yet estimated).  Inflating `maxinno` by 10× (default 30 m → 300 m) admitted
extreme outliers into the filter, corrupting the bias state.  Benchmark confirmed:
tokyo_run1 MADOCA RMS degraded from **1.8 m to 199 m**.  The change was reverted; PPP
retains the original fixed-threshold check.

**Files changed**: `src/pos/mrtk_ppp_rtk.c`

---

## Benchmark Results

All benchmarks use `--skip-epochs 60`, PPC-Dataset urban driving runs (6 × nagoya/tokyo).
See [docs/benchmark.md](benchmark.md) for full three-tier tables.

### CLAS (PPP-RTK) — FIX tier (Q = 4)

| Run | Fix% (v0.3.3) | Fix% (v0.4.2) | Δ Fix% | RMS_2D fix (v0.3.3) | RMS_2D fix (v0.4.2) | 1σ (v0.3.3) | 1σ (v0.4.2) |
|-----|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| nagoya_run1 | 17.0% | 17.0% | 0 pp | 1.105 m | 1.105 m | 0.402 m | 0.402 m |
| nagoya_run2 | 26.9% | 23.4% | **−3.5 pp** | 1.088 m | 1.119 m | 0.717 m | **0.461 m** |
| nagoya_run3 |  6.3% |  6.3% | 0 pp | 0.318 m | 0.318 m | 0.339 m | 0.339 m |
| tokyo_run1  |  5.2% |  4.9% | −0.3 pp | 0.868 m | **0.747 m** | 0.239 m | 0.244 m |
| tokyo_run2  | 21.7% | 21.7% | 0 pp | 0.590 m | **0.514 m** | 0.117 m | 0.120 m |
| tokyo_run3  |  7.4% |  7.4% | 0 pp | 0.801 m | 0.801 m | 0.075 m | 0.075 m |

- **2/6 Tokyo runs**: FIX RMS_2D improved (tokyo_run1: −14%, tokyo_run2: −13%)
- **4/6 runs**: Fix% and accuracy unchanged
- **nagoya_run2**: −3.5 pp fix rate, but 1σ accuracy improved (0.717 m → 0.461 m) and
  FF rate improved (+4 pp: 63.9% → 67.9%); fewer but higher-quality fixes

### MADOCA (PPP) — PPP tier (Q = 3)

All six MADOCA runs are **identical to the v0.3.3 baseline**.  The Phase A/C/D1
improvements did not produce a measurable change on this dataset; Phase D2 was
explicitly excluded.

---

## Test Suite (57 tests, unchanged)

All 57 regression tests from v0.4.1 continue to pass.  No new test entries were added.

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

## Acknowledgements

RTK algorithm improvements in this release (Phases A–D) are adapted from
**demo5 RTKLIB** by [rtklibexplorer](https://github.com/rtklibexplorer/RTKLIB).

Benchmark results use the **PPC-Dataset** from the Precise Positioning Challenge
2024 (高精度測位チャレンジ2024), organised by the
[Institute of Navigation Japan (測位航法学会)](https://www.in-japan.org/):

> Taro Suzuki, Chiba Institute of Technology.
> *PPC-Dataset — GNSS/IMU driving data for precise positioning research.*
> <https://github.com/taroz/PPC-Dataset>
