# Release Notes — v0.7.8

## Kinematic RTK: rover cycle-slip detection restored (regression since v0.6.10)

**Release date:** 2026-08-04
**Type:** Bug fix (relative-RTK positioning regression) + benchmark tooling
**Branch:** `release/v0.7.8`

---

### Overview

The v0.7.7 benchmark re-run ([#318](https://github.com/h-shiono/MRTKLIB/issues/318))
exposed a kinematic-RTK regression against the v0.4.1 record on all six
PPC-Dataset cases: the fixed-tier *tail* blew up while the median stayed at
centimetres (tokyo_run2: 1.3 cm 1σ against a 240 m 95th percentile — ~11 % of
epochs reporting Q=4 were wrong integer fixes), and integer-fix availability
collapsed on the Nagoya cases (nagoya_run3 10.1 % → 0.5 %). This was the same
false-fix mode that v0.4.0 had eliminated.

A per-tag build-and-run bisect isolated the step change to **v0.6.10**:
v0.4.1 through v0.6.9 are metric-identical and healthy; v0.6.10 through
v0.7.7 are metric-identical and regressed.

### Root cause

v0.6.10's SPP TDCP work ([#116](https://github.com/h-shiono/MRTKLIB/issues/116)
P4) added an **unconditional** store at the end of `pntpos()`:

```c
ssat[obs[i].sat - 1].ph[0][f] = obs[i].L[f];   /* current epoch! */
ssat[obs[i].sat - 1].pt[0][f] = obs[i].time;
```

`rtkpos()` calls `pntpos()` with `rtk->ssat` for the rover SPP seed at the top
of every epoch. `ph[0]`/`pt[0]` are the engines' *previous-epoch*
carrier-phase bookkeeping — so when `relpos()` subsequently ran its cycle-slip
detectors, the stored time was already the current epoch and the
`timediff < DTTOL` guard skipped **every rover satellite**. Rover-side LLI
(`detslp_ll`) and Doppler (`detslp_dop`) slip detection were silently dead in
relative RTK for six minor releases; only the geometry-free detector survived,
and the base side (rcv=2) was unaffected. Undetected slips carried
integer-cycle phase-bias errors into fix-and-hold — wrong fixes with clean
residuals, and corrupted float states that suppressed re-fixing.

The release had claimed "bit-identical when the new options are off"; this
single store sat outside every gate. `pppos()`'s `detslp_dop()` reads the same
slots and was equally dead for PPP configurations with
`[slip_detection] doppler > 0` (the benchmark MADOCA config leaves it unset,
which is why MADOCA stayed bit-identical across the regression — and why it
worked as the control that proved the comparison itself had not drifted).

### Fix

[PR #320](https://github.com/h-shiono/MRTKLIB/pull/320) moves the SPP TDCP
previous-phase snapshot to **pntpos-private `spt[]`/`sph[]` fields** in
`ssat_t`. The TDCP machinery (including the CLAS `enhanced_spp_seed` profile,
which enables TDCP in its private option copy) reads identical values from its
own slots, so SPP and seed behaviour are unchanged; the engines' `ph`/`pt`
bookkeeping is never touched by `pntpos()` again. Two files, +26/−16 lines.

### Validation

PPC-Dataset, 6 cases × 3 modes, same environment as the v0.7.7 record:

| case | RTK Fix% (v0.4.1 → v0.7.7 → v0.7.8) | RTK RMS_2D(fix) (v0.4.1 → v0.7.7 → v0.7.8) |
|---|---|---|
| nagoya_run1 | 27.4 → 28.9 → **27.4** | 0.425 → 1.002 → **0.404 m** |
| nagoya_run2 | 28.3 → 7.4 → **28.3** | 1.014 → 1.849 → **1.014 m** |
| nagoya_run3 | 10.1 → 0.5 → **10.1** | 0.716 → 2.049 → **0.716 m** |
| tokyo_run1 | 2.9 → 3.4 → **2.9** | 0.555 → 2.068 → **0.555 m** |
| tokyo_run2 | 22.1 → 18.3 → **20.4** | 0.079 → 77.658 → **6.271 m** † |
| tokyo_run3 | 27.7 → 17.3 → **27.7** | 0.095 → 1.867 → **0.084 m** |

† the v0.4.1 *tag rebuilt on the same machine* also produces 20.4 % / 6.27 m;
the recorded 0.079 m was a single lucky draw of the ~11
Accelerate-nondeterminism flip epochs. Post-fix tokyo_run2 fixed tier: 1σ
0.014 m, 95 % 0.153 m, misfix 0.6 %.

**CLAS and MADOCA are unchanged**: all twelve case-mode combinations are
metric-identical before/after the fix (NMEA differences are the known
macOS/Accelerate last-digit noise, ≈0.2 mm). Structurally, PPP-RTK
(`ppp_rtk_pos()`) and VRS (`relposvrs()`) use their own slip detectors and
never read `ph[0]`/`pt[0]`. The full `ctest` suite passes with only the two
documented environment failures (`rtkrcv_rt`, `madocalib_pppar_ion_check`);
all claslib parity / absolute-accuracy tests pass and no tolerances changed.

### Benchmark tooling (#316 / #317)

The same release carries the benchmark-harness work that made #318 visible:

- **Hourly L6 sessions are concatenated per stream**, so drives crossing a
  UTC hour boundary keep their corrections for the whole run (previously
  `mrtk post` silently used only the first hour; e.g. tokyo_run3 MADOCA
  2 855 → 14 430 epochs). The decoders resynchronise on the 250-byte L6
  frame preamble, so concatenation is safe.
- **`<30cm` per-tier accuracy column** — on the FIX row its complement is the
  misfix rate; this is the column that exposed the false fixes.
- `-ts`/`-te` are now passed in the form `mrtk post` actually parses (the
  previous week/TOW pair was silently discarded).
- `plot_track_2d.py` side-by-side quality-coloured tracks, MADOCA L6D
  download, sustained-run TTFF / convergence metrics.

`docs/reference/benchmark.md` records the v0.7.7 full-suite results, the #318
root-cause note, and the corrected post-fix RTK table.

### Compatibility

No configuration changes. `ssat_t` gains two fields (`spt[]`/`sph[]`);
external drivers that replicate MRTKLIB struct layouts must rebuild against
the new headers (the struct is not binary-serialized anywhere in-tree).
