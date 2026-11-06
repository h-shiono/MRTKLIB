# Release Notes — v0.7.3

## Real-time MADOCA-PPP PPP-AR/IONO — L6D wide-area ionospheric augmentation

**Release date:** 2026-06-29
**Type:** Feature (real-time MADOCA-PPP PPP-AR/IONO)
**Branch:** `release/v0.7.3`

---

### Overview

v0.7.3 wires the MADOCA-PPP **L6D wide-area ionospheric augmentation**
(QZS PRN 200/201) into the real-time `mrtk run` path. With this change,
`ionosphere = est-stec` + `iono_correction = on` applies the same slant-TEC
(STEC) constraint in real time as it already did in post-processing.

Previously this mode was **post-processing only**. In real time the L6E SSR was
decoded and applied, but the L6D iono augmentation was never routed into the
server — so an `est-stec` real-time run silently degraded to plain PPP-AR.

The design is **SBF-first** (mosaic-G5): a single SBF stream carries the
observations, the L6E SSR and the L6D ionosphere in one slot. The Septentrio SBF
L6D decoder steers PRN 200/201 to the iono decoder via a dedicated return code
that `rtksvr` feeds into `nav.pppiono`.

---

### 1. What's new

#### Real-time PPP-AR/IONO

`rtksvr` now decodes the L6D ionospheric frames (`QZSRawL6D`, PRN 200/201) into
`nav.pppiono` and applies the wide-area STEC constraint live, mirroring
`update_qzssl6d()` in the post-processing path.

The path is gated on:

- `correction = qzs-madoca`
- `ionosphere = est-stec`
- `iono_correction = on`

The GPST week is anchored lazily from the first stream time (the CLAS
`week_ref` pattern), so recorded-stream replay decodes in the correct week
([#223](https://github.com/h-shiono/MRTKLIB/issues/223),
[PR #233](https://github.com/h-shiono/MRTKLIB/pull/233)).

#### New sample configuration

- **`conf/madocalib/rtkrcv_madoca_pppar_iono.toml`** — a real-time single-SBF
  MADOCA-PPP PPP-AR/IONO sample configuration, plus a real-time subsection added
  to the MADOCA-PPP quickstart.

---

### 2. Changed

#### Septentrio SBF L6D routing

The `Source = 1` (L6D) branch now returns a dedicated decoder code for
PRN 200/201/197 (MADOCA ionosphere), so the frame is steered to the iono decoder
instead of the CLAS redirect.

The discriminator is the **PRN**: 200/201 carry the iono augmentation and are
disjoint from CLAS (193–199) per IS-QZSS-MDC-004 Table 3-1. There is no effect on
CLAS — in MADOCA mode the CLAS context is absent, and in CLAS mode PRN 200/201
were already rejected.

---

### 3. Validation

**Offline decode** of a mosaic-G5 SBF capture:

- 3660 L6D iono frames → 366 complete decodes → `nav.pppiono` regions populated.
- 89 L6E SSR satellites decoded from the same stream.

**Live mosaic-G5 hardware** (over TCP):

- The real-time iono path fires (`pppiono` regions filled).
- L6E SSR age ≈ 1 s.
- PPP-AR/IONO reached a fixed solution (Q=1) ≈ 1 minute after start, holding
  continuous Q=1 at **~1.2 cm horizontal / ~3.4 cm vertical std**.

The fast fix is the L6D ionospheric augmentation working as intended — the
same STEC constraint that post-processing already applied, now applied live.

---

### 4. Known limitations

- The augmentation is gated on `correction = qzs-madoca` +
  `ionosphere = est-stec` + `iono_correction = on`; outside that mode the L6D
  iono frames are not consumed.
- L6D iono routing keys on PRN 200/201 (disjoint from CLAS 193–199 per
  IS-QZSS-MDC-004 Table 3-1); PRN 197 is included in the dedicated return code.

---

### Compatibility & migration

No configuration changes are required for existing workflows. The real-time
iono augmentation is opt-in through the `est-stec` + `iono_correction = on`
gate; without it, `mrtk run` MADOCA-PPP behaves exactly as before. A new sample
configuration (`conf/madocalib/rtkrcv_madoca_pppar_iono.toml`) is provided for
the new mode.

### Issues / PRs

- [#223](https://github.com/h-shiono/MRTKLIB/issues/223) — wire the L6D iono
  (PRN 200/201) augmentation into the real-time `mrtk run` path.
- [PR #233](https://github.com/h-shiono/MRTKLIB/pull/233) — implementation
  (real-time L6D iono decode into `nav.pppiono`, lazy GPST week anchor,
  SBF L6D routing return code, sample configuration).
- [PR #235](https://github.com/h-shiono/MRTKLIB/pull/235) — release/v0.7.3.

### References

- IS-QZSS-MDC-004 — MADOCA-PPP L6E SSR and L6D wide-area ionospheric
  augmentation message formats; Table 3-1 PRN allocation (CLAS 193–199,
  iono 200/201).
- Septentrio SBF Reference Guide — `QZSRawL6D` (L6D) block layout and the
  `Source` field.
