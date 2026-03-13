# MRTKLIB v0.6.0 Release Notes

**Release date:** 2026-03-12
**Type:** Feature — Unified `mrtk` single CLI binary
**Branch:** `feat/unified-cli`

---

## Overview

v0.6.0 consolidates all eight MRTKLIB console applications into a single
BusyBox/Git-style binary called `mrtk`.  Instead of building separate
executables (`rnx2rtkp`, `rtkrcv`, `str2str`, `convbin`, `ssr2obs`, `ssr2osr`,
`recvbias`, `dumpcssr`), CMake now produces a single `mrtk` binary that
dispatches subcommands via `argv[1]`.

---

## Subcommand Mapping

| Original App | Subcommand | Function |
|--------------|------------|----------|
| `rtkrcv`     | `run`      | `mrtk_run()` |
| `rnx2rtkp`   | `post`     | `mrtk_post()` |
| `str2str`    | `relay`    | `mrtk_relay()` |
| `convbin`    | `convert`  | `mrtk_convert()` |
| `ssr2obs`    | `ssr2obs`  | `mrtk_ssr2obs()` |
| `ssr2osr`    | `ssr2osr`  | `mrtk_ssr2osr()` |
| `recvbias`   | `bias`     | `mrtk_bias()` |
| `dumpcssr`   | `dump`     | `mrtk_dump()` |

### Usage Examples

```bash
mrtk --help              # Global help
mrtk post -k conf.toml obs.obs nav.nav   # Post-processing
mrtk run -s -o rtkrcv.toml               # Real-time positioning
mrtk relay -in ntrip://... -out file://   # Stream relay
mrtk convert raw.ubx                     # Convert to RINEX
```

---

## Changes

### Added

| File | Description |
|------|-------------|
| `apps/mrtk/mrtk_main.c` | Unified CLI entry point with subcommand router and shared `showmsg`/`settspan`/`settime` implementation |

### Changed

| File | Change |
|------|--------|
| `apps/rnx2rtkp/rnx2rtkp.c` | `main()` → `mrtk_post()`; removed duplicate `showmsg`/`settspan`/`settime` |
| `apps/rtkrcv/rtkrcv.c` | `main()` → `mrtk_run()`; `static rtksvr_t svr` and `static rtcm_t rtcm[3]` (in `prstatus()`) converted to heap allocation to eliminate ~1.9 GB BSS |
| `apps/str2str/str2str.c` | `main()` → `mrtk_relay()` |
| `apps/convbin/convbin.c` | `main()` → `mrtk_convert()`; removed duplicate `showmsg` |
| `apps/ssr2obs/ssr2obs.c` | `main()` → `mrtk_ssr2obs()` |
| `apps/ssr2osr/ssr2osr.c` | `main()` → `mrtk_ssr2osr()`; removed duplicate `showmsg`/`settspan`/`settime` |
| `apps/recvbias/recvbias.c` | `main()` → `mrtk_bias()`; `static rtcm_t rtcm` converted to heap allocation (~314 MB BSS eliminated) |
| `apps/dumpcssr/dumpcssr.c` | `main()` → `mrtk_dump()`; removed duplicate `showmsg`/`settspan`/`settime` |
| `src/pos/mrtk_postpos.c` | `static rtcm_t rtcm` and `static rtcm_t l6e` converted to heap allocation (~628 MB BSS eliminated) |
| `CMakeLists.txt` | 8 `add_executable()` targets replaced by single `add_executable(mrtk ...)`; all CTest commands updated; version bumped to 0.6.0 |
| `tests/cmake/run_rtkrcv_test.sh` | Added `$2` (subcmd) argument to support `mrtk run` invocation |

### Removed

- 8 individual build targets (`rnx2rtkp`, `rtkrcv`, `str2str`, `convbin`, `ssr2obs`, `ssr2osr`, `recvbias`, `dumpcssr`)
- Duplicate `showmsg`/`settspan`/`settime` definitions from 4 app source files (consolidated into `mrtk_main.c`)

---

## BSS Reduction

Converting large static variables to heap allocation dramatically reduced the
binary's data segment:

| Variable | File | BSS Size | Fix |
|----------|------|----------|-----|
| `static rtksvr_t svr` | `rtkrcv.c` | 972 MB | `calloc` in `mrtk_run()` |
| `static rtcm_t rtcm[3]` | `rtkrcv.c:prstatus()` | 943 MB | lazy `calloc` |
| `static rtcm_t rtcm` | `recvbias.c` | 314 MB | `calloc` in `mrtk_bias()` |
| `static rtcm_t rtcm` | `mrtk_postpos.c` | 314 MB | `calloc` on use |
| `static rtcm_t l6e` | `mrtk_postpos.c` | 314 MB | `calloc` on use |

**`__DATA` segment: 3,032 MB → 34 MB** (99% reduction)

---

## Scope

12 files changed, +234 / -254 lines (net -20 lines).

---

## Test Results

All 62 tests pass (59 non-RT + 3 RT), no regressions.
