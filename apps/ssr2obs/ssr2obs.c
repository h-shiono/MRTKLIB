/*------------------------------------------------------------------------------
 * ssr2obs.c : convert Compact SSR to RINEX3 OBS (VRS pseudo-observations)
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
 * @file ssr2obs.c
 * @brief Convert Compact SSR to RINEX3 OBS (VRS pseudo-observations).
 *
 * Notes:
 *   Verbatim port of upstream claslib util/ssr2obs/ssr2obs.c.
 *   Generates VRS (Virtual Reference Station) pseudo-observations from
 *   navigation data + QZSS L6 CLAS corrections.
 *
 * References:
 *   [1] upstream claslib util/ssr2obs/ssr2obs.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

/* constants and macros ------------------------------------------------------*/

#define PROGNAME    "ssr2obs"
#define PROG_VER    "1.0"

#define OSR_RCORR   1                 /* output mode: range corrections */
#define OSR_RTCM3   2                 /* output mode: RTCM 3 MSM */
#define OSR_RINEX   3                 /* output mode: RINEX OBS */

#define OSR_SYS     (SYS_GPS|SYS_QZS|SYS_GAL)
#define OSR_NFREQ   4
#define OSR_ELMASK  0.0
#define OSR_RNXVER  302            /* RINEX version x100 */
#define OSR_MARKER  "CLAS_FOR_VRS"

#define MAXFILE     16
#define OUT_FILE    "out.txt"

#ifndef CLIGHT
#define CLIGHT      299792458.0
#endif
#ifndef D2R
#define D2R         (3.1415926535897932384626433832795/180.0)
#endif

/* global variables ----------------------------------------------------------*/

static const char *usage[] = {
    "usage: ssr2obs [options] file ...",
    "",
    "options: ([] as default)",
    "  -ts y/m/d h:m:s   start time of OBS (GPST) []",
    "  -te y/m/d h:m:s   end time of OBS   (GPST) [start time + 1h]",
    "  -ti tint          time interval (s) [1]",
    "  -k  file          configuration file []",
    "  -o  file          output file [" OUT_FILE "]",
    "  -r                output RINEX3 OBS (default)",
    "  -b                output RTCM3 MSM4 binary",
    "  -c  file          also write OSR corrections CSV to file",
    "  -x                debug trace level (0: no trace) [0]",
    "  file ...          QZSS L6 message file (.l6/.L6) and RINEX NAV files",
    NULL
};

#define OSR_CSV_HDR \
    "msg,tow,sys,prn,pbias0,pbias1,pbias2," \
    "cbias0,cbias1,cbias2,trop,iono," \
    "CPC0,CPC1,CPC2,PRC0,PRC1,PRC2,sis\n"

static rnxopt_t rnx_opt = {0};

/* find and open L6 file from input list -------------------------------------*/
static FILE *open_L6(char **infile, int n)
{
    FILE *fp;
    char *ext;
    int i;

    for (i = 0; i < n; i++) {
        if (!(ext = strrchr(infile[i], '.'))) {
            continue;
        }
        if (!strcmp(ext, ".l6") || !strcmp(ext, ".L6")) {
            break;
        }
    }
    if (i >= n) {
        fprintf(stderr, "No L6 message file in input files.\n");
        return NULL;
    }
    if (!(fp = fopen(infile[i], "rb"))) {
        fprintf(stderr, "L6 message file open error. %s\n", infile[i]);
        return NULL;
    }
    return fp;
}

/* set RINEX output options --------------------------------------------------*/
static void set_rnxopt(rnxopt_t *opt, char **infile, int n,
                       const prcopt_t *prcopt)
{
    static const int sys[] = {SYS_GPS, SYS_GLO, SYS_GAL, SYS_QZS};
    static const char *tobs[][OSR_NFREQ] = {
        {"1C", "2W", "2X", "5X"}, {"1C", "2P"}, {"1X", "5X"}, {"1C", "2X", "5X"}
    };
    gtime_t time = {0};
    int i, j, nobs;

    opt->rnxver = OSR_RNXVER;
    opt->navsys = prcopt->navsys;
    sprintf(opt->prog, "%s %s", PROGNAME, PROG_VER);
    sprintf(opt->marker, "%s", OSR_MARKER);
    matcpy(opt->apppos, prcopt->ru, 3, 1);
    for (i = 0; i < n && i < MAXCOMMENT; i++) {
        sprintf(opt->comment[i], "infile: %-55.55s", infile[i]);
    }
    opt->tstart = opt->tend = time;

    for (i = 0; i < 4; i++) {
        if (!(sys[i] & opt->navsys)) {
            continue;
        }
        for (j = nobs = 0; j < OSR_NFREQ; j++) {
            if (tobs[i][j] == NULL) {
                continue;
            }
            sprintf(opt->tobs[i][nobs++], "C%s", tobs[i][j]);
            sprintf(opt->tobs[i][nobs++], "L%s", tobs[i][j]);
        }
        opt->nobs[i] = nobs;
    }
}

/* open output file ----------------------------------------------------------*/
static FILE *open_osr(const char *file, nav_t *nav, int mode,
                      char **infile, int n, const prcopt_t *prcopt)
{
    FILE *fp = NULL;

    if (!(fp = fopen(file, mode == OSR_RTCM3 ? "wb" : "w"))) {
        fprintf(stderr, "Output file open error. %s\n", file);
        return NULL;
    }
    if (mode == OSR_RINEX) {
        set_rnxopt(&rnx_opt, infile, n, prcopt);
        outrnxobsh(fp, &rnx_opt, nav);
    }
    return fp;
}

/* close output file ---------------------------------------------------------*/
static void close_osr(FILE *fp, nav_t *nav, int mode)
{
    if (mode == OSR_RINEX) {
        rewind(fp);
        outrnxobsh(fp, &rnx_opt, nav);
    }
    fclose(fp);
}

/* write RINEX observation body ----------------------------------------------*/
static void write_osr(FILE *fp, int mode, const obs_t *obs, nav_t *nav)
{
    if (mode == OSR_RINEX) {
        if (obs->n > 0) {
            outrnxobsb(fp, &rnx_opt, obs->data, obs->n, 0);
            if (!rnx_opt.tstart.time) {
                rnx_opt.tstart = obs->data[0].time;
            }
            rnx_opt.tend = obs->data[0].time;
        }
    }
}

/* write OSR corrections as CSV (one row per satellite) ----------------------*/
static void output_osr_csv(FILE *fp, gtime_t time, int sat,
                            const clas_osrd_t *osr, int *first)
{
    int sys, prn, week;
    double tow;

    if (*first) { fprintf(fp, OSR_CSV_HDR); *first = 0; }
    sys = satsys(sat, &prn);
    tow = time2gpst(time, &week);
    fprintf(fp, "OSRRES,%.1f,%d,%d,", tow, sys, prn);
    fprintf(fp, "%.3f,%.3f,%.3f,", osr->pbias[0], osr->pbias[1], osr->pbias[2]);
    fprintf(fp, "%.3f,%.3f,%.3f,", osr->cbias[0], osr->cbias[1], osr->cbias[2]);
    fprintf(fp, "%.3f,%.3f,",       osr->trop, osr->iono);
    fprintf(fp, "%.3f,%.3f,%.3f,", osr->CPC[0], osr->CPC[1], osr->CPC[2]);
    fprintf(fp, "%.3f,%.3f,%.3f,", osr->PRC[0], osr->PRC[1], osr->PRC[2]);
    fprintf(fp, "%.3f\n",           osr->sis);
}

/* write VRS observations as RTCM3 MSM4 per constellation -------------------*/
static const int rtcm3_msm_types[] = {1074, 1084, 1094, 1114, 0};
static const int rtcm3_msm_sys[]   = {SYS_GPS, SYS_GLO, SYS_GAL, SYS_QZS, 0};

static void write_rtcm3(FILE *fp, const obs_t *obs, nav_t *nav,
                        const prcopt_t *prcopt)
{
    rtcm_t *rtcm;
    int i, j, k, sys, sync;

    if (obs->n <= 0) {
        return;
    }
    if (!(rtcm = (rtcm_t*)calloc(1, sizeof(rtcm_t)))) {
        return;
    }
    if (!init_rtcm(rtcm)) { free(rtcm); return; }
    rtcm->time = obs->data[0].time;
    matcpy(rtcm->sta.pos, prcopt->ru, 3, 1);

    /* station coordinates message (1005); sync=1 — MSM follows */
    if (gen_rtcm3(rtcm, 1005, 0, 1)) {
        fwrite(rtcm->buff, rtcm->nbyte, 1, fp);
    }

    /* MSM4 per constellation — copy filtered obs into init_rtcm's heap buffer */
    for (k = 0; rtcm3_msm_sys[k]; k++) {
        sys = rtcm3_msm_sys[k];
        rtcm->obs.n = 0;
        for (i = 0; i < obs->n && rtcm->obs.n < MAXOBS; i++) {
            if (satsys(obs->data[i].sat, NULL) & sys) {
                rtcm->obs.data[rtcm->obs.n++] = obs->data[i];
            }
        }
        if (rtcm->obs.n <= 0) {
            continue;
        }

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
            fwrite(rtcm->buff, rtcm->nbyte, 1, fp);
        }
    }
    free_rtcm(rtcm);
    free(rtcm);
}

/* calculate actual geometric distance between receiver and satellites -------*/
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

    for (i = n = 0; i < MAXSAT; i++) {
        if (!nav->ssr_ch[0][i].t0[0].time || !nav->ssr_ch[0][i].t0[1].time) {
            continue;
        }
        lsat[n++] = i + 1;
    }

    for (i = 0; i < 3; i++) {
        rr[i] = x[i];
    }
    if (norm(rr, 3) <= 0.0) {
        return -1;
    }

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

/* generate OSR (main processing loop) --------------------------------------*/
static int gen_osr(gtime_t ts, gtime_t te, double ti, int mode,
                   prcopt_t *prcopt, filopt_t *fopt,
                   char **infile, int n, const char *outfile, FILE *fp_csv)
{
    static rtk_t rtk = {0};
    static obsd_t data[MAXOBS] = {{0}};
    static clas_osrd_t osr[MAXOBS] = {{0}};
    clas_ctx_t *clas;
    nav_t *nav;
    FILE *fp_in, *fp_out;
    obs_t obs = {0};
    gtime_t time;
    char path[1024];
    int i, j, ret, nt;
    int ch = 0;
    int first_csv = 1;

    nt = ti <= 0.0 ? 0 : (int)((timediff(te, ts)) / ti) + 1;

    /* heap-allocate large structures (clas_ctx_t ~5.7MB, nav_t ~2.4MB) */
    clas = (clas_ctx_t *)calloc(1, sizeof(clas_ctx_t));
    nav  = (nav_t *)calloc(1, sizeof(nav_t));
    if (!clas || !nav) {
        fprintf(stderr, "Memory allocation error.\n");
        free(clas); free(nav);
        return -1;
    }

    /* initialize CLAS context */
    if (clas_ctx_init(clas) != 0) {
        fprintf(stderr, "CLAS context init error.\n");
        free(clas); free(nav);
        return -1;
    }

    /* initialize GPS week reference from start time (required for CSSR decoding) */
    {
        int iw, week;
        time2gpst(ts, &week);
        for (iw = 0; iw < CSSR_REF_MAX; iw++) {
            clas->week_ref[iw] = week;
            clas->tow_ref[iw] = -1;
        }
    }

    /* read grid definition file */
    if (clas_read_grid_def(clas, fopt->grid)) {
        fprintf(stderr, "Grid file read error. %s\n", fopt->grid);
        clas_ctx_free(clas); free(clas); free(nav);
        return -1;
    }

    /* read ocean tide loading parameters */
    if (prcopt->tidecorr >= 3 && fopt->blq[0]) {
        if (!readblqgrid(fopt->blq, clas)) {
            fprintf(stderr, "OTL grid read error. %s\n", fopt->blq);
            freenav(nav, 0xFF);
            clas_ctx_free(clas); free(clas); free(nav);
            return -1;
        }
    }

    /* read RINEX NAV files */
    for (i = 0; i < n; i++) {
        char *ext = strrchr(infile[i], '.');
        if (ext && (!strcmp(ext, ".l6") || !strcmp(ext, ".L6"))) {
            continue;
        }
        reppath(infile[i], path, ts, "", "");
        readrnx(path, 0, "", NULL, nav, NULL);
    }
    uniqnav(nav);

    if (nav->n <= 0) {
        fprintf(stderr, "No navigation data.\n");
        clas_ctx_free(clas); free(clas); free(nav);
        return -1;
    }

    /* open QZSS L6 message file */
    if (!(fp_in = open_L6(infile, n))) {
        freenav(nav, 0xFF);
        clas_ctx_free(clas); free(clas); free(nav);
        return -1;
    }

    /* open output file */
    reppath(outfile, path, ts, "", "");
    if (!(fp_out = open_osr(path, nav, mode, infile, n, prcopt))) {
        fclose(fp_in);
        freenav(nav, 0xFF);
        clas_ctx_free(clas); free(clas); free(nav);
        return -1;
    }

    rtkinit(&rtk, prcopt);
    nav->clas_ctx = clas;
    obs.data = data;

    /* initialize position from config (needed for grid lookup in clas_ssr2osr) */
    for (i = 0; i < 3; i++) {
        rtk.sol.rr[i] = prcopt->ru[i];
        rtk.x[i] = prcopt->ru[i];
    }

    /* main epoch loop */
    for (i = 0; i < nt; i++) {
        time = timeadd(ts, ti * i);
        rtk.sol.time = time;

        /* read compact SSR from L6 message file until current time */
        while (timediff(clas->l6buf[ch].time, time) < 1E-3) {
            ret = clas_input_cssrf(clas, fp_in, ch);
            if (ret < -1) {
                break; /* EOF */
            }
            if (ret == 10) {
                /* subtype 10 (service info) received — update corrections */
                int net = clas->grid[ch].network;
                if (net > 0) {
                    if (clas_bank_get_close(clas, clas->l6buf[ch].time,
                                            net, ch, &clas->current[ch]) == 0) {
                        clas_update_global(nav, &clas->current[ch], ch);
                        clas_check_grid_status(clas, &clas->current[ch], ch);
                    }
                }
            }
        }

        /* bootstrap: scan all networks when network is unknown */
        if (clas->grid[ch].network <= 0 && clas->bank[ch] && clas->bank[ch]->use) {
            clas_corr_t *tmp = (clas_corr_t *)calloc(1, sizeof(clas_corr_t));
            if (tmp) {
                int net;
                for (net = 1; net < CLAS_MAX_NETWORK; net++) {
                    if (clas_bank_get_close(clas, clas->l6buf[ch].time,
                                            net, ch, tmp) == 0) {
                        clas_check_grid_status(clas, tmp, ch);
                        clas_update_global(nav, tmp, ch);
                    }
                }
                free(tmp);
            }
        }

        /* generate dummy observations from satellite geometry */
        if (actualdist(time, &obs, nav, prcopt->ru) < 0) {
            continue;
        }

        /* convert SSR to OSR */
        obs.n = clas_ssr2osr(&rtk, obs.data, obs.n, nav, osr, 0, clas);

        if (obs.n > 0) {
            if (mode == OSR_RTCM3) {
                write_rtcm3(fp_out, &obs, nav, prcopt);
            } else {
                write_osr(fp_out, mode, &obs, nav);
            }
            if (fp_csv) {
                for (j = 0; j < obs.n; j++) {
                    output_osr_csv(fp_csv, obs.data[j].time,
                                   obs.data[j].sat, &osr[j], &first_csv);
                }
            }
        }
    }

    rtkfree(&rtk);
    close_osr(fp_out, nav, mode);
    fclose(fp_in);
    freenav(nav, 0xFF);
    clas_ctx_free(clas); free(clas); free(nav);

    return 0;
}

/* set processing options ----------------------------------------------------*/
static int set_prcopt(const char *file, prcopt_t *prcopt, solopt_t *solopt,
                      filopt_t *filopt, double *pos)
{
    prcopt->mode    = PMODE_SSR2OSR;
    prcopt->nf      = OSR_NFREQ;
    prcopt->navsys  = OSR_SYS;
    prcopt->elmin   = OSR_ELMASK * D2R;
    prcopt->tidecorr = 1;
    prcopt->posopt[2] = 1; /* phase windup correction */
    solopt->timef = 0;

    if (*file) {
        setsysopts(prcopt, solopt, filopt);
        if (!loadopts(file, sysopts)) {
            fprintf(stderr, "Configuration file read error. %s\n", file);
            return 0;
        }
        getsysopts(prcopt, solopt, filopt);
    }
    if (norm(pos, 3) > 0.0) {
        pos[0] *= D2R;
        pos[1] *= D2R;
        pos2ecef(pos, prcopt->ru);
    }
    if (norm(prcopt->ru, 3) <= 0.0) {
        fprintf(stderr, "No user position specified.\n");
        return 0;
    }
    return 1;
}

/* main ----------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    prcopt_t prcopt = prcopt_default;
    solopt_t solopt = solopt_default;
    filopt_t filopt = {""};
    gtime_t ts = {0}, te = {0};
    double es[6] = {0}, ee[6] = {0}, ti = 1.0, pos[3] = {0};
    char *conffile = "", *outfile = OUT_FILE, *csvfile = NULL;
    char *infile[MAXFILE];
    FILE *fp_csv = NULL;
    int i, j, n = 0, stat, mode = OSR_RINEX;

    prcopt.mode = PMODE_SSR2OSR;
    prcopt.navsys = SYS_GPS | SYS_QZS | SYS_GAL;
    prcopt.refpos = 1;
    solopt.timef = 0;

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
        } else if (!strcmp(argv[i], "-ti") && i + 1 < argc) {
            ti = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
            sscanf(argv[++i], "%lf,%lf,%lf", pos, pos + 1, pos + 2);
        } else if (!strcmp(argv[i], "-k") && i + 1 < argc) {
            conffile = argv[++i];
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            outfile = argv[++i];
        } else if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            csvfile = argv[++i];
        } else if (!strcmp(argv[i], "-x") && i + 1 < argc) {
            solopt.trace = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-r")) {
            mode = OSR_RINEX;
        } else if (!strcmp(argv[i], "-b")) {
            mode = OSR_RTCM3;
        } else if (!strncmp(argv[i], "-", 1)) {
            for (j = 0; usage[j]; j++) {
                fprintf(stderr, "%s\n", usage[j]);
            }
            return 0;
        } else if (n < MAXFILE) {
            infile[n++] = argv[i];
        }
    }

    if (norm(es, 3) <= 0.0) {
        fprintf(stderr, "No start time specified.\n");
        return -1;
    }
    if (norm(ee, 3) <= 0.0) {
        te = timeadd(epoch2time(es), 3599.0);
    }
    if (!set_prcopt(conffile, &prcopt, &solopt, &filopt, pos)) {
        return -1;
    }

    if (solopt.trace > 0) {
        traceopen(NULL, "ssr2obs.trace");
        tracelevel(NULL, solopt.trace);
    }
    if (csvfile && !(fp_csv = fopen(csvfile, "w"))) {
        fprintf(stderr, "CSV file open error. %s\n", csvfile);
        return -1;
    }

    stat = gen_osr(ts, te, ti, mode, &prcopt, &filopt, infile, n, outfile, fp_csv);

    if (fp_csv) {
        fclose(fp_csv);
    }
    traceclose(NULL);
    return stat;
}
