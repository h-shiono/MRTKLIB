# MRTKLIB v0.5.0 Release Notes

**Release date:** 2026-03-10
**Type:** Infrastructure (TOML configuration migration)
**Branch:** `feat/toml-config`

---

## Overview

v0.5.0 replaces the legacy RTKLIB `key=value` `.conf` configuration format with
**TOML v1.0**.  All 19 configuration files have been converted, the C loader
auto-detects `.toml` files, and the legacy `.conf` files are removed.

The migration is **zero-regression**: all 59 tests pass with bit-identical
positioning output.

### Highlights

- **TOML v1.0 parser** — [tomlc99](https://github.com/cktan/tomlc99) (MIT, C99)
  vendored at `src/core/tomlc99/`.  No external dependency required.
- **Semantic grouping** — Options are organised by function (`[positioning]`,
  `[ambiguity_resolution]`, `[kalman_filter]`, …) instead of the legacy
  Windows-GUI-derived `pos1-`, `pos2-`, `stats-` prefixes.
- **Python converter** — `scripts/tools/conf2toml.py` converts any existing
  `.conf` file to `.toml` with full enum resolution and rtkrcv stream support.
- **Backward compatible loader** — `loadopts()` auto-detects file extension;
  `.toml` dispatches to the new TOML loader, other extensions use the legacy parser.
- **19 TOML configs** — claslib (9), madocalib (3), malib (2), benchmark (5).

---

## TOML Section Structure

Legacy flat keys are mapped to semantic TOML sections:

| TOML Section | Legacy Prefix | Description |
|-------------|---------------|-------------|
| `[positioning]` | `pos1-*` | Mode, frequencies, dynamics, tidal correction |
| `[ambiguity_resolution]` | `pos2-*` | AR mode, thresholds, GLONASS/BDS AR |
| `[kalman_filter]` | `stats-*` | Process noise, initial variance |
| `[rejection]` | `pos2-*` | Observation rejection thresholds |
| `[slip_detection]` | `pos2-*` | Cycle-slip detectors (LLI, geometry-free, Doppler) |
| `[adaptive_filter]` | `pos2-*` | Adaptive Kalman filter parameters |
| `[signals]` | `pos1-*` | Signal selection, L2 priority, code observation |
| `[receiver]` | `pos1-*` | Receiver type, ISB, STEC model |
| `[antenna.rover]` / `[antenna.base]` | `ant1-*`, `ant2-*` | Antenna type, delta, PCV |
| `[output]` | `out-*` | Solution format, time format, field separator |
| `[files]` | `file-*` | Satellite/antenna/geoid/ionosphere files |
| `[streams.*]` | `inpstr*`, `outstr*`, `logstr*` | rtkrcv stream configuration |
| `[console]` | `console-*` | rtkrcv console settings |
| `[server]` | `misc-*` | rtkrcv server miscellaneous |

---

## Example: Before and After

### Legacy `.conf`
```ini
pos1-posmode       =ppp-rtk
pos1-frequency     =l1+2+5
pos1-soltype       =forward
pos2-armode        =continuous
pos2-arthres       =3
stats-eratio1      =300
ant1-anttype       =AOAD/M_T
out-solformat      =nmea
```

### TOML `.toml`
```toml
[positioning]
mode = "ppp-rtk"
frequency = "l1+2+5"
solution_type = "forward"

[ambiguity_resolution]
mode = "continuous"
ratio_threshold = 3.0

[kalman_filter]
carrier_to_code_ratio_L1 = 300.0

[antenna.rover]
type = "AOAD/M_T"

[output]
solution_format = "nmea"
```

---

## Architecture

### C Loader (`src/core/mrtk_toml.c`)

The TOML loader maintains a mapping table (`toml_map_t[]`) that associates each
TOML path (e.g., `"positioning"`, `"mode"`) with the legacy option name
(e.g., `"pos1-posmode"`).  For each mapping entry:

1. `navigate_table()` walks the TOML tree to the target section
2. `toml_val_to_str()` reads the TOML value (string/bool/int/double/array) as
   a C string
3. `searchopt()` + `str2opt()` apply the value through the existing option
   infrastructure

This design ensures **zero changes** to the positioning engine — the TOML loader
is purely a configuration front-end.

### Python Converter (`scripts/tools/conf2toml.py`)

```
Usage: python scripts/tools/conf2toml.py [conf_files...]
       python scripts/tools/conf2toml.py conf/**/*.conf   # batch
```

Features:
- 195-entry mapping table (sysopts + rtkrcv stream/console keys)
- Enum resolution: bare integers → named strings (e.g., `3` → `"solid+otl-clasgrid+pole"`)
- Stream key pattern matching (inpstr, outstr, logstr, cmdfile)
- Console and server misc key support

### Auto-detection in `loadopts()`

```c
ext = strrchr(file, '.');
if (ext && strcmp(ext, ".toml") == 0) {
    return loadopts_toml(file, opts);
}
// else: legacy parser
```

---

## Verification

- **Bit-identical output**: CLAS PPP-RTK (7,160-line NMEA) produces 0 diff
  between `.conf` and `.toml` runs.
- **59/59 tests pass**: 56 non-realtime + 3 realtime (MADOCA, CLAS 1ch, CLAS 2ch).
- **No mathematical changes**: The TOML loader produces identical `opt_t` values
  as the legacy parser.

---

## Files Changed (52 files, +6,732 / −2,382)

| File | Change |
|------|--------|
| `src/core/tomlc99/` | Vendored tomlc99 (toml.c, toml.h, LICENSE) |
| `src/core/mrtk_toml.c` | TOML loader (535 lines) |
| `include/mrtklib/mrtk_toml.h` | Public API (loadopts_toml, saveopts_toml) |
| `src/pos/mrtk_options.c` | .toml auto-detection in loadopts() |
| `scripts/tools/conf2toml.py` | Python conf→TOML converter (654 lines) |
| `conf/**/*.toml` | 19 new TOML config files |
| `conf/**/*.conf` | 19 deleted legacy config files |
| `CMakeLists.txt` | CTest references switched to .toml |
| `tests/cmake/run_rtkrcv_test.sh` | TOML-aware config patching |
| `scripts/benchmark/run_benchmark.py` | .conf → .toml references |
| `tests/data/*/generate_reference*.sh` | .conf → .toml references |
| `vcpkg.json` | Removed unused toml11 dependency |

---

## Test Suite (59 tests)

| Test Suite | Tests |
|------------|-------|
| Unit tests | 12 |
| SPP / receiver bias | 4 |
| rtkrcv real-time (MADOCA + CLAS 1ch + CLAS 2ch) | 3 |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 |
| CLAS PPP-RTK + VRS-RTK | 19 |
| ssr2obs / ssr2osr / BINEX | 4 |
| Tier 2 absolute accuracy | 2 |
| Tier 3 position scatter | 2 |
| Fixtures | 3 |
