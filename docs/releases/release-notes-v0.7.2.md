# Release Notes — v0.7.2

## CLAS PPP-RTK accuracy/robustness fixes and `mrtk dump --parse`

**Release date:** 2026-06-29
**Type:** Fix + feature (CLAS PPP-RTK accuracy/robustness + CSSR tooling)
**Branch:** `release/v0.7.2`

---

### Overview

v0.7.2 collects two CLAS PPP-RTK correctness fixes and adds a CSSR dump tool for
inspecting decoded CLAS L6D / Compact SSR:

- The PPP-RTK **adaptive-ionosphere process noise** is restored under storm-day
  conditions — a lost `Qp` scatter-back meant the adaptive noise was never
  actually applied. Storm-day CLAS fix rate improves from **77% to 85% (+8 pp)**.
- CLAS now **falls back to global orbit/clock corrections** when the
  close-network partition goes stale, instead of dropping corrections.
- `mrtk dump` gains a **`--parse`** mode that reproduces the upstream claslib
  `dumpcssr` per-subtype CSV output byte-identically.

The two fixes are CLAS PPP-RTK behavior changes with an accuracy/robustness
effect; the dump tool is a self-contained parser that leaves the production CLAS
decoder untouched. F5 (GSI daily-coordinate) absolute-accuracy regression tests
are added to guard geodetic accuracy going forward.

---

### 1. Bug fixes

#### PPP-RTK adaptive-ionosphere process noise

The compacted→full-`nx` `Qp` scatter-back had been lost, so the adaptive
ionosphere process noise was never applied to the filter. Restoring the
scatter-back makes the adaptive iono noise take effect again.

Storm-day CLAS fix rate improves from **77% to 85% (+8 pp)**
([#225](https://github.com/h-shiono/MRTKLIB/issues/225),
[PR #226](https://github.com/h-shiono/MRTKLIB/pull/226)).

#### CLAS stale network partition

When the close-network orbit/clock partition goes stale, CLAS now falls back to
the **global orbit/clock corrections** rather than dropping the corrections
entirely. This keeps a solution alive through a stale-partition window
([#218](https://github.com/h-shiono/MRTKLIB/issues/218),
[PR #228](https://github.com/h-shiono/MRTKLIB/pull/228)).

---

### 2. What's new

#### `mrtk dump --parse`

`mrtk dump` gains a `--parse` mode that dumps decoded CLAS L6D / Compact SSR to
the upstream-format per-subtype CSV files (`parse_cssr_type1..12*.csv` and
`parse_cssr_header.csv`).

It is implemented as a **self-contained vendored parser**
(`apps/dumpcssr/cssr_parse.c`) ported from claslib 082; the production CLAS
decoder is left untouched. The mode requires `-grid FILE`.

#### `mrtk dump -grid/--grid FILE`

The CLAS grid definition is now passed directly on the command line via `-grid`
(alias `--grid`), replacing the previous `-k`/`--config` file indirection.

---

### 3. Changed

- **`mrtk dump` grid input** — the grid definition is taken via `-grid` instead
  of the `-k`/`--config` file (which existed in `dump` only to read the grid
  path). The redundant config handling is removed.

---

### 4. Tests

CLAS **absolute-accuracy regression tests** are added: F5 (GSI daily-coordinate)
geodetic-truth checks for all CLAS PPP-RTK/VRS datasets (TSUKUBA3 / station
0627), guarding geodetic accuracy in addition to the existing
consistency-vs-claslib checks
([PR #227](https://github.com/h-shiono/MRTKLIB/pull/227)).

---

### 5. Validation

**`mrtk dump --parse` byte-identity.** The `--parse` output is byte-identical to
upstream claslib 082 `dumpcssr` / `ssr2osr -dump` on `2026121A.l6` across all
per-subtype CSVs (data fields); the only difference is an upstream version delta
in one header column label
([PR #230](https://github.com/h-shiono/MRTKLIB/pull/230)).

**CLAS PPP-RTK absolute accuracy.** 2D 95% ≈ **4.2–4.3 cm** vs GSI F5 truth
(TSUKUBA3, 2019/08/27).

**Storm-day fix rate.** CLAS PPP-RTK storm-day fix rate rises from **77% to 85%**
with the restored adaptive-iono `Qp` scatter-back.

---

### Compatibility & migration

The `mrtk dump` grid input changes from the `-k`/`--config` file to a direct
`-grid FILE` argument; scripts that relied on the config-file indirection for the
grid path must pass `-grid` instead. The PPP-RTK adaptive-iono and stale-partition
fixes are corrections to existing CLAS behavior and require no configuration
changes.

### Issues / PRs

- Release PR [#231](https://github.com/h-shiono/MRTKLIB/pull/231).
- [#225](https://github.com/h-shiono/MRTKLIB/issues/225) /
  [PR #226](https://github.com/h-shiono/MRTKLIB/pull/226) — restore adaptive-iono
  `Qp` scatter-back (storm 77%→85%).
- [#218](https://github.com/h-shiono/MRTKLIB/issues/218) /
  [PR #228](https://github.com/h-shiono/MRTKLIB/pull/228) — global orbit/clock
  fallback on stale network partition.
- [PR #227](https://github.com/h-shiono/MRTKLIB/pull/227) — F5 absolute-accuracy
  regression checks.
- [PR #230](https://github.com/h-shiono/MRTKLIB/pull/230) — `mrtk dump --parse`
  byte-identity validation.

### References

- claslib 082 — `dumpcssr` / `ssr2osr -dump` per-subtype CSV format
  (`parse_cssr_type1..12*.csv`, `parse_cssr_header.csv`).
- IS-QZSS-L6 — CLAS L6D Compact SSR message format.
- GSI F5 daily coordinates — geodetic-truth reference for the CLAS
  absolute-accuracy regression tests.
