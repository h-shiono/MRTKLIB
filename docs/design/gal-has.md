# Galileo HAS (High Accuracy Service) Support

Status: in development (branch `feat/gal-has`)
Reference: Galileo HAS SIS ICD Issue 1.0, May 2022 (`upstream/Galileo_HAS_SIS_ICD_v1.0.pdf`)

## 1. Scope

Decode Galileo HAS corrections (orbit / clock / code bias / phase bias for GPS+Galileo,
broadcast on E6-B C/NAV) and apply them in PPP, for both post-processing
(`mrtk post`) and real-time (`mrtk run`). Config:

```toml
[positioning]
mode = "ppp-kine"
correction = "gal-has"   # CORR_GAL_HAS = 4, already reserved in mrtk_opt.h
```

HAS Initial Service carries no ionosphere/troposphere corrections, so the target
mode is float PPP (`ionoopt = EST`), analogous to the MADOCA-PPP path. PPP-AR is a
later phase contingent on phase-bias broadcast.

## 2. Architecture

```
src/has/
├── mrtk_has.c       # page collection (MID/PID), MT1 decode → ssr_t
├── mrtk_has_rs.c    # GF(256) arithmetic + RS(255,32,224) erasure decoder
└── mrtk_has_rs.h    # internal header (not public API)
include/mrtklib/mrtk_has.h   # public API (has_t, entry points)
```

Data flow:

- **Post-processing**: SBF → `mrtk l6extract` extracts GALRawCNAV (SBF block 4024)
  into a `.has` file → `mrtk_postpos.c` detects `.has` infile →
  `update_gal_has(time)` feeds records to `has_input_page()` →
  on complete message, MT1 decode fills `navs.ssr_ch[0][sat]` → existing
  `satpos_ssr()` + `corr_meas()` SSR branch apply corrections.
- **Real-time**: correction stream (slot 2) carries SBF; the Septentrio decoder
  gains a GALRawCNAV handler that stashes the page in `raw_t` and returns a
  dedicated return code; `decoderaw()` routes it to `svr->has` →
  `nav->ssr_ch[0]`.

## 3. `.has` file format

Fixed 64-byte little-endian records:

| Offset | Type      | Field   | Description                                   |
|--------|-----------|---------|-----------------------------------------------|
| 0      | uint32    | tow_ms  | GPS time of week of page reception [ms]       |
| 4      | uint16    | wnc     | GPS continuous week number                    |
| 6      | uint8     | prn     | Galileo PRN (1–40)                            |
| 7      | uint8     | flags   | reserved, 0                                   |
| 8      | uint8[56] | page    | 448-bit HAS page (24-bit header + 424-bit encoded page), MSB-first |

Extraction from SBF GALRawCNAV (block ID 4024, payload after 8-byte SBF header):
TOW u4, WNc u2, SVID u1 (GAL PRN = SVID−70), CRCPassed u1, ViterbiCnt u1,
Source u1, FreqNr u1, RxChannel u1, NAVBits u4[16]. Concatenate the 16 words
MSB-first → 512 bits; bits 0–491 are the C/NAV page; the HAS page is bits
14–461 (56 bytes). Skip records with CRCPassed == 0 and dummy pages
(24-bit HAS header == 0xAF3BC3).

## 4. HAS page and message assembly

24-bit HAS page header: HASS(2) | Reserved(2) | MT(2) | MID(5) | MS(5) | PID(8).
MS is stored minus 1 (0→1 page … 31→32 pages). PID 1–255 (0 reserved).

- Discard dummy pages (header 0xAF3BC3) and pages with HASS == 3 ("don't use";
  on HASS==3 also flush all collected state). HASS 0 (test) and 1 (operational)
  are both accepted (gate by option later if needed).
- Collect pages keyed by (MID, MT): store 424-bit payloads (53 bytes) of
  *distinct* PIDs. When k = MS distinct PIDs are collected, run the RS erasure
  decoder to recover the k×53-byte message, then decode MT1.
- A message not completed within **150 s** of its first page is discarded
  (ICD §6.4.1). A new MID supersedes collection of older MIDs (keep a small
  ring of in-flight MIDs; 2 is sufficient in practice).

## 5. RS(255,32,224) erasure decoder (`mrtk_has_rs.c`)

GF(256) with primitive polynomial 0x11D (α⁸+α⁴+α³+α²+1). Generator polynomial
g(x) = Π_{i=1..223} (x − αⁱ) (ICD Eq. 8, coefficients in Table 42). Build the
systematic 255×32 generator matrix **G** = [I₃₂; P] at init by systematically
encoding the 32 unit information vectors (polynomial remainder method, ICD
§6.2.2); do NOT embed the Annex B CSV.

Indexing (ICD §6.3/6.4): encoded page with PID p corresponds to row p−1 of G;
column j (0-based) corresponds to message page j+1. Pages 1..32 are systematic.

Decoding: given k received pages with PIDs p₁..p_k, build D (k×k) from rows
pᵢ−1 and the first k columns of G, invert in GF(256) (Gauss-Jordan), and for
each of the 53 octet positions j multiply D⁻¹ by the received column vector to
recover message octets w₁ⱼ..w_kⱼ.

API (internal):

```c
int has_rs_init(void);  /* build GF tables + G; idempotent; 0:ok */
/* pages[i]: 53-byte encoded page payload with PID pids[i] (1..255), all PIDs
 * distinct, 1<=k<=32. out: k*53 bytes (message pages concatenated).
 * return 0:ok, -1:singular/invalid input */
int has_rs_decode(const uint8_t pages[][53], const uint8_t* pids, int k, uint8_t* out);
```

Validation: ICD Annex C (PDF pages 48–51) gives a worked decoding example and
Annex D (page 52) a full message example — use them as unit-test vectors.

## 6. MT1 message decoding (`mrtk_has.c`)

Bit layout (MSB-first; use `getbitu`/`getbits` from `mrtk_bits.h`; note 13/12/11-bit
signed fields are two's complement → `getbits`).

**MT1 Header (32 bits)**: TOH(12, s, 0–3599) | Mask Flag(1) | Orbit Corr Flag(1) |
Clock Full-set Flag(1) | Clock Subset Flag(1) | Code Bias Flag(1) | Phase Bias
Flag(1) | Reserved(4) | Mask ID(5) | IOD Set ID(5). Body blocks follow in flag
order: Mask, Orbit, Clock Full-set, Clock Subset, Code Bias, Phase Bias.

**Mask block**: Nsys(4); per GNSS: GNSS ID(4; 0=GPS, 2=Galileo, others skip) |
SatM(40) | SigM(16) | CMAF(1) | CM(Nsat×Nsig, only if CMAF=1) | NM(3);
then Reserved(6). Satellite index i (0-based, MSB first) ↔ PRN i+1. Signal
index per Table 20 (see §8). Nsat = popcount(SatM), Nsig = popcount(SigM).
Masks must be cached per Mask ID: an MT1 with Mask Flag=0 references a
previously received mask with the same Mask ID (keep a cache of up to 32
masks keyed by Mask ID; in practice the current + previous suffice but the
cache is cheap). The same applies to IOD Set ID for orbit-less messages
(§7.6: corrections link to Reference IODs of a prior orbit block with the
same Mask ID + IOD Set ID pair).

**Orbit block**: VI(4); per masked satellite (all GNSS, mask order):
IODref(10 for GAL, 8 for GPS) | DR(13, 0.0025 m) | DIT(12, 0.0080 m) |
DCT(12, 0.0080 m). "Data not available" sentinels: DR=−4096·0.0025 (pattern
1000…0), DIT/DCT=−2048·0.0080 → skip satellite.

**Clock full-set block**: VI(4); DCM(2) per GNSS (multiplier = DCM+1);
then DCC(13, 0.0025 m) per masked satellite. DCC pattern 1000…0 = data not
available (skip); pattern 0111…1 = satellite shall not be used (mark unhealthy
/ clear correction).

**Clock subset block**: VI(4) | Nsys_sub(4); per GNSS in subset: GNSS ID(4) |
DCM(2) | SatM_sub(Nsat bits, relative to the ones of the full SatM) |
DCC(13) per subset satellite.

**Code bias block**: VI(4); per masked satellite × masked signal (or Cell Mask
cell if CMAF=1): CB(11, 0.02 m, ±20.46). Pattern 1000…0 = n/a.

**Phase bias block**: VI(4); per masked satellite × masked signal:
PB(11, 0.01 cycles, ±10.23) | PDI(2). PB n/a pattern 1000…0.

**Time of application** (ICD §7.7): with reception time GST_r (use page
reception time converted to GST seconds; GPST≈GST, week offset irrelevant when
working with `gtime_t`): Hr = floor(sow_r/3600) within the GST week;
t_MT1 = Hr·3600 + TOH if ≤ GST_r else (Hr−1)·3600 + TOH.

## 7. Mapping to `ssr_t` (`nav->ssr_ch[0][sat]`) and sign conventions

`satpos_ssr()` (src/data/mrtk_eph.c) applies `rs += −R·deph` with
deph = {radial, along, cross} and `dts += dclk/CLIGHT`, selecting the broadcast
ephemeris by `ssr->iode`. HAS defines x̃ = x + R·δR (ICD Eq. 22) and
d̃t = dt + δC/c (Eq. 23). Therefore:

- `deph[0] = −DR`, `deph[1] = −DIT`, `deph[2] = −DCT` (**negated**), `ddeph = 0`.
- `dclk[0] = +DCC × DCM_multiplier`, `dclk[1] = dclk[2] = 0`, `hrclk = 0`.
- `iode = IODref` (IODnav for GAL, IODE for GPS).
- `iod[0..5] = IOD Set ID` (satpos_ssr requires iod[0]==iod[1]).
- `t0[0..5] = t_MT1` of the providing message; `udi[*] = 0` (HAS reference time
  is t_MT1 directly; no RTCM-style udi/2 shift).
- `cbias[code−1] = +CB` [m], `vcbias=1`. HAS code biases are **added** to the
  pseudorange (Eq. 25, P̃ = P + d) and replace BGD/TGD — same direction as the
  existing `corr_meas()` SSR branch (`P[i] += cb`).
- `pbias[code−1] = +PB × λ` [m] (convert cycles→m), `vpbias=1`,
  `discnt[code−1] = PDI`. (Eq. 26, Φ̃ = Φ + δ.)
- `ura` = leave 0 (HAS broadcasts none); `update = 1`; `vendor`: add
  `SSR_VENDOR_HAS` if a discriminator is needed.
- Clock "shall not be used" sentinel → clear `t0[1]` (or set svh path) so
  satpos_ssr rejects the satellite.

Orbit reference point is the ionosphere-free APC of the navigation message
(ICD §7.1.2) — same as broadcast eph, so no CoM offset (`opt=0` path; do not
add satantoff for HAS, matching the MADOCA convention — verify at integration).

## 8. Signal index → obs code mapping (Table 20)

| Idx | Galileo  | CODE_   | GPS        | CODE_   |
|-----|----------|---------|------------|---------|
| 0   | E1-B     | L1B     | L1 C/A     | L1C     |
| 1   | E1-C     | L1C     | —          | —       |
| 2   | E1-B+C   | L1X     | —          | —       |
| 3   | E5a-I    | L5I     | L1C(D)     | L1S     |
| 4   | E5a-Q    | L5Q     | L1C(P)     | L1L     |
| 5   | E5a-I+Q  | L5X     | L1C(D+P)   | L1X     |
| 6   | E5b-I    | L7I     | L2 CM      | L2S     |
| 7   | E5b-Q    | L7Q     | L2 CL      | L2L     |
| 8   | E5b-I+Q  | L7X     | L2 CM+CL   | L2X     |
| 9   | E5-I     | L8I     | L2 P       | L2W     |
| 10  | E5-Q     | L8Q     | —          | —       |
| 11  | E5-I+Q   | L8X     | L5 I       | L5I     |
| 12  | E6-B     | L6B     | L5 Q       | L5Q     |
| 13  | E6-C     | L6C     | L5 I+Q     | L5X     |
| 14  | E6-B+C   | L6X     | —          | —       |

Biases are stored under these obs codes. `corr_meas()` needs a HAS-specific
selector (`has_sel_biascode(sys, code)`) with same-frequency fallback
(component ↔ combined, e.g. obs L5Q ↔ bias L5X), since the receiver-tracked
attribute may differ from the broadcast bias signal.

## 9. Public API (`include/mrtklib/mrtk_has.h`)

```c
typedef struct { ... } has_t;            /* page collector + mask/IOD caches; heap-allocate */
has_t* has_new(void);                    /* allocate + init (calls has_rs_init) */
void   has_free(has_t* has);
/* one received page; time = reception time (GPST). returns 10 when a complete
 * MT1 was decoded and ssr[] updated, 0 otherwise, <0 on error */
int    has_input_page(has_t* has, int prn, const uint8_t* page56, gtime_t time);
/* read .has records from fp up to time limit; same return convention */
int    has_input_file(has_t* has, FILE* fp, gtime_t tmax);
/* decoded corrections live in has->ssr[MAXSAT]; caller copies to nav->ssr_ch[0] */
```

The struct keeps its own `ssr_t ssr[MAXSAT]` (like `rtcm_t`) so callers copy to
`nav->ssr_ch[0]` exactly as the MADOCA post-processing loop does.

## 10. Integration points

- `resolve_correction()` (src/pos/mrtk_opt.c:184): allow CORR_GAL_HAS for
  PPP modes (kinema/static/fixed); keep CORR_BDS_B2B reserved.
- `mrtk_postpos.c`: detect `.has` infile extension → `gal_has_file`; open/feed
  in `update_gal_has()` mirroring `update_qzssl6e()`; copy `has->ssr` to
  `navs.ssr_ch[0]`.
- `corr_meas()` (src/pos/mrtk_ppp.c): route CORR_GAL_HAS through the SSR branch
  with `has_sel_biascode()` instead of `mcssr_sel_biascode()`.
- CMakeLists.txt: add `src/has/mrtk_has.c`, `src/has/mrtk_has_rs.c`.
- RT (phase 2): SBF 4024 handler in `mrtk_rcv_septentrio.c`, routing in
  `decoderaw()` (src/stream/mrtk_rtksvr.c), `svr->has` lifecycle.

## 11. Validation plan

1. RS decoder unit test against ICD Annex C/D vectors.
2. Decode `G5P3162a.has` (extracted from `G5P3162a.sbf`: 10 GAL satellites,
   ~1 h, HASS=1, MS=10–11) and cross-check decoded orbit/clock/bias values
   against cssrlib's HAS decoder output on the same data
   (`scripts/tests/has_cssrlib_decode.py`). cssrlib (MIT License,
   © 2021 Rui Hirokawa, https://github.com/hirokawa/cssrlib) is used as an
   independent reference implementation at development/validation time only;
   it is not bundled with or linked into MRTKLIB.
3. PPP run: `mrtk post` with RINEX converted from the same SBF, brdc nav,
   `correction = "gal-has"` — convergence and accuracy vs. surveyed position.
4. Full ctest regression (no change to existing tests).

### 11.1 Bundled regression test data

The dataset `has-v1` (~2.2 MB gz; a release asset of
[h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data) since #386,
previously `tests/data/has/has_testdata.tar.gz` in this repository) holds
`G5P3162a_15m.sbf`: a block-filtered, 15-minute-trimmed slice of the full-hour
mosaic-G5 capture `G5P3162a.sbf` (2026/06/10 23:59:31 → 00:14:30 GPST, TOW
window [345571000, 346471000) ms). It is produced by
`scripts/tests/sbf_filter.py` keeping SBF blocks **4027** MeasEpoch, **4017**
GPSRawCA, **4023** GALRawINAV, **4024** GALRawCNAV (5914 ReceiverTime is not
present in the source and is not needed — the SBF decoder derives time from
each block's own TOW). The QZSS L6E block (4271) is intentionally excluded from
the bundle to keep it under the size budget; L6E extraction is exercised
separately (see §11.3).

The ctest pipeline (`has_*` in `CMakeLists.txt`) is:

```
has_setup    extract the tar.gz
has_extract  mrtk l6extract  → G5P3162a_15m.has  (assert 4834 valid records)
has_convert  mrtk convert    → RINEX obs/nav
has_ppp      mrtk post -k conf/has/ppp_gal_has.toml → float PPP .pos
has_ppp_check scripts/tests/check_has_ppp.py: epochs ≥ 95 % of 900, all Q=6,
             last-5-min mean within 1.0 m horiz / 1.5 m 3D of the reference
has_cleanup  remove extracted artefacts
```

**Trim equivalence proof** (all comparisons within the common 15-min window,
trim must not change results):

| Check | Result |
|-------|--------|
| (a) `.has` records (orig-window vs filtered) | **byte-identical**, 4834 records, md5 `6e43487dcd30f8ceb8cf035317b68c24` |
| (b) `mrtk convert` OBS records | **identical**, 900 epochs, md5 `db9d8624…`. NAV: orbit/clock coefficients identical; only the GAL-week/data-source field (line 5) and transmission-time `ttr` (line 7) differ for 13 ephemerides — a transmission-time-snapshot artifact of the shorter horizon, **zero positioning effect**. Against a same-block-set full-hour control the trimmed NAV is a perfect subset (0 differences). |
| (c) `mrtk post` PPP solutions | **bit-identical** over all 893 common solution epochs: max \|ΔENU\| = 0.000000 m, 0 Q mismatches. |

### 11.2 Reference position and accuracy status

Reference receiver position (passed to `check_has_ppp.py --ref-xyz`):

```
XYZ = -3961905.2046  3348992.8024  3698212.4790   (m, ITRF current epoch)
LLH = 35.666341868°  139.792211087°  59.837 m
```

**Provenance:** derived 2026-06-12 from MADOCA-PPP static + combined PPP-AR over
the full hour, ITRF current epoch, ~1–2 cm. As a cross-check against the CLAS
reference frame (F5.1), Ref2 − Ref1 = **+4.4 cm in East** — within the
combined PPP-AR / frame-realisation uncertainty.

**HAS absolute-accuracy status:** the HAS float solution sits **~0.56 m
horizontal / ~0.69 m 3D** from this reference (full-hour: dE +0.390 dN +0.397
dU −0.400 m, self-RMS E/N 0.075 / U 0.38 m; trimmed-window last-5-min mean:
horiz 0.575 m / 3D 0.634 m, all Q=6). This ~0.5 m offset is larger than mature
PPP would give and the **accuracy investigation is open** (candidate causes:
code-bias sign/selection, orbit/clock reference-point handling, troposphere
estimation over the short window). The `has_ppp_check` thresholds (1.0 m horiz /
1.5 m 3D) are set with ~2× margin over the measured offsets and never tighter
than the full-hour result (0.56 m horiz) + 0.3 m, so the gate guards against
regression without prejudging the accuracy investigation.

### 11.3 l6extract L6E (mosaic-G5 block 4271)

`apps/l6extract/l6extract.c` extracts QZSS L6E frames from SBF block **4271**
(QZSRawL6E), whose payload is byte-identical to the L6D block 4270 except the
Source field (offset 9) = 2 (L6E) vs 1 (L6D). mosaic-G5 numbers the L6E stream
+10 above the L6D PRN, so the per-PRN files are written as J204/J205/J209 (for
the J194/J195/J199 L6D satellites), matching the MADOCA `.l6` PRN convention.
Verified on the full SBF: J204/J205/J209, **3660 frames each, 250-byte frames
with the 0x1ACFFC1D preamble**, and one extracted file (J204) drives a
`mrtk post` MADOCA-PPP smoke run to Q=6 float solutions.

> Since v0.7.12 ([#362](https://github.com/h-shiono/MRTKLIB/issues/362))
> l6extract takes the signal and PRN from the L6 frame header (the 4271
> header PRN is already the L6E PRN 204/205/209), and the files are named
> `{prefix}.{PRN}.l6e` (e.g. `l6.204.l6e`).

### 11.4 Future work

- **Real-time HAS replay ctest** (rtkrcv file replay). Not added here: the
  rtkrcv headless RT replay path is known env-flaky on the maintainer's machine
  (cf. `rtkrcv_rt`). To be added once the RT HAS decoder lands and the replay
  harness is stabilised.
