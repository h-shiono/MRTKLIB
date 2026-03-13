# MRTKLIB v0.5.7 Release Notes

**Release date:** 2026-03-12
**Type:** Feature — Port RTKLIB `convbin` and `str2str` CLI applications
**Branch:** `feat/convbin-str2str`

---

## Overview

v0.5.7 ports the two remaining RTKLIB console applications — `convbin` (RINEX
converter) and `str2str` (stream relay/converter) — into the MRTKLIB build
system.  Both executables are fully modernized: WIN32-specific code has been
removed, POSIX-only paths are retained, and all source files have been formatted
with `clang-tidy` (mandatory braces) and `clang-format` (Google style).

---

## Changes

### Added

| File | Description |
|------|-------------|
| `apps/convbin/convbin.c` | CLI application to convert receiver binary log files to RINEX obs/nav and SBAS message files |
| `apps/str2str/str2str.c` | CLI application for stream-to-stream data relay with optional format conversion |
| `src/data/mrtk_convrnx.c` | Core RINEX translation logic for RTCM and receiver raw data (library module) |
| `src/stream/mrtk_streamsvr.c` | Stream server functions for data relay and format conversion (library module) |

### Changed

| File | Change |
|------|--------|
| `CMakeLists.txt` | Added `convbin` and `str2str` executable targets; added `mrtk_convrnx.c` and `mrtk_streamsvr.c` to `mrtklib` library sources |

### Removed

- **WIN32 code** — All `#ifdef WIN32` / `#ifndef WIN32` preprocessor blocks and
  Windows-specific includes (`winsock2.h`, `WSAStartup`, etc.) have been removed.
  Only POSIX (Linux/macOS) code paths are retained.
- **Constellation enable macros** — `#ifdef ENAGLO`, `ENAGAL`, `ENACMP`, etc.
  guards have been removed; all constellations are unconditionally enabled.

---

## Modernization Details

The following transformations were applied to the upstream RTKLIB source:

1. **Header includes** — `#include "rtklib.h"` replaced with
   `#include "mrtklib/rtklib.h"` (MRTKLIB compatibility wrapper).
2. **License headers** — Upstream copyright blocks replaced with MRTKLIB
   BSD-2-Clause license headers.  Original version/history information is
   preserved as Doxygen docstrings (`/** ... */`).
3. **Code formatting** — `clang-tidy` applied for mandatory `{}` on all
   control flow statements; `clang-format` applied with Google style.

---

## Scope

5 files changed, +3,216 / -1 lines.

| File | Lines |
|------|-------|
| `apps/convbin/convbin.c` | 663 |
| `apps/str2str/str2str.c` | 387 |
| `src/data/mrtk_convrnx.c` | 1,363 |
| `src/stream/mrtk_streamsvr.c` | 788 |
| `CMakeLists.txt` | +15 |

---

## Test Results

59/59 tests pass (no regressions).

| Test Suite | Count | Status |
|------------|-------|--------|
| Unit tests | 15 | PASS |
| SPP / receiver bias | 4 | PASS |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 | PASS |
| CLAS PPP-RTK + VRS-RTK | 19 | PASS |
| ssr2obs / ssr2osr / BINEX | 4 | PASS |
| Tier 2 absolute accuracy | 2 | PASS |
| Tier 3 position scatter | 2 | PASS |
| Fixtures | 3 | PASS |
