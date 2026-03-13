# MRTKLIB v0.4.4 Release Notes

**Release date:** 2026-03-09
**Type:** Feature (dual-channel CLAS real-time PPP-RTK)
**Branch:** `feat/rtkrcv-clas-2ch`

---

## Overview

v0.4.4 adds **dual-channel (2ch) CLAS L6 support to `rtkrcv`** real-time positioning.
The unused base-station stream slot (stream index 1) is repurposed as a second L6
correction input, enabling `rtkrcv` to process two independent CLAS L6D streams
simultaneously — matching the post-processing engine's dual-channel capability.

### Highlights

- **2ch CLAS real-time** — `rtkrcv` now accepts two L6 correction streams: stream 3
  carries L6 ch1, stream 2 (base slot) carries L6 ch2.  The CLAS channel is derived
  from the stream index automatically.
- **CTest `rtkrcv_rt_clas_2ch`** — Automated regression test replaying 1 hour of
  2-channel BINEX+L6 data at 10x speed (~372s wall time).
- **Minimal code change** — Only 2 lines changed in `mrtk_rtksvr.c`; the existing
  `CLAS_CH_NUM=2` infrastructure handles the rest.

---

## Performance: Single-Channel vs Dual-Channel Real-Time

### Single-channel (2019/239 dataset, v0.4.3)

| Metric | PP (rnx2rtkp) | RT (rtkrcv) |
|--------|:---:|:---:|
| Total epochs | 3,580 | 3,599 |
| Fix (Q=4) | 3,575 (99.86%) | 3,517 (97.72%) |
| Float (Q=5) | 5 (0.14%) | 5 (0.14%) |
| SPP (Q=1) | 0 (0.00%) | 77 (2.14%) |

### Dual-channel (2025/157 dataset, v0.4.4)

| Metric | PP (rnx2rtkp) | RT (rtkrcv) |
|--------|:---:|:---:|
| Total epochs | 3,600 | 3,600 |
| Fix (Q=4) | ~3,168 (88%) | 2,428 (67.4%) |
| Float (Q=5) | ~432 (12%) | 1,155 (32.1%) |
| SPP (Q=1) | 0 (0%) | 17 (0.5%) |

### Analysis

- The 2025/157 dataset is inherently more challenging than 2019/239 (PP fix rate
  88% vs 99.86%), so the RT fix rate gap is expected.
- The RT 2ch fix rate (67.4%) is lower than PP (88%) primarily due to real-time
  stream synchronisation: L6 ch1 and ch2 corrections arrive via independent streams,
  and the correction merge timing is less optimal than in post-processing where
  both channels are read synchronously per epoch.
- All 3,600 epochs produce valid solutions (only 17 SPP during initial convergence).

---

## What Changed

### Stream-to-Channel Mapping (`mrtk_rtksvr.c`)

The CLAS channel assignment was changed from hardcoded `ch=0` to stream-index-derived:

```c
int ch = (index == 1) ? 1 : 0;  /* index 1 (inpstr2)→ch1, index 2 (inpstr3)→ch0 */
```

This applies to both the direct CLAS decode path and the UBX/L6E redirect path.
The existing rate limiter, week_ref initialisation, and bank/grid update logic
all work per-channel without modification.

### Design: Base Stream Repurposing

In PPP-RTK mode, stream index 1 (base station) is unused.  Rather than extending
`rtksvr_t` with a 4th input stream (which would require significant structural
changes), the base slot is repurposed for L6 ch2:

| Stream | Original Purpose | PPP-RTK 1ch | PPP-RTK 2ch |
|--------|-----------------|-------------|-------------|
| 0 | Rover obs | BINEX obs | BINEX obs |
| 1 | Base obs | Off | L6 ch2 (CLAS) |
| 2 | Corrections | L6 ch1 (CLAS) | L6 ch1 (CLAS) |

### Configuration (`rtkrcv_2ch.conf`)

Key differences from single-channel `rtkrcv.conf`:

```ini
inpstr2-type   =file           # was: off
inpstr2-path   =...ch2.l6::T::x10
inpstr2-format =clas           # was: (empty)
```

---

## Test Suite (59 tests)

59 total tests (58 from v0.4.3 + 1 new `rtkrcv_rt_clas_2ch`).

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

---

## Files Changed

| File | Change |
|------|--------|
| `src/stream/mrtk_rtksvr.c` | Derive CLAS channel from stream index (2 lines) |
| `conf/claslib/rtkrcv_2ch.conf` | New (2ch BINEX+L6 config) |
| `CMakeLists.txt` | `rtkrcv_rt_clas_2ch` test + cleanup fixture update |
| `tests/data/claslib/claslib_testdata.tar.gz` | Added 2ch tag files + RT reference |
