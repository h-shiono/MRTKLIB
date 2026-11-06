# Release Notes — v0.7.9

## Memory footprint: lazy allocation of the RTCM local-correction block

**Release date:** 2026-08-25
**Type:** Performance (memory footprint) — no positioning change
**Branch:** `release/v0.7.9`

---

### Overview

A repository memory audit and production measurements from a containerized
CLAS deployment ([#295](https://github.com/h-shiono/MRTKLIB/issues/295))
showed that every `mrtk run` solver process was ~1.2 GB resident at startup —
essentially ~12 copies of `rtcm_t`. The cause: `rtcm_t` embedded the MADOCA
local-correction block `lclblock_t` (~307 MB, 97.6 % of the struct, dominated
by `siteion_t istat[MAXBLK][MAXTRPSTA]`) **by value**. `rtksvr_t` carries
three `rtcm_t`, each stream converter (`strconv_t`) two more — yet the block
is only ever touched by the local-correction feature (RTCM3 message types
2001–2016). Every normal rover/base/PPP/RTK/PPP-RTK run paid the footprint
for a feature it never used.

### Fix

[PR #323](https://github.com/h-shiono/MRTKLIB/pull/323) converts the member
to a lazily heap-allocated pointer:

- `rtcm_lclblk()` allocates the block on first use; the type-2001–2016
  decoders call it on entry.
- Encoders and `block2stat()` treat a missing block exactly like the legacy
  all-zeros state (NULL guard, same return values); `free_rtcm()` releases it.
- `init_rtcm()` intentionally leaves the pointer untouched — the struct must
  be zero-initialized before the first call (all in-tree callers use
  calloc/memset), and an existing allocation is preserved across re-init to
  match the legacy embedded-array semantics.
- 46 mechanical `.`→`->` migrations; outside the inserted guards the code is
  byte-identical (verified by reverse-rewrite diff).
- On the way, a latent bug: `strconvnew()` allocated `strconv_t` with
  `malloc`, so `strconvfree()` freed an uninitialized `lclblk` pointer.
  Now `calloc` (required by the ownership contract).

### Results

Measured with the default preset (NFREQ=5, all constellations, MAXSAT=221):

| | Before | After |
|---|---:|---:|
| `rtcm_t` | 314.4 MB | **7.5 MB** |
| `rtksvr_t` | 971.7 MB | **51.1 MB** |
| `strconv_t` | 635.8 MB | **22.1 MB** |
| `mrtk run` startup peak RSS (RT replay, macOS) | 808 MiB | **84 MiB (−89.5 %)** |

Memory stops being the binding constraint on solver count per host: the
three-CLAS-solver deployment that motivated #295 needed an 8 GB machine;
after the fix the same workload fits in well under 1 GB.

### Validation

- Full regression gate: 121 tests, 120 pass; the single failure is the
  documented environment-only `madocalib_pppar_ion_check` (LAPACK-vs-reference
  ~1.6 cm vs 0.5 cm tolerance, reproduces on clean develop). RT CLAS replays
  pass at their recorded ~371 s in isolation.
- New `utest_lclblk` (30 checks): fresh-struct invariants (NULL block,
  guarded consumers, idempotent allocation, double-free safety) plus a
  type-2001 encode→decode round-trip through `gen_rtcm3()`/`input_rtcm3()` —
  the first direct coverage of the 2001–2016 path.
- Post-processing output equivalence: before/after `.pos` deltas are
  indistinguishable from same-binary rerun noise (macOS Accelerate FP
  nondeterminism); code-level identity is established by the reverse-rewrite
  diff.

### Compatibility

- **External embedders of `rtcm_t` must rebuild** (struct layout change). Any
  out-of-tree code that declares its own `rtcm_t` must zero-initialize it
  before the first `init_rtcm()` call (all in-tree callers already did).
- Behavior note: decoded local-correction blocks no longer survive an
  `rtksvr` stop/start cycle. The legacy code kept the stale array and
  re-stamped stale blocks with the current time on the next `block2stat()` —
  arguably a defect; fresh corrections repopulate the block on arrival.
- Upstream syncs: MALIB still embeds `lclblk` by value, so hunks
  cherry-picked from `rtcm3lcl.c` / `lclcmn.c` / `lclcmbcmn.c` need the
  member-access and allocation-guard adaptation — see
  `docs/dev/pitfalls-public.md` P-13.
