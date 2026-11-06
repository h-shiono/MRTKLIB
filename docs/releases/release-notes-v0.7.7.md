# Release Notes — v0.7.7

## CLAS dual-channel correction merge — post-processing and real-time

**Release date:** 2026-07-29
**Type:** Feature (CLAS PPP-RTK dual-channel merge) + resilience + test-infrastructure
**Branch:** `release/v0.7.7`

---

### Overview

CLAS is broadcast in **two transmit patterns** (IS-QZSS-L6, "two L6 message
input"): two satellites carry complementary correction schedules from the same
facility, and each pattern alone contains only ~93 % of their union — per
epoch, roughly one satellite's atmospheric corrections exist in only one
pattern. MRTKLIB has accepted two `.l6` inputs since v0.3.0 and decoded both
into separate channels, but the PPP-RTK engine processed **channel 0 only**:
with `l6_merge` enabled the solution was bit-identical to feeding one file.

v0.7.7 completes the dual-channel story end to end
([#303](https://github.com/h-shiono/MRTKLIB/issues/303),
[#309](https://github.com/h-shiono/MRTKLIB/issues/309); PRs
[#304](https://github.com/h-shiono/MRTKLIB/pull/304),
[#305](https://github.com/h-shiono/MRTKLIB/pull/305),
[#308](https://github.com/h-shiono/MRTKLIB/pull/308),
[#310](https://github.com/h-shiono/MRTKLIB/pull/310),
[#311](https://github.com/h-shiono/MRTKLIB/pull/311)):

1. **The PPP-RTK engine genuinely merges both patterns** — per-channel
   zero-difference residuals, priority-channel selection by valid-observation
   count, per-satellite channel assignment with cross-channel fallback, and
   (for different-facility channels) per-channel reference-satellite search
   with phase-bias reset on channel switches, ported from upstream claslib.
2. **Correction fetch has OR semantics** — a channel that cannot supply
   corrections is skipped; the epoch fails only when every channel fails.
   `l6_merge` with a single L6 source now degrades gracefully to
   single-channel operation with a one-shot notice (previously: every epoch
   fell to Single).
3. **Real-time merge is on by default** in the dual-pattern-capable RT
   configurations. The transmit pattern is the channel identity in the L6
   demux, so the behaviour is adaptive with no runtime switching: merge when
   both patterns are visible, single-channel otherwise.

### Correctness anchor

The port's merge decisions were verified against an upstream claslib build on
identical data (2025/157, 3600 epochs): per-epoch valid-observation counts and
priority-channel selection, and all 10 795 double-difference calls, are
**byte-identical** between the two engines. The `l6mrg=0` path is
byte-identical to the pre-change binary on all bundled datasets.

### Measured effect (2025/157, station 0627 TSUKUBA3)

Post-processing, vs GSI F5 daily-coordinate truth (±7-day median):

| Solution | 2D RMS | 3D RMS | mean sats/fixed epoch |
|---|---|---|---|
| ch1 only | 4.75 cm | 5.63 cm | 14.64 |
| **merged** | 4.84 cm | **5.54 cm** | **15.58** |
| upstream ch1 only | 4.95 cm | 6.79 cm | 14.64 |
| upstream merged | 4.88 cm | 6.69 cm | 15.58 |

- **+0.94 satellites per epoch** — the single-pattern coverage gap, recovered.
  Satellite counts match upstream to two decimals; both engines move the same
  way on every metric.
- Vertical accuracy improves ~3 mm (3.02 → 2.71 cm RMS); horizontal, TTFF
  (6 s in every variant) and fix rate (99.86 %) are unchanged on this clean
  dataset. The merge's operational value is correction density and outage
  resilience rather than clean-sky accuracy.
- Distance to upstream's merged reference is 4.54 cm 3D RMS, floored by a
  pre-existing channel-2 correction-stream divergence tracked separately in
  [#306](https://github.com/h-shiono/MRTKLIB/issues/306) (absolute accuracy is
  unaffected — MRTKLIB is ahead of upstream on every variant above).

Real-time (x10 file replay of the same data): mean satellites per fixed epoch
14.59 → 15.55; no epoch stalls; fix rate within replay scatter. Real-time
replay is not run-to-run reproducible (3.0–3.3 cm 3D RMS between identical
runs), so the RT regression asserts the structural satellite-count gain
(threshold +0.5, measured +0.96 ± 0.07) instead of a position tolerance.

### Live SBF validation (mosaic-G5)

The mosaic-G5 single-SBF path was validated against a live NTRIP feed
(40-minute tagged capture, replayed with the merge on and off): the receiver
delivers CLAS L6D from two satellites with different patterns (PRN 199 /
pattern 0, PRN 194 / pattern 1), the pattern-keyed demux locks one channel per
pattern and routes every frame (4124 + 4143), and both channels assemble CSSR
epochs (3119 / 3158). Single-quality epochs drop 41 → 25 with the merge on;
fix rate is unchanged. On this capture the two patterns carried near-identical
satellite sets — the coverage gap between patterns is broadcast-dependent —
so the redundancy benefit appears without a satellite-count gain.
`rtkrcv_mosaic_g5.toml` ships with `l6_merge = 1` on this evidence.
`rtkrcv_sbf_l6d.toml` (single `.l6` file, one pattern by construction) keeps
the default 0.

### Configuration

`l6_merge` (in `[positioning.clas.resilience]`): `0` = off (channel 0 only),
`1` = merge with priority-channel selection, `2` = merge with load-balanced
selection. Now documented in the generated configuration reference. Default
remains `0` for post-processing (upstream parity: `rnx2rtkp` ignores the
second file by default); the RT CLAS configurations
(`rtkrcv_2ch.toml`, `rtkrcv_ubx_clas.toml`, `rtkrcv_rtcm3_ubx.toml`,
`rtkrcv_mosaic_g5.toml`) ship with `1`.

### Test-infrastructure changes

- The dual-channel regression is now **sensitive to the merge**. Before, the
  2ch check passed with or without the merge working (tolerance 0.21 m vs a
  ~cm merge signal). Now: tolerance tightened to 0.10 m (measured 4.54 cm,
  ~2.2× headroom — a deliberate, issue-mandated change), the fix-rate check is
  enabled, and a new differential check asserts the merged and single-channel
  outputs differ by ≥ 2 mm (working merge measures 1.77 cm; a disabled merge
  measures 0.015 cm and fails).
- `compare_nmea.py` gains `--expect-min-rms` (inverse assertion: two solutions
  must differ); non-positive thresholds are rejected.
- `run_rtkrcv_test.sh` gains `save_output` and `toml_overrides` arguments
  (one shipped conf serves both a default and an option-pinned run), and a
  bounded SIGTERM wait with a diagnostic before SIGKILL — a hung server now
  reports as a test failure instead of a silent CTest timeout.
- The RT suite grows by one replay (`rtkrcv_rt_clas_2ch_merge` +
  satellite-count check, ~6 min); the existing 2ch replay pins `l6_merge = 0`
  so the single-channel path keeps its own regression.

### Fixed

- `ddres()` `nb[]` sized for all six system groups as upstream does
  ([#307](https://github.com/h-shiono/MRTKLIB/issues/307)); latent —
  unreachable with `nf ≤ 3` — found by the #305 algorithm-safety review.

### Known limitations / open follow-ups

- **[#306](https://github.com/h-shiono/MRTKLIB/issues/306)** — the channel-2
  correction stream diverges from upstream ~1.8× more than channel 1's
  (dV median 3.4 vs 1.9 cm, measurable with `l6_merge = 0`); this floors the
  merged solution's consistency with upstream at ~4.5 cm on 2025/157. Open
  investigation; absolute accuracy is unaffected.
- **[#312](https://github.com/h-shiono/MRTKLIB/issues/312)** — the L6
  pattern demux (`clas_route_l6frame()`) has no CI replay coverage: the
  `clas`-format replay streams bypass it and the bundled 2ch dataset contains
  no PRN handover. Live validation covered the dual-lock path; a raw UBX/SBF
  fixture containing a real transmitting-satellite handover is the open item.
- Intermittent SIGTERM-ignored shutdown observed once in seven RT merge-run
  shutdowns under machine load; not reproducible in deliberate probes. The
  bounded shutdown wait makes any recurrence diagnosable.
