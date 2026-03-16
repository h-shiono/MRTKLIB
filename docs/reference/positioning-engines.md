# Positioning Engines

MRTKLIB provides four positioning engines, each optimized for a different
use case. All engines share the same Kalman filter state vector layout
and core math utilities but differ in observation model, ambiguity
resolution strategy, and signal handling.

## Engine Overview

| Engine | Mode | Entry Point | Source |
|--------|------|-------------|--------|
| SPP | `single` | `pntpos()` | `mrtk_pntpos.c` |
| Standard RTK | `kinematic`, `static`, `dgps`, `moveb`, `fixed` | `relpos()` | `mrtk_rtkpos.c` |
| VRS-RTK | `vrs-rtk` | `relposvrs()` | `mrtk_vrs.c` |
| PPP-RTK | `ppp-rtk` | `ppp_rtk_pos()` | `mrtk_ppp_rtk.c` |

All engines are dispatched from `rtkpos()` in `mrtk_rtkpos.c` based on
`prcopt.mode`.

## Standard RTK (`relpos`)

The general-purpose RTK engine derived from RTKLIB. Handles single-base
differential positioning with DGPS, kinematic, static, and
moving-baseline modes.

### Characteristics

- **Input**: Rover RINEX obs + single base RINEX obs + broadcast nav
- **Signal selection**: Automatic via `selobs()` with signal priority list
- **Ambiguity resolution**: `manage_amb_LAMBDA()` — multi-attempt partial
  AR with iterative satellite exclusion
- **Filter**: Standard Kalman `filter()`
- **Validation**: `valpos()` chi-squared test
- **ISB**: Optional, via `isb_table`
- **Filter reset**: None (runs until convergence or divergence)

### Typical use case

Conventional RTK with a physical base station or NTRIP corrections.

## VRS-RTK (`relposvrs`)

CLAS-optimized RTK engine derived from claslib. Designed for processing
virtual reference station observations generated from CLAS SSR
corrections via `clas_ssr2osr`.

### Characteristics

- **Input**: Rover RINEX obs + VRS base obs (from `cssr2rtcm3` or
  `ssr2obs`) + broadcast nav
- **Signal selection**: `selfreqpair()` — CLAS smode-based frequency
  pair selection with same-frequency fallback
- **Ambiguity resolution**: `resamb_LAMBDA()` — single-shot LAMBDA with
  satellite exclusion loop
- **Filter**: Adaptive Kalman `filter2()` with process noise estimation
- **Validation**: Module-level `vrs_chisq` + `maxinno_ext[]` thresholds
- **ISB**: CLAS `receiver_type` based (Trimble, Septentrio, etc.)
- **Filter reset**: Periodic (`opt->regularly`) + emergency
  (`nav->filreset` after prolonged float)
- **Dual-channel**: `l6mrg` option merges two CLAS L6 base streams
  (`nr[0]` + `nr[1]`)

### Typical use case

Post-processing or real-time positioning with CLAS L6D corrections
converted to VRS observations.

### VRS vs Standard RTK for the same base obs

Both engines can process the same RINEX base observations, but VRS-RTK
includes CLAS-specific optimizations:

| Aspect | Standard RTK | VRS-RTK |
|--------|--------------|---------|
| Freq selection | Signal priority list | `selfreqpair()` with smode fallback |
| AR strategy | Multi-attempt partial AR | Single-shot LAMBDA + exclusion |
| Filter | Fixed noise | Adaptive process noise |
| Multi-base | No | Dual-channel via `l6mrg` |
| Auto-reset | No | Periodic + float-count reset |

## PPP-RTK (`ppp_rtk_pos`)

Single-receiver precise point positioning using CLAS SSR corrections
directly (orbit, clock, code/phase bias, ionosphere, troposphere).
No base station observations required.

### Characteristics

- **Input**: Rover RINEX obs + broadcast nav + L6 file (`.l6`)
- **Corrections**: CLAS CSSR decoded in-process — orbit, clock,
  code/phase bias, STEC, troposphere
- **State vector**: Position + ionosphere + troposphere + bias
  (no clock states — absorbed by SSR corrections)
- **Ambiguity resolution**: Fix-and-hold with LAMBDA
- **Signal selection**: `clas_osr_selfreqpair()` with `code_same_freq()`
  fallback

### Typical use case

Highest-accuracy single-receiver positioning with CLAS L6D/L6E.

## Shared State Vector

All RTK/PPP-RTK engines share the same state vector layout:

```
Index:  [0..NP-1]  [NP..NP+NI-1]  [NP+NI..NP+NI+NT-1]  [NR..NR+NB-1]
State:  Position    Ionosphere      Troposphere           Phase ambiguity
        (+ vel/acc  (per satellite)  (ZWD + gradient)      (per sat × freq)
         if dynamics)
```

Macros defined identically in both `mrtk_rtkpos.c` and `mrtk_vrs.c`:

- `NP(opt)` — position states: 3 (static) or 9 (kinematic)
- `NI(opt)` — ionosphere: 0 or MAXSAT
- `NT(opt)` — troposphere: 0, 2, or 6
- `NR(opt)` — non-ambiguity states: NP + NI + NT + NL
- `NB(opt)` — ambiguity states: MAXSAT x NF
- `NX(opt)` — total: NR + NB
- `IB(s,f,opt)` — index of ambiguity for satellite `s`, frequency `f`

## Mode Dispatch (`rtkpos`)

```
rtkpos()
  ├── PMODE_SINGLE        → pntpos() only
  ├── PMODE_SSR2OSR       → clas_ssr2osr()
  ├── PMODE_PPP_RTK       → ppp_rtk_pos()
  ├── PMODE_VRS_RTK       → relposvrs()
  ├── PMODE_PPP_KINEMA+   → pppos()
  └── PMODE_DGPS..STATIC  → relpos()
```
