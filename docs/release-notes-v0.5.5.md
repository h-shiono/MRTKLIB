# MRTKLIB v0.5.5 Release Notes

**Release date:** 2026-03-11
**Type:** Bug fix — CLAS real-time via UBX
**Branch:** `31-bug-rtkrcv-clas-real-time-positioning-does-not-work-properly`

---

## Overview

v0.5.5 fixes [#31](https://github.com/h-shiono/MRTKLIB/issues/31): CLAS PPP-RTK
real-time positioning via UBX streams (`format = "ubx"`) did not work.  L6D
correction frames from UBX-RXM-QZSSL6 messages were silently dropped, preventing
the CLAS decoder from receiving any data.

After this fix, `rtkrcv` achieves PPP-RTK float via live UBX streams from u-blox
receivers (F9P obs + D9C L6D corrections).

---

## Root Cause

Three cascading bugs prevented UBX L6D data from reaching the CLAS decoder:

### Bug 1: L6D frames dropped by MADOCA decoder

`decode_rxmqzssl6()` unconditionally called `decode_qzss_l6emsg()` (the MADOCA-PPP
decoder) for all L6 frames, regardless of the `msg` field (0=L6D, 1=L6E).  The
MADOCA decoder checks `vendor_id == 2` and silently returns 0 for L6D frames
(vendor_id=1), so `ret=0` was returned instead of `ret=10`.  The redirect block
in `decoderaw()` only fires when `ret == 10`, so L6D data never reached
`clas_input_cssr()`.

**Fix:** Branch on the `msg` field — L6D (`msg=0`) returns `ret=10` directly,
skipping the MADOCA decoder.  L6E (`msg=1`) continues through
`decode_qzss_l6emsg()` as before.

### Bug 2: 2-channel L6D frame interleaving

UBX streams contain L6D frames from **two** QZS satellites (e.g., PRN 194 and
PRN 199) interleaved in a single stream.  The redirect block sent all frames to
CLAS channel 0 using the fixed mapping `ch = (index == 1) ? 1 : 0`.  This
corrupted the byte-by-byte subframe assembly in `clas_input_cssr()` — each PRN's
subframe-start indicator reset the other PRN's partially accumulated data.

**Fix:** For UBX format, extract the PRN from the L6 frame header
(`rtcm->buff[4]`) and route to separate CLAS channels.  The first PRN
encountered is assigned to ch=0, the second to ch=1.

### Bug 3: Incorrect initial value check for channel assignment

`clas_ctx_init()` initializes `l6delivery[ch]` to `-1` (not `0`).  The
PRN-based demux condition checked `l6delivery[0] == 0`, which was never true,
causing **all** frames to be routed to ch=1 regardless of PRN.

**Fix:** Changed the condition to `l6delivery[0] < 0`.

### Why simulated RT tests were not affected

The existing `rtkrcv_rt_clas` and `rtkrcv_rt_clas_2ch` tests use
`format = "clas"` (STRFMT_CLAS) for the correction stream.  In this path, raw
L6 bytes are fed **directly** to `clas_input_cssr()`, bypassing the UBX decoder
entirely.  The UBX path (`format = "ubx"`) was never exercised by the simulated
RT tests, so the bug was undetected.

In practice, users receive L6D corrections via UBX-RXM-QZSSL6 messages from
u-blox receivers — the `format = "clas"` path is only used with pre-recorded
raw L6 data.

---

## Changes

### Fixed

| File | Change |
|------|--------|
| `src/data/rcv/mrtk_rcv_ublox.c` | `decode_rxmqzssl6()`: branch on `msg` field, return `ret=10` for L6D |
| `src/stream/mrtk_rtksvr.c` | Redirect block: PRN-based channel demux for UBX format |
| `src/stream/mrtk_rtksvr.c` | `l6delivery[]` initial value check: `< 0` instead of `== 0` |

### Added

| File | Change |
|------|--------|
| `conf/claslib/rtkrcv_ubx_clas.toml` | Template config for real-time CLAS PPP-RTK via UBX TCP streams |
| `src/stream/mrtk_rtksvr.c` | Diagnostic trace for CSSR epoch decode events (level 2) |

---

## Data Flow: Before and After

### Before (broken)

```
UBX stream → decode_rxmqzssl6()
  → decode_qzss_l6emsg()     ← called for ALL L6 frames
  → vid != 2 → return 0      ← L6D silently dropped
  → ret=0 → redirect block skipped
  → CLAS decoder never receives data
```

### After (fixed)

```
UBX stream → decode_rxmqzssl6()
  → msg=0 (L6D): ret=10      ← routed to CLAS
  → msg=1 (L6E): decode_qzss_l6emsg()  ← MADOCA path unchanged
  → redirect block fires
  → PRN-based demux: PRN-A→ch0, PRN-B→ch1
  → clas_input_cssr() per channel
  → clas_decode_msg() → clas_update_global()
```

---

## Test Results

56/56 non-realtime tests pass — unchanged from v0.5.4.

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

### Live UBX verification

PPP-RTK float achieved via live UBX streams (u-blox F9P obs + D9C L6D) with
`conf/claslib/rtkrcv_ubx_clas.toml`.  Fix convergence is dependent on CLAS
correction availability and grid coverage.
