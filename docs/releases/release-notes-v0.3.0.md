# MRTKLIB v0.3.0 Release Notes

**Release date:** 2026-03-06
**Branch merged:** `feat/integrate-claslib` → `develop`
**Upstream reference:** CLASLIB ver.0.8.2 (`9e714b9`)

---

## Overview

v0.3.0 completes the CLASLIB integration, bringing full QZSS CLAS L6D augmentation
support to MRTKLIB. This release adds two new positioning engines (PPP-RTK and
VRS DD-RTK), a complete CSSR decoder for IS-QZSS-L6-007 two-channel L6 input,
and three utility applications for SSR/OSR processing and stream inspection.

The test suite grows from 30 to **53 tests**, all passing.

---

## What's New

### CLAS Positioning Engines

| Engine | Mode | 3D RMS | Fix Rate | Dataset |
|--------|------|--------|----------|---------|
| PPP-RTK | Single-channel L6 | 5.9 cm | 99.86% | DOY 239/2019 |
| PPP-RTK | Dual-channel ST12 | 10.8 cm | 98.75% | DOY 349/2019 |
| PPP-RTK | Dual-channel (2025) | ~15.6 cm | — | DOY 157/2025 |
| PPP-RTK | L1+L5 (2025) | ~10.0 cm | — | DOY 022/2025 |
| VRS DD-RTK | Single-channel L6 | 3.3 cm | 99.86% | DOY 239/2019 |

All results match upstream CLASLIB ver.0.8.2 within regression tolerances.

### New Source Modules

| File | Description |
|------|-------------|
| `src/clas/mrtk_clas.c` | CSSR ST1–ST12 decoder, L6 I/O, bank management |
| `src/clas/mrtk_clas_grid.c` | Tropospheric/STEC grid interpolation (GSILIB v1.0.3) |
| `src/clas/mrtk_clas_osr.c` | OSR computation from CSSR corrections |
| `src/clas/mrtk_clas_isb.c` | Inter-System Bias handling |
| `src/pos/mrtk_ppp_rtk.c` | CLAS PPP-RTK positioning engine |
| `src/pos/mrtk_vrs.c` | VRS double-differenced RTK engine (~2050 lines) |

### New Utility Applications

| App | Description |
|-----|-------------|
| `ssr2obs` | Convert CLAS L6 corrections → RINEX3 VRS pseudo-observations.<br>Also outputs OSR corrections as CSV (`-c`) or RTCM3 MSM4 (`-b`). |
| `ssr2osr` | SSR→OSR verification via `postpos()`. Outputs per-satellite OSR residuals. |
| `dumpcssr` | CSSR message dump for L6 stream inspection and debugging. |

### New Configuration Options

`prcopt_t` fields extended for CLAS:

| Field | Description |
|-------|-------------|
| `err[5]`–`err[7]` | PPP-RTK measurement noise model (code/phase/iono) |
| `posopt[10]`–`posopt[12]` | Per-system frequency selection (GPS / GLONASS / Galileo+QZSS) |
| `floatcnt` | VRS-RTK float epoch threshold for filter reset |

Configuration keys: `pos1-errratio4`, `pos1-errratio5`, `pos2-floatcnt`,
`pos1-posopt10` – `pos1-posopt12`.

---

## Bug Fixes

- **`set_ssr_ch_idx()` / `set_mcssr_ch()`**: added `[0, SSR_CH_NUM)` bounds guard
  to prevent out-of-bounds memory access when an invalid channel index is passed.
- **`memset` sizeof targets**: `biaosb->spb` and `biaosb->vspb` now correctly
  reference their own member size instead of the sibling member.
- **ST12 fix rate** (93% → 98.75%): two root causes fixed:
  - `obs_code_for_freq(QZS, f=1)` now returns `CODE_L5Q` (not `CODE_L2S`)
  - SIS/IODE boundary detection uses `prev_orb_tow` instead of requiring
    `orb_tow == clk_tow` (ST12 orbit epoch always leads clock epoch by 5 s)
- **PPP-RTK fix rate** (97% → 99.86%): `sat_lambda()` switched from obsdef table
  to direct obs-code→frequency mapping, fixing QZS L2 wavelength = 0 caused by
  `apply_pppsig()` reordering the obsdef table for MADOCA signal selection.
- **`compensatedisp()` crash**: fixed null-pointer dereference when SSR
  corrections are not yet available at epoch start.
- **`clas_mode` gate**: `PMODE_SSR2OSR` now included in CLAS context
  initialisation check inside `postpos()`.

---

## License & Attribution Updates

All ~112 source files (library + apps) have been updated with:
- Full 9-party copyright stack (H.SHIONO, Cabinet Office, Lighthouse Technology,
  JAXA, TOSHIBA, Mitsubishi Electric Corp., GSI, T.SUZUKI, T.TAKASU)
- `SPDX-License-Identifier: BSD-2-Clause`
- Doxygen `@file` blocks separated from license comments

`LICENSE.txt` and `README.md` updated to reflect Mitsubishi Electric Corp. and
Geospatial Information Authority of Japan contributions.

---

## Known Limitations

- **Real-time CLAS channel**: `rtkrcv` currently processes only channel 0.
  Dual-channel real-time switching is not yet implemented.
- **Real-time L6D**: Ionospheric STEC correction (L6D) is post-processing only.
- **Static mode**: PPP-RTK/VRS-RTK in static receiver mode is not yet validated.

---

## Upgrade Notes

`prcopt_t` has been extended (`err[5→8]`, `posopt[6→13]`). A **full rebuild
from source** is required. No configuration file changes are needed for existing
MADOCALIB PPP workflows; CLAS-specific options default to safe values.
