# Release Notes — v0.7.18

## CLAS remnants and claslib 0.9.x alignment

**Release date:** 2026-11-20
**Type:** Maintenance release: CLAS VRS-RTK aligned with claslib 0.9.0,
CLAS PPP-RTK / real-time fixes, input-decoder and IGS post-processing fixes.
**No ABI change**; new public API in `mrtk_clas.h` and `mrtk_obs.h`.
**Branch:** `release/v0.7.18`

---

### Overview

v0.7.18 takes up the CLAS items left after the v0.7.14 / v0.7.15 real-time
series and the claslib 0.9.0 / 0.9.1 reviews; ambiguity-resolution quality
(#394, #538) stays open for v0.7.21.

- **VRS-RTK follows claslib 0.9.0.** The frequency pair is selected per
  system, so `qzs_frequency` takes effect and `l1+l2+l5` uses the third
  frequency. `mrtk ssr2obs` builds QZS L1C + L5 VRS observations.
- **PPP-RTK rides through short correction gaps.** On 4.9 days of live
  data replayed in real time, the fix rate goes from 93.91 % to 96.10 % and
  the misfixes over 0.3 m from 2511 to 1605.
- **CLAS applies satellite health and the SNR mask as claslib does.** The
  CSSR mask decoder rejects masks that would overrun its arrays.
- **Real time:** empty rover epochs are skipped, and an RTCM3 rover keeps the
  CLAS corrections.
- **Post-processing:** several L6D files per CLAS channel; IGS PPP-AR with
  `-ts` alone and IGS PPP without GPS work.
- **Input decoders:** half-cycle flags of u-blox and RTCM3 MSM in RTK, the
  slip of a blank RINEX phase, RINEX 3.05 GLONASS records, SBF QZSS J08–J10,
  and the QZS-1R / QZS-6 health bit.

| | |
|:--|:--|
| QZS L1C + L5 VRS, 2025/022, 1 h | fix 99.86 %, 2D 95 % 4.52 cm (claslib 0.9.1: 99.83 %, 4.97 cm) |
| PPP-RTK, 4.9-day RT replay, `l6_merge = 1` (#389) | fix 93.91 → 96.10 %, misfix > 0.3 m 2511 → 1605 |
| CLAS RT replay, RTCM3 rover | fix 55 → 3553 epochs, 2D 95 % 122 → 4.56 cm |
| Regression suite | 220 → 271 tests, all passing |
| Contributors | @hal1278 (#481, #505, #509); reports by @badaboom, @njkasdasd, @dj1711572002 |

---

### CLAS VRS-RTK

- **Frequency pair per system**
  ([#346](https://github.com/h-shiono/MRTKLIB/issues/346)). The VRS engine kept
  the selection from before claslib 0.9.0, which read `gps_frequency` for
  every system and always took slot 2 for Galileo. It now calls
  `clas_osr_selfreqpair()`, as claslib 0.9.0 `rtkvrs.c` does. QZS follows
  `qzs_frequency`. A rover with Galileo E1 but no E5a no longer
  re-initializes its Galileo ionosphere state on every epoch. With both
  options unset (all bundled configurations) the output is byte-identical.
- **`l1+l2+l5` and `l1+l5`**
  ([#534](https://github.com/h-shiono/MRTKLIB/issues/534)). An MRTKLIB-only
  fallback read the slot bitmask as an index. `l1+l2+l5` always ran as
  L1 + L2, and `l1+l5` used L2 for a satellite without L5. The fallback is
  removed, so both options behave as in claslib. The default stays L1 + L2.
- **QZS L1C + L5 from `mrtk ssr2obs`**
  ([#349](https://github.com/h-shiono/MRTKLIB/issues/349)). In the SSR2OSR modes
  (`ssr2obs`, `cssr2rtcm3`, `mrtk ssr2osr`) the signal index is the j-th CSSR
  signal of a satellite, as in claslib `CSSR2OSR_VRS`. The port had used the
  PPP-RTK frequency slots there:
  - QZS came out as L1C/A + L2C without L5;
  - GPS L5 was missing on every dataset;
  - the QZS carrier phase was NaN in 1,488 values of the hour.

  The signal loops, the wavelength ratio of the ionosphere compensation and
  the L1 selection (claslib 0.9.0 `getL1idx`) now follow the CSSR signals.
  The RINEX header lists the codes of every satellite in the corrections. New
  configurations `conf/claslib/ssr2obs_L1CL5.toml` and
  `conf/claslib/rnx2rtkp_L1CL5_vrs.toml`.

| 2025/022, 1 h, `ssr2obs` → VRS-RTK | MRTKLIB | claslib 0.9.1 |
|:--|:--|:--|
| QZS codes | L1C, L2C, L5 | L1C, L2C, L5 |
| Fix rate | 99.86 % | 99.83 % |
| 2D 95 % vs GSI F5 | 4.52 cm | 4.97 cm |
| 3D RMS to the upstream solution | 4.67 cm | — |

The parity gap (4.67 cm, against 2.3–3.2 cm on the other VRS cases) comes
from an ambiguity-resolution divergence in the last 20 minutes of the dataset
([#538](https://github.com/h-shiono/MRTKLIB/issues/538)). The bundled 2019/239
`ssr2obs` → VRS-RTK case gains GPS L5 and moves closer to upstream (3D RMS
5.33 → 4.87 cm).

---

### CLAS PPP-RTK and CSSR decoding

- **Short correction gaps**
  ([#389](https://github.com/h-shiono/MRTKLIB/issues/389)). When the
  corrections dropped out for a few epochs while the rover still had
  observations (for example at an L6 handover), the filter re-converged from
  scratch. Within 5 s of the last measurement update, the outage counters are
  now restored and the reset-on-failure is skipped. The claslib
  post-processing outputs are byte-identical. The `mcla_a` handover goes from
  82 to 14 non-fix epochs; the rest is ambiguity-resolution acceptance (#394).
  The 4.9-day replay is in the table below.
- **Pattern change with `l6_merge = 1`**
  ([#390](https://github.com/h-shiono/MRTKLIB/issues/390)). A silent channel
  now keeps its last facility in `check_same_fac()`, as in claslib, so the
  phase biases are not re-initialized. `mcla_e_merge`: 5 → 0 non-fix epochs.
- **Satellite health**
  ([#285](https://github.com/h-shiono/MRTKLIB/issues/285)). `clas_osr_zdres()`
  rejects unhealthy satellites, and for QZS the flagged L1 / L2C / L5 signal
  (claslib `satsigexclude()`).
- **SNR mask**
  ([#479](https://github.com/h-shiono/MRTKLIB/issues/479)). The CLAS PPP-RTK
  and VRS checks used claslib's 0.25 dBHz unit on MRTKLIB's 0.001 dBHz values,
  so the mask never applied. It does now.
- **Two L1 signals with `iono_compensation = "ssr"`**
  ([#345](https://github.com/h-shiono/MRTKLIB/issues/345)): a NaN dropped the
  satellite; the L1–L1 pair is skipped, as in claslib 0.9.0.
- **CSSR ST1 mask bounded**
  ([#276](https://github.com/h-shiono/MRTKLIB/issues/276)). A mask with more
  than 64 satellites, a repeated GNSS ID or a satellite without a number is
  dropped instead of writing past the decoder arrays.
- **Grid selection crash**
  ([#481](https://github.com/h-shiono/MRTKLIB/pull/481), @hal1278) when the
  number of selected grid points dropped.

| 4.9-day RT replay (416,004 epochs) | fix | misfix > 0.3 m |
|:--|:--|:--|
| `l6_merge = 1`, before → after #389 | 93.91 → 96.10 % | 2511 → 1605 |
| `l6_merge = 0`, before → after #389 | 93.78 → 95.86 % | 1835 → 1547 |

---

### Real time

- **Empty rover epochs**
  ([#382](https://github.com/h-shiono/MRTKLIB/issues/382)). An epoch with no
  rover observations (a total signal blockage) no longer goes to `rtkpos()`
  and is no longer written under the previous epoch's time.
- **RTCM3 rover with CLAS**
  ([#365](https://github.com/h-shiono/MRTKLIB/issues/365)). The CLAS rate
  limit, week anchors and handover demux read the clock of the stream the
  rover decodes into. Before, an RTCM3 rover lost the corrections after 60 s.

---

### Post-processing

- **Several L6D files per CLAS channel**
  ([#373](https://github.com/h-shiono/MRTKLIB/issues/373)). `mrtk post`
  builds the channels from the contents of the files (transmit pattern, ST1
  epochs), so the per-PRN files of `mrtk l6extract` can be given directly
  across a QZS handover. See
  [CLAS L6 channels from several files](../design/clas-l6-multi-file.md).
- **Bias-SINEX with `-ts` alone**
  ([#475](https://github.com/h-shiono/MRTKLIB/issues/475)): IGS PPP-AR loaded
  no phase bias unless both `-ts` and `-te` were given.
- **IGS PPP without GPS**
  ([#476](https://github.com/h-shiono/MRTKLIB/issues/476)). The receiver clock
  was initialized from the SPP GPS term, so a run without GPS on a receiver
  with a large clock bias produced no epoch.

---

### Input decoders

- **RTK half-cycle state**
  ([#160](https://github.com/h-shiono/MRTKLIB/issues/160)). u-blox `LLI_HALFS`
  (0x80) leaked into the stored previous LLI and flagged a half-cycle slip on
  every epoch. On the reporter's F10N capture: 0 → 1,326 fixed epochs of
  1,800.
- **RTCM3 MSM half-cycle indicator**
  ([#531](https://github.com/h-shiono/MRTKLIB/issues/531)) is decoded as
  `LLI_HALFC` (it added 3 to the LLI), as demo5 does.
- **Blank RINEX phase**
  ([#504](https://github.com/h-shiono/MRTKLIB/issues/504)): its slip is carried
  to the next valid phase.
- **RINEX 3.05 navigation with 3-line GLONASS records**
  ([#482](https://github.com/h-shiono/MRTKLIB/issues/482)): every second
  record was lost.
- **SNR mask with no C/N0**
  ([#477](https://github.com/h-shiono/MRTKLIB/issues/477)): an unknown C/N0 is
  no longer masked.
- **SBF QZSS J08–J10**
  ([#471](https://github.com/h-shiono/MRTKLIB/issues/471),
  [#505](https://github.com/h-shiono/MRTKLIB/pull/505), @hal1278).
- **QZS-1R / QZS-6 health**
  ([#512](https://github.com/h-shiono/MRTKLIB/issues/512),
  [#485](https://github.com/h-shiono/MRTKLIB/issues/485)): the health bit of
  the L1 signal that is not broadcast no longer excludes the satellite.

---

### Tools, tests and documentation

- **CLAS support files:** `scripts/tools/make_clas_atx.py` and
  `scripts/tools/update_igu_erp.py` regenerate the claslib antenna and ERP
  files ([#478](https://github.com/h-shiono/MRTKLIB/pull/478);
  [CLAS support files](../design/clas-support-files.md)).
- **CLAS SSR validity:** design record of the 60 s orbit / clock limits
  ([#416](https://github.com/h-shiono/MRTKLIB/issues/416);
  [CLAS SSR orbit / clock validity](../design/clas-ssr-validity.md)).
- **CLAS RT replay tags** follow the ST1 epochs in the files, in step with
  the observations ([#515](https://github.com/h-shiono/MRTKLIB/issues/515)).
- **`assert()` unit tests** run with their asserts
  ([#277](https://github.com/h-shiono/MRTKLIB/issues/277),
  [#509](https://github.com/h-shiono/MRTKLIB/pull/509), @hal1278).
- **New dataset** `rtksvr_empty-v1` (352 KB SBF, #382).

---

### Upgrade notes

- **VRS-RTK options:** `gps_frequency` no longer moves QZS; set
  `qzs_frequency` for QZS. `l1+l2+l5` now uses the third frequency, and
  `l1+l5` runs a satellite without L5 on L1 alone (use `l1+l5(l2)` for the L2
  fallback).
- **SNR mask:** on the CLAS PPP-RTK and VRS paths the mask now rejects
  satellites; configurations that set a mask there see fewer satellites. On
  every path an observation without C/N0 is no longer masked.
- **`mrtk ssr2obs`** writes GPS / QZS L5 and one QZS L1 code (L1C/A, or L1C
  with `-JL1X`).
- **`mrtk post` with CLAS L6D files:** with more than two files, two files of
  one pattern, or `l6_merge = 0` with two or more files, the channels are
  built from the file contents. Files that were silently ignored are now used.
- **RTCM3 MSM half-cycle indicator:** `mrtk convert` writes RINEX LLI 2 / 3
  where it wrote 3 / 4.
- **API:** new `clas_l6_scan_file()`, `clas_l6_free_file()`,
  `clas_l6seq_build()`, `clas_l6seq_input()`, `clas_l6seq_free()`,
  `clas_osr_vrs_codes()` and `mrtk_claslib_qzs_l1_priority()`. No struct
  changes layout.

### Test status

Full suite: **271 / 271** (macOS, Accelerate, `-j16`, 486 s). RT replay tests
are timing-sensitive and can false-fail under CPU contention; re-run them in
isolation. The RT console ports are now 52003–52023.

### Known / open

- VRS-RTK ambiguity resolution diverges from claslib in the last 20 minutes
  of 2025/022 ([#538](https://github.com/h-shiono/MRTKLIB/issues/538)).
- `mrtk run` VRS-RTK assumes the claslib slot layout, which only `mrtk post`
  applies ([#535](https://github.com/h-shiono/MRTKLIB/issues/535)).
- PPP-RTK ambiguity-resolution acceptance after a reset
  ([#394](https://github.com/h-shiono/MRTKLIB/issues/394)).
- The bundled CLAS test files are still the claslib 0.9.0 versions
  ([#358](https://github.com/h-shiono/MRTKLIB/issues/358)).
- The MADOCA part of the stream-clock fix
  ([#359](https://github.com/h-shiono/MRTKLIB/issues/359)).
- `mrtk ssr2obs` takes the RINEX header types from the corrections at the
  first output epoch; a code added to the CSSR mask later in the run is not
  written ([#541](https://github.com/h-shiono/MRTKLIB/issues/541)).
- Several L6D files per channel: with same-pattern files overlapping in time,
  a lost ST1 together with a long frame gap can make the reader step back in
  time ([#542](https://github.com/h-shiono/MRTKLIB/issues/542)).

### Next

- **v0.7.19:** IGS / CNES / Galileo HAS remaining items.
- **v0.7.20:** MADOCA remaining items.
- **v0.7.21:** CLAS PPP-RTK ambiguity-resolution quality (#394, #538).
- **v0.7.22:** `cssr2rtcm3` items (#453, #97).
- **v0.8.x:** CLAS CMA authentication; MALIB v1.3.0 sync.
