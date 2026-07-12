# MRTKLIB — SPP Accuracy Enhancement (Doppler / TDCP EKF + Robust Weighting)

> **Status:** P1–P4 implemented (PR #164, default-off) · P5/P6 deferred to the
> smartphone benchmark ([#165](https://github.com/h-shiono/MRTKLIB/issues/165)) ·
> **Tracking:** [#116](https://github.com/h-shiono/MRTKLIB/issues/116)
>
> This document is both the design rationale and the as-built record for
> improving single-point positioning (SPP, `PMODE_SINGLE`) accuracy. P1–P4 ship
> in PR #164 (all TOML-gated, `prcopt_default` off → existing behaviour
> bit-identical); P5 (clock-jump) and P6 (position EKF) were investigated and
> deferred (§4.6). GNSS-algorithm changes here each had maintainer sign-off and
> an algorithm-safety review per CLAUDE.md §3/§9.1; per-step results are in §4.

---

## 1. Background

MRTKLIB's SPP is a faithful port of RTKLIB 2.4.3 `pntpos.c`, living in
[`src/pos/mrtk_spp.c`](../../src/pos/mrtk_spp.c). It is a **per-epoch
weighted-least-squares (WLS) snapshot estimator** with these characteristics:

- `estpos()` — pseudorange WLS, state `x[NX]` = position(3) + 7 inter-system
  clock terms. **No state is carried across epochs**; the previous solution is
  only an iteration seed, not a covariance-weighted prior.
- `estvel()` / `resdop()` — **Doppler is already used** for a *separate*
  snapshot velocity solve (`x[4]` = velocity(3) + clock drift). It is not fused
  with position and does not smooth the position track.
- `varerr()` — **elevation-only** measurement weighting. SNR/C-N0 is used only
  for *masking* (`snrmask()`), never for weighting.
- `raim_fde()` — single-satellite fault detection/exclusion.
- No carrier-phase usage, no time-differenced carrier phase (TDCP), no
  carrier smoothing, no robust estimator, no clock-jump handling.

The consequence: a static SPP user gets the full per-epoch scatter (no
time-averaging at all), and a kinematic user gets a track that jumps whenever a
single pseudorange outlier slips through RAIM.

## 2. What "accuracy" means here (precision vs. bias)

The two halves of "accuracy" respond to *different* techniques. Conflating them
is the most common way to over-promise on this work.

| Aspect | Definition | What moves it |
|--------|-----------|---------------|
| **Precision** | epoch-to-epoch scatter / repeatability (RMS noise) | time-averaging (EKF), TDCP (position-domain carrier smoothing), Doppler velocity constraint |
| **Bias / accuracy** | offset of the *mean* position from truth | robust down-weighting of multipath/NLOS, C-N0 weighting, better atmosphere/ephemeris models |

**TDCP is a time difference**, so quasi-static systematic errors (fixed
multipath bias, ionosphere/troposphere residual, broadcast orbit/clock error,
code biases) cancel in the difference and carry *no* information to correct the
bias. TDCP therefore tightens the cloud but does not move its centre. Moving
the centre (especially for a static receiver) is the job of robust + C-N0
weighting. This is why the delivery order below leads with weighting, not EKF.

## 3. Scope

**In scope**

A **robust WLS front-end** that stays inside the existing snapshot estimator
(no architecture change):

- C-N0 (Sigma-ε) measurement weighting added to `varerr()`.
- IGG-III equivalent-weight robust re-weighting in the `estpos()` iteration.
- RAIM-FDE improvement (multi-fault, better exclusion thresholds).
- Doppler-based quality control (predicted-vs-measured consistency gating).

Then an **auxiliary TDCP relative-displacement constraint** (still
snapshot-coupled), and only after that an **optional, gated SPP-EKF**:

- TDCP used to tighten the velocity solve and to gate between-epoch jumps.
- Common-mode (all-satellite) receiver clock-jump correction + a logging /
  reset strategy (the prerequisite for a stable filter).
- An EKF for `PMODE_SINGLE` carrying position / velocity / acceleration /
  clock / clock-drift / inter-system bias states, with Doppler and TDCP fused.

Tooling:

- A `single` mode added to the PPC-Dataset benchmark for before/after metrics
  (P0, shipped).

**Out of scope** — external / post-processing / optional extensions, not the
SPP core:

- ML / NLOS classification (issue #116 priority 4).
- Factor-graph optimization (priority 3) — a batch / sliding-window optimizer
  that does not fit the streaming rtkrcv loop (see [§7](#7-real-time-considerations)).
- 3D-mapping-aided (3DMA) GNSS — requires an external building model.

## 4. Delivery order and rationale

A literature survey of SPP accuracy techniques (maintainer-local research
report; its findings are distilled in §2–4 and the references in §9, and the
work is tracked under [#116](https://github.com/h-shiono/MRTKLIB/issues/116))
ranks **TDCP-EKF first** for *kinematic impact* and *RTKLIB affinity*. That is
the single highest-ceiling lever and the architecturally correct estimator for a
real-time library — but it is not the right *first* step.

Two pieces of evidence reorder the work toward a **robust WLS front-end first,
EKF last**:

1. **The P0 baseline is outlier-dominated** ([§4.1](#41-p0-baseline-measured-2026-05-24)):
   in several runs `RMS 2D > p95`, so a handful of blunders dominate the error.
   The direct fix for that is robust weighting + fault exclusion, not a filter.
2. An independent design review (Codex) reached the same conclusion: land the
   improvements that drop into the existing WLS without breaking it
   (**SNR/C-N0 weighting, robust WLS, RAIM-FDE improvement, Doppler quality
   control**) first; introduce **TDCP as an auxiliary relative-displacement
   constraint** next; and only **EKF-ify after sufficient logging and a reset
   strategy exist**. FGO / ML / 3DMA are external/optional, not the core.

This also keeps risk monotonic: P1–P3 are confined to the snapshot WLS (no
state carried across epochs, so RTK/PPP seeding is unaffected in structure), and
the architecture change (the EKF) is deferred until the front-end is robust.
C-N0 weighting is additionally the cheap lever that moves **static bias**, which
the maintainer explicitly wants improved and which TDCP/EKF cannot.

| Step | Content | Architecture | Primary gain | Risk |
|------|---------|--------------|--------------|------|
| **P0** | `single` mode in PPC benchmark + baseline *(shipped)* | tooling | measurement harness | none |
| **P1** | C-N0 (Sigma-ε) weighting in `varerr()`, TOML-gated *(implemented)* | WLS | bulk (rate/median); defeats gate → §4.2 | low |
| **P2** | IGG-III robust re-weighting in `estpos()` (MAD scale) *(implemented)* | WLS | bulk (rate +11pp, median); defeats gate → §4.3 | low |
| **P3** | Pre-robust all-satellite acceptance gate *(implemented)* — unblocks P1+P2 → clean win (§4.4) | WLS | **tail control; rate +16pp, median −43%** | medium |
| **P4** | TDCP velocity + jump-rejection QC + slip detection *(implemented)* — §4.5 | WLS + light coupling | **tail/RMS −56% vs P0; rate +20pp** | medium |
| **P5** | Common-mode clock-jump correction + logging / reset strategy | infra | low-cost-receiver continuity; EKF prerequisite — *N/A on the GSDC `device_gnss.csv` source* (§4.7) | medium |
| **P6** | SPP position EKF (loosely-coupled) — *investigated, reverted twice* (§4.6 PPC, §4.7 GSDC) | EKF | no accuracy gain (PPC or smartphone); smoothing adds lag | — |

Each step ships as an independent, reviewable commit, must pass the full
`ctest --output-on-failure`, and records numerical deltas (CLAUDE.md §7.2 —
tolerances are never silently changed).

### 4.1 P0 baseline (measured 2026-05-24)

The current SPP code path, measured on all six PPC-Dataset urban-driving runs
via the new `single` benchmark mode (config: [`conf/benchmark/single.toml`](../../conf/benchmark/single.toml)
— single-frequency L1, Klobuchar iono, Saastamoinen tropo, broadcast ephemeris,
elevation-only weighting, RAIM-FDE; first 60 epochs skipped for convergence):

| Case | N | mean nSV | <2 m | RMS 2D | 1σ (p68) | p95 |
|------|---|---------:|-----:|-------:|---------:|----:|
| nagoya_run1 | 5832 | 19.8 | 57.1% | 27.92 m | 3.31 m | 17.82 m |
| nagoya_run2 | 6101 | 20.5 | 56.9% | 11.12 m | 3.41 m | 20.30 m |
| nagoya_run3 | 4238 | 18.3 | 25.7% | 10.59 m | 12.15 m | 18.36 m |
| tokyo_run1  | 9435 | 20.7 | 33.0% | 11.59 m | 6.90 m | 24.41 m |
| tokyo_run2  | 8327 | 25.8 | 44.3% |  5.19 m | 3.91 m | 10.67 m |
| tokyo_run3  | 14265 | 27.0 | 30.1% | 36.02 m | 3.91 m | 14.69 m |

Reproduce:

```
python3 scripts/benchmark/run_benchmark.py \
  --dataset-dir <PPC-Dataset> --mode single --skip-download --skip-epochs 60
```

(The PPC-Dataset is downloaded manually — see
[`docs/reference/benchmark.md`](../reference/benchmark.md).)

**What the numbers say, and how it shapes the work:**

- **RMS is outlier-dominated.** In several runs `RMS 2D > p95` (run1: 27.9 m vs
  17.8 m; tokyo_run3: 36.0 m vs 14.7 m) — a handful of large blunders beyond the
  95th percentile dominate the RMS. This is exactly the snapshot-WLS
  position-jump vulnerability that robust down-weighting + fault exclusion
  (P2/P3) and temporal continuity (EKF, P6) target. Expect the biggest headline
  improvement from the robust WLS front-end.
- **Median accuracy is 3–12 m (p68).** Typical urban-canyon single-frequency
  SPP. Tightening this is the precision job of the EKF + TDCP.
- **~18–27 satellites tracked.** Ample redundancy for robust estimators and
  RAIM-style exclusion — the geometry is not the limiter; outlier handling is.

These six rows are the fixed before/after reference for P1–P6.

### 4.2 P1 result (C/N0 weighting, measured 2026-05-24)

P1 ([§5.8](#58-cn0-sigma-epsilon-weighting-p1)) was measured against the P0
baseline across a small `(snr_max, snr_error)` sweep (mean over the six runs):

| Setting | <2 m rate | RMS 2D | p68 | p95 |
|---------|----------:|-------:|----:|----:|
| P0 (off) | 41.2% | 17.07 m | 5.61 m | 17.71 m |
| (50, 0.5) | 44.1% | 17.05 m | 5.35 m | 18.44 m |
| (50, 0.3) | 42.8% | 17.00 m | 5.46 m | 18.29 m |
| (45, 0.3) | 42.1% | 17.06 m | 5.52 m | 17.99 m |
| (48, 0.4) | 42.9% | 16.97 m | 5.45 m | 18.38 m |

The pattern is consistent across every setting: C/N0 weighting **improves the
bulk** (fix rate +1–3 pp, median p68 slightly) **but worsens the p95 tail**
(+0.3–0.7 m). The mechanism is gate loosening — the additive SNR variance
inflates the per-epoch covariance, so the chi-square test in `valsol()` admits
more epochs (epoch count rises), and the newly-admitted marginal epochs land in
the tail. Per-run it is not uniform: a clear win on tokyo_run2 (<2 m
44.3 → 53.2%, p95 10.67 → 9.09 m) but a tail regression on nagoya_run1.

This is exactly the designed division of labour: **C/N0 weighting moves the bulk,
not the outlier tail** — the tail (the dominant P0 error) is the job of P2
(robust residual re-weighting) and P3 (RAIM). C/N0 weighting is therefore
**shipped but left default-off** (`snr_error = 0`) in [`single.toml`](../../conf/benchmark/single.toml);
the enable decision is deferred until P2/P3, after which P1+P2 are evaluated
together (the hybrid validated in Remote Sensing 12(16):2550).

### 4.3 P2 result and the gate-defeat finding (measured 2026-05-24)

P2 ([§5.6](#56-robust-estimation-p2-implemented-and-the-gate-defeat-finding))
adds IGG-III robust re-weighting. Mean over the six runs:

| Config | <2 m rate | RMS 2D | p68 | p95 |
|--------|----------:|-------:|----:|----:|
| P0 baseline | 41.2% | 17.07 m | 5.61 m | 17.71 m |
| P2 alone (k0=1.5,k1=4.0) | 47.6% | 21.35 m | 4.83 m | 30.82 m |
| **P1+P2 (1.5,4.0)+snr** | **52.5%** | 19.11 m | **4.30 m** | 27.30 m |

Robust + C/N0 weighting is a **large bulk win** (fix rate 41 → 52 %, median p68
5.61 → 4.30 m) but **blows up the p95 tail** (17.7 → 27 m). The per-run detail
identifies the mechanism precisely:

| Run | epochs N (P0 → P1+P2) | p68 | p95 |
|-----|----------------------:|-----|-----|
| tokyo_run2 | 8327 → 8505 (stable) | 3.91 → 2.19 m | 10.67 → **8.08 m** ✓ |
| tokyo_run3 | 14265 → 13933 (−) | 3.91 → 2.45 m | 14.69 → **14.15 m** ✓ |
| nagoya_run2 | 6101 → 8573 (**+40 %**) | 3.41 → 6.20 m | 20.30 → **56.13 m** ✗ |

**Where the epoch count N stays stable, P1+P2 is a clean win on every metric**
(tokyo_run2/run3: better rate, median, *and* tail). **Where N explodes, the tail
explodes with it** (nagoya_run2 +40 % epochs → p95 triples). The median improves
almost everywhere — the estimator genuinely produces better solutions.

The problem is therefore **not the estimator but the output gate**: the C/N0
variance inflation and robust down-weighting shrink the residuals that
`valsol()`'s chi-square test consumes, so the test stops rejecting the bad
epochs it used to catch (N rises) and those epochs land in the tail. The
weighting defeats the gate.

This precisely scopes **P3**: restore an acceptance gate that the weighting
cannot defeat. With the gate restored, the tokyo_run2 result predicts P1+P2+P3
should be a clean win across the board.

### 4.4 P3 result — the gate is the fix (measured 2026-05-24)

A baseline diagnostic with the gate disabled (`raim_fde = false`) settles the
mechanism: the number of *converged* epochs (e.g. nagoya_run1 7477, nagoya_run2
9364) **exceeds** the P1+P2 epoch count (6846 / 8573), which in turn exceeds P0
(5832 / 6101). So the bad epochs **already converge in the baseline** — robust
does not rescue non-converging epochs (it is *not* a convergence problem). P0's
`valsol()` correctly rejects them (gate-off p95 ≈ 62 m); weighting merely defeats
that gate. **It is a gate problem.**

The first P3 attempt — chi-square over the robust *inliers* (excluding the
down-weighted sats) — did **not** work (tail unchanged), because the
consistent-bias inliers are mutually consistent and pass. The fix is the
opposite: gate on the **pre-robust residuals over *all* satellites** (including
the ones the robust pass is about to suppress), evaluated at the robust solution,
and output the robust solution for accepted epochs. An epoch is accepted only if
the robust solution fits *every* satellite at nominal noise — so the suppressed
outliers' large residuals re-trigger rejection of the consistent-bias epochs,
while the genuinely clean epochs pass and keep their improved solution.

Result (P1+P2+P3, `robust="igg3"` k0=1.5/k1=4.0 + C/N0 snr_error=0.5), mean over
the six runs:

| Metric | P0 baseline | **P1+P2+P3** | change |
|--------|------------:|-------------:|-------:|
| <2 m fix rate | 41.2% | **57.5%** | **+16.3 pp** |
| p68 (median) | 5.61 m | **3.19 m** | **−43%** |
| p95 (tail) | 17.71 m | **15.73 m** | **−11%** (improved) |
| RMS 2D | 17.07 m | 16.55 m | slightly better |

A **clean win on every aggregate metric**: rate and median improve sharply and
the p95 tail *improves* (5 of 6 runs) rather than regressing. The epoch count
returns to ≈P0 (the gate again rejects the bad epochs), yet the absolute number
of <2 m epochs rises by ~900. The residual extreme tail (a few worst urban
epochs, visible in nagoya_run1's RMS) is the consistent-bias class that snapshot
methods cannot catch — that is the remit of the temporal work (P4 TDCP / P6 EKF).

P1+P2+P3 is therefore enabled together in [`single.toml`](../../conf/benchmark/single.toml);
the library default (`prcopt_default`) keeps all three off, so non-benchmark and
existing-test behaviour is bit-identical.

### 4.5 P4 result — TDCP velocity + jump rejection (measured 2026-05-24)

P4 adds time-differenced carrier phase: a per-satellite TDCP velocity solve
(replacing the Doppler measurement when the phase is locked and slip-free) and a
between-epoch jump-rejection QC that drops epochs whose code position change
disagrees with the TDCP-derived displacement (`velocity × dt`) by more than
`tdcp_jump` (5 m). Mean over the six runs, P1+P2+P3 → P1+P2+P3+P4:

| Metric | P0 | P1+P2+P3 | **+P4** | total vs P0 |
|--------|---:|---------:|--------:|------------:|
| <2 m rate | 41.2% | 57.5% | **61.2%** | **+20.0 pp** |
| p68 (median) | 5.61 m | 3.19 m | **2.83 m** | **−50%** |
| p95 (tail) | 17.71 m | 15.73 m | **12.42 m** | **−30%** |
| RMS 2D | 17.07 m | 16.55 m | **7.54 m** | **−56%** |

The jump-rejection QC collapses the **extreme tail** that P3 could not reach —
the consistent/jumpy worst-case epochs whose code position spikes contradict the
precise TDCP displacement. The clearest cases: tokyo_run3 RMS 36.85 → 4.84 m,
nagoya_run1 RMS 29.26 → 15.10 m. Rate and p95 improve on every run; the epoch
count drops only ~4%, so the QC removes wrong-position spikes, not good epochs.

The benchmark measures position, so this is the QC (position) effect. The TDCP
*velocity* (mm/s vs Doppler cm/s) is exercised by the same machinery — the QC
works precisely because the TDCP velocity is accurate (a noisy velocity would
mis-reject and hurt) — but a direct velocity check against the reference
`East/North/Up Velocity` columns is deferred tooling.

All four (P1–P4) are enabled in [`single.toml`](../../conf/benchmark/single.toml);
`prcopt_default` keeps them off (`tdcp=0`), so existing behaviour is bit-identical.

### 4.6 P6 investigated and reverted — the position EKF does not pay off here (2026-05-24)

The loosely-coupled position EKF ([§5.1](#51-architecture-decision--reuse-rtk_t-for-pmode_single)) was
implemented (`spp_posfilt()`: `udpos()` CV/CA time update + `filter()` with the
WLS position and TDCP velocity as measurements) and measured against P4:

| Metric (mean) | P4 | P6 loosely-coupled |
|---------------|---:|-------------------:|
| <2 m rate | 61.2% | 61.1% (same) |
| p68 | 2.83 m | 2.83 m (**no smoothing gain**) |
| p95 | 12.42 m | 12.49 m (slightly worse) |
| RMS 2D | 7.54 m | 37.1 m (**diverges**) |

Two findings, both negative on this dataset:

1. **No smoothing gain.** After P1–P4 the per-epoch WLS position is already good,
   so it dominates the filter (R ≈ a few m² ≪ the predicted covariance) and the
   output ≈ WLS — the median is unchanged. There is little epoch-to-epoch jitter
   left to smooth on geodetic-grade kinematic data.
2. **Divergence risk.** The only way to gain smoothing is to trust the
   velocity-driven prediction, but the TDCP velocity is occasionally
   slip-corrupted; integrated into the filter state it diverges (RMS spikes to
   100s of m). An innovation gate tames the worst of it but the residual spikes
   still leave RMS ~5× worse than P4, with no upside.

**Caveat — this was not a tuned evaluation.** The first cut reused `udpos()`'s
RTK-oriented process noise (white-noise on acceleration only) and a single
innovation-gate threshold; the SINGLE-specific process noise, velocity weight,
gate, and — most importantly — the slip handling that feeds the velocity were
*not* swept. So this is "an untuned first cut on unfavourable data," not "the
position EKF cannot work." Two things still make PPC the wrong place to tune it:
(a) the post-P1–P4 WLS already dominates, so the smoothing upside is inherently
small here; (b) the remaining tail is consistent NLOS bias, which no filter
fixes (needs 3DMA / external aiding).

The position EKF's value (track smoothing, coasting) shows up where there is
real jitter to smooth — **static** receivers or **smartphone/low-cost** data,
which the geodetic kinematic PPC set cannot exercise. **P6 was therefore reverted**
from the P1–P4 ship and deferred: a proper, *tuned* re-attempt (ideally a
tightly-coupled formulation with rigorous slip handling) belongs on a
smartphone/static benchmark — the GSDC / SDC sets, where jitter is large and
clock jumps actually occur (which also makes that the right venue for P5's
clock-jump correction).

### 4.7 P6 re-attempt on smartphone data (GSDC) — confirmed and reverted (2026-05-25)

[#165](https://github.com/h-shiono/MRTKLIB/issues/165) built the GSDC-2023
smartphone SPP benchmark precisely to give P6 (and P5) the favourable data §4.6
said they needed: large per-epoch jitter and real outage bursts. The loosely
coupled position EKF was re-implemented there — this time *tuned* (a robust
innovation gate, a covariance-bounded coast, and TDCP/Doppler velocity rows to
pin the velocity state) and finally as a **coast-only sidecar** (accepted WLS
epochs pass through unchanged; the EKF only fills the epochs the P1–P4 QC
rejects). Measured on the curated-6 anchor (official GSDC metric = mean of the
2D-horizontal p50 and p95, plus a `Cov%` = predicted ÷ reference epochs):

| Variant | N | Cov% | p50 | p95 | score |
|---------|---:|---:|---:|---:|---:|
| P1–P4 (snapshot WLS) | 907 | 52.3 | 2.56 | 5.82 | **4.19** |
| P6 untuned (no gate / no coast bound) | 1665 | — | 185 | 3297 | **1741** *(diverges)* |
| P6 smoothing-only (coast disabled) | 906 | 52.3 | 2.61 | 6.07 | **4.34** |
| P6 coast-only (tuned) | 941 | 54.4 | 2.59 | 6.00 | **4.29** |

Two findings, both confirming and **extending §4.6 to smartphone data**:

1. **Smoothing actively hurts — it is not merely neutral.** With the coast
   disabled (so only the measured epochs are touched), the EKF makes accuracy
   *worse* (4.19 → 4.34). The constant-velocity/acceleration model lags real
   stop-go / turning smartphone motion, and that lag outweighs the jitter it
   removes; the post-P1–P4 WLS is already good enough that there is no net
   averaging gain. This is the *second* independent negative result for the
   loosely-coupled position EKF (PPC in §4.6, GSDC here).
2. **The matched-only metric cannot reward P6's only contribution.**
   `compute_metrics` scores the epochs a run *predicts*; ground-truth epochs left
   unpredicted are simply absent. So it rewards aggressive QC (P1–P4 discarding
   ~48 % of epochs) and penalises coast (the filled epochs are harder than the
   median, so they raise p50/p95). The real GSDC requires a position for *every*
   sample-submission epoch, so the +2.1 pp of coverage P6 recovers would there
   replace default/penalty rows — but our harness, by design, undercounts that.
   The lasting fix kept from this work is the **`Cov%` column** (added to
   `run_gsdc_benchmark.py` / `compare_gsdc.py`), which surfaces the
   availability⇄accuracy trade-off for *every* config without re-scoring the
   already-published P1–P4 numbers over all epochs.

**P5 is not applicable on this data either.** P5 targets the millisecond
common-mode clock steering of low-cost receivers, but the chosen OBS source —
Google's derived `device_gnss.csv` — already reconstructs the clock: across all
six curated trips the `HardwareClockDiscontinuityCount` never increments (zero
common-mode jumps). The remaining slips are per-satellite and already handled by
the converter's `LLI`-on-`ADR_RESET`/`SLIP` plus P4's `spp_detslp`. P5's premise
holds only for the raw `gnss_log.txt` path, which #165 deliberately did not use.
demo5 confirms there is no smartphone clock-jump primitive to port: its
`pntpos.c` is clean and only `ppp.c` carries a *day-boundary* ambiguity reset.

**Decision: P6 reverted (the finding is the deliverable, not the code).** As a
reference implementation, MRTKLIB should carry only algorithms that earn their
place; a default-off EKF known not to improve accuracy is debt, not a feature.
The C code (`spp_filter` option, `spp_posfilt()`, the `pntpos` velocity-covariance
clear, the `gsdc_p6.toml` overlay) was reverted; the `Cov%` benchmark metric was
kept. The only credible accuracy path left for SPP is a **tightly-coupled**
raw-pseudorange/Doppler EKF with clock + inter-system-bias states (§5.2/§5.3 —
*not* pursued now); a loose post-filter over the WLS output structurally cannot
absorb the clock/ISB and NLOS-bias errors that dominate the residual tail.
Independently cross-reviewed (Codex), which reached the same conclusion.

## 5. Design

### 5.1 Architecture decision — reuse `rtk_t` for `PMODE_SINGLE`

> **Status: investigated and reverted ([§4.6](#46-p6-investigated-and-reverted--the-position-ekf-does-not-pay-off-here-2026-05-24)).**
> The loosely-coupled variant below was built and measured; it gave no gain and
> diverged on the geodetic kinematic benchmark. Retained as design rationale for
> a future tightly-coupled attempt on static/smartphone data.

The decisive finding is that the EKF infrastructure SPP needs already exists and
is already wired into the SPP code path:

- [`rtkpos()`](../../src/pos/mrtk_rtkpos.c) owns a **persistent `rtk_t`**
  (`rtk->x`, `rtk->P`, `rtk->ssat`, `rtk->tt`) across epochs.
- For `PMODE_SINGLE`, `rtkpos()` already calls `pntpos()` **with `rtk->ssat`**,
  so the per-satellite phase-history slots (`ssat.ph[]`, `ssat.pt[]`) used by
  TDCP are already reachable.
- The inter-epoch time delta `rtk->tt` is already computed.
- A constant-velocity / constant-acceleration position EKF (the kinematic
  dynamics model) already exists in [`udpos()`](../../src/pos/mrtk_rtkpos.c)
  (`NP(opt) = 9` when `dynamics != 0`), with its state-transition matrix.
- The Kalman primitives [`filter()`](../../src/core/mrtk_mat.c) and
  `smoother()` are battle-tested across PPP/RTK/VRS.

**Decision:** add an EKF path for `PMODE_SINGLE` inside `rtkpos()` that stores
state in the existing persistent `rtk_t`, reusing the `udpos()`-style time
update, `ssat` phase history, and `rtk->tt`. The snapshot `pntpos()` is **left
unchanged** because it is still the initial-position seed for RTK/PPP/PPP-RTK/VRS
([`mrtk_rtkpos.c` rover seed](../../src/pos/mrtk_rtkpos.c) and base seed). The
EKF is a parallel, gated path. The alternative — threading a new persistent SPP
state struct through `pntpos()`, `postpos`, and `rtksvr` — duplicates what
`rtk_t` already provides and was rejected.

> Note: the realtime helper SPP call in
> [`src/stream/mrtk_rtksvr.c`](../../src/stream/mrtk_rtksvr.c) passes
> `ssat = NULL`; it computes a throwaway base position, not the user solution,
> and stays on the snapshot path. Only the `rtkpos()` `PMODE_SINGLE` user
> solution is EKF-enabled.

### 5.2 State vector

SPP needs the receiver clock as a state (RTK differences it away), so the RTK
state layout is *not* reused verbatim — a dedicated SPP layout is defined:

```
x = [ rx ry rz | vx vy vz | ax ay az | cdt cddt | isb_glo isb_gal isb_bds3 ... ]
      position(3)  velocity(3)  accel(3)   clock+drift(2)   inter-system bias(NT-1)
                 (dynamics>=1) (dynamics==2)
```

- Velocity states exist only when `dynamics >= 1`; acceleration only when
  `dynamics == 2`. `dynamics == 0` ⇒ no kinematic states (degenerates toward
  snapshot, see [§6](#6-the-doppler-absence-invariant)).
- Inter-system biases reuse the seven-clock decomposition already in `estpos()`,
  modelled as random walks.
- Time update reuses the `udpos()` transition pattern (`F[i+(i+3)*nx]=tt`,
  optional accel coupling), with `cdt += cddt*tt`.

### 5.3 Pseudorange + Doppler measurement update

A single [`filter()`](../../src/core/mrtk_mat.c) update stacks all measurement
rows:

- **Pseudorange** rows constrain position / clock / ISB. The Jacobian and
  correction terms are lifted directly from
  [`rescode()`](../../src/pos/mrtk_spp.c).
- **Doppler** rows constrain velocity / clock-drift, lifted from
  [`resdop()`](../../src/pos/mrtk_spp.c) (existing, validated logic).

### 5.4 TDCP velocity + jump rejection (P4, implemented)

The auxiliary TDCP, kept within the snapshot architecture (the full EKF fusion is
P6). Two pieces, plus the enabling infrastructure:

- **Phase history**: `pntpos()` stores `ssat.ph[0][f]` / `ssat.pt[0][f]` at the
  end of each epoch (the SPP path previously did not; only RTK/PPP did). Reached
  via `rtk->ssat` in the post `PMODE_SINGLE` path.
- **Slip detection** (`spp_detslp()`): LLI plus the demo5 Doppler-vs-phase-rate
  consistency test (the `detslp_dop()` logic, kept SPP-local), gated by
  `thresdop`. Slipped sats fall back to Doppler.
- **TDCP velocity** (`resdop()`): per satellite, when locked and slip-free, the
  measured range rate is the TDCP phase rate `(φ(t)−φ(t-1))/Δt` (mm/s-class;
  van Graas & Soloviev 2004, Freda et al. 2015) instead of the Doppler;
  otherwise Doppler (preserving the Doppler-absence invariant). The geometric
  model and design matrix are unchanged. *Approximation:* the TDCP average rate
  over [t-1,t] is paired with the instantaneous geometric rate at t — an
  O(accel·Δt) error, negligible at 1 Hz next to the noise reduction.
- **Jump rejection** (P4b): compare the code position change to the TDCP
  displacement (`velocity·Δt`); if they differ by more than `tdcp_jump` the epoch
  is rejected (`SOLQ_NONE`). Phase history is still recorded for rejected epochs
  so TDCP continuity survives.

Gated by `[positioning] tdcp` (default off → bit-identical). Measured effect:
[§4.5](#45-p4-result--tdcp-velocity--jump-rejection-measured-2026-05-24).

### 5.5 Common-mode clock-jump correction (P5)

> **Status: deferred — no validation target in the available data ([§4.7](#47-p6-re-attempt-on-smartphone-data-gsdc--confirmed-and-reverted-2026-05-25)).**
> PPC is geodetic (clocks do not step) and the GSDC `device_gnss.csv` source has
> its clock already reconstructed (zero `HardwareClockDiscontinuityCount`
> increments). P5 only bites on the raw `gnss_log.txt` smartphone path or a live
> low-cost stream, neither of which is currently benchmarked. Design retained for
> when such data is available.

Low-cost receivers steer their clock, producing simultaneous millisecond jumps
on every satellite's carrier phase. `rescode()` already processes all visible
satellites in one pass, so the structural prerequisite is met. The added logic
estimates a single common ms step (median of per-satellite `Δφ`, in integer
multiples of `c × 1 ms`) and removes it from all satellites **before**
differencing — preventing the jump from being mistaken for per-satellite cycle
slips (Everett et al. 2022 / demo5 — [§9](#9-references)). It is a prerequisite
for stable TDCP on low-cost hardware.

### 5.6 Robust estimation (P2, implemented) and the gate-defeat finding

An IGG-III equivalent-weight (Yang et al. 2001) re-weighting pass is added to the
`estpos()` WLS Gauss-Newton iteration: each pseudorange row's weight is scaled by
a three-segment function `igg3_weight()` of its standardized residual, smoothly
down-weighting medium outliers (`k0<|r̃|≤k1`) and rejecting gross ones
(`|r̃|>k1`). The residual is standardized by a **MAD robust scale**
(`σ̂ = max(1, 1.4826·median|r̃|)`; Rousseeuw & Croux 1993, Huber 1981), so a
uniform mis-scaling of the a-priori variances does not trigger mass rejection.
It is applied from the 2nd iteration (`i≥1`), after the clock is estimated, and
only to the satellite rows (the rank constraints are left intact). Gated by
`[positioning] robust = "igg3"` with `robust_k0`/`robust_k1` (defaults 1.5/4.0);
`robust = "off"` (default) is bit-identical.

The benchmark ([§4.3](#43-p2-result-and-the-gate-defeat-finding-measured-2026-05-24))
shows this is a large bulk win that **defeats the chi-square gate** and inflates
the tail. That is the central finding: weighting (P1) and robust re-weighting
(P2) improve the solution but make `valsol()`'s residual-based test pass for
epochs it should reject. The fix is the P3 gate ([§5.8](#58-pre-robust-acceptance-gate-p3-implemented)),
not more estimator work. The same robust pass is later exposed in the EKF
measurement update (P6), giving the robust-WLS + TDCP-EKF hybrid validated in
Remote Sensing 12(16):2550 (2020).

### 5.7 Cycle-slip gating (P4 for TDCP auxiliary, reused at P6)

TDCP breaks on a single undetected slip, so detection is layered:

- `obs.LLI[]` loss-of-lock bits.
- Doppler-predicted vs. measured phase consistency (port/share
  [`detslp_dop()`](../../src/pos/mrtk_ppp.c) logic).
- Geometry-free phase jump (dual-frequency,
  [`detslp_gf()`](../../src/pos/mrtk_ppp.c) equivalent).

A satellite flagged as slipped drops only its TDCP row for that epoch; its
pseudorange row is still used. The existing `detslp_*` helpers are `rtk_t`-bound
file statics in [`mrtk_ppp.c`](../../src/pos/mrtk_ppp.c); sharing them with SPP
needs a small refactor to a common helper, designed at P4.

### 5.8 Pre-robust acceptance gate (P3, implemented)

When the robust pass runs, `estpos()` snapshots the standardized residuals over
**all** satellites *before* the IGG-III re-weighting (`vpre`) and feeds those to
`valsol()` instead of the down-weighted residuals. The accept/reject chi-square
therefore tests whether the (robust) solution is consistent with *every*
satellite at nominal noise, so the outliers the robust pass suppresses still
re-trigger rejection — the weighting can no longer defeat the gate. The robust
solution is what gets written to `sol`; only the gate statistic uses `vpre`.

The discarded first attempt gated over the robust *inliers* (excluding the
suppressed sats); §4.4 shows that fails, because consistent-bias urban epochs
have mutually-consistent inliers. Including all sats is the whole point. The gate
is active only when `robust != "off"`, so the default path is unchanged.

### 5.8 C/N0 (Sigma-epsilon) weighting (P1, implemented)

[`varerr()`](../../src/pos/mrtk_spp.c) gains an optional C/N0 term, identical in
form to the RTK `varerr()` ([`mrtk_rtkpos.c`](../../src/pos/mrtk_rtkpos.c)):

```
sigma^2 += (fact * err[6])^2 * 10^(0.1 * max(0, err[5] - CN0))
           err[5] = snr_max (dB-Hz),  err[6] = snr_error (m)
```

`err[5]`/`err[6]` were previously zero-initialised and unreachable from any
config; P1 exposes them as legacy options `stats-snrmax` / `stats-errsnr` and
TOML keys `[kalman_filter.measurement_error] snr_max` / `snr_error`, which also
makes the RTK engine's long-dormant SNR term configurable. The term is gated by
`err[6] > 0` (default 0 → off → bit-identical) and skipped when `CN0 == 0` so a
receiver that reports no C/N0 keeps the elevation-only model. The base
elevation/constant variance is untouched. See [§4.2](#42-p1-result-cn0-weighting-measured-2026-05-24)
for the measured effect and why it is shipped default-off.

## 6. The Doppler-absence invariant

**Hard design invariant** (testable acceptance criterion):

> When Doppler/TDCP are absent, the SPP solution is equivalent to the current
> WLS: bit-identical with the EKF gate off, and accuracy-equivalent with the
> gate on via the no-Doppler fallback.

This holds because Doppler and TDCP are **additive measurement rows, never
replacements** for the pseudorange path:

- Per-satellite absence is already handled — `resdop()` skips `D[0]==0.0`; the
  pseudorange row is unaffected.
- Whole-epoch absence ⇒ velocity states lose observability. Two safeguards:
  1. position process noise is set so an unobserved velocity prior cannot
     over-constrain position (the update degenerates to pseudorange-only ≈ WLS);
  2. a **safety valve**: if an epoch has zero Doppler *and* zero TDCP rows, that
     epoch falls back to the snapshot WLS solution (mirroring the
     `var > VAR_POS` reset already in [`udpos()`](../../src/pos/mrtk_rtkpos.c)).

**Verification:** the P0 benchmark is rerun on Doppler-stripped observations and
must reproduce the baseline metrics; this becomes a CI assertion.

## 7. Real-time considerations

No real-time concern:

- State dimension ≈ 17 (pos 3 + vel 3 + accel 3 + clock 2 + ISB 6).
  `filter()` is O(n³) but n=17 is negligible; ~30 measurement rows likewise.
- TDCP needs only the previous epoch (already in `ssat`), so it is naturally
  streaming; no batch buffering.
- rtkrcv already runs the far larger RTK/PPP EKFs in real time; the SPP EKF is a
  fraction of that cost.

This is also the positive argument for EKF over factor-graph optimization
(priority 3): FGO is a sliding-window batch optimizer that does not fit the
streaming `rtkrcv` loop, whereas the EKF is natively sequential. For a
real-time-capable library the EKF is the correct estimator.

## 8. Configuration (TOML gating)

Default behaviour is unchanged — every step is opt-in, so all existing tests
stay bit-identical:

- `[positioning] spp_filter = "wls" | "ekf"` (default `"wls"`).
- Reuse existing `pos1-dynamics` (0 none / 1 velocity / 2 accel) for the
  kinematic state selection.
- Sub-keys `spp_cn0_weight`, `spp_tdcp`, `spp_clkjump` to enable each step
  independently for staged rollout and ablation.

## 9. References

Verified against publisher records (May 2026). Confidence: ◎ established /
primary, ○ corroborating / applied.

**TDCP velocity estimation (mm/s; ambiguity cancels in the time difference)**

- ◎ Van Graas, F.; Soloviev, A. (2004). "Precise Velocity Estimation Using a
  Stand-Alone GPS Receiver." *NAVIGATION* 51(4):283–292.
  DOI [10.1002/j.2161-4296.2004.tb00359.x](https://doi.org/10.1002/j.2161-4296.2004.tb00359.x).
  Reports 2–4 mm/s (1σ) horizontal, 9.7 mm/s (1σ) vertical.
- ◎ Serrano, L.; Kim, D.; Langley, R.B.; Itani, K.; Ueno, M. (2004). "A GPS
  Velocity Sensor: How Accurate Can It Be? — A First Look." *Proc. ION NTM 2004*,
  San Diego, pp. 875–885.
  [PDF](http://gauss.gge.unb.ca/papers.pdf/ionntm2004.serrano.pdf).
- ◎ Freda, P.; Angrisano, A.; Gaglione, S.; Troisi, S. (2015).
  "Time-differenced carrier phases technique for precise GNSS velocity
  estimation." *GPS Solutions* 19(2):335–341.
  DOI [10.1007/s10291-014-0425-1](https://doi.org/10.1007/s10291-014-0425-1).

**Doppler / TDCP fused in an EKF (state-augmented), real-time, stand-alone**

- ◎ "Doppler measurement integration for kinematic real-time GPS positioning."
  *Applied Geomatics* 2(4):155–162 (2010).
  DOI [10.1007/s12518-010-0031-z](https://doi.org/10.1007/s12518-010-0031-z).
  Single-frequency, urban, loosely-coupled EKF — direct precedent for
  Doppler-in-EKF in real time.
- ◎ "The Design a TDCP-Smoothed GNSS/Odometer Integration Scheme with
  Vehicular-Motion Constraint and Robust Regression." *Remote Sensing*
  12(16):2550 (2020).
  DOI [10.3390/rs12162550](https://doi.org/10.3390/rs12162550).
  State-augmented EKF combining **TDCP + robust regression** — the template for
  the robust-WLS (P2) + TDCP-EKF (P6) hybrid in this design.
- ○ "A Doppler enhanced TDCP algorithm based on terrain adaptive and robust
  Kalman filter using a stand-alone receiver." *The Journal of Navigation*
  (Cambridge University Press).
  [Article page](https://www.cambridge.org/core/journals/journal-of-navigation/article/abs/doppler-enhanced-tdcp-algorithm-based-on-terrain-adaptive-and-robust-kalman-filter-using-a-standalone-receiver/6A77A5FAE63FB107348D85F16FB1E908).
  (volume/year to confirm on cite.)

**Common-mode clock-jump correction (demo5 lineage)**

- ◎ Everett, T.; Taylor, T.; Lee, D.-K.; Akos, D.M. (2022). "Optimizing the Use
  of RTKLIB for Smartphone-Based GNSS Measurements." *Sensors* 22(10):3825.
  DOI [10.3390/s22103825](https://doi.org/10.3390/s22103825).
- demo5 / rtklibexplorer — implementation primary source (gray literature).

**Carrier smoothing (origin of the position-domain smoothing TDCP generalises)**

- ◎ Hatch, R. (1982). "The Synergism of GPS Code and Carrier Measurements."
  *Proc. 3rd Int. Geodetic Symp. on Satellite Doppler Positioning*, vol. 2,
  pp. 1213–1231.

**Robust estimation (IGG-III; the lever for static bias — priority 2 / P2)**

- ◎ Yang, Y.; He, H.; Xu, G. (2001). "Adaptively robust filtering for kinematic
  geodetic positioning." *Journal of Geodesy* 75:109–116.
  DOI [10.1007/s001900000157](https://doi.org/10.1007/s001900000157).
- ○ "Mitigating Integrity Risk in SBAS Positioning Using Enhanced IGG III Robust
  Estimation." *Remote Sensing* 17(17):3067 (2025).
  DOI [10.3390/rs17173067](https://doi.org/10.3390/rs17173067).

**Robust scale (MAD) for the standardized residual (P2)**

- ◎ Rousseeuw, P.J.; Croux, C. (1993). "Alternatives to the Median Absolute
  Deviation." *JASA* 88(424):1273–1283.
  DOI [10.1080/01621459.1993.10476408](https://doi.org/10.1080/01621459.1993.10476408).
  Establishes `MADn = 1.4826·med{|xᵢ − med xⱼ|}` as a 50%-breakdown,
  bounded-influence robust scale (1.4826 = Gaussian-consistency factor).
- ◎ Huber, P.J. (1981). *Robust Statistics*. Wiley. MAD as the "single most
  useful ancillary estimate of scale"; foundation for robust scale + IRLS.
- ○ Blewitt, G. et al. (2016). "MIDAS robust trend estimator for accurate GPS
  station velocities without step detection." *JGR Solid Earth* 121 — MAD-scaled
  robust estimation in operational GNSS.

## 10. Open questions

- **Static accuracy truth.** PPC-Dataset is kinematic. Static bias improvement
  (P1) needs a known static coordinate validated against an IGS SINEX
  (epoch-propagated, not a station webpage coordinate).
- **`detslp_*` sharing.** Exact refactor to expose cycle-slip detection to SPP
  without disturbing PPP/RTK — settled at P4.
- **demo5 clock-jump parity.** *Resolved ([§4.7](#47-p6-re-attempt-on-smartphone-data-gsdc--confirmed-and-reverted-2026-05-25)):* demo5 has no
  smartphone clock-jump primitive to port — `pntpos.c` is clean and only `ppp.c`
  carries a day-boundary ambiguity reset. P5 deferred for want of a data source
  that actually exhibits common-mode clock jumps.
- **IGG-III thresholds.** The `k0`/`k1` segment thresholds need tuning against
  the P0 baseline; start from the literature range and validate per [§4.1](#41-p0-baseline-measured-2026-05-24).
- **Tightly-coupled SPP EKF (Stage B).** The only credible remaining accuracy
  lever ([§4.7](#47-p6-re-attempt-on-smartphone-data-gsdc--confirmed-and-reverted-2026-05-25)): a raw-pseudorange/Doppler EKF with clock + ISB states
  (§5.2/§5.3), able to absorb errors the loose post-filter cannot. Not pursued
  now; the residual tail is largely NLOS bias, which no filter fixes (needs
  3DMA / external aiding).

## 11. Enhanced a-priori SPP seed for PPP-RTK (2026-06)

### 11.1 Motivation and mechanism

PPP-RTK/VRS computes a single-point fix every epoch (`pntpos`) that seeds and,
on reset, re-seeds the CLAS/VRS filter position state `x[0..2]`
([`mrtk_rtkpos.c`](../../src/pos/mrtk_rtkpos.c) seed block). On filter resets
(obs-loss gap, persistent-FLOAT, large PPP-RTK↔SPP divergence) `udpos_ppp`
re-initialises the position from this seed, and `resamb_LAMBDA` then builds its
float vector — including the position states — from `rtk->x`. So a poor seed can
propagate into the ambiguity search. The hypothesis was that the v0.6.10 SPP
accuracy work could lift the seed quality and reduce mis-fixes.

### 11.2 Separation of C/N0 and CLAS atmosphere-error terms

`err[5]`/`err[6]` are the C/N0 `snr_max`/`snr_error` coefficients used by the
SPP and RTK measurement models. They were formerly overloaded as ionosphere and
troposphere estimation-error terms by the CLAS PPP-RTK model. #261 separates the
CLAS terms into `stats_erriono` / `stats_errtrop`, exposed as
`[kalman_filter.measurement_error] ionosphere` / `troposphere`. C/N0 weighting
can therefore be configured without changing the CLAS measurement variance.

The seed still uses a private option copy (`prcopt_t sppopt = rtk->opt`) so its
profile does not overwrite the operational filter options.

### 11.3 The `enhanced_spp_seed` profile

`[positioning.clas] enhanced_spp_seed` (opt key `pos1-seedenh`):

| Value | Seed gets |
|-------|-----------|
| `off` | nothing — bit-identical to prior behaviour |
| `cn0+tdcp` | C/N0 weighting + TDCP jump-reject (**default**, `prcopt_default`) |
| `cn0+tdcp+robust` | the above + IGG-III robust (open-sky opt-in) |

PPC-Dataset (6 urban runs), `cn0+tdcp` vs baseline: fix rate 23.7 % → 24.7 %,
fixed-epoch 95th-pct 3.23 m → 3.02 m, with no large-misfix regression. It is the
default because it is a net win on kinematic data and **inert on the static
regression suite** — all six claslib static (みなしローバー) cases run
byte-close to the off baseline, with deltas (sub-cm RMS-vs-reference) well inside
the existing test tolerances; one VRS case improves (fix 98.9 % → 99.9 %).

### 11.4 Why robust is opt-in, not default — and why it cannot be auto-gated

Adding `robust` raises the aggregate fix rate (+1.0 pp more) and helps open-sky
runs markedly (nagoya_run2 fixed-RMS 2.00 m → 1.12 m, worst fixed error 24.8 m →
14.8 m). **But on the deep-urban-canyon run (tokyo_run3) it is destructive**:
mis-fixes 5 → 71, worst fixed error 4.4 m → 12 m, in sustained ~15-epoch clusters.

The mechanism is *not* seed inaccuracy — robust actually improves that run's seed
accuracy (Q1 median 2.48 m → 2.23 m). Instead, the *different* (even better) seed
trajectory trips the filter's reset/AR thresholds at discrete epochs, and
fix-and-hold then locks the resulting wrong integer (the clusters hold a constant
multi-metre offset). Dropping fix-and-hold globally is not viable — it halves the
fix rate on this data.

Two auto-gating strategies were implemented and benchmarked, then **removed**:

- **Self-gating in `estpos`** (skip robust when its MAD scale `s0` is large or the
  flagged-satellite fraction is high): *failed*. On tokyo_run3 robust is
  internally valid (residuals are not flagged), so the gate never fired —
  identical to always-on — and per-epoch on/off flipping worsened aggregate p95.
- **Filter-health gating** (enable robust only when an EWMA of recent fixed
  epochs is high): *partial*. Best aggregate p95 and fewest mis-fixes among the
  robust arms, and it halved tokyo_run3 mis-fixes (88 → 38), but it failed to
  capture robust's per-case benefit and a 12 m cluster survived.

**Conclusion:** robust's harm is a filter-coupling effect invisible to any
seed-local diagnostic; only an external filter-state signal points the right way,
and a coarse one cannot cleanly separate. The robust harm correlates with the
SPP-fallback regime (tokyo_run3 = 82 % SPP fallback vs nagoya_run2 = 11 %), i.e.
how reset-prone the filter is — not with geometry (tokyo_run3 has the *most*
satellites). So robust is left as an explicit operator opt-in: use it where the
environment is open-sky, omit it in dense urban canyons.

### 11.5 Real-time applicability

Real-time PPP-RTK/VRS-RTK (`mrtk run`) runs the same `rtkpos()` and the same seed
block (`mrtk_rtksvr.c` calls `rtkpos(&svr->rtk, ...)`), with the loaded `prcopt`
propagated to `svr->rtk.opt` via `rtkinit` in `rtksvrstart`, so the default
profile is active in real time as well. The persistent `svr->rtk.ssat` carries
the TDCP phase history across the stream loop. Real-time is arguably where the
seed matters most — stream gaps and cycle slips make it the reset-prone regime
the enhancement targets — but the benchmark numbers in this document are
post-processing; the real-time effect has not yet been separately measured.
