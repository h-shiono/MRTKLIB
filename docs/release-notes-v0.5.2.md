# MRTKLIB v0.5.2 Release Notes

**Release date:** 2026-03-10
**Type:** Code quality (style enforcement)
**Branch:** `chore/code-quality-braces-ternary`

---

## Overview

v0.5.2 is a code-quality release that enforces two style rules across the entire
codebase (67 source files, +12,727 / -5,085 lines):

1. **Mandatory braces** on all control flow statements (`if`, `for`, `while`, `else`)
2. **No nested or complex ternary operators**

No functional or algorithmic changes.  All 56/56 non-realtime tests pass with
identical output.

---

## Changes

### Style — Mandatory braces (4,053 instances)

All single-statement `if`, `for`, `while`, and `else` blocks now use explicit
`{}` braces, enforced by Clang-Tidy `readability-braces-around-statements`.

Before:
```c
if (!ssat->vs) continue;
for (j=0;j<3;j++) rs[j+i*6]=0.0;
```

After:
```c
if (!ssat->vs) {
    continue;
}
for (j = 0; j < 3; j++) {
    rs[j + i * 6] = 0.0;
}
```

### Style — Nested ternary elimination (19 instances)

All nested ternary operators (`a ? b ? c : d : e`) converted to `if`-`else`
chains or `switch`-`case` statements.

Before:
```c
sys = (U1(p)==1)?SYS_GLO:((U1(p)==2)?SYS_GPS:((U1(p)==4)?SYS_SBS:SYS_NONE));
```

After:
```c
if (U1(p) == 1) {
    sys = SYS_GLO;
} else if (U1(p) == 2) {
    sys = SYS_GPS;
} else if (U1(p) == 4) {
    sys = SYS_SBS;
} else {
    sys = SYS_NONE;
}
```

Three `#define NT(opt)` / `NT_RTK(opt)` macros with nested ternaries were
replaced by `static inline` functions in `mrtk_ppp.c`, `mrtk_ppp_ar.c`, and
`mrtk_clas_osr.c`.

### Style — Complex ternary cleanup (8 instances)

Ternary operators with side-effect assignments in conditions or function calls
in both branches were refactored to explicit `if`-`else`:

- `strchr()` assignments extracted from ternary conditions (`mrtk_obs.c`,
  `mrtk_rinex.c`)
- `getbitu()` conditional calls converted to `if`-`else` (`mrtk_rcv_septentrio.c`)
- `LLI` arithmetic ternaries expanded to explicit `if` blocks
  (`mrtk_rcv_septentrio.c`)
- `strgettime()` conditional calls converted to `if`-`else` (`mrtk_rtksvr.c`)
- NTRIP function-dispatch ternaries converted to `if`-`else` (`mrtk_stream.c`)
- Nested `adr` ternary with `I8()` call converted to `if`-`else`
  (`mrtk_rcv_ublox.c`)

### Formatting

Changed lines formatted with `git clang-format` (Google style, 4-space indent,
120-column limit) to ensure consistent style on all modified code.

---

## Scope

| Category | Count |
|----------|-------|
| Files changed | 67 |
| Lines added | 12,727 |
| Lines removed | 5,085 |
| Brace fixes | 4,053 |
| Nested ternary refactors | 19 |
| Complex ternary refactors | 8 |

### Excluded from scope

- `src/core/tomlc99/` — Vendored third-party code (tomlc99, MIT license)
- `apps/cssr2rtcm3/` — Unreleased tool, not yet on `develop`
- Header files (`include/mrtklib/`) — Not processed (no control-flow logic)

---

## Tooling

| Tool | Version | Purpose |
|------|---------|---------|
| Clang-Tidy | 21.1.6 (Homebrew LLVM) | `readability-braces-around-statements` auto-fix |
| clang-format | 21.1.6 | `git clang-format` on changed lines only |
| `.clang-format` | Google / 4-space / 120-col | Project formatting rules |

---

## Test Results

All 56/56 non-realtime tests pass, unchanged from v0.5.1.

| Test Suite | Count | Status |
|------------|-------|--------|
| Unit tests | 12 | PASS |
| SPP / receiver bias | 4 | PASS |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 | PASS |
| CLAS PPP-RTK + VRS-RTK | 19 | PASS |
| ssr2obs / ssr2osr / BINEX | 4 | PASS |
| Tier 2 absolute accuracy | 2 | PASS |
| Tier 3 position scatter | 2 | PASS |
| Fixtures | 3 | PASS |

---

## Files changed

All 67 files under `src/` and `apps/` (excluding `src/core/tomlc99/` and
`apps/cssr2rtcm3/`):

| Directory | Files |
|-----------|-------|
| `apps/` | dumpcssr.c, recvbias.c, rnx2rtkp.c, rtkrcv.c, vt.c, ssr2obs.c, ssr2osr.c |
| `src/clas/` | mrtk_clas.c, mrtk_clas_grid.c, mrtk_clas_isb.c, mrtk_clas_osr.c |
| `src/core/` | mrtk_bits.c, mrtk_context.c, mrtk_coords.c, mrtk_mat.c, mrtk_sys.c, mrtk_time.c, mrtk_toml.c, mrtk_trace.c |
| `src/data/` | mrtk_bias_sinex.c, mrtk_eph.c, mrtk_fcb.c, mrtk_ionex.c, mrtk_nav.c, mrtk_obs.c, mrtk_peph.c, mrtk_rcvraw.c, mrtk_rinex.c, mrtk_sbas.c |
| `src/data/rcv/` | mrtk_rcv_binex.c, mrtk_rcv_crescent.c, mrtk_rcv_javad.c, mrtk_rcv_novatel.c, mrtk_rcv_nvs.c, mrtk_rcv_rt17.c, mrtk_rcv_septentrio.c, mrtk_rcv_skytraq.c, mrtk_rcv_ss2.c, mrtk_rcv_ublox.c |
| `src/madoca/` | mrtk_madoca.c, mrtk_madoca_iono.c, mrtk_madoca_local_comb.c, mrtk_madoca_local_corr.c |
| `src/models/` | mrtk_antenna.c, mrtk_astro.c, mrtk_atmos.c, mrtk_geoid.c, mrtk_station.c, mrtk_tides.c |
| `src/pos/` | mrtk_lambda.c, mrtk_options.c, mrtk_postpos.c, mrtk_ppp.c, mrtk_ppp_ar.c, mrtk_ppp_iono.c, mrtk_ppp_rtk.c, mrtk_rtkpos.c, mrtk_sol.c, mrtk_spp.c, mrtk_vrs.c |
| `src/rtcm/` | mrtk_rtcm.c, mrtk_rtcm2.c, mrtk_rtcm3.c, mrtk_rtcm3_local_corr.c, mrtk_rtcm3e.c |
| `src/stream/` | mrtk_rtksvr.c, mrtk_stream.c |
