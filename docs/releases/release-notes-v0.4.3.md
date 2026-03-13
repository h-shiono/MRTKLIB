# MRTKLIB v0.4.3 Release Notes

**Release date:** 2026-03-09
**Type:** Feature (real-time CLAS PPP-RTK via rtkrcv)
**Branch:** `feat/rtkrcv-clas-realtime`

---

## Overview

v0.4.3 enables **real-time CLAS PPP-RTK positioning via `rtkrcv`** using BINEX/SBF
observation streams and CLAS L6D correction streams.  The same PPP-RTK engine used
by `rnx2rtkp` (post-processing) now runs in the rtksvr real-time pipeline, achieving
**97.7% fix rate** (3517/3599 Q=4 epochs) on the 2019/239 reference dataset — matching
the post-processing engine's 99.86% steady-state fix rate after convergence.

### Highlights

- **Real-time CLAS PPP-RTK** — `rtkrcv` processes BINEX or SBF observations alongside
  CLAS L6D corrections to produce centimetre-level PPP-RTK solutions in real time.
- **L6 rate limiter** — Prevents the L6 correction stream from overrunning observations
  during file replay, protecting the CLAS bank ring buffer from premature overwrite.
- **Time-tag generators** — New `gen_bnx_tag.py` and `gen_l6_tag.py` scripts create
  RTKLIB-compatible `.tag` files for synchronised `::T::xN` file replay.
- **CTest `rtkrcv_rt_clas`** — Automated regression test replaying 1 hour of BINEX+L6
  data at 10x speed (~370s wall time), verifying >=90% output line coverage.
- **Documentation** — [docs/rtkrcv-clas-realtime.md](../reference/rtkrcv-clas-realtime.md) with
  setup guide, PP vs RT performance comparison, architecture overview, and
  troubleshooting notes.

---

## Post-Processing vs Real-Time Performance

Both modes use the same CLAS PPP-RTK engine and the same 2019/239 dataset
(0627 station, 2019-08-27 16:00-17:00 UTC, Trimble NetR9).

| Metric | PP (rnx2rtkp) | RT (rtkrcv) |
|--------|:---:|:---:|
| Total epochs | 3,580 | 3,599 |
| Fix (Q=4) | 3,575 (99.86%) | 3,517 (97.72%) |
| Float (Q=5) | 5 (0.14%) | 5 (0.14%) |
| SPP (Q=1) | 0 (0.00%) | 77 (2.14%) |
| **Fix rate** | **99.86%** | **97.72%** |

The 77 Q=1 epochs represent the initial convergence period (~77 seconds).
After convergence, the steady-state fix rate is identical to post-processing.

---

## What Changed

### L6 Rate Limiter (`mrtk_rtksvr.c`)

During file replay, the L6 correction stream can deliver data faster than the
observation stream consumes it.  Without throttling, the CLAS bank ring buffer
(32 entries, ~16 minutes of corrections) is overwritten before the positioning
engine can use the data.

The rate limiter pauses L6 byte processing when the L6 decode time exceeds the
observation time by more than 60 seconds.  Unconsumed bytes are shifted to the
start of the buffer for processing in the next server cycle.

```c
gtime_t l6t = svr->clas->l6buf[ch].time;
gtime_t obst = svr->raw[0].time;
if (l6t.time > 0 && obst.time > 0 && timediff(l6t, obst) > 60.0) {
    /* pause L6 processing; shift remaining bytes */
}
```

### CLAS Real-Time Integration (`mrtk_rtksvr.c`)

The rtksvr thread now:
1. Initialises the CLAS context (`clas_ctx_t`) when the L6 stream format is detected
2. Wires the CLAS context into `nav->clas_ctx` for PPP-RTK engine access
3. Initialises `week_ref[]` from the first valid observation time
4. On each CSSR epoch boundary (`clas_decode_msg() == 10`), performs bank lookup
   and grid status checks for the detected network

### Time-Tag Generators

#### `gen_bnx_tag.py`

Parses BINEX 0x7F-05 observation records to extract epoch timestamps (minutes since
GPS epoch + milliseconds).  Creates one tag entry per observation epoch with accurate
file position mapping.

#### `gen_l6_tag.py`

Generates tags for L6 data at 1-second intervals (250 bytes/frame).  With `--sync-tag`,
synchronises to a master tag file by computing the GNSS-time offset and adjusting
`tick_f` and `tick_n` scaling.  Automatically detects compressed master tags (recorded
faster than real-time) and adjusts accordingly.

### Test Script Parameterisation (`run_rtkrcv_test.sh`)

The rtkrcv test script now accepts optional parameters:

```
bash run_rtkrcv_test.sh <binary> <project_root> <reference> \
     [playback_speed] [conf_file] [port] [max_timeout]
```

This enables reuse for both the MADOCA `rtkrcv_rt` test (default conf) and the
CLAS `rtkrcv_rt_clas` test (custom conf, port, and timeout).  Both tests use
`RESOURCE_LOCK rtkrcv_port` to prevent parallel execution conflicts.

### Debug Cleanup

Removed temporary debug `fprintf` statements from five source files:
- `mrtk_clas.c` — orbit/pbias/trop lookup diagnostics
- `mrtk_clas_grid.c` — grid pickup and trop interpolation diagnostics
- `mrtk_clas_osr.c` — OSR trop grid status
- `mrtk_ppp_rtk.c` — filter/zdres/ddres stage diagnostics

### Configurations

Three rtkrcv configurations for different input stream combinations:

| Config | Obs Format | Corrections | Use Case |
|--------|-----------|-------------|----------|
| `conf/claslib/rtkrcv.conf` | BINEX | CLAS L6D | CTest regression |
| `conf/claslib/rtkrcv_sbf_l6d.conf` | SBF | CLAS L6D | Septentrio receivers |
| `conf/claslib/rtkrcv_rtcm3_ubx.conf` | RTCM3 | UBX L6 | u-blox receivers |

---

## Test Suite (58 tests)

58 total tests (57 from v0.4.2 + 1 new `rtkrcv_rt_clas`).

| Test Suite | Tests |
|------------|-------|
| Unit tests | 12 |
| SPP / receiver bias | 4 |
| rtkrcv real-time (MADOCA + CLAS) | 2 |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 |
| CLAS PPP-RTK + VRS-RTK | 19 |
| ssr2obs / ssr2osr / BINEX | 4 |
| Tier 2 absolute accuracy | 2 |
| Tier 3 position scatter | 2 |
| Fixtures | 3 |

> `claslib_ppp_rtk_2ch_check` remains a marginal failure (0.202 m vs 0.200 m tolerance)
> unrelated to this release.

---

## Files Changed

| File | Change |
|------|--------|
| `src/stream/mrtk_rtksvr.c` | L6 rate limiter, CLAS context wiring |
| `apps/rtkrcv/rtkrcv.c` | CLAS stream format support |
| `include/mrtklib/mrtk_clas.h` | Public API additions |
| `src/clas/mrtk_clas.c` | Debug fprintf removal |
| `src/clas/mrtk_clas_grid.c` | Debug fprintf removal |
| `src/clas/mrtk_clas_osr.c` | Debug fprintf removal |
| `src/pos/mrtk_ppp_rtk.c` | Debug fprintf removal |
| `CMakeLists.txt` | `rtkrcv_rt_clas` test + RESOURCE_LOCK |
| `tests/cmake/run_rtkrcv_test.sh` | Parameterised (conf, port, timeout) |
| `tests/data/claslib/claslib_testdata.tar.gz` | Added .tag files + reference |
| `conf/claslib/rtkrcv.conf` | New (BINEX+L6) |
| `conf/claslib/rtkrcv_sbf_l6d.conf` | New (SBF+L6) |
| `conf/claslib/rtkrcv_rtcm3_ubx.conf` | New (RTCM3+UBX) |
| `scripts/tools/gen_bnx_tag.py` | New (BINEX tag generator) |
| `scripts/tools/gen_l6_tag.py` | New (L6 tag generator) |
| `docs/rtkrcv-clas-realtime.md` | New (setup + performance guide) |
