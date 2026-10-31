# Release Notes — v0.7.13

## MADOCALIB 2.2 sync: the BeiDou PRN reassignment

**Release date:** 2026-10-31
**Type:** Upstream sync + bug-fix release. **Configuration action needed if you enable BeiDou.** **ABI change** in `prcopt_t`.
**Branch:** `release/v0.7.13`

---

### Overview

Between **2026-04-10 and 2026-04-21** BeiDou retired most BDS-2 satellites
and moved BDS-3 satellites onto the PRNs they freed:

- C01–C08 and C11–C14 are now BDS-3.
- BDS-2 remains only on C09 and C10.
- The BDS-3 IGSO satellites moved from C38–C40 to C06–C08, and C38–C40 are now MEO.

MRTKLIB, like MADOCALIB up to 2.1, treated every BeiDou PRN below 19 as
BDS-2. On current data this gave those satellites the wrong signal and
frequency model, put them in the BDS-2 receiver-clock group, and placed them
in the wrong PPP-AR groups.

MADOCALIB 2.2 (2026-10-30, "Support IS-QZSS-MDC-005") makes the
classification configurable. v0.7.13 ports that change, its MDC-005 decoder
change, and fixes found while porting.

The PRN table and the rules for choosing a value are in the new
[BeiDou PRN Reassignment](../guide/beidou-prn-reassignment.md) guide.

### BeiDou BDS-2 / BDS-3 classification ([#401](https://github.com/h-shiono/MRTKLIB/issues/401))

| Key | Section | Meaning | Default |
|:----|:--------|:--------|:--------|
| `bds2_sats` | `[positioning]` | BDS-2 satellites in PRN 1–18 | none (all BDS-3) |
| `bds3_igso_sats` | `[ambiguity_resolution]` | BDS-3 IGSO satellites (PPP-AR) | `C06-C08` |

- **Defaults.** They are the same as MADOCALIB 2.2. Legacy `.conf` files use
  `pos2-bds2prn` / `pos2-bds3igso`.
- **List syntax.** Entries are satellite IDs or ranges, e.g. `["C09", "C10"]`
  or `["C01-C18"]`.
- **Invalid lists.** A malformed entry or a list longer than 255 characters
  is rejected with a warning. It is not partly applied. For
  `mrtk bias -bds2prn` a list over 1023 characters is an error.
- **The IGSO list replaces a hard-coded rule.** PPP-AR used C38–C40 as IGSO.
  The IGSO satellites are never taken as the PPP-AR reference satellite and
  are dropped first in partial AR.
- **When the setting is applied.** `mrtk post` applies it before reading the
  input files, and `mrtk run` applies it at startup.
- **Warning.** Both print a warning when BeiDou is enabled and `bds2_sats` is
  absent. `bds2_sats = []` states "no BDS-2" and silences the warning.
- **`mrtk bias`** reads no configuration file. It gains `-bds2prn LIST`
  (`--bds2-sats`), the same name as the MADOCALIB 2.2 `cssr2ssr` option
  ([#403](https://github.com/h-shiono/MRTKLIB/issues/403)).

**Bundled configurations** are set for the data they process:

| Value | Configurations |
|:------|:---------------|
| `C01-C18` / `C38-C40` | MADOCALIB 2.0 sample, PPC / GSDC benchmarks |
| `C09, C10` | MADOCALIB 2.2 sample, live `mrtk run`, IGS test data (2026-05) |

### MADOCA-PPP decoder (IS-QZSS-MDC-005)

- **BDS-3 satellites in GNSS ID 3.** MDC-005 may send a BDS-3 satellite on
  PRN 1–18 in GNSS ID 3. Its code and phase biases are now read with the
  BDS-3 (GNSS ID 7) signal table.
- **Out-of-bounds write for an undefined signal.** A signal-mask bit that is
  undefined in the table in use used to index `cbias[-1]` / `pbias[-1]`, an
  out-of-bounds stack write. It is now skipped.
- **Sparse cell masks** (also in MADOCALIB 2.2). A skipped cell left the
  cell-mask cursor where it was. With a sparse cell mask, the decoder then
  read a bias for an absent cell and shifted every later bias in the message.
- **Unsupported satellites** (also in MADOCALIB 2.1 / 2.2,
  [#402](https://github.com/h-shiono/MRTKLIB/issues/402)). A masked satellite
  that MRTKLIB cannot number had its biases written to `ssr[-1]`, over the
  `rtcm_t` members before `ssr[]`.

The current L6E broadcast and the bundled data do not trigger these faults:
all MADOCA regression outputs are byte-identical before and after.

### Validation

**Unchanged outputs.** On an internal-LU build, before vs after:

- **Bundled data:** all 13 MADOCA-PPP and IGS regression outputs are byte-identical.
- **PPC benchmark:** the SPP and RTK outputs of all six runs are byte-identical.

These datasets predate the reassignment, and their configurations are set to
the old rule.

**Live data (2026-10-13, 1 h).** The before/after numbers come from one
1-hour session with a mosaic-G5. The reference is a GPS + Galileo static PPP
over the same session.

| Case | Change |
|:-----|:-------|
| SPP | 2D RMS 87.3 → 81.6 cm; 3D95 246 → 239 cm |
| IGS kinematic PPP (B1I + B2a) | C14 gained; 2D RMS 3.71 → 3.19 cm; 3D95 10.99 → 12.02 cm (worse) |
| IGS PPP-AR, static (B1C + B2a) | 2D RMS 5.16 → 3.82 cm; 3D95 9.33 → 7.97 cm |
| IGS PPP-AR, kinematic (B1C + B2a) | 2D RMS 7.61 → 6.18 cm; 3D95 25.6 → 28.2 cm (worse); fixed epochs 12 → 34 of 123 |

The CODE MGEX biases cover BeiDou only for the BDS-3 B1C / B2a signals.
They list C06–C08 and C11–C14 among the BDS-3 satellites.

### Tests

- **MDC-005 regression on the MADOCALIB 2.2 sample** (MIZU, 2026-03-28,
  L6E PRN 207; dataset `madocalib_mdc005-v1`): PPP and PPP-AR.
  - **Upstream parity.** On the same platform, MRTKLIB is bit-identical to
    upstream 2.2.
  - **Parity tolerances.** PPP 0.16 m, PPP-AR 0.090 m. The references come
    from upstream built on macOS. Upstream built on Linux (gcc / glibc)
    differs from them by up to 3D RMS 12.07 cm, because the PPP float
    solution has not yet converged. The tolerances cover that platform
    difference.
  - **Absolute accuracy** against the IGS week-2411 SINEX gates accuracy:
    2D 95 % ppp 6.0–7.1 cm, pppar 1.4 cm.
- **`utest_mcssr_bd3`.** A synthetic ST1 / ST4 / ST5 message covering:
  - BDS-3 in GNSS ID 3
  - an undefined signal
  - a sparse cell mask
  - an unsupported satellite
  - the BeiDou generation API

  Each decoder fault fails this test on the old code.
- **`utest_t_toml`.** Covers the new keys and the absent / `[]` / invalid /
  over-long cases.
- **`madocalib_pppar_ion_l6d_template`.** It no longer compares two runs byte
  for byte, which failed intermittently on Accelerate
  ([#404](https://github.com/h-shiono/MRTKLIB/issues/404)). It checks that no
  `.l6d` input is ignored, that both runs produce the same solution epochs,
  and that the solutions agree within the `pppar_ion` tolerance.

### Upgrade notes

- **If your configuration enables BeiDou, set `bds2_sats`:**
  - `["C09", "C10"]` for data after 2026-04-21;
  - `["C01-C18"]` for data before 2026-04-10, plus
    `bds3_igso_sats = ["C38-C40"]` for PPP-AR;
  - for 2026-04-10 to 04-21 see the guide (no single list is right).

  Without the key, every PRN 1–18 satellite is BDS-3, which differs from
  v0.7.12 for pre-reassignment data.
- **`mrtk bias`:** pass `-bds2prn` in the same way.
- **External drivers** that embed `prcopt_t` must rebuild: the struct gains
  `bds2prn`, `bds3igso` and `bds2prn_set` (appended).
- **New API:** `set_bds2prn()`, `is_bds2prn()`, `set_bds3igso()`,
  `is_bds3igso()`, `bds_gen()`, `set_bdsopt()`.

### Also in this release

The repository was repositioned as a library for open PPP / PPP-RTK
correction services ([PR #399](https://github.com/h-shiono/MRTKLIB/pull/399)):

- new GitHub description;
- a "Supported Correction Services" table in the README;
- an updated `.zenodo.json` description, which reaches Zenodo with this
  tagged release.

### Test status

Full suite: **180 / 180** (macOS, Accelerate). RT replay tests are
timing-sensitive and can false-fail under CPU contention; re-run them in
isolation.

### Next

- **v0.7.14:** BeiDou PPP-B2b.
- **v0.7.15:** SouthPAN PVS (under evaluation).
- **v0.8.x:** CLAS CMA authentication.
- **Still open:** full TOML save for `mrtk run`
  ([#342](https://github.com/h-shiono/MRTKLIB/issues/342)) and several PRN
  files per CLAS channel in post-processing
  ([#373](https://github.com/h-shiono/MRTKLIB/issues/373)).
