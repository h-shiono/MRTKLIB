# Configuration Options Reference

TOML configuration options available in MRTKLIB (generated from the internal mapping table).
Options are grouped by their TOML section.

!!! tip "Auto-generated"
    This page is auto-generated from the internal option mapping table.
    Run `python scripts/docs/gen_config_ref.py > docs/reference/config-options.md` to regenerate.

## Positioning

TOML section: `[positioning]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `mode` | enum | `0:single,1:dgps,2:kinematic,3:static,4:movingbase,5:fixed,6:ppp-kine,7:ppp-static,8:ppp-fixed,9:ppp-rtk,10:ssr2osr,11:ssr2osr-fixed,12:vrs-rtk` | `pos1-posmode` |
| `frequency` | enum | `1:l1,2:l1+2,3:l1+2+3,4:l1+2+3+4,5:l1+2+3+4+5` | `pos1-frequency` |
| `solution_type` | enum | `0:forward,1:backward,2:combined` | `pos1-soltype` |
| `elevation_mask` | float | — | `pos1-elmask` |
| `dynamics` | boolean | `true` / `false` | `pos1-dynamics` |
| `satellite_ephemeris` | enum | `0:brdc,1:precise,2:brdc+sbas,3:brdc+ssrapc,4:brdc+ssrcom` | `pos1-sateph` |
| `systems` | string[] | e.g. `["GPS", "Galileo", "QZSS"]` | `pos1-navsys` |
| `excluded_sats` | string | — | `pos1-exclsats` |
| `signals` | string[] | e.g. `["G1C", "G2W", "E1C"]` | `pos1-signals` |

## Positioning — CLAS

TOML section: `[positioning.clas]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `grid_selection_radius` | integer | — | `pos1-gridsel` |
| `receiver_type` | string | — | `pos1-rectype` |
| `position_uncertainty_x` | float | — | `pos1-rux` |
| `position_uncertainty_y` | float | — | `pos1-ruy` |
| `position_uncertainty_z` | float | — | `pos1-ruz` |

## Positioning — SNR Mask

TOML section: `[positioning.snr_mask]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `rover_enabled` | boolean | `true` / `false` | `pos1-snrmask_r` |
| `base_enabled` | boolean | `true` / `false` | `pos1-snrmask_b` |
| `L1` | array[int] | 9-element array (0-45 dBHz per 5°) | `pos1-snrmask_L1` |
| `L2` | array[int] | 9-element array (0-45 dBHz per 5°) | `pos1-snrmask_L2` |
| `L5` | array[int] | 9-element array (0-45 dBHz per 5°) | `pos1-snrmask_L5` |

## Positioning — Corrections

TOML section: `[positioning.corrections]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `satellite_antenna` | boolean | `true` / `false` | `pos1-posopt1` |
| `receiver_antenna` | boolean | `true` / `false` | `pos1-posopt2` |
| `phase_windup` | enum | `0:off,1:on,2:precise` | `pos1-posopt3` |
| `exclude_eclipse` | boolean | `true` / `false` | `pos1-posopt4` |
| `raim_fde` | boolean | `true` / `false` | `pos1-posopt5` |
| `iono_compensation` | enum | `0:off,1:ssr,2:meas` | `pos1-posopt6` |
| `partial_ar` | boolean | `true` / `false` | `pos1-posopt7` |
| `shapiro_delay` | boolean | `true` / `false` | `pos1-posopt8` |
| `exclude_qzs_ref` | boolean | `true` / `false` | `pos1-posopt9` |
| `no_phase_bias_adj` | boolean | `true` / `false` | `pos1-posopt10` |
| `gps_frequency` | enum | `1:l1,2:l1+l2,3:l1+l5,4:l1+l2+l5,5:l1+l5(l2)` | `pos1-posopt11` |
| `reserved` | boolean | `true` / `false` | `pos1-posopt12` |
| `qzs_frequency` | enum | `1:l1,2:l1+l2,3:l1+l5,4:l1+l2+l5,5:l1+l5(l2)` | `pos1-posopt13` |
| `tidal_correction` | enum | `0:off,1:on,2:otl,3:solid+otl-clasgrid+pole` | `pos1-tidecorr` |

## Positioning — Atmosphere

TOML section: `[positioning.atmosphere]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `ionosphere` | enum | `0:off,1:brdc,2:sbas,3:dual-freq,4:est-stec,5:ionex-tec,6:qzs-brdc,9:est-adaptive` | `pos1-ionoopt` |
| `troposphere` | enum | `0:off,1:saas,2:sbas,3:est-ztd,4:est-ztdgrad` | `pos1-tropopt` |

## Ambiguity Resolution

TOML section: `[ambiguity_resolution]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `mode` | enum | `0:off,1:continuous,2:instantaneous,3:fix-and-hold` | `pos2-armode` |
| `gps_ar` | boolean | `true` / `false` | `pos2-gpsarmode` |
| `glonass_ar` | enum | `0:off,1:on` | `pos2-gloarmode` |
| `bds_ar` | boolean | `true` / `false` | `pos2-bdsarmode` |
| `qzs_ar` | boolean | `true` / `false` | `pos2-qzsarmode` |
| `systems` | integer | — | `pos2-arsys` |

## Ambiguity Resolution — Thresholds

TOML section: `[ambiguity_resolution.thresholds]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `ratio` | float | — | `pos2-arthres` |
| `ratio1` | float | — | `pos2-arthres1` |
| `ratio2` | float | — | `pos2-arthres2` |
| `ratio3` | float | — | `pos2-arthres3` |
| `ratio4` | float | — | `pos2-arthres4` |
| `ratio5` | float | — | `pos2-arthres5` |
| `ratio6` | float | — | `pos2-arthres6` |
| `alpha` | enum | `0:0.1%,1:0.5%,2:1%,3:5%,4:10%,5:20%` | `pos2-aralpha` |
| `elevation_mask` | float | — | `pos2-arelmask` |
| `hold_elevation` | float | — | `pos2-elmaskhold` |

## Ambiguity Resolution — Counters

TOML section: `[ambiguity_resolution.counters]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `lock_count` | integer | — | `pos2-arlockcnt` |
| `min_fix` | integer | — | `pos2-arminfix` |
| `max_iterations` | integer | — | `pos2-armaxiter` |
| `out_count` | integer | — | `pos2-aroutcnt` |

## Ambiguity Resolution — Partial AR

TOML section: `[ambiguity_resolution.partial_ar]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `min_ambiguities` | integer | — | `pos2-arminamb` |
| `max_excluded_sats` | integer | — | `pos2-armaxdelsat` |
| `min_fix_sats` | integer | — | `pos2-arminfixsats` |
| `min_drop_sats` | integer | — | `pos2-armindropsats` |
| `min_hold_sats` | integer | — | `pos2-arminholdsats` |
| `ar_filter` | boolean | `true` / `false` | `pos2-arfilter` |

## Ambiguity Resolution — Hold

TOML section: `[ambiguity_resolution.hold]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `variance` | float | — | `pos2-varholdamb` |
| `gain` | float | — | `pos2-gainholdamb` |

## Rejection Criteria

TOML section: `[rejection]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `innovation` | float | — | `pos2-rejionno` |
| `l1_l2_residual` | float | — | `pos2-rejionno1` |
| `dispersive` | float | — | `pos2-rejionno2` |
| `non_dispersive` | float | — | `pos2-rejionno3` |
| `hold_chi_square` | float | — | `pos2-rejionno4` |
| `fix_chi_square` | float | — | `pos2-rejionno5` |
| `gdop` | float | — | `pos2-rejgdop` |
| `pseudorange_diff` | float | — | `pos2-rejdiffpse` |
| `position_error_count` | integer | — | `pos2-poserrcnt` |

## Slip Detection

TOML section: `[slip_detection]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `threshold` | float | — | `pos2-slipthres` |
| `doppler` | float | — | `pos2-thresdop` |

## Kalman Filter

TOML section: `[kalman_filter]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `iterations` | integer | — | `pos2-niter` |
| `sync_solution` | boolean | `true` / `false` | `pos2-syncsol` |

## Kalman Filter — Measurement Error

TOML section: `[kalman_filter.measurement_error]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `code_phase_ratio_L1` | float | — | `stats-eratio1` |
| `code_phase_ratio_L2` | float | — | `stats-eratio2` |
| `code_phase_ratio_L5` | float | — | `stats-eratio3` |
| `phase` | float | — | `stats-errphase` |
| `phase_elevation` | float | — | `stats-errphaseel` |
| `phase_baseline` | float | — | `stats-errphasebl` |
| `doppler` | float | — | `stats-errdoppler` |
| `ura_ratio` | float | — | `stats-uraratio` |

## Kalman Filter — Initial Std. Deviation

TOML section: `[kalman_filter.initial_std]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `bias` | float | — | `stats-stdbias` |
| `ionosphere` | float | — | `stats-stdiono` |
| `troposphere` | float | — | `stats-stdtrop` |

## Kalman Filter — Process Noise

TOML section: `[kalman_filter.process_noise]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `bias` | float | — | `stats-prnbias` |
| `ionosphere` | float | — | `stats-prniono` |
| `iono_max` | float | — | `stats-prnionomax` |
| `troposphere` | float | — | `stats-prntrop` |
| `accel_h` | float | — | `stats-prnaccelh` |
| `accel_v` | float | — | `stats-prnaccelv` |
| `position_h` | float | — | `stats-prnposith` |
| `position_v` | float | — | `stats-prnpositv` |
| `position` | float | — | `stats-prnpos` |
| `ifb` | float | — | `stats-prnifb` |
| `iono_time_const` | float | — | `stats-tconstiono` |
| `clock_stability` | float | — | `stats-clkstab` |

## Adaptive Filter

TOML section: `[adaptive_filter]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `enabled` | boolean | `true` / `false` | `pos2-prnadpt` |
| `iono_forgetting` | float | — | `pos2-forgetion` |
| `iono_gain` | float | — | `pos2-afgainion` |
| `pva_forgetting` | float | — | `pos2-forgetpva` |
| `pva_gain` | float | — | `pos2-afgainpva` |

## Signal Selection

TOML section: `[signals]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `gps` | enum | `0:L1/L2,1:L1/L5,2:L1/L2/L5` | `pos2-siggps` |
| `qzs` | enum | `0:L1/L5,1:L1/L2,2:L1/L5/L2` | `pos2-sigqzs` |
| `galileo` | enum | `0:E1/E5a,1:E1/E5b,2:E1/E6,3:E1/E5a/E5b/E6,4:E1/E5a/E6/E5b` | `pos2-siggal` |
| `bds2` | enum | `0:B1I/B3I,1:B1I/B2I,2:B1I/B3I/B2I` | `pos2-sigbds2` |
| `bds3` | enum | `0:B1I/B3I,1:B1I/B2a,2:B1I/B3I/B2a` | `pos2-sigbds3` |

## Receiver

TOML section: `[receiver]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `iono_correction` | boolean | `true` / `false` | `pos2-ionocorr` |
| `ignore_chi_error` | boolean | `true` / `false` | `pos2-ign_chierr` |
| `bds2_bias` | boolean | `true` / `false` | `pos2-bds2bias` |
| `ppp_sat_clock_bias` | integer | — | `pos2-pppsatcb` |
| `ppp_sat_phase_bias` | integer | — | `pos2-pppsatpb` |
| `uncorr_bias` | integer | — | `pos2-uncorrbias` |
| `max_bias_dt` | integer | — | `pos2-maxbiasdt` |
| `satellite_mode` | integer | — | `pos2-sattmode` |
| `phase_shift` | enum | `0:off,1:table` | `pos2-phasshft` |
| `isb` | boolean | `true` / `false` | `pos2-isb` |
| `reference_type` | string | — | `pos2-rectype` |
| `max_age` | float | — | `pos2-maxage` |
| `baseline_length` | float | — | `pos2-baselen` |
| `baseline_sigma` | float | — | `pos2-basesig` |

## Antenna — Rover

TOML section: `[antenna.rover]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `position_type` | enum | `0:llh,1:xyz,2:single,3:posfile,4:rinexhead,5:rtcm,6:raw` | `ant1-postype` |
| `position_1` | float | — | `ant1-pos1` |
| `position_2` | float | — | `ant1-pos2` |
| `position_3` | float | — | `ant1-pos3` |
| `type` | string | — | `ant1-anttype` |
| `delta_e` | float | — | `ant1-antdele` |
| `delta_n` | float | — | `ant1-antdeln` |
| `delta_u` | float | — | `ant1-antdelu` |

## Antenna — Base

TOML section: `[antenna.base]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `position_type` | enum | `0:llh,1:xyz,2:single,3:posfile,4:rinexhead,5:rtcm,6:raw` | `ant2-postype` |
| `position_1` | float | — | `ant2-pos1` |
| `position_2` | float | — | `ant2-pos2` |
| `position_3` | float | — | `ant2-pos3` |
| `type` | string | — | `ant2-anttype` |
| `delta_e` | float | — | `ant2-antdele` |
| `delta_n` | float | — | `ant2-antdeln` |
| `delta_u` | float | — | `ant2-antdelu` |
| `max_average_epochs` | integer | — | `ant2-maxaveep` |
| `init_reset` | boolean | `true` / `false` | `ant2-initrst` |

## Output

TOML section: `[output]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `format` | enum | `0:llh,1:xyz,2:enu,3:nmea` | `out-solformat` |
| `header` | boolean | `true` / `false` | `out-outhead` |
| `options` | boolean | `true` / `false` | `out-outopt` |
| `velocity` | boolean | `true` / `false` | `out-outvel` |
| `time_system` | enum | `0:gpst,1:utc,2:jst` | `out-timesys` |
| `time_format` | enum | `0:tow,1:hms` | `out-timeform` |
| `time_decimals` | integer | — | `out-timendec` |
| `coordinate_format` | enum | `0:deg,1:dms` | `out-degform` |
| `field_separator` | string | — | `out-fieldsep` |
| `single_output` | boolean | `true` / `false` | `out-outsingle` |
| `max_solution_std` | float | — | `out-maxsolstd` |
| `height_type` | enum | `0:ellipsoidal,1:geodetic` | `out-height` |
| `geoid_model` | enum | `0:internal,1:egm96,2:egm08_2.5,3:egm08_1,4:gsi2000` | `out-geoid` |
| `static_solution` | enum | `0:all,1:single` | `out-solstatic` |
| `nmea_interval_1` | float | — | `out-nmeaintv1` |
| `nmea_interval_2` | float | — | `out-nmeaintv2` |
| `solution_status` | enum | `0:off,1:state,2:residual` | `out-outstat` |

## Files

TOML section: `[files]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `satellite_atx` | string | — | `file-satantfile` |
| `receiver_atx` | string | — | `file-rcvantfile` |
| `station_pos` | string | — | `file-staposfile` |
| `geoid` | string | — | `file-geoidfile` |
| `ionosphere` | string | — | `file-ionofile` |
| `dcb` | string | — | `file-dcbfile` |
| `eop` | string | — | `file-eopfile` |
| `ocean_loading` | string | — | `file-blqfile` |
| `elevation_mask_file` | string | — | `file-elmaskfile` |
| `temp_dir` | string | — | `file-tempdir` |
| `geexe` | string | — | `file-geexefile` |
| `solution_stat` | string | — | `file-solstatfile` |
| `trace` | string | — | `file-tracefile` |
| `fcb` | string | — | `file-fcbfile` |
| `bias_sinex` | string | — | `file-biafile` |
| `cssr_grid` | string | — | `file-cssrgridfile` |
| `isb_table` | string | — | `file-isbfile` |
| `phase_cycle` | string | — | `file-phacycfile` |
| `cmd_file_1` | string | — | `file-cmdfile1` |
| `cmd_file_2` | string | — | `file-cmdfile2` |
| `cmd_file_3` | string | — | `file-cmdfile3` |

## Server (rtkrcv)

TOML section: `[server]`

| TOML Key | Type | Values | Legacy Key |
|----------|------|--------|------------|
| `cycle_ms` | integer | — | `misc-svrcycle` |
| `timeout_ms` | integer | — | `misc-timeout` |
| `reconnect_ms` | integer | — | `misc-reconnect` |
| `nmea_cycle_ms` | integer | — | `misc-nmeacycle` |
| `buffer_size` | integer | — | `misc-buffsize` |
| `nav_msg_select` | string | — | `misc-navmsgsel` |
| `proxy` | string | — | `misc-proxyaddr` |
| `swap_margin` | integer | — | `misc-fswapmargin` |
| `time_interpolation` | boolean | `true` / `false` | `misc-timeinterp` |
| `sbas_satellite` | string | — | `misc-sbasatsel` |
| `max_obs_loss` | float | — | `misc-maxobsloss` |
| `float_count` | integer | — | `misc-floatcnt` |
| `rinex_option_1` | string | — | `misc-rnxopt1` |
| `rinex_option_2` | string | — | `misc-rnxopt2` |
| `ppp_option` | string | — | `misc-pppopt` |
| `rtcm_option` | string | — | `misc-rtcmopt` |
| `l6_margin` | integer | — | `misc-l6mrg` |
| `regularly` | integer | — | `misc-regularly` |
| `start_cmd` | string | — | `misc-startcmd` |
| `stop_cmd` | string | — | `misc-stopcmd` |

## `[streams]`

**Stream Configuration (rtkrcv only)**

Stream configuration uses a hierarchical structure:

```toml
[streams.input.rover]
type = "serial"
path = "ttyACM0:115200"
format = "ubx"

[streams.input.correction]
type = "file"
path = "correction.l6::T::+1"
format = "sbf"

[streams.output.stream1]
type = "file"
path = "output.pos"

[streams.log.stream1]
type = "file"
path = "rover.log"
```

| Key | Description |
|-----|-------------|
| `type` | Stream type: `serial`, `file`, `tcpsvr`, `tcpcli`, `ntrip`, `off` |
| `path` | Stream path (device, file, URL) |
| `format` | Data format: `rtcm3`, `ubx`, `sbf`, `binex`, `rinex`, etc. |
| `nmeareq` | Send NMEA request to stream (boolean) |
| `nmealat` | NMEA request latitude (float) |
| `nmealon` | NMEA request longitude (float) |

