# Configuration Options Reference

TOML configuration options available in MRTKLIB.
Options are grouped by their TOML section.

!!! tip "Auto-generated"
    This page is auto-generated from the internal option mapping table.
    Run `python scripts/docs/gen_config_ref.py > docs/reference/config-options.md` to regenerate.

## Mode Abbreviations

| Abbreviation | Positioning Mode(s) |
|:-------------|:--------------------|
| **All** | All modes |
| **SPP** | Single Point Positioning (`single`) |
| **DGPS** | Differential GPS (`dgps`) |
| **RTK** | `kinematic` · `static` · `movingbase` · `fixed` |
| **PPP** | `ppp-kine` · `ppp-static` · `ppp-fixed` |
| **PPP-RTK** | `ppp-rtk` (CLAS PPP-RTK) |
| **SSR2OSR** | `ssr2osr` · `ssr2osr-fixed` |
| **VRS** | `vrs-rtk` |
| **RT** | Real-time only (`mrtk run` / `rtkrcv`) |
| **PP** | Post-processing only (`mrtk post` / `rnx2rtkp`) |

## Positioning

TOML section: `[positioning]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `mode` | enum | All | Positioning mode selector. `single` · `dgps` · `kinematic` · `static` · `movingbase` · `fixed` · `ppp-kine` · `ppp-static` · `ppp-fixed` · `ppp-rtk` · `ssr2osr` · `ssr2osr-fixed` · `vrs-rtk` |
| `frequency` | enum | All | Number of carrier frequencies to use. CLAS PPP-RTK requires `l1+2` (nf=2). `l1` · `l1+2` · `l1+2+3` · `l1+2+3+4` · `l1+2+3+4+5` |
| `solution_type` | enum | PP | Filter direction for post-processing. `forward` · `backward` · `combined` |
| `elevation_mask` | float | All | Minimum satellite elevation angle (degrees). Satellites below this are excluded. |
| `dynamics` | boolean | All | Enable receiver dynamics model (velocity/acceleration state estimation). |
| `satellite_ephemeris` | enum | All | Satellite ephemeris source. SSR modes (`brdc+ssrapc`, `brdc+ssrcom`) are used for PPP and PPP-RTK. `brdc` · `precise` · `brdc+sbas` · `brdc+ssrapc` · `brdc+ssrcom` |
| `systems` | string[] | All | GNSS constellations to use. Accepts a human-readable string list like `["GPS", "Galileo"]`. |
| `excluded_sats` | string | All | Satellites to exclude. Space-separated PRN list (e.g., `G01 G02`). Prefix `+` to include only. |
| `signals` | string[] | All | Explicit signal code list. Overrides default observation definition when set. e.g. `["G1C", "G2W", "E1C"]` |

### Frequency Index Mapping

The `frequency` option selects how many frequency slots to use (`l1` = 1, `l1+2` = 2, etc.).
Each slot maps to a different signal depending on the constellation:

| | L1 (idx 0) | L2 (idx 1) | L3 (idx 2) | L4 (idx 3) | L5 (idx 4) |
|:---|:-----------|:-----------|:-----------|:-----------|:-----------|
| **GPS** | L1 | L2 | L5 | — | — |
| **GLONASS** | G1 | G2 | G3 | — | — |
| **Galileo** | E1 | E5a | E5b | E6 | E5a+b |
| **QZSS** | L1 | L5 | L2 | L6 | — |
| **BDS** | B1I/B1C/B1A | B3I/B3A | B2I/B2b | B2a | B2a+b |
| **SBAS** | L1 | L5 | — | — | — |
| **NavIC** | L5 | S | — | — | — |

!!! warning "CLAS PPP-RTK: Use `l1+2` (nf=2)"
    With `l1+2`, GPS uses L1+L2 and Galileo uses E1+E5a.
    CLAS does not provide E5b bias corrections. Using `l1+2+3` (nf=3)
    adds the E5b slot without valid bias, causing false cycle slips on
    Galileo and degrading fix rate from >99% to ~67%.

## Positioning — CLAS

TOML section: `[positioning.clas]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `grid_selection_radius` | integer | PPP-RTK, VRS | CLAS grid search radius (m). Controls how far from the rover to search for grid-based tropospheric/ionospheric corrections. |
| `receiver_type` | string | PPP-RTK, VRS | Rover receiver type identifier. Used for ISB (inter-system bias) table lookup. |
| `position_uncertainty_x` | float | PPP-RTK, VRS | Rover approximate position X in ECEF (m). Used for initial CLAS grid search before first fix. |
| `position_uncertainty_y` | float | PPP-RTK, VRS | Rover approximate position Y in ECEF (m). |
| `position_uncertainty_z` | float | PPP-RTK, VRS | Rover approximate position Z in ECEF (m). |

## Positioning — SNR Mask

TOML section: `[positioning.snr_mask]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `rover_enabled` | boolean | All | Enable elevation-dependent SNR mask for the rover receiver. |
| `base_enabled` | boolean | RTK | Enable elevation-dependent SNR mask for the base station. |
| `L1` | array[int] | All | L1 SNR mask values per elevation bin (dBHz). 9-element array (0–45 dBHz per 5° elevation bin). |
| `L2` | array[int] | All | L2 SNR mask values per elevation bin (dBHz). 9-element array (0–45 dBHz per 5° elevation bin). |
| `L5` | array[int] | All | L5 SNR mask values per elevation bin (dBHz). 9-element array (0–45 dBHz per 5° elevation bin). |

## Positioning — Corrections

TOML section: `[positioning.corrections]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `satellite_antenna` | boolean | All | Apply satellite antenna phase center offset correction using ANTEX file. |
| `receiver_antenna` | boolean | All | Apply receiver antenna phase center offset/variation correction. |
| `phase_windup` | enum | PPP, PPP-RTK, VRS | Phase wind-up correction. `off` · `on` · `precise` |
| `exclude_eclipse` | boolean | PPP, PPP-RTK | Exclude satellites in eclipse (yaw maneuver period) to avoid degraded orbit/clock. |
| `raim_fde` | boolean | SPP | RAIM fault detection and exclusion for single-point positioning. |
| `iono_compensation` | enum | PPP-RTK, VRS, SSR2OSR | Ionospheric compensation method for SSR-based processing. `off` · `ssr` · `meas` |
| `partial_ar` | boolean | PPP-RTK, VRS | Enable partial ambiguity resolution (fix a satellite subset). |
| `shapiro_delay` | boolean | PPP, PPP-RTK, VRS | Apply relativistic Shapiro time delay correction. |
| `exclude_qzs_ref` | boolean | PPP-RTK, VRS | Exclude QZS satellites from reference satellite selection in DD processing. |
| `no_phase_bias_adj` | boolean | PPP-RTK, VRS | Disable phase bias adjustment. Used when phase bias is already applied by SSR corrections. |
| `gps_frequency` | enum | PPP-RTK, VRS | GPS frequency pair selection for CLAS processing. `l1` · `l1+l2` · `l1+l5` · `l1+l2+l5` · `l1+l5(l2)` |
| `reserved` | boolean | — | Reserved for future use. |
| `qzs_frequency` | enum | PPP-RTK, VRS | QZS frequency pair selection for CLAS processing. `l1` · `l1+l2` · `l1+l5` · `l1+l2+l5` · `l1+l5(l2)` |
| `tidal_correction` | enum | PPP, PPP-RTK, VRS | Tidal displacement correction. Option `solid+otl-clasgrid+pole` uses CLAS grid-based ocean tide loading. `off` · `on` · `otl` · `solid+otl-clasgrid+pole` |

## Positioning — Atmosphere

TOML section: `[positioning.atmosphere]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `ionosphere` | enum | All | Ionospheric correction model. PPP uses `dual-freq` or `est-stec`. PPP-RTK uses `est-stec` or `est-adaptive`. RTK typically uses `dual-freq`. `off` · `brdc` · `sbas` · `dual-freq` · `est-stec` · `ionex-tec` · `qzs-brdc` · `est-adaptive` |
| `troposphere` | enum | All | Tropospheric correction model. PPP/PPP-RTK use `est-ztd` or `est-ztdgrad`. SPP/RTK typically use `saas`. `off` · `saas` · `sbas` · `est-ztd` · `est-ztdgrad` |

## Ambiguity Resolution

TOML section: `[ambiguity_resolution]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `mode` | enum | RTK, PPP, PPP-RTK, VRS | Ambiguity resolution strategy. `off` · `continuous` · `instantaneous` · `fix-and-hold` |
| `gps_ar` | boolean | RTK | GPS AR mode for GLONASS fix-and-hold second pass. |
| `glonass_ar` | enum | RTK | GLONASS ambiguity resolution mode. `off` · `on` |
| `bds_ar` | boolean | RTK, PPP-RTK | BDS ambiguity resolution enable/disable. |
| `qzs_ar` | boolean | PPP-RTK, VRS | QZS ambiguity resolution mode. |
| `systems` | integer | RTK, PPP-RTK, VRS | Constellation bitmask for AR. Limits which systems participate in integer ambiguity resolution. |

## Ambiguity Resolution — Thresholds

TOML section: `[ambiguity_resolution.thresholds]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `ratio` | float | RTK, PPP, PPP-RTK, VRS | LAMBDA ratio test threshold (2nd-best / best). Typical value: 3.0. |
| `ratio1` | float | RTK, PPP, PPP-RTK | Secondary AR threshold. In MADOCA-PPP: max 3D position std-dev to start narrow-lane AR. |
| `ratio2` | float | RTK, PPP-RTK | Additional AR threshold parameter. |
| `ratio3` | float | RTK, PPP-RTK | Additional AR threshold parameter. |
| `ratio4` | float | RTK | Additional AR threshold parameter. |
| `ratio5` | float | PPP-RTK | Chi-square threshold for hold validation. |
| `ratio6` | float | PPP-RTK | Chi-square threshold for fix validation. |
| `alpha` | enum | PPP-RTK, VRS | AR significance level (ILS success rate). `0.1%` · `0.5%` · `1%` · `5%` · `10%` · `20%` |
| `elevation_mask` | float | RTK, PPP-RTK, VRS | Minimum satellite elevation for AR participation (degrees). |
| `hold_elevation` | float | RTK, PPP-RTK, VRS | Minimum satellite elevation for fix-and-hold constraint application (degrees). |

## Ambiguity Resolution — Counters

TOML section: `[ambiguity_resolution.counters]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `lock_count` | integer | RTK, PPP-RTK, VRS | Minimum continuous lock count before a satellite participates in AR. |
| `min_fix` | integer | RTK, PPP-RTK | Minimum fix epochs before applying fix-and-hold constraint. |
| `max_iterations` | integer | RTK, PPP-RTK | Maximum LAMBDA search iterations per epoch. |
| `out_count` | integer | RTK, PPP-RTK, VRS | Reset ambiguity after this many continuous outage epochs. |

## Ambiguity Resolution — Partial AR

TOML section: `[ambiguity_resolution.partial_ar]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `min_ambiguities` | integer | PPP-RTK, VRS | Minimum number of ambiguities required for partial AR attempt. |
| `max_excluded_sats` | integer | PPP-RTK, VRS | Maximum satellites to exclude during partial AR satellite rotation. |
| `min_fix_sats` | integer | RTK, PPP-RTK | Minimum DD pairs required for a valid fix. 0 = disabled. |
| `min_drop_sats` | integer | RTK | Minimum DD pairs before excluding the weakest satellite in partial AR. 0 = disabled. |
| `min_hold_sats` | integer | RTK | Minimum DD pairs for fix-and-hold mode activation. 0 = no minimum. |
| `ar_filter` | boolean | RTK, PPP-RTK | Exclude newly-locked satellites that degrade the AR ratio. |

## Ambiguity Resolution — Hold

TOML section: `[ambiguity_resolution.hold]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `variance` | float | PPP-RTK, VRS | Variance of held ambiguity pseudo-observation (cyc²). Controls how tightly held ambiguities constrain the filter. |
| `gain` | float | RTK | Gain factor for fractional GLONASS/SBAS inter-channel bias update in fix-and-hold. |

## Rejection Criteria

TOML section: `[rejection]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `innovation` | float | RTK, PPP | Innovation (pre-fit residual) threshold (m). Observations exceeding this are rejected. |
| `l1_l2_residual` | float | PPP-RTK, VRS | L1/L2 residual rejection threshold (σ). |
| `dispersive` | float | PPP-RTK, VRS | Dispersive (ionospheric) residual rejection threshold (σ). |
| `non_dispersive` | float | PPP-RTK, VRS | Non-dispersive (geometric) residual rejection threshold (σ). |
| `hold_chi_square` | float | PPP-RTK, VRS | Chi-square threshold for hold-mode outlier detection. |
| `fix_chi_square` | float | PPP-RTK, VRS | Chi-square threshold for fix-mode outlier detection. |
| `gdop` | float | All | Maximum GDOP for valid solution output. |
| `pseudorange_diff` | float | VRS | Pseudorange consistency check threshold (m). Rejects satellites with large code-phase disagreement. |
| `position_error_count` | integer | VRS | Number of consecutive position error epochs before filter reset. |

## Slip Detection

TOML section: `[slip_detection]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `threshold` | float | RTK, PPP, PPP-RTK, VRS | Geometry-free (LG) cycle slip detection threshold (m). |
| `doppler` | float | RTK, PPP, PPP-RTK | Doppler-phase rate cycle slip detection threshold (cyc/s). 0 = disabled. |

## Kalman Filter

TOML section: `[kalman_filter]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `iterations` | integer | RTK, PPP, PPP-RTK, VRS | Number of measurement update iterations per epoch. More iterations improve linearization accuracy. |
| `sync_solution` | boolean | RT | Synchronize solution output with observation time in real-time mode. |

## Kalman Filter — Measurement Error

TOML section: `[kalman_filter.measurement_error]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `code_phase_ratio_L1` | float | All | Code/phase measurement error ratio for L1. Code error = phase error × ratio. Typical: 100. |
| `code_phase_ratio_L2` | float | All | Code/phase measurement error ratio for L2. |
| `code_phase_ratio_L5` | float | All | Code/phase measurement error ratio for L5. |
| `phase` | float | All | Base carrier phase measurement error (m). Used in elevation-dependent weighting: error = phase + phase_elevation / sin(el). |
| `phase_elevation` | float | All | Elevation-dependent carrier phase error coefficient (m). |
| `phase_baseline` | float | RTK | Baseline-length-dependent phase error (m/10km). Proportional to rover–base distance. |
| `doppler` | float | All | Doppler measurement error (Hz). |
| `ura_ratio` | float | PPP | User Range Accuracy scaling ratio. Adjusts satellite-specific weighting based on broadcast URA. |

## Kalman Filter — Initial Std. Deviation

TOML section: `[kalman_filter.initial_std]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `bias` | float | RTK, PPP, PPP-RTK, VRS | Initial standard deviation for carrier phase bias states (m). |
| `ionosphere` | float | PPP-RTK, VRS | Initial standard deviation for ionospheric delay states (m). |
| `troposphere` | float | PPP, PPP-RTK, VRS | Initial standard deviation for tropospheric delay states (m). |

## Kalman Filter — Process Noise

TOML section: `[kalman_filter.process_noise]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `bias` | float | RTK, PPP, PPP-RTK, VRS | Process noise for carrier phase bias states (m/√s). |
| `ionosphere` | float | PPP-RTK, VRS | Process noise for ionospheric delay states (m/√s). |
| `iono_max` | float | PPP-RTK, VRS | Maximum ionospheric process noise clamp (m). Prevents excessive iono state growth. |
| `troposphere` | float | PPP, PPP-RTK, VRS | Process noise for tropospheric delay states (m/√s). |
| `accel_h` | float | All | Horizontal acceleration process noise (m/s²). Active when `dynamics = true`. |
| `accel_v` | float | All | Vertical acceleration process noise (m/s²). Active when `dynamics = true`. |
| `position_h` | float | PPP-RTK, VRS | Horizontal position process noise (m). |
| `position_v` | float | PPP-RTK, VRS | Vertical position process noise (m). |
| `position` | float | All | General position process noise (m). Fallback when h/v not specified. |
| `ifb` | float | PPP | Inter-frequency bias process noise (m). For multi-frequency PPP bias estimation. |
| `iono_time_const` | float | PPP-RTK, VRS | Ionospheric time constant (s). Controls iono state temporal correlation in the adaptive filter. |
| `clock_stability` | float | PPP | Receiver clock stability (s/s). Used in PPP clock state prediction. |

## Adaptive Filter

TOML section: `[adaptive_filter]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `enabled` | boolean | PPP-RTK, VRS | Enable adaptive Kalman filter process noise scaling. |
| `iono_forgetting` | float | PPP-RTK, VRS | Forgetting factor for ionospheric state (0–1). Lower = faster adaptation. |
| `iono_gain` | float | PPP-RTK, VRS | Adaptive gain for ionospheric process noise adjustment. |
| `pva_forgetting` | float | PPP-RTK, VRS | Forgetting factor for position/velocity/acceleration states (0–1). |
| `pva_gain` | float | PPP-RTK, VRS | Adaptive gain for PVA process noise adjustment. |

## Signal Selection

TOML section: `[signals]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `gps` | enum | PPP, PPP-RTK | GPS frequency pair selection for PPP/PPP-RTK processing. `L1/L2` · `L1/L5` · `L1/L2/L5` |
| `qzs` | enum | PPP, PPP-RTK | QZS frequency pair selection. `L1/L5` · `L1/L2` · `L1/L5/L2` |
| `galileo` | enum | PPP, PPP-RTK | Galileo frequency pair selection. `E1/E5a` · `E1/E5b` · `E1/E6` · `E1/E5a/E5b/E6` · `E1/E5a/E6/E5b` |
| `bds2` | enum | PPP, PPP-RTK | BDS-2 frequency pair selection. `B1I/B3I` · `B1I/B2I` · `B1I/B3I/B2I` |
| `bds3` | enum | PPP, PPP-RTK | BDS-3 frequency pair selection. `B1I/B3I` · `B1I/B2a` · `B1I/B3I/B2a` |

## Receiver

TOML section: `[receiver]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `iono_correction` | boolean | PPP | Enable ionospheric correction in MADOCA-PPP processing. |
| `ignore_chi_error` | boolean | SPP | Ignore chi-square test errors in SPP solution validation. |
| `bds2_bias` | boolean | PPP | Enable BDS-2 code bias correction (satellite-dependent group delay). |
| `ppp_sat_clock_bias` | integer | PPP | PPP satellite code bias source selection. |
| `ppp_sat_phase_bias` | integer | PPP | PPP satellite phase bias source selection. |
| `uncorr_bias` | integer | PPP | Uncorrelated bias parameter for PPP processing. |
| `max_bias_dt` | integer | PPP | Maximum age of bias correction data (s) before invalidation. |
| `satellite_mode` | integer | PPP | Satellite processing mode selector. |
| `phase_shift` | enum | PPP-RTK, VRS | Phase cycle shift correction. Corrects quarter-cycle shifts between systems. `off` · `table` |
| `isb` | boolean | PPP-RTK, VRS | Inter-system bias estimation mode. |
| `reference_type` | string | PPP-RTK, VRS | Reference station receiver type for ISB table lookup. |
| `max_age` | float | RTK, PPP-RTK | Maximum age of differential correction (s). |
| `baseline_length` | float | RTK | Baseline length constraint (m). 0 = no constraint. |
| `baseline_sigma` | float | RTK | Standard deviation of baseline length constraint (m). |

## Antenna — Rover

TOML section: `[antenna.rover]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `position_type` | enum | All | Rover position input type. `llh` · `xyz` · `single` · `posfile` · `rinexhead` · `rtcm` · `raw` |
| `position_1` | float | All | Rover position coordinate 1 (latitude or X, depending on `position_type`). |
| `position_2` | float | All | Rover position coordinate 2 (longitude or Y). |
| `position_3` | float | All | Rover position coordinate 3 (height or Z). |
| `type` | string | All | Rover antenna type (must match ANTEX file entry). `*` = use RINEX header. |
| `delta_e` | float | All | Rover antenna delta East offset (m). |
| `delta_n` | float | All | Rover antenna delta North offset (m). |
| `delta_u` | float | All | Rover antenna delta Up offset (m). |

## Antenna — Base

TOML section: `[antenna.base]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `position_type` | enum | RTK, VRS | Base station position input type. `llh` · `xyz` · `single` · `posfile` · `rinexhead` · `rtcm` · `raw` |
| `position_1` | float | RTK, VRS | Base position coordinate 1. |
| `position_2` | float | RTK, VRS | Base position coordinate 2. |
| `position_3` | float | RTK, VRS | Base position coordinate 3. |
| `type` | string | RTK, VRS | Base antenna type. |
| `delta_e` | float | RTK, VRS | Base antenna delta East offset (m). |
| `delta_n` | float | RTK, VRS | Base antenna delta North offset (m). |
| `delta_u` | float | RTK, VRS | Base antenna delta Up offset (m). |
| `max_average_epochs` | integer | RT | Maximum epochs for base position averaging in real-time mode. |
| `init_reset` | boolean | RT | Reset base position averaging on restart. |

## Output

TOML section: `[output]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `format` | enum | All | Solution output format. `llh` · `xyz` · `enu` · `nmea` |
| `header` | boolean | All | Output header line with option summary. |
| `options` | boolean | All | Output processing options in header. |
| `velocity` | boolean | All | Include velocity in solution output. |
| `time_system` | enum | All | Time system for output. `gpst` · `utc` · `jst` |
| `time_format` | enum | All | Time format. `tow` · `hms` |
| `time_decimals` | integer | All | Number of decimal digits for time output. |
| `coordinate_format` | enum | All | Coordinate format. `deg` · `dms` |
| `field_separator` | string | All | Field separator character for output. |
| `single_output` | boolean | All | Output SPP solution alongside fixed solution. |
| `max_solution_std` | float | All | Maximum solution standard deviation for output (m). Solutions exceeding this are suppressed. |
| `height_type` | enum | All | Height output type. `ellipsoidal` · `geodetic` |
| `geoid_model` | enum | All | Geoid model for geodetic height conversion. `internal` · `egm96` · `egm08_2.5` · `egm08_1` · `gsi2000` |
| `static_solution` | enum | PP | Static mode output control. `all` · `single` |
| `nmea_interval_1` | float | All | NMEA GGA/RMC output interval (s). |
| `nmea_interval_2` | float | All | NMEA GSA/GSV output interval (s). |
| `solution_status` | enum | All | Solution status output level. `off` · `state` · `residual` |

## Files

TOML section: `[files]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `satellite_atx` | string | All | Satellite antenna ANTEX file. Required for satellite antenna PCV correction. |
| `receiver_atx` | string | All | Receiver antenna ANTEX file. Required for receiver antenna PCV correction. |
| `station_pos` | string | All | Station position file for fixed/known positions. |
| `geoid` | string | All | Geoid data file for geodetic height conversion. |
| `ionosphere` | string | All | IONEX ionosphere map file. Used when `ionosphere = ionex-tec`. |
| `dcb` | string | PPP | Differential code bias file. |
| `eop` | string | PPP, PPP-RTK | Earth orientation parameter file for precise coordinate frame. |
| `ocean_loading` | string | PPP, PPP-RTK | BLQ ocean tide loading file. Required when `tidal_correction` includes OTL. |
| `elevation_mask_file` | string | All | Azimuth-dependent elevation mask file. |
| `temp_dir` | string | All | Temporary directory for intermediate files. |
| `geexe` | string | All | Google Earth executable path (legacy GUI feature). |
| `solution_stat` | string | All | Solution statistics output file path. |
| `trace` | string | All | Debug trace output file path. |
| `fcb` | string | PPP | Fractional cycle bias file for PPP-AR. |
| `bias_sinex` | string | PPP | Bias SINEX file for PPP satellite bias correction. |
| `cssr_grid` | string | PPP-RTK, VRS | CLAS CSSR grid definition file. Required for CLAS grid-based corrections. |
| `isb_table` | string | PPP-RTK, VRS | Inter-system bias correction table file. |
| `phase_cycle` | string | PPP-RTK, VRS | Phase cycle shift correction table file. |
| `cmd_file_1` | string | RT | Receiver command file for input stream 1. |
| `cmd_file_2` | string | RT | Receiver command file for input stream 2. |
| `cmd_file_3` | string | RT | Receiver command file for input stream 3. |

## Server (rtkrcv)

TOML section: `[server]`

| TOML Key | Type | Modes | Description |
|:---------|:-----|:------|:------------|
| `cycle_ms` | integer | RT | Server main loop cycle interval (ms). Controls processing rate. |
| `timeout_ms` | integer | RT | Stream read timeout (ms). After timeout, stream is considered disconnected. |
| `reconnect_ms` | integer | RT | Stream reconnection interval (ms) after disconnect. |
| `nmea_cycle_ms` | integer | RT | NMEA request transmission cycle (ms) for NTRIP caster position updates. |
| `buffer_size` | integer | RT | Stream input buffer size (bytes). |
| `nav_msg_select` | string | RT | Navigation message type selection for multi-source environments. |
| `proxy` | string | RT | HTTP proxy address for NTRIP connections. |
| `swap_margin` | integer | RT | File swap margin (s) for continuous logging across file boundaries. |
| `time_interpolation` | boolean | RT | Enable time interpolation between observation epochs. |
| `sbas_satellite` | string | RT | SBAS satellite selection. |
| `max_obs_loss` | float | RT | Maximum observation gap duration before filter reset (s). |
| `float_count` | integer | RT | Number of float epochs before triggering filter reset. |
| `rinex_option_1` | string | All | RINEX conversion option string for rover stream. |
| `rinex_option_2` | string | All | RINEX conversion option string for base stream. |
| `ppp_option` | string | PPP | PPP processing option string (passed to PPP engine). |
| `rtcm_option` | string | RT | RTCM decoder option string. |
| `l6_margin` | integer | RT | L6 message margin (epochs) for CLAS L6 real-time synchronization. |
| `regularly` | integer | VRS | Regular filter reset interval (s). 0 = disabled. |
| `start_cmd` | string | RT | Shell command executed on server start. |
| `stop_cmd` | string | RT | Shell command executed on server stop. |

## Streams

TOML section: `[streams]` — **Real-time only** (`mrtk run` / `rtkrcv`)

Stream configuration uses a hierarchical structure:

```toml
[streams.input.rover]
type = "serial"
path = "ttyACM0:115200"
format = "ubx"

[streams.input.correction]
type = "file"
path = "correction.l6::T::+1"
format = "clas"

[streams.output.stream1]
type = "file"
path = "output.pos"

[streams.log.stream1]
type = "file"
path = "rover.log"
```

| Key | Type | Description |
|:----|:-----|:------------|
| `type` | string | Stream type: `serial` · `file` · `tcpsvr` · `tcpcli` · `ntrip` · `off` |
| `path` | string | Stream path (serial device, file path, or URL) |
| `format` | string | Data format: `rtcm3` · `ubx` · `sbf` · `binex` · `rinex` · `clas` · `l6e` |
| `nmeareq` | boolean | Send NMEA position request to stream |
| `nmealat` | float | NMEA request latitude (degrees) |
| `nmealon` | float | NMEA request longitude (degrees) |

