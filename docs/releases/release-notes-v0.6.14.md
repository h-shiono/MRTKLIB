# Release Notes — v0.6.14

## Real-time CLAS handover robustness + cssr2rtcm3 / VRS carrier-phase quality

**Release date:** 2026-06-10
**Type:** Real-time reliability fixes (CLAS PPP-RTK handover; cssr2rtcm3 / VRS-RTK output quality)
**Branch:** `release/v0.6.14`

---

### Overview

v0.6.14 bundles two independent real-time reliability fixes plus supporting
tooling and documentation:

1. **Real-time CLAS no longer freezes on a QZSS satellite handover**
   ([#197](https://github.com/h-shiono/MRTKLIB/issues/197),
   [#205](https://github.com/h-shiono/MRTKLIB/issues/205)) — `mrtk run` CLAS
   PPP-RTK could drop permanently to Single and never recover (until restart)
   once the QZS satellite the L6 demux was locked to set below the horizon.
   Fixed across both the demux (coherent per-PRN locking + handover re-lock) and
   the re-lock clock (now advances for RTCM2/RTCM3 rovers).

2. **cssr2rtcm3 / VRS carrier-phase quality**
   ([#97](https://github.com/h-shiono/MRTKLIB/issues/97),
   [#98](https://github.com/h-shiono/MRTKLIB/issues/98)) — the synthesized VRS/OSR
   carrier phase emitted by `mrtk cssr2rtcm3` leaked phase-bias-wrap jumps and
   stale signal-slot glitches that forced cycle slips on the downstream RTK
   rover. Both are now repaired, removing prime suspects for the fix-rate gap vs
   mosaic-CLAS and the vertical sawtooth.

There are **no positioning-algorithm or numerical changes** to the
post-processing engines; the CLAS post-processing regression and the static
VRS/OSR (`nf=2`) output are unchanged.

---

### 1. Real-time CLAS handover freeze (#197, #205)

#### Symptom

In real-time PPP-RTK (`mrtk run`, single-channel CLAS), the solution would
hold Fix for hours and then drop to Single with `age = 1e4` and never recover —
typically at a fixed-but-drifting time of day — even though the L6 stream kept
flowing. A process restart was the only recovery. With a multi-channel
(D9C-class) receiver feeding interleaved L6D frames, an earlier symptom was
"L6 Lost": corrections flowed in but never decoded.

#### Root cause(s)

The UBX/SBF single-stream L6D demux is where several QZS PRNs' L6D frames arrive
interleaved. Three defects compounded:

- **Stranded channel** (original #197) — the demux locked channel 0 to the
  first PRN seen; after a handover the new PRN's frames went to an unread
  channel and corrections stopped.
- **Subframe interleave corruption** — a naïve "collapse everything to ch0" mixed
  two PRNs' subframe sequences into one decode buffer, corrupting CSSR assembly
  ("L6 Lost") on a 2-channel receiver.
- **Re-lock clock stall (the "never recovers" root cause, #205)** — the demux
  was given a re-lock timeout to release a set satellite's stale lock, but the
  timeout clock used `svr->raw[0].time`. A decoded **RTCM2/RTCM3 rover** writes
  its observations into `svr->rtcm[0]`, leaving `svr->raw[0].time` frozen, so the
  timeout **never fired** and the channel stayed locked to the set satellite
  forever. (Cloud monitoring stations all use RTCM3 rovers, so they froze at the
  same instant — the moment their startup-locked PRN set — which looked
  constellation-wide.)

#### Fix

`clas_route_l6frame()` keeps each CLAS channel locked to a single coherent
active-source PRN, drops frames from any other PRN (so a 2-channel receiver does
not corrupt the subframe stream), and re-locks to a live PRN once the locked one
has been silent past a 10 s timeout. Channels are keyed by CLAS Transmit Pattern
ID, so two channels suffice no matter how many PRNs the receiver tracks
(single CLAS uses ch 0; dual `l6_margin` maps each pattern to its own channel
for merging). The re-lock clock now takes the fresher of `raw[0].time` /
`rtcm[0].time`, so it advances for any rover format.

*Clarification (added later): "for merging" here means each channel gets a
coherent, routable L6 stream. Genuine per-channel correction merging inside
the PPP-RTK filter (as opposed to demux-level routing / handover redundancy)
landed later, via [#303](https://github.com/h-shiono/MRTKLIB/issues/303)/#305.*

#### Verification

A live archive that froze on a real QZS handover (J195 set) was replayed through
an instrumented build: with the fix the channel re-locks J195 → J199 ~14 s after
the set, the per-epoch `age` recovers, and the solution stays Fix (Fix 90 %, max
`age` 16 s, zero `age ≥ 1e4` epochs) instead of freezing permanently. The fix
has also run 5 h+ continuously on live cloud stations with no freeze. Unit test
`utest_t_clas_route` covers the interleave-drop, handover re-lock and
dual-pattern routing. CLAS post-processing regression unchanged (15/15).

> **Scope:** real-time only (`mrtk run`). Post-processing CLAS (separate L6
> streams) and the L6E/MADOCA path are untouched.

---

### 2. cssr2rtcm3 / VRS carrier-phase quality (#97, #98)

`mrtk cssr2rtcm3` reconstructs a VRS/OSR base observation (RTCM3 MSM7 + 1005)
from CLAS corrections for a downstream RTK engine. Two carrier-phase defects in
that synthesis were forcing rover cycle slips:

#### 2a. Phase-bias-wrap repair was dropped (#207)

The ±100-cycle CLAS phase-bias-wrap repair computed in `clas_osr_zdres()` was
never applied to the emitted carrier. The port collapsed upstream claslib's two
compile-time output variants (`ENA_PPP_RTK` and `CSSR2OSR_VRS`) into one runtime
path but kept only the PPP-RTK ambiguity-state correction, dropping the VRS
output correction (`obs[].L += pbias_ofst`). Whenever a CLAS phase bias (ST5)
wrapped, ~100 cycles (~19 m at L1) leaked into the synthesized base carrier and
forced a cycle slip on the rover (Fix → Float). The repair is now applied in the
VRS fill — a single point covering `cssr2rtcm3`, `ssr2obs` and the rtkpos VRS
path — and is the prime suspect for the fix-rate gap vs mosaic-CLAS (#98) and the
vertical sawtooth (#97), consistent with Septentrio R&D feedback that the
reconstructed base-phase residual was "large and unstable."

#### 2b. Signal-slot desync and dead-satellite emission (#208)

The reused VRS dummy obs buffer could carry a stale per-signal code from a
previous epoch. The old guard only set the code when it was zero, so a stale
non-zero code desynced the OSR signal-slot order from `smode`: the carrier was
computed for one signal and emitted under another's slot. This surfaced on a
live G5 session as single-epoch ~1153 m carrier-phase glitches on Galileo E5a
whenever the receiver-tracked signal set differed from the CLAS-corrected set.
The fix sets the VRS obs codes unconditionally from `smode`, clears unused slots
(defensive for `nf < NFREQ`), and stops advertising satellites that have no
usable observation (emitted with the invalid MSM rough range 255). CLAS RTCM3
output is byte-identical for the shipped `nf=2` configuration.

#### 2c. New diagnostic — `osr_residual.py` (#206)

`scripts/analysis/osr_residual.py` is a no-ephemeris analyzer that decodes the
RTCM3 emitted by `mrtk cssr2rtcm3` and, per satellite, tracks combinations in
which geometry, orbit and satellite clock cancel — geometry-free phase (L1−L2),
code-minus-carrier, and lock-time — to surface carrier discontinuities, and
scans the MSM rough ranges to flag satellites advertised with an invalid range.
It root-caused both defects above (E33 E5a glitches; J194/J195 and several
GPS/Galileo sats emitted with no usable range).

---

### Other fixes

- **`mrtk relay -t` now writes a trace file** (#79, PR
  [#204](https://github.com/h-shiono/MRTKLIB/pull/204)) — `relay` did not create
  the runtime context, so `trace*()` no-opped. The context is now created when a
  trace level is requested and torn down on both exit paths; the
  `strsvrstart`-failure path also now frees its stream converters.
- **CI** retries transient `taplo` formatter download failures
  (PR [#203](https://github.com/h-shiono/MRTKLIB/pull/203)).
- **Docs**: single-port relay-back VRS setup guide for cssr2rtcm3
  (#117, PR [#202](https://github.com/h-shiono/MRTKLIB/pull/202)).

---

### Compatibility & migration

- **Real-time CLAS users** (`mrtk run`): no configuration change required — the
  handover fix is automatic and applies to UBX, SBF and RTCM-rover setups.
- **cssr2rtcm3 / VRS-RTK users**: no configuration change required; the emitted
  RTCM3 is byte-identical for the shipped `nf=2` configuration except where the
  phase-bias-wrap / stale-slot repairs remove the spurious carrier jumps.
- **Everyone else**: no post-processing positioning change.

### Tests

Full `ctest` regression: only the two documented pre-existing
environment/numerical failures (`rtkrcv_rt` headless replay,
`madocalib_pppar_ion_check` LAPACK tolerance); no new failures. New unit test
`utest_t_clas_route` for the L6 demux routing. CLAS post-processing /
VRS static cases pass; CLAS RTCM3 (`nf=2`) byte-identical. clang-format 21.1.6 /
taplo 0.10.0 clean; no test tolerances changed.

### Issues / PRs

- [#197](https://github.com/h-shiono/MRTKLIB/issues/197) /
  [#205](https://github.com/h-shiono/MRTKLIB/issues/205) — real-time CLAS
  handover freeze (PRs [#200](https://github.com/h-shiono/MRTKLIB/pull/200),
  [#201](https://github.com/h-shiono/MRTKLIB/pull/201),
  [#209](https://github.com/h-shiono/MRTKLIB/pull/209))
- [#97](https://github.com/h-shiono/MRTKLIB/issues/97) /
  [#98](https://github.com/h-shiono/MRTKLIB/issues/98) — cssr2rtcm3 / VRS
  carrier-phase quality (PRs [#207](https://github.com/h-shiono/MRTKLIB/pull/207),
  [#208](https://github.com/h-shiono/MRTKLIB/pull/208),
  [#206](https://github.com/h-shiono/MRTKLIB/pull/206))
- [#79](https://github.com/h-shiono/MRTKLIB/issues/79) — `mrtk relay -t` trace
  file (PR [#204](https://github.com/h-shiono/MRTKLIB/pull/204))
- [#117](https://github.com/h-shiono/MRTKLIB/issues/117) — single-port relay-back
  VRS docs (PR [#202](https://github.com/h-shiono/MRTKLIB/pull/202)); CI taplo
  retry (PR [#203](https://github.com/h-shiono/MRTKLIB/pull/203))
