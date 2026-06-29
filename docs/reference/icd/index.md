# Reference Specifications (ICDs & Vendor Guides)

This page catalogs the interface-control documents (ICDs) and receiver vendor
guides that MRTKLIB's decoders and positioning engines were implemented against.

!!! warning "The PDFs themselves are **not** in this repository"
    These documents are copyrighted by their respective issuers and are
    **kept local only** — `docs/reference/icd/*.pdf` is gitignored and never
    pushed to the remote. This page (the provenance catalog) **is** tracked.
    To obtain a copy, download the official version from the **Source** column.
    Drop the PDF into `docs/reference/icd/` and it will be available locally
    without being committed.

## Public open ICDs

These are freely published by their issuing authority and can be downloaded by
anyone from the official source.

| File | Document / Issue | Issuer | Source | Used by |
|------|------------------|--------|--------|---------|
| `is-qzss-l6-008.pdf` | IS-QZSS-L6-008 (QZSS L6 signal / CLAS · MADOCA-PPP) | Cabinet Office (QZSS) | <https://qzss.go.jp/en/technical/ps-is-qzss/ps-is-qzss.html> | `src/clas/`, `src/madoca/`, L6 frame decoders |
| `is-qzss-mdc-004.pdf` | IS-QZSS-MDC-004 (QZSS MADOCA-PPP / CLAS) | Cabinet Office (QZSS) | <https://qzss.go.jp/en/technical/ps-is-qzss/ps-is-qzss.html> | `src/madoca/`, `src/clas/`, `src/pos/mrtk_ppp.c` |
| `sli-mdc-ion-draft.pdf` | MADOCA L6D wide-area ionosphere (draft) | Cabinet Office (QZSS) | QZSS technical documents (draft distribution) | `src/madoca/mrtk_madoca_iono.c`, `src/pos/mrtk_ppp_iono.c` |
| `IS-GPS-200N.pdf` | IS-GPS-200N — Navstar GPS Space Segment / Navigation User Interfaces | US SMC / GPS Directorate | <https://www.gps.gov/technical/icwg/> | `src/data/` (ephemeris), RINEX nav |
| `Galileo_HAS_SIS_ICD_v1.0.pdf` | Galileo HAS SIS ICD, Issue 1.0 (May 2022) | EUSPA / European Union | <https://www.gsc-europa.eu/electronic-library/programme-reference-documents> | `src/has/` |
| `BeiDou_PPP_B2b_ICD_v1.0.pdf` | BDS-SIS-ICD-PPP-B2b-1.0 | CSNO (China Satellite Navigation Office) | <http://en.beidou.gov.cn/SYSTEMS/ICD/> | `src/pos/` (PPP-B2b) |

## Vendor firmware reference guides

!!! danger "Restricted distribution"
    Septentrio reference guides are distributed through registered customer
    accounts and are **not** freely redistributable. Do not post them anywhere
    public. Obtain them from your own Septentrio account.

| File | Document / Version | Vendor | Source | Used by |
|------|--------------------|--------|--------|---------|
| `mosaic-CLAS Firmware v4.15.1 Reference Guide.pdf` | mosaic-CLAS Reference Guide, fw v4.15.1 | Septentrio | Septentrio account (login required) | `docs/hardware/`, two-receiver QZSS L6 (v0.7.1) |
| `mosaic-G5 Firmware v1.1.0 Reference Guide.pdf` | mosaic-G5 Reference Guide, fw v1.1.0 | Septentrio | Septentrio account (login required) | `docs/hardware/cssr2rtcm3-mosaic-g5.md` |

## Cited by upstream RTKLIB (link-only, not collected)

Standards and ICDs referenced in the `references :` headers of
`upstream/RTKLIB/src/*` and reused by MALIB / CLASLIB / MADOCALIB. These are
**not** held locally — download from the official issuer when needed.
Near-identical versions are folded into one row (the cited editions are listed).
Vendor receiver protocol specs (u-blox/NovAtel/Septentrio/Javad/… UBX, BINR,
GREIS, OEM) are intentionally excluded — they are not open standards.

🎯 = collection candidate (actually exercised by MRTKLIB positioning, worth
holding a local copy). Documents already held above are cross-referenced.

### GNSS signal ICDs

| Document (cited editions) | Issuer | Source | Cited by |
|---------------------------|--------|--------|----------|
| IS-GPS-200 — Navstar GPS Space Segment / Navigation User Interfaces (200D 2006, 200K 2019; **200N → held, see above**) | US SMC / GPS Directorate | <https://www.gps.gov/technical/icwg/> | `ephemeris.c`, `rinex.c`, `rcvraw.c` |
| IS-QZSS — QZSS Navigation Service IS (v1.1 2009, v1.5 2014, PNT IS-QZSS-PN-003 2018) | Cabinet Office (QZSS) | <https://qzss.go.jp/en/technical/ps-is-qzss/ps-is-qzss.html> | `ephemeris.c`, `rcvraw.c`, `qzslex.c`, `sbas.c` |
| IS-QZSS-MDC — MADOCA-PPP / CLAS (MDC-002 2023; **MDC-004 2025 → held, see above**) 🎯 | Cabinet Office (QZSS) | <https://qzss.go.jp/en/technical/ps-is-qzss/ps-is-qzss.html> | `mdciono.c`, `mdccssr.c`, `ppp.c`, `ppp_ar.c` |
| European GNSS (Galileo) Open Service SIS ICD (Issue 1 2010, 1.2 2015, 1.3 2016) | EUSPA / EU | <https://www.gsc-europa.eu/electronic-library/programme-reference-documents> | `ephemeris.c` |
| BeiDou (BDS) SIS ICD — Open Service Signal B1I (v1.0 2012, v3.0 2019) | CSNO | <http://en.beidou.gov.cn/SYSTEMS/ICD/> | `rtkcmn.c`, `ephemeris.c`, `rcvraw.c` |
| GLONASS ICD — Navigational radiosignal in bands L1, L2 (v5.1 2008) | Russian Space Systems | <https://glonass-iac.ru/en/documents/> | `ephemeris.c`, `rcvraw.c` |
| ISRO-IRNSS-ICD-SPS-1.1 — IRNSS/NavIC SPS (2017) | ISRO | <https://www.isro.gov.in/> | `rcvraw.c` |

### Augmentation / integrity standards

| Document | Issuer | Source | Cited by |
|----------|--------|--------|----------|
| RTCA/DO-229C — MOPS for GPS/WAAS Airborne Equipment (2001) | RTCA | <https://www.rtca.org/> | `ephemeris.c`, `rtkcmn.c`, `sbas.c` |

### RINEX family

| Document (cited editions) | Issuer | Source | Cited by |
|---------------------------|--------|--------|----------|
| RINEX — Receiver Independent Exchange Format (2.11, 2.12, 3.00–3.05, 4.00, 4.01) | IGS RINEX WG / RTCM-SC104 | <https://igs.org/wg/rinex/> | `rinex.c` |
| RINEX clock-information extensions (1998, 3.02 2010, 3.04 2017) | IGS / J.Ray, W.Gurtner | <https://files.igs.org/pub/data/format/> | `rinex.c` |

### RTCM standards & SSR

| Document (cited editions) | Issuer | Source | Cited by |
|---------------------------|--------|--------|----------|
| RTCM 10403.x — Differential GNSS Services v3 (10403.1 2006 + Amdt 3/5, 10403.2 2013, 10403.3 2020) 🎯 | RTCM SC-104 | <https://www.rtcm.org/publications> | `rtcm.c`, `ephemeris.c` |
| RTCM Recommended Standards for DGNSS v2.3 (2001) | RTCM SC-104 | <https://www.rtcm.org/publications> | `rtcm.c` |
| RTCM Ntrip (Networked Transport via Internet Protocol) v1.0 (2004) | RTCM SC-104 | <https://www.rtcm.org/publications> | `stream.c` |
| RTCM SC-104 draft/proposal papers — SSR & MSM & ephemeris (012-2009-528/-582, 059-2011-635, 019-2012-689, 163-2012-725, 034-2012-693, 133-2012-709, 122-2012-707.r1, 107-2014-818; "Proposed SSR Messages" 2010; ssr_1_gal_qzss_sbas_dbs_v05 2014) | RTCM SC-104 | <https://www.rtcm.org/> | `rtcm.c`, `ephemeris.c` |

### IGS / GNSS exchange formats

| Document | Issuer | Source | Cited by |
|----------|--------|--------|----------|
| SP3 — Extended Standard Product 3 Orbit Format (SP3-c 2007, SP3-d 2016) 🎯 | IGS / S.Hilla | <https://files.igs.org/pub/data/format/sp3d.pdf> | `preceph.c` |
| ANTEX — The Antenna Exchange Format v1.4 (2010) 🎯 | IGS / Rothacher, Schmid | <https://files.igs.org/pub/data/format/antex14.txt> | `rtkcmn.c` |
| IONEX — The IONosphere Map Exchange Format v1 (1998) | IGS / Schaer, Gurtner, Feltens | <https://files.igs.org/pub/data/format/ionex1.pdf> | `ionex.c` |
| SINEX-Bias — Solution INdependent EXchange for GNSS Biases v1.00 (2018) 🎯 | IGS / S.Schaer | <https://files.igs.org/pub/data/format/sinex_bias_100.pdf> | `biassnx.c` |
| IGS State Space Representation (SSR) Format v1.00 (2020) | IGS | <https://files.igs.org/pub/data/format/igs_ssr_v1.pdf> | `rtcm.c` |

### Timekeeping / reference frames

| Document | Issuer | Source | Cited by |
|----------|--------|--------|----------|
| IERS Conventions — IERS Technical Notes 21 (1996), 32 (2003), **36 (2010)** 🎯 | IERS / McCarthy, Petit, Luzum | <https://www.iers.org/IERS/EN/Publications/TechnicalNotes/> | `ppp.c`, `mrtk_tides.c`, `cssr2osr.c` |

### Output / peripheral I/O formats

| Document | Issuer | Source | Cited by |
|----------|--------|--------|----------|
| NMEA 0183 v4.10 (2012) + Talker Identifier Mnemonics (2019) | NMEA | <https://www.nmea.org/> | `solution.c` |
| OGC KML 2.2 (07-147r2, 2008) | Open Geospatial Consortium | <https://www.ogc.org/standard/kml/> | `convkml.c` |
| GPX — The GPS Exchange Format | TopoGrafix | <https://www.topografix.com/gpx.asp> | `convgpx.c` |
| ESRI Shapefile Technical Description (1998) | ESRI | <https://www.esri.com/> | `gis.c` |
| BINEX — Binary Exchange Format | UNAVCO | <https://binex.unavco.org/> | `binex.c` |

## Adding a new specification

1. Download the official PDF and place it in `docs/reference/icd/`.
2. Add a row to the appropriate table above: filename, full title + issue/version,
   issuer, official source URL, and which MRTKLIB modules cite it.
3. Commit **only** `index.md` — the PDF is gitignored automatically.
