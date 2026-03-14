/*------------------------------------------------------------------------------
 * cssr2rtcm3.c : real-time QZSS CSSR (CLAS) to RTCM3 MSM4 (OSR) converter
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2015- Mitsubishi Electric Corp.
 * Copyright (C) 2014 Geospatial Information Authority of Japan
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007- T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file cssr2rtcm3.c
 * @brief Real-time QZSS CSSR (CLAS L6D) to RTCM3 MSM4 (OSR) converter.
 *
 * Receives QZSS CLAS L6D CSSR correction data via stream (serial/TCP/NTRIP/
 * file) and converts it to RTCM3 MSM4 (OSR) messages in real-time. This
 * enables CLAS-unsupported GNSS receivers to use CLAS corrections as a
 * Virtual Reference Station (VRS) via NTRIP/TCP.
 *
 * The core processing pipeline is based on ssr2obs gen_osr(), with file I/O
 * replaced by the MRTKLIB stream API for real-time operation.
 *
 * Data flow (per epoch):
 *   1. strread(stream_in) → L6 bytes → clas_input_cssr() → CSSR decode
 *   2. clas_decode_msg() == 10 → epoch boundary → update corrections
 *   3. actualdist() → satellite positions → dummy observations
 *   4. clas_ssr2osr() → OSR (VRS pseudo-observations)
 *   5. gen_rtcm3(1005+MSM4) → RTCM3 binary → strwrite(stream_out)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>

#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_sol.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_trace.h"
#include "mrtklib/mrtk_sys.h"
#include "mrtklib/mrtk_options.h"
#include "mrtklib/mrtk_rinex.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_rtkpos.h"
#include "mrtklib/mrtk_clas.h"
#include "mrtklib/mrtk_stream.h"
#include "mrtklib/mrtk_rcvraw.h"
#include "mrtklib/mrtk_toml.h"

/* constants and macros ------------------------------------------------------*/

#define PROGNAME    "cssr2rtcm3"
#define PROG_VER    "1.0"

#define OSR_SYS     (SYS_GPS|SYS_QZS|SYS_GAL)
#define OSR_NFREQ   4
#define OSR_ELMASK  0.0

#define MAXNAVFILE  16
#define STREAMBUF   4096

#ifndef CLIGHT
#define CLIGHT      299792458.0
#endif
#ifndef D2R
#define D2R         (3.1415926535897932384626433832795/180.0)
#endif

/* RTCM3 MSM4 message types per constellation */
static const int rtcm3_msm_types[] = {1074, 1084, 1094, 1114, 0};
static const int rtcm3_msm_sys[]   = {SYS_GPS, SYS_GLO, SYS_GAL, SYS_QZS, 0};

/* global state for signal handler */
static volatile sig_atomic_t g_shutdown = 0;

static const char *usage_text[] = {
    "usage: mrtk cssr2rtcm3 [options] [-nav file ...]",
    "",
    "  Real-time QZSS CSSR (CLAS L6D) to RTCM3 MSM4 (OSR) converter.",
    "  Converts CLAS corrections to VRS pseudo-observations for",
    "  CLAS-unsupported receivers.",
    "",
    "options:",
    "  -k  file          Configuration file (TOML or legacy .conf)",
    "  -in  uri          L6 CSSR input stream  [stdin]",
    "                    Use sbf://... for single SBF stream mode",
    "                    (auto-extracts L6D, PVT position, and NAV)",
    "  -2ch uri          Second L6 input stream (channel 2)",
    "  -out uri          RTCM3 output stream   [stdout]",
    "  -pos uri          Position input stream  (NMEA GGA)",
    "  -p  lat,lon,hgt   Fixed user position   (deg, m)",
    "  -nav file ...     Navigation files (RINEX NAV)",
    "  -d  level         Trace level (0-5)     [0]",
    "  -t  interval      Output interval (s)   [1]",
    "",
    "stream URI formats:",
    "  file://path                File",
    "  serial://port:baud         Serial port",
    "  tcpsvr://:port             TCP server (listen)",
    "  tcpcli://host:port         TCP client",
    "  ntripsvr://:pw@host:port/mnt  NTRIP server (push)",
    "  ntripcli://[user:pw@]host:port/mnt  NTRIP client",
    "  ntripcas://[user:pw@]:port/mnt  NTRIP caster",
    "",
    "examples:",
    "  # File replay (testing)",
    "  mrtk cssr2rtcm3 -in file://data/2019239Q.l6 \\",
    "    -out file://out.rtcm3 \\",
    "    -nav data/nav.nav -p 36.104,140.087,70.0",
    "",
    "  # Serial L6 -> TCP server, position from receiver NMEA",
    "  mrtk cssr2rtcm3 -in serial://ttyUSB0:115200 \\",
    "    -out tcpsvr://:9001 \\",
    "    -pos serial://ttyUSB1:9600 \\",
    "    -nav /path/to/broadcast.nav",
    "",
    "  # NTRIP L6 -> NTRIP caster, fixed position",
    "  mrtk cssr2rtcm3 -in ntripcli://user:pw@caster:2101/L6 \\",
    "    -out ntripsvr://:pw@caster:2101/VRS \\",
    "    -p 35.681,139.767,40.0 -nav /path/to/nav",
    "",
    "  # Single SBF stream from mosaic-G5 (serial)",
    "  mrtk cssr2rtcm3 -in sbf://serial://ttyUSB0:115200 \\",
    "    -out tcpsvr://:9001",
    "",
    "  # Single SBF stream from mosaic-G5 (TCP)",
    "  mrtk cssr2rtcm3 -in sbf://tcpcli://192.168.1.100:28785 \\",
    "    -out ntripsvr://:pw@caster:2101/VRS",
    NULL
};

/*============================================================================
 * Stream URI Parser
 *
 * Parses URI strings like "tcpsvr://:9001" into stream type + path.
 *===========================================================================*/

/**
 * @brief Parse a stream URI into type and path components.
 * @param[in]  uri   URI string (e.g. "tcpsvr://:9001")
 * @param[out] type  Stream type (STR_*)
 * @param[out] path  Stream path (buffer, at least 1024 bytes)
 * @return 1 on success, 0 on error
 */
static int parse_stream_uri(const char *uri, int *type, char *path)
{
    static const struct {
        const char *prefix;
        int type;
    } uri_types[] = {
        {"file://",     STR_FILE},
        {"serial://",   STR_SERIAL},
        {"tcpsvr://",   STR_TCPSVR},
        {"tcpcli://",   STR_TCPCLI},
        {"ntripsvr://", STR_NTRIPSVR},
        {"ntripcli://", STR_NTRIPCLI},
        {"ntripcas://", STR_NTRIPCAS},
        {"udpsvr://",   STR_UDPSVR},
        {"udpcli://",   STR_UDPCLI},
        {NULL, 0}
    };
    int i;

    for (i = 0; uri_types[i].prefix; i++) {
        size_t len = strlen(uri_types[i].prefix);
        if (!strncmp(uri, uri_types[i].prefix, len)) {
            *type = uri_types[i].type;
            strncpy(path, uri + len, 1023);
            path[1023] = '\0';
            return 1;
        }
    }
    /* if no prefix, treat as file path */
    *type = STR_FILE;
    strncpy(path, uri, 1023);
    path[1023] = '\0';
    return 1;
}

/*============================================================================
 * Signal Handler
 *===========================================================================*/

static void sig_shutdown(int sig)
{
    (void)sig;
    g_shutdown = 1;
}

/*============================================================================
 * NMEA GGA Position Parser
 *
 * Lightweight inline GGA parser — extracts lat/lon/hgt from $xxGGA sentence.
 * Does not depend on sol_t time field (unlike decode_nmeagga in mrtk_sol.c).
 *===========================================================================*/

/**
 * @brief Convert NMEA ddmm.mmmm format to decimal degrees.
 */
static double nmea_dmm2deg(double dmm)
{
    int deg = (int)(dmm / 100.0);
    return deg + (dmm - deg * 100.0) / 60.0;
}

/**
 * @brief Parse NMEA GGA sentence and extract position in ECEF.
 * @param[in]  buf    NMEA sentence buffer
 * @param[in]  len    Buffer length
 * @param[out] ecef   ECEF position [X,Y,Z] (m), unchanged if parse fails
 * @return 1 if position updated, 0 otherwise
 */
static int parse_nmea_gga(const char *buf, int len, double *ecef)
{
    char line[512], *fields[20];
    double lat, lon, alt, msl, pos[3];
    char ns, ew;
    int i, nf;

    if (len <= 0 || len >= (int)sizeof(line)) return 0;
    memcpy(line, buf, len);
    line[len] = '\0';

    /* check for GGA sentence */
    if (!strstr(line, "GGA")) return 0;

    /* split by comma */
    fields[0] = line;
    for (i = 0, nf = 1; i < len && nf < 20; i++) {
        if (line[i] == ',') {
            line[i] = '\0';
            fields[nf++] = line + i + 1;
        }
    }
    /* GGA: $xxGGA,time,lat,N/S,lon,E/W,qual,nsat,hdop,alt,M,geoid,M,... */
    if (nf < 12) return 0;

    /* check quality indicator (field 6) — 0 means no fix */
    if (atoi(fields[6]) == 0) return 0;

    lat = atof(fields[2]);
    ns  = fields[3][0];
    lon = atof(fields[4]);
    ew  = fields[5][0];
    alt = atof(fields[8]);
    msl = atof(fields[10]);

    if ((ns != 'N' && ns != 'S') || (ew != 'E' && ew != 'W')) return 0;

    pos[0] = (ns == 'N' ? 1.0 : -1.0) * nmea_dmm2deg(lat) * D2R;
    pos[1] = (ew == 'E' ? 1.0 : -1.0) * nmea_dmm2deg(lon) * D2R;
    pos[2] = alt + msl;

    pos2ecef(pos, ecef);
    return 1;
}

/**
 * @brief Read NMEA GGA from position stream and update user position.
 * @param[in]  strm      Position input stream
 * @param[out] user_pos  User position in ECEF [X,Y,Z] (m)
 * @return 1 if position updated, 0 otherwise
 */
static int update_position_from_stream(stream_t *strm, double *user_pos)
{
    static char nmeabuf[2048];
    static int nmealen = 0;
    uint8_t buf[512];
    int n, i, updated = 0;

    n = strread(strm, buf, sizeof(buf));
    if (n <= 0) return 0;

    /* append to line buffer */
    for (i = 0; i < n; i++) {
        if (nmealen >= (int)sizeof(nmeabuf) - 1) {
            nmealen = 0; /* overflow — reset */
        }
        nmeabuf[nmealen++] = (char)buf[i];

        /* look for complete line (CR or LF) */
        if (buf[i] == '\n' || buf[i] == '\r') {
            if (nmealen > 1) {
                nmeabuf[nmealen] = '\0';
                if (parse_nmea_gga(nmeabuf, nmealen, user_pos)) {
                    updated = 1;
                }
            }
            nmealen = 0;
        }
    }
    return updated;
}

/*============================================================================
 * Satellite Geometry + Dummy Observations
 *===========================================================================*/

/**
 * @brief Generate dummy observations from satellite geometry.
 *
 * For each satellite with valid SSR corrections, compute geometric range
 * and create a pseudorange observation needed by clas_ssr2osr().
 * (Verbatim from ssr2obs.c actualdist())
 */
static int actualdist(gtime_t time, obs_t *obs, nav_t *nav, const double *x)
{
    int i, n, sat, lsat[MAXSAT];
    double r, rr[3], dt, dt_p;
    double rs1[6], dts1[2], var1, e1[3];
    gtime_t tg;
    obsd_t *obsd = obs->data;
    int svh1;

    obs->n = 0;
    for (i = 0; i < MAXOBS; i++) {
        obsd[i].time = time;
    }

    /* build satellite list from broadcast ephemeris */
    for (i = n = 0; i < MAXSAT; i++) {
        if (nav->eph[i].sat == i + 1 && nav->eph[i].toe.time > 0) {
            lsat[n++] = i + 1;
        }
    }

    for (i = 0; i < 3; i++) rr[i] = x[i];
    if (norm(rr, 3) <= 0.0) return -1;

    /* compute pseudorange via iterative light-time correction */
    for (i = 0; i < n; i++) {
        sat = lsat[i];
        dt = 0.08; dt_p = 0.0;
        while (1) {
            tg = timeadd(time, -dt);
            if (!satpos(tg, time, sat, EPHOPT_BRDC, nav,
                        rs1, dts1, &var1, &svh1)) {
                obsd[i].sat = 0;
                break;
            }
            if ((r = geodist(rs1, x, e1)) <= 0.0) {
                obsd[i].sat = 0;
                break;
            }
            dt_p = dt;
            dt = r / CLIGHT;
            if (fabs(dt - dt_p) < 1.0e-12) {
                obsd[i].time = time;
                obsd[i].sat = sat;
                obsd[i].P[0] = r + CLIGHT * (-dts1[0]);
                break;
            }
        }
    }
    obs->n = n;
    return 0;
}

/*============================================================================
 * RTCM3 Output
 *===========================================================================*/

/**
 * @brief Encode and send RTCM3 MSM4 messages to output stream.
 *
 * Generates RTCM3 1005 (station position) + MSM4 per constellation
 * (1074/1084/1094/1114) and writes to the output stream.
 */
static int encode_and_send_rtcm3(stream_t *strm_out, rtcm_t *rtcm,
                                  const obs_t *obs, nav_t *nav,
                                  const double *pos)
{
    int i, j, k, sys, sync, total = 0;

    if (obs->n <= 0) return 0;

    rtcm->time = obs->data[0].time;
    matcpy(rtcm->sta.pos, pos, 3, 1);

    /* station coordinates message (1005); sync=1 — MSM follows */
    if (gen_rtcm3(rtcm, 1005, 0, 1)) {
        strwrite(strm_out, rtcm->buff, rtcm->nbyte);
        total += rtcm->nbyte;
    }

    /* MSM4 per constellation */
    for (k = 0; rtcm3_msm_sys[k]; k++) {
        sys = rtcm3_msm_sys[k];
        rtcm->obs.n = 0;
        for (i = 0; i < obs->n && rtcm->obs.n < MAXOBS; i++) {
            if (satsys(obs->data[i].sat, NULL) & sys)
                rtcm->obs.data[rtcm->obs.n++] = obs->data[i];
        }
        if (rtcm->obs.n <= 0) continue;

        /* sync=1 if any later constellation has data */
        sync = 0;
        for (j = k + 1; rtcm3_msm_sys[j] && !sync; j++) {
            for (i = 0; i < obs->n; i++) {
                if (satsys(obs->data[i].sat, NULL) & rtcm3_msm_sys[j]) {
                    sync = 1; break;
                }
            }
        }
        if (gen_rtcm3(rtcm, rtcm3_msm_types[k], 0, sync)) {
            strwrite(strm_out, rtcm->buff, rtcm->nbyte);
            total += rtcm->nbyte;
        }
    }
    return total;
}

/*============================================================================
 * CLAS Correction Update
 *===========================================================================*/

/**
 * @brief Update CLAS corrections after epoch boundary (subtype 10).
 */
static void update_corrections(clas_ctx_t *clas, nav_t *nav, int ch)
{
    int net = clas->grid[ch].network;
    int rc;

    if (net > 0) {
        rc = clas_bank_get_close(clas, clas->l6buf[ch].time,
                                 net, ch, &clas->current[ch]);
        if (rc == 0) {
            clas_update_global(nav, &clas->current[ch], ch);
            clas_check_grid_status(clas, &clas->current[ch], ch);
        } else {
            fprintf(stderr, "  bank_get failed: net=%d ch=%d rc=%d\n", net, ch, rc);
        }
    }

    /* bootstrap: scan all networks when network is unknown */
    if (clas->grid[ch].network <= 0 && clas->bank[ch] && clas->bank[ch]->use) {
        clas_corr_t *tmp = (clas_corr_t *)calloc(1, sizeof(clas_corr_t));
        int found = 0;
        if (tmp) {
            for (net = 1; net < CLAS_MAX_NETWORK; net++) {
                if (clas_bank_get_close(clas, clas->l6buf[ch].time,
                                        net, ch, tmp) == 0) {
                    clas_check_grid_status(clas, tmp, ch);
                    clas_update_global(nav, tmp, ch);
                    found++;
                }
            }
            if (found == 0) {
                fprintf(stderr, "  bootstrap: no network found (bank.use=%d)\n",
                        clas->bank[ch]->use);
            } else {
                fprintf(stderr, "  bootstrap: found %d networks, grid.network=%d\n",
                        found, clas->grid[ch].network);
            }
            free(tmp);
        }
    } else if (clas->grid[ch].network <= 0) {
        fprintf(stderr, "  no grid network: bank=%p use=%d\n",
                (void*)clas->bank[ch],
                clas->bank[ch] ? clas->bank[ch]->use : -1);
    }
}

/*============================================================================
 * Main Processing
 *===========================================================================*/

int mrtk_cssr2rtcm3(int argc, char **argv)
{
    prcopt_t prcopt = prcopt_default;
    solopt_t solopt = solopt_default;
    filopt_t filopt = {""};

    /* stream state */
    stream_t strm_in  = {0};
    stream_t strm_out = {0};
    stream_t strm_pos = {0};
    stream_t strm_2ch = {0};
    int in_type  = STR_NONE, out_type = STR_NONE, pos_type = STR_NONE;
    int ch2_type = STR_NONE;
    char in_path[1024] = "", out_path[1024] = "", pos_path[1024] = "";
    char ch2_path[1024] = "";

    /* config */
    char *conffile = "";
    char *navfiles[MAXNAVFILE];
    int nnav = 0;
    double user_pos[3] = {0};     /* ECEF position */
    double fixed_pos[3] = {0};    /* lat,lon,hgt from -p option */
    int has_fixed_pos = 0;
    double output_interval = 1.0; /* seconds */
    int trace_level = 0;
    int sbf_mode = 0;             /* 1: single SBF stream input */

    /* processing state */
    clas_ctx_t *clas = NULL;
    nav_t *nav = NULL;
    rtk_t rtk = {0};
    rtcm_t *rtcm = NULL;
    raw_t *raw_sbf = NULL;
    obsd_t *obsdata = NULL;
    clas_osrd_t *osr = NULL;
    obs_t obs = {0};
    gtime_t last_output = {0};
    int i, ret;
    int pos_updated = 0;
    int epoch_count = 0;
    int osr_count = 0;
    long total_bytes = 0;
    int nav_count = 0, l6d_count = 0, pvt_count = 0;
    int dbg_nopos = 0, dbg_notime = 0, dbg_nogeom = 0, dbg_noosr = 0;
    int dbg_cssr_decode = 0;
    int dbg_subtype[16] = {0};
    int l6d_prn_filter = 0;  /* auto-select first QZS satellite for L6D */
    int l6d_prn_count[MAXSAT + 1];  /* L6D block count per satellite */
    memset(l6d_prn_count, 0, sizeof(l6d_prn_count));

    /* parse command-line arguments */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-k") && i + 1 < argc) {
            conffile = argv[++i];
        }
        else if (!strcmp(argv[i], "-in") && i + 1 < argc) {
            const char *uri = argv[++i];
            if (!strncmp(uri, "sbf://", 6)) {
                sbf_mode = 1;
                uri += 6; /* strip sbf:// prefix → inner URI */
            }
            parse_stream_uri(uri, &in_type, in_path);
        }
        else if (!strcmp(argv[i], "-out") && i + 1 < argc) {
            parse_stream_uri(argv[++i], &out_type, out_path);
        }
        else if (!strcmp(argv[i], "-pos") && i + 1 < argc) {
            parse_stream_uri(argv[++i], &pos_type, pos_path);
        }
        else if (!strcmp(argv[i], "-2ch") && i + 1 < argc) {
            parse_stream_uri(argv[++i], &ch2_type, ch2_path);
        }
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
            sscanf(argv[++i], "%lf,%lf,%lf",
                   &fixed_pos[0], &fixed_pos[1], &fixed_pos[2]);
            has_fixed_pos = 1;
        }
        else if (!strcmp(argv[i], "-nav")) {
            /* collect all following non-option arguments as NAV files */
            while (i + 1 < argc && argv[i + 1][0] != '-' &&
                   nnav < MAXNAVFILE) {
                navfiles[nnav++] = argv[++i];
            }
        }
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) {
            trace_level = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) {
            output_interval = atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            for (i = 0; usage_text[i]; i++)
                fprintf(stderr, "%s\n", usage_text[i]);
            return 0;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return -1;
        }
    }

    /* validate inputs */
    if (in_type == STR_NONE) {
        fprintf(stderr, "Error: no input stream specified (-in).\n");
        return -1;
    }
    if (out_type == STR_NONE) {
        fprintf(stderr, "Error: no output stream specified (-out).\n");
        return -1;
    }
    if (!sbf_mode) {
        if (!has_fixed_pos && pos_type == STR_NONE) {
            fprintf(stderr, "Error: no position specified (-p or -pos).\n");
            return -1;
        }
        if (nnav <= 0) {
            fprintf(stderr, "Error: no navigation files specified (-nav).\n");
            return -1;
        }
    }

    /* load configuration */
    prcopt.mode    = PMODE_SSR2OSR;
    prcopt.nf      = OSR_NFREQ;
    prcopt.navsys  = OSR_SYS;
    prcopt.elmin   = OSR_ELMASK * D2R;
    prcopt.tidecorr = 1;
    prcopt.posopt[2] = 1; /* phase windup correction */

    if (*conffile) {
        setsysopts(&prcopt, &solopt, &filopt);
        /* try TOML first, fall back to legacy .conf */
        if (!loadopts_toml(conffile, sysopts) && !loadopts(conffile, sysopts)) {
            fprintf(stderr, "Configuration file read error: %s\n", conffile);
            return -1;
        }
        getsysopts(&prcopt, &solopt, &filopt);
    }

    /* set user position */
    if (has_fixed_pos) {
        double pos_rad[3];
        pos_rad[0] = fixed_pos[0] * D2R;
        pos_rad[1] = fixed_pos[1] * D2R;
        pos_rad[2] = fixed_pos[2];
        pos2ecef(pos_rad, user_pos);
        for (i = 0; i < 3; i++) prcopt.ru[i] = user_pos[i];
    }

    /* trace */
    if (trace_level > 0) {
        traceopen(NULL, "cssr2rtcm3.trace");
        tracelevel(NULL, trace_level);
    }

    fprintf(stderr, "%s v%s starting...\n", PROGNAME, PROG_VER);

    /* ── Allocate large structures ── */
    clas = (clas_ctx_t *)calloc(1, sizeof(clas_ctx_t));
    nav  = (nav_t *)calloc(1, sizeof(nav_t));
    rtcm = (rtcm_t *)calloc(1, sizeof(rtcm_t)); /* ~103MB */
    obsdata = (obsd_t *)calloc(MAXOBS, sizeof(obsd_t));
    osr = (clas_osrd_t *)calloc(MAXOBS, sizeof(clas_osrd_t));
    if (!clas || !nav || !rtcm || !obsdata || !osr) {
        fprintf(stderr, "Memory allocation error.\n");
        goto cleanup;
    }

    /* initialize nav_t arrays (eph, geph, etc.) */
    nav->eph  = NULL;
    nav->geph = NULL;
    nav->seph = NULL;
    if (!(nav->eph  = (eph_t  *)calloc(MAXSAT * 2, sizeof(eph_t))) ||
        !(nav->geph = (geph_t *)calloc(NSATGLO,    sizeof(geph_t))) ||
        !(nav->seph = (seph_t *)calloc(NSATSBS * 2, sizeof(seph_t)))) {
        fprintf(stderr, "Navigation data allocation error.\n");
        goto cleanup;
    }
    nav->n    = 0;
    nav->nmax = MAXSAT * 2;
    nav->ng   = 0;
    nav->ngmax = NSATGLO;
    nav->ns   = 0;
    nav->nsmax = NSATSBS * 2;

    /* initialize CLAS context */
    if (clas_ctx_init(clas) != 0) {
        fprintf(stderr, "CLAS context init error.\n");
        goto cleanup;
    }

    /* initialize RTCM encoder */
    if (!init_rtcm(rtcm)) {
        fprintf(stderr, "RTCM init error.\n");
        goto cleanup;
    }

    /* read grid definition file */
    if (filopt.grid[0]) {
        if (clas_read_grid_def(clas, filopt.grid)) {
            fprintf(stderr, "Grid file read error: %s\n", filopt.grid);
            goto cleanup;
        }
    }

    /* read ocean tide loading */
    if (prcopt.tidecorr >= 3 && filopt.blq[0]) {
        if (!readblqgrid(filopt.blq, clas)) {
            fprintf(stderr, "OTL grid read error: %s\n", filopt.blq);
            goto cleanup;
        }
    }

    /* allocate raw_t for SBF mode */
    if (sbf_mode) {
        raw_sbf = (raw_t *)calloc(1, sizeof(raw_t));
        if (!raw_sbf) {
            fprintf(stderr, "Memory allocation error (raw_t).\n");
            goto cleanup;
        }
        if (!init_raw(raw_sbf, STRFMT_SEPT)) {
            fprintf(stderr, "Failed to initialize raw_t.\n");
            goto cleanup;
        }
        fprintf(stderr, "SBF mode: single-stream input (L6D + NAV + PVT)\n");
    }

    /* read navigation files (optional in SBF mode — bootstrap only) */
    for (i = 0; i < nnav; i++) {
        readrnx(navfiles[i], 0, "", NULL, nav, NULL);
    }
    if (nnav > 0) uniqnav(nav);
    if (!sbf_mode && nav->n <= 0) {
        fprintf(stderr, "No navigation data found.\n");
        goto cleanup;
    }
    if (nav->n > 0) {
        fprintf(stderr, "Loaded %d navigation records.\n", nav->n);
    }

    /* initialize GPS week reference from navigation data or system time.
     * CSSR decoder needs a valid GPS week to interpret ToW fields. */
    {
        int iw, week;
        gtime_t tref = {0};

        /* prefer first ephemeris toe */
        if (nav->n > 0 && nav->eph[0].toe.time > 0) {
            tref = nav->eph[0].toe;
        } else {
            /* fallback: current system time */
            tref = utc2gpst(timeget());
        }
        time2gpst(tref, &week);
        for (iw = 0; iw < CSSR_REF_MAX; iw++) {
            clas->week_ref[iw] = week;
            clas->tow_ref[iw] = -1;
        }
        fprintf(stderr, "GPS week reference: %d\n", week);
    }

    /* initialize RTK structure */
    rtkinit(&rtk, &prcopt);
    nav->clas_ctx = clas;
    obs.data = obsdata;

    /* set initial position */
    for (i = 0; i < 3; i++) {
        rtk.sol.rr[i] = user_pos[i];
        rtk.x[i] = user_pos[i];
    }

    /* ── Open streams ── */
    strinitcom();

    if (!stropen(&strm_in, in_type, STR_MODE_R, in_path)) {
        fprintf(stderr, "Input stream open error: %s\n", in_path);
        goto cleanup;
    }
    if (!stropen(&strm_out, out_type, STR_MODE_W, out_path)) {
        fprintf(stderr, "Output stream open error: %s\n", out_path);
        goto cleanup;
    }
    if (pos_type != STR_NONE) {
        if (!stropen(&strm_pos, pos_type, STR_MODE_R, pos_path)) {
            fprintf(stderr, "Position stream open error: %s\n", pos_path);
            goto cleanup;
        }
        fprintf(stderr, "Position stream: %s\n", pos_path);
    }
    if (ch2_type != STR_NONE) {
        if (!stropen(&strm_2ch, ch2_type, STR_MODE_R, ch2_path)) {
            fprintf(stderr, "Ch2 stream open error: %s\n", ch2_path);
            goto cleanup;
        }
        fprintf(stderr, "L6 ch2 stream: %s\n", ch2_path);
    }

    fprintf(stderr, "Input:  %s\n", in_path);
    fprintf(stderr, "Output: %s\n", out_path);
    if (has_fixed_pos) {
        fprintf(stderr, "Position: %.6f, %.6f, %.1f (fixed)\n",
                fixed_pos[0], fixed_pos[1], fixed_pos[2]);
    }

    /* ── Install signal handlers ── */
    signal(SIGINT,  sig_shutdown);
    signal(SIGTERM, sig_shutdown);
    signal(SIGPIPE, SIG_IGN);

    /* ══════════════════════════════════════════════════════════════════════
     * Main Loop
     * ══════════════════════════════════════════════════════════════════════*/

    fprintf(stderr, "Running (Ctrl-C to stop)...\n");

    while (!g_shutdown) {
        uint8_t buf[STREAMBUF];
        int n, k;
        gtime_t now;

        /* ── Read and decode input ── */
        n = strread(&strm_in, buf, sizeof(buf));

        /* detect end of file: readfile() sets stream msg to "end" */
        if (n == 0 && !strncmp(strm_in.msg, "end", 3)) {
            fprintf(stderr, "\nEnd of input stream.\n");
            break;
        }

        if (n > 0) {
            total_bytes += n;
        }

        if (sbf_mode && n > 0) {
            /* SBF mode: demux L6D, NAV, and PVT from single SBF stream */
            for (i = 0; i < n; i++) {
                ret = input_sbf(raw_sbf, rtcm, buf[i]);

                if (ret == 10) {
                    /* L6D frame decoded */
                    int cret, l6d_sat, fprn;
                    l6d_sat = raw_sbf->ephsat;
                    l6d_count++;
                    if (l6d_sat > 0 && l6d_sat <= MAXSAT) {
                        l6d_prn_count[l6d_sat]++;
                    }

                    /* auto-select first QZS satellite for L6D filtering */
                    if (l6d_prn_filter == 0 && l6d_sat > 0) {
                        l6d_prn_filter = l6d_sat;
                        satsys(l6d_sat, &fprn);
                        fprintf(stderr, "L6D: auto-selected J%d for CLAS input\n", fprn);
                    }

                    /* only feed L6D from selected satellite (avoid frame corruption) */
                    if (l6d_sat == l6d_prn_filter) {
                        for (k = 0; k < 250; k++) {
                            clas_input_cssr(clas, rtcm->buff[k], 0);
                            cret = clas_decode_msg(clas, 0);
                            if (cret == 10) {
                                dbg_cssr_decode++;
                                if (clas->l6buf[0].subtype < 16) {
                                    dbg_subtype[clas->l6buf[0].subtype]++;
                                }
                                update_corrections(clas, nav, 0);
                            }
                        }
                        /* drain remaining CSSR messages in buffer */
                        while ((cret = clas_decode_msg(clas, 0)) != 0) {
                            if (cret == 10) {
                                dbg_cssr_decode++;
                                if (clas->l6buf[0].subtype < 16) {
                                    dbg_subtype[clas->l6buf[0].subtype]++;
                                }
                                update_corrections(clas, nav, 0);
                            }
                        }
                    }
                }
                else if (ret == 2) {
                    nav_count++;
                    /* ephemeris → copy to nav */
                    int esat = raw_sbf->ephsat;
                    int sys = satsys(esat, NULL);
                    if (esat > 0 && esat <= MAXSAT) {
                        if (sys == SYS_GLO) {
                            int prn;
                            satsys(esat, &prn);
                            if (prn >= 1 && prn <= NSATGLO) {
                                nav->geph[prn - 1] = raw_sbf->nav.geph[prn - 1];
                                if (nav->ng < prn) nav->ng = prn;
                            }
                        } else {
                            nav->eph[esat - 1] = raw_sbf->nav.eph[esat - 1];
                            if (raw_sbf->ephset) {
                                nav->eph[esat - 1 + MAXSAT] =
                                    raw_sbf->nav.eph[esat - 1 + MAXSAT];
                            }
                            if (nav->n < esat) nav->n = esat;
                        }
                    }
                }
                else if (ret == 5) {
                    pvt_count++;
                    /* PVTGeodetic → update user position (unless -p override) */
                    if (!has_fixed_pos && norm(raw_sbf->sta.pos, 3) > 0.0) {
                        matcpy(user_pos, raw_sbf->sta.pos, 3, 1);
                        for (k = 0; k < 3; k++) {
                            prcopt.ru[k] = user_pos[k];
                            rtk.sol.rr[k] = user_pos[k];
                            rtk.x[k] = user_pos[k];
                        }
                        pos_updated = 1;
                    }
                }
            }
            /* initialize GPS week from SBF timestamp if not yet set */
            if (clas->week_ref[0] == 0 && raw_sbf->time.time > 0) {
                int iw, week;
                time2gpst(raw_sbf->time, &week);
                for (iw = 0; iw < CSSR_REF_MAX; iw++) {
                    clas->week_ref[iw] = week;
                    clas->tow_ref[iw] = -1;
                }
                fprintf(stderr, "GPS week from SBF: %d\n", week);
            }
        }
        else if (!sbf_mode && n > 0) {
            /* Legacy mode: raw L6 CSSR bytes → ch1 */
            for (i = 0; i < n; i++) {
                clas_input_cssr(clas, buf[i], 0);
                ret = clas_decode_msg(clas, 0);
                if (ret == 10) {
                    update_corrections(clas, nav, 0);
                }
            }
        }

        /* L6 ch2 input (legacy mode only — SBF mode is single-stream) */
        if (!sbf_mode && ch2_type != STR_NONE) {
            n = strread(&strm_2ch, buf, sizeof(buf));
            if (n > 0) {
                for (i = 0; i < n; i++) {
                    clas_input_cssr(clas, buf[i], 1);
                    ret = clas_decode_msg(clas, 1);
                    if (ret == 10) {
                        update_corrections(clas, nav, 1);
                    }
                }
            }
        }

        /* Update position from NMEA GGA (legacy mode only) */
        if (!sbf_mode && pos_type != STR_NONE) {
            if (update_position_from_stream(&strm_pos, user_pos)) {
                pos_updated = 1;
                for (i = 0; i < 3; i++) {
                    prcopt.ru[i] = user_pos[i];
                    rtk.sol.rr[i] = user_pos[i];
                    rtk.x[i] = user_pos[i];
                }
            }
        }

        /* Check if position is available */
        if (norm(user_pos, 3) <= 0.0) {
            dbg_nopos++;
            sleepms(100);
            continue;
        }

        /* Output RTCM3 at configured interval */
        now = clas->l6buf[0].time;
        if (now.time == 0) {
            dbg_notime++;
            sleepms(10);
            continue;
        }

        if (last_output.time == 0 ||
            timediff(now, last_output) >= output_interval - 0.01) {

            rtk.sol.time = now;

            /* generate dummy observations from satellite geometry */
            if (actualdist(now, &obs, nav, user_pos) < 0) {
                dbg_nogeom++;
                sleepms(10);
                continue;
            }

            /* convert SSR to OSR */
            {
                int obs_in = obs.n;
                obs.n = clas_ssr2osr(&rtk, obs.data, obs.n, nav, osr, 0, clas);
                if (obs.n == 0 && dbg_noosr < 3) {
                    int week;
                    double tow = time2gpst(now, &week);
                    fprintf(stderr, "  OSR fail: week=%d tow=%.1f obs_in=%d nav.n=%d pos=%.1f,%.1f,%.1f\n",
                            week, tow, obs_in, nav->n,
                            user_pos[0], user_pos[1], user_pos[2]);
                }
            }

            if (obs.n > 0) {
                int bytes = encode_and_send_rtcm3(&strm_out, rtcm,
                                                   &obs, nav, user_pos);
                epoch_count++;
                osr_count += obs.n;

                {
                    int week;
                    double tow = time2gpst(now, &week);
                    fprintf(stderr, "\rEpoch %d: week=%d tow=%.0f sats=%d "
                            "bytes=%d",
                            epoch_count, week, tow, obs.n, bytes);
                    fflush(stderr);
                }
            } else {
                dbg_noosr++;
            }

            last_output = now;
        }

        sleepms(10);
    }

    fprintf(stderr, "\nShutting down...\n");
    /* dump bank state for debugging */
    if (clas->bank[0] && clas->bank[0]->use) {
        clas_bank_ctrl_t *bnk = clas->bank[0];
        fprintf(stderr, "Bank: use=%d NextOrbit=%d NextClock=%d NextBias=%d NextTrop=%d\n",
                bnk->use, bnk->NextOrbit, bnk->NextClock, bnk->NextBias, bnk->NextTrop);
        for (i = 0; i < bnk->NextOrbit && i < 3; i++) {
            int week;
            double tow = time2gpst(bnk->OrbitBank[i].time, &week);
            fprintf(stderr, "  orbit[%d]: net=%d week=%d tow=%.0f\n",
                    i, bnk->OrbitBank[i].network, week, tow);
        }
        for (i = 0; i < bnk->NextClock && i < 3; i++) {
            int week;
            double tow = time2gpst(bnk->ClockBank[i].time, &week);
            fprintf(stderr, "  clock[%d]: net=%d week=%d tow=%.0f\n",
                    i, bnk->ClockBank[i].network, week, tow);
        }
    }
    fprintf(stderr, "Grid: network=%d num=%d\n",
            clas->grid[0].network, clas->grid[0].num);
    fprintf(stderr, "Read: %ld bytes, NAV: %d, L6D: %d, PVT: %d\n",
            total_bytes, nav_count, l6d_count, pvt_count);
    fprintf(stderr, "CSSR decoded: %d (ST1=%d ST2=%d ST3=%d ST4=%d ST5=%d ST7=%d ST11=%d ST12=%d)\n",
            dbg_cssr_decode, dbg_subtype[1], dbg_subtype[2], dbg_subtype[3],
            dbg_subtype[4], dbg_subtype[5], dbg_subtype[7], dbg_subtype[11], dbg_subtype[12]);
    /* L6D satellite breakdown */
    fprintf(stderr, "L6D by PRN: ");
    for (i = 0; i <= MAXSAT; i++) {
        if (l6d_prn_count[i] > 0) {
            int sprn;
            satsys(i, &sprn);
            fprintf(stderr, "J%d=%d ", sprn, l6d_prn_count[i]);
        }
    }
    fprintf(stderr, "(filter=J");
    if (l6d_prn_filter > 0) {
        int sprn;
        satsys(l6d_prn_filter, &sprn);
        fprintf(stderr, "%d", sprn);
    }
    fprintf(stderr, ")\n");
    fprintf(stderr, "Skipped: nopos=%d notime=%d nogeom=%d noosr=%d\n",
            dbg_nopos, dbg_notime, dbg_nogeom, dbg_noosr);
    fprintf(stderr, "Total: %d epochs, %d satellite-observations\n",
            epoch_count, osr_count);

cleanup:
    /* close streams */
    strclose(&strm_in);
    strclose(&strm_out);
    if (pos_type != STR_NONE) strclose(&strm_pos);
    if (ch2_type != STR_NONE) strclose(&strm_2ch);

    /* free resources */
    rtkfree(&rtk);
    if (rtcm) { free_rtcm(rtcm); free(rtcm); }
    if (nav) { freenav(nav, 0xFF); free(nav); }
    if (clas) { clas_ctx_free(clas); free(clas); }
    if (raw_sbf) free_raw(raw_sbf);
    free(raw_sbf);
    free(obsdata);
    free(osr);

    traceclose(NULL);

    return 0;
}
