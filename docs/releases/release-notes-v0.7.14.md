# Release Notes — v0.7.14

## CLAS real-time robustness: L6 frame losses and the two-channel merge

**Release date:** 2026-11-02
**Type:** Bug-fix release (real-time CLAS PPP-RTK). **ABI change** in `raw_t` and `clas_l6buf_t`.
**Branch:** `release/v0.7.14`

---

### Overview

Five days of live CLAS PPP-RTK on a mosaic-G5 (DOY 2026-296 to 301) showed
the solution dropping to Single or to metre-level Float in ways that
post-processing of the same data did not. The receiver output over USB loses
1–2 s of SBF blocks on about 2.5 % of epochs, QZSRawL6 frames included, and
the CLAS decoder did not handle the lost frames well. Together with a fault
of the two-channel merge found on a separate one-hour capture, v0.7.14
fixes four real-time faults:

| Fault | Symptom | Issue |
|:------|:--------|:------|
| Subframe 1 lost | ~17 s of Single | [#406](https://github.com/h-shiono/MRTKLIB/issues/406) |
| Frame lost inside a subframe | garbage corrections: metre-level Float, false fixes, the [#381](https://github.com/h-shiono/MRTKLIB/issues/381) reset loop | [#363](https://github.com/h-shiono/MRTKLIB/issues/363) |
| Top-of-hour ST1 lost | messages dated one hour old | [#392](https://github.com/h-shiono/MRTKLIB/issues/392) |
| `l6_merge`, one channel a clock update behind | 2 epochs of Single + Float re-convergence | [#380](https://github.com/h-shiono/MRTKLIB/issues/380) |

claslib post-processing (8 cases) is byte-identical throughout.

### Lost subframe 1 ([#406](https://github.com/h-shiono/MRTKLIB/issues/406))

CLAS sends the global code bias (ST4) only in subframe 1. Networks whose ST6
is phase-only therefore have no other code bias. When subframe 1 of frame T
was lost, the correction set for T+30 paired phase bias T with code bias
T−30, which is past its 60 s validity. The set failed until the next network
phase bias (~T+47), and PPP-RTK fell back to the previous set, whose orbit
had expired.

The code bias of frame T+30 is now used. The existing consistency check
already allows a code bias up to 30 s newer than the phase bias, and the OSR
check now accepts the same window. The same substitution also applies when
the code bias was lost in two consecutive frames.

The correction set now pairs the code and phase biases by signal. A bank
entry orders its signals by the mask of its own epoch, and the old merge
paired the two by position, so a code bias from another mask could be
applied to a different signal. This could happen with the newer code bias
taken here, and also with one up to 30 s older, which upstream claslib
allows. No case occurs in the replayed data, whose output is byte-identical.

On three hours of live replay, 13 of 15 such outages shrink from 17 s to
1–3 s. The remaining 1–3 s is correct by the IS: orbit T−30 expires at T+31,
and orbit T+30 has not arrived yet. The other two outages had no valid
corrections at all (an IOD SSR change in the lost mask; consecutive losses).

### Frame lost inside a subframe ([#363](https://github.com/h-shiono/MRTKLIB/issues/363))

A CLAS subframe is five L6 frames, assembled by counting frames after the
subframe indicator. The L6 header has no frame counter, so after a lost
frame the next frames were appended at the wrong offset. The messages
decoded from them were garbage, for example STEC of several hundred TECU or
code biases metres off.

The receiver time of each frame is now passed to the CLAS decoder through
the new `clas_set_l6tag()`:

- SBF: QZSRawL6D / QZSRawL6 block TOW
- UBX: UBX-RXM-QZSSL6 time tag

`mrtk run` and `mrtk cssr2rtcm3` set it for every frame. When a frame does
not follow the previous one by 1 s, the rest of the subframe is discarded,
as was already done when the head frame was missing. Raw L6 byte streams and
`.l6` files carry no time and are unchanged.

**ST1 mask width check.** The check that decides whether the ST1 mask has
been received completely had two bit-position errors, the same as in
upstream claslib 0.8.0–0.9.0 (0.7.5 was right):

- it counted the header as 49 bits instead of 33, and read the number of
  GNSS from the satellite mask;
- for a GNSS with a cell mask, it read the satellite and signal masks again
  from the cell-mask bits.

A mask that fits in the first frame (272 bits in the captured example)
therefore waited for the second frame. Once lost frames discard the rest of
the subframe, that cost the whole mask. It is now decoded from the first
frame.

### Top-of-hour ST1 lost ([#392](https://github.com/h-shiono/MRTKLIB/issues/392))

ST2–ST12 carry only a 12-bit time within the hour, and only ST1 advanced the
full time base. When the top-of-hour ST1 was lost, the messages up to the
next ST1 were dated one hour old. A decoded time more than 30 min before the
last one now advances the base by an hour, wrapping at the end of the week.
Without this, the #406 recovery could not apply at the top of the hour.

### `l6_merge` reference satellite ([#380](https://github.com/h-shiono/MRTKLIB/issues/380))

With `l6_merge`, the reference-satellite search took ch0 whenever ch0 was
valid and skipped the satellite if ch0's clock was 5 s old, without trying
ch1. In real time, the other channel's frame with the new 5 s clock can be
decoded first. When every satellite was valid on the stale ch0, no reference
satellite was found ("no double-differenced residual"). The first channel
that is both valid and fresh is now used. The 5 s threshold is unchanged,
and post-processing, which reads both channels ahead, is unaffected.

### Validation

Live mosaic-G5 RT replays (`mrtk run`, file stream):

| Data | Change |
|:-----|:-------|
| DOY 296, 3 h, 15 subframe-1 losses (#406) | 13/15 outages 17–19 s Single → 1–3 s, then Float ~5 s → Fix; 183 new Fix epochs, H median 2.8 cm, none > 30 cm |
| DOY 296, hour with five #381 loops (#363) | Fix 92.2 % → 98.4 %; Float max error 58 m → 0.16 m |
| DOY 296, second hour (#363) | Fix 90.1 % → 92.5 %; false fixes > 30 cm 42 → 0; Float max 5.4 m → 0.20 m |
| DOY 296, third hour, no loss inside a subframe | byte-identical |
| 2026-10-18, simpleRTK4 Pro, 1 h, `l6_merge = 1` (#380) | Fix 97.48 % → 99.07 % (= `l6_merge = 0`); Float 71 → 17 epochs |
| Same, `l6_merge = 0` | byte-identical |

Where subframe 1 is damaged in two consecutive frames, no code bias within
its 60 s validity exists. The solution is now Single for ~30 s in that case,
instead of a Float metres off.

### Tests

- **`rtkrcv_clas_l6gap_{nomerge,merge}`** (new dataset `clas_l6gap-v1`,
  3.5 MB): RT replay of 20 min of the DOY 296 capture, in which 11–13 CLAS
  subframes per PRN have a frame missing inside them.
  - Gates, from the first Fix: fix rate ≥ 0.95, Fix / Float horizontal
    error ≤ 1 m, no Fix over 0.30 m, no gap over 5 s up to the end of the
    window.
  - Measured: fix rate 0.975, max horizontal error 0.09 m, in both merge
    settings. The previous decoder gets 0.83–0.89 and 27 m.
- **`rtkrcv_rt_clas_sf1gap`**: RT replay across lost subframes 1 (#406).
- **`utest_clas_l6`**: synthetic subframes for the frame-gap discard and the
  hour and week wrap; two captured frames for decoding ST1 from the first
  frame and the discard after a gap; a three-GNSS mask with full cell masks.
- **`utest_clas_bank`**: the bank after a lost subframe 1, and after the
  code bias was lost in two consecutive frames.

Each test fails with its fix removed. #380 has no regression test: the race
depends on timing and reproduced only when replaying the capture from its
start with every SBF block kept, not on shorter or block-filtered slices.

### Upgrade notes

- **External drivers** that embed `raw_t` or `clas_l6buf_t` must rebuild:
  `raw_t` gains `l6tag`; `clas_l6buf_t` gains `ftag`, `ftag_last` and
  `tow_last`.
- **New API:** `clas_set_l6tag(ctx, ch, tag)`. A driver that feeds L6 frames
  to the CLAS decoder itself gets the frame-gap check only if it calls this
  before each frame with the receiver time of that frame.
- Configuration and output formats are unchanged.

### Test status

Full suite: **191 / 191** (macOS, Accelerate). RT replay tests are
timing-sensitive and can false-fail under CPU contention; re-run them in
isolation.

### Next

- **v0.7.15:** BeiDou PPP-B2b.
- **Real-time CLAS, still open:**
  - L6 channel lock, relock and stream clocks: a lock kept on a degraded
    PRN ([#414](https://github.com/h-shiono/MRTKLIB/issues/414)), fix loss
    after a handover ([#389](https://github.com/h-shiono/MRTKLIB/issues/389)),
    the demux and rate-limit clocks
    ([#365](https://github.com/h-shiono/MRTKLIB/issues/365));
  - the CSSR phase discontinuity indicator
    ([#393](https://github.com/h-shiono/MRTKLIB/issues/393));
  - the reset loop and wrong ambiguity sets held after a reset, to be
    re-measured on this release
    ([#381](https://github.com/h-shiono/MRTKLIB/issues/381),
    [#394](https://github.com/h-shiono/MRTKLIB/issues/394)).
- **v0.8.x:** CLAS CMA authentication.
