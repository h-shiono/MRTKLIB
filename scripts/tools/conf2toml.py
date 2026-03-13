#!/usr/bin/env python3
"""conf2toml.py — Convert MRTKLIB legacy .conf files to TOML format.

Usage:
    python conf2toml.py input.conf [-o output.toml]
    python conf2toml.py conf/claslib/*.conf          # batch mode

If -o is not given, writes to input_stem.toml alongside the input file.
"""
from __future__ import annotations

import argparse
import re
import sys
from collections import OrderedDict
from pathlib import Path
from typing import Any

# ═══════════════════════════════════════════════════════════════════════════════
# Legacy key → (toml_section, toml_key, value_type) mapping
#
# value_type:
#   "enum"   — string enum value (kept as-is)
#   "int"    — integer
#   "float"  — floating-point
#   "str"    — string
#   "bool"   — on/off → true/false
#   "snr"    — comma-separated SNR mask values → array
#   "navsys" — integer bitmask (kept as int, comment added)
# ═══════════════════════════════════════════════════════════════════════════════

MAPPING: list[tuple[str, str, str, str]] = [
    # (legacy_key, toml_section, toml_key, value_type)

    # ── positioning ──────────────────────────────────────────────────────────
    ("pos1-posmode",    "positioning", "mode",             "enum"),
    ("pos1-frequency",  "positioning", "frequency",        "enum"),
    ("pos1-soltype",    "positioning", "solution_type",    "enum"),
    ("pos1-elmask",     "positioning", "elevation_mask",   "float"),
    ("pos1-dynamics",   "positioning", "dynamics",         "bool"),
    ("pos1-sateph",     "positioning", "satellite_ephemeris", "enum"),
    ("pos1-navsys",     "positioning", "systems",          "navsys"),
    ("pos1-exclsats",   "positioning", "excluded_sats",    "str"),
    ("pos1-signals",    "positioning", "signals",          "siglist"),
    ("pos1-gridsel",    "positioning.clas", "grid_selection_radius", "int"),
    ("pos1-rectype",    "positioning.clas", "receiver_type",  "str"),
    ("pos1-rux",        "positioning.clas", "position_uncertainty_x", "float"),
    ("pos1-ruy",        "positioning.clas", "position_uncertainty_y", "float"),
    ("pos1-ruz",        "positioning.clas", "position_uncertainty_z", "float"),

    # ── positioning.snr_mask ─────────────────────────────────────────────────
    ("pos1-snrmask_r",  "positioning.snr_mask", "rover_enabled",  "bool"),
    ("pos1-snrmask_b",  "positioning.snr_mask", "base_enabled",   "bool"),
    ("pos1-snrmask_L1", "positioning.snr_mask", "L1",             "snr"),
    ("pos1-snrmask_L2", "positioning.snr_mask", "L2",             "snr"),
    ("pos1-snrmask_L5", "positioning.snr_mask", "L5",             "snr"),

    # ── positioning.corrections ──────────────────────────────────────────────
    ("pos1-posopt1",    "positioning.corrections", "satellite_antenna",  "bool"),
    ("pos1-posopt2",    "positioning.corrections", "receiver_antenna",   "bool"),
    ("pos1-posopt3",    "positioning.corrections", "phase_windup",       "enum"),  # off/on/precise
    ("pos1-posopt4",    "positioning.corrections", "exclude_eclipse",    "bool"),
    ("pos1-posopt5",    "positioning.corrections", "raim_fde",           "bool"),
    ("pos1-posopt6",    "positioning.corrections", "iono_compensation",  "enum"),  # off/ssr/meas
    ("pos1-posopt7",    "positioning.corrections", "partial_ar",         "bool"),
    ("pos1-posopt8",    "positioning.corrections", "shapiro_delay",      "bool"),
    ("pos1-posopt9",    "positioning.corrections", "exclude_qzs_ref",   "bool"),
    ("pos1-posopt10",   "positioning.corrections", "no_phase_bias_adj",  "bool"),
    ("pos1-posopt11",   "positioning.corrections", "gps_frequency",     "enum"),
    ("pos1-posopt12",   "positioning.corrections", "reserved",          "bool"),
    ("pos1-posopt13",   "positioning.corrections", "qzs_frequency",     "enum"),

    # ── positioning.atmosphere ───────────────────────────────────────────────
    ("pos1-ionoopt",    "positioning.atmosphere", "ionosphere",       "enum"),
    ("pos1-tropopt",    "positioning.atmosphere", "troposphere",      "enum"),
    ("pos1-tidecorr",   "positioning.corrections", "tidal_correction", "enum"),

    # ── ambiguity_resolution ─────────────────────────────────────────────────
    ("pos2-armode",     "ambiguity_resolution", "mode",         "enum"),
    ("pos2-gpsarmode",  "ambiguity_resolution", "gps_ar",       "bool"),
    ("pos2-gloarmode",  "ambiguity_resolution", "glonass_ar",   "enum"),
    ("pos2-bdsarmode",  "ambiguity_resolution", "bds_ar",       "bool"),
    ("pos2-qzsarmode",  "ambiguity_resolution", "qzs_ar",       "bool"),
    ("pos2-arsys",      "ambiguity_resolution", "systems",      "int"),

    # ── ambiguity_resolution.thresholds ──────────────────────────────────────
    ("pos2-arthres",    "ambiguity_resolution.thresholds", "ratio",          "float"),
    ("pos2-arthres1",   "ambiguity_resolution.thresholds", "ratio1",         "float"),
    ("pos2-arthres2",   "ambiguity_resolution.thresholds", "ratio2",         "float"),
    ("pos2-arthres3",   "ambiguity_resolution.thresholds", "ratio3",         "float"),
    ("pos2-arthres4",   "ambiguity_resolution.thresholds", "ratio4",         "float"),
    ("pos2-arthres5",   "ambiguity_resolution.thresholds", "ratio5",         "float"),
    ("pos2-arthres6",   "ambiguity_resolution.thresholds", "ratio6",         "float"),
    ("pos2-aralpha",    "ambiguity_resolution.thresholds", "alpha",          "enum"),
    ("pos2-arelmask",   "ambiguity_resolution.thresholds", "elevation_mask", "float"),
    ("pos2-elmaskhold", "ambiguity_resolution.thresholds", "hold_elevation", "float"),

    # ── ambiguity_resolution.counters ────────────────────────────────────────
    ("pos2-arlockcnt",  "ambiguity_resolution.counters", "lock_count",     "int"),
    ("pos2-arminfix",   "ambiguity_resolution.counters", "min_fix",        "int"),
    ("pos2-armaxiter",  "ambiguity_resolution.counters", "max_iterations", "int"),
    ("pos2-aroutcnt",   "ambiguity_resolution.counters", "out_count",      "int"),

    # ── ambiguity_resolution.partial_ar ──────────────────────────────────────
    ("pos2-arminamb",       "ambiguity_resolution.partial_ar", "min_ambiguities",   "int"),
    ("pos2-armaxdelsat",    "ambiguity_resolution.partial_ar", "max_excluded_sats", "int"),
    ("pos2-arminfixsats",   "ambiguity_resolution.partial_ar", "min_fix_sats",      "int"),
    ("pos2-armindropsats",  "ambiguity_resolution.partial_ar", "min_drop_sats",     "int"),
    ("pos2-arminholdsats",  "ambiguity_resolution.partial_ar", "min_hold_sats",     "int"),
    ("pos2-arfilter",       "ambiguity_resolution.partial_ar", "ar_filter",         "bool"),

    # ── ambiguity_resolution.hold ────────────────────────────────────────────
    ("pos2-varholdamb",  "ambiguity_resolution.hold", "variance",  "float"),
    ("pos2-gainholdamb", "ambiguity_resolution.hold", "gain",      "float"),

    # ── rejection ────────────────────────────────────────────────────────────
    ("pos2-rejionno",   "rejection", "innovation",          "float"),
    ("pos2-rejionno1",  "rejection", "l1_l2_residual",      "float"),
    ("pos2-rejionno2",  "rejection", "dispersive",          "float"),
    ("pos2-rejionno3",  "rejection", "non_dispersive",      "float"),
    ("pos2-rejionno4",  "rejection", "hold_chi_square",     "float"),
    ("pos2-rejionno5",  "rejection", "fix_chi_square",      "float"),
    ("pos2-rejgdop",    "rejection", "gdop",                "float"),
    ("pos2-rejdiffpse", "rejection", "pseudorange_diff",    "float"),
    ("pos2-poserrcnt",  "rejection", "position_error_count", "int"),

    # ── slip_detection ───────────────────────────────────────────────────────
    ("pos2-slipthres",  "slip_detection", "threshold",  "float"),
    ("pos2-thresdop",   "slip_detection", "doppler",    "float"),

    # ── kalman_filter ────────────────────────────────────────────────────────
    ("pos2-niter",      "kalman_filter", "iterations",  "int"),
    ("pos2-syncsol",    "kalman_filter", "sync_solution", "bool"),

    # ── kalman_filter.measurement_error ──────────────────────────────────────
    ("stats-eratio1",    "kalman_filter.measurement_error", "code_phase_ratio_L1", "float"),
    ("stats-eratio2",    "kalman_filter.measurement_error", "code_phase_ratio_L2", "float"),
    ("stats-eratio3",    "kalman_filter.measurement_error", "code_phase_ratio_L5", "float"),
    ("stats-errphase",   "kalman_filter.measurement_error", "phase",               "float"),
    ("stats-errphaseel", "kalman_filter.measurement_error", "phase_elevation",     "float"),
    ("stats-errphasebl", "kalman_filter.measurement_error", "phase_baseline",      "float"),
    ("stats-errdoppler", "kalman_filter.measurement_error", "doppler",             "float"),
    ("stats-uraratio",   "kalman_filter.measurement_error", "ura_ratio",           "float"),

    # ── kalman_filter.initial_std ────────────────────────────────────────────
    ("stats-stdbias",   "kalman_filter.initial_std", "bias",        "float"),
    ("stats-stdiono",   "kalman_filter.initial_std", "ionosphere",  "float"),
    ("stats-stdtrop",   "kalman_filter.initial_std", "troposphere", "float"),

    # ── kalman_filter.process_noise ──────────────────────────────────────────
    ("stats-prnbias",      "kalman_filter.process_noise", "bias",             "float"),
    ("stats-prniono",      "kalman_filter.process_noise", "ionosphere",       "float"),
    ("stats-prnionomax",   "kalman_filter.process_noise", "iono_max",         "float"),
    ("stats-prntrop",      "kalman_filter.process_noise", "troposphere",      "float"),
    ("stats-prnaccelh",    "kalman_filter.process_noise", "accel_h",          "float"),
    ("stats-prnaccelv",    "kalman_filter.process_noise", "accel_v",          "float"),
    ("stats-prnposith",    "kalman_filter.process_noise", "position_h",       "float"),
    ("stats-prnpositv",    "kalman_filter.process_noise", "position_v",       "float"),
    ("stats-prnpos",       "kalman_filter.process_noise", "position",         "float"),
    ("stats-prnifb",       "kalman_filter.process_noise", "ifb",              "float"),
    ("stats-prndcb",       "kalman_filter.process_noise", "ifb",              "float"),  # alias
    ("stats-tconstiono",   "kalman_filter.process_noise", "iono_time_const",  "float"),
    ("stats-clkstab",      "kalman_filter.process_noise", "clock_stability",  "float"),

    # ── adaptive_filter ──────────────────────────────────────────────────────
    ("pos2-prnadpt",    "adaptive_filter", "enabled",          "bool"),
    ("pos2-forgetion",  "adaptive_filter", "iono_forgetting",  "float"),
    ("pos2-afgainion",  "adaptive_filter", "iono_gain",        "float"),
    ("pos2-forgetpva",  "adaptive_filter", "pva_forgetting",   "float"),
    ("pos2-afgainpva",  "adaptive_filter", "pva_gain",         "float"),

    # ── signals ──────────────────────────────────────────────────────────────
    ("pos2-siggps",     "signals", "gps",      "enum"),
    ("pos2-sigqzs",     "signals", "qzs",      "enum"),
    ("pos2-siggal",     "signals", "galileo",  "enum"),
    ("pos2-sigbds2",    "signals", "bds2",     "enum"),
    ("pos2-sigbds3",    "signals", "bds3",     "enum"),

    # ── receiver ─────────────────────────────────────────────────────────────
    ("pos2-ionocorr",   "receiver", "iono_correction",  "bool"),
    ("pos2-ign_chierr", "receiver", "ignore_chi_error", "bool"),
    ("pos2-bds2bias",   "receiver", "bds2_bias",        "bool"),
    ("pos2-pppsatcb",   "receiver", "ppp_sat_clock_bias", "int"),
    ("pos2-pppsatpb",   "receiver", "ppp_sat_phase_bias", "int"),
    ("pos2-uncorrbias", "receiver", "uncorr_bias",      "int"),
    ("pos2-maxbiasdt",  "receiver", "max_bias_dt",      "int"),
    ("pos2-sattmode",   "receiver", "satellite_mode",   "int"),
    ("pos2-phasshft",   "receiver", "phase_shift",      "enum"),
    ("pos2-isb",        "receiver", "isb",              "bool"),
    ("pos2-rectype",    "receiver", "reference_type",   "str"),
    ("pos2-maxage",     "receiver", "max_age",          "float"),
    ("pos2-baselen",    "receiver", "baseline_length",  "float"),
    ("pos2-basesig",    "receiver", "baseline_sigma",   "float"),

    # ── antenna.rover ────────────────────────────────────────────────────────
    ("ant1-postype",    "antenna.rover", "position_type",  "enum"),
    ("ant1-pos1",       "antenna.rover", "position_1",     "float"),
    ("ant1-pos2",       "antenna.rover", "position_2",     "float"),
    ("ant1-pos3",       "antenna.rover", "position_3",     "float"),
    ("ant1-anttype",    "antenna.rover", "type",           "str"),
    ("ant1-antdele",    "antenna.rover", "delta_e",        "float"),
    ("ant1-antdeln",    "antenna.rover", "delta_n",        "float"),
    ("ant1-antdelu",    "antenna.rover", "delta_u",        "float"),

    # ── antenna.base ─────────────────────────────────────────────────────────
    ("ant2-postype",    "antenna.base", "position_type",  "enum"),
    ("ant2-pos1",       "antenna.base", "position_1",     "float"),
    ("ant2-pos2",       "antenna.base", "position_2",     "float"),
    ("ant2-pos3",       "antenna.base", "position_3",     "float"),
    ("ant2-anttype",    "antenna.base", "type",           "str"),
    ("ant2-antdele",    "antenna.base", "delta_e",        "float"),
    ("ant2-antdeln",    "antenna.base", "delta_n",        "float"),
    ("ant2-antdelu",    "antenna.base", "delta_u",        "float"),
    ("ant2-maxaveep",   "antenna.base", "max_average_epochs", "int"),
    ("ant2-initrst",    "antenna.base", "init_reset",     "bool"),

    # ── output ───────────────────────────────────────────────────────────────
    ("out-solformat",   "output", "format",             "enum"),
    ("out-outhead",     "output", "header",             "bool"),
    ("out-outopt",      "output", "options",            "bool"),
    ("out-outvel",      "output", "velocity",           "bool"),
    ("out-timesys",     "output", "time_system",        "enum"),
    ("out-timeform",    "output", "time_format",        "enum"),
    ("out-timendec",    "output", "time_decimals",      "int"),
    ("out-degform",     "output", "coordinate_format",  "enum"),
    ("out-fieldsep",    "output", "field_separator",    "str"),
    ("out-outsingle",   "output", "single_output",      "bool"),
    ("out-maxsolstd",   "output", "max_solution_std",   "float"),
    ("out-height",      "output", "height_type",        "enum"),
    ("out-geoid",       "output", "geoid_model",        "enum"),
    ("out-solstatic",   "output", "static_solution",    "enum"),
    ("out-nmeaintv1",   "output", "nmea_interval_1",    "float"),
    ("out-nmeaintv2",   "output", "nmea_interval_2",    "float"),
    ("out-outstat",     "output", "solution_status",    "enum"),

    # ── files ────────────────────────────────────────────────────────────────
    ("file-satantfile",   "files", "satellite_atx",  "str"),
    ("file-rcvantfile",   "files", "receiver_atx",   "str"),
    ("file-staposfile",   "files", "station_pos",    "str"),
    ("file-geoidfile",    "files", "geoid",          "str"),
    ("file-ionofile",     "files", "ionosphere",     "str"),
    ("file-dcbfile",      "files", "dcb",            "str"),
    ("file-eopfile",      "files", "eop",            "str"),
    ("file-blqfile",      "files", "ocean_loading",  "str"),
    ("file-elmaskfile",   "files", "elevation_mask_file", "str"),
    ("file-tempdir",      "files", "temp_dir",       "str"),
    ("file-geexefile",    "files", "geexe",          "str"),
    ("file-solstatfile",  "files", "solution_stat",  "str"),
    ("file-tracefile",    "files", "trace",          "str"),
    ("file-fcbfile",      "files", "fcb",            "str"),
    ("file-biafile",      "files", "bias_sinex",     "str"),
    ("file-cssrgridfile", "files", "cssr_grid",      "str"),
    ("file-isbfile",      "files", "isb_table",      "str"),
    ("file-phacycfile",   "files", "phase_cycle",    "str"),

    # ── server (rtkrcv misc) ─────────────────────────────────────────────────
    ("misc-svrcycle",    "server", "cycle_ms",          "int"),
    ("misc-timeout",     "server", "timeout_ms",        "int"),
    ("misc-reconnect",   "server", "reconnect_ms",      "int"),
    ("misc-nmeacycle",   "server", "nmea_cycle_ms",     "int"),
    ("misc-buffsize",    "server", "buffer_size",       "int"),
    ("misc-navmsgsel",   "server", "nav_msg_select",    "str"),
    ("misc-proxyaddr",   "server", "proxy",             "str"),
    ("misc-fswapmargin", "server", "swap_margin",       "int"),
    ("misc-timeinterp",  "server", "time_interpolation", "bool"),
    ("misc-sbasatsel",   "server", "sbas_satellite",    "str"),
    ("misc-maxobsloss",  "server", "max_obs_loss",      "float"),
    ("misc-floatcnt",    "server", "float_count",       "int"),
    ("misc-rnxopt1",     "server", "rinex_option_1",    "str"),
    ("misc-rnxopt2",     "server", "rinex_option_2",    "str"),
    ("misc-pppopt",      "server", "ppp_option",        "str"),
    ("misc-rtcmopt",     "server", "rtcm_option",       "str"),
    ("misc-l6mrg",       "server", "l6_margin",         "int"),
    ("misc-regularly",   "server", "regularly",         "int"),
    ("misc-startcmd",   "server", "start_cmd",         "str"),
    ("misc-stopcmd",    "server", "stop_cmd",          "str"),

    # ── files (rtkrcv-specific) ──────────────────────────────────────────────
    ("file-cmdfile1",   "files", "cmd_file_1",        "str"),
    ("file-cmdfile2",   "files", "cmd_file_2",        "str"),
    ("file-cmdfile3",   "files", "cmd_file_3",        "str"),
]

# Build lookup dict: legacy_key → (section, key, type)
_LEGACY_MAP: dict[str, tuple[str, str, str]] = {}
for _lk, _sec, _tk, _vt in MAPPING:
    _LEGACY_MAP[_lk] = (_sec, _tk, _vt)

# ── rtkrcv stream keys (not in sysopts[], parsed separately) ─────────────────

STREAM_KEY_RE = re.compile(
    r"^(inpstr|outstr|logstr)(\d+)-(type|path|format|nmeareq|nmealat|nmealon|nmeahgt)$"
)

CONSOLE_KEY_RE = re.compile(r"^console-(passwd|timetype|soltype|solflag)$")

# ═══════════════════════════════════════════════════════════════════════════════
# Value conversion
# ═══════════════════════════════════════════════════════════════════════════════

# Enum definitions from mrtk_options.c — used to resolve numeric enum values
ENUM_DEFS: dict[str, dict[int, str]] = {}

_ENUM_STRINGS = {
    "MODOPT":  "0:single,1:dgps,2:kinematic,3:static,4:movingbase,5:fixed,6:ppp-kine,7:ppp-static,8:ppp-fixed,9:ppp-rtk,10:ssr2osr,11:ssr2osr-fixed,12:vrs-rtk",
    "FRQOPT":  "1:l1,2:l1+2,3:l1+2+3,4:l1+2+3+4,5:l1+2+3+4+5",
    "FRQOPT2": "1:l1,2:l1+l2,3:l1+l5,4:l1+l2+l5,5:l1+l5(l2)",
    "TYPOPT":  "0:forward,1:backward,2:combined",
    "IONOPT":  "0:off,1:brdc,2:sbas,3:dual-freq,4:est-stec,5:ionex-tec,6:qzs-brdc,9:est-adaptive",
    "TRPOPT":  "0:off,1:saas,2:sbas,3:est-ztd,4:est-ztdgrad",
    "EPHOPT":  "0:brdc,1:precise,2:brdc+sbas,3:brdc+ssrapc,4:brdc+ssrcom",
    "SWTOPT":  "0:off,1:on",
    "SOLOPT":  "0:llh,1:xyz,2:enu,3:nmea",
    "TSYOPT":  "0:gpst,1:utc,2:jst",
    "TFTOPT":  "0:tow,1:hms",
    "DFTOPT":  "0:deg,1:dms",
    "HGTOPT":  "0:ellipsoidal,1:geodetic",
    "GEOOPT":  "0:internal,1:egm96,2:egm08_2.5,3:egm08_1,4:gsi2000",
    "STAOPT":  "0:all,1:single",
    "STSOPT":  "0:off,1:state,2:residual",
    "ARMOPT":  "0:off,1:continuous,2:instantaneous,3:fix-and-hold",
    "POSOPT":  "0:llh,1:xyz,2:single,3:posfile,4:rinexhead,5:rtcm,6:raw",
    "TIDEOPT": "0:off,1:on,2:otl,3:solid+otl-clasgrid+pole",
    "PHWOPT":  "0:off,1:on,2:precise",
    "COMOPT":  "0:off,1:ssr,2:meas",
    "PSHFTOPT":"0:off,1:table",
    "GAROPT":  "0:off,1:on",
    "SIGOPT1": "0:L1/L2,1:L1/L5,2:L1/L2/L5",
    "SIGOPT2": "0:L1/L5,1:L1/L2,2:L1/L5/L2",
    "SIGOPT3": "0:E1/E5a,1:E1/E5b,2:E1/E6,3:E1/E5a/E5b/E6,4:E1/E5a/E6/E5b",
    "SIGOPT4": "0:B1I/B3I,1:B1I/B2I,2:B1I/B3I/B2I",
    "SIGOPT5": "0:B1I/B3I,1:B1I/B2a,2:B1I/B3I/B2a",
    "ARALPHAOPT": "0:0.1%,1:0.5%,2:1%,3:5%,4:10%,5:20%",
}

for _name, _defstr in _ENUM_STRINGS.items():
    _d: dict[int, str] = {}
    for _part in _defstr.split(","):
        _k, _, _v = _part.partition(":")
        try:
            _d[int(_k)] = _v
        except ValueError:
            pass
    ENUM_DEFS[_name] = _d

# Map legacy keys to their enum definition name
_KEY_ENUM: dict[str, str] = {
    "pos1-posmode": "MODOPT", "pos1-frequency": "FRQOPT", "pos1-soltype": "TYPOPT",
    "pos1-ionoopt": "IONOPT", "pos1-tropopt": "TRPOPT", "pos1-sateph": "EPHOPT",
    "pos1-tidecorr": "TIDEOPT",
    "pos1-snrmask_r": "SWTOPT", "pos1-snrmask_b": "SWTOPT",
    "pos1-dynamics": "SWTOPT",
    "pos1-posopt1": "SWTOPT", "pos1-posopt2": "SWTOPT",
    "pos1-posopt3": "PHWOPT", "pos1-posopt4": "SWTOPT",
    "pos1-posopt5": "SWTOPT", "pos1-posopt6": "COMOPT",
    "pos1-posopt7": "SWTOPT", "pos1-posopt8": "SWTOPT",
    "pos1-posopt9": "SWTOPT", "pos1-posopt10": "SWTOPT",
    "pos1-posopt11": "FRQOPT2", "pos1-posopt12": "SWTOPT",
    "pos1-posopt13": "FRQOPT2",
    "pos2-armode": "ARMOPT",
    "pos2-gloarmode": "GAROPT", "pos2-bdsarmode": "SWTOPT",
    "pos2-qzsarmode": "SWTOPT", "pos2-gpsarmode": "SWTOPT",
    "pos2-aralpha": "ARALPHAOPT",
    "pos2-syncsol": "SWTOPT", "pos2-arfilter": "SWTOPT",
    "pos2-ionocorr": "SWTOPT", "pos2-ign_chierr": "SWTOPT",
    "pos2-bds2bias": "SWTOPT", "pos2-prnadpt": "SWTOPT",
    "pos2-phasshft": "PSHFTOPT", "pos2-isb": "SWTOPT",
    "pos2-siggps": "SIGOPT1", "pos2-sigqzs": "SIGOPT2",
    "pos2-siggal": "SIGOPT3", "pos2-sigbds2": "SIGOPT4",
    "pos2-sigbds3": "SIGOPT5",
    "out-solformat": "SOLOPT", "out-outhead": "SWTOPT",
    "out-outopt": "SWTOPT", "out-outvel": "SWTOPT",
    "out-timesys": "TSYOPT", "out-timeform": "TFTOPT",
    "out-degform": "DFTOPT", "out-outsingle": "SWTOPT",
    "out-height": "HGTOPT", "out-geoid": "GEOOPT",
    "out-solstatic": "STAOPT", "out-outstat": "STSOPT",
    "ant1-postype": "POSOPT", "ant2-postype": "POSOPT",
    "ant2-initrst": "SWTOPT",
    "misc-timeinterp": "SWTOPT",
}


def _resolve_enum(key: str, raw: str) -> str:
    """If raw is a bare integer and key has an enum def, resolve to string name."""
    if key not in _KEY_ENUM:
        return raw
    try:
        ival = int(raw)
    except ValueError:
        return raw
    enum_name = _KEY_ENUM[key]
    if enum_name in ENUM_DEFS and ival in ENUM_DEFS[enum_name]:
        return ENUM_DEFS[enum_name][ival]
    return raw


def convert_value(raw: str, vtype: str, legacy_key: str = "") -> Any:
    """Convert a raw conf value string to a Python/TOML-compatible value."""
    raw = raw.strip()
    if vtype == "bool":
        # Resolve numeric enum values first
        resolved = _resolve_enum(legacy_key, raw) if legacy_key else raw
        if resolved.lower() in ("on", "1", "true"):
            return True
        if resolved.lower() in ("off", "0", "false"):
            return False
        return raw
    if vtype == "int":
        try:
            return int(raw)
        except ValueError:
            return raw
    if vtype == "float":
        try:
            return float(raw)
        except ValueError:
            return raw
    if vtype == "siglist":
        # Comma-separated signal codes → list of strings
        parts = [s.strip() for s in raw.split(",") if s.strip()]
        return parts if parts else []
    if vtype == "snr":
        parts = [s.strip() for s in raw.split(",") if s.strip()]
        return [float(x) if "." in x else int(x) for x in parts]
    if vtype == "navsys":
        try:
            val = int(raw)
        except ValueError:
            return raw
        # Convert bitmask to list of constellation names
        names = [(1, "GPS"), (2, "SBAS"), (4, "GLONASS"), (8, "Galileo"),
                 (16, "QZSS"), (32, "BeiDou"), (64, "NavIC")]
        return [name for bit, name in names if val & bit]
    if vtype == "enum":
        # Resolve pure-integer values to their string names
        resolved = _resolve_enum(legacy_key, raw) if legacy_key else raw
        return resolved
    # str
    return raw


# ═══════════════════════════════════════════════════════════════════════════════
# TOML writer
# ═══════════════════════════════════════════════════════════════════════════════

def _toml_value(val: Any) -> str:
    """Format a Python value as a TOML value string."""
    if isinstance(val, bool):
        return "true" if val else "false"
    if isinstance(val, int):
        return str(val)
    if isinstance(val, float):
        # Use scientific notation for very small numbers
        if val != 0.0 and abs(val) < 0.001:
            return f"{val:.2e}"
        # Avoid unnecessary trailing zeros but keep at least one decimal
        s = f"{val:.15g}"
        if "." not in s and "e" not in s.lower():
            s += ".0"
        return s
    if isinstance(val, list):
        items = ", ".join(_toml_value(v) for v in val)
        return f"[{items}]"
    # String — quote it
    return f'"{val}"'



def write_toml(sections: dict[str, OrderedDict], out_path: Path,
               source_name: str = "") -> None:
    """Write a nested section dict as a TOML file."""
    lines: list[str] = []
    lines.append("# MRTKLIB Configuration (TOML v1.0.0)")
    if source_name:
        lines.append(f"# Converted from: {source_name}")
    lines.append("")

    # Determine section order: follow the order in MAPPING
    seen_sections: list[str] = []
    for _, sec, _, _ in MAPPING:
        if sec not in seen_sections:
            seen_sections.append(sec)
    # Add any extra sections (streams, console) at the end
    for sec in sections:
        if sec not in seen_sections:
            seen_sections.append(sec)

    for section in seen_sections:
        if section not in sections:
            continue
        items = sections[section]
        if not items:
            continue

        lines.append(f"[{section}]")
        for key, (val, comment) in items.items():
            val_str = _toml_value(val)
            line = f"{key} = {val_str}"
            if comment:
                line += f"  # {comment}"
            lines.append(line)
        lines.append("")

    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


# ═══════════════════════════════════════════════════════════════════════════════
# Parser
# ═══════════════════════════════════════════════════════════════════════════════

def parse_conf(path: Path) -> dict[str, OrderedDict]:
    """Parse a .conf file and return TOML sections dict.

    Returns: {section_name: OrderedDict{key: (value, comment)}}
    """
    sections: dict[str, OrderedDict] = {}

    def _ensure_section(sec: str) -> OrderedDict:
        if sec not in sections:
            sections[sec] = OrderedDict()
        return sections[sec]

    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n\r")

            # Strip inline comment (but preserve it for output)
            comment = ""
            m = re.match(r"^([^#]*?)\s*#\s*(.*)$", line)
            if m:
                line_data = m.group(1)
                comment = m.group(2).strip()
            else:
                line_data = line

            line_data = line_data.strip()
            if not line_data:
                continue

            # Parse key=value
            if "=" not in line_data:
                continue
            key, _, raw_val = line_data.partition("=")
            key = key.strip()
            raw_val = raw_val.strip()

            # Check stream keys (rtkrcv)
            sm = STREAM_KEY_RE.match(key)
            if sm:
                prefix, idx, field = sm.group(1), sm.group(2), sm.group(3)
                # Map to TOML section
                if prefix == "inpstr":
                    stream_names = {
                        "1": "streams.input.rover",
                        "2": "streams.input.base",
                        "3": "streams.input.correction",
                    }
                    sec = stream_names.get(idx, f"streams.input.stream{idx}")
                elif prefix == "outstr":
                    sec = f"streams.output.stream{idx}"
                else:  # logstr
                    sec = f"streams.log.stream{idx}"

                sect = _ensure_section(sec)
                # Convert on/off for type field
                if field == "type" and raw_val.lower() == "off":
                    sect[field] = ("off", comment)
                elif field in ("nmealat", "nmealon"):
                    try:
                        sect[field] = (float(raw_val), comment)
                    except ValueError:
                        sect[field] = (raw_val, comment)
                elif field == "nmeareq":
                    sect[field] = (raw_val.lower() in ("on", "1"), comment)
                else:
                    sect[field] = (raw_val, comment)
                continue

            # Check console keys
            cm = CONSOLE_KEY_RE.match(key)
            if cm:
                field = cm.group(1)
                sect = _ensure_section("console")
                sect[field] = (raw_val, comment)
                continue

            # Standard sysopts key
            if key in _LEGACY_MAP:
                sec, tkey, vtype = _LEGACY_MAP[key]
                val = convert_value(raw_val, vtype, legacy_key=key)
                sect = _ensure_section(sec)

                # Skip duplicate alias (prndcb → prnifb)
                if key == "stats-prndcb" and "ifb" in sect:
                    continue

                sect[tkey] = (val, comment)
            else:
                # Unknown key — put in [unknown] section
                sect = _ensure_section("unknown")
                sect[key] = (raw_val, comment)

    return sections


# ═══════════════════════════════════════════════════════════════════════════════
# CLI
# ═══════════════════════════════════════════════════════════════════════════════

def convert_file(conf_path: Path, out_path: Path | None = None) -> Path:
    """Convert a single .conf file to .toml."""
    if out_path is None:
        out_path = conf_path.with_suffix(".toml")

    sections = parse_conf(conf_path)
    write_toml(sections, out_path, source_name=conf_path.name)
    return out_path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert MRTKLIB legacy .conf files to TOML format."
    )
    parser.add_argument("input", nargs="+", type=Path, help=".conf file(s)")
    parser.add_argument("-o", "--output", type=Path, default=None,
                        help="output .toml file (single input only)")
    args = parser.parse_args()

    if args.output and len(args.input) > 1:
        print("ERROR: -o can only be used with a single input file", file=sys.stderr)
        return 1

    for conf_path in args.input:
        if not conf_path.exists():
            print(f"ERROR: {conf_path} not found", file=sys.stderr)
            return 1

        out = convert_file(conf_path, args.output)
        print(f"  {conf_path} → {out}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
