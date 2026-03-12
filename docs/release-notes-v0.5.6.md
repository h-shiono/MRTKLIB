# MRTKLIB v0.5.6 Release Notes

**Release date:** 2026-03-12
**Type:** Feature — RINEX 4.00 CNAV/CNV2 navigation support
**Branch:** `feat/rinex4-cnav`

---

## Overview

v0.5.6 adds RINEX 4.00 navigation message support for modern GNSS civil
navigation (CNAV) signals.  The new decoder handles GPS CNAV, GPS CNV2, QZSS
CNAV, QZSS CNV2, and BeiDou CNV1/CNV2/CNV3 ephemeris records, covering all
CNAV-class message types defined in the RINEX 4.00 specification.

RINEX 4.00 observation files are format-identical to RINEX 3.05 and require no
code changes — they are read correctly by the existing parser.

---

## Background

RINEX 4.00 (December 2021) introduces inline `> EPH` record types for modern
navigation messages that differ from the legacy LNAV/D1/D2 formats:

- **GPS/QZSS CNAV** (L2C/L5): 8 broadcast orbits (vs 7 for LNAV)
- **GPS/QZSS CNV2** (L1C): 9 broadcast orbits, adds ISC_L1Cd/ISC_L1Cp
- **BDS CNV1** (B1C): 9 broadcast orbits, SISAI/SISMAI accuracy indices
- **BDS CNV2** (B2a): 9 broadcast orbits, ISC_B2ad corrections
- **BDS CNV3** (B2b): 8 broadcast orbits, TGD_B2bI

These formats carry additional parameters not present in legacy messages:
semi-major axis rate (Adot), mean motion rate (Delta_n0_dot), inter-signal
corrections (ISC), and new accuracy indices (URAI_NED/URAI_ED for GPS/QZSS,
SISAI/SISMAI for BDS).

---

## Changes

### Added

| File | Change |
|------|--------|
| `src/data/mrtk_rinex.c` | `decode_eph_cnav()` — GPS/QZSS CNAV and CNV2 ephemeris decoder |
| `src/data/mrtk_rinex.c` | `decode_eph_bds_cnav()` — BDS CNV1/CNV2/CNV3 ephemeris decoder |
| `src/data/mrtk_rinex.c` | v4type detection for EPH CNAV/CNV2/CNV1/CNV3 record types |
| `src/data/mrtk_rinex.c` | v4type detection for STO/ION CNVX record types (GPS/QZSS/BDS) |
| `src/data/mrtk_rinex.c` | v4type-aware orbit count thresholds (35 for 8-orbit, 39 for 9-orbit types) |
| `tests/utest/t_rinex4.c` | 4 unit tests: record counts, GPS CNAV fields, BDS CNAV fields, QZSS CNV2 fields |
| `tests/data/rinex4/BRD400DLR_S_20260680000_01D_MN.rnx.gz` | DLR merged broadcast NAV test data |
| `CMakeLists.txt` | `t_rinex4` test target with gz decompress fixture |

### Changed

| File | Change |
|------|--------|
| `src/data/mrtk_rinex.c` | EPH decode trigger: replaced fixed `i >= 31` with v4type-aware `ndata_eph` threshold |

### Technical Details

**v4type codes (new):**

| Code | Record Type | Orbits |
|------|------------|--------|
| 2 | GPS CNAV | 8 |
| 3 | GPS CNV2 | 9 |
| 42 | QZSS CNAV | 8 |
| 43 | QZSS CNV2 | 9 |
| 53 | BDS CNV1 | 9 |
| 54 | BDS CNV2 | 9 |
| 55 | BDS CNV3 | 8 |

**eph_t field mapping for CNAV types:**

- `eph->type`: 1=CNAV, 2=CNV2 (GPS/QZSS); 2=CNV1, 3=CNV2, 4=CNV3 (BDS)
- `eph->Adot`: semi-major axis rate (CNAV-specific, orbit 1)
- `eph->ndot`: mean motion rate (CNAV-specific, orbit 5)
- `eph->sva`: URAI_ED index (GPS/QZSS) or SISMAI index (BDS) — stored directly, not converted via `uraindex()`
- `eph->tgd[0..5]`: TGD + ISC corrections (layout varies by message type)

**BDS CNAV week derivation:** BDS CNAV records do not contain a week number
field.  The BDT week is derived from the Toc epoch via `time2bdt(bdt2gpst(toc))`.

---

## Test Data

**BRD400DLR_S_20260680000_01D_MN.rnx.gz** — DLR (German Aerospace Center)
merged broadcast ephemeris file containing:

| Type | Count |
|------|-------|
| GPS LNAV | 418 |
| GPS CNAV | 414 |
| GPS CNV2 | 83 |
| QZSS LNAV | ~100 |
| QZSS CNAV | ~100 |
| QZSS CNV2 | ~100 |
| GAL INAV+FNAV | 5,836 |
| BDS D1/D2 | 1,157 |
| BDS CNV1 | 1,246 |
| BDS CNV2 | 2,177 |
| BDS CNV3 | 700 |
| GLO FDMA | 1,322 |
| SBAS | 9,594 |
| IRN LNAV | 283 |

---

## Test Results

62/62 non-realtime tests pass (59 existing + 3 new RINEX 4 tests).

| Test Suite | Count | Status |
|------------|-------|--------|
| Unit tests | 15 | PASS |
| SPP / receiver bias | 4 | PASS |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 | PASS |
| CLAS PPP-RTK + VRS-RTK | 19 | PASS |
| ssr2obs / ssr2osr / BINEX | 4 | PASS |
| Tier 2 absolute accuracy | 2 | PASS |
| Tier 3 position scatter | 2 | PASS |
| Fixtures | 6 | PASS |
