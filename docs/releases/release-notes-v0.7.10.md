# Release Notes — v0.7.10

## RTCM3 MSM: BeiDou-3 B2b signal IDs restored (decoder and encoder)

**Release date:** 2026-09-22
**Type:** Bug fix (RTCM3 MSM BeiDou signal table) — no positioning change in the regression suite
**Branch:** `release/v0.7.10`

---

### Overview

Field comparisons on RENAG stations and a mosaic-X5 receiver
([#333](https://github.com/h-shiono/MRTKLIB/issues/333), reported by
@philippebourcier) showed that MRTKLIB decoded no BDS-3 B2b observations from
RTCM3 MSM streams that carried them. The cause: the BeiDou
MSM signal-ID → RINEX code table `msm_sig_cmp[32]` in `src/rtcm/mrtk_rtcm3.c`
was byte-identical to the MALIB parent and left signal IDs 25–27 empty. RTCM
10403.3 Amendment 2 assigns those IDs to the B2b signals `7D` / `7P` / `7Z`,
so every B2b cell resolved to `CODE_NONE` and was skipped with only a
level-2 trace (`unknown signal id=N`).

The encoder was affected too. `to_sigid()` in `src/rtcm/mrtk_rtcm3e.c`
performs the reverse lookup against the same table, so RTCM3 output built
from SBF / NovAtel / BINEX input — whose decoders already emit `CODE_L7D` —
silently lost B2b as well.

### Fix

[PR #335](https://github.com/h-shiono/MRTKLIB/pull/335) adds the three
missing entries (`"7D", "7P", "7Z"` at IDs 25–27) and replaces the stale
"Tentative IDs for B1C and B2a" comment with the source citation. The change
is table-only; the decoder (`save_msm_obs`) and encoder (`to_sigid`) paths
are untouched.

Provenance: PocketSDR's reading of table 3.5-108 lists exactly these three
at IDs 25–27 as standard entries; demo5 carries the same three (marking
`7P`/`7Z` tentative) and MADOCALIB has `7D` only. The further tentative
PocketSDR / demo5 extensions (1S/1L/6D/8D, …) are deliberately **not**
imported.

`docs/dev/pitfalls-public.md` P-23 now records that the encoder shares the
per-constellation tables (a missing entry drops the signal in both
directions with no error) and that the tables must track RTCM amendments.

### Verification

- **Red → green.** New `utest_msm_bds_b2b` encodes one BDS epoch (BDS-3 C21
  with `2I` + `7D`/`7P`/`7Z`, BDS-2 C06 with `2I` + `7I` as controls) as MSM7
  (type 1127) via `gen_rtcm3()` and feeds it byte-by-byte into a second
  `rtcm_t` via `input_rtcm3()`. On the unmodified table exactly the three
  B2b presence checks fail while the `2I`/`7I` controls pass (67-byte
  frame); after the fix all checks pass (98-byte frame; P deltas < 2e-4 m,
  L < 6e-5 m, SNR exact). This is the suite's first MSM encode→decode
  round-trip coverage ([#296](https://github.com/h-shiono/MRTKLIB/issues/296)).
- **Mutation check (wire IDs).** The encoded MSM header signal mask is
  inspected directly: the combined frame must set exactly bits
  {2, 14, 25, 26, 27}, and one-signal frames must each set only their own
  bit. A swapped table entry (`7D` ↔ `7P`) fails here even though the
  symmetric round-trip and the combined-frame ID set would both still pass.
- **Full regression gate:** 122 tests, 121 pass; the single failure is the
  documented environment-only `madocalib_pppar_ion_check` (LAPACK-vs-reference
  ~1.6 cm vs 0.5 cm tolerance, reproduces on clean develop). All RT replays
  passed at full length. No positioning test in the suite decodes BDS MSM
  observations (the only `.rtcm3` positioning input is IGS-RTS SSR), so the
  suite is unchanged by construction; the round-trip test is the coverage.
- Review: Copilot approved with no findings; Greptile's P2 (pin the wire
  IDs independently of the shared table) is the mutation check above.

### Behaviour note (slot placement)

Decoded `7D` takes the B2 slot (index 2), not an extended slot: `sigindex()`
applies the per-band code priority (`7D > 7I > 7Q > 7X`, `src/data/mrtk_obs.c`)
across the signal list of each MSM message. In a message that also carries
BDS-2 `7I` (typical mosaic-X5 base), `7I` moves to an extended slot (`7P` /
`7Z` go to extended slots). This is the same placement the RINEX reader's
`set_index()` already produces for `C7D` / `C7I` from the same receiver, so
RTCM input now matches the RINEX path; it is also upstream RTKLIB's
per-message semantics. Override lever: RTCM option `-CL7I` (`getcodepri()`);
sigcfg does not govern RTCM3 observations yet.

### Related issues

- [#333](https://github.com/h-shiono/MRTKLIB/issues/333) — fixed.
- [#296](https://github.com/h-shiono/MRTKLIB/issues/296) — partially
  addressed (first MSM round-trip test; broader RTCM3 coverage remains open).
- [#189](https://github.com/h-shiono/MRTKLIB/issues/189) /
  [#234](https://github.com/h-shiono/MRTKLIB/issues/234) — follow-ups:
  positioning use of B2b and sigcfg control of RTCM3 observation slots.

### Upgrade notes

None — table-only change: no struct layout change, no configuration change.
