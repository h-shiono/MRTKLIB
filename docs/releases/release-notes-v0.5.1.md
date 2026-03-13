# MRTKLIB v0.5.1 Release Notes

**Release date:** 2026-03-10
**Type:** Bug fix (dual-channel CLAS real-time performance)
**Branch:** `35-bug-rtkrcv-notable-performance-degradation-in-dual-channel-clas-realtime-positioning`

---

## Overview

v0.5.1 resolves a significant performance degradation in dual-channel CLAS real-time
PPP-RTK positioning.  The fix rate improves from **67% to 93%** (RT) and from
**87% to 99%** (PP).

### Root Cause

The `rtkrcv_2ch.toml` configuration used `frequency = "l1+2+3"` (nf=3), but CLAS
does not provide Galileo E5a (L5) phase bias corrections.  This caused:

1. `pbias[2]` (L5) remained `CLAS_CSSRINVALID` for all Galileo satellites
2. The L1-L5 geometry-free cycle slip detector (`detslp_gf()` in `mrtk_ppp_rtk.c`)
   fired false slips at every epoch
3. Galileo ambiguities were destroyed repeatedly, preventing AR convergence

### Fix

Set `frequency = "l1+2"` (nf=2) to match the bias corrections actually available
from CLAS.  With nf=2, the L1-L5 GF slip detector is skipped entirely
(`if (rtk->opt.nf <= 2) return`), and Galileo ambiguities converge normally.

---

## Performance Improvement

### Dual-channel CLAS (2025/157 dataset)

| Metric | v0.4.4 (nf=3) | v0.5.1 (nf=2) |
|--------|:---:|:---:|
| **RT fix rate** | 67.4% (2428/3600) | **92.6% (3335/3600)** |
| **PP fix rate** | 87.1% | **99.4%** |

### Single-channel CLAS (2019/239 dataset) — unchanged

| Metric | v0.4.4 | v0.5.1 |
|--------|:---:|:---:|
| RT fix rate | 97.7% | 97.7% |
| PP fix rate | 99.86% | 99.86% |

---

## Changes

### Fixed

- **`conf/claslib/rtkrcv_2ch.toml`** — Changed `frequency` from `"l1+2+3"` to
  `"l1+2"`.  CLAS does not provide Galileo E5a bias corrections; using nf=3
  caused false L1-L5 geometry-free cycle slip detection, destroying ambiguities
  and degrading AR fix rate by ~25 percentage points (RT) and ~12 pp (PP).

### Improved

- **`scripts/tools/gen_l6_tag.py`** — Two fixes:
  1. **tick_scale**: now always matches the master tag's timing scale, not only
     when the master is detected as "compressed".
  2. **UTC→GPST time basis**: L6 tag `time_time` now includes GPS-UTC leap
     seconds (+18 s) to match the GPST basis used by `gen_bnx_tag.py` and
     RTKLIB's `strsync()`.  Previously the ~18 s mismatch caused L6 data to
     replay early relative to observations.

### Updated

- **`tests/data/claslib/claslib_testdata.tar.gz`** — Updated `ref_L6_2CH_RT.nmea`
  reference to reflect the improved fix rate with nf=2 configuration.

---

## Test Results

All 59 tests pass (56 non-RT + 3 RT), unchanged from v0.5.0.

| Test Suite | Count | Status |
|------------|-------|--------|
| Unit tests | 12 | PASS |
| SPP / receiver bias | 4 | PASS |
| rtkrcv real-time (MADOCA + CLAS 1ch + CLAS 2ch) | 3 | PASS |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 | PASS |
| CLAS PPP-RTK + VRS-RTK | 19 | PASS |
| ssr2obs / ssr2osr / BINEX | 4 | PASS |
| Tier 2 absolute accuracy | 2 | PASS |
| Tier 3 position scatter | 2 | PASS |
| Fixtures | 3 | PASS |

---

## Technical Details

### Why nf=3 worked for single-channel but not dual-channel

The 2019/239 single-channel dataset has fewer Galileo E5a observations, so the
false GF slip detection had minimal impact.  The 2025/157 dual-channel dataset
has full Galileo E5a tracking, making every Galileo satellite vulnerable to
the false slip issue at every epoch.

### Code path analysis

```
nf=3 + CLAS (no E5a bias):
  clas_osr_corrmeas() → pbias[2] = CLAS_CSSRINVALID (no E5a bias available)
  detslp_gf() → gfmeas_L1L5() returns non-zero (L5 obs exists)
                → |g1 - g0| > thresslip → slip flagged
  ppp_rtk_pos() → ambiguity reset for Galileo → AR fails

nf=2 + CLAS:
  detslp_gf() → if (rtk->opt.nf <= 2) return → no L1-L5 GF check
  → Galileo ambiguities converge normally → AR succeeds
```

### Files changed

| File | Change |
|------|--------|
| `conf/claslib/rtkrcv_2ch.toml` | `frequency: "l1+2+3"` → `"l1+2"` |
| `scripts/tools/gen_l6_tag.py` | tick_scale fix + UTC→GPST time basis correction |
| `tests/data/claslib/claslib_testdata.tar.gz` | Updated RT reference NMEA + regenerated L6 tags |
