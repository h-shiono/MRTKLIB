# Release Notes — v0.7.1

## Two-receiver QZSS L6 support (mosaic-CLAS / mosaic-G5) and single-SBF MADOCA-PPP

**Release date:** 2026-06-21
**Type:** Feature + fix (QZSS L6 decoding / real-time correction handling)
**Branch:** `release/v0.7.1`

---

### Overview

v0.7.1 makes the QZSS L6 decode path work uniformly across the two Septentrio
receivers used with MRTKLIB — the older **mosaic-CLAS** and the newer
**mosaic-G5** — and wires single-stream **MADOCA-PPP** into the real-time engine.

The two receivers emit the QZSS L6 signal under different SBF blocks:

| Receiver | L6D (CLAS) | L6E (MADOCA) |
|----------|------------|--------------|
| **mosaic-CLAS** | `QZSRawL6` (4069), `Source = 1` | `QZSRawL6` (4069), `Source = 2` |
| **mosaic-G5** | `QZSRawL6D` (4270) | `QZSRawL6E` (4271) |

Older firmware carries every L6 message in one generic `QZSRawL6` block and
distinguishes the signal with the SBF **`Source`** byte; newer firmware
pre-splits the stream into signal-specific `QZSRawL6D` / `QZSRawL6E` blocks. The
decoder previously routed purely by block ID and only handled 4069 (→ MADOCA)
and 4270 (→ CLAS), so it mishandled the other combinations.

This release routes the QZSRawL6 family by the `Source` field, decodes the
mosaic-G5 L6E block, and applies MADOCA L6E SSR carried in a raw SBF stream in
real-time. A single mosaic-G5 SBF stream (L6D + L6E) now drives **both**
`mrtk run` PPP-RTK (CLAS via L6D) **and** MADOCA-PPP (L6E SSR), and the same
change fixes two latent gaps for `mrtk run` and `mrtk cssr2rtcm3`.

There are **no positioning-algorithm or numerical changes**; MADOCA L6E SSR is
applied through the same `update_ssr()` / `nav->ssr_ch[0]` path already used by
RTCM3 and IGS-RTS SSR.

---

### 1. What's new

#### `Source`-field routing for the QZSRawL6 family

`decode_qzsrawl6()` (SBF) now dispatches on the SBF `Source` byte rather than the
block ID:

- **`Source = 1` (L6D / CLAS)** — copy the 250-byte frame and return the
  "L6D frame ready" code so the caller redirects it to the CLAS decoder.
- **`Source = 2` (L6E / MADOCA)** — decode the MADOCA L6E SSR inline.

All three block IDs (`QZSRawL6` 4069, `QZSRawL6D` 4270, `QZSRawL6E` 4271) route
through this one `Source`-aware handler; the duplicate `decode_qzsrawl6d()`
helper is removed. The same `Source` distinction holds on the UBX
`RXM-QZSSL6` path.

#### Real-time MADOCA L6E SSR from a raw SBF/UBX stream

The L6 decoders now return a **dedicated code (15)** for "MADOCA L6E SSR
decoded", separate from the "L6D frame ready" code (10) used for the CLAS
redirect. `rtksvr` applies code 15 via `update_ssr()` into `nav->ssr_ch[0]` —
the same path as RTCM3 / IGS-RTS SSR — but **only outside CLAS/PPP-RTK mode**,
where the CLAS decoder owns `nav->ssr_ch[0]`. As a result:

- In **MADOCA-PPP** (`mrtk run`, PPP mode), a single SBF stream is a valid
  correction source: the L6E SSR is decoded and applied.
- In **PPP-RTK** (CLAS), the same mixed mosaic-G5 stream uses the L6D frames for
  CLAS and leaves `ssr_ch[0]` untouched by L6E.

---

### 2. Bug fixes

#### CLAS from a raw mosaic-CLAS SBF stream

A mosaic-CLAS receiver emits CLAS L6D under the generic `QZSRawL6` (4069) block
with `Source = 1`. The previous block-ID routing sent 4069 unconditionally to
the MADOCA decoder, which rejected it by vendor ID and dropped it — so neither
`mrtk run` PPP-RTK nor `mrtk cssr2rtcm3` decoded anything from a raw mosaic-CLAS
SBF stream. `Source`-field routing fixes this.

#### L6E leak into `mrtk cssr2rtcm3`

`mrtk cssr2rtcm3` is a CLAS L6D → RTCM3 converter that identifies L6D frames by
the decoder return code. Because a completed L6E message previously returned the
same code (10) as an "L6D frame ready" — and the L6E path left a stale satellite
id — an L6E frame from a mixed mosaic-G5 stream (4270 + 4271 are emitted as a
per-satellite pair) could be fed into the CLAS decoder. The dedicated L6E code
(15) keeps cssr2rtcm3 (which acts on 10 only) limited to genuine L6D frames.

---

### 3. Validation

**Decoder replay** (real captures through `input_sbff`):

| Capture | L6D → CLAS | L6E SSR | Notes |
|---------|-----------|---------|-------|
| `G5P3100d.sbf` (mosaic-G5) | matches the exact `QZSRawL6D` block count (no L6E leak) | 93 satellites of MADOCA SSR | L6D and L6E cleanly separated |
| `MCLA100e` (mosaic-CLAS) | CLAS CSSR sets now decoded | — | previously dropped (returned 0) |

The SBF `Source` values were verified against the Septentrio SBF reference
(mosaic-CLAS 4069 `Source = 1`; mosaic-G5 4270 `Source = 1` / 4271 `Source = 2`).

**Real-time, mosaic-G5 hardware, single SBF stream, `mrtk run` MADOCA-PPP
(`PPP-kinema`):**

- The `ssr` input counter (the number of `update_ssr()` applications) climbs
  steadily, confirming MADOCA L6E SSR is applied continuously from the single
  SBF stream.
- Correction age settles to ~4 s.
- Float std converges to **~3 cm horizontal / ~2 cm vertical** with the position
  stable at the test location; AR ratio approaches the fix threshold.

**Test suite:** unchanged. The two pre-existing environment-dependent failures
(`rtkrcv_rt`, `madocalib_pppar_ion_check`) are unrelated and reproduce on a clean
`develop` build.

---

### 4. Known limitations

- The UBX `RXM-QZSSL6` L6E remap follows the SBF path by analogy and has not been
  exercised against a UBX L6E sample.
- MADOCA L6E SSR is applied to `nav->ssr_ch[0]` only outside CLAS/PPP-RTK mode;
  running a single stream simultaneously as CLAS *and* MADOCA is not a supported
  configuration (one positioning mode at a time).

---

### Compatibility & migration

No configuration changes are required. Existing CLAS (`mrtk run` PPP-RTK,
`mrtk cssr2rtcm3`) and MADOCA-PPP workflows are unaffected; the new behavior is
additive (mosaic-CLAS raw SBF now works, mosaic-G5 single-SBF MADOCA-PPP now
applies corrections). No positioning-algorithm, matrix, physical-constant, or
test-tolerance changes.

### Tests

`cd build && ctest --output-on-failure` — 90/91 passing on the reference
machine, the lone failure being the pre-existing `madocalib_pppar_ion_check`
LAPACK-vs-reference tolerance case.

### Issues / PRs

- [#219](https://github.com/h-shiono/MRTKLIB/issues/219) — RT MADOCA-PPP: SBF
  decoder ignores `QZSRawL6E` (4271); two-receiver QZSS L6 support.
- [PR #220](https://github.com/h-shiono/MRTKLIB/pull/220) — implementation
  (three commits: 4271 decode, `Source`-field routing, dedicated L6E return
  code).

### References

- Septentrio SBF Reference Guide — `QZSRawL6` (4069), `QZSRawL6D` (4270),
  `QZSRawL6E` (4271) block layouts and the `Source` field.
- IS-QZSS-L6 — QZSS L6D (CLAS) and L6E (MADOCA-PPP) message formats.
