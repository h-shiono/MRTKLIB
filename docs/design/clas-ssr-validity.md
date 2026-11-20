# CLAS SSR orbit / clock validity

**Status: decided, no code change (#416, v0.7.18).**
MRTKLIB keeps a 60 s orbit and a 60 s clock age limit for CLAS, and it keeps the
upstream rule that pairs each clock with the orbit of the same 30 s set. This page
records how these values compare with IS-QZSS-L6 and with upstream claslib, the
measurements behind the decision, and when to revisit it.

---

## 1. Three sets of limits

| | Orbit (ST2) | Clock (ST3) | Code / phase bias, STEC, grid |
|---|---|---|---|
| IS-QZSS-L6-008, Table 4.1.1-3 (nominal validity) | 60 s | 10 s | 60 s |
| upstream claslib 0.8.2 / 0.9.0 (`MAXAGESSR_ORB` / `_CLK` / `_BIAS` / `_TROP` / `_IONO`) | 240 s | 120 s | 120 s |
| MRTKLIB | **60 s** | **60 s** | 120 s (as upstream) |

- The IS counts validity from the message epoch (GPS Epoch Time 1s / GNSS Hourly
  Epoch Time 1s). It states that the quality of outdated data is not guaranteed.
- MRTKLIB applies its limits in `satpos_ssr()` (`src/data/mrtk_eph.c`, `MAXAGESSR`).
  That constant is shared with MADOCA, HAS and RTCM SSR. The SIS-discontinuity
  tracker in `clas_osr_satcorr_update()` (`src/clas/mrtk_clas_osr.c`,
  `MAXAGESSR_SIS`) applies the same 60 / 60 s limits.
- The bias, STEC and grid limits are outside the scope of this page (§6).

## 2. How a correction set is chosen

`sub_get_close_cssr()` (`src/clas/mrtk_clas.c`) follows upstream `cssr.c`. The
orbit epoch anchors the whole set:

| Correction | Taken from the bank |
|---|---|
| Orbit | the newest set at or before the observation time, up to 180 s old |
| Clock | the newest set at or before the observation time that is **less than 30 s newer than the orbit** |
| Troposphere / STEC | within ±30 s of the orbit epoch |
| Phase bias / code bias | phase bias at or before the orbit epoch; code bias per #406 |

- The pairing rule is one-sided. `get_close_clock()` rejects a clock that is 30 s
  or more newer than the orbit, but it sets no lower bound. A clock newer than its
  orbit set is therefore never used. If the ST3s that follow a decoded ST2 are lost,
  however, a clock of the previous set, older than the orbit, can be selected.
- ST3 carries no IODE (only ST2 and ST11 do). Its C0 refers to the broadcast
  ephemeris that the ST2 of its own set names. The rule thus prevents a newer clock
  being combined with an older orbit. It does not prevent the reverse, which is
  inconsistent if the IODE changed between the two sets.
- In the data measured for this page (§4), a clock older than its orbit was never
  selected: 0 of about 89 000 bank lookups (claslib and handover post-processing,
  `rtkrcv_clas_l6gap_*`, `rtkrcv_handover_*`, the two live hours), and no
  satellite-epoch with a clock age above the orbit age. This is an observation on that data, not a property of the
  code.
- When the bank lookup fails, BackupCSSR keeps the previous snapshot, for up to
  180 s, and refreshes its clock under the same 30 s rule.
- The orbit age limit does not bound the clock age, so the separate 60 s clock
  limit in `satpos_ssr()` is not redundant.

### When one ST2 is lost

This is the #392 event: a 4 s stream gap lost the top-of-hour subframe.

1. Clocks newer than orbit + 30 s are not used. The engine keeps the last clock of
   the old set, which reaches an age of about 36 s.
2. Once the orbit is older than 60 s, the epoch is Single until the next ST2 is
   decoded, which takes 1–2 epochs.
3. Fix resumes as soon as that ST2 arrives.

## 3. What error the limits accept

Upstream gives no rationale, the claslib sources carry no history, and the manual
does not mention these limits. They cannot be traced to a document, so the
question becomes: how large an error does each age limit let through?

Method:
- Use clean one-hour captures, 2019/239 (`2019239Q.l6`) and 2025/157
  (`2025157U_ch1.l6`).
- Dump ST2 / ST3 with `mrtk dump --parse`.
- For each satellite and each lag τ, take the change of the correction over τ, with
  the IODE the same at both ends. This change is the error of using a correction
  τ seconds old.
- For the clock, remove the per-epoch mean over all satellites first, because the
  receiver clock absorbs that common part.

| τ | Clock ΔC0 RMS / p95, 2019 | Clock ΔC0 RMS / p95, 2025 | Orbit radial RMS, 2019 / 2025 |
|---|---|---|---|
| 5 s | 1.8 / 2.0 cm | 1.6 / 1.5 cm | — |
| 10 s (IS clock) | 2.5 / 3.2 cm | 2.3 / 2.3 cm | — |
| 30 s | 4.5 / 6.4 cm | 4.1 / 4.8 cm | 2.5 / 0.2 cm |
| **60 s (MRTKLIB)** | **5.7 / 9.7 cm** | **4.7 / 6.5 cm** | 3.7 / 0.2 cm |
| 120 s (claslib clock) | 7.0 / 13.2 cm | 5.8 / 8.9 cm | 4.4 / 0.3 cm |
| 240 s (claslib orbit) | 8.8 / 17.1 cm | 7.8 / 14.4 cm | 5.7 / 0.6 cm |

Findings:
- The error grows smoothly, roughly as √τ. There is no knee at 120 s or 240 s, so
  the data does not single out the upstream values.
- At its limits, upstream accepts a clock error of 6–7 cm RMS (p95 9–13 cm) and an
  orbit radial error of up to about 6 cm RMS.
- The upstream bank searches orbits up to 180 s old, and BackupCSSR bridges outages
  of up to 180 s. 240 / 120 s therefore reads as an outer safety net above that
  180 s continuity design, chosen for continuity rather than derived from the IS
  validity or from an error budget. This is an inference; upstream does not state
  it.

## 4. Decision

- **Orbit: 60 s.** This is the IS validity.
- **Clock: 60 s.** The clock age limit is not the IS value. For CLAS, MRTKLIB cuts
  off at a stale-clock error of about **5 cm RMS** (4.7–5.7 cm at 60 s in the table
  above). That is about half of the upstream 120 s and about twice the IS 10 s.
- **Pairing: unchanged.** A clock is never used with an orbit older than its own
  30 s set. A clock older than the orbit remains possible (§2), and the 60 s clock
  limit bounds its age.

### Alternatives not taken

All of them were measured with a probe at the bank lookup and at the
`satpos_ssr()` gate, with no behaviour change. The data covers the claslib and
`post_handover_*` post-processing cases, the RT replay tests, and two live hours
of the 2026-10-27 mosaic-G5 capture (hours t and u). It does not include a
multi-day replay.

| Option | Why not |
|---|---|
| IS clock validity (10 s), pairing kept | On clean data the clock age at use is at most 4 s, so nothing changes there. On lossy live streams (`rtkrcv_clas_l6gap_*`, the two live hours, the `mcla` handover windows), 3–10 % of epochs use a clock older than 10 s. Those epochs would lose satellites or drop to Single. |
| IS validity plus pairing a new clock with the last valid orbit | The pairing explains only about a quarter to a third of the epochs with a clock older than 10 s. The rest come from lost clocks and BackupCSSR, so the regression of the row above largely remains. A new clock with an old orbit is also only consistent while the IODE is unchanged. CLAS changes the IODE of 0.6–3.6 % of satellites per 30 s set (typically 1.3 %), so a lost ST2 hides at least one IODE change with a probability of about 20 % for 20 satellites. The resulting SIS error would show up as misfix, not as Single. The relaxed pairing would also have to cover troposphere, STEC and biases, because the orbit epoch anchors them too. |
| upstream limits (240 / 120 s) | Accepts clocks up to 120 s old, with about 6–7 cm RMS error, further from the IS validity. It would gain only the 1–2 epochs of real orbit expiry at a lost ST2. |

### Consequences

- On clean data the outputs are those of upstream claslib, because no correction
  comes near any of the limits. Orbit ages stay at or below 29 s and clock ages at
  or below 4 s.
- On lossy streams MRTKLIB drops to Single earlier than claslib, once the orbit is
  older than 60 s. It also uses clocks older than the IS 10 s, up to about 36 s
  after a lost ST2 and up to about 53 s in the worst observed case.
- The claslib post-processing references cannot detect a change of these limits:
  they were generated with 240 / 120 s and also pass at 60 / 60 s. Any future
  change needs its own lossy-stream gate.

## 5. Known limits of this analysis

- §3 measures how much the correction changes, not the positioning error. The
  accuracy and misfix rate of epochs with a clock between 10 s and 60 s old have
  not been measured.
- Two clean hours and about three lossy hours were analysed. No multi-day replay
  was available at the time.
- The RT replay tests that read BINEX and L6 from separate files
  (`rtkrcv_rt_clas*`) replayed the L6 stream 18–20 s late when this was measured
  (#515), so their clock ages were excluded from the conclusions above. With the tags
  rebuilt from the L6 content, the clock age at use in those replays is 1–5 s (median
  3 s; post-processing 2 s, live single-SBF replays about 4 s). It was 22 s median on 2019/239 and 11 s on
  2025/157 before. Single-stream SBF replays and live data were not affected.

## 6. Out of scope

- The 120 s limits for code / phase bias (`CLAS_MAXAGESSR_BIAS`) and for
  troposphere / STEC (`MAXAGESSR_TROP` / `MAXAGESSR_IONO` in
  `src/clas/mrtk_clas_grid.c`) exceed the IS 60 s, as in upstream. They are not
  part of this decision.
- `MAXAGESSR` is shared by all SSR sources, and CLAS has no SSR vendor tag of its
  own (vendor 0 is also MADOCA-PPP). A CLAS-only limit would need one first.

## 7. When to revisit

- The accuracy of stale-clock epochs (§5) shows a degradation, or a lower misfix
  rate with a tighter limit.
- IS-QZSS-L6 changes the CLAS transmission intervals or the validity periods.
