# MRTKLIB v0.5.4 Release Notes

**Release date:** 2026-03-11
**Type:** Architecture / signals redesign
**Branch:** `feat/signals-architecture`

---

## Overview

v0.5.4 introduces a structured signals architecture that replaces the legacy
string-based `codepris[]` system.  The new `mrtk_band_t` enum defines 26
per-constellation physical frequency bands, and a structured priority table
(`SIG_PRIORITY_TABLE`) serves as the single source of truth for signal selection.

TOML configs gain explicit `signals = ["G1C", "G2W", "E1C", "E5Q"]` arrays,
replacing the opaque `frequency = "l1+2"` strings.

No algorithmic changes; all positioning output is identical to v0.5.3.

---

## Changes

### Added — `mrtk_band_t` enum and structured signal priority

- **`mrtk_band_t` enum** (`mrtk_foundation.h`) — 26 per-constellation physical
  frequency bands:
  - GPS: L1, L2, L5
  - GLONASS: G1, G2, G3
  - Galileo: E1, E5a, E5b, E6, E5ab
  - QZSS: L1, L2, L5, L6
  - SBAS: L1, L5
  - BDS: B1I, B1C, B2a, B2b, B2ab, B3
  - NavIC: L5, S
  - Plus `MRTK_BAND_UNKNOWN` and `MRTK_BAND_MAX` sentinels

- **`SIG_PRIORITY_TABLE`** (`mrtk_obs.c`) — 26-entry structured priority table
  using C99 designated initializers and `CODE_*` constants

- **New API functions** (`mrtk_obs.h`):
  - `mrtk_rinex_freq_to_band()` — RINEX freq digit → band (incl. GLO CDMA aliases)
  - `mrtk_get_signal_priority()` — (sys, band) → priority-ordered code array
  - `mrtk_band2freq_hz()` — band → carrier frequency (Hz)
  - `mrtk_band_to_freq_num()` — band → RINEX freq number (reverse mapping)
  - `mrtk_parse_signal_str()` — parse "G1C" → (SYS_GPS, MRTK_BAND_GPS_L1, CODE_L1C)
  - `mrtk_sigcfg_from_signals()` — build per-system signal config from string array
  - `mrtk_sigcfg_to_obsdef()` — bridge signal config to obsdef tables

- **New types** (`mrtk_obs.h` / `mrtk_opt.h`):
  - `mrtk_signal_t` — (band, preferred_code) pair
  - `mrtk_sigcfg_t` — per-constellation signal config
  - `prcopt_t.sigcfg[7]` / `sigcfg_set` — processing option fields

- **TOML `signals` array** (`mrtk_toml.c`) — `[positioning] signals` key accepts
  TOML string arrays, auto-deriving `nf` from configured signals

### Changed — getcodepri / code2idx rewrite

- **`getcodepri()`** — Rewritten to use `mrtk_rinex_freq_to_band()` +
  `mrtk_get_signal_priority()` instead of legacy `codepris[]` string scan
- **`code2idx()`** — Rewritten as 3-stage pipeline:
  `code2freq_num()` → `mrtk_rinex_freq_to_band()` → `band2idx_fixed()`
- **obsdef tables** (`mrtk_nav.c`) — Annotated with `mrtk_band_t` names in
  comments (e.g., `/* 0:L1 (MRTK_BAND_GPS_L1) */`)
- **CLASLIB/benchmark TOML configs** — Migrated from `frequency` to `signals`
  arrays (11 files); old `frequency` lines commented out for reference

### Removed — Legacy signal infrastructure

- `codepris[7][MAXFREQ][16]` — string-based signal priority array
- `setcodepri()` / `get_codepris()` — legacy priority access functions
- `obsdef_t.codepris` field — removed from struct and all 8 obsdef initializers
- 7 × `code2freq_*()` functions — replaced by band-based lookup pipeline

### Fixed — Code review findings

- `buff2sysopts()` nf validation — reject `nf > NFREQ` from signal config
- `code2idx()` SYS_BD2 normalization — consistent with `getcodepri()`
- `toml_val_to_str()` snprintf buffer overflow — clamp advance on truncation
- `NUM_PRIORITY_ENTRIES` hardcoded constant — replaced with `sizeof`-based count
- Docstring corrections — signal format is "G1C" (not "GL1C")

---

## Known Limitations

### `positioning.signals` and `[signals]` section conflict

Configs that use the MADOCALIB per-system signal selection (`[signals] gps`,
`galileo`, `bds2`, `bds3`, etc., mapped to `pos2-sig*`) cannot use
`positioning.signals` at the same time.  Both mechanisms call `set_obsdef()`,
and the `[signals]` section overrides the obsdef ordering set by
`positioning.signals`, causing incorrect frequency assignment.

**Affected configs:** MADOCALIB, MALIB, and benchmark/madoca TOML files retain
`frequency` for now.  A future release will unify these two mechanisms.

---

## Scope

| Category | Files changed | Description |
|----------|---------------|-------------|
| Core API | 4 | mrtk_foundation.h, mrtk_obs.h, mrtk_opt.h, mrtk_nav.h |
| Implementation | 3 | mrtk_obs.c, mrtk_nav.c, mrtk_toml.c |
| Options | 1 | mrtk_options.c |
| TOML configs | 11 | CLASLIB + benchmark configs migrated to `signals` |

---

## Test Results

56/56 non-realtime tests pass — unchanged from v0.5.3.

| Test Suite | Count | Status |
|------------|-------|--------|
| Unit tests | 12 | PASS |
| SPP / receiver bias | 4 | PASS |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 | PASS |
| CLAS PPP-RTK + VRS-RTK | 19 | PASS |
| ssr2obs / ssr2osr / BINEX | 4 | PASS |
| Tier 2 absolute accuracy | 2 | PASS |
| Tier 3 position scatter | 2 | PASS |
| Fixtures | 3 | PASS |
