/*------------------------------------------------------------------------------
 * mrtk_toml.c : TOML configuration file loader for MRTKLIB
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Maps TOML semantic section.key paths → legacy sysopts[] option names,
 * then uses the existing str2opt() infrastructure to set values.
 *----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_toml.h"
#include "mrtklib/mrtk_trace.h"
#include "tomlc99/toml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── TOML path → legacy option name mapping ────────────────────────────────── */

typedef struct {
    const char *toml_section;   /* e.g. "positioning" */
    const char *toml_key;       /* e.g. "mode" */
    const char *legacy_name;    /* e.g. "pos1-posmode" */
} toml_map_t;

static const toml_map_t toml_mapping[] = {
    /* ── positioning ───────────────────────────────────────────────────────── */
    {"positioning", "mode",             "pos1-posmode"},
    {"positioning", "frequency",        "pos1-frequency"},
    {"positioning", "solution_type",    "pos1-soltype"},
    {"positioning", "elevation_mask",   "pos1-elmask"},
    {"positioning", "dynamics",         "pos1-dynamics"},
    {"positioning", "satellite_ephemeris", "pos1-sateph"},
    {"positioning", "constellations",   "pos1-navsys"},
    {"positioning", "excluded_sats",    "pos1-exclsats"},

    /* ── positioning.clas ──────────────────────────────────────────────────── */
    {"positioning.clas", "grid_selection_radius",  "pos1-gridsel"},
    {"positioning.clas", "receiver_type",          "pos1-rectype"},
    {"positioning.clas", "position_uncertainty_x",  "pos1-rux"},
    {"positioning.clas", "position_uncertainty_y",  "pos1-ruy"},
    {"positioning.clas", "position_uncertainty_z",  "pos1-ruz"},

    /* ── positioning.snr_mask ──────────────────────────────────────────────── */
    {"positioning.snr_mask", "rover_enabled",  "pos1-snrmask_r"},
    {"positioning.snr_mask", "base_enabled",   "pos1-snrmask_b"},
    {"positioning.snr_mask", "L1",             "pos1-snrmask_L1"},
    {"positioning.snr_mask", "L2",             "pos1-snrmask_L2"},
    {"positioning.snr_mask", "L5",             "pos1-snrmask_L5"},

    /* ── positioning.corrections ───────────────────────────────────────────── */
    {"positioning.corrections", "satellite_antenna",  "pos1-posopt1"},
    {"positioning.corrections", "receiver_antenna",   "pos1-posopt2"},
    {"positioning.corrections", "phase_windup",       "pos1-posopt3"},
    {"positioning.corrections", "exclude_eclipse",    "pos1-posopt4"},
    {"positioning.corrections", "raim_fde",           "pos1-posopt5"},
    {"positioning.corrections", "iono_compensation",  "pos1-posopt6"},
    {"positioning.corrections", "partial_ar",         "pos1-posopt7"},
    {"positioning.corrections", "shapiro_delay",      "pos1-posopt8"},
    {"positioning.corrections", "exclude_qzs_ref",   "pos1-posopt9"},
    {"positioning.corrections", "no_phase_bias_adj",  "pos1-posopt10"},
    {"positioning.corrections", "gps_frequency",      "pos1-posopt11"},
    {"positioning.corrections", "reserved",           "pos1-posopt12"},
    {"positioning.corrections", "qzs_frequency",      "pos1-posopt13"},

    /* ── positioning.atmosphere ─────────────────────────────────────────────── */
    {"positioning.atmosphere", "ionosphere",       "pos1-ionoopt"},
    {"positioning.atmosphere", "troposphere",      "pos1-tropopt"},
    {"positioning.atmosphere", "tidal_correction", "pos1-tidecorr"},

    /* ── ambiguity_resolution ──────────────────────────────────────────────── */
    {"ambiguity_resolution", "mode",        "pos2-armode"},
    {"ambiguity_resolution", "gps_ar",      "pos2-gpsarmode"},
    {"ambiguity_resolution", "glonass_ar",  "pos2-gloarmode"},
    {"ambiguity_resolution", "bds_ar",      "pos2-bdsarmode"},
    {"ambiguity_resolution", "qzs_ar",      "pos2-qzsarmode"},
    {"ambiguity_resolution", "systems",     "pos2-arsys"},

    /* ── ambiguity_resolution.thresholds ───────────────────────────────────── */
    {"ambiguity_resolution.thresholds", "ratio",          "pos2-arthres"},
    {"ambiguity_resolution.thresholds", "ratio1",         "pos2-arthres1"},
    {"ambiguity_resolution.thresholds", "ratio2",         "pos2-arthres2"},
    {"ambiguity_resolution.thresholds", "ratio3",         "pos2-arthres3"},
    {"ambiguity_resolution.thresholds", "ratio4",         "pos2-arthres4"},
    {"ambiguity_resolution.thresholds", "ratio5",         "pos2-arthres5"},
    {"ambiguity_resolution.thresholds", "ratio6",         "pos2-arthres6"},
    {"ambiguity_resolution.thresholds", "alpha",          "pos2-aralpha"},
    {"ambiguity_resolution.thresholds", "elevation_mask", "pos2-arelmask"},
    {"ambiguity_resolution.thresholds", "hold_elevation", "pos2-elmaskhold"},

    /* ── ambiguity_resolution.counters ─────────────────────────────────────── */
    {"ambiguity_resolution.counters", "lock_count",     "pos2-arlockcnt"},
    {"ambiguity_resolution.counters", "min_fix",        "pos2-arminfix"},
    {"ambiguity_resolution.counters", "max_iterations", "pos2-armaxiter"},
    {"ambiguity_resolution.counters", "out_count",      "pos2-aroutcnt"},

    /* ── ambiguity_resolution.partial_ar ───────────────────────────────────── */
    {"ambiguity_resolution.partial_ar", "min_ambiguities",   "pos2-arminamb"},
    {"ambiguity_resolution.partial_ar", "max_excluded_sats", "pos2-armaxdelsat"},
    {"ambiguity_resolution.partial_ar", "min_fix_sats",      "pos2-arminfixsats"},
    {"ambiguity_resolution.partial_ar", "min_drop_sats",     "pos2-armindropsats"},
    {"ambiguity_resolution.partial_ar", "min_hold_sats",     "pos2-arminholdsats"},
    {"ambiguity_resolution.partial_ar", "ar_filter",         "pos2-arfilter"},

    /* ── ambiguity_resolution.hold ─────────────────────────────────────────── */
    {"ambiguity_resolution.hold", "variance", "pos2-varholdamb"},
    {"ambiguity_resolution.hold", "gain",     "pos2-gainholdamb"},

    /* ── rejection ─────────────────────────────────────────────────────────── */
    {"rejection", "innovation",          "pos2-rejionno"},
    {"rejection", "l1_l2_residual",      "pos2-rejionno1"},
    {"rejection", "dispersive",          "pos2-rejionno2"},
    {"rejection", "non_dispersive",      "pos2-rejionno3"},
    {"rejection", "hold_chi_square",     "pos2-rejionno4"},
    {"rejection", "fix_chi_square",      "pos2-rejionno5"},
    {"rejection", "gdop",               "pos2-rejgdop"},
    {"rejection", "pseudorange_diff",    "pos2-rejdiffpse"},
    {"rejection", "position_error_count","pos2-poserrcnt"},

    /* ── slip_detection ────────────────────────────────────────────────────── */
    {"slip_detection", "threshold", "pos2-slipthres"},
    {"slip_detection", "doppler",   "pos2-thresdop"},

    /* ── kalman_filter ─────────────────────────────────────────────────────── */
    {"kalman_filter", "iterations",    "pos2-niter"},
    {"kalman_filter", "sync_solution", "pos2-syncsol"},

    /* ── kalman_filter.measurement_error ───────────────────────────────────── */
    {"kalman_filter.measurement_error", "code_phase_ratio_L1", "stats-eratio1"},
    {"kalman_filter.measurement_error", "code_phase_ratio_L2", "stats-eratio2"},
    {"kalman_filter.measurement_error", "code_phase_ratio_L5", "stats-eratio3"},
    {"kalman_filter.measurement_error", "phase",               "stats-errphase"},
    {"kalman_filter.measurement_error", "phase_elevation",     "stats-errphaseel"},
    {"kalman_filter.measurement_error", "phase_baseline",      "stats-errphasebl"},
    {"kalman_filter.measurement_error", "doppler",             "stats-errdoppler"},
    {"kalman_filter.measurement_error", "ura_ratio",           "stats-uraratio"},

    /* ── kalman_filter.initial_std ─────────────────────────────────────────── */
    {"kalman_filter.initial_std", "bias",        "stats-stdbias"},
    {"kalman_filter.initial_std", "ionosphere",  "stats-stdiono"},
    {"kalman_filter.initial_std", "troposphere", "stats-stdtrop"},

    /* ── kalman_filter.process_noise ───────────────────────────────────────── */
    {"kalman_filter.process_noise", "bias",             "stats-prnbias"},
    {"kalman_filter.process_noise", "ionosphere",       "stats-prniono"},
    {"kalman_filter.process_noise", "iono_max",         "stats-prnionomax"},
    {"kalman_filter.process_noise", "troposphere",      "stats-prntrop"},
    {"kalman_filter.process_noise", "accel_h",          "stats-prnaccelh"},
    {"kalman_filter.process_noise", "accel_v",          "stats-prnaccelv"},
    {"kalman_filter.process_noise", "position_h",       "stats-prnposith"},
    {"kalman_filter.process_noise", "position_v",       "stats-prnpositv"},
    {"kalman_filter.process_noise", "position",         "stats-prnpos"},
    {"kalman_filter.process_noise", "ifb",              "stats-prnifb"},
    {"kalman_filter.process_noise", "iono_time_const",  "stats-tconstiono"},
    {"kalman_filter.process_noise", "clock_stability",  "stats-clkstab"},

    /* ── adaptive_filter ───────────────────────────────────────────────────── */
    {"adaptive_filter", "enabled",         "pos2-prnadpt"},
    {"adaptive_filter", "iono_forgetting", "pos2-forgetion"},
    {"adaptive_filter", "iono_gain",       "pos2-afgainion"},
    {"adaptive_filter", "pva_forgetting",  "pos2-forgetpva"},
    {"adaptive_filter", "pva_gain",        "pos2-afgainpva"},

    /* ── signals ───────────────────────────────────────────────────────────── */
    {"signals", "gps",      "pos2-siggps"},
    {"signals", "qzs",      "pos2-sigqzs"},
    {"signals", "galileo",  "pos2-siggal"},
    {"signals", "bds2",     "pos2-sigbds2"},
    {"signals", "bds3",     "pos2-sigbds3"},

    /* ── receiver ──────────────────────────────────────────────────────────── */
    {"receiver", "iono_correction",    "pos2-ionocorr"},
    {"receiver", "ignore_chi_error",   "pos2-ign_chierr"},
    {"receiver", "bds2_bias",          "pos2-bds2bias"},
    {"receiver", "ppp_sat_clock_bias", "pos2-pppsatcb"},
    {"receiver", "ppp_sat_phase_bias", "pos2-pppsatpb"},
    {"receiver", "uncorr_bias",        "pos2-uncorrbias"},
    {"receiver", "max_bias_dt",        "pos2-maxbiasdt"},
    {"receiver", "satellite_mode",     "pos2-sattmode"},
    {"receiver", "phase_shift",        "pos2-phasshft"},
    {"receiver", "isb",                "pos2-isb"},
    {"receiver", "reference_type",     "pos2-rectype"},
    {"receiver", "max_age",            "pos2-maxage"},
    {"receiver", "baseline_length",    "pos2-baselen"},
    {"receiver", "baseline_sigma",     "pos2-basesig"},

    /* ── antenna.rover ─────────────────────────────────────────────────────── */
    {"antenna.rover", "position_type", "ant1-postype"},
    {"antenna.rover", "position_1",    "ant1-pos1"},
    {"antenna.rover", "position_2",    "ant1-pos2"},
    {"antenna.rover", "position_3",    "ant1-pos3"},
    {"antenna.rover", "type",          "ant1-anttype"},
    {"antenna.rover", "delta_e",       "ant1-antdele"},
    {"antenna.rover", "delta_n",       "ant1-antdeln"},
    {"antenna.rover", "delta_u",       "ant1-antdelu"},

    /* ── antenna.base ──────────────────────────────────────────────────────── */
    {"antenna.base", "position_type",      "ant2-postype"},
    {"antenna.base", "position_1",         "ant2-pos1"},
    {"antenna.base", "position_2",         "ant2-pos2"},
    {"antenna.base", "position_3",         "ant2-pos3"},
    {"antenna.base", "type",               "ant2-anttype"},
    {"antenna.base", "delta_e",            "ant2-antdele"},
    {"antenna.base", "delta_n",            "ant2-antdeln"},
    {"antenna.base", "delta_u",            "ant2-antdelu"},
    {"antenna.base", "max_average_epochs", "ant2-maxaveep"},
    {"antenna.base", "init_reset",         "ant2-initrst"},

    /* ── output ────────────────────────────────────────────────────────────── */
    {"output", "format",             "out-solformat"},
    {"output", "header",             "out-outhead"},
    {"output", "options",            "out-outopt"},
    {"output", "velocity",           "out-outvel"},
    {"output", "time_system",        "out-timesys"},
    {"output", "time_format",        "out-timeform"},
    {"output", "time_decimals",      "out-timendec"},
    {"output", "coordinate_format",  "out-degform"},
    {"output", "field_separator",    "out-fieldsep"},
    {"output", "single_output",      "out-outsingle"},
    {"output", "max_solution_std",   "out-maxsolstd"},
    {"output", "height_type",        "out-height"},
    {"output", "geoid_model",        "out-geoid"},
    {"output", "static_solution",    "out-solstatic"},
    {"output", "nmea_interval_1",    "out-nmeaintv1"},
    {"output", "nmea_interval_2",    "out-nmeaintv2"},
    {"output", "solution_status",    "out-outstat"},

    /* ── files ─────────────────────────────────────────────────────────────── */
    {"files", "satellite_atx",      "file-satantfile"},
    {"files", "receiver_atx",       "file-rcvantfile"},
    {"files", "station_pos",        "file-staposfile"},
    {"files", "geoid",              "file-geoidfile"},
    {"files", "ionosphere",         "file-ionofile"},
    {"files", "dcb",                "file-dcbfile"},
    {"files", "eop",                "file-eopfile"},
    {"files", "ocean_loading",      "file-blqfile"},
    {"files", "elevation_mask_file","file-elmaskfile"},
    {"files", "temp_dir",           "file-tempdir"},
    {"files", "geexe",              "file-geexefile"},
    {"files", "solution_stat",      "file-solstatfile"},
    {"files", "trace",              "file-tracefile"},
    {"files", "fcb",                "file-fcbfile"},
    {"files", "bias_sinex",         "file-biafile"},
    {"files", "cssr_grid",          "file-cssrgridfile"},
    {"files", "isb_table",          "file-isbfile"},
    {"files", "phase_cycle",        "file-phacycfile"},

    /* ── server (misc) ─────────────────────────────────────────────────────── */
    {"server", "cycle_ms",           "misc-svrcycle"},
    {"server", "timeout_ms",         "misc-timeout"},
    {"server", "reconnect_ms",       "misc-reconnect"},
    {"server", "nmea_cycle_ms",      "misc-nmeacycle"},
    {"server", "buffer_size",        "misc-buffsize"},
    {"server", "nav_msg_select",     "misc-navmsgsel"},
    {"server", "proxy",              "misc-proxyaddr"},
    {"server", "swap_margin",        "misc-fswapmargin"},
    {"server", "time_interpolation", "misc-timeinterp"},
    {"server", "sbas_satellite",     "misc-sbasatsel"},
    {"server", "max_obs_loss",       "misc-maxobsloss"},
    {"server", "float_count",        "misc-floatcnt"},
    {"server", "rinex_option_1",     "misc-rnxopt1"},
    {"server", "rinex_option_2",     "misc-rnxopt2"},
    {"server", "ppp_option",         "misc-pppopt"},
    {"server", "rtcm_option",        "misc-rtcmopt"},
    {"server", "l6_margin",          "misc-l6mrg"},
    {"server", "regularly",          "misc-regularly"},
    {"server", "start_cmd",          "misc-startcmd"},
    {"server", "stop_cmd",           "misc-stopcmd"},

    /* ── streams (rtkrcv rcvopts) ──────────────────────────────────────────── */
    {"streams.input.rover",      "type",      "inpstr1-type"},
    {"streams.input.rover",      "path",      "inpstr1-path"},
    {"streams.input.rover",      "format",    "inpstr1-format"},
    {"streams.input.base",       "type",      "inpstr2-type"},
    {"streams.input.base",       "path",      "inpstr2-path"},
    {"streams.input.base",       "format",    "inpstr2-format"},
    {"streams.input.base",       "nmeareq",   "inpstr2-nmeareq"},
    {"streams.input.base",       "nmealat",   "inpstr2-nmealat"},
    {"streams.input.base",       "nmealon",   "inpstr2-nmealon"},
    {"streams.input.base",       "nmeahgt",   "inpstr2-nmeahgt"},
    {"streams.input.correction", "type",      "inpstr3-type"},
    {"streams.input.correction", "path",      "inpstr3-path"},
    {"streams.input.correction", "format",    "inpstr3-format"},
    {"streams.output.stream1",   "type",      "outstr1-type"},
    {"streams.output.stream1",   "path",      "outstr1-path"},
    {"streams.output.stream1",   "format",    "outstr1-format"},
    {"streams.output.stream2",   "type",      "outstr2-type"},
    {"streams.output.stream2",   "path",      "outstr2-path"},
    {"streams.output.stream2",   "format",    "outstr2-format"},
    {"streams.log.stream1",      "type",      "logstr1-type"},
    {"streams.log.stream1",      "path",      "logstr1-path"},
    {"streams.log.stream2",      "type",      "logstr2-type"},
    {"streams.log.stream2",      "path",      "logstr2-path"},
    {"streams.log.stream3",      "type",      "logstr3-type"},
    {"streams.log.stream3",      "path",      "logstr3-path"},

    /* ── console (rtkrcv rcvopts) ──────────────────────────────────────────── */
    {"console", "passwd",   "console-passwd"},
    {"console", "timetype", "console-timetype"},
    {"console", "soltype",  "console-soltype"},
    {"console", "solflag",  "console-solflag"},

    /* ── files (rtkrcv rcvopts) ────────────────────────────────────────────── */
    {"files", "cmd_file_1", "file-cmdfile1"},
    {"files", "cmd_file_2", "file-cmdfile2"},
    {"files", "cmd_file_3", "file-cmdfile3"},

    {NULL, NULL, NULL}  /* terminator */
};

/* ── Helper: navigate nested TOML tables ───────────────────────────────────── */

/**
 * @brief Navigate to a nested TOML table by dotted path.
 * @param[in] root  Root TOML table.
 * @param[in] path  Dotted path (e.g. "positioning.atmosphere").
 * @return Pointer to the nested table, or NULL if not found.
 */
static toml_table_t *navigate_table(toml_table_t *root, const char *path)
{
    char buf[256];
    char *p, *tok;
    toml_table_t *tbl = root;

    if (!path || !*path) {
        return root;
    }

    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    for (tok = buf; (p = strchr(tok, '.')) != NULL; tok = p + 1) {
        *p = '\0';
        tbl = toml_table_in(tbl, tok);
        if (!tbl) {
            return NULL;
        }
    }
    /* last segment */
    if (*tok) {
        tbl = toml_table_in(tbl, tok);
    }
    return tbl;
}

/* ── Helper: read a TOML value as string for str2opt() ─────────────────────── */

/**
 * @brief Read a TOML key value and convert to string representation
 *        compatible with str2opt().
 * @param[in]  tbl   TOML table containing the key.
 * @param[in]  key   Key name within the table.
 * @param[out] buf   Buffer to receive the string value.
 * @param[in]  bufsz Size of buf.
 * @return 1 if key found and converted, 0 if not found.
 */
static int toml_val_to_str(toml_table_t *tbl, const char *key,
                           char *buf, int bufsz)
{
    toml_datum_t d;
    toml_array_t *arr;
    int i, n;
    char *p;

    /* Try string first */
    d = toml_string_in(tbl, key);
    if (d.ok) {
        strncpy(buf, d.u.s, bufsz - 1);
        buf[bufsz - 1] = '\0';
        free(d.u.s);
        return 1;
    }

    /* Try boolean */
    d = toml_bool_in(tbl, key);
    if (d.ok) {
        strncpy(buf, d.u.b ? "on" : "off", bufsz - 1);
        buf[bufsz - 1] = '\0';
        return 1;
    }

    /* Try integer */
    d = toml_int_in(tbl, key);
    if (d.ok) {
        snprintf(buf, bufsz, "%lld", (long long)d.u.i);
        return 1;
    }

    /* Try double */
    d = toml_double_in(tbl, key);
    if (d.ok) {
        snprintf(buf, bufsz, "%.15g", d.u.d);
        return 1;
    }

    /* Try array (for SNR masks: comma-separated) */
    arr = toml_array_in(tbl, key);
    if (arr) {
        n = toml_array_nelem(arr);
        p = buf;
        for (i = 0; i < n && p - buf < bufsz - 20; i++) {
            d = toml_double_at(arr, i);
            if (d.ok) {
                if (i > 0) {
                    *p++ = ',';
                }
                p += snprintf(p, bufsz - (p - buf), "%.0f", d.u.d);
            } else {
                d = toml_int_at(arr, i);
                if (d.ok) {
                    if (i > 0) {
                        *p++ = ',';
                    }
                    p += snprintf(p, bufsz - (p - buf), "%lld",
                                 (long long)d.u.i);
                }
            }
        }
        *p = '\0';
        return 1;
    }

    return 0;
}

/* ── loadopts_toml ─────────────────────────────────────────────────────────── */

extern int loadopts_toml(const char *file, opt_t *opts)
{
    FILE *fp;
    toml_table_t *root;
    char errbuf[256];
    const toml_map_t *m;
    toml_table_t *tbl;
    opt_t *opt;
    char valbuf[2048];
    int count = 0;

    trace(NULL, 3, "loadopts_toml: file=%s\n", file);

    if (!(fp = fopen(file, "r"))) {
        trace(NULL, 1, "loadopts_toml: file open error (%s)\n", file);
        return 0;
    }

    root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        trace(NULL, 1, "loadopts_toml: parse error (%s): %s\n", file, errbuf);
        fprintf(stderr, "TOML parse error in %s: %s\n", file, errbuf);
        return 0;
    }

    /* Walk the mapping table */
    for (m = toml_mapping; m->toml_section; m++) {
        /* Navigate to the TOML section */
        tbl = navigate_table(root, m->toml_section);
        if (!tbl) {
            continue;
        }

        /* Read the value */
        if (!toml_val_to_str(tbl, m->toml_key, valbuf, sizeof(valbuf))) {
            continue;
        }

        /* Find the legacy option and set it */
        opt = searchopt(m->legacy_name, opts);
        if (!opt) {
            trace(NULL, 2, "loadopts_toml: unknown legacy key %s\n",
                  m->legacy_name);
            continue;
        }

        if (!str2opt(opt, valbuf)) {
            fprintf(stderr, "TOML: invalid value for %s.%s = %s (legacy: %s)\n",
                    m->toml_section, m->toml_key, valbuf, m->legacy_name);
            continue;
        }
        count++;
    }

    toml_free(root);

    trace(NULL, 3, "loadopts_toml: loaded %d options from %s\n", count, file);
    return 1;
}

/* ── saveopts_toml ─────────────────────────────────────────────────────────── */

extern int saveopts_toml(const char *file, const char *comment,
                         const opt_t *opts)
{
    FILE *fp;
    const toml_map_t *m;
    opt_t *opt;
    char valbuf[2048];
    const char *prev_section = "";

    trace(NULL, 3, "saveopts_toml: file=%s\n", file);

    if (!(fp = fopen(file, "w"))) {
        trace(NULL, 1, "saveopts_toml: file open error (%s)\n", file);
        return 0;
    }

    fprintf(fp, "# MRTKLIB Configuration (TOML v1.0.0)\n");
    if (comment) {
        fprintf(fp, "# %s\n", comment);
    }
    fprintf(fp, "\n");

    for (m = toml_mapping; m->toml_section; m++) {
        opt = searchopt(m->legacy_name, (opt_t *)opts);
        if (!opt) {
            continue;
        }

        /* Section header */
        if (strcmp(m->toml_section, prev_section) != 0) {
            if (*prev_section) {
                fprintf(fp, "\n");
            }
            fprintf(fp, "[%s]\n", m->toml_section);
            prev_section = m->toml_section;
        }

        /* Value */
        opt2str(opt, valbuf);

        /* Format based on type */
        switch (opt->format) {
        case 0: /* int */
            fprintf(fp, "%-24s = %s\n", m->toml_key, valbuf);
            break;
        case 1: /* double */
            fprintf(fp, "%-24s = %s\n", m->toml_key, valbuf);
            break;
        case 2: /* string */
            fprintf(fp, "%-24s = \"%s\"\n", m->toml_key, valbuf);
            break;
        case 3: /* enum */
            fprintf(fp, "%-24s = \"%s\"\n", m->toml_key, valbuf);
            break;
        }
    }

    fclose(fp);
    return 1;
}
