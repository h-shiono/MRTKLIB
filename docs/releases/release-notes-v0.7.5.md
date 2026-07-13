# Release Notes — v0.7.5

## Single-band CLAS PPP-RTK — regression coverage and capability record

**Release date:** 2026-07-06
**Type:** Test coverage + capability record (no positioning change)
**Branch:** `release/v0.7.5`

---

### Overview

v0.7.5 is **not a feature release** — there are **no positioning-code changes**.
It locks in two single-band CLAS PPP-RTK configurations that the v0.7.4 engine
already handles, backing each with a regression test anchored to **independent
truth**, and records the mechanics in a new design note.

The v0.7.4 engine already positions these configurations correctly without any
dedicated single-frequency machinery: the ionosphere Gauss-Markov decay bounds
the unobservable per-satellite iono residual, so a single-band solution stays
well-behaved. This release captures that behavior as tests so it cannot silently
regress, and documents *why* it works.

The two configurations are:

1. **Single-frequency (nf=1) CLAS PPP-RTK.**
2. **Galileo E1/E5b band mismatch** — u-blox ZED-F9P-class receivers tracking
   E1+E5b under CLAS E5a-only biases.

---

### 1. Regression coverage for single-band CLAS PPP-RTK

Two new claslib test triples lock in the v0.7.4 behavior. Each triple pairs a
consistency reference with an absolute check against independent truth.

#### Single-frequency (nf=1)

- New config `conf/claslib/rnx2rtkp_sf.toml`.
- **2019/239** dataset, validated against **GSI F5 truth**.
- Absolute accuracy: **2D 1σ 2.5 cm / 95 % 3.9 cm**.

#### Galileo E1/E5b band mismatch

- ZED-F9P-class receivers track E1+E5b, but CLAS carries E5a-only biases, so the
  mismatched satellites run **E1-only**.
- Self-collected **ECJ02** dataset, validated against an **independent
  IGS-products coordinate**.
- Absolute accuracy: **3D 1σ 18.5 cm / 95 % 20.6 cm**.
- The E1-only satellites bound the solution at **dm-level** — this is **not**
  DF-equivalent accuracy, and the test enforces that bound rather than a
  dual-frequency target.

---

### 2. Tooling

`compare_nmea_abs.py` gains a `--llh` fixed-coordinate option, so an absolute
accuracy check can be anchored to a supplied lat/lon/height (the ECJ02
IGS-products coordinate) rather than an averaged solution.

---

### 3. Design record

`docs/design/sf-ppp-rtk.md` records:

- The mechanics of single-band CLAS PPP-RTK, and the dependency on the
  ionosphere Gauss-Markov decay that bounds the unobservable iono residual.
- A post-mortem of the explicit SF iono constraint that was **prototyped**, then
  **subsumed by the v0.7.4 engine changes**, and **deliberately not shipped**.

---

### Compatibility & migration

No configuration changes are required for existing workflows; the new
`conf/claslib/rnx2rtkp_sf.toml` is additive. There are **no positioning-algorithm,
matrix, physical-constant, or test-tolerance changes** — v0.7.5 only adds tests,
one tooling option, and documentation.

### Issues / PRs

- [PR #253](https://github.com/h-shiono/MRTKLIB/pull/253) — regression coverage,
  `compare_nmea_abs.py --llh`, and `docs/design/sf-ppp-rtk.md`.
- [PR #254](https://github.com/h-shiono/MRTKLIB/pull/254) — release.

### References

- `docs/design/sf-ppp-rtk.md` — single-band CLAS PPP-RTK behavior record and the
  not-shipped SF iono-constraint post-mortem.
