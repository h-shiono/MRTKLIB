# Release Notes — v0.7.4

## CLAS PPP-RTK claslib parity — storm-day fix-rate gap closed

**Release date:** 2026-07-04
**Type:** Accuracy (CLAS PPP-RTK claslib parity) + independent fixes
**Branch:** `release/v0.7.4`

---

### Overview

v0.7.4 is a five-part sync series that aligns the CLAS PPP-RTK engine with
upstream claslib across three layers — the correction-stream timing, the
measurement model, and the SPP seed — closing the storm-day fix-rate gap tracked
in [#225](https://github.com/h-shiono/MRTKLIB/issues/225).

On the deterministic-BLAS storm benchmark the kinematic fix rate rises
**89.44 % → 93.54 %**, now **exceeding upstream claslib's 92.88 %** on the same
data. The single largest contributor is the **BackupCSSR / BackupGrid** port
that bridges short correction gaps (**+118 fixed epochs**, confirmed by a 5-cell
ablation). A new opt-in **`MRTK_DETERMINISTIC_BLAS`** build links single-thread
OpenBLAS so these ±0.1 pp comparisons are bit-reproducible run-to-run — with
**no positioning-code change and the default build unaffected**.

The release also carries several independent bug fixes: a ~0.10 m Galileo E5a
antenna-PCV error, an `mrtk l6extract` stack overflow, all-GNSS solution-status
output, and `traceopen` path handling.

All CLAS-scoped changes are applied for CLAS only; the MADOCA-PPP references are
unaffected. All five #225 PRs (#244–#248) passed CI and Copilot review.

---

### 1. Storm-day fix-rate gap — what closed it

The #225 investigation traced the CLAS PPP-RTK deficit to three layers rather
than a single bug. Each layer was brought to claslib parity independently:

| Layer | Change | PR |
|-------|--------|----|
| Correction-stream timing | mask-time-aligned CSSR selection + per-epoch bank refresh | [#246](https://github.com/h-shiono/MRTKLIB/pull/246) |
| Gap bridging | BackupCSSR / BackupGrid (≤ 180 s) | [#246](https://github.com/h-shiono/MRTKLIB/pull/246) |
| Measurement model | claslib slot layout + OSR/adaptive-iono + PCV | [#247](https://github.com/h-shiono/MRTKLIB/pull/247) |
| Ionosphere decay | est-adaptive AR(1) time constant wired into `udion()` | [#245](https://github.com/h-shiono/MRTKLIB/pull/245) |
| SPP seed | Galileo E5a rows restored + large-residual exclusion | [#248](https://github.com/h-shiono/MRTKLIB/pull/248) |

#### BackupCSSR / BackupGrid gap bridging

Ported from claslib: CLAS now bridges correction gaps (≤ 180 s) with the last
usable CSSR / grid snapshot instead of dropping the affected epochs to a single
solution. On the #225 storm benchmark this is the largest single contributor
(**+118 fixed epochs**, confirmed by a 5-cell ablation).

#### CLAS PPP-RTK correction-stream timing

CSSR is read through the correction set whose mask time equals the observation
epoch, stopping at the first future mask — this fixes one-epoch-stale correction
selection and a startup epoch lost to an empty grid. Grid status is bootstrapped
by a network scan on the first epoch. `mrtk ssr2osr` now refreshes the bank
snapshot every epoch instead of only on a network change (it previously served
the first correction set until it aged out at ~60 s and then stopped)
([PR #246](https://github.com/h-shiono/MRTKLIB/pull/246)).

#### CLAS PPP-RTK measurement model

Brought to the claslib observation slot layout —
GPS {L1, L2, L5} / GAL {E1, –, E5a, E6, E5b, E5ab} / QZS {L1, L2, L5, L6} — with
the OSR ambiguity index for est-adaptive ionosphere, a facility-change
filter-reset guard, residual wavelength-ratio parity, and CPC freq-major
persistence across `zdres` passes. All of this is applied for CLAS only.

QZSS L1 signal priority (C > S > L > X > Z) is a **CLAS-scoped runtime
override**: claslib and MADOCALIB genuinely disagree on the order, so it is
enabled only from `mrtk post` while `correction = CLAS`, leaving the MADOCA-PPP
references unaffected ([PR #247](https://github.com/h-shiono/MRTKLIB/pull/247)).

#### Ionosphere Gauss-Markov decay

The est-adaptive ionosphere time constant was parsed but never wired into
`udion()` state propagation; the AR(1) decay now matches claslib
([PR #245](https://github.com/h-shiono/MRTKLIB/pull/245)).

#### CLAS SPP velocity seed

The GAL / SBS iono-free second frequency is now taken from slot 2 (E5a),
restoring the Galileo rows to the seed like upstream
([PR #248](https://github.com/h-shiono/MRTKLIB/pull/248)).

---

### 2. Deterministic-BLAS builds

`MRTK_DETERMINISTIC_BLAS` is a new opt-in CMake option that links a deterministic
LP64 single-thread OpenBLAS instead of Apple Accelerate. Accelerate's
non-deterministic floating-point reductions scatter storm-day fix rates
run-to-run because the AR ratio test amplifies ULP noise into fix/float flips;
OpenBLAS makes those comparisons bit-reproducible so the ±0.1 pp deltas above are
meaningful.

It is intended for benchmarking / CI reproducibility only. There is **no
positioning-code change and the default build is unaffected**. A configure-time
check verifies OpenBLAS is the resolved LAPACK provider
([PR #244](https://github.com/h-shiono/MRTKLIB/pull/244)).

---

### 3. New configuration

- **`pos2-rejethres` / `pos2-rejeminsat`** — upstream SPP large-residual
  exclusion. Default `0` keeps standalone SPP bit-compatible; the CLAS
  PPP-RTK / VRS private seed maps `0` to upstream's 20 m / 7-sat defaults
  ([PR #248](https://github.com/h-shiono/MRTKLIB/pull/248)).
- **`pos1-frequency = l1+l2+l5`** — config alias for the CLAS three-frequency
  observation slot layout ([PR #247](https://github.com/h-shiono/MRTKLIB/pull/247)).

---

### 4. Bug fixes

- **Galileo E5a antenna PCV error (~0.10 m)** — receiver PCV slot mapping now
  uses claslib-compatible ANTEX compaction. The stale offset had been cascading
  into ambiguity resets (the E34 case in the #225 ledger)
  ([PR #247](https://github.com/h-shiono/MRTKLIB/pull/247)).
- **`mrtk l6extract` 2-byte stack overflow** in `sbf_extract_l6_payload`
  ([PR #237](https://github.com/h-shiono/MRTKLIB/pull/237)).
- **`traceopen` filename handling** — guard path length before `reppath` and
  expand time keywords in the trace filename
  ([#83](https://github.com/h-shiono/MRTKLIB/issues/83),
  [PR #239](https://github.com/h-shiono/MRTKLIB/pull/239)).
- **Solution-status (`.stat`) output** now emits all GNSS, not just GPS
  ([PR #240](https://github.com/h-shiono/MRTKLIB/pull/240)).
- **Test harness** — BSD `mktemp` suffix collision in `run_rtkrcv_test.sh`
  ([#194](https://github.com/h-shiono/MRTKLIB/issues/194),
  [PR #241](https://github.com/h-shiono/MRTKLIB/pull/241)).

---

### 5. Documentation

- **ICD & academic-paper reference catalogs** — `docs/reference/icd/` and
  `docs/reference/papers/` index pages added to the MkDocs Reference nav
  (link-only rows; collected PDFs held locally)
  ([PR #238](https://github.com/h-shiono/MRTKLIB/pull/238)).

---

### 6. Validation

- **Storm-day CLAS kinematic PPP-RTK (deterministic OpenBLAS):** fix rate
  **89.44 % → 93.54 %**, above upstream claslib's **92.88 %** on the same data.
  The BackupCSSR contribution (**+118 epochs**) is confirmed by a 5-cell
  ablation.
- **claslib label 34/34** (including RT replay and F5 absolute-accuracy checks);
  the MADOCA-PPP references are unaffected (the QZSS L1 priority change is
  CLAS-scoped).
- Full `ctest` suite green apart from the two perennial environment failures on
  the maintainer's machine (`rtkrcv_rt` headless RT replay,
  `madocalib_pppar_ion_check` LAPACK-vs-reference tolerance), both of which
  reproduce identically on a clean `develop`.
- All five #225 PRs (#244–#248) passed CI and Copilot review.

---

### Compatibility & migration

No configuration changes are required. Existing CLAS PPP-RTK and MADOCA-PPP
workflows are unaffected; the CLAS parity work is scoped to `correction = CLAS`
and the MADOCA-PPP references are untouched. The new `pos2-rejethres` /
`pos2-rejeminsat` and `pos1-frequency = l1+l2+l5` options default to
backward-compatible behavior, and `MRTK_DETERMINISTIC_BLAS` is off by default
with no effect on the standard build.

### Issues / PRs

- [#225](https://github.com/h-shiono/MRTKLIB/issues/225) — CLAS PPP-RTK storm-day
  fix-rate gap vs. upstream claslib.
- [PR #244](https://github.com/h-shiono/MRTKLIB/pull/244) — `MRTK_DETERMINISTIC_BLAS`.
- [PR #245](https://github.com/h-shiono/MRTKLIB/pull/245) — ionosphere Gauss-Markov decay.
- [PR #246](https://github.com/h-shiono/MRTKLIB/pull/246) — correction-stream timing + BackupCSSR / BackupGrid.
- [PR #247](https://github.com/h-shiono/MRTKLIB/pull/247) — measurement model + Galileo E5a PCV fix.
- [PR #248](https://github.com/h-shiono/MRTKLIB/pull/248) — SPP seed + large-residual exclusion.
- [PR #250](https://github.com/h-shiono/MRTKLIB/pull/250) — release.
- Independent fixes: [PR #237](https://github.com/h-shiono/MRTKLIB/pull/237),
  [PR #238](https://github.com/h-shiono/MRTKLIB/pull/238),
  [PR #239](https://github.com/h-shiono/MRTKLIB/pull/239),
  [PR #240](https://github.com/h-shiono/MRTKLIB/pull/240),
  [PR #241](https://github.com/h-shiono/MRTKLIB/pull/241).

### References

- IS-QZSS-L6 — CLAS (L6D) CSSR message format.
- claslib — the upstream reference implementation for CLAS PPP-RTK.
