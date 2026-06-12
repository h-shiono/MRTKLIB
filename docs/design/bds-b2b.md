# BeiDou PPP-B2b (B2b-I SSR) Support — Design

Status: **Phase 1 (post-processing) implemented & validated** (2026-06-12):
decoder bit-exact vs cssrlib on 1 h of live broadcast; BDS float PPP converges
to cm-level scatter on the cssrlib-data doy2025-233 dataset. Phase 2
(real-time) pending B2b-enabled SBF capture. Authoritative source:
`upstream/BeiDou_PPP_B2b_ICD_v1.0.pdf` (BDS-SIS-ICD-PPP-B2b-1.0, 2020-07).
Reference implementation: cssrlib `cssr_bds.py` (cross-check golden, as done
for Galileo HAS).

## 1. Service overview

- PPP-B2b broadcasts SSR corrections (orbit / clock / code bias / URAI) on the
  **B2b-I** signal (1207.14 MHz, BPSK(10), 1000 sps) from **BDS-3 GEO**
  satellites C59 / C60 / C61 (80°E, 110.5°E, 140°E). Coverage: China and
  surrounding area — all three GEOs are visible from Japan.
- Corrections target **BDS-3 and GPS** (Galileo / GLONASS slots are defined in
  the mask but not broadcast as of 2026).
- **No phase biases** are broadcast → float PPP only (same situation as
  Galileo HAS Initial Service). Datum: BDT time, BDCS (≈ITRF) frame.
- Config axis already reserved: `correction = "bds-b2b"` (`CORR_BDS_B2B = 5`
  in `include/mrtklib/mrtk_opt.h:121`). Requires
  `satellite_ephemeris = "brdc+ssrapc"`, mirroring `gal-has`.

## 2. Message structure (ICD §6)

Frame (1 s): 16-sym preamble `0xEB90` + 6-sym PRN + 6-sym reserved +
972 symbols of 64-ary LDPC(162,81)-encoded data. Decoded message =
**486 bits**: MesType(6) + data(456) + **CRC-24** (generator = CRC-24Q —
reuse `rtk_crc24q()`). Receivers (Septentrio SBF `BDSRawB2b`, block 4242)
perform LDPC decoding and deliver the 486-bit message with a CRC-pass flag,
so **no LDPC/RS implementation is needed on our side** (unlike HAS RS(255,32)).

Common header after MesType: Epoch (17 bit, **BDT seconds-of-day** — resolve
day via `bdt2time`/`bdt2gpst` from receiver time), Reserved(4), IOD SSR(2).

| MT | Content | Key fields |
|----|---------|-----------|
| 1 | Satellite mask | IODP(4) + 255-bit mask: slots 1–63 BDS, 64–100 GPS, 101–137 GAL, 138–174 GLO |
| 2 | Orbit + URAI (6 sats) | per sat 69 bit: SatSlot(9), IODN(10), IOD Corr(3), radial s15×0.0016 m, along s13×0.0064 m, cross s13×0.0064 m, URAI(6) |
| 3 | Code bias (≤31 sats) | per sat: SatSlot(9), N(4), N×{mode(4), DCB s12×0.017 m} |
| 4 | Clock (23 sats/subtype) | IODP(4), SubType1(5); per sat 18 bit: IOD Corr(3), C0 s15×0.0016 m. Sat order = mask-compressed order, offset SubType1×23 |
| 5 | URAI (70 sats/subtype) | IODP(4), SubType2(3) |
| 6 | Clock+orbit combo 1 | mask-ordered clock run (Slot_S start) + MT2-style orbit blocks |
| 7 | Clock+orbit combo 2 | slot-addressed clock (27 bit/sat incl. SatSlot) + orbit blocks |
| 63 | Null | filler |

- Signed fields are two's complement; **two invalid encodings** per ICD /
  cssrlib: `-2^(n-1)` and `-(2^(n-1)-1)` → treat as "no correction".
- IOD chain: data usable only when **IOD SSR** matches across MTs; clock and
  orbit combine only when **IOD Corr** matches (MT2 vs MT4/6/7); MT4/5/6
  satellite indexing valid only when **IODP** matches MT1 mask. On IODP
  change, keep using the old mask until all types align (ICD §6.2.2.2).
- Nominal validity (ICD Table 6-12): orbit **96 s**, clock **12 s**, DCB
  86400 s → `ssr->udi[0] = 96`, `udi[1] = 12` (satpos_ssr uses udi/2 offsets).

## 3. Sign conventions (validated against cssrlib)

| Quantity | ICD | Store in `ssr_t` |
|----------|-----|------------------|
| Orbit δO (r/a/c) | X_orbit = X_brdc **−** [e_r e_a e_c]·δO (eq 7-5/7-9) | `deph[] = +δO` — satpos_ssr already subtracts (`mrtk_eph.c:862`). **Unlike HAS (which negates)** |
| Clock C0 | t_sat = t_brdc **−** C0/c (eq 7-10) | `dclk[0] = −C0` — satpos_ssr adds (`mrtk_eph.c:865`). cssrlib does the same reversal |
| DCB | l̃ = l **−** DCB (eq 7-1) | `cbias = −DCB` — corr_meas applies `P += cb`. **Opposite of MADOCA/HAS storage; must be confirmed bit-exact vs cssrlib golden** |

Orbit corrections reference the **broadcast-ephemeris APC** (like brdc) — no
`satantoff`, same as HAS. BDS clock datum is the B3I signal; MT3 DCBs are
relative to each system's clock reference, so applying eq (7-1) per signal
before forming iono-free pairs is sufficient.

## 4. Ephemeris matching — the key design risk

`IODN` (10 bit, MT2) identifies the broadcast ephemeris the correction applies
to:

- **GPS**: IODN ↔ IODC of LNAV. Per cssrlib, `iodn & 0xff` = IODE → existing
  `seleph()` GPS path works unchanged.
- **BDS**: IODN ↔ IODC of **B-CNAV1 (B1C)** — *not* the D1/D2 (B1I) AODE.
  - Current `seleph()` SYS_CMP branch matches `toes % 2048 == iode*8 % 2048`
    (`mrtk_eph.c:503`) — that is the CSSR/MADOCA convention and is **wrong for
    B2b**. Need a B2b-aware selection: match `iodn & 0xff` against
    `eph->iode` of CNAV1 records (`eph->type == 2`).
  - **Post-processing**: RINEX 4 BRDC already parsed — `decode_eph_bds_cnav()`
    fills `iodc`/`iode`/`type` (`mrtk_rinex.c:1480–1484`). Use a RINEX 4 nav
    file (e.g. BRDC00WRD) instead of RINEX 3 brdc.
  - **Real-time**: SBF decoder currently handles only `BDSRaw` 4047 (D1/D2).
    Add `BDSRawB1C` (block **4218**) → B-CNAV1 ephemeris decoder
    (cssrlib `rawnav.py decode_bds_b1c` as reference).
  - `getseleph(SYS_CMP)` gains meaning: 0 = D1/D2 (default, CSSR matching),
    1 = CNAV1 (selected automatically when `correction == bds-b2b`).

## 5. Architecture — mirror `src/has/`

New module `src/b2b/mrtk_b2b.c` + `include/mrtklib/mrtk_b2b.h`:

- `b2b_t`: mask state (per-IODP satellite list, old+new for the IODP
  transition rule), staging `ssr_t ssr[MAXSAT]`, BDT day/week resolution
  state. Heap-allocate via `b2b_new()` / `b2b_free()` (has_t precedent;
  ~1.6 MB class of struct — never stack).
- `b2b_input_msg(b2b, prn, const uint8_t *msg /*61 B, 486 bits*/, gtime_t)`:
  CRC-24Q check → MT dispatch (MT1/2/3/4/5/6/7) → update staging `ssr[]`
  with `update=1`. Returns 10 when corrections changed (HAS convention).
- `b2b_input_file()`: feed `.b2b` records for post-processing.
- `b2b_sel_biascode(sys, code)`: MT3 signal-and-tracking-mode table
  (ICD Table 6-5) → `CODE_*` with same-tracking-mode fallback:
  BDS 0=B1I(C2I) 1=B1C(D) 2=B1C(P) 4=B2a(D) 5=B2a(P) 7=B2b-I 8=B2b-Q 12=B3I;
  GPS 0=L1C/A 1=L1P 4=L1C(P) 5=L1C(D+P) 7=L2C(L) 8=L2C(M+L) 11=L5I 12=L5Q
  13=L5I+Q; GAL 1=E1B 2=E1C 4=E5aQ 5=E5aI 7=E5bI 8=E5bQ 11=E6C.

Integration points (all mirroring HAS, file:line of the HAS counterpart):

| Point | HAS precedent |
|-------|---------------|
| postpos: `update_bds_b2b()` in epoch loop, IOD-gated copy → `navs.ssr_ch[0]` | `mrtk_postpos.c:364–402, 669` |
| rtksvr: route new raw return code **14** → `svr->b2b` → `nav.ssr_ch[0]` | `mrtk_rtksvr.c:565–578, 1230` |
| SBF: `decode_bdsrawb2b()` (block 4242) → `raw->b2b_msg/b2b_prn/b2b_time`, ret 14 | `mrtk_rcv_septentrio.c:920–976` (4024) |
| l6extract: extract 4242 → `.b2b` file | `l6extract.c:222–300` |
| resolve_correction: lift the "reserved" rejection, require SSRAPC eph | `mrtk_opt.c:186, 209` |
| corr_meas: `CORR_BDS_B2B` joins the SSR branch; keep carrier when pbias absent | `mrtk_ppp.c:646, 694–703` |

SVID note: SBF svid→sat already maps C41–C63 (svid 223–245,
`mrtk_rcv_septentrio.c:236`), so GEO C59–C61 frames decode without changes.

`.b2b` file format (analogous to `.has` 64-byte records): fixed **72-byte**
little-endian records `[tow_ms u4][wnc u2][prn u1][flags u1][msg u8[61]][pad
u8[3]]`; msg = 486-bit message MSB-first (MesType..CRC), flags bit0 =
receiver CRC pass.

## 6. Phasing

1. **Phase 1 — post-processing** (parallel to user's test-data hunt):
   decoder core + utest, l6extract 4242, `update_bds_b2b()`, config
   validation, seleph CNAV1 matching. Validate decoder bit-exact vs cssrlib
   (`scripts/tests/b2b_cssrlib_decode.py`, venv pattern from HAS).
2. **Phase 2 — real-time**: SBF 4242 + **4218 B-CNAV1** decoders, rtksvr
   routing (ret 14), E2E replay of the same dataset.
3. **Phase 3 — regression**: trimmed SBF bundle under `tests/data/b2b/`,
   ctest chain `b2b_setup/extract/convert/ppp/ppp_check/cleanup` (HAS
   pattern, `CMakeLists.txt:1472–1563`), docs (mkdocs correction page).

Accuracy target: published PPP-B2b performance ≈0.1–0.2 m H / 0.3 m V
(95%, converged) in coverage area; gate thresholds to be measured on real
data first (HAS lesson: thresholds set loose, then tightened).

## 6.5 Phase 1 implementation findings (2026-06-12)

- **Two latent shared-code bugs surfaced** (B2b is the first consumer of
  CNAV-type BDS ephemerides in the positioning path):
  1. `eph2pos()` ignored the CNAV `Adot`/`ndot` terms → CNV1-derived satellite
     positions were 1–63 m off vs D1. Fixed per BDS-SIS-ICD-B1C-1.0 Table 7-2
     (`Ak = A + Adot·tk` for the radius, `n += ndot/2·tk`); legacy ephemerides
     decode with `Adot = ndot = 0` and reduce to the previous expressions
     exactly.
  2. `satpos_ssr()` used a fixed 60 s orbit age gate; B2b updates each
     satellite every ~57 s with multi-second latency (ICD validity 96 s), so
     healthy corrections were rejected. The gate now honours `udi[0]` when it
     exceeds `MAXAGESSR`.
- **Live broadcast content (doy 2025-233)**: MT1/2/3/4 + null only (no
  MT5/6/7). BDS gets orbit + clock + 8 DCB modes (13 MEO/IGSO sats); GPS gets
  orbit + clock but **no DCB** → GPS pseudoranges are dropped by the SSR rule
  and GPS does not contribute. GAL/GLO masks empty. cssrlib quirk: its golden
  decode drops GPS MT2 entries (slot2prn); MRTKLIB decodes them.
- **Nav-file requirement**: RINEX 4 with CNV1/CNV2 records (e.g.
  `BRD400DLR_S`); the BKG `BRDC00WRD` product is RINEX 3.05 and unusable.
  Receiver-local 1 h logs are insufficient (B1C nav collection is sparse).
- **Validation result**: BDS-only float PPP, 1 h mosaic-X5 data — mean
  nsat 6.7, last-20-min scatter E 2.9 / N 1.4 / U 21 cm, sub-meter agreement
  with the (meter-class) RINEX header position.

## 7. Open questions

1. **Receiver availability of BDSRawB2b (4242)** on the user's hardware
   (mosaic-G5 firmware?) — user is sourcing test data now. If unavailable,
   alternatives: PolaRx/X5 logs, or public B2b captures.
2. RINEX 4 BRDC with CNV1 records for the PP dataset (BRDC00WRD works).
3. Confirm with real data whether live MT3 DCB set covers the signals we
   track (B1C/B2a vs B1I/B3I pairs), and which BDS pair to prefer for
   iono-free PPP under `bds-b2b`.
4. GAL/GLO: decode + store (mask slots defined) but expect empty until the
   service broadcasts them.
