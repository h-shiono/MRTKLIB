# Release Notes — v0.7.11

## MADOCA regression tests: build-independent upstream parity + absolute accuracy

**Release date:** 2026-09-29
**Type:** Test infrastructure — **no positioning change** (no engine, decoder or numerical code changed)
**Branch:** `release/v0.7.11`

---

### Overview

v0.7.11 is **not a feature release**. It fixes the MADOCA regression tests so
the full suite passes on every supported build, and gives every MADOCA case
the same two-tier check the CLAS tests already have: **upstream parity**
(port fidelity) plus **absolute accuracy** against independent truth, which
stays meaningful as MRTKLIB diverges from or improves on upstream.

`madocalib_pppar_ion_check` passed when its 5 mm tolerance was introduced in
v0.3.1 (0.25 cm), but from v0.6.6 onward it failed on every build that links
LAPACK (Accelerate on macOS), and release notes carried it as a known
"environment-only" failure. CI stayed green only because it installs no BLAS
and CMake then switched the same test to an 8× looser tolerance. With this
release the suite is **125 / 125** on macOS and the strict gate runs in CI.

### Root cause ([#302](https://github.com/h-shiono/MRTKLIB/issues/302))

Measured on macOS with upstream MADOCALIB and MRTKLIB each built against
internal LU, Accelerate and OpenBLAS:

- **Not nondeterminism.** Every build is bit-identical run-to-run.
- **Not an MRTKLIB regression.** Against a same-backend upstream build MRTKLIB
  differs by 0.236 cm, matching what was recorded when the reference was
  introduced.
- **A stale, backend-specific reference.** The `pppar` / `pppar_ion`
  references came from an upstream Accelerate build that no longer reproduces
  on the current toolchain (today's upstream Accelerate build is 1.62 cm away,
  one fix/float flip at 00:29).
- **A metric that cannot tolerate any backend change.** This one-hour dataset
  has several epochs where PPP-AR sits at the ratio-test margin; a ULP-level
  difference flips fix ↔ float there, and one flip is 17–24 cm. Pinning both
  sides to OpenBLAS still fails (2.2 cm), because port and upstream are
  different compiled code. At 00:29 the fixed solution is closer to SINEX
  truth than the reference's float (2D 4.1 vs 9.6 cm), so the old gate
  penalised the better answer.

### Changes

1. **References regenerated from upstream as shipped** — internal LU, no
   `-DLAPACK`; platform-independent and what CI builds. The generator was
   passing MRTKLIB TOML to upstream `rnx2rtkp` (which reads only `key = value`
   `.conf`); upstream-format configs are restored in
   `tests/data/madocalib/upstream_conf/`, and each regeneration writes
   `reference_provenance.txt` (upstream version and commit, backend, flags, compiler,
   platform). `pp` / `pppar_003` are byte-identical; `pppar` / `pppar_ion`
   moved by 1.51 / 3.78 cm (3D RMS).
2. **Parity tolerances are the same in every build** — the `LAPACK_FOUND`
   switch is gone.
3. **Integer-fix-rate gate for PPP-AR parity.** `compare_pos.py` counted
   float PPP (Q=6) as fixed and computed the rate over the ref/test
   intersection, so neither a fix→float drop nor a lost epoch was visible.
   PPP-AR checks now count Q=1 only over the reference timeline (a missing
   test epoch is not fixed), one-sided at −5 %. New options `--fix-q` and
   `--max-fix-drop`; defaults preserve the old behaviour for other callers.
4. **Absolute checks for all four MADOCA cases** against a MIZU-only extract
   of the week-2360 IGS SINEX (2025/03/30–04/05, contains the data epoch;
   977 bytes). It replaces the 15 MB week-2383 file, taken five months after
   the data without velocity propagation (MIZU moved 2.26 cm in between —
   about half the previously reported PPP-AR error).
5. **CI pins the matrix backend** (`-DCMAKE_DISABLE_FIND_PACKAGE_LAPACK=TRUE`
   in the regression job) and configure prints `Matrix backend: …`.

### Tolerance changes

| Test | Before | After | Measured worst (LU / Accelerate / OpenBLAS) |
|---|---:|---:|---|
| `madocalib_pppar_check` | 0.008 m (LAPACK) / 0.020 m | 0.020 m | 1.563 cm |
| `madocalib_pppar_ion_check` | 0.005 m (LAPACK) / 0.040 m | 0.050 m | 3.397 cm |
| PPP-AR fix rate (3 checks) | Q ∈ {1,6}, −1 %, intersection | Q = 1, −5 %, reference timeline | −3.39 % |
| `madocalib_ppp_abs_check` | — | 0.50 m | 24.36 cm (2D 95 %) |
| `madocalib_pppar_abs_check` | 0.100 m (week 2383) | 0.06 m (week 2360) | 2.66 cm |
| `madocalib_pppar_003_abs_check` | — | 0.08 m | 3.65 cm |
| `madocalib_pppar_ion_abs_check` | — | 0.06 m | 2.75 cm |

The parity loosening is offset by the new fix-rate gate and absolute checks:
removing 7 fixed epochs from a `pppar_ion` output, or turning them into float,
passed the old checks (+0.00 %) and fails the new one (−5.93 %).

### Verification

- MADOCA tests (15) pass on macOS internal-LU, Accelerate and OpenBLAS
  (`MRTK_DETERMINISTIC_BLAS`) builds.
- Full suite on macOS (Accelerate): **125 / 125**.
- Linux CI (internal LU, pinned): 120 / 120 (`-LE realtime`).
- No positioning code changed.

### Related issues

- [#302](https://github.com/h-shiono/MRTKLIB/issues/302) — fixed
  ([PR #338](https://github.com/h-shiono/MRTKLIB/pull/338)).
- [#300](https://github.com/h-shiono/MRTKLIB/issues/300) — multi-dimensional
  acceptance framework; the CLAS `compare_nmea.py` fix rate has the same
  "counts non-RTK-fix as fix" weakness and is left for that work.

### Upgrade notes

None for library or CLI users. Contributors: `madocalib_pppar_ion_check` is
no longer an expected failure — treat any MADOCA check failure as real.
