# SouthPAN PVS (PPP via SouthPAN)

Status: implemented (#418): `correction = "southpan-pvs"`, post-processing and
real time (SBF GEORawL5). §1–§4 are the survey; §5 is the implementation and §7 the
validation.
References (local only, see [`reference/icd/index.md`](../reference/icd/index.md)):

- SBAS-STN-0001 Rev 04 (9 Apr 2026) — SDD for Signal-In-Space Open Services ("SDD-SIS")
- SBAS-STN-0002 Rev 02 (9 Apr 2026) — SDD for Data Access Services ("SDD-DAS")
- Reference implementation: cssrlib `cssr_pvs.py`, `sbas.py` (MIT) and samples in cssrlib-data (MIT)

## 1. What PVS is

PVS is **not a separate message format**. During IOC it rides inside the DFMC
SBAS L5 stream (ED-259) of GEO **PRN 122** (143.5°E):

- Carrier: MT32 (satellite clock-ephemeris correction and covariance), the same
  message DFMC SBAS uses. SDD-SIS §5.3.1.
- PVS flag: the parity of the time of applicability, `tD/16` (`tD` is 13 bits × 16 s).
  Odd means "for both SBAS and PVS"; even means "for SBAS only". PVS users take only the odd ones.
- Content: ECEF δx/δy/δz, δB (clock), their rates, IODN, tD. **No code bias, phase
  bias, ionosphere or troposphere** → float PPP on the ionosphere-free combination only.
- Augmented signals: GPS L1 C/A + L5 (LNAV ephemeris), Galileo E1 + E5a
  (**F/NAV** ephemeris). SDD-SIS §8.1.1.
- Frame: ITRF2014, current epoch (§8.1.3); not WGS84 like the SBAS services.
- Validity: 100 s **from reception of the MT32**, not from `tD` (§5.3.2).
- MT31 (mask) is not needed: the MT32 slot number identifies the satellite (§5.3.1).
- IOC targets (§8.1.4): HPE 95 % ≤ 0.375 m, VPE 95 % ≤ 0.525 m, convergence ≤ 80 min,
  availability ≥ 0.95. FOC (late 2028): 0.15 / 0.225 m, ≤ 40 min.
  **No observed PVS performance is published yet** (§8.1.6).
- Future: PVS moves to a new signal L5b (1207.14 MHz) **with a new data format** on
  new GEOs (PRN and date to be announced). The MT32-based decoder is therefore an
  IOC-era implementation.

### 1.1 MT32 layout

ED-259 itself (EUROCAE, paid) is not held. The field list and resolutions come from
SDD Table 7. The bit order and the covariance block size come from cssrlib `sbas.py`.

| bits | field | scale |
|------|-------|-------|
| 4 | preamble (L5: 4-bit sequence 0x5,0xC,0x6,0x9,0xA,0x3) | |
| 6 | message type (= 32) | |
| 8 | satellite slot | GPS 1–32, GLO 38–69, GAL 75–110, SBAS 120–158, BDS 159–195 |
| 10 | IODN | |
| 11 ×3 | δx δy δz (ECEF) | 0.0625 m |
| 12 | δB | 0.03125 m |
| 8 ×3 | δẋ δẏ δż | 2⁻¹¹ m/s |
| 9 | δḂ | 2⁻¹² m/s |
| 13 | tD | 16 s (time of day) |
| 99 | covariance (scale exponent + Cholesky factor) | unused for float PPP |
| 4 + 4 | DFREI, ΔRcorr | unused for float PPP |
| 24 | CRC-24Q | |

The total is 4 + 6 + 216 + 24 = 250 bits. When slot = 0, the message carries no correction.

Measured on the samples (§3.3): ~94 % of MT32 messages are PVS-flagged. Each
satellite is refreshed about every 65 s (median; max 73 s), inside the 100 s
validity. The rate fields are zero in 96 % of the messages, and the non-zero ones are ≤ 1 LSB.

### 1.2 Application

The equations are in ED-259 §A.3.9.2, which we do not hold. MRTKLIB follows cssrlib
and settled its open points on data:

- `rs += (δx, δy, δz) + rate·dt` in ECEF, the same sign as RTKLIB `sbslongcorr()`.
- `dts += (δB + δḂ·dt) / c`.
- GPS: `dts -= TGD`, because the DFMC clock refers to the L1/L5 iono-free clock.
  Confirmed on ALBY 2025-233 h: without it the GPS-only kinematic solution has a
  5.7 m vertical bias, and GPS + Galileo over the last 20 min goes from 0.06 to
  0.17 m 2D rms. Galileo F/NAV clocks already refer to E1/E5a (no BGD).
- Satellite antenna: the corrected orbit is the APC of the iono-free combination
  (`satellite_ephemeris = "brdc+ssrapc"`).
- **Rate reference time:** the reception time (`t0` of the stored correction). The
  rates are ≤ 1 LSB and zero in 96 % of the messages, so this cannot be told apart
  from `tD` on the samples. MRTKLIB does not accumulate the rate term (cssrlib's
  `dorb += dvel·dt` updates the stored array in place).
- **IODN:** for GPS it is the LNAV **IODC** (live 2026-10-23: G04 IODN 795 = IODC
  795, IODE 27 = 795 & 0xFF); MRTKLIB matches its 8 LSBs against the IODE. For
  Galileo it is the IODnav of the **F/NAV** ephemeris.
- Other DFMC GEOs send MT32 too: the Kamakura log has BDSBAS PRN 130/143/144 MT32
  with other values in the same seconds. Only PRN 122 is used.

## 2. Delivery channels

| Channel | How | Notes |
|---------|-----|-------|
| SIS | PRN 122 L5 navigation data | mosaic-G5: track `GEOL5` and log SBF **GEORawL5 (4021)** (250 bits after Viterbi). u-blox F9P has no SBAS L5. |
| DAS (SISNeT) | TCP `sisnet.data.gnss.ga.gov.au`, port **61005** / 62005 (L5 DFMC + PVS); 61001 / 62001 = L1 | Account by e-mail to `gnss@ga.gov.au` with the preferred user name (SDD-DAS §6.1). |
| OS-GOBS | AUSCORS NTRIP `ntrip.data.gnss.ga.gov.au` (443 TLS / 2101) | Registration at <https://gnss.ga.gov.au/stream>. GRE mountpoints `SSSS00CCC0`: 1077/1097/1107 MSM7 (L1C/L5Q, E1C/E5aQ) + 1019/1045/1046. This is the observation source for in-area validation. |

### 2.1 DS2DC / SISNeT protocol (SDD-DAS §8–9, App. A)

- Text lines terminated by LF or CRLF. After connecting, the client must send
  `AUTH,<user>,<password>` within 10 s. The server answers `*AUTH`; then `START` → `*START`.
  `STOP` ends the stream. Errors come back as `*ERR`.
- Data lines: `*MSG,<GPS week>,<GPS TOW>,<SINCA hex>*<checksum>`.
- SINCA: `X|n` = digit X repeated n times (1 hex digit, 4–15); `X/nn` = X repeated nn
  times (2 hex digits, >15). The decompressed payload is 64 hex digits (32 bytes = 250 bits + pad).
- The `*<checksum>` algorithm is **not specified** in the SDD; it is defined in ESA
  E-RD-SYS-E31-010, which we do not hold. cssrlib ignores it.
- The L5 preamble is 4 bits, so the MT sits at bits 4–9, not 8–13 as on L1.

Data licence: CC BY 4.0 (SDD "Licence"). Captures may be redistributed as
`mrtklib-data` assets with attribution.

## 3. Data availability

### 3.1 Reception in Japan

PRN 122 is at about 48° elevation from Tokyo. Reception in Japan is demonstrated:
cssrlib-data contains PRN 122 L5 messages logged by a mosaic-X5 in Kamakura
(2025-046, 2025-178, 2025-233). The broadcast is the same everywhere, so these
captures can drive post-processing of Australian observations.

Later mosaic-G5 captures log `GEORawL5` (4021): the kubolab session of 2026-10-23
(test dataset `g5p3_296u-v1`) has 2599 PRN 122 MT32 in one hour. Older captures
such as `G5P3073h.sbf` have PRN 122 L1 only.

From Japan only the satellites monitored by SouthPAN (AU/NZ ground network) carry
corrections, so positioning in Japan is out of the service area and not an accuracy reference.

### 3.2 Archives

The SDD describes no historical message archive. Long datasets therefore have to be
recorded through DAS. Observations from AU/NZ IGS stations (ALIC, HOB2, CEDU,
TOW2, MOBS, AUCK, WGTN, …) are on the IGS data centres. Check per station that
C5Q/L5Q and E5a are present. Take the truth from IGS SINEX, epoch-propagated.

### 3.3 Ready-made samples (cssrlib-data, MIT)

| Set | Corrections | Observations | Use |
|-----|-------------|--------------|-----|
| doy2025-233 h | `233h_sbas.txt` (SIS, Kamakura) | `alby233h_rnx.obs` (ALBY, PolaRx5) + `233h_rnx.obs` (Kamakura, mosaic-X5) | primary in-area PP case |
| doy2025-110 f | `DAS2025110f.txt` (DAS, 3660 `*MSG` lines) | `ALIC00AUS0110f.obs` | DAS-path PP case |
| doy2025-046 r, 2025-178 a | `*_sbas.txt` (SIS, Kamakura) | Kamakura obs | out-of-area behaviour |

Format of the SIS `*_sbas.txt` files: `week tow prn mt : <58 hex = 29 bytes>`. This is the
RTKLIB `sbsoutmsg()` form that `readmsgs()` already parses. L1 and L5 frames sit in the
same file and can only be told apart by the MT column (> 30 → L5; MT 0/62/63 are ambiguous).
The 2023 files use an older column layout.

**Limitation:** each set is one hour, and the IOC convergence target is 80 min, so
converged accuracy cannot be judged from these alone. Acceptance against the IOC
targets needs a self-recorded DAS log of ≥ 6 h plus daily RINEX from IGS stations.

## 4. Fit with the existing code

- `src/data/mrtk_sbas.c` is L1-only: it reads the type at bits 8–13, and `sbsdecodemsg()`
  uses the L1 frame. Nothing in the tree handles DFMC (MT31–47), SISNeT or SINCA.
- `sbslcorr_t` (`dpos`/`dvel` ECEF, `daf0`/`daf1`, `iode`, `t0`) matches MT32
  almost one-to-one, but:
  - `sbssat_t` is indexed through the L1 MT1 mask (`nsat`, `iodp`); MT32 is slot-addressed.
  - `sbssatcorr()` returns 0 without a fast correction (MT2–5 are L1-only), so
    `EPHOPT_SBAS` cannot be reused as-is.
  - L1 MT25 and L5 MT32 written into the same `lcorr` would overwrite each other.
  - Its 1800 s age limit is the wrong limit for PVS, which needs 100 s from reception.
- `sbsmsg_t` (`msg[29]` = 226 bits) can hold an L5 frame but has no band flag. Adding one is
  an ABI change (`rtksvr_t.sbsmsg[]`, `raw_t.sbsmsg`).
- Reusable as-is: the SBF/raw → `sbsmsg_t` (ret = 3) plumbing, the `rtksvr` SBAS message buffer,
  the `.sbs` reader, the `[positioning].signals` selection (L1C/L5Q, E1C/E5aQ),
  and `strsendcmd`/`cmds[]` for stream start-up commands.

## 5. Implementation

The PVS correction is a slot-addressed broadcast-ephemeris correction, so the decoder
writes it straight into the existing per-satellite SSR store (`nav->ssr_ch[0]`,
`ssr_t`) with its own vendor `SSR_VENDOR_PVS`, rather than into a separate PVS
store as the survey proposed: `satpos_ssr()` already has the age gate, the rate
terms, the IODE-matched broadcast ephemeris and the APC handling, and the real-time
path then needs no new `raw_t` return code.

| Part | Where | What |
|------|-------|------|
| Input | `sbsmsg_t.band` (`SBS_BAND_L1` / `SBS_BAND_L5`, in the padding byte: `sizeof` stays 40) | SBF GEORawL5 (4021) → `sbsmsg_t` with band L5 (`decode_georaw()`), ret = 3 as L1; `sbsmsgtype()` reads the type at bit 4 (L5) or 8 (L1); `sbsoutmsg()` writes the L5 type in the type column; the `.sbs` reader sets band L5 when the type column matches the L5 field and not the L1 field (also reads the cssrlib `*_sbas.txt` form) |
| DAS | `scripts/tests/das2sbs.py` | SISNeT `*MSG` lines → `.sbs` (SINCA expansion, CRC-24Q check); not validated (#462); the C DAS reader / stream is left for later |
| Decode | `sbsupdatepvs()` (`mrtk_sbas.c`) | PRN 122 MT32, odd tD/16, GPS slots 1–32 and Galileo 75–110 → `ssr_t`: ECEF `deph`/`ddeph`, `dclk[0..1]`, `iode` (GPS IODC & 0xFF), `t0[0..1]` = reception time, `udi` = 0 |
| L1 isolation | `sbsupdatecorr()`; postpos SBAS loops | L5 frames never reach the L1 decoders; postpos feeds L5 frames from their reception time through a separate index, so the L1 frames of the same second do not delay them |
| Engine | `CORR_SOUTHPAN_PVS` (7); `satpos_ssr()`; `corr_meas()` | `brdc+ssrapc` required, AR rejected; no RAC rotation, 100 s from reception, GPS TGD removed; no code/phase bias (float IF PPP on L1 C/A + L5, E1 + E5a); `setseleph(SYS_GAL, 1)` (F/NAV) in postpos and rtksvr |
| Real time | `rtksvr` `update_sbs()` | SBAS messages of band L5 → `sbsupdatepvs()` when `correction = "southpan-pvs"` |
| Confs | `conf/pvs/ppp_southpan_pvs.toml`, `conf/pvs/run_southpan_pvs.toml` | `[signals] gps = "L1/L5"`, `galileo = "E1/E5a"`, `[input.sbas] satellite = 122` |

Two shared-code bugs were found and fixed on the way, both of which dropped the
Galileo F/NAV ephemeris PVS needs:

- SBF GALNav (4002) stored F/NAV in ephemeris set 0, over the I/NAV ephemeris
  (GALRawFNAV already used set 1). `mrtk convert` of the 296u capture wrote 1 F/NAV
  record instead of 29.
- `uniqeph()` merged the I/NAV and F/NAV records of one IODnav when they sorted next
  to each other. HAS and CLAS regression outputs are bit-identical with both
  fixes; MADOCA outputs stay within their ≤ 6 mm run-to-run variation.

## 6. Risks

- ED-259 (equations and field order) is not held. The mitigations are the SDD Table 7,
  cssrlib, the RTKLIB L1 long-term analogue, and empirical validation on AU stations.
- The rate reference time cannot be checked on the samples (rates ≈ 0, §1.2).
- GPS satellites without L5 (Block IIR/IIR-M) cannot form the L1/L5 IF and drop out.
- The PVS-on-L5 format is transitional: L5b brings a new format.
- SDD typos: Table 7 δB "max error 0.015625 m/s" (should be m). The DAS SDD and the SIS SDD
  disagree on the δḂ max error (0.00012207 vs 0.0012207).

## 7. Validation (2026-11-06)

Float PPP, GPS L1/L5 + Galileo E1/E5a, mean / rms of the last 20 min of one hour vs
the RINEX header position (`igs14_L5copy.atx`):

| Case | Mode | 2D rms | U rms | satellites |
|------|------|--------|-------|------------|
| ALBY 2025-233 h (SIS) | kinematic | 0.060 m | 0.126 m | 11.7 |
| ALBY 2025-233 h (SIS) | static | 0.080 m | 0.064 m | 11.7 |
| ALIC 2025-110 f (DAS) | kinematic | 0.66 m | 0.92 m | 11.0 |
| Tokyo 2026-296 u (out of area), post | kinematic | 0.15 m | 0.18 m | 10.4 |
| Tokyo 2026-296 u (out of area), `mrtk run` | kinematic | 0.15 m | 0.18 m | 10.4 |

cssrlib `test_ppppvs.py` on the same ALBY / ALIC inputs (static): 0.10 / 0.53 m and
0.29 / 0.78 m. ALIC is poor in both, about 1 m low at the end of the hour. The
header positions of both stations agree with the IGS weekly SINEX to about 1 cm,
and float PPP of the same ALIC observations with CODE final products (Galileo
E1/E5a) ends within 0.17 m, so the offset is in the DAS-logged PVS corrections
of that hour (details in `tests/data/southpan/README.md`). The DAS path is not
validated in #418; that, and the cause of the ALIC offset, are tracked in #462.
Converged accuracy against the IOC targets still needs a long DAS log with daily
IGS RINEX (§3.3).

Regression tests: `utest_pvs`, `pvs_*` / `rtkrcv_pvs` on `g5p3_296u-v1`, and
`pvs_alby_*` / `pvs_alic_*` on `southpan-v1` (tests/data/southpan/README.md).
