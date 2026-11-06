# Release Notes — v0.7.6

## TOML configuration redesign, IGS PPP-AR receiver code bias, and CLAS periodic filter reset

**Release date:** 2026-07-12
**Type:** Configuration redesign (backward compatible via deprecation aliases) + PPP features
**Branch:** `release/v0.7.6`

---

### Overview

v0.7.6 re-organizes the TOML configuration vocabulary that MRTKLIB has carried
since the v0.5.0 TOML migration. The change is broad but deliberately
non-disruptive:

- **Old configs keep working.** Every renamed section/key is accepted through a
  **deprecation alias**: the loader applies the value and prints a one-line
  warning pointing at the new location. When both the old and new paths are
  present, the new path wins. The full old → new mapping is reproduced in
  [section 4](#4-deprecated-old-new-migration-table) below and in the
  [configuration guide's migration section](../guide/configuration.md).
- **Unknown keys warn instead of vanishing.** A key the loader does not
  recognize now emits `TOML: unknown key [section].key (ignored)` instead of
  silently falling back to a default — so a typo is visible rather than silently
  ineffective.
- **No results change for shipped/default configurations.** The redesign fixes
  several *option-slot collisions* (two unrelated settings sharing one storage
  slot in the positioning code), but none of these fixes alters the output of
  any shipped `conf/` sample or default configuration.
- **The generated config reference is now CI-gated.**
  `docs/reference/config-options.md` is produced by
  `scripts/docs/gen_config_ref.py`, and a new `config-ref` CI job fails the
  build if the committed reference drifts from a fresh generator run.

Alongside the configuration work, v0.7.6 also ships two positioning features:
**receiver code-bias support** plus an opt-in uncorrected-pseudorange gate for
IGS PPP-AR ([#263](https://github.com/h-shiono/MRTKLIB/issues/263)), and the
**claslib periodic full-filter reset** ported to post-processing PPP-RTK
([#264](https://github.com/h-shiono/MRTKLIB/issues/264)).

This is the largest configuration-facing release to date. If you author or
maintain `conf/` files by hand, read section 4 — every old path still loads, but
re-saving a config always writes the new layout.

---

### 1. Configuration redesign

#### Sections regrouped by consumer

The historical grab-bag `[receiver]` and `[server]` sections are dismantled and
re-grouped by the engine that actually consumes each setting:

- `[positioning.ppp]` — PPP-family options (satellite clock / phase bias, clock
  jump, uncorrected-code gate, `options`, …).
- `[positioning.relative]` — baseline / max-age / time-interpolation options.
- `[positioning.spp]` — single-point-positioning options.
- `[positioning.madoca]` — MADOCA-PPP options.
- `[positioning.clas.*]` — CLAS PPP-RTK / VRS options, including
  `[positioning.clas.ambiguities]`, `[positioning.clas.resilience]`, and
  `[positioning.clas.adaptive_filter]`.
- `[input.*]` — stream/format inputs (`[input.rinex]`, `[input.rtcm]`,
  `[input.sbas]`).

The former `[adaptive_filter]` section is nested as
`[positioning.clas.adaptive_filter]` because it is consumed by the CLAS
PPP-RTK / VRS engines only. All shipped `conf/` samples and docs use the new
layout; old paths continue to load via the aliases in section 4.

#### Other key changes

| Change | Detail |
|---|---|
| `excluded_sats` is a string list | `excluded_sats = ["G05", "+G26"]`; the old single-string form still loads. `+` cannot force-include a satellite that has no ephemeris. |
| `[input.sbas].satellite` | Integer PRN selector for the SBAS satellite. |
| `[positioning.corrections].snr_fixed` | Renamed from `reserved`; an integer dB-Hz value routed to its own option slot. |
| Config-reference metadata audited | Per-option mode applicability corrected (phase windup and Shapiro delay are PPP-family options, etc.). |
| RT CLAS test references relocated | Moved out of the source tree into the build directory so parallel `ctest` invocations no longer collide. |

---

### 2. New PPP features

#### Receiver code bias in IGS PPP-AR ([#263](https://github.com/h-shiono/MRTKLIB/issues/263))

Station-level OSB records in a Bias-SINEX product (matched against the RINEX
marker name) are now applied to pseudoranges in the uncombined PPP-AR engine,
with satellite-specific and system-wide fallback paths. Products without station
records are unaffected.

#### `[positioning.ppp].drop_uncorrected_code` (opt-in, default off)

A port of the MADOCALIB unbias gate: pseudoranges without an applicable code bias
are excluded from the solution instead of entering it uncorrected. Carrier phase
is retained. Off by default, so existing IGS PPP-AR runs are unchanged unless the
option is enabled.

#### `[positioning.clas.resilience].reset_interval` (opt-in, default off) ([#264](https://github.com/h-shiono/MRTKLIB/issues/264))

claslib's periodic full-filter reset, ported into post-processing
`ppp_rtk_pos()` and covered by a trace-checked regression test. Off by default.

---

### 3. Option-slot collision fixes

Several unrelated settings shared a single storage slot in the positioning code;
each now has its own. **None of these changes the results of shipped/default
configurations** — the fixes prevent cross-contamination that only manifested
under non-default combinations.

| Collision | Fix |
|---|---|
| CLAS OSR gated the Shapiro delay on the *phase windup* option instead of `shapiro_delay`. | Now keyed to the correct option. `cssr2rtcm3` / `ssr2obs` explicitly enable it, so converter behavior is unchanged ([PR #267](https://github.com/h-shiono/MRTKLIB/pull/267)). |
| `osr[].relatv` could carry a stale cross-epoch relativity term when the option was off. | Cleared per epoch ([PR #267](https://github.com/h-shiono/MRTKLIB/pull/267)). |
| CLAS/VRS position process noise fell back to the *IFB* process-noise slots. | `[kalman_filter.process_noise].position_h` / `position_v` / `ifb` are now independent ([PR #275](https://github.com/h-shiono/MRTKLIB/pull/275)). |
| PPP day-boundary clock-jump compensation shared a slot with CLAS `iono_compensation`. | Now its own `[positioning.ppp].clock_jump` option (default off, as before) ([PR #278](https://github.com/h-shiono/MRTKLIB/pull/278)). |
| CLAS atmosphere variance terms shared slots with the SPP C/N0 error model (`snr_max` / `snr_error`). | Now dedicated `[kalman_filter.measurement_error].ionosphere` / `troposphere` options; the ionosphere variance is applied in `est-adaptive` mode too ([PR #279](https://github.com/h-shiono/MRTKLIB/pull/279)). |

An additional fix: rtkrcv previously printed every unknown-key warning twice (it
loads the options file once for server options and once for system options). The
unknown-key sweep now runs once per file fingerprint
([PR #290](https://github.com/h-shiono/MRTKLIB/pull/290)).

---

### 4. Deprecated: old → new migration table

The old paths below still load with a one-line warning; they are planned for
removal in a future minor release. Re-saving a config always writes the new
layout.

| Old path | New path |
|---|---|
| `[receiver].phase_shift` / `isb` / `reference_type` | `[positioning.clas.ambiguities]` (same key) |
| `[receiver].iono_correction` | `[positioning.madoca].iono_correction` |
| `[receiver].ignore_chi_error` | `[positioning.spp].ignore_chi_error` |
| `[receiver].ppp_sat_clock_bias` / `ppp_sat_phase_bias` | `[positioning.ppp].satellite_clock_bias` / `satellite_phase_bias` |
| `[receiver].max_bias_dt` | `[positioning.ppp].max_bias_dt` |
| `[receiver].uncorr_bias` | `[positioning.ppp].drop_uncorrected_code` |
| `[receiver].max_age` / `baseline_length` / `baseline_sigma` | `[positioning.relative]` (same key) |
| `[server].max_obs_loss` / `float_count` | `[positioning.clas.resilience]` (same key) |
| `[server].l6_margin` | `[positioning.clas.resilience].l6_merge` |
| `[server].regularly` | `[positioning.clas.resilience].reset_interval` |
| `[server].ppp_option` | `[positioning.ppp].options` |
| `[server].time_interpolation` | `[positioning.relative].time_interpolation` |
| `[server].rinex_option_1` / `rinex_option_2` | `[input.rinex].option_1` / `option_2` |
| `[server].rtcm_option` | `[input.rtcm].options` |
| `[server].sbas_satellite` | `[input.sbas].satellite` |
| `[adaptive_filter].*` | `[positioning.clas.adaptive_filter].*` (same keys) |

---

### 5. Removed dead keys

The keys below had no effect (or, for `position_uncertainty_*`, a wrong one), so
there is no replacement; setting them now triggers the unknown-key warning
([PR #271](https://github.com/h-shiono/MRTKLIB/pull/271),
[#280](https://github.com/h-shiono/MRTKLIB/pull/280),
[#283](https://github.com/h-shiono/MRTKLIB/pull/283)):

| Removed key | Note |
|---|---|
| `[ambiguity_resolution.thresholds].ratio2` / `ratio3` / `ratio4` | Unread |
| `[ambiguity_resolution.hold].gain` | No consumer |
| `[kalman_filter].sync_solution` | Unread |
| `[receiver].bds2_bias`, `[receiver].satellite_mode` | Unread |
| `[files].elevation_mask_file` / `geexe` / `solution_stat` | Unread |
| `[positioning.clas].position_uncertainty_x` / `_y` / `_z` | Miswired: they overwrote the rover ECEF position used by fixed-position modes instead of setting any uncertainty. Fixed-mode rover coordinates remain settable via the antenna-position options. |

---

### 6. Validation

A five-perspective pre-release review (config loader, algorithm safety,
config-sweep, docs, tests) was run against the series. The deprecation aliases
and unknown-key warning are locked in by a new `t_toml` unit test
([#286](https://github.com/h-shiono/MRTKLIB/issues/286),
[PR #288](https://github.com/h-shiono/MRTKLIB/pull/288)–[#290](https://github.com/h-shiono/MRTKLIB/pull/290)),
and the new `[positioning.clas.resilience].reset_interval` path is covered by a
trace-checked regression test.

**Test suite:** 114/115 on the release gate — the only failure is the known
environment-dependent `madocalib_pppar_ion_check` (LAPACK-vs-reference
tolerance), which reproduces on a clean `develop` build. The RT CLAS reference
relocation ([PR #282](https://github.com/h-shiono/MRTKLIB/pull/282)) removes the
parallel-`ctest` port-collision false failures previously seen in `rtkrcv_rt`
tests.

---

### Compatibility & migration

No positioning-algorithm, matrix, physical-constant, or test-tolerance changes.
Existing configurations continue to run: every renamed path loads through a
deprecation alias (with a one-line warning pointing at the new location), and the
option-slot collision fixes leave shipped/default configurations bit-for-bit
unchanged. To migrate, either follow the table in
[section 4](#4-deprecated-old-new-migration-table) or simply re-save each config
— saved configs always use the new layout. See the
[configuration guide's migration section](../guide/configuration.md) for a
worked walkthrough.

### Issues / PRs

- [PR #291](https://github.com/h-shiono/MRTKLIB/pull/291) — release PR for
  v0.7.6.
- Series [PRs #256–#290](https://github.com/h-shiono/MRTKLIB/pull/290) — the TOML
  configuration redesign, config-reference CI gate, IGS PPP-AR receiver code bias
  ([#263](https://github.com/h-shiono/MRTKLIB/issues/263)), and the CLAS periodic
  filter reset ([#264](https://github.com/h-shiono/MRTKLIB/issues/264)).

### References

- [Configuration guide](../guide/configuration.md) — including the migration
  section for the v0.7.6 layout.
- `docs/reference/config-options.md` — the CI-gated generated configuration
  reference.
