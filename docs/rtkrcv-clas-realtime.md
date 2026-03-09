# Real-Time CLAS PPP-RTK via rtkrcv

MRTKLIB supports real-time CLAS PPP-RTK positioning using `rtkrcv`.
Input streams can be any RTKLIB-supported type — serial, TCP client/server,
NTRIP, or file replay.  This document describes the setup, usage, and
performance characteristics, with file-stream replay as the primary example.

---

## Overview

`rtkrcv` processes GNSS observations and CLAS L6D corrections in real time
(or accelerated file replay) to produce centimetre-level PPP-RTK solutions.
The same CLAS engine used by `rnx2rtkp` (post-processing) runs inside
the rtksvr thread, with corrections decoded from a separate L6 input stream.

### Supported Input Combinations

| Stream 1 (obs) | Stream 3 (corrections) | Format |
|-----------------|----------------------|--------|
| BINEX (`.bnx`)  | CLAS L6D (`.l6`)     | `binex` + `clas` |
| SBF (`.sbf`)    | CLAS L6D (`.l6`)     | `sbf` + `clas` |
| RTCM3 (`.rtcm`) | UBX L6 (`.ubx`)      | `rtcm3` + `ubx` |

For file replay, both streams require `.tag` time-tag files and the `::T::xN`
suffix for synchronised playback (e.g. `::T::x10` for 10x speed).

---

## Quick Start (File Replay)

### 1. Generate Time Tags

If your data files do not have `.tag` files, generate them:

```bash
# BINEX observation tag (parses epoch timestamps from 0x7F-05 records)
python3 scripts/tools/gen_bnx_tag.py  <file.bnx>

# L6 correction tag (synchronized to the BINEX master tag)
python3 scripts/tools/gen_l6_tag.py   <file.l6>  --sync-tag <file.bnx.tag>
```

### 2. Configure

Use `conf/claslib/rtkrcv.conf` as a template.  Key settings:

```ini
# Positioning mode
pos1-posmode       =ppp-rtk
pos1-sateph        =brdc+ssrapc

# Input streams with time-tag replay at 10x speed
inpstr1-path       =./path/to/obs.bnx::T::x10
inpstr1-format     =binex
inpstr3-path       =./path/to/corrections.l6::T::x10
inpstr3-format     =clas

# Required support files
file-cssrgridfile  =./tests/data/claslib/clas_grid.def
file-blqfile       =./tests/data/claslib/clas_grid.blq
file-eopfile       =./tests/data/claslib/igu00p01.erp
file-rcvantfile    =./tests/data/claslib/igs14_L5copy.atx
file-isbfile       =./tests/data/claslib/isb.tbl
file-phacycfile    =./tests/data/claslib/l2csft.tbl
```

### 3. Run

```bash
./build/rtkrcv -s -p 52005 -o conf/claslib/rtkrcv.conf
```

Flags:
- `-s` : auto-start streaming on launch
- `-p PORT` : telnet console port
- `-d N` : trace level (0-5)
- `-o FILE` : configuration file

---

## Performance: Post-Processing vs Real-Time

Both modes use the **same CLAS PPP-RTK engine** (`mrtk_ppp_rtk.c`) and the
same CLAS correction decoder.  The comparison below uses the 2019/239 dataset
(0627 station, 2019-08-27 16:00-17:00 UTC, Trimble NetR9).

| Metric | Post-Processing (rnx2rtkp) | Real-Time (rtkrcv) |
|--------|---------------------------|-------------------|
| Total epochs | 3,580 | 3,599 |
| Fix (Q=4) | 3,575 (99.86%) | 3,517 (97.72%) |
| Float (Q=5) | 5 (0.14%) | 5 (0.14%) |
| SPP (Q=1) | 0 (0.00%) | 77 (2.14%) |
| **Fix rate** | **99.86%** | **97.72%** |

### Analysis

- The 77 Q=1 epochs in real-time correspond to the initial **convergence period**
  (~77 seconds) before the Kalman filter reaches fixing state.  In post-processing,
  the full navigation data is available from the start, enabling faster convergence.
- After convergence, the steady-state fix rate is identical: ~99.86%.
- The slight epoch count difference (3,599 vs 3,580) arises from the real-time
  server processing more observation boundaries at stream startup.

---

## Architecture

### Server Thread Flow

```
strread(obs)  ──> decoderaw(0)  ──> rtkpos()  ──> writesol()
strread(l6)   ──> decoderaw(2)  ──> clas_decode_msg()
                                  ──> clas_bank_get_close()
                                  ──> clas_update_global() ──> nav.ssr[]
```

The rtksvr main loop processes all input streams in each cycle:

1. Read bytes from all streams (`strread`)
2. Decode observations (stream 0) and corrections (stream 2)
3. When a new observation epoch is available, call `rtkpos()` which
   accesses CLAS corrections via `nav->clas_ctx`

### L6 Rate Limiter

A rate limiter prevents the L6 correction stream from overrunning the
observation stream during file replay.  When L6 time exceeds observation
time by more than 60 seconds, L6 processing pauses until observations
catch up.  This protects the CLAS bank ring buffer (32 entries, ~16 minutes
of corrections) from being overwritten before the positioning engine
can use the data.

### Time-Tag Synchronisation

File replay with `::T::xN` requires `.tag` files for both streams.
The tag file maps wall-clock time (`tick_n` in milliseconds) to file
positions, allowing `strsync()` to release data at the correct rate.

- **Master stream** (obs): `tick_f` sets the base clock
- **Slave stream** (L6): `offset = master_tick_f - slave_tick_f`
  determines when data starts relative to the master

---

## Tag File Generation

### BINEX Tag (`gen_bnx_tag.py`)

Parses BINEX 0x7F-05 observation records to extract epoch timestamps
(minutes since GPS epoch + milliseconds).  Each epoch creates one tag
entry mapping its `tick_n` (ms since first epoch) to its file offset.

### L6 Tag (`gen_l6_tag.py`)

L6 data is transmitted at 250 bytes/s (one frame per second).  The tag
generator creates one entry per frame at 1-second intervals.

With `--sync-tag`, it aligns the L6 tag to a master tag file:
- Reads the master's `tick_f` and base time
- Computes the GNSS-time offset between L6 start and master start
- Sets `tick_f` so the offset produces correct `strsync()` alignment
- Detects compressed master tags (recorded faster than real-time) and
  adjusts `tick_n` scaling accordingly

---

## CTest Integration

The `rtkrcv_rt_clas` test is registered in `CMakeLists.txt` and runs
automatically as part of the full test suite:

```bash
cd build && ctest -R rtkrcv_rt_clas --output-on-failure
```

The test:
1. Extracts test data from `claslib_testdata.tar.gz` (fixture)
2. Patches the config file with a temporary output path
3. Runs `rtkrcv` at x10 speed (~370s wall time for 1 hour of data)
4. Compares output line count against reference (>=90% threshold)

The test uses `RESOURCE_LOCK rtkrcv_port` to prevent parallel execution
conflicts with the MADOCA `rtkrcv_rt` test.

---

## Configuration Reference

### Key rtkrcv.conf Options for CLAS PPP-RTK

| Option | Value | Description |
|--------|-------|-------------|
| `pos1-posmode` | `ppp-rtk` | CLAS PPP-RTK positioning mode |
| `pos1-sateph` | `brdc+ssrapc` | Broadcast + SSR (antenna phase centre) |
| `pos1-navsys` | `25` | GPS(1) + Galileo(8) + QZSS(16) |
| `pos1-dynamics` | `on` | Enable kinematic dynamics model |
| `pos1-ionoopt` | `est-adaptive` | Adaptive ionosphere estimation |
| `pos1-gridsel` | `1000` | CLAS grid selection radius (m) |
| `pos2-armode` | `fix-and-hold` | Ambiguity resolution mode |
| `pos2-isb` | `on` | Inter-system bias correction |
| `file-cssrgridfile` | `clas_grid.def` | CLAS grid definition (required) |
| `misc-timeinterp` | `on` | Interpolate corrections between epochs |
| `misc-maxobsloss` | `90` | Max epochs without obs before reset |
| `misc-floatcnt` | `15` | Float epochs before fix attempt |

### Required Support Files

| File | Description |
|------|-------------|
| `clas_grid.def` | CLAS correction grid point definitions |
| `clas_grid.blq` | Ocean tide loading at grid points |
| `igu00p01.erp` | Earth rotation parameters |
| `igs14_L5copy.atx` | Satellite/receiver antenna phase centres |
| `isb.tbl` | Inter-system bias correction table |
| `l2csft.tbl` | L2C 1/4-cycle phase shift correction |

---

## Troubleshooting

### All solutions are Q=1 (SPP)

1. **Missing `clas_grid.def`**: This is the most common cause. Ensure
   `file-cssrgridfile` points to a valid grid definition file. Without it,
   the CLAS engine cannot select atmospheric correction grids.

2. **Missing `.tag` files**: Without time tags, both streams dump data at
   maximum speed with no synchronisation. Generate tags using the scripts
   in `scripts/tools/`.

3. **Stream format mismatch**: Verify `inpstr1-format` and `inpstr3-format`
   match the actual file formats.

### `clas grid file error` in output

The grid definition file path is invalid or the file doesn't exist.
Check `file-cssrgridfile` in the config.

### `Error: vt is NULL`

Harmless warning from rtkrcv's console subsystem when running without an
interactive terminal (e.g. from scripts).  Does not affect positioning.
