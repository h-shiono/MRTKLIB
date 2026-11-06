# Release Notes — v0.7.15

## CLAS real-time robustness, part 2: phase-bias discontinuities, L6 channel management and partial-AR integrity

**Release date:** 2026-11-06
**Type:** Bug-fix release (real-time CLAS PPP-RTK, MADOCA-PPP, `mrtk cssr2rtcm3`).
**Breaking CLI change** in `mrtk cssr2rtcm3` (SBF input only). **ABI / API
changes** in the CLAS and MADOCA structures.
**Branch:** `release/v0.7.15`

---

### Overview

v0.7.14 fixed the CLAS decoder's handling of lost L6 frames. The same
5.9 days of live mosaic-G5 data (DOY 2026-296 to 301, network 7) and a
4.9-day capture of the same receiver (2026-10-23 to 10-28) showed the next
layer of real-time faults: phase-bias steps, L6 channel selection, the
two-channel merge, and the converter. v0.7.15 fixes them:

| Fault | Symptom | Issue |
|:------|:--------|:------|
| CSSR phase discontinuity indicator ignored | every phase residual rejected at a CLAS bias step → Single, ~2.9 times a day | [#393](https://github.com/h-shiono/MRTKLIB/issues/393) |
| `l6_merge`, channel resumes across a week rollover | clocks dated a week ahead, Single until restart | [#436](https://github.com/h-shiono/MRTKLIB/issues/436) |
| `l6_merge`, silent channel counted as a source | ~17 s Single at an input gap where `l6_merge = 0` keeps Fix | [#443](https://github.com/h-shiono/MRTKLIB/issues/443) |
| Single-channel lock kept on a degraded PRN | corrections starve, Single, while other CLAS PRNs are clean | [#414](https://github.com/h-shiono/MRTKLIB/issues/414) |
| Partial AR re-enables a frequency invalid before the trial | per-satellite valid flags wrong for the rest of the epoch | [#413](https://github.com/h-shiono/MRTKLIB/issues/413) |
| VRS-RTK partial AR with nothing to exclude | write to `rtk->ssat[-1]` | [#412](https://github.com/h-shiono/MRTKLIB/issues/412) |
| MADOCA-PPP L6E / L6D-iono frame lost inside a message | message decoded from shifted bits → metre-level excursions | [#363](https://github.com/h-shiono/MRTKLIB/issues/363) |
| `mrtk cssr2rtcm3` after a lost correction set / input gap | 13–33 s outages; output 2–10 s behind the receiver, then 10–19 s gaps | [#442](https://github.com/h-shiono/MRTKLIB/issues/442) |

claslib post-processing is byte-identical except two cases that move by
less than 0.1 mm (#393); VRS-RTK, `ssr2obs` and `ssr2osr` are byte-identical,
and MADOCA post-processing is byte-identical.

The reset loop of [#381](https://github.com/h-shiono/MRTKLIB/issues/381) is
gone without a change of its own: re-measured on 101 windows of the live
capture, 153 live loops replay as 0 on this code (47 of 50 still reproduce on
v0.7.13). It was a consequence of the shifted-bit decodes fixed in v0.7.14.

### CSSR phase discontinuity indicator ([#393](https://github.com/h-shiono/MRTKLIB/issues/393))

CLAS advances a 2-bit indicator of a signal in ST5 / ST6 when its phase bias
steps. The decoder read it, but the bias bank, `clas_corr_t` and
`clas_update_global()` dropped it (upstream claslib ignores it too), so the
filter kept ambiguities estimated against the old bias. When CLAS stepped the
network phase biases of every satellite at once, all phase residuals
(0.5–1.5 m) were rejected and the epoch fell to Single. In the 5.9-day
capture this happened about 2.9 times a day and was the leading cause of
Single epochs.

The network (ST6) and global (ST5) indicators now travel with the phase bias
to `nav->ssr_ch[].discnt`. `ppp_rtk_pos()` compares them per L6 channel,
satellite and frequency, and re-initializes the phase-bias state of each
signal whose indicator changed. On the two issue captures the Single epochs
are gone and the solution stays Float through both bias steps.

`mrtk cssr2rtcm3` does not use the indicator yet (see *Known / open*).

### Two-channel merge (`l6_merge = 1`)

- **Week rollover ([#436](https://github.com/h-shiono/MRTKLIB/issues/436)).**
  The CSSR week state (`week_ref`, `tow_ref`) was shared by both channels,
  whereas upstream keeps it per L6 stream. A channel that was silent across
  the rollover resumed with the previous week, and the shared week then moved
  one week ahead. Every clock was newer than the observation time, so the
  output stayed Single. The state is now per channel, in `clas_l6buf_t`.
  5.9-day RT replay: fix rate 38.85 % → 98.95 % (98.87 % without merge).
- **Silent channel ([#443](https://github.com/h-shiono/MRTKLIB/issues/443)).**
  When the only PRN of one transmit pattern set, its channel still passed the
  bank lookup, so the BackupCSSR bridge was not used when the live channel
  lost a subframe. A channel whose locked PRN has sent no frame for more than
  `CLAS_L6_RELOCK_TIMEOUT` is now skipped. 4.9-day replay: Fix→Single events
  875 → 860 (`l6_merge = 0`: 872); all 16 events that only `l6_merge = 1`
  had are gone.
- **BackupCSSR restore with one unusable channel.** When neither channel
  supplies corrections and only one has a usable backup (the other's facility
  changed, or its backup is older than 180 s), the other channel kept the
  set of an earlier epoch, which was then applied. It is now cleared, as a
  channel that fails a normal fetch already is. Merge only. The 4.9-day
  replay is unchanged by this fix (see Release validation).

### Degraded single-channel lock ([#414](https://github.com/h-shiono/MRTKLIB/issues/414))

A channel released its PRN lock only after 10 s of silence. A PRN degrading
at low elevation still sends some frames, so the lock was kept while no
subframe completed. The demux now tracks subframe completeness per CLAS PRN
from the frame time tags. When the locked PRN has delivered no complete
subframe for more than 10 s and another PRN has delivered two in a row, the
channel moves to that PRN at the start of its next subframe. On the issue
window (2026-10-26 20:47 UTC) 47 Single epochs are gone; over nine more
forced-lock windows Single epochs 1022 → 611 and misfixes 247 → 168, none
worse. The full 4.9-day replay keeps its fix rate (93.74 % → 93.78 %).

### Partial-AR integrity ([#413](https://github.com/h-shiono/MRTKLIB/issues/413), [#412](https://github.com/h-shiono/MRTKLIB/issues/412))

Both CLAS engines try excluding one satellite at a time. After a failed trial
they set every frequency valid, including one that was invalid before; the
pre-trial `vsat[]` is now restored. In the VRS-RTK engine, when no trial gave
a ratio, the loop cleared `vsat[]` of satellite index −1, writing past
`ambc[]` in `rtk_t`; it now stops. Upstream claslib 0.9.0 has both
behaviours. All claslib PPP-RTK / VRS NMEA outputs and the six PPC CLAS runs
are byte-identical; only the per-satellite valid flags in `.stat` change.

### MADOCA-PPP frame continuity ([#363](https://github.com/h-shiono/MRTKLIB/issues/363))

The MADOCA side of the v0.7.14 CLAS fix. The L6E and L6D-iono decoders now
receive the receiver frame time (SBF / UBX); a frame that continues a message
but does not follow the previous one by 1 s discards the message until the
next subframe indicator. On 5.9 days of SBF this happened 158–359 times per
L6E PRN and 53 times on L6D PRN 200. RT replay of four 2-hour windows
(`qzs-madoca-ppp`, PPP-AR + L6D iono): maximum horizontal / vertical error
2.34 / 4.41 m → 0.09 / 0.20 m; H95 better in 3 of 4 windows. `.l6e` / `.l6d`
files carry no frame time and decode as before.

### `mrtk cssr2rtcm3` ([#442](https://github.com/h-shiono/MRTKLIB/issues/442))

- **BackupCSSR bridge in `clas_ssr2osr()`.** An epoch without a complete
  correction set (for example after the #363 check discarded a subframe) now
  uses the last good set for up to 180 s, as upstream `cssr2osr.c` and the
  PPP-RTK engine do. This also covers `mrtk ssr2obs` / `ssr2osr`. A backup is
  no longer restored across an L6 facility change (PPP-RTK too). 4.9-day
  replay: output availability 92.30 % → 95.82 %.
- **Output epoch = latest receiver PVT.** The converter output one epoch per
  read, one interval after the last, so after an input gap it produced past
  epochs and then jumped ≥ 10 s. The epoch is now the latest valid PVT time on
  the `-t` grid. 4.9-day replay: 10–19 s gaps 1276 → 132; RT age of
  differential mean ~6 s → 1.1 s.
- **SBF input only (breaking).** The raw L6 path (`-in` without `sbf://`,
  `-2ch`, `-pos`) had emitted no RTCM3 since v0.6.5, because output is driven
  by the SBF PVT block, and a raw L6 stream carries no frame time for the #363
  check. These options now exit with an error that points to `sbf://`.
- The degraded-PRN selection of #414 was checked on the converter's own
  `l6d_select_best()`: the degraded PRN was selected in 0 of 30 windows, so
  no change was needed.

### NMEA GGA station id names the CLAS L6 source

In PPP-RTK mode, GGA field 14 (until now `0000`) carries `P1*100 + P2*10 + F`:
`P1` / `P2` = L6 PRN − 192 of channel 1 / 2 (0 if the channel carries no
corrections), `F` = facility id + 1, or 9 when the merged channels carry
different facilities. A QZSS handover or a facility switch is then visible in
the NMEA alone. On by default; `[output] clas_nmea_source_id = false` writes
`0000`. VRS-RTK is not covered.

### Tests

- **RT replay tests run concurrently** ([#448](https://github.com/h-shiono/MRTKLIB/issues/448)).
  Each `mrtk run` replay now has its own working directory: `mrtk run` reads
  and rewrites `rtkrcv.nav` in its working directory, so replays sharing the
  source tree inherited the previous test's broadcast ephemerides. The
  serialising resource lock is removed. Full suite on 8 cores: 2281 s →
  ~480 s at `-j16`. `-j24` oversubscribes an 8-core host. CI is unchanged.
- New tests: `utest_madoca_l6gap` (MADOCA frame-gap discard, on the
  existing `l6route_*.sbf` fixtures), `utest_clas_refid` (GGA station id),
  `utest_cssr2rtcm3_sched` (converter epoch scheduler) and
  `cssr2rtcm3_clas_l6gap` with a `-t 5` variant (converter on
  `clas_l6gap-v1`). Extended: `utest_clas_l6` (per-channel week reference,
  discontinuity indicator decode), `utest_clas_bank` (indicator delivery; BackupCSSR restore with one unusable merge channel),
  `utest_t_clas_route` (degraded-PRN relock, silent merge channel). The
  partial-AR fixes (#412 / #413) have no dedicated test: every regression
  output they touch is byte-identical. No new dataset.

### Upgrade notes

- **`mrtk cssr2rtcm3`:** use `-in sbf://<URI>`. `-2ch` and `-pos` are
  removed; `-nav` is optional.
- **External drivers must rebuild** (ABI change): `clas_ctx_t`
  (`l6sfok_time[]`, `l6health[]`; `week_ref`, `tow_ref`, `obs_ref` moved out),
  `clas_l6buf_t` (per-channel week state), `clas_bias_bank_t` and
  `clas_corr_t` (`discnt[MAXSAT][MAXCODE]`), `clas_clock_bank_t`, `sol_t`,
  `solopt_t`, `mdcl6d_t` (`ftag`).
- **API changes:** `clas_route_l6frame()` takes the L6 header and the frame
  time tag (`const l6_header_t*`, `double tag`); `decode_qzss_l6frame()`
  gains a `tag` argument. New: `clas_osr_bias_smode()`, `clas_nmea_refid()`,
  `clas_set_week_ref()`, `clas_week_ref_isset()`, `set_mcssr_ftag()`.
- **NMEA:** GGA field 14 is no longer `0000` in CLAS PPP-RTK mode (see
  above).

### Release validation

The release build was replayed on the full 4.9-day kubolab capture
(2026-10-23 to 10-28, RT ×10, first hour excluded, 416 004 epochs):

| Build | `l6_merge` | Fix | Single | Fix→Single | Fix, horizontal > 0.3 m |
|-------|-----------:|----:|-------:|-----------:|------------------------:|
| develop before #451 | 0 | 93.78 % | 7254 | 872 | 1835 |
| v0.7.15 | 0 | 93.78 % | 7254 | 872 | 1835 |
| develop before #451 | 1 | 93.80 % | 7123 | 875 | 2511 |
| after #451 | 1 | 93.90 % | 6900 | 860 | 2511 |
| v0.7.15 | 1 | 93.90 % | 6900 | 860 | 2511 |

The changes merged after #451 (#452 and the BackupCSSR restore fix) leave
these counts unchanged. Positions differ by at most 5.8 cm at some epochs,
with no epoch changing its solution quality, the same spread as two runs of
one build (RT replay is not bit-reproducible).

### Test status

Full suite: **196 / 196** (macOS, Accelerate, `-j16`, 428 s). RT replay tests are
timing-sensitive and can false-fail under CPU contention; re-run them in
isolation.

### Known / open

- `mrtk cssr2rtcm3` does not yet signal a CSSR phase-bias discontinuity to
  the external RTK engine (MSM lock-time reset via LLI)
  ([#453](https://github.com/h-shiono/MRTKLIB/issues/453), split from #442).
- With `l6_merge = 1`, Fix epochs with a horizontal error > 0.3 m are more
  frequent than with `l6_merge = 0` on the 4.9-day replay (2511 vs 1835),
  tracked in [#394](https://github.com/h-shiono/MRTKLIB/issues/394).

### Next

- **v0.7.16:** BeiDou PPP-B2b.
- **v0.7.17:** SouthPAN PVS.
- **v0.8.x:** CLAS CMA authentication.
