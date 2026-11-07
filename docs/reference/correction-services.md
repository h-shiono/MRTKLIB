# Correction Service Capability Matrix

This page lists, for each correction service MRTKLIB supports, what runs in
post-processing (`mrtk post`) and in real time (`mrtk run`), which positioning
modes and constellations are available, which inputs are accepted, the release
that added it and the regression tests that cover it.

The service is selected with the `correction` configuration key; see the
[configuration guide](../guide/configuration.md) and the per-service quick
starts.

!!! note "Keeping this page current"
    Each release that adds or changes a service updates this page and the
    services table in the README.

## Overview

| Service | `correction` | Coverage | Modes | Constellations | Post | Real-time |
|---------|--------------|----------|-------|----------------|:-:|:-:|
| **QZSS CLAS** | `qzs-clas` | Japan | PPP-RTK; VRS-RTK | GPS, Galileo, QZSS | ✅ 1ch / 2ch | ✅ 1ch / 2ch |
| **QZSS MADOCA-PPP** | `qzs-madoca` | Asia–Oceania; ionosphere for Japan and nearby areas | PPP, PPP-AR, PPP-AR + ionosphere | GPS, GLONASS, Galileo, QZSS, BeiDou (GLONASS float only) | ✅ | ✅ |
| **Galileo HAS** | `gal-has` | Global | Float PPP | GPS, Galileo | ✅ | ✅ |
| **BeiDou PPP-B2b** | `bds-b2b` | China and the Asia-Pacific region | Float PPP | BeiDou-3 | ✅ | ✅ |
| **SouthPAN PVS** | `southpan-pvs` | Australia and New Zealand | Float PPP | GPS (L1 C/A + L5), Galileo (E1 + E5a) | ✅ | ✅ |
| **IGS products** | `igs` | Global | Float PPP, integer PPP-AR (Bias-SINEX OSB phase biases) | GPS, Galileo validated (PPP-AR: GPS, GPS + Galileo); others as in the products¹ | ✅ | — |
| **IGS-RTS** | `igs-rts` | Global | Float PPP | GPS, Galileo validated; others as in the stream¹ | ✅ (recorded stream) | ✅ |

For the five satellite-broadcast services, the constellations are those the
service sends corrections for. ¹ For IGS products and IGS-RTS they depend on
the product or stream; the regression tests and the quick starts cover GPS and
Galileo. Other constellations can be added to `systems` (for BeiDou, also set
`[positioning] bds2_sats`; see
[BeiDou PRN reassignment](../guide/beidou-prn-reassignment.md)).

The five satellite-broadcast services (CLAS, MADOCA-PPP, HAS, PPP-B2b, PVS)
run in real time directly from the correction messages the receiver outputs.
IGS products are files and have no real-time mode. IGS-RTS is delivered over
the internet (NTRIP).

## Release history

| Service | Post-processing | Real-time |
|---------|-----------------|-----------|
| **QZSS CLAS** | v0.3.0 (CLASLIB integration: PPP-RTK, VRS-RTK) | v0.4.3 (1ch), v0.4.4 (2ch), v0.6.5 (`mrtk cssr2rtcm3` VRS stream), v0.7.7 (two-channel merge) |
| **QZSS MADOCA-PPP** | v0.2.0 (MADOCALIB integration: PPP, PPP-AR, PPP-AR + ionosphere) | v0.2.0 (PPP, PPP-AR), v0.6.12 (multi-GNSS signal selection), v0.7.3 (PPP-AR + ionosphere, L6D PRN 200/201) |
| **Galileo HAS** | v0.7.0 | v0.7.0 |
| **BeiDou PPP-B2b** | v0.7.16 | v0.7.16 |
| **SouthPAN PVS** | v0.7.17 | v0.7.17 |
| **IGS products** | v0.6.7 (float), v0.6.9 (PPP-AR) | — |
| **IGS-RTS** | v0.6.8 | v0.6.8 (NTRIP) |

## Inputs

| Service | `mrtk post` corrections | `mrtk run` receiver input |
|---------|-------------------------|---------------------------|
| **QZSS CLAS** | L6 file (`.l6`), one per channel | u-blox UBX `RXM-QZSSL6`; Septentrio SBF `QZSRawL6` (4069) / `QZSRawL6D` (4270); raw L6 stream (`clas`); rover observations in any supported format (e.g. BINEX, RTCM3, SBF, UBX) |
| **QZSS MADOCA-PPP** | L6E file (`.l6` / `.l6e`, several files allowed); L6D ionosphere (`.l6d`) | UBX `RXM-QZSSL6`; SBF `QZSRawL6` (4069) / `QZSRawL6E` (4271), L6D ionosphere from the same UBX / SBF stream; raw L6E stream (`l6e`) |
| **Galileo HAS** | `.has` pages from the SBF capture (`mrtk l6extract`) | SBF `GALRawCNAV` (4024) |
| **BeiDou PPP-B2b** | `.b2b` messages from the SBF capture (`mrtk l6extract`) | SBF `BDSRawB2b` (4242) + `BDSCNav1` (4251) |
| **SouthPAN PVS** | `.sbs` PRN 122 L5 messages, from SBF (`mrtk convert -s`) or a DAS log (`scripts/tests/das2sbs.py`) | SBF `GEORawL5` (4021), with GPS LNAV and Galileo F/NAV ephemeris |
| **IGS products** | SP3, clock, Bias-SINEX | — |
| **IGS-RTS** | recorded RTCM3 SSR stream | RTCM3 SSR over NTRIP |

In real time, HAS, PPP-B2b and PVS each take a single SBF stream with the
observations, ephemeris and corrections. QZSS L6 frames are routed by their L6
header, not by PRN or input container, so a stream that carries several L6
services (CLAS, MADOCA-PPP, MADOCA-PPP ionosphere) feeds each one to its own
decoder.

## Regression tests

All test names below are CTest names. Every service has a test with an
absolute check against a reference coordinate that does not come from the
service itself (IGS SINEX, GSI F5 daily coordinates, a PPP-AR solution, or a
RINEX header position checked against SINEX); CLAS and MADOCA-PPP also check
parity with the upstream CLASLIB / MADOCALIB outputs. See the
[test methodology](test-accuracy-methodology.md).

| Service | Post-processing | Real-time (replay) |
|---------|-----------------|--------------------|
| **QZSS CLAS** | `claslib_ppp_rtk*` (1ch, 2ch, merge, ST12, L1C/L5, single-frequency, F9P, BINEX), `claslib_vrs_rtk*`, `claslib_ssr2obs*`, `claslib_ssr2osr*`, `post_handover_*` | `rtkrcv_rt_clas`, `rtkrcv_rt_clas_2ch`, `rtkrcv_rt_clas_2ch_merge`, `rtkrcv_rt_clas_sf1gap`, `rtkrcv_handover_*`, `rtkrcv_clas_l6gap_*`, `cssr2rtcm3_clas_l6gap*` |
| **QZSS MADOCA-PPP** | `madocalib_ppp`, `madocalib_pppar`, `madocalib_pppar_003`, `madocalib_pppar_ion`, `madocalib_mdc005_*` | `rtkrcv_rt` (PPP-AR) |
| **Galileo HAS** | `has_ppp` | — |
| **BeiDou PPP-B2b** | `b2b_ppp` | `rtkrcv_b2b` |
| **SouthPAN PVS** | `pvs_ppp`, `pvs_alby_static`, `pvs_alby_kin`, `pvs_alic_kin` | `rtkrcv_pvs` |
| **IGS products** | `igs_ppp`, `igs_iflc_ppp`, `igs_pppar_gps`, `igs_pppar_ge`, `igs_pppar_gps_recvbias*` | — |
| **IGS-RTS** | `igsrts_ppp` | — |

Real-time paths with no replay test: Galileo HAS, MADOCA-PPP PPP-AR +
ionosphere and IGS-RTS over NTRIP. They were validated on live or recorded
data when they were added (see the release notes).

## Notes

- **MADOCA-PPP ionospheric augmentation.** The L6D wide-area ionospheric
  corrections (QZS PRN 200/201) are used in post-processing since v0.2.0 and in
  real time since v0.7.3, with `positioning.madoca.iono_correction = true`.
- **IGS-RTS.** `mrtk run` takes the RTCM3 SSR stream over NTRIP (v1 or v2);
  see [NTRIP streams](../guide/ntrip.md) and the
  [IGS-RTS quick start](../guide/quickstart-igs-rts.md).
- **BeiDou PPP-B2b.** The broadcast has GPS orbit and clock corrections but no
  GPS code biases, so MRTKLIB runs BeiDou only. The GEO PRNs changed in the
  April 2026 BeiDou PRN reassignment; see
  [BeiDou PRN reassignment](../guide/beidou-prn-reassignment.md).
- **SouthPAN PVS.** The SBAS L5 messages of PRN 122 in the current (IOC) format.
- **Real-time channels.** CLAS takes two L6 channels (`l6_merge` merges their
  transmit patterns); MADOCA-PPP L6E takes one correction stream, which may
  carry several QZSS L6E channels multiplexed by the receiver.

## Quick starts

- [CLAS PPP-RTK](../guide/quickstart-clas.md)
- [MADOCA-PPP](../guide/quickstart-madoca-ppp.md)
- [Galileo HAS PPP](../guide/quickstart-gal-has.md)
- [BeiDou PPP-B2b](../guide/quickstart-bds-b2b.md)
- [SouthPAN PVS](../guide/quickstart-southpan-pvs.md)
- [IGS-RTS PPP](../guide/quickstart-igs-rts.md)
