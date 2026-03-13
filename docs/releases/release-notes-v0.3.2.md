# MRTKLIB v0.3.2 Release Notes

**Release date:** 2026-03-06
**Type:** Patch (test infrastructure improvement — no functional changes)

---

## Overview

v0.3.2 expands the regression test suite from 53 to **57 tests** by introducing
a three-tier test methodology that adds absolute geodetic accuracy (Tier 2) and
position precision (Tier 3) checks alongside the existing relative porting
correctness tests (Tier 1). Python tooling is cleaned up and standardised with
Ruff linting.

**No library code, algorithms, or public API changed in this release.**

---

## What Changed

### Three-Tier Test Methodology

A new framework document ([docs/test-accuracy-methodology.md](../reference/test-accuracy-methodology.md))
defines three complementary validation layers:

| Tier | Measures | Reference |
|------|----------|-----------|
| **Tier 1** — Relative | MRTKLIB vs upstream output (same input) | Pre-committed upstream `.pos` / `.nmea` |
| **Tier 2** — Absolute | MRTKLIB vs surveyed ground truth | IGS SINEX or GSI F5 daily coordinates |
| **Tier 3** — Precision | Solution scatter around session centroid | None (centroid is self-referential) |

### Tier 2 — Absolute Accuracy Tests (2 new tests)

Two new CTest entries validate geodetic accuracy against independent reference
coordinates:

| CTest Name | Reference | Tolerance | Metric | Notes |
|------------|-----------|-----------|--------|-------|
| `madocalib_pppar_abs_check` | IGS SINEX MIZU (GPS week 2383) | 0.100 m | 2D horiz | 60-epoch skip |
| `claslib_ppp_rtk_2ch_abs_check` | GSI F5 TSUKUBA3 2025/06/06 | 0.300 m | 2D horiz | ±7-day median |

**Reference source details:**

- *IGS SINEX* — Station coordinate from `+SOLUTION/ESTIMATE` block with optional
  velocity propagation to the observation date. Formal precision = σ₃D =
  √(σ_X² + σ_Y² + σ_Z²).
- *GSI F5* — 15-day median of daily ECEF coordinates (D−7 … D+7). Reference
  precision = 68th-percentile of 3D scatter within the window.

New scripts: `scripts/tests/compare_pos_abs.py`, `scripts/tests/compare_nmea_abs.py`

### Tier 3 — Position Scatter Tests (2 new tests)

Two new CTest entries check solution *precision* (repeatability) without any
external reference:

| CTest Name | Input | Tolerance | skip-epochs | Actual |
|------------|-------|-----------|-------------|--------|
| `madocalib_ppp_scatter` | `out_madocalib_ppp.pos` | 0.150 m | 30 | 1σ=7.4 cm, 95%=11.2 cm |
| `claslib_ppp_rtk_2ch_scatter` | `out_claslib_ppp_rtk_2ch.nmea` | 0.400 m | 20 | 1σ=5.8 cm, 95%=25.2 cm |

The algorithm computes the ECEF session centroid and measures per-epoch ENU
deviation. Pass criteria: both 1σ (68th percentile) and 95th percentile of 2D
horizontal scatter must be below the tolerance.

New script: `scripts/tests/check_pos_scatter.py`

### Python Script Tooling

- **Ruff linting** — `ruff.toml` added at project root (analogous to
  `.clang-format` for C/C++); `scripts/pyproject.toml` retains only package
  dependency metadata. All scripts pass `ruff check` with `E`, `F`, `W`, `I`,
  `D` rule sets.
- **VSCode integration** — `.vscode/settings.json` added with Ruff
  format-on-save for Python files and interpreter path pointing to
  `scripts/.venv`.
- **Script cleanup:**
  - Deleted: `fix_trace_fwd_decls.py`, `migrate_trace_ctx.py` (one-time
    migration tools, no longer needed)
  - Deleted: `plotting/plot_lex_{eph,ion,ure}.py` (LEX service retired 2020)
  - Moved: `tests/plot_{gloeph,igp,peph}.py` → `plotting/` (analysis tools,
    not test runners)
- **Shared geodetic helpers** — `scripts/tests/_geo.py` consolidates `blh2xyz`,
  `xyz2blh`, `xyz2enu`, and `nmea_to_deg` that were duplicated across four test
  scripts. Single authoritative implementation with convergence-checked
  `xyz2blh` (early exit at 1×10⁻¹² rad).

---

## Bug Fixes

- **NMEA height recovery** — `compare_nmea_abs.py` now correctly recovers
  ellipsoidal height as `h_ell = field[9] + field[11]` (MSL + geoid
  separation). Previously field[11] was not summed, causing ~40 m Up errors
  for MRTKLIB-generated GGA files.
- **Absolute check 2D mode** — `compare_pos_abs.py` `--use-2d` flag now
  consistently applies to both the reported metric and the pass/fail criterion.

---

## All 57 Tests Pass

| Test Suite | Tests | Notes |
|------------|-------|-------|
| Unit tests | 12 | — |
| SPP regression | 2 | — |
| Receiver bias | 2 | — |
| rtkrcv real-time | 1 | — |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 | unchanged from v0.3.1 |
| CLAS PPP-RTK (single-ch / dual-ch / ST12 / L1CL5) | 10 | unchanged |
| CLAS VRS-RTK (single-ch / ST12 / dual-ch) | 9 | unchanged |
| ssr2obs / ssr2osr | 3 | unchanged |
| BINEX reading | 1 | unchanged |
| **Tier 2 absolute accuracy** | **2** | new in v0.3.2 |
| **Tier 3 position scatter** | **2** | new in v0.3.2 |
| Fixtures (setup/cleanup/download) | 3 | — |
