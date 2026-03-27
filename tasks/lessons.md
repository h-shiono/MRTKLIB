# MRTKLIB — Lessons Learned

> **Rule:** After every user correction, add an entry here immediately.
> AI reviews this at session start. Patterns promoted to CLAUDE.md Section 7.5 when high-impact.

---

## L-001: obsdef-dependent wavelength lookup is dangerous

**Context:** `sat_lambda(QZS, f=1)` returned L2 wavelength (0.2442 m), but the BINEX decoder
assigns L5X to QZS slot 1 via `code2freq_idx()` (obsdef-based).
Wavelength mismatch (~4%) caused DD residuals of ~74 km → QZS always rejected as outlier.

**Rule:** Do not rely on `sat_lambda` / `obs_code_for_freq` when obsdef has been reordered.
`apply_pppsig()` reshuffles obsdef for MADOCA signal selection.
QZS frequency slot assignment: assume obsdef-based order (slot0=L1, **slot1=L5**, slot2=L2).

**Fix:** `src/pos/mrtk_ppp_rtk.c` — `obs_code_for_freq(QZS, f=1)` → `CODE_L5Q`

---

## L-002: SIS boundary detection must not assume orb_tow == clk_tow

**Context:** The "0s boundary" detection in `clas_osr_satcorr_update` required
`timediff(t0[0], t0[1]) == 0.0` (orbit epoch == clock epoch).
In ST12 datasets, orbit always leads clock by 5 s → condition never met
→ SIS correction never applied → ~20 float epochs at every 30 s correction boundary.

**Rule:** Orbit and clock update timing can differ across CLAS datasets.
Track `prev_orb_tow` directly to detect orbit epoch advances,
eliminating the orb_tow == clk_tow assumption.

**Fix:** Added `prev_orb_tow` field to `include/mrtklib/mrtk_clas.h`;
updated boundary detection logic in `clas_osr_satcorr_update()`.

---

## L-003: `trace()` is compiled out by default

**Context:** Added `trace(NULL, 2, ...)` for debugging but got no output.

**Reason:** CMakeLists.txt does not define `-DTRACE`, so `trace()` is a no-op macro.

**Rule:** Use `fprintf(stderr, ...)` for debug output. Always remove before committing.

---

## L-004: Extract test data tarball before running tests

**Context:** Running `rnx2rtkp` directly produced "no rec ant pcv" and exited.
Test data tarball had not been extracted.

**Fix:**
```bash
tar xzf tests/data/claslib/claslib_testdata.tar.gz -C tests/data/claslib/
```

---

## L-005: Forward declaration required when exposing static functions

**Context:** Removing `static` from `ephpos()` to call it from `clas_osr_satcorr_update()`
failed because `nav_t` was an anonymous struct in `mrtk_nav.h`, blocking forward declaration.

**Fix:**
1. Tag the struct: `struct nav_s` in `mrtk_nav.h`
2. Add forward declaration `struct nav_s; typedef struct nav_s nav_t;` in `mrtk_eph.h`
3. Un-static `seleph()` / `ephpos()` in `src/data/mrtk_eph.c`

---

## L-006: Setting `stec->data[k].flag = -1` on age error has broad side effects

**Context:** `stec_data()` set `flag = -1` on age errors, permanently disabling the slot.
New data arriving later could not reuse it.

**Rule:** On age error, just `return 0`. Do not mutate the flag field.

---

## L-007: `fastfix` must be initialized in `clas_bank_init()`

**Context:** `fastfix[i]` was not initialized in `clas_bank_init()`,
causing `clas_bank_get_close()` to fail to retrieve trop data in some cases.

**Fix:** Set `bank->fastfix[i] = 1` for all networks during init
(equivalent to upstream's `init_fastfix_flag()`).

---

## L-008: CLAS configs require nf=2

**Context:** Using nf=3 dropped CLAS PPP-RTK fix rate to 67% (RT) / 87% (PP).
When E5a bias is unavailable, the third frequency slot receives invalid corrections,
degrading ambiguity resolution.

**Rule:** Always use `nf=2` for CLAS configurations.
`nf=3` only if E5a bias is explicitly confirmed available.

**Fix:** v0.5.1 — RT 67→93%, PP 87→99% after reverting to nf=2.

---

## L-009: Do not hardcode GNSS indicators in RTCM3 1005/1006

**Context:** `encode_type1005()` / `encode_type1006()` hardcoded GPS=1, GLO=1, GAL=0.
cssr2rtcm3 was sending MSM for GPS+GAL+QZS, contradicting the 1005 header (GLO=1, GAL=0).
mosaic-G5 detected the inconsistency and rejected correction data.

**Rule:** Derive GNSS indicators dynamically from `rtcm->obs` satellite systems.
Hardcoded values cause receiver compatibility failures.

**Fix:** `f9b9100` — auto-detect constellation from obs data.

---

## L-010: VRS base coordinates must not be updated every epoch

**Context:** cssr2rtcm3 used SPP position as VRS base coordinate each epoch.
Meter-level jumps destroyed DD ambiguity continuity → RTK fix impossible.
mosaic-CLAS used a fixed coordinate and achieved fix.

**Rule:** VRS base coordinate stability matters more than absolute accuracy.
Latch after first SPP fix; never update thereafter.

**Fix:** `4fa4d65` — `has_fixed_pos = 1` latches position after first SPP.

---

## L-011: Parallel arrays for MSM sys/type mapping break with filtering

**Context:** `rtcm3_msm_sys[]` and `rtcm3_msm_types[]` were parallel arrays.
When `set_msm_systems()` compacted the sys array, the types array index shifted.
GAL satellites were encoded as GLO MSM7 (1087).

**Rule:** Do not manage sys→type mapping via parallel arrays.
Use a direct mapping function like `msm_base_type(sys)`.

**Fix:** `21f7b1f` — replaced parallel arrays with `msm_base_type()`.

---

## L-012: `actualdist()` satellite list must not depend on SSR table

**Context:** `actualdist()` in cssr2rtcm3 built the satellite list from `nav->ssr_ch` only.
CLAS corrections are stored in the CLAS internal table, not the SSR table,
so GAL/QZS satellites were missing from dummy observations.

**Rule:** When using CLAS, build the satellite list from broadcast ephemeris.
`clas_ssr2osr()` will drop satellites without CLAS corrections downstream.

**Fix:** `c9990d5` — switched to broadcast eph-based satellite list.

---

## L-013: A rejected Type-1 signal in SBF MeasEpoch must not skip the whole satellite

**Context:** In `decode_measepoch()`, if a Type-1 sub-block signal was not in obsdef,
the entire satellite including Type-2 sub-blocks was skipped.
mosaic-G5 often places E7Q (E5b) in the Type-1 slot for GAL satellites.
Without E7Q in signals config, only 1 of 9 GAL satellites was decoded.

**Rule:** Process Type-2 sub-blocks even when Type-1 is rejected.
Always extract Type-1 P1/D1 as the base for Type-2 delta computation.
Commit obs entry only if at least one signal was accepted.

**Fix:** `a173e3c` — Type-2 processing continues after Type-1 rejection. GAL 1→9 satellites.

---

## L-014: Frequency slot f==1 for GAL is not always L2

**Context:** `clas_osr_zdres()` hardcoded a skip for `f==1 && SYS_GAL`.
With `signals=["E1C","E5Q"]` nf=2, obsdef reordering puts E5a in slot[1].
The skip excluded E5a residuals → GAL treated as single-frequency → DD AR failed.

**Rule:** Never hardcode frequency slot indices.
Use `code2freq_num()` to determine actual band; skip only for L2 band (freq_num==2).

**Fix:** `7bd043c` — dynamic frequency band detection. `selfreqpair` fallback unified for PPP-RTK.

---

## L-015: RTCM3 output timing must not depend on L6D timestamps

**Context:** cssr2rtcm3 RTCM3 output was gated on `clas->l6buf[0].time` (5 s interval),
causing burst output every 5 s. Age-of-Correction became a 0→5 s sawtooth → accuracy degraded.

**Rule:** Use SBF PVT time (1 s interval) as the output timing reference.
CLAS corrections update every 5 s, but regenerate and transmit OSR every second using the same values.

**Fix:** `03a798d` — switched timing reference to `raw_sbf->time`.

---

## L-016: On macOS, use `cu.*` device nodes for serial output (`tty.*` blocks on DCD)

**Context:** Using `tty.usbmodem*` on macOS blocked on DCD signal wait, halting output.

**Rule:** Always use `cu.usbmodem*` for outbound serial port connections on macOS.

---

## L-017: Septentrio mosaic-CLAS technical characteristics

**Source:** Field testing + Septentrio feedback (2026-03-23)

- **E5a→E5b conversion patent:** mosaic-CLAS converts CLAS E5a corrections to E5b (E8X AltBOC) for RTCM3 output.
- **QZS exclusion reason:** 1/4-cycle shift between GPS L2C and QZS L2C in mixed processing.
- **JGD2011 crustal deformation:** Disable via Position > CLAS > CrustalDeformation = OFF.
- **Satellite cut:** SSR2OSR library cuts satellites with large ionospheric correction delta (details not disclosed).
- **Float/DGPS:** Recommended OFF to keep data coherent.


---

## L-019: cssr2rtcm3 rate limiter must use wall clock, not obs time

**Context:** rtkrcv's L6 rate limiter compares against obs time, but cssr2rtcm3 has no obs.
Without a limiter, file replay runs unbounded.

**Rule:** Compare system wall clock against L6 decode time to detect and throttle replay speed.
Do not port rtkrcv's obs-time-based limiter directly to cssr2rtcm3.

---

## L-021: mosaic-G5 Solution Sensitivity must be Loose for VRS-RTK

**Context:** cssr2rtcm3 → G5 stayed Float for hours despite correct RTCM3 output.
mosaic-CLAS RTCM3 → G5 also stayed Float. mosaic-CLAS internal processing fixed normally.
Root cause: Solution Sensitivity was set to Medium.

**Rule:** VRS-RTK RTCM3 (both cssr2rtcm3 and mosaic-CLAS) requires Solution Sensitivity = **Loose** on mosaic-G5.
Medium/High settings apply stricter ambiguity validation that VRS observations cannot pass.
Always check this setting before debugging code when G5 shows permanent Float.

---

## L-022: a173e3c SBF decoder change causes mrtk run instability

**Context:** Commit `a173e3c` modified `decode_measepoch()` to process Type-2 sub-blocks
even when the Type-1 signal is rejected by obsdef. While this fixed GAL satellite count (#69),
it caused single/float flapping in `mrtk run` (PPP-RTK). Reverting to `2535182` restored stability.

**Rule:** SBF decoder changes affect all downstream consumers (rtkrcv, cssr2rtcm3, convbin).
The #69 fix needs reimplementation that doesn't degrade observation quality for PPP-RTK.
cssr2rtcm3 is unaffected (does not use SBF observations directly).

**Status:** Reverted to `2535182` for RT testing. Proper fix pending.

---

## L-020: CLAS trop/iono corrections fail outside grid coverage (~1000 km)

**Context:** CLAS trop/iono corrections are interpolated from grid points.
If the user position is outside grid coverage, grid lookup fails silently.

**Rule:** `pos1-gridsel` sets the grid search radius. Within Japan, coverage is not an issue.
For testing at non-Japan locations, verify grid lookup succeeds before debugging correction quality.
