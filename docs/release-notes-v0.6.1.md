# MRTKLIB v0.6.1 Release Notes

**Release date:** 2026-03-12
**Type:** Enhancement — TOML configuration UX improvements
**Branch:** `feat/config-systems`

---

## Overview

v0.6.1 improves the TOML configuration experience with human-readable string
lists for constellation selection (`systems`) and satellite exclusion
(`excluded_sats`), reorganizes config sections for clarity, and introduces the
`taplo` TOML formatter with VSCode integration.

---

## Changes

### Added

| File | Description |
|------|-------------|
| `src/core/mrtk_toml.c` | `navsys_stricmp()` + `navsys_str2mask()` — case-insensitive constellation name → bitmask conversion |
| `src/core/mrtk_toml.c` | Post-processing block for `positioning.systems` string list (overrides `constellations`) |
| `src/core/mrtk_toml.c` | Post-processing block for `positioning.excluded_sats` as string list (joins to space-separated) |
| `taplo.toml` | taplo formatter configuration (align comments, no key padding, preserve key order) |
| `.vscode/settings.json` | TOML format-on-save with Even Better TOML extension |
| `.vscode/extensions.json` | `tamasfe.even-better-toml` added to recommended extensions |

### Changed

| File | Change |
|------|--------|
| `conf/**/*.toml` (20 files) | `constellations = N` → `systems = ["GPS", ...]` string list |
| `conf/**/*.toml` (20 files) | Formatted with `taplo` (key padding removed, comments aligned) |
| `conf/claslib/*.toml`, `conf/madocalib/*.toml` | `tidal_correction` moved from `[positioning.atmosphere]` to `[positioning.corrections]` |
| `scripts/tools/conf2toml.py` | Outputs `systems` list instead of `constellations` integer; added `siglist` type; removed key padding; removed `_navsys_comment()` |

### Removed

| File | Change |
|------|--------|
| `conf/malib/rnx2rtkp.toml` | `[unknown]` section with obsolete `pos2-sig*` per-satellite-type signal options |
| `conf/malib/rtkrcv.toml` | `[unknown]` section with obsolete `pos2-sig*` per-satellite-type signal options |

---

## Configuration Examples

```toml
# Before (v0.6.0)
constellations = 25  # which systems is this?

# After (v0.6.1) — either format works
systems = ["GPS", "Galileo", "QZSS"]  # human-readable
constellations = 25                    # still supported (systems takes priority)

# Satellite exclusion — either format works
excluded_sats = ["G01", "G02", "+E05"]  # new: string list
excluded_sats = "G01 G02 +E05"          # legacy: space-separated string
```

---

## Related Issues

- [#59](https://github.com/h-shiono/MRTKLIB/issues/59) — Restore per-satellite-type GPS signal selection from MALIB (tracked for future release)

---

## Scope

24 files changed across 6 commits.

---

## Test Results

All 62 tests pass (59 non-RT + 3 RT), no regressions.
