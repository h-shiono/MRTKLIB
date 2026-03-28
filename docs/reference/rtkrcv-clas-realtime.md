# Real-Time CLAS PPP-RTK via rtkrcv

MRTKLIB supports real-time CLAS PPP-RTK positioning using `rtkrcv`.
Input streams can be any RTKLIB-supported type — serial, TCP client/server,
NTRIP, or file replay.  Both single-channel and dual-channel L6 configurations
are supported.  This document describes the setup, usage, and performance
characteristics, with file-stream replay as the primary example.

---

## Overview

`rtkrcv` processes GNSS observations and CLAS L6D corrections in real time
(or accelerated file replay) to produce centimetre-level PPP-RTK solutions.
The same CLAS engine used by `rnx2rtkp` (post-processing) runs inside
the rtksvr thread, with corrections decoded from a separate L6 input stream.

### Supported Input Combinations

**Single-channel L6:**

| Stream 1 (obs) | Stream 3 (corrections) | Format |
|-----------------|----------------------|--------|
| BINEX (`.bnx`)  | CLAS L6D (`.l6`)     | `binex` + `clas` |
| SBF (`.sbf`)    | CLAS L6D (`.l6`)     | `sbf` + `clas` |
| RTCM3 (`.rtcm`) | UBX L6 (`.ubx`)      | `rtcm3` + `ubx` |

**Dual-channel L6:**

| Stream 1 (obs) | Stream 2 (L6 ch2) | Stream 3 (L6 ch1) | Format |
|-----------------|-------------------|-------------------|--------|
| BINEX (`.bnx`)  | CLAS L6D ch2      | CLAS L6D ch1      | `binex` + `clas` + `clas` |

Stream 2 (internal index 1, the base-station slot unused in PPP-RTK) is
repurposed for L6 ch2.  Channel mapping: `ch = (index == 1) ? 1 : 0`.

For file replay, all streams require `.tag` time-tag files and the `::T::xN`
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

Use `conf/claslib/rtkrcv.toml` (single-channel) or `rtkrcv_2ch.toml`
(dual-channel) as a template.  Key settings:

```toml
[positioning]
mode                = "ppp-rtk"
frequency           = "l1+2"       # CLAS does not provide E5b bias; use nf=2
satellite_ephemeris = "brdc+ssrapc"
constellations      = 25           # GPS + Galileo + QZSS

# Single-channel L6
[streams.input.rover]
path   = "./path/to/obs.bnx::T::x10"
format = "binex"

[streams.input.correction]
path   = "./path/to/corrections.l6::T::x10"
format = "clas"

# --- OR dual-channel L6 ---
[streams.input.base]          # repurposed for L6 ch2
path   = "./path/to/ch2.l6::T::x10"
format = "clas"

[streams.input.correction]    # L6 ch1
path   = "./path/to/ch1.l6::T::x10"
format = "clas"

[files]
cssr_grid     = "./tests/data/claslib/clas_grid.def"
ocean_loading = "./tests/data/claslib/clas_grid.blq"
eop           = "./tests/data/claslib/igu00p01.erp"
receiver_atx  = "./tests/data/claslib/igs14_L5copy.atx"
isb_table     = "./tests/data/claslib/isb.tbl"
phase_cycle   = "./tests/data/claslib/l2csft.tbl"
```

> **Important:** Use `frequency = "l1+2"` (nf=2) for CLAS.  CLAS does not
> provide Galileo E5b bias corrections; using `"l1+2+3"` (nf=3) adds the
> E5b slot without valid bias, causing false geometry-free cycle slip
> detection that destroys Galileo ambiguities and severely degrades AR fix rate.

### 3. Run

```bash
# Single-channel
./build/rtkrcv -s -p 52005 -o conf/claslib/rtkrcv.toml

# Dual-channel
./build/rtkrcv -s -p 52005 -o conf/claslib/rtkrcv_2ch.toml
```

Flags:
- `-s` : auto-start streaming on launch
- `-p PORT` : telnet console port
- `-d N` : trace level (0-5)
- `-o FILE` : configuration file

---

## Performance: Post-Processing vs Real-Time

Both modes use the **same CLAS PPP-RTK engine** (`mrtk_ppp_rtk.c`) and the
same CLAS correction decoder.

### Single-channel (2019/239 dataset)

0627 station, 2019-08-27 16:00-17:00 UTC, Trimble NetR9.

| Metric | Post-Processing (rnx2rtkp) | Real-Time (rtkrcv) |
|--------|:---:|:---:|
| Total epochs | 3,580 | 3,599 |
| Fix (Q=4) | 3,575 (99.86%) | 3,517 (97.72%) |
| Float (Q=5) | 5 (0.14%) | 5 (0.14%) |
| SPP (Q=1) | 0 (0.00%) | 77 (2.14%) |

The 77 Q=1 epochs correspond to the initial convergence period (~77 seconds).
After convergence, the steady-state fix rate is identical: ~99.86%.

### Dual-channel (2025/157 dataset)

0627 station, 2025-06-06 20:00-21:00 UTC, Trimble NetR9.

| Metric | Post-Processing (rnx2rtkp) | Real-Time (rtkrcv) |
|--------|:---:|:---:|
| Fix (Q=4) | 3,579 (99.4%) | ~3,335 (92.6%) |
| Float (Q=5) | ~21 (0.6%) | ~192 (5.3%) |
| SPP (Q=1) | 0 (0.0%) | ~73 (2.0%) |

RT fix rate varies slightly between runs due to stream timing, but consistently
exceeds 90%.  The RT-PP gap is primarily from initial convergence and stream
synchronisation latency.

### Analysis

- After convergence, the steady-state fix rate is near-identical between PP and RT.
- The slight epoch count differences arise from the real-time server processing
  more observation boundaries at stream startup.
- **nf=2 is required for CLAS** — Using nf=3 degrades the 2ch fix rate to ~67%
  due to false L1-L5 GF cycle slips on Galileo (see Troubleshooting).

---

## Architecture

### Server Thread Flow

```
strread(obs)       ──> decoderaw(0)  ──> rtkpos()  ──> writesol()
strread(l6_ch2)    ──> decoderaw(1)  ──> clas_decode_msg(ch=1)  [2ch only]
strread(l6_ch1)    ──> decoderaw(2)  ──> clas_decode_msg(ch=0)
                                       ──> clas_bank_get_close()
                                       ──> clas_update_global() ──> nav.ssr[]
```

The rtksvr main loop processes all input streams in each cycle:

1. Read bytes from all streams (`strread`)
2. Decode observations (stream 0) and corrections (stream 1/2)
3. Channel mapping: `ch = (index == 1) ? 1 : 0` — stream 2 → ch0, stream 1 → ch1
4. When a new observation epoch is available, call `rtkpos()` which
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

The L6 filename encodes UTC start time (session letter A–X maps to hours
0–23).  Since `gen_bnx_tag.py` stores GPST-basis timestamps, `gen_l6_tag.py`
applies GPS-UTC leap seconds (+18 s as of 2017) to match the same time basis.

With `--sync-tag`, it aligns the L6 tag to a master tag file:
- Reads the master's `tick_f` and base time (GPST basis)
- Converts L6 UTC start time to GPST (adds leap seconds)
- Computes the GNSS-time offset between L6 start and master start
- Sets `tick_f` so the offset produces correct `strsync()` alignment
- Matches `tick_n` scaling to the master tag's timing (handles both
  real-time and non-real-time recorded masters)

---

## CTest Integration

Two CLAS real-time tests are registered in `CMakeLists.txt`:

```bash
cd build && ctest -R rtkrcv_rt_clas --output-on-failure
```

| Test | Config | Dataset | Wall time |
|------|--------|---------|-----------|
| `rtkrcv_rt_clas` | `rtkrcv.toml` | 2019/239 (1ch) | ~370s at x10 |
| `rtkrcv_rt_clas_2ch` | `rtkrcv_2ch.toml` | 2025/157 (2ch) | ~372s at x10 |

Each test:
1. Extracts test data from `claslib_testdata.tar.gz` (fixture)
2. Patches the TOML config with a temporary output path
3. Runs `rtkrcv` at x10 speed
4. Compares output line count against reference (>=90% threshold)

Both tests use `RESOURCE_LOCK rtkrcv_port` to prevent parallel execution
conflicts with the MADOCA `rtkrcv_rt` test.

---

## Configuration Reference

### Key TOML Options for CLAS PPP-RTK

| TOML Key | Value | Description |
|----------|-------|-------------|
| `positioning.mode` | `"ppp-rtk"` | CLAS PPP-RTK positioning mode |
| `positioning.frequency` | `"l1+2"` | **Must be nf=2** (CLAS has no E5b bias) |
| `positioning.satellite_ephemeris` | `"brdc+ssrapc"` | Broadcast + SSR (antenna phase centre) |
| `positioning.constellations` | `25` | GPS(1) + Galileo(8) + QZSS(16) |
| `positioning.dynamics` | `true` | Enable kinematic dynamics model |
| `positioning.atmosphere.ionosphere` | `"est-adaptive"` | Adaptive ionosphere estimation |
| `positioning.clas.grid_selection_radius` | `1000` | CLAS grid selection radius (m) |
| `ambiguity_resolution.mode` | `"fix-and-hold"` | Ambiguity resolution mode |
| `receiver.isb` | `true` | Inter-system bias correction |
| `files.cssr_grid` | `"clas_grid.def"` | CLAS grid definition (required) |
| `server.time_interpolation` | `true` | Interpolate corrections between epochs |
| `server.max_obs_loss` | `90.0` | Max epochs without obs before reset |
| `server.float_count` | `15` | Float epochs before fix attempt |

### Required Support Files

| File | TOML Key | Description |
|------|----------|-------------|
| `clas_grid.def` | `files.cssr_grid` | CLAS correction grid point definitions |
| `clas_grid.blq` | `files.ocean_loading` | Ocean tide loading at grid points |
| `igu00p01.erp` | `files.eop` | Earth rotation parameters |
| `igs14_L5copy.atx` | `files.receiver_atx` | Satellite/receiver antenna phase centres |
| `isb.tbl` | `files.isb_table` | Inter-system bias correction table |
| `l2csft.tbl` | `files.phase_cycle` | L2C 1/4-cycle phase shift correction |

---

## Troubleshooting

### Low fix rate with Galileo satellites

**Symptom:** Fix rate drops to ~67% (2ch) or noticeably below expected
levels, with frequent ambiguity resets on Galileo satellites.

**Cause:** `frequency = "l1+2+3"` (nf=3) in the config.  CLAS does not
provide Galileo E5b phase bias corrections.  With nf=3, the E5b frequency
slot has no valid bias, causing the geometry-free cycle slip detector
(`detslp_gf()` in `mrtk_ppp_rtk.c`) to fire false slips at every epoch.
This destroys Galileo ambiguities and prevents AR convergence.

**Fix:** Set `frequency = "l1+2"` (nf=2).  This skips the L1-L5 GF
slip detector entirely and uses only L1+L2 observations, which have
valid CLAS bias corrections.

### All solutions are Q=1 (SPP)

1. **Missing `clas_grid.def`**: This is the most common cause. Ensure
   `files.cssr_grid` points to a valid grid definition file. Without it,
   the CLAS engine cannot select atmospheric correction grids.

2. **Missing `.tag` files**: Without time tags, both streams dump data at
   maximum speed with no synchronisation. Generate tags using the scripts
   in `scripts/tools/`.

3. **Stream format mismatch**: Verify stream format settings match the
   actual file formats.

### `clas grid file error` in output

The grid definition file path is invalid or the file doesn't exist.
Check `files.cssr_grid` in the config.

### `Error: vt is NULL`

Harmless warning from rtkrcv's console subsystem when running without an
interactive terminal (e.g. from scripts).  Does not affect positioning.
