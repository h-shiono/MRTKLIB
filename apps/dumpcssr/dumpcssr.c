/*------------------------------------------------------------------------------
 * dumpcssr.c : dump decoded Compact SSR (CSSR) corrections to CSV
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2015- Mitsubishi Electric Corp.
 * Copyright (C) 2014 Geospatial Information Authority of Japan
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file dumpcssr.c
 * @brief Dump decoded Compact SSR (CSSR) corrections to CSV.
 *
 * Notes:
 *   Reads a QZSS L6 binary file, decodes CSSR message bundles, and writes
 *   a CSV row for each satellite at each epoch boundary (subtype 10).
 *   Unlike the upstream dumpcssr (14 separate files via decoder-internal
 *   fprintf), this tool dumps the post-decode nav->ssr_ch[] state as a
 *   single CSV file.
 *
 * Usage:
 *   dumpcssr [options] l6file [navfile...]
 *     -ts y/m/d h:m:s   start time (GPST)
 *     -te y/m/d h:m:s   end time   (GPST)
 *     -k  file           configuration file
 *     -o  file           output CSV file [stdout]
 *     -ch n              L6 channel 0 or 1 [0]
 *     -x  level          trace level [0]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_const.h"
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
#include "mrtklib/mrtk_clas.h"

/* constants -----------------------------------------------------------------*/

#define PROGNAME    "dumpcssr"
#define PROG_VER    "1.0"
#define MAXFILE     16
#define CSSR_CH_MAX 2

/* cbias/pbias indices for representative CLAS signals */
#define IDX_L1C  (CODE_L1C - 1)   /* L1C/A  */
#define IDX_L2W  (CODE_L2W - 1)   /* L2 Z-track (GPS) / L2X (QZS) approx. */
#define IDX_L5X  (CODE_L5X - 1)   /* L5I+Q  */

#define DUMP_HDR \
    "tow,ch,sys,prn," \
    "orb_r,orb_a,orb_c," \
    "dclk0,dclk1,dclk2," \
    "cb_L1C,cb_L2W,cb_L5X," \
    "pb_L1C,pb_L2W,pb_L5X," \
    "iode\n"

/* showmsg / settspan / settime stubs required by mrtklib ---------------------*/
extern int showmsg(const char *format, ...)
{
    va_list arg;
    va_start(arg, format); vfprintf(stderr, format, arg); va_end(arg);
    fprintf(stderr, "\r");
    return 0;
}
extern void settspan(gtime_t ts, gtime_t te) { (void)ts; (void)te; }
extern void settime(gtime_t time) { (void)time; }

/* dump nav->ssr_ch state at one epoch to CSV --------------------------------*/
static void dump_ssr_state(FILE *fp, gtime_t time, const nav_t *nav, int ch)
{
    const ssr_t *ssr;
    int sat, sys, prn, week;
    double tow;

    tow = time2gpst(time, &week);
    for (sat = 1; sat <= MAXSAT; sat++) {
        ssr = &nav->ssr_ch[ch][sat - 1];
        /* skip if no orbit or clock data yet */
        if (!ssr->t0[0].time && !ssr->t0[1].time) {
            continue;
        }
        sys = satsys(sat, &prn);
        fprintf(fp, "%.1f,%d,%d,%d,",   tow, ch, sys, prn);
        fprintf(fp, "%.4f,%.4f,%.4f,",  ssr->deph[0], ssr->deph[1], ssr->deph[2]);
        fprintf(fp, "%.6f,%.6f,%.6f,",  ssr->dclk[0], ssr->dclk[1], ssr->dclk[2]);
        fprintf(fp, "%.4f,%.4f,%.4f,",  ssr->cbias[IDX_L1C],
                                         ssr->cbias[IDX_L2W],
                                         ssr->cbias[IDX_L5X]);
        fprintf(fp, "%.4f,%.4f,%.4f,",  ssr->pbias[IDX_L1C],
                                         ssr->pbias[IDX_L2W],
                                         ssr->pbias[IDX_L5X]);
        fprintf(fp, "%d\n", ssr->iode);
    }
}

/* find L6 file in input list ------------------------------------------------*/
static FILE *open_l6(char **infile, int n)
{
    FILE *fp;
    const char *ext;
    int i;

    for (i = 0; i < n; i++) {
        ext = strrchr(infile[i], '.');
        if (ext && (!strcmp(ext, ".l6") || !strcmp(ext, ".L6"))) {
            break;
        }
    }
    if (i >= n) {
        fprintf(stderr, "No L6 file in input files.\n");
        return NULL;
    }
    if (!(fp = fopen(infile[i], "rb"))) {
        fprintf(stderr, "L6 file open error: %s\n", infile[i]);
        return NULL;
    }
    return fp;
}

/* main ----------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    clas_ctx_t *clas;
    nav_t *nav;
    clas_corr_t *tmp;
    gtime_t ts = {0}, te = {0}, t;
    double es[6] = {0}, ee[6] = {0};
    char *infile[MAXFILE], *outfile = NULL, *conffile = "";
    FILE *fp_in, *fp_out;
    char path[1024];
    int i, iw, n = 0, ret, net, ch = 0, trace_level = 0;

    /* parse command-line options */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-ts") && i + 2 < argc) {
            sscanf(argv[++i], "%lf/%lf/%lf", es, es + 1, es + 2);
            sscanf(argv[++i], "%lf:%lf:%lf", es + 3, es + 4, es + 5);
            ts = epoch2time(es);
        }
        else if (!strcmp(argv[i], "-te") && i + 2 < argc) {
            sscanf(argv[++i], "%lf/%lf/%lf", ee, ee + 1, ee + 2);
            sscanf(argv[++i], "%lf:%lf:%lf", ee + 3, ee + 4, ee + 5);
            te = epoch2time(ee);
        } else if (!strcmp(argv[i], "-k") && i + 1 < argc) {
            conffile = argv[++i];
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            outfile = argv[++i];
        } else if (!strcmp(argv[i], "-ch") && i + 1 < argc) {
            ch = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-x") && i + 1 < argc) {
            trace_level = atoi(argv[++i]);
        } else if (argv[i][0] == '-') {
            fprintf(stderr,
                "usage: %s [options] l6file [navfile...]\n"
                "  -ts y/m/d h:m:s   start time (GPST)\n"
                "  -te y/m/d h:m:s   end time   (GPST)\n"
                "  -k  file          configuration file\n"
                "  -o  file          output CSV [stdout]\n"
                "  -ch n             L6 channel 0|1 [0]\n"
                "  -x  level         trace level [0]\n",
                PROGNAME);
            return 0;
        } else if (n < MAXFILE) {
            infile[n++] = argv[i];
        }
    }
    if (n <= 0) {
        fprintf(stderr, "error: no input file\n");
        return -1;
    }
    if (ch < 0 || ch >= CSSR_CH_MAX) {
        fprintf(stderr, "error: invalid channel %d (0 or 1)\n", ch);
        return -1;
    }

    /* allocate large structures on heap */
    clas = (clas_ctx_t *)calloc(1, sizeof(clas_ctx_t));
    nav  = (nav_t *)calloc(1, sizeof(nav_t));
    tmp  = (clas_corr_t *)calloc(1, sizeof(clas_corr_t));
    if (!clas || !nav || !tmp) {
        fprintf(stderr, "Memory allocation error.\n");
        free(clas); free(nav); free(tmp);
        return -1;
    }

    /* initialize CLAS context */
    if (clas_ctx_init(clas) != 0) {
        fprintf(stderr, "CLAS context init error.\n");
        free(clas); free(nav); free(tmp);
        return -1;
    }

    /* initialize GPS week reference from start time */
    if (ts.time) {
        int week;
        time2gpst(ts, &week);
        for (iw = 0; iw < CSSR_REF_MAX; iw++) {
            clas->week_ref[iw] = week;
            clas->tow_ref[iw]  = -1;
        }
    }

    /* optional configuration file */
    if (*conffile) {
        prcopt_t prcopt = prcopt_default;
        solopt_t solopt = solopt_default;
        filopt_t filopt = {""};
        setsysopts(&prcopt, &solopt, &filopt);
        if (!loadopts(conffile, sysopts)) {
            fprintf(stderr, "Config file read error: %s\n", conffile);
            clas_ctx_free(clas); free(clas); free(nav); free(tmp);
            return -1;
        }
        getsysopts(&prcopt, &solopt, &filopt);
        /* read grid definition if available */
        if (filopt.grid[0]) {
            clas_read_grid_def(clas, filopt.grid);
        }
    }

    /* read RINEX NAV files (optional, for broadcast eph in orbit calcs) */
    for (i = 0; i < n; i++) {
        gtime_t t0 = {0};
        const char *ext = strrchr(infile[i], '.');
        if (ext && (!strcmp(ext, ".l6") || !strcmp(ext, ".L6"))) {
            continue;
        }
        reppath(infile[i], path, ts.time ? ts : t0, "", "");
        readrnx(path, 0, "", NULL, nav, NULL);
    }
    if (nav->n > 0) {
        uniqnav(nav);
    }

    /* open L6 file */
    if (!(fp_in = open_l6(infile, n))) {
        freenav(nav, 0xFF);
        clas_ctx_free(clas); free(clas); free(nav); free(tmp);
        return -1;
    }

    /* open output file */
    fp_out = outfile ? fopen(outfile, "w") : stdout;
    if (!fp_out) {
        fprintf(stderr, "Output file open error: %s\n", outfile);
        fclose(fp_in);
        freenav(nav, 0xFF);
        clas_ctx_free(clas); free(clas); free(nav); free(tmp);
        return -1;
    }

    if (trace_level > 0) {
        traceopen(NULL, "dumpcssr.trace");
        tracelevel(NULL, trace_level);
    }

    /* write CSV header */
    fprintf(fp_out, DUMP_HDR);

    /* main decode loop */
    while ((ret = clas_input_cssrf(clas, fp_in, ch)) > -2) {
        if (ret < 0) {
            continue;
        }
        if (ret != 10) {
            continue; /* wait for service info (end of bundle) */
        }

        t = clas->l6buf[ch].time;

        /* time filter */
        if (ts.time && timediff(t, ts) < -0.5) {
            continue;
        }
        if (te.time && timediff(t, te) > 0.5) {
            break;
        }

        /* update nav->ssr_ch from first available network */
        for (net = 1; net < CLAS_MAX_NETWORK; net++) {
            if (clas_bank_get_close(clas, t, net, ch, tmp) == 0) {
                clas_update_global(nav, tmp, ch);
                break;
            }
        }

        dump_ssr_state(fp_out, t, nav, ch);
    }

    /* cleanup */
    if (fp_out != stdout) {
        fclose(fp_out);
    }
    fclose(fp_in);
    freenav(nav, 0xFF);
    clas_ctx_free(clas); free(clas); free(nav); free(tmp);
    traceclose(NULL);
    return 0;
}
