# CLAS PPP-RTK single-band handling — SF (nf=1) and the Galileo E1/E5b band mismatch

**Status: capability record + regression coverage (2026-07-05).**
The v0.7.4 engine handles both configurations below **without any dedicated
machinery**; this document records how, quantifies the accuracy against
independent truth, and describes the regression tests that lock the behavior
in. An explicit single-frequency ionosphere constraint was prototyped in June
2026 and **not shipped** — its full record, including why the v0.7.4 engine
subsumed it, is kept in Appendix A for anyone who considers reviving it.

---

## 1. The two configurations

**(a) True single-frequency (nf=1).** A signal set with one band per
constellation (e.g. `signals = ["G1C", "E1C", "J1C"]`,
[`conf/claslib/rnx2rtkp_sf.toml`](../../conf/claslib/rnx2rtkp_sf.toml)).
Low-cost L1-only receivers are the use case.

**(b) Galileo E1/E5b band mismatch.** A dual-frequency receiver tracks Galileo
E1+E5b (E7Q) while CLAS ST5/ST4 carry biases only for E5a (E5Q) — the standard
situation for u-blox ZED-F9P-class receivers under CLAS. The E5b band has no
matching correction, so the affected Galileo satellites are usable on E1 only
even though the receiver is dual-frequency.

## 2. How the engine handles them (mechanics)

### 2.1 A received signal with no matching CLAS bias is dropped per-frequency

The biases are initialised to an invalid sentinel and only overwritten on a
match:

- `pbias[i] = cbias[i] = CLAS_CSSRINVALID` in `clas_osr_corrmeas()`;
- two-pass resolve: exact `obs->code[i] == smode`, then same-frequency fallback
  `code_on_same_freq()`. E7Q and E5Q are different frequencies, so neither pass
  matches and the slot stays invalid.

`clas_osr_zdres()` then **drops that frequency** — it never applies a wrong
correction: the frequency loop and the residual write both `continue` on an
invalid bias, leaving `y[] = 0`, which `validobs()` rejects. The satellite's
corrected band (E1) is still used. In other words, "Galileo E5b with no CLAS
bias" degrades that satellite to E1-only *for free* — no false correction, no
satellite loss.

### 2.2 Why the unobservable iono state stays bounded

The per-satellite state `x[II_RTK(sat)]` is the *residual* slant ionosphere
after CLAS grid-STEC removal (target value 0). For a dual-frequency satellite
it is observable through the L1/L2 dispersive split; for a single-band
satellite it is **not observable** and, on pre-v0.7.4 engines, drifted under
the `udion()` random walk and contaminated the L1 DD/AR.

Since v0.7.4 the ionosphere state carries the claslib **Gauss-Markov (AR(1))
decay** (`stats_tconstiono` wiring,
[PR #245](https://github.com/h-shiono/MRTKLIB/pull/245)): a state that
receives no measurement update is pulled back toward zero with the configured
time constant instead of random-walking away. That is functionally the same
job an explicit zero-pseudo-observation would do — measured head-to-head in
Appendix A, the difference is sub-mm. Together with the v0.7.4 Galileo E5a
antenna-PCV fix (which removed a ~0.10 m systematic on exactly these
satellites), no dedicated single-band machinery is needed.

**This capability is CONDITIONAL on the iono decay being active.** "No
dedicated machinery" does not mean "unconditional": disable or break the
Gauss-Markov decay and the unobserved `x[II]` states drift again, single-band
satellites contaminate the L1 DD/AR, and the pre-v0.7.4 behavior returns —
which is exactly why the June 2026 engine *did* need the explicit constraint
(Appendix A.2). The regression tests in §4 pass *because* of this decay; anyone
changing the iono time-constant wiring should expect them to act as the
tripwire.

## 3. Measured capability (v0.7.4 engine, 2026-07-05)

### 3.1 True SF (nf=1) — bundled CLAS 2019/239, 16:00–17:00, 1 Hz

| metric | value |
|---|---|
| vs the DF reference solution (`ref_L6.nmea`), 3D RMS | 4.12 cm |
| fix rate | 99.86 % (same as DF) |
| **vs GSI F5 truth (TSUKUBA3, independent)**, 2D 1σ / 95 % | **2.54 / 3.91 cm** |

### 3.2 Galileo E1/E5b mismatch — ECJ02 u-blox ZED-F9P, 2026/05/01, 1 h

| metric | value |
|---|---|
| fix rate (incl. initial convergence) | 85.7 % (771/900 epochs) |
| **vs the ECJ02 IGS-products coordinate (independent)**, 3D 1σ / 95 % (after 150-epoch convergence skip) | **18.5 / 20.6 cm** |

The offset against truth is E/Up-loaded (mean E +14 / N 0 / U +10 cm) — an
inter-technique difference between CLAS PPP-RTK and the IGS-products frame on
this receiver/antenna, stable across the hour.

**What "handled" means here.** The mismatched Galileo satellites are degraded
to E1-only, so their contribution is reduced and the overall accuracy is
dm-level — clearly below the cm-level of a fully dual-frequency solution
(§3.1). "Handled" means the solution stays **bounded and non-divergent**
(previously the risk was losing those satellites entirely or applying a wrong
correction, and pre-v0.7.4 a drifting iono residual could contaminate the L1
AR) — it does **not** mean DF-equivalent accuracy on this receiver class.

**Truth provenance.** The ECJ02 coordinate (35.666334193, 139.792201653,
59.8539 m) is the IGS/MGEX final-products coordinate already used by
`igs_iflc_ppp_check` (#142) — fully independent of the CLAS corrections under
test. It was cross-checked this session against a 3-hour MADOCA-PPP float
solution (L6E sessions A+B+C): agreement E 9 / N 6 / U 12 cm, within the float
solution's own scatter (1σ 3D ≈ 8 cm). A tautological "engine locked against
its own output" reference is avoided: the consistency references
(`ref_L6_SF.nmea`, `ref_F9P0121a.nmea`) detect *drift*, while the `_abs_check`
tests anchor *correctness* to external truth.

## 4. Regression tests

| test | checks |
|---|---|
| `claslib_ppp_rtk_sf` (+`_check`, `_abs_check`) | nf=1 run on 2019/239; consistency vs `ref_L6_SF.nmea` (tol 0.10 m); absolute vs GSI F5 (tol 0.08 m 2D) |
| `claslib_ppp_rtk_f9p` (+`_check`, `_abs_check`) | E1/E5b-mismatch run on ECJ02 2026/05/01; consistency vs `ref_F9P0121a.nmea` (tol 0.10 m); absolute vs the IGS-products LLH (tol 0.30 m 3D, 150-epoch skip) |

Test data: `tests/data/claslib/claslib_f9p_testdata.tar.gz` (self-collected
ECJ02 raw converted to RINEX + the CLAS L6 hour), reusing the main
`claslib_data` fixture for aux files. `compare_nmea_abs.py` gained a `--llh`
truth option (mirroring `compare_pos_abs.py`) for the fixed-coordinate anchor.

---

## Appendix A — the SF iono-residual constraint (June 2026; NOT shipped)

Kept for anyone re-considering an explicit constraint. Code lives on the local
branch `feat/sf-ppp-rtk-iono-constraint` (commits `36f7af5`, `4f33595`,
`c2c69a9`); it was never merged.

### A.1 What it was

A pseudo-observation `v = 0 − x[II_RTK(sat)]`, `H = 1`, applied before AR to
every satellite usable on exactly one band this epoch, with variance
`svar·covratio + IONO_RESL_GRID_STD²` (svar = CLAS STEC URA variance,
GRID_STD default 0.10 m). Applied via a plain `filter()` call (the `holdamb`
pattern) — `filter2()`'s DD-pair gating cannot host a single-state pseudo-obs.

Eligibility evolved P2 → P3 → P3.1 and the evolution carries the durable
lesson: **count the bands that are *usable* (`vsat`, set by `ddres` only when
the band was observed AND had a matching valid CLAS bias), not the bands CLAS
*broadcasts* (`smode`)**. The intermediate smode criterion looked right and
failed on real F9P data — CLAS broadcasts E1+E5a for a receiver that can only
use E1, so the satellite looked dual-frequency and was never constrained
(output byte-identical to develop).

### A.2 What it achieved on the June 2026 engine

On the pre-parity engine (v0.7.2 era) the constraint was genuinely valuable:
F9P fix-scatter 17.5 → 8.8 cm (no receiver PCV) / 10.5 → 7.5 cm (correct PCV),
SF-mode 5.0 cm 3D vs the DF reference, DF regressions unchanged at sub-mm.

### A.3 Why it is not shipped — v0.7.4 subsumed it and inverted the storm case

Re-validated 2026-07-05 on the v0.7.4 engine (deterministic OpenBLAS builds,
clean-develop worktree comparator):

- **True SF**: clean develop 4.117 cm vs constraint-on 4.111 cm — Δ 0.006 cm.
  The iono Gauss-Markov decay (§2.2) already does the constraint's job.
- **F9P mismatch**: clean develop ≡ constraint-on within 0.055 cm 3D RMS,
  identical fix count. The June benefit had been largely absorbing the Galileo
  E5a PCV error that v0.7.4 fixed at the source.
- **Storm day (0627 / 2023 DOY309, production nf=3 config): −5.48 pp fix**
  (88.06 % constrained vs 93.54 % clean develop). Under disturbance the
  eligibility condition also catches DF satellites that transiently lose one
  band, and the constraint then pins a *moving* iono residual to zero at the
  worst possible time. The calm claslib suite (sub-mm deltas) cannot see this;
  only a disturbed-day A/B exposes it.

### A.4 Known defect recorded for any future revival

The constraint variance never actually received the STEC quality: **svar was 0
end-to-end on every dataset evaluated** (the URA term is not populated along
this path), so `var` collapsed to the fixed GRID_STD floor and did not loosen
under ionospheric disturbance. Any revival must first wire the STEC URA (or an
iono-activity proxy) into the variance — the storm harm in A.3 is exactly what
an un-inflated variance produces.
