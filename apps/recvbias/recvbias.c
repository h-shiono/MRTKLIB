/*------------------------------------------------------------------------------
 * recvbias.c : generate receiver bias
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
 *-----------------------------------------------------------------------------*/
/**
 * @file recvbias.c
 * @brief Generate receiver bias from QZSS L6E SSR or RTCM3 corrections.
 *
 * History:
 *   2024/12/20  1.0  new, for MALIB from madoca ver.2.0.2 rcvdcb
 *   2025/02/06  1.1  change nav_t to static due to size increase
 *   2025/03/24  1.2  add option -allbias; update code bias calculation
 */
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_trace.h"
#include "rtklib.h"

#ifndef PROGNAME
#define PROGNAME "recvbias"
#endif
#define SQR(x) ((x) * (x))

#define MAXAGESSRRTCM 86400
#define BTYPE_L6 1
#define BTYPE_RTCM 2
#define BTYPE_BIA 3

typedef struct {
    int cnt;
    char staname[32];
    int sysno;
    int sat;
    int code; /* signal CODE_xxx */
    double ave;
    double var;
} rawbias_t;

static int nsatrb, nsatrbmax;
static int nsysrb, nsysrbmax;
static rawbias_t* satrb;
static rawbias_t* sysrb;
static gtime_t st = {0}, et = {0};
static rtcm_t* rtcm;      /* rtcm control struct (heap, ~103MB) */
static int selsiggps = 0; /* 0:L1C/A-L2P,1:L1C/A-L2C,2:L1C/A-L5 */
static int selsigqzs = 0; /* 0:L1C-L5,1:L1C/A-L2C */
static int selsigcmp = 0; /* 0:B1-B3,1:B1C-B2a */

static const uint8_t malib_sig_gps[16] = {CODE_L1C, CODE_L2P, CODE_L2Y, CODE_L2W, CODE_L2C, CODE_L2M,
                                          CODE_L2N, CODE_L2D, CODE_L2L, CODE_L2X, CODE_L2S, CODE_L5I,
                                          CODE_L5Q, CODE_L5X, 0,        0};
static const uint8_t malib_sig_glo[16] = {CODE_L1C, CODE_L1P, CODE_L2P, CODE_L2C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t malib_sig_gal[16] = {CODE_L1C, CODE_L1B, CODE_L1X, CODE_L5Q, CODE_L5X, CODE_L5I, 0, 0,
                                          0,        0,        0,        0,        0,        0,        0, 0};
static const uint8_t malib_sig_qzs[16] = {CODE_L1C, CODE_L1L, CODE_L1X, CODE_L1S, CODE_L2L, CODE_L2X,
                                          CODE_L2S, CODE_L5Q, CODE_L5X, CODE_L5I, 0,        0,
                                          0,        0,        0,        0};
static const uint8_t malib_sig_cmp[16] = {CODE_L2Q, CODE_L2X, CODE_L2I, CODE_L6Q, CODE_L6X, CODE_L6I,
                                          CODE_L1P, CODE_L1X, CODE_L1D, CODE_L5P, CODE_L5X, CODE_L5D,
                                          0,        0,        0,        0};

/* print usage ---------------------------------------------------------------*/
static const char* help[] = {"",
                             "NAME:",
                             "  recvbias - generate receiver code biases",
                             "",
                             "SYNOPSIS:",
                             "  recvbias [options] file",
                             "",
                             "DESCRIPTION:",
                             "  It reads RINEX OBS/NAV, IONEX TEC data, and satellite code biases, ",
                             " and generates receiver code biases. The generated receiver code biases, ",
                             " as well as input satellite code biases (only when Bias SINEX format files ",
                             " are input), are saved to a file specified with option -out. All file paths",
                             " can include keywords replaced by date and time. RINEX OBS/NAV and ",
                             " IONEX TEC files can include wildcards (*) to be expanded to multiple files.",
                             "",
                             "  The program supports the following input file formats:",
                             "    - Bias SINEX format (.bia)",
                             "    - L6 archive (.l6)",
                             "    - RTCM3 (.rtcm3)",
                             "",
                             "OPTIONS: (* indicates required options)",
                             " -td y/m/d        date (y=year,m=month,d=date in GPST) *",
                             " -ts tspan        time span (days) [1]",
                             " -nav file        RINEX NAV file *",
                             " -tec file        IONEX TEC file *",
                             " -scb file        satellite code bias file(.bia or .l6 or .rtcm3) *",
                             " -pos x,y,z       receiver position(ecef-x,y,z）*",
                             " -el  elmask      elevation mask(deg)[30]",
                             " -sta staname     station name[RINEX MARKER NAME]",
                             " -sig sx[,sx...]  signal selection(s=G:GPS,J:QZS,C:BDS x=0-2)",
                             "                  G0(L1C/A-L2P),G1(L1C/A-L2C),G2(L1C/A-L5)",
                             "                  J0(L1C-L5),J1(L1C/A-L2C)",
                             "                  C0(B1-B3),C1(B1C-B2a)",
                             " -allbias         outputs receiver code biases for each satellite",
                             " -out file        output satellite/receiver code bias file [stdout]",
                             " -t   level       debug trace level (0:off) [0]",
                             " file             RINEX OBS file *",
                             "",
                             " keywords replaced in file path",
                             "   %Y -> yyyy     year (4 digits) (2000-2099)",
                             "   %y -> yy       year (2 digits) (00-99)",
                             "   %m -> mm       month           (01-12)",
                             "   %d -> dd       day of month    (01-31)",
                             "   %h -> hh       hours           (00-23)",
                             "   %H -> a        hour code       (a-x)",
                             "   %M -> mm       minutes         (00-59)",
                             "   %n -> ddd      day of year     (001-366)",
                             "   %W -> wwww     gps week        (0001-9999)",
                             "   %D -> d        day of gps week (0-6)",
                             ""};

/* print help ----------------------------------------------------------------*/
static void print_help(void) {
    int i;

    for (i = 0; i < sizeof(help) / sizeof(*help); i++) {
        fprintf(stderr, "%s\n", help[i]);
    }
    exit(0);
}

/* get filenname -------------------------------------------------------------*/
const char* get_filename(const char* path) {
    const char* filename = strrchr(path, '/');

    if (filename == NULL) {
        return path;
    } else {
        return filename + 1;
    }
}

/* update raw bias  ----------------------------------------------------------*/
static int uprawbias(char* staname, int sat, int code, double rawbias) {
    int i, sysno;
    rawbias_t* b;
    double delta, delta2;
    char satid[8];
    char syscode[] = "GREJCIS";

    satno2id(sat, satid);
    for (sysno = 0; sysno < MAXBSNXSYS; sysno++) {
        if (satid[0] == syscode[sysno]) {
            break;
        }
    }
    if (sysno >= MAXBSNXSYS) {
        trace(NULL, 1, "uprawbias: satellite system error %s\n", satid);
        return 0;
    }

    /* satellite */
    for (i = 0; i < nsatrb; i++) {
        if (satrb[i].sat == sat && satrb[i].code == code && strcmp(satrb[i].staname, staname) == 0) {
            break;
        }
    }

    if (nsatrb == i) {
        if (nsatrbmax <= nsatrb) {
            nsatrbmax += 256;
            if (!(b = (rawbias_t*)realloc(satrb, sizeof(rawbias_t) * (nsatrbmax)))) {
                trace(NULL, 1, "uprawbias: memory allocation error\n");
                free(satrb);
                satrb = NULL;
                nsatrb = nsatrbmax = 0;
                return 0;
            }
            satrb = b;
        }
        satrb[i].cnt = 0;
        satrb[i].sysno = sysno;
        satrb[i].sat = sat;
        satrb[i].code = code;
        satrb[i].ave = 0.0;
        satrb[i].var = 0.0;
        strcpy(satrb[i].staname, staname);
        nsatrb += 1;
    }
    satrb[i].cnt += 1;

    delta = rawbias - satrb[i].ave;
    satrb[i].ave += delta / satrb[i].cnt;

    delta2 = rawbias - satrb[i].ave;
    satrb[i].var += delta * delta2;

    /* system */
    for (i = 0; i < nsysrb; i++) {
        if (sysrb[i].sysno == sysno && sysrb[i].code == code && strcmp(sysrb[i].staname, staname) == 0) {
            break;
        }
    }

    if (nsysrb == i) {
        if (nsysrbmax <= nsysrb) {
            nsysrbmax += 256;
            if (!(b = (rawbias_t*)realloc(sysrb, sizeof(rawbias_t) * (nsysrbmax)))) {
                trace(NULL, 1, "uprawbias: memory allocation error\n");
                free(sysrb);
                sysrb = NULL;
                nsysrb = nsysrbmax = 0;
                return 0;
            }
            sysrb = b;
        }
        sysrb[i].cnt = 0;
        sysrb[i].sysno = sysno;
        sysrb[i].code = code;
        sysrb[i].ave = 0.0;
        sysrb[i].var = 0.0;
        strcpy(sysrb[i].staname, staname);
        nsysrb += 1;
    }
    sysrb[i].cnt += 1;

    delta = rawbias - sysrb[i].ave;
    sysrb[i].ave += delta / sysrb[i].cnt;

    delta2 = rawbias - sysrb[i].ave;
    sysrb[i].var += delta * delta2;

    return 1;
}
/* update Code/Phase Bias corrections ----------------------------------------*/
static void update_bias(const char* biafile, osb_t* osb, gtime_t time) {
    static char bsnx_path[1024] = ""; /* bias sinex data path */
    char path[MAXSTRPATH];
    int ret = 0;

    /* open or swap bia file */
    reppath(biafile, path, time, "", "");

    if (strcmp(path, bsnx_path)) {
        strcpy(bsnx_path, path);

        if ((ret = readbsnx(bsnx_path)) == 0) {
            trace(NULL, 2, "read bia file error: %s num=%d\n", path, ret);
        }
        trace(NULL, 4, "read bia file: %s num=%d\n", path, ret);
    }

    if ((ret = udosb_sat(osb, time, 0)) == 0) {
        trace(NULL, 2, "update satellite osb error: %s num=%d\n", time_str(time, 3), ret);
    }
    trace(NULL, 4, "update satellite osb: %s num=%d\n", time_str(time, 3), ret);
}
/* update rtcm ssr correction ------------------------------------------------*/
static void update_rtcm_ssr(const char* file, nav_t* nav, gtime_t time) {
    static char rtcm_path[MAXSTRPATH] = ""; /* rtcm data path */
    static FILE* fp_rtcm = NULL;            /* rtcm data file pointer */
    char path[MAXSTRPATH], tstr[32];
    int i;

    /* open or swap rtcm file */
    reppath(file, path, time, "", "");

    if (strcmp(path, rtcm_path)) {
        strcpy(rtcm_path, path);

        if (fp_rtcm) {
            fclose(fp_rtcm);
        }
        fp_rtcm = fopen(path, "rb");
        if (fp_rtcm) {
            rtcm->time = time;
            input_rtcm3f(rtcm, fp_rtcm);
            trace(NULL, 2, "rtcm file open: %s\n", path);
        }
    }
    if (!fp_rtcm) {
        return;
    }

    /* read rtcm file until current time */
    while (timediff(rtcm->time, time) < 1E-3) {
        strcpy(tstr, time_str(rtcm->time, 3));
        if (input_rtcm3f(rtcm, fp_rtcm) < -1) {
            break;
        }
        trace(NULL, 3, "update_rtcm_ssr: %s %s\n", time_str(time, 3), tstr);

        /* update ssr corrections */
        for (i = 0; i < MAXSAT; i++) {
            if (!rtcm->ssr[i].update || rtcm->ssr[i].iod[0] != rtcm->ssr[i].iod[1] ||
                timediff(time, rtcm->ssr[i].t0[0]) < -1E-3) {
                continue;
            }
            nav->ssr_ch[0][i] = rtcm->ssr[i];
            rtcm->ssr[i].update = 0;
        }
    }
}
/* update QZSS L6E MADOCA-PPP corrections ------------------------------------*/
static void update_qzssl6e(const char* file, nav_t* nav, gtime_t gt) {
    static int init_flg = 1;
    char path[MAXSTRPATH], tstr[32];
    static char qzssl6e_path[MAXSTRPATH] = ""; /* QZSS L6E data path */
    static FILE* fp_qzssl6e = NULL;            /* QZSS L6E data file pointer */
    int i;

    /* open or swap QZSS L6E file, record bit size is 1744(w/o R-S) or 2000 */
    reppath(file, path, gt, "", "");

    if (strcmp(path, qzssl6e_path)) {
        strcpy(qzssl6e_path, path);

        if (fp_qzssl6e) {
            fclose(fp_qzssl6e);
        }
        fp_qzssl6e = fopen(path, "rb");
        if (fp_qzssl6e) {
            trace(NULL, 2, "qzssl6e file open: %s\n", path);
        }
    }
    if (!fp_qzssl6e) {
        return;
    }

    if (init_flg) {
        init_mcssr(gt);
        init_flg = 0;
    }

    while (timediff(rtcm->time, gt) < 1E-3) {
        strcpy(tstr, time_str(rtcm->time, 3));
        trace(NULL, 3, "update_qzssl6e: %s %s\n", time_str(gt, 3), tstr);

        /* update QZSS L6E MADOCA-PPP corrections */
        for (i = 0; i < MAXSAT; i++) {
            if (!rtcm->ssr[i].update || rtcm->ssr[i].iod[0] != rtcm->ssr[i].iod[1] ||
                timediff(gt, rtcm->ssr[i].t0[0]) < -1E-3) {
                continue;
            }
            nav->ssr_ch[0][i] = rtcm->ssr[i];
            rtcm->ssr[i].update = 0;
        }
        if (input_qzssl6ef(rtcm, fp_qzssl6e) < -1) {
            break;
        }
    }
}

/* update satellite code bias ------------------------------------------------*/
static void udsatcb(gtime_t gt, nav_t* nav, osb_t* biaosb, int btype) {
    int i, j, ssrcode, sys;
    int udcnt = 0;
    double vp = MAXAGESSRL6;

    /* check the vendor and apply specific settings */
    for (i = 0; i < MAXSAT; i++) {
        if (nav->ssr_ch[0][i].vendor == SSR_VENDOR_RTCM) {
            vp = MAXAGESSRRTCM;
            break;
        } else if (nav->ssr_ch[0][i].vendor == SSR_VENDOR_L6) {
            break;
        }
    }

    /* ssr */
    if (btype == BTYPE_L6 || btype == BTYPE_RTCM) {
        for (i = 0; i < MAXSAT; i++) {
            sys = satsys(i + 1, NULL);
            if (timediff(gt, nav->ssr_ch[0][i].t0[4]) > vp) {
                continue;
            }
            for (j = 0; j < MAXCODE; j++) {
                ssrcode = mcssr_sel_biascode(sys, j + 1);
                if (ssrcode == CODE_NONE) {
                    continue;
                }
                nav->osb.vscb[i][j] = 1;
                nav->osb.scb[i][j] = nav->ssr_ch[0][i].cbias[ssrcode - 1];
                udcnt++;
            }
            if (0 < udcnt) {
                nav->osb.gt[0] = nav->ssr_ch[0][i].t0[4];
            }
        }
    }
    if (0 < udcnt) {
        trace(NULL, 4, "%s ssr update satellite code bias cnt=%d vp=%.f\n", time_str(gt, 0), udcnt, vp);
        return;
    }

    /* bia */
    if (btype == BTYPE_BIA) {
        for (i = 0; i < MAXSAT; i++) {
            for (j = 0; j < MAXCODE; j++) {
                if (biaosb->vscb[i][j] != 0) {
                    udcnt++;
                }
            }
        }
        if (0 < udcnt) {
            memcpy(nav->osb.vscb, biaosb->vscb, sizeof(nav->osb.vscb));
            memcpy(nav->osb.scb, biaosb->scb, sizeof(nav->osb.scb));
            for (i = 0; i < MAXSAT; i++) {
                for (j = 0; j < MAXCODE; j++) {
                    nav->osb.scb[i][j] *= -1;
                }
            }
            nav->osb.gt[0] = biaosb->gt[0];
        }
    }
    if (0 < udcnt) {
        trace(NULL, 4, "%s bia update satellite code bias cnt=%d\n", time_str(gt, 0), udcnt);
        return;
    }

    trace(NULL, 3, "%s no update satellite code bias \n", time_str(gt, 0));
    return;
}

/* update biass --------------------------------------------------------------*/
static void udbiass(const char* scbfile, gtime_t gt, nav_t* nav) {
    osb_t biaosb;
    int btype = 0;
    char* ext;

    /* read satellite code bias file */
    if ((ext = strrchr(scbfile, '.')) != NULL) {
        if (!strcmp(ext, ".l6") || !strcmp(ext, ".L6")) {
            update_qzssl6e(scbfile, nav, gt);
            btype = BTYPE_L6;
        } else if (!strcmp(ext, ".rtcm3") || !strcmp(ext, ".RTCM3")) {
            update_rtcm_ssr(scbfile, nav, gt);
            btype = BTYPE_RTCM;
        } else if (!strcmp(ext, ".bia") || !strcmp(ext, ".BIA")) {
            update_bias(scbfile, &biaosb, gt);
            btype = BTYPE_BIA;
        }
    }

    if (btype == 0) {
        trace(NULL, 2, "update bias file error %s %s\n", time_str(gt, 3), scbfile);
        return;
    }

    /* update satellite code bias and station codebias correction */
    udsatcb(gt, nav, &biaosb, btype);
}

/* get satellite code bias  --------------------------------------------------*/
static int get_cbias(nav_t* nav, int sat, int code, double* bias) {
    int ret = 0;

    *bias = 0.0;

    if (nav->osb.vscb[sat - 1][code - 1] != 0) {
        *bias = nav->osb.scb[sat - 1][code - 1];
        ret = 1;
    }
    return ret;
}

/* signal selection for recvbias ---------------------------------------------*/
static void signal_sel_bias(obsd_t* biasobs, int ns) {
    char satid[8];
    char* tstr = time_str(biasobs->time, 3);
    int i, sys;

    for (i = 0; i < ns; i++) {
        sys = satsys(biasobs->sat, NULL);

        /* GPS=L1C/A-L2P, GLO=G1-G2, GAL=E1-E5a, QZS=L1C-L5 , BDS=B1-B3*/
        switch (sys) {
            case SYS_GPS:
                signal_replace(biasobs, 0, '1', "C");
                if (selsiggps == 0) {                          /* L1C/A-L2P */
                    signal_replace(biasobs, 1, '2', "PYWCMD"); /* Note, codepries="PYWCMDLXS" */
                } else if (selsiggps == 1) {                   /* L1C/A-L2C */
                    signal_replace(biasobs, 1, '2', "LXS");
                } else if (selsiggps == 2) { /* L1C/A-L5 */
                    signal_replace(biasobs, 1, '5', "QXI");
                }
                break;
            case SYS_GLO:
                signal_replace(biasobs, 0, '1', "CP");
                signal_replace(biasobs, 1, '2', "PC");
                break;
            case SYS_GAL:
                signal_replace(biasobs, 0, '1', "CBX");
                signal_replace(biasobs, 1, '5', "QXI");
                break;
            case SYS_QZS:
                if (selsigqzs == 0) {                       /* L1C-L5 */
                    signal_replace(biasobs, 0, '1', "LXS"); /* Note, codepries="CLXS" */
                    signal_replace(biasobs, 1, '5', "QXI");
                } else if (selsigqzs == 1) { /* L1C/A-L2C */
                    signal_replace(biasobs, 0, '1', "C");
                    signal_replace(biasobs, 1, '2', "LXS");
                }
                break;
            case SYS_CMP:
                if (selsigcmp == 0) { /* B1-B3 */
                    signal_replace(biasobs, 0, '2', "QXI");
                    signal_replace(biasobs, 1, '6', "QXI");
                } else if (selsigcmp == 1) { /* B1C-B2a */
                    signal_replace(biasobs, 0, '1', "PXD");
                    signal_replace(biasobs, 1, '5', "PXD");
                }
                break;
        }
        satno2id(biasobs->sat, satid);
        trace(NULL, 3, "signal_sel_bias %s %s code=%2d,%2d P=%13.3f,%13.3f L=%13.3f,%13.3f LLI=%d,%d SNR=%6.2f,%6.2f\n",
              tstr, satid, biasobs->code[0], biasobs->code[1], biasobs->P[0], biasobs->P[1], biasobs->L[0],
              biasobs->L[1], biasobs->LLI[0], biasobs->LLI[1], biasobs->SNR[0] * 0.001, biasobs->SNR[1] * 0.001);
        biasobs++;
    }
}

/* Check if the given code is a valid signal for MALIB -----------------------*/
static int signal_check(int sys, uint8_t code) {
    const uint8_t* sigs;
    int i;

    switch (sys) {
        case SYS_GPS:
            sigs = malib_sig_gps;
            break;
        case SYS_GLO:
            sigs = malib_sig_glo;
            break;
        case SYS_GAL:
            sigs = malib_sig_gal;
            break;
        case SYS_QZS:
            sigs = malib_sig_qzs;
            break;
        case SYS_CMP:
            sigs = malib_sig_cmp;
            break;
        default:
            return 0;
    }
    for (i = 0; i < 16; i++) {
        if (sigs[i] == code) {
            return 1;
        }
    }
    return 0;
}

/* generate receiver bias for a station --------------------------------------*/
static int gen_bias_sta(nav_t* nav, const char* file, char* staname, const char* scbfile, double* ecef, double elmask) {
    static obsd_t sobs[MAXOBS];
    sta_t sta = {{0}};
    obs_t obs = {0};
    obsd_t* data;
    double *rs, *dts, *var, azel[2], e[3], r, P1, P2, ion, ionvar, dcb_t;
    double freq1, freq2, osb1, osb2, llh[3], satcb1, satcb2;
    int svh[MAXOBS], i, j, k, sat, sys;
    int n = 0;
    char satid[8];

    readrnx(file, 1, "", &obs, NULL, &sta);
    fprintf(stderr, "reading... %s%s\n", file, obs.n > 0 ? "" : " (no data)");
    if (strlen(staname) == 0 && strlen(sta.name) > 0) {
        strncpy(staname, sta.name, 4);
    }
    if (obs.n <= 0) {
        return 0;
    }

    for (i = 0; i < obs.n; i += n) {
        data = obs.data + i;
        for (n = 0; i + n < obs.n; n++) {
            if (timediff(data[n].time, data[0].time) > 1E-3) {
                break;
            }
        }
        memcpy(sobs, data, sizeof(obsd_t) * n);
        signal_sel_bias(sobs, n);

        rs = mat(6, n);
        dts = mat(2, n);
        var = mat(1, n);

        /* satellite positons, velocities and clocks */
        satposs(data[0].time, sobs, n, nav, EPHOPT_BRDC, rs, dts, var, svh);

        if (st.time == 0) {
            st = data[0].time;
        }
        et = data[0].time;

        /* update satellite code bias */
        udbiass(scbfile, data[0].time, nav);

        ecef2pos(ecef, llh);

        for (j = 0; j < n; j++) {
            sat = sobs[j].sat;
            satno2id(sat, satid);
            sys = satsys(sat, NULL);
            if ((r = geodist(rs + j * 6, ecef, e)) <= 0.0 || satazel(llh, e, azel) < elmask) {
                continue;
            }

            if (!iontec(data[0].time, nav, llh, azel, 1, &ion, &ionvar)) {
                continue;
            }

            freq1 = sat2freq(sat, sobs[j].code[0], nav);
            freq2 = sat2freq(sat, sobs[j].code[1], nav);
            P1 = sobs[j].P[0];
            P2 = sobs[j].P[1];
            if (freq1 == 0.0 || freq2 == 0.0 || P1 == 0.0 || P2 == 0.0) {
                trace(NULL, 4, "obs error %s freq1=%f freq2=%f P1=%f P2=%f code=%s,%s\n", satid, freq1, freq2, P1, P2,
                      code2obs(sobs[j].code[0]), code2obs(sobs[j].code[1]));
                continue;
            }

            if (get_cbias(nav, sat, sobs[j].code[0], &satcb1) == 0) {
                trace(NULL, 2, "no satellite code bias %s code=%s\n", satid, code2obs(sobs[j].code[0]));
                continue;
            }
            P1 += satcb1;
            if (get_cbias(nav, sat, sobs[j].code[1], &satcb2) == 0) {
                trace(NULL, 2, "no satellite code bias %s code=%s\n", satid, code2obs(sobs[j].code[1]));
                continue;
            }
            P2 += satcb2;

            dcb_t = (P2 - P1) - ion * (SQR(FREQ1 / freq2) - SQR(FREQ1 / freq1));
            osb1 = dcb_t / (SQR(freq1 / freq2) - 1);

            uprawbias(staname, sat, sobs[j].code[0], osb1);
            trace(NULL, 3, "bias(%s-%s) : %s,%s,%s,%s,%10.6f,%10.6f,%6.2f\n", code2obs(sobs[j].code[0]),
                  code2obs(sobs[j].code[1]), time_str(data[0].time, 0), staname, satid, code2obs(sobs[j].code[0]), osb1,
                  satcb1, azel[1] * R2D);

            for (k = 0; k < NFREQ + NEXOBS; k++) {
                freq2 = sat2freq(sat, data[j].code[k], nav);
                P2 = data[j].P[k];
                if (data[j].code[k] == sobs[j].code[0] || data[j].code[k] == CODE_NONE || freq2 == 0.0 || P2 == 0.0 ||
                    signal_check(sys, data[j].code[k]) == 0) {
                    continue;
                }
                if (get_cbias(nav, sat, data[j].code[k], &satcb2) == 0) {
                    continue;
                }
                P2 += satcb2;

                dcb_t = (P2 - P1) - ion * (SQR(FREQ1 / freq2) - SQR(FREQ1 / freq1));
                osb2 = osb1 + dcb_t;
                uprawbias(staname, sat, data[j].code[k], osb2);
                trace(NULL, 3, "bias(%s-%s) : %s,%s,%s,%s,%10.6f,%10.6f,%6.2f\n", code2obs(sobs[j].code[0]),
                      code2obs(data[j].code[k]), time_str(data[0].time, 0), staname, satid, code2obs(data[j].code[k]),
                      osb2, satcb2, azel[1] * R2D);
            }
        }
        free(rs);
        free(dts);
        free(var);
    }
    free(obs.data);
    return 1;
}
/* output bias ---------------------------------------------------------------*/
static void outbsnx(const char* outfile, gtime_t ts, gtime_t te, const char* navfile, const char* obsfile,
                    const char* tecfile, const char* scbfile, double* ecef, double elmask) {
    char buff[81];
    FILE* fp;

    trace(NULL, 3, "outbsnx : file=%s\n", outfile);

    if (!(fp = fopen(outfile, "w"))) {
        trace(NULL, 2, "bias sinex file open error: %s\n", outfile);
        return;
    }
    outbsnxh(fp, ts, te, "MLB");

    outbsnxrefh(fp);
    outbsnxrefb(fp, 0, "Receiver Code biases estimated by MRTKLIB");
    snprintf(buff, sizeof(buff), "%s(%s ver.%s)", PROGNAME, MRTKLIB_SOFTNAME, MRTKLIB_VERSION_STRING);
    outbsnxrefb(fp, 3, buff);
    outbsnxreff(fp);

    outbsnxcomh(fp);
    snprintf(buff, sizeof(buff), "Input OBS: %s", get_filename(obsfile));
    outbsnxcomb(fp, buff);
    snprintf(buff, sizeof(buff), "Input NAV: %s", get_filename(navfile));
    outbsnxcomb(fp, buff);
    snprintf(buff, sizeof(buff), "Input TEC: %s", get_filename(tecfile));
    outbsnxcomb(fp, buff);
    snprintf(buff, sizeof(buff), "Input Sat BIAS: %s", get_filename(scbfile));
    outbsnxcomb(fp, buff);
    snprintf(buff, sizeof(buff), "Reciver Pos: %.3f,%.3f,%.3f", ecef[0], ecef[1], ecef[2]);
    outbsnxcomb(fp, buff);
    snprintf(buff, sizeof(buff), "Elevation Mask: %.1f deg", elmask * R2D);
    outbsnxcomb(fp, buff);
    outbsnxcomf(fp);

    outbsnxsol(fp);
    fclose(fp);
}

/* generate receiver bias ----------------------------------------------------*/
static int gen_bias(gtime_t ts, double tspan, const char* navfile, const char* obsfile, const char* tecfile,
                    const char* scbfile, double* ecef, double elmask, const char* outfile, char* staname, int allb) {
    gtime_t te;
    static nav_t nav = {0}, navi = {0};
    char *files[MAXEXFILE], *paths[MAXEXFILE];
    int i, j, n, m;
    biass_t* biass;
    bia_t newbia;

    biass = getbiass();
    nsatrb = nsatrbmax = nsysrb = nsysrbmax = 0;
    te = timeadd(ts, tspan * 86400.0 - 1.0);
    for (i = 0; i < MAXEXFILE; i++) {
        if (!(files[i] = (char*)malloc(1024)) || !(paths[i] = (char*)malloc(1024))) {
            return 0;
        }
    }

    /* read navigation file */
    n = reppaths(navfile, files, MAXEXFILE, ts, te, "", "");
    for (i = 0; i < n; i++) {
        m = nav.n;
        readrnx(files[i], 1, "", NULL, &nav, NULL);
        fprintf(stderr, "reading... %s%s\n", files[i], nav.n > m ? "" : " (no data)");
    }
    uniqnav(&nav);

    /* read tec file */
    n = reppaths(tecfile, files, MAXEXFILE, ts, te, "", "");
    for (i = 0; i < n; i++) {
        m = navi.nt;
        readtec(files[i], &navi, 1);
        fprintf(stderr, "reading... %s%s\n", files[i], navi.nt > m ? "" : " (no data)");
    }
    for (i = 0; i < MAXSAT; i++) {
        if (navi.cbias[i][0] != 0) {
            nav.cbias[i][0] = navi.cbias[i][0];
        }
    }
    nav.nt = navi.nt;
    nav.tec = navi.tec;

    /* read obsavation file and generate receiver bias */
    n = reppaths(obsfile, files, MAXEXFILE, ts, te, "", "");
    for (i = 0; i < n; i++) {
        m = expath(files[i], paths, MAXEXFILE);
        for (j = 0; j < m; j++) {
            /* generate receiver bias for a station */
            if (!(gen_bias_sta(&nav, paths[j], staname, scbfile, ecef, elmask))) {
                continue;
            }
        }
    }
    for (i = 0; (i < MAXSTA) && (i < biass->nsta); i++) {
        for (j = 0; j < MAXBSNXSYS; j++) {
            free(biass->sta[i].syscb[j]);
            biass->sta[i].syscb[j] = NULL;
            biass->sta[i].nsyscb[j] = 0;
            biass->sta[i].nsyscbmax[j] = 0;
        }
        for (j = 0; j < MAXSAT; j++) {
            free(biass->sta[i].satcb[j]);
            biass->sta[i].satcb[j] = NULL;
            biass->sta[i].nsatcb[j] = 0;
            biass->sta[i].nsatcbmax[j] = 0;
        }
    }

    /* satellite system bias */
    for (i = 0; i < nsysrb; i++) {
        memset(&newbia, 0x00, sizeof(bia_t));
        newbia.type = 2;
        newbia.t0 = st;
        newbia.t1 = et;
        newbia.code[0] = sysrb[i].code;
        newbia.val = sysrb[i].ave;
        newbia.valstd = sqrt(sysrb[i].var / (sysrb[i].cnt - 1));
        for (j = 0; (j < MAXSTA) && (j < biass->nsta); j++) {
            if (strcmp(sysrb[i].staname, biass->sta[j].name) == 0) {
                break;
            }
        }
        if (j >= MAXSTA) {
            trace(NULL, 2, "gen_bias %s has exceeded the station limit of %d.\n", sysrb[i].staname, MAXSTA);
            continue;
        }
        if (j >= biass->nsta) {
            strcpy(biass->sta[j].name, sysrb[i].staname);
            biass->nsta++;
        }
        addbia(&newbia, &biass->sta[j].syscb[sysrb[i].sysno], &biass->sta[j].nsyscb[sysrb[i].sysno],
               &biass->sta[j].nsyscbmax[sysrb[i].sysno]);
    }

    /* satellite bias */
    if (allb != 0) {
        for (i = 0; i < nsatrb; i++) {
            memset(&newbia, 0x00, sizeof(bia_t));
            newbia.type = 2;
            newbia.t0 = st;
            newbia.t1 = et;
            newbia.code[0] = satrb[i].code;
            newbia.val = satrb[i].ave;
            newbia.valstd = sqrt(satrb[i].var / (satrb[i].cnt - 1));
            for (j = 0; (j < MAXSTA) && (j < biass->nsta); j++) {
                if (strcmp(satrb[i].staname, biass->sta[j].name) == 0) {
                    break;
                }
            }
            if (j >= MAXSTA) {
                trace(NULL, 2, "gen_bias %s has exceeded the station limit of %d.\n", satrb[i].staname, MAXSTA);
                continue;
            }
            if (j >= biass->nsta) {
                strcpy(biass->sta[j].name, satrb[i].staname);
                biass->nsta++;
            }
            addbia(&newbia, &biass->sta[j].satcb[satrb[i].sat - 1], &biass->sta[j].nsatcb[satrb[i].sat - 1],
                   &biass->sta[j].nsatcbmax[satrb[i].sat - 1]);
        }
    }

    /* write bia file */
    outbsnx(outfile, ts, te, navfile, obsfile, tecfile, scbfile, ecef, elmask);
    for (i = 0; i < MAXEXFILE; i++) {
        free(files[i]);
        free(paths[i]);
    }
    freenav(&nav, 0xFF);
    free(satrb);
    free(sysrb);
    return 1;
}
/* main ----------------------------------------------------------------------*/
int mrtk_bias(int argc, char** argv) {
    gtime_t ts;
    double ep[6] = {2014, 1, 1}, tspan = 1.0, ecef[3] = {0.0}, elmask = 30 * D2R;
    int i;
    int tlevel = 0, req[6] = {0}, allb = 0;
    char *navfile = "", *obsfile = "", *outfile = "", *tecfile = "";
    char *scbfile = "", *p;
    char tracefile[MAXSTRPATH] = "recvbias.trace";
    char* reqarg[] = {"-td", "-nav", "-tec", "-scb", "-pos", "obsavation file"};
    char staname[32] = "";
    mrtk_ctx_t* ctx;

    /* Allocate rtcm_t on heap (~103MB) to avoid bloating BSS */
    rtcm = (rtcm_t*)calloc(1, sizeof(rtcm_t));
    if (!rtcm) {
        fprintf(stderr, "error: rtcm_t allocation failed\n");
        return -1;
    }
    if (!init_rtcm(rtcm)) {
        fprintf(stderr, "error: init_rtcm failed\n");
        free(rtcm);
        return -1;
    }

    /* Initialize MRTKLIB runtime context */
    ctx = mrtk_ctx_create();
    g_mrtk_ctx = ctx;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-td") && i + 1 < argc) {
            sscanf(argv[++i], "%lf/%lf/%lf", ep, ep + 1, ep + 2);
            req[0] = 1;
        } else if (!strcmp(argv[i], "-ts") && i + 1 < argc) {
            tspan = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-nav") && i + 1 < argc) {
            navfile = argv[++i];
            req[1] = 1;
        } else if (!strcmp(argv[i], "-tec") && i + 1 < argc) {
            tecfile = argv[++i];
            req[2] = 1;
        } else if (!strcmp(argv[i], "-scb") && i + 1 < argc) {
            scbfile = argv[++i];
            req[3] = 1;
        } else if (!strcmp(argv[i], "-pos") && i + 1 < argc) {
            sscanf(argv[++i], "%lf,%lf,%lf", ecef, ecef + 1, ecef + 2);
            req[4] = 1;
        } else if (!strcmp(argv[i], "-el") && i + 1 < argc) {
            elmask = atof(argv[++i]) * D2R;
        } else if (!strcmp(argv[i], "-sig") && i + 1 < argc) {
            for (p = argv[++i]; *p; p++) {
                switch (*p) {
                    case 'G':
                        selsiggps = atoi(p + 1);
                        break;
                    case 'J':
                        selsigqzs = atoi(p + 1);
                        break;
                    case 'C':
                        selsigcmp = atoi(p + 1);
                        break;
                }
                if (!(p = strchr(p, ','))) {
                    break;
                }
            }
        } else if (!strcmp(argv[i], "-out") && i + 1 < argc) {
            outfile = argv[++i];
        } else if (!strcmp(argv[i], "-sta") && i + 1 < argc) {
            strncpy(staname, argv[++i], sizeof(staname));
        } else if (!strcmp(argv[i], "-allbias")) {
            allb = 1;
        } else if (!strcmp(argv[i], "-t") && i + 1 < argc) {
            tlevel = atoi(argv[++i]);
        } else if (*argv[i] == '-') {
            print_help();
            goto cleanup;
        } else {
            obsfile = argv[i];
            req[5] = 1;
        }
    }
    ts = epoch2time(ep);

    /* open debug trace */
    if (tlevel > 0) {
        if (*outfile) {
            snprintf(tracefile, sizeof(tracefile), "%s.trace", outfile);
        }
        traceclose(ctx);
        traceopen(ctx, tracefile);
        tracelevel(ctx, tlevel);
    }

    /* required input error check */
    for (i = 0; i < 6; i++) {
        if (req[i] != 0) {
            continue;
        }
        fprintf(stderr, "error : %s required options\n", reqarg[i]);
        print_help();
        goto cleanup;
    }

    /* generate receiver bias */
    if (!gen_bias(ts, tspan, navfile, obsfile, tecfile, scbfile, ecef, elmask, outfile, staname, allb)) {
        goto cleanup;
    }

    if (strlen(tracefile) > 0) {
        traceclose(ctx);
    }
    g_mrtk_ctx = NULL;
    mrtk_ctx_destroy(ctx);
    free_rtcm(rtcm);
    free(rtcm);
    return 1;

cleanup:
    if (strlen(tracefile) > 0) {
        traceclose(ctx);
    }
    g_mrtk_ctx = NULL;
    mrtk_ctx_destroy(ctx);
    free_rtcm(rtcm);
    free(rtcm);
    return -1;
}
