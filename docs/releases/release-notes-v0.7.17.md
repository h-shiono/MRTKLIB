# Release Notes — v0.7.17

## SouthPAN PVS

**Release date:** 2026-11-07
**Type:** Feature release (new correction service), plus three fixes and a
capability matrix of the open correction services.
**No ABI break** (`sbsmsg_t.band` uses a padding byte); new public API in
`mrtk_sbas.h`.
**Branch:** `release/v0.7.17`

---

### Overview

v0.7.17 adds **SouthPAN PVS** (PPP via SouthPAN), the precise point
positioning service of the Australian / New Zealand SBAS, broadcast on L5 by
GEO PRN 122 and free of charge. `correction = "southpan-pvs"` runs float PPP
on GPS and Galileo in post-processing (`mrtk post`) and in real time
(`mrtk run`).

With PVS, MRTKLIB runs all five open satellite-broadcast PPP / PPP-RTK
services in post-processing and in real time: QZSS CLAS, QZSS MADOCA-PPP,
Galileo HAS, BeiDou PPP-B2b and SouthPAN PVS. The new
[Correction Services](../reference/correction-services.md) reference page
lists, for each service and for IGS products / IGS-RTS:
- modes and constellations;
- receiver inputs;
- the release that added it;
- the regression tests that cover it.

| | |
|:--|:--|
| Corrections | ECEF orbit and clock (DFMC L5 MT32, PVS-flagged when `tD/16` is odd) |
| Mode | float PPP (no phase biases broadcast) |
| Constellations | GPS L1 C/A + L5 (LNAV), Galileo E1 + E5a (F/NAV) |
| Input | SBF `GEORawL5` (4021) + observations; `.sbs` files in post-processing |
| Issue | [#418](https://github.com/h-shiono/MRTKLIB/issues/418) |

---

### SouthPAN PVS ([#418](https://github.com/h-shiono/MRTKLIB/issues/418))

**How MRTKLIB handles it.**

- **Decoder:**
  - `sbsupdatepvs()` takes the MT32 messages of PRN 122 with odd `tD/16` for
    the GPS and Galileo slots.
  - The ECEF orbit / clock corrections go to the SSR store with vendor
    `SSR_VENDOR_PVS`.
  - Other DFMC GEOs send MT32 for their own service and are not used. In
    Japan these are the BDSBAS PRNs 130 / 143 / 144.
- **Corrections:** `satpos_ssr()` applies the corrections in three steps:
  - it adds them in ECEF, without the radial / along / cross rotation;
  - it accepts each for 100 s from its reception (SBAS-STN-0001 5.3.2);
  - it removes the GPS TGD. The DFMC clock refers to the L1/L5 iono-free
    clock. Without this, GPS-only PPP on ALBY has a 5.7 m vertical bias.

  `corr_meas()` applies no satellite bias.
- **Options:**
  - Needs `satellite_ephemeris = "brdc+ssrapc"` (or `brdc+ssrcom`) and the
    GPS L1/L5 + Galileo E1/E5a signal pairs.
  - Ambiguity resolution and other signal pairs are rejected.
  - In `mrtk post`, backward and combined solutions are rejected too,
    because a correction is valid only after its reception.
- **Real time:** the PVS corrections own the SSR store. MADOCA or RTCM3 SSR
  arriving on the same streams is not applied.
- **SBAS L5 messages:**
  - `sbsmsg_t` gains `band` (`SBS_BAND_L1` / `SBS_BAND_L5`).
  - SBF `GEORawL5` (4021) is decoded. `mrtk convert -s` writes L5 messages
    to `.sbs`.
  - The `.sbs` reader recognises them, including cssrlib `*_sbas.txt` files.
  - L5 messages never reach the L1 SBAS decoders.
- **DAS logs:** `scripts/tests/das2sbs.py` converts SouthPAN Data Access
  Service logs (SISNeT `*MSG` lines) to `.sbs`. The DAS path is not
  validated ([#462](https://github.com/h-shiono/MRTKLIB/issues/462)). A
  real-time DAS input is not implemented.
- **Samples:**
  - configurations `conf/pvs/ppp_southpan_pvs.toml` (post) and
    `conf/pvs/run_southpan_pvs.toml` (real time);
  - guide: [Quick Start: SouthPAN PVS](../guide/quickstart-southpan-pvs.md);
  - design: `docs/design/southpan-pvs.md`.

**Accuracy** (float PPP, last 20 min of one hour, against the RINEX header
position):

| Case | 2D rms | U rms |
|:--|:-:|:-:|
| ALBY (Albany, AU) 2025-08-21, kinematic | 6.0 cm | 12.6 cm |
| ALBY, static | 8.0 cm | 6.4 cm |

About 12 satellites were used per epoch. One hour is shorter than the IOC
convergence target (≤ 80 min to HPE 0.375 m / VPE 0.525 m, 95 %), so these
numbers are not a service performance figure.

A DAS log of ALIC (2025-04-20) ends about 1 m off, in MRTKLIB and in cssrlib
alike. The reference position agrees with IGS SINEX to about 1 cm. A
CODE-products PPP of the same observations ends within 0.17 m. So the offset
is in the DAS-logged corrections of that hour (#462).

---

### Fixes

- **SBF `GALNav` (4002) F/NAV** is stored in ephemeris set 1, as
  `GALRawFNAV` does. Before, it used set 0 and overwrote the I/NAV ephemeris:
  `mrtk convert` of a one-hour mosaic-G5 capture wrote 1 F/NAV record instead
  of 29. The ephemeris de-duplication (`uniqeph()`) also no longer merges a
  Galileo I/NAV and an F/NAV record of the same IODnav.
  - HAS and CLAS regression outputs are bit-identical.
  - MADOCA outputs stay within their ≤ 6 mm run-to-run variation, with fix
    quality unchanged.
  - Tolerances are unchanged.
- **`mrtk run` with `gal-has` or `bds-b2b`**
  ([#464](https://github.com/h-shiono/MRTKLIB/issues/464)): MADOCA L6E SSR
  (UBX / SBF L6 frames, raw L6E stream) and RTCM3 SSR no longer overwrite the
  HAS / PPP-B2b corrections in `nav.ssr_ch[0]`. The fault showed on one
  mosaic-G5 SBF stream logging GALRawCNAV and QZSRawL6E. A one-hour HAS
  replay ended −11.8 / −0.3 / −31.1 m dENU and now ends
  −0.11 / −0.13 / −0.17 m, the same as the capture without its L6 blocks.
- **`[output] geoid_model`** is read again. v0.7.16 defined the value list of
  `bds_geo_sats` under the macro name of the geoid model list, so every
  `geoid_model` value was rejected and the internal model was used. The
  bundled configurations all use `internal`, so their output is unchanged.

---

### Documentation

- New reference page
  [Correction Services](../reference/correction-services.md)
  ([#419](https://github.com/h-shiono/MRTKLIB/issues/419)).
- README and docs home:
  - The introduction names the five services.
  - "Key Architectural Overhauls" becomes "Changes from RTKLIB", with the
    platform statement matched to what is tested: CI on Linux without the
    real-time replays, the full suite on macOS (Apple Silicon).
  - The build badge shows the workflow status.
  - Known Limitations cover HAS, PPP-B2b and PVS.
  - "Running Applications" runs on the bundled CLAS sample.
  - The AI-assistance statement describes the actual process.
- The LP64 BLAS / LAPACK details move to the installation guide.

---

### Tests

- **New dataset `southpan-v1`** (4.6 MB, `h-shiono/mrtklib-data`): ALBY
  2025-233 h (signal-in-space log) and ALIC 2025-110 f (DAS log) from
  cssrlib-data (MIT; SouthPAN data CC BY 4.0), with cut DLR broadcast
  navigation. See `tests/data/southpan/README.md`.
- **New regression tests:**
  - `pvs_alby_kin`, `pvs_alby_static`, `pvs_alic_das2sbs`, `pvs_alic_kin`
    (in the service area);
  - `pvs_sbs_check`, `pvs_ppp`, `rtkrcv_pvs` on `g5p3_296u-v1`. That data is
    from Tokyo, outside the service area, so these check the pipeline only.
- **New unit test** `utest_pvs`. `t_toml` now checks `geoid_model` and
  `bds_geo_sats`.

### Upgrade notes

- **SouthPAN PVS:**
  - In real time, log SBF `GEORawL5` (4021).
  - In post-processing, give the `.sbs` file as input
    (`mrtk convert -s` writes the L5 messages).
  - Use `solution_type = "forward"`.
- **`geoid_model`:** configurations that set `egm96`, `egm08_2.5`, `egm08_1`
  or `gsi2000` take effect again. On v0.7.16 they used the internal model.
- **`mrtk run` with `gal-has` / `bds-b2b`:** MADOCA / RTCM3 SSR on the same
  streams is no longer applied.
- **API:**
  - new `sbsmsgtype()` and `sbsupdatepvs()`;
  - new constants `SBS_BAND_L1` / `SBS_BAND_L5`, `SSR_VENDOR_PVS` and
    `CORR_SOUTHPAN_PVS`.

  `sizeof(sbsmsg_t)` is unchanged.

### Test status

Full suite: **220 / 220** (macOS, Accelerate, `-j16`). RT replay tests are
timing-sensitive and can false-fail under CPU contention; re-run them in
isolation.

### Known / open

- PVS DAS input is not validated
  ([#462](https://github.com/h-shiono/MRTKLIB/issues/462)).
- SouthPAN plans to move PVS to a new signal, L5b (1207.14 MHz), with a new
  message format on new GEOs; that will need a new decoder.
- The macOS metadata in the committed test-data archives will be removed by
  moving them to `mrtklib-data`
  ([#358](https://github.com/h-shiono/MRTKLIB/issues/358)).
- `conf/has/run_gal_has.toml` rover path
  ([#456](https://github.com/h-shiono/MRTKLIB/issues/456)); HAS context
  across an rtkrcv restart
  ([#459](https://github.com/h-shiono/MRTKLIB/issues/459)).

### Next

- **v0.7.18:** CLAS remaining items.
- **v0.7.19:** IGS / CNES / Galileo HAS remaining items.
- **v0.7.20:** MADOCA remaining items.
- **v0.8.x:** CLAS CMA authentication.
