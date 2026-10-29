# Release Notes — v0.7.12

## QZSS L6 service routing

**Release date:** 2026-10-29
**Type:** Feature + bug-fix release — QZSS L6 decoding/routing changes (behaviour change for SBF QZS5, UBX MADOCA iono and `format = "l6e"` in CLAS mode); **`mrtk l6extract` output names change**; **public API change** in the stream byte counters
**Branch:** `release/v0.7.12`

---

### Overview

QZSS broadcasts several services on L6 — CLAS (L6D), MADOCA-PPP clock/orbit
(L6E), MADOCA-PPP ionosphere (L6D, PRN 200/201) and QZNMA (L6E) — and the
mapping of services to PRNs has changed over time. MRTKLIB used to decide the
service from the PRN or from the receiver container (SBF block, `Source`
field, UBX message type), with six slightly different copies of the header
decoding. v0.7.12 classifies every L6 frame by **its own header** — vendor ID
and MADOCA correction service ID (IS-QZSS-L6-008, IS-QZSS-MDC-004,
IS-QZSS-SAS-001) — on every input path, and adds the first regression tests
that replay real QZSS L6 satellite handovers.

This is the first release of the claslib 0.9.0 / CLAS CMA work (epic
[#356](https://github.com/h-shiono/MRTKLIB/issues/356)): the CMA itself
(authentication) follows in v0.8.x; v0.7.12 delivers the routing it needs
and the independent upstream fixes.

### QZSS L6 routing ([#351](https://github.com/h-shiono/MRTKLIB/issues/351))

- **Header parser** `include/mrtklib/mrtk_l6.h` / `src/data/mrtk_l6.c`:
  `l6_parse_header()`, `l6_is_null()`, `l6_file_type()`,
  `l6_file_service()`, and an auxiliary PRN table used only for the
  ionosphere region slot and warnings, never for classification
  ([PR #366](https://github.com/h-shiono/MRTKLIB/pull/366), refactor, no
  behaviour change).
- **Receiver routing** ([PR #369](https://github.com/h-shiono/MRTKLIB/pull/369)):

  | Before | After |
  |---|---|
  | SBF treated PRN 197 as MADOCA ionosphere; since IS-QZSS-MDC rev 002 it is **QZS5 CLAS** — its CLAS frames were lost | classified by header → CLAS (also in `mrtk cssr2rtcm3`) |
  | UBX (NEO-D9C) passed MADOCA ionosphere frames (PRN 200/201) into the CLAS demux; with `l6_merge = 0` they could hold the CLAS channel | routed to the ionosphere decoder (code 16) |
  | no vendor check on CLAS inputs | MADOCA / QZNMA / reserved-vendor frames skipped |
  | `format = "l6e"` in CLAS / PPP-RTK mode overwrote CLAS SSR with MADOCA SSR | MADOCA SSR gated in CLAS mode |
  | QZNMA dropped inside the MADOCA decoder | own return code 17 (for CLAS CMA, #353) |

- **Post-processing input** ([PR #370](https://github.com/h-shiono/MRTKLIB/pull/370)):
  `.l6d` (routed by content: CLAS, or MADOCA ionosphere) and `.l6e`
  (MADOCA-PPP L6E; reserved for CLAS CMA in CLAS mode, ignored with a
  warning until v0.8.x). L6 files are no longer handed to the RINEX reader.
  New parity test against upstream claslib 0.9.0 (CMA off): 3D RMS 3.0 cm.
- **`mrtk l6extract`** ([#362](https://github.com/h-shiono/MRTKLIB/issues/362)):
  signal and PRN from the L6 header; **outputs are now
  `{prefix}.{PRN}.l6d` / `{prefix}.{PRN}.l6e`** (official MADOCA L6E naming)
  instead of `{prefix}_J{PRN}_{l6d|l6e}.l6`, readable by `mrtk post`
  directly. Fixes mosaic-CLAS block 4069 written as L6E, L6E PRNs 210/211
  dropped, and UBX L6E named by the satellite PRN. **Update scripts that use
  the old names.**
- **Null messages** ([#344](https://github.com/h-shiono/MRTKLIB/issues/344)):
  CLAS drops L6 null messages ("do not use CLAS", IS-QZSS-L6-008 5.4.4)
  instead of splicing them into the subframe, as upstream claslib 0.9.0 does;
  UBX/SBF L6D null frames are no longer passed to the CLAS demux. Tested with
  synthetic null frames inserted into real CLAS files (no capture contains
  one).
- claslib 0.9.0 guards: RINEX 2 navigation QZSS PRN 93..99
  ([#347](https://github.com/h-shiono/MRTKLIB/issues/347)), CLAS signal-mode
  bound ([#348](https://github.com/h-shiono/MRTKLIB/issues/348)), UBX
  RXM-QZSSL6 debug counter bound ([#364](https://github.com/h-shiono/MRTKLIB/issues/364)).

### QZSS L6 handover regression tests ([#372](https://github.com/h-shiono/MRTKLIB/issues/372))

Five 15-minute windows around real CLAS satellite handovers (DOY 2026-286):
NEO-D9C UBX L6 (two channels, both transmit patterns) with mosaic-G5
observations, and mosaic-CLAS single-channel streams covering both
transmit-pattern changes. All five are post-processed and replayed in real
time with `mrtk run`; the mosaic-CLAS pattern 1 → 2 case is replayed with
`l6_merge = 0` and `1` (six replays). The RT replays include the
single-channel relock path (`l6_merge = 0`), which had no coverage before.

- **RT replay** (6 tests) and **post-processing** (5 tests, run in CI) are
  gated on behaviour, not RMS: no solution freeze (#205 / #218), fix before
  the handover, fix held again within a per-case limit, bounded non-fixed
  epochs, satellite count.
- **Absolute accuracy** of the post-processed windows against the antenna
  reference point (24 h static PPP-AR with CODE MGEX finals; JAXA finals agree
  within 1.5 cm): 3D 1σ 4–14 cm, gate 0.25 m.
- **Finding:** post-processing holds fix through the handovers where the RT
  replays lose it for ~30–80 s, starting ~30 s after the switch. Tracked in
  [#389](https://github.com/h-shiono/MRTKLIB/issues/389) (RT-only fix loss)
  and [#390](https://github.com/h-shiono/MRTKLIB/issues/390) (`l6_merge = 1`
  worse than `0` after a single-channel pattern change). The tests gate the
  current behaviour and will be tightened when these are fixed.

### Test datasets as release assets ([#386](https://github.com/h-shiono/MRTKLIB/issues/386))

Large test datasets are no longer committed here. They are immutable release
assets of [h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data),
pinned by SHA256 in `tests/data/datasets.cmake`, fetched by the ctest
fixtures on first use and cached in `~/.cache/mrtklib-data`
(`-DMRTK_TEST_DATA_CACHE`; offline: `-DMRTK_TEST_DATA_DIR`; prefetch:
`cmake --build build --target fetch-test-data`). Configure and build stay
offline. First datasets: `has-v1` (Galileo HAS, moved from the repository)
and `handover-v1`. The remaining in-tree datasets move in follow-up PRs, one dataset each.

### Streams and configuration

- **64-bit stream byte counters — public API change**
  ([#368](https://github.com/h-shiono/MRTKLIB/issues/368)). The
  `mrtk relay` status went negative after 2 GiB.
  `stream_t.inb/outb/inbt/outbt` are `uint64_t`, and
  `strsum(stream_t*, uint64_t* inb, int* inr, uint64_t* outb, int* outr)`,
  `strsvrstat(strsvr_t*, int*, int*, uint64_t* byte, int* bps, char*)`.
  **Code calling these functions must update its variables.** The `mrtk run`
  console `stream` command reads the counters under the stream lock.
- `SO_REUSEADDR` now reaches the library's server sockets: a restarted
  `mrtk relay` / `mrtk run` rebinds a `tcpsvr` port with a client still
  connected ([#367](https://github.com/h-shiono/MRTKLIB/issues/367)).
- Serial writes return the bytes written, and a full output buffer no longer
  marks the port as failed ([#343](https://github.com/h-shiono/MRTKLIB/issues/343)).
- `mrtk run` console `save <file>.toml` reports an error instead of saving a
  file without the positioning options (interim fix,
  [#342](https://github.com/h-shiono/MRTKLIB/issues/342); full TOML save in
  v0.7.13 — use a `.conf` file meanwhile).

### Upgrade notes

- `mrtk l6extract` output names changed (see above).
- External code using `strsum()` / `strsvrstat()` must pass `uint64_t*` for
  the byte counts.
- Running ctest needs network access once (or `-DMRTK_TEST_DATA_DIR`) for
  the datasets now hosted as release assets.
- `.l6e` inputs to `mrtk post` in CLAS mode are ignored with a warning (they
  are reserved for CLAS CMA).

### Test status

Full suite `ctest -j1`: **170 / 170** (macOS, Accelerate). RT replay tests are
timing-sensitive and can false-fail under CPU contention (for example with
another `mrtk run` on the same machine); re-run them in isolation.

### Next

v0.7.13: BeiDou PPP-B2b; full TOML save for `mrtk run` (#342); post-processing
of several PRN files per CLAS channel (#373). v0.8.x: CLAS CMA authentication.
