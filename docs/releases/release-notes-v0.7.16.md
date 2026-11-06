# Release Notes — v0.7.16

## BeiDou PPP-B2b

**Release date:** 2026-11-06
**Type:** Feature release (new correction service), plus two contributed fixes
(NavIC SPP, NTRIP v2 source).
**ABI / API changes** in `prcopt_t`, `raw_t` and `rtksvr_t`; new public API
in `mrtk_b2b.h`.
**Branch:** `release/v0.7.16`

---

### Overview

v0.7.16 adds the second global-style SSR service of the v0.7.x series after
Galileo HAS: **BeiDou PPP-B2b**, broadcast by the four BDS-3 GEO satellites on
B2b-I and free of charge. It covers China and the surrounding region, and all
four GEOs are visible from Japan. `correction = "bds-b2b"` runs BeiDou-only
float PPP in post-processing (`mrtk post`) and in real time (`mrtk run`).

| | |
|:--|:--|
| Corrections | orbit, clock, code bias for BDS-3 (and orbit / clock for GPS) |
| Mode | float PPP (no phase biases broadcast) |
| Constellation | BeiDou only (no GPS code biases broadcast) |
| Input | SBF `BDSRawB2b` (4242) + `BDSCNav1` (4251) + observations |
| Issue | [#417](https://github.com/h-shiono/MRTKLIB/issues/417) |

Two fixes by an external contributor are included: the single-frequency NavIC
L5 group delay in SPP ([#384](https://github.com/h-shiono/MRTKLIB/pull/384))
and the NTRIP v2 source handshake
([#385](https://github.com/h-shiono/MRTKLIB/pull/385)).

---

### BeiDou PPP-B2b ([#417](https://github.com/h-shiono/MRTKLIB/issues/417))

**What the live broadcast looks like.** It was checked on the kubolab
mosaic-G5 capture of 2026-10-23 to 10-28 (TOPCON CR-G5, Tokyo):

- The GEOs are **C01–C04** since the April 2026 PRN reassignment (C59–C62
  before). Their static az/el match the four GEO slots and the CSNO table of
  2026-03-20.
- Message types 1/2/3/4/63; no MT5–7 seen. The mask (IODP 14) lists 30 BDS and
  32 GPS satellites. Orbit corrections cover 12 BDS-3 and 15 GPS satellites in
  an hour. Code biases (8 BDS signal modes) are broadcast for BeiDou only.
- The GEOs send **two streams**: C01–C03 IOD SSR 1, C04 IOD SSR 2, with the
  same mask. The 2025 capture shows the same split (C59 / C60 vs C62).
- IGSO / MEO satellites also send B2b-I frames (B-CNAV3 navigation). They pass
  the same CRC and reuse message types 1/4/5.

**How MRTKLIB handles it.**

- **GEO list:** `[positioning] bds_geo_sats`, default `["C01-C04"]`; use
  `["C59-C62"]` for data before 2026-04-17. Only frames from these PRNs are
  decoded as PPP-B2b. No header bit separates GEO and non-GEO frames, so the
  list cannot be derived from the data. It sits next to `bds2_sats` in the
  [BeiDou PRN Reassignment guide](../guide/beidou-prn-reassignment.md).
- **One stream at a time:** the decoder takes the first GEO that sends a mask
  and keeps it until that GEO is silent for 10 s. An orbit of one stream is
  therefore never paired with a clock of the other. An IOD SSR change clears
  every staged correction.
- **Ephemeris:** the orbit IODN refers to the **B-CNAV1** ephemeris (IODC).
  - Post-processing needs a RINEX 4 broadcast file with CNV1 records (e.g.
    `BRD400DLR_S`).
  - In real time, SBF `BDSCNav1` (4251) is decoded into ephemeris set 1 as type
    CNAV-1. The layout is from the mosaic-G5 Firmware v1.1.0 Reference Guide,
    pp. 245–246. It was checked field by field against the BRD400DLR CNV1
    records.
  - The D1/D2 ephemeris (`BDSNav`, 4081) in set 0 is untouched. 4251 is decoded
    only while `correction = "bds-b2b"`.
- **Clock age limit:** 30 s for PPP-B2b. The ICD's 12 s nominal validity is not
  used, because live clocks arrive 6–8 s after their epoch and are renewed
  every 6 s. A 12 s limit turned the 2 h kinematic 2D rms from 0.16 to
  0.34 m. Other sources keep 60 s.
- **Tools:** `mrtk l6extract` writes the GEO frames to `<input>.b2b` (options
  `-no-b2b`, `-b2b-geo LIST`); `mrtk post` reads `.b2b` input.
- **Samples:** `conf/b2b/ppp_bds_b2b.toml`, `conf/b2b/run_bds_b2b.toml`;
  guide: [Quick Start: BeiDou PPP-B2b](../guide/quickstart-bds-b2b.md).

**Accuracy** (BeiDou-only float PPP, 2026-10-23 20:00–22:00 GPST, against the
PPP-AR reference coordinate of the antenna, after the first 60 min):

| | 2D rms | U rms | 3D 95 % |
|:--|:-:|:-:|:-:|
| `mrtk post`, static | 1.8 cm | 3.9 cm | 6.4 cm |
| `mrtk post`, kinematic | 15.8 cm | 10.8 cm | 30.9 cm |
| `mrtk run` replay, kinematic | 14.8 cm | 10.7 cm | 27.9 cm |

About 9 BeiDou satellites per epoch. This is one 2-hour session, not a service
performance figure. The other stream (C04 alone) gives static
3.6 / 7.0 / 10.2 cm and kinematic 11.8 / 9.6 / 20.9 cm, so neither stream is
better on both counts.

**Shared code.**

- `eph2pos()` now applies the CNAV `Adot` / `ndot` terms. They were ignored, so
  CNAV-type ephemerides were 1–63 m off. D1/D2 and LNAV ephemerides have both
  terms at zero and are unchanged.
- `satpos_ssr()` accepts an orbit correction up to its `udi[0]` when that
  exceeds 60 s, capped at 120 s. PPP-B2b orbits are valid for 96 s and renewed
  about every 57 s. Sources with `udi[0]` ≤ 60 s keep the 60 s limit.

---

### NavIC L5-only group delay ([#384](https://github.com/h-shiono/MRTKLIB/pull/384))

Contributed by @badaboom. In SPP, `prange()` computed γ for the
single-frequency NavIC TGD correction from the two observation codes.

- **No S-band code:** the second frequency was zero, so the correction was
  non-finite. This gave an infinite residual, then NaN, then an LSQ failure,
  once a NavIC ephemeris was available.
- **S-band code present:** γ was the inverse of the ICD value, so the
  correction was about 20× too small.

γ is now the nominal (f_S / f_L5)² of ISRO SPS ICD 1.1 §6.2.1.5 for
single-frequency NavIC. The iono-free branch and other systems are unchanged.

### NTRIP v2 source handshake ([#385](https://github.com/h-shiono/MRTKLIB/pull/385))

Contributed by @badaboom.

**What went wrong.** A caster answering the NTRIP v2 `POST` with
`HTTP/1.1 200 OK` matched the legacy `OK\r\n` first. The stream was taken as
v1, and the server stream sent a raw body although the request had advertised
`Transfer-Encoding: chunked`.

**What changed.**

- HTTP responses now stay in the HTTP parser until the headers are complete.
- Chunked output is used only after a v2 request answered with HTTP 200.
- A v1 `SOURCE` answered with an HTTP status line stays raw.
- Bare `OK` / `ICY 200 OK` stay v1, as before.

The review also made `mrtklib` link the thread library (`Threads::Threads`,
PUBLIC). Before, utests that pull in the stream code through the static
library did not link with glibc < 2.34.

---

### Tests

- **New dataset `g5p3_296u-v1`** (9.2 MB, `h-shiono/mrtklib-data`): one hour
  of the kubolab capture (2026-10-23 19:59:30–21:00 GPST) with GEO
  `BDSRawB2b`, `BDSCNav1`, SouthPAN `GEORawL5` PRN 122 (for the PVS work of
  v0.7.17), and a RINEX 4 navigation file cut from BRD400DLR. Built
  reproducibly by `scripts/tests/make_g5p3_296u_fixture.py`.
- **New regression tests:** `b2b_extract`, `b2b_convert`, `b2b_ppp`,
  `b2b_ppp_check`, `rtkrcv_b2b`, `rtkrcv_b2b_check`.
  - The replay runs at ×20 and takes about 190 s.
  - The gates (0.30 m horizontal / 0.60 m 3D on the last 20 min) catch
    regressions; they are not the service accuracy.
  - A performance baseline across releases is planned in
    [#457](https://github.com/h-shiono/MRTKLIB/issues/457).
- **New unit tests:**
  - `utest_b2b`: decoder, GEO list, a real `BDSCNav1` block, IOD SSR change,
    withdrawn bias, stream lock.
  - `utest_navic_l5_tgd` (#384).
  - `utest_ntrip_server_response` (#385).

### Upgrade notes

- **BeiDou PPP-B2b:**
  - Set `bds_geo_sats` for data before 2026-04-17 (`["C59-C62"]`).
  - In real time, log SBF 4242 and 4251.
  - Post-processing needs a RINEX 4 navigation file with BeiDou CNV1 records.
- **External drivers must rebuild** (ABI change):
  - `prcopt_t` gains `bdsgeo[256]` (appended).
  - `raw_t` gains `b2b_msg[61]`, `b2b_prn`, `b2b_time`.
  - `rtksvr_t` gains `b2b`.
- **New API:**
  - In `mrtk_b2b.h`: `b2b_new()`, `b2b_free()`, `b2b_input_msg()`,
    `b2b_input_file()`, `b2b_sel_biascode()`.
  - `set_bdsgeo()`, `is_bdsgeo()`, `SSR_VENDOR_B2B`.
  - `input_raw()` return code 14.
- **Linking:** `mrtklib` now carries the thread library in its link
  interface.

### Test status

Full suite: **206 / 206** (macOS, Accelerate, `-j8`), on develop with #458,
#384 and #385 merged. RT replay tests are timing-sensitive and can false-fail
under CPU contention; re-run them in isolation.

### Known / open

- GPS is corrected (orbit / clock) but cannot be used without GPS code
  biases; a bias-free use is an open question in `docs/design/bds-b2b.md`.
- MT5–7 have not been seen on the live broadcast; MT6 `Slot_S` indexing is
  unverified.
- `conf/has/run_gal_has.toml` uses a `file://` rover path that `mrtk run`
  cannot open ([#456](https://github.com/h-shiono/MRTKLIB/issues/456)). The
  HAS context survives an rtkrcv restart
  ([#459](https://github.com/h-shiono/MRTKLIB/issues/459)).

### Next

- **v0.7.17:** SouthPAN PVS ([#418](https://github.com/h-shiono/MRTKLIB/issues/418)).
- **v0.7.18:** CLAS remaining items.
- **v0.8.x:** CLAS CMA authentication.
