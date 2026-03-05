/*------------------------------------------------------------------------------
 * mrtk_clas_isb.c : ISB (inter-system bias) and L2C phase shift corrections
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2015- Mitsubishi Electric Corp.
 * Copyright (C) 2014 Geospatial Information Authority of Japan
 * Copyright (C) 2007-2013 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_clas_isb.c
 * @brief ISB (inter-system bias) and L2C phase shift corrections.
 *
 * References:
 *   [1] claslib (https://github.com/mf-arai/claslib) — upstream isb.c
 *
 * Notes:
 *   - Ported from upstream claslib isb.c with minimal changes.
 *   - ISB corrects inter-system receiver biases between GPS/GLO/GAL/QZS.
 *   - L2C corrects 1/4 cycle phase shift on certain GPS L2C satellites.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_sys.h"
#include "mrtklib/mrtk_trace.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_clas.h"

#define NINCISB     256     /* ISB data realloc increment */

/* strip leading/trailing spaces from a string ---------------------------------
 * copy src to dst, stripping leading and trailing spaces.
 * args   : char   *dst     O   destination string
 *          const char *src  I   source string
 *          int    n         I   max chars to copy from src
 *-----------------------------------------------------------------------------*/
static void setstr(char *dst, const char *src, int n)
{
    char *p = dst;
    const char *q = src;

    while (*q && q < src + n) {
        if (p == dst && *q == ' ') { q++; continue; }
        *p++ = *q++;
    }
    *p = '\0';
    p--;
    while (p >= dst && *p == ' ') {
        *p = '\0';
        if (p > dst) p--;
    }
}

/* satellite code character to system ------------------------------------------*/
static int code2sys(char code)
{
    if (code == 'G' || code == ' ') return SYS_GPS;
    if (code == 'R') return SYS_GLO;
    if (code == 'E') return SYS_GAL;
    if (code == 'J') return SYS_QZS;
    if (code == 'C') return SYS_CMP;
    return SYS_NONE;
}

/* system to ISB array index ---------------------------------------------------
 * maps satellite system to index in gsb[NSYS][NFREQ][2] array.
 * GPS→0, GLO→1, GAL→2, QZS→3, CMP→4
 *-----------------------------------------------------------------------------*/
static int sys2isbidx(int sys)
{
    switch (sys) {
        case SYS_GPS: return NSYSGPS - 1;
#ifdef ENAGLO
        case SYS_GLO: return NSYSGPS + NSYSGLO - 1;
#endif
#ifdef ENAGAL
        case SYS_GAL: return NSYSGPS + NSYSGLO + NSYSGAL - 1;
#endif
#ifdef ENAQZS
        case SYS_QZS: return NSYSGPS + NSYSGLO + NSYSGAL + NSYSQZS - 1;
#endif
#ifdef ENACMP
        case SYS_CMP: return NSYSGPS + NSYSGLO + NSYSGAL + NSYSQZS + NSYSCMP - 1;
#endif
    }
    return -1;
}

/* discard trailing spaces and comments ----------------------------------------*/
static void chop_isb(char *str)
{
    char *p;
    if ((p = strchr(str, '#'))) *p = '\0';
    for (p = str + strlen(str) - 1; p >= str && !isgraph((int)*p); p--) *p = '\0';
}

/* add ISB data to navigation data ---------------------------------------------*/
static int addisbdata(nav_t *nav, const isb_t *data)
{
    isb_t *isb_data;

    if (nav->nimax <= nav->ni) {
        if (nav->nimax <= 0) nav->nimax = NINCISB; else nav->nimax *= 2;
        if (!(isb_data = (isb_t *)realloc(nav->isb, sizeof(isb_t) * nav->nimax))) {
            trace(NULL, 1, "addisbdata: memalloc error n=%d\n", nav->nimax);
            free(nav->isb); nav->isb = NULL; nav->ni = nav->nimax = 0;
            return -1;
        }
        nav->isb = isb_data;
    }
    nav->isb[nav->ni++] = *data;
    return 1;
}

/* read ISB parameters from a single file --------------------------------------*/
static int readisbf(const char *file, nav_t *nav)
{
    FILE *fp;
    double bias;
    char buff[256];
    int sys, i, s_code, res;
    char stationname[21] = {0};
    char stationname0[21] = {0};
    char stationname_base[21] = {0};
    char stationname0_base[21] = {0};
    isb_t data = {0};
    int nbuf = 0;
    int freq = 0;
    char type = '\0';
    int n = 0;
    int ifreq = 0;
    int itype = 0; /* 0:phase, 1:code */
    int ver = -1;

    trace(NULL, 3, "readisbf: file=%s\n", file);

    if (!(fp = fopen(file, "r"))) {
        trace(NULL, 2, "isb parameters file open error: %s\n", file);
        return 0;
    }
    while (fgets(buff, sizeof(buff), fp)) {
        ++n;
        if (4 > n) continue;

        chop_isb(buff);
        if (4 == n) {
            if (strcmp(buff, "******************** * * * **********************") == 0) {
                ver = 0;
            } else if (strcmp(buff, "******************** ******************** * * * **********************") == 0) {
                ver = 1;
            }
            continue;
        }

        nbuf = (int)strlen(buff);
        if (28 > nbuf) {
            trace(NULL, 2, "isb parameter error: %s (%s:%d)\n", buff, file, n);
            continue;
        }

        if (ver == 0) {
            /* version 0: single receiver format */
            setstr(stationname, buff, 20);
            bias = str2num(buff, 27, nbuf - 27);
            freq = (int)str2num(buff, 23, 1);
            type = buff[25];
            sys = code2sys(buff[21]);
            s_code = sys2isbidx(sys);

            if (' ' != buff[20] || ' ' != buff[22] || ' ' != buff[24] || ' ' != buff[26]
                || ('P' != type && 'L' != type)
                || (1 != freq && 2 != freq && 5 != freq)
                || -1 == s_code) {
                trace(NULL, 2, "isb parameter error: %s (%s:%d)\n", buff, file, n);
                continue;
            }

            ifreq = (5 == freq) ? 2 : freq - 1;
            itype = ('L' == type) ? 0 : 1;

            if (stationname[0] != '\0') {
                strcpy(stationname0, stationname);
            }
            if (stationname0[0] != '\0') {
                res = 0;
                for (i = 0; i < nav->ni; i++) {
                    if (strcmp(nav->isb[i].sta_name, stationname0) == 0) {
                        nav->isb[i].gsb[s_code][ifreq][itype] = bias * 1E-9 * CLIGHT;
                        res = 1;
                        break;
                    }
                }
                if (res == 0) {
                    memset(&data, 0, sizeof(data));
                    strcpy(data.sta_name, stationname0);
                    data.gsb[s_code][ifreq][itype] = bias * 1E-9 * CLIGHT;
                    addisbdata(nav, &data);
                }
            }
        } else if (ver == 1) {
            /* version 1: rover+base receiver format */
            setstr(stationname, buff, 20);
            setstr(stationname_base, buff + 21, 20);
            bias = str2num(buff, 48, nbuf - 48);
            freq = (int)str2num(buff, 44, 1);
            type = buff[46];
            sys = code2sys(buff[42]);
            s_code = sys2isbidx(sys);

            if (' ' != buff[20] || ' ' != buff[41] || ' ' != buff[43] ||
                ' ' != buff[45] || ' ' != buff[47]
                || ('P' != type && 'L' != type)
                || (1 != freq && 2 != freq && 5 != freq)
                || -1 == s_code) {
                trace(NULL, 2, "isb parameter error: %s (%s:%d)\n", buff, file, n);
                continue;
            }

            ifreq = (5 == freq) ? 2 : freq - 1;
            itype = ('L' == type) ? 0 : 1;

            if (stationname[0] != '\0') {
                strcpy(stationname0, stationname);
                stationname0_base[0] = '\0';
                if (stationname_base[0] != '\0') {
                    strcpy(stationname0_base, stationname_base);
                }
            } else if (stationname0[0] != '\0') {
                if (stationname_base[0] != '\0') {
                    strcpy(stationname0_base, stationname_base);
                }
            }

            if (stationname0[0] != '\0') {
                res = 0;
                for (i = 0; i < nav->ni; i++) {
                    if (strcmp(nav->isb[i].sta_name, stationname0) == 0 &&
                        strcmp(nav->isb[i].sta_name_base, stationname0_base) == 0) {
                        nav->isb[i].gsb[s_code][ifreq][itype] = bias * 1E-9 * CLIGHT;
                        res = 1;
                        break;
                    }
                }
                if (res == 0) {
                    memset(&data, 0, sizeof(data));
                    strcpy(data.sta_name, stationname0);
                    strcpy(data.sta_name_base, stationname0_base);
                    data.gsb[s_code][ifreq][itype] = bias * 1E-9 * CLIGHT;
                    addisbdata(nav, &data);

                    /* add reverse entry */
                    if (stationname0_base[0] != '\0') {
                        memset(&data, 0, sizeof(data));
                        strcpy(data.sta_name_base, stationname0);
                        strcpy(data.sta_name, stationname0_base);
                        data.gsb[s_code][ifreq][itype] = -bias * 1E-9 * CLIGHT;
                        addisbdata(nav, &data);
                    }
                }
            }
        }
    }
    fclose(fp);

    trace(NULL, 3, "readisbf: ni=%d\n", nav->ni);
    return 1;
}

/* read ISB parameters ---------------------------------------------------------
 * read inter-system bias (ISB) parameters
 * args   : char   *file       I   ISB parameters file (wild-card * expanded)
 *          nav_t  *nav        IO  navigation data
 * return : status (1:ok,0:error)
 *-----------------------------------------------------------------------------*/
extern int readisb(const char *file, nav_t *nav)
{
    int i, n;
    char *efiles[MAXEXFILE] = {0};

    trace(NULL, 3, "readisb: file=%s\n", file);

    for (i = 0; i < MAXEXFILE; i++) {
        if (!(efiles[i] = (char *)malloc(1024))) {
            for (i--; i >= 0; i--) free(efiles[i]);
            return 0;
        }
    }
    nav->ni = nav->nimax = 0;

    n = expath(file, efiles, MAXEXFILE);

    for (i = 0; i < n; i++) {
        readisbf(efiles[i], nav);
    }
    for (i = 0; i < MAXEXFILE; i++) free(efiles[i]);

    trace(NULL, 3, "readisb: total ni=%d\n", nav->ni);
    return 1;
}

/* set ISB corrections for rover and reference stations ------------------------
 * match receiver types to ISB table and populate sta_t.isb arrays
 *-----------------------------------------------------------------------------*/
extern void setisb(const nav_t *nav, const char *rectype0, const char *rectype1,
                   sta_t *sta0, sta_t *sta1)
{
    int i, j, k, m;
    int r = 0;
    isb_t *pisb[2] = {NULL, NULL};

    trace(NULL, 3, "setisb: rover=%s ref=%s ni=%d\n",
          rectype0 ? rectype0 : "(null)",
          rectype1 ? rectype1 : "(null)", nav->ni);

    /* clear ISB arrays */
    if (sta0) {
        for (i = 0; i < NSYS; ++i)
            for (j = 0; j < NFREQ; ++j)
                for (k = 0; k < 2; ++k) sta0->isb[i][j][k] = 0.0;
    }
    if (sta1) {
        for (i = 0; i < NSYS; ++i)
            for (j = 0; j < NFREQ; ++j)
                for (k = 0; k < 2; ++k) sta1->isb[i][j][k] = 0.0;
    }

    if (!rectype0) return;

    for (m = 0; m < nav->ni; ++m) {
        /* exact match: rover-base pair */
        if (!strcmp(rectype0, nav->isb[m].sta_name)) {
            if (rectype1 && !strcmp(rectype1, nav->isb[m].sta_name_base)) {
                if (sta0) {
                    for (i = 0; i < NSYS; ++i)
                        for (j = 0; j < NFREQ; ++j)
                            for (k = 0; k < 2; ++k)
                                sta0->isb[i][j][k] = nav->isb[m].gsb[i][j][k];
                }
                r = 1;
                break;
            }
        }
        /* reverse match: base-rover pair */
        if (rectype1 && !strcmp(rectype1, nav->isb[m].sta_name)) {
            if (!strcmp(rectype0, nav->isb[m].sta_name_base)) {
                if (sta0) {
                    for (i = 0; i < NSYS; ++i)
                        for (j = 0; j < NFREQ; ++j)
                            for (k = 0; k < 2; ++k)
                                sta0->isb[i][j][k] = -nav->isb[m].gsb[i][j][k];
                }
                r = 1;
                break;
            }
        }
        /* fallback: single receiver entries (no base) */
        if (r == 0) {
            if (!strcmp(rectype0, nav->isb[m].sta_name) &&
                !strcmp(nav->isb[m].sta_name_base, "")) {
                pisb[0] = &nav->isb[m];
            } else if (rectype1 && !strcmp(rectype1, nav->isb[m].sta_name) &&
                       !strcmp(nav->isb[m].sta_name_base, "")) {
                pisb[1] = &nav->isb[m];
            }
        }
    }
    if (r == 0) {
        if (pisb[0] && sta0) {
            for (i = 0; i < NSYS; ++i)
                for (j = 0; j < NFREQ; ++j)
                    for (k = 0; k < 2; ++k) sta0->isb[i][j][k] = pisb[0]->gsb[i][j][k];
        }
        if (pisb[1] && sta1) {
            for (i = 0; i < NSYS; ++i)
                for (j = 0; j < NFREQ; ++j)
                    for (k = 0; k < 2; ++k) sta1->isb[i][j][k] = pisb[1]->gsb[i][j][k];
        }
    }

    /* log ISB values */
    if (sta0) {
        for (i = 0; i < NSYS; ++i) {
            for (j = 0; j < NFREQ; ++j) {
                if (sta0->isb[i][j][0] != 0.0 || sta0->isb[i][j][1] != 0.0) {
                    trace(NULL, 3, "setisb: sys=%d freq=%d L=%.6f P=%.6f m\n",
                          i, j, sta0->isb[i][j][0], sta0->isb[i][j][1]);
                }
            }
        }
    }
}

/* get ISB correction for a satellite system -----------------------------------
 * return ISB correction values for a given satellite system
 *-----------------------------------------------------------------------------*/
extern void chk_isb(int sysno, const prcopt_t *opt, const sta_t *sta,
                    double y[NFREQ][2])
{
    int i, j;
    int isys = sys2isbidx(sysno);

    for (i = 0; i < NFREQ; ++i)
        for (j = 0; j < 2; ++j) y[i][j] = 0.0;

    if (isys < 0) return;
    if (opt->isb != ISBOPT_TABLE) return;

    for (i = 0; i < NFREQ; ++i) {
        for (j = 0; j < 2; ++j) {
            y[i][j] = sta->isb[isys][i][j];
        }
    }
}

/* trim trailing whitespace ----------------------------------------------------*/
static void trim_str(char *str)
{
    char *p = str + strlen(str) - 1;
    while (p >= str && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        *p-- = '\0';
    }
}

/* read L2C 1/4 cycle phase shift table ----------------------------------------
 * read L2C phase shift correction table from file
 * args   : char   *file       I   L2C phase shift table file path
 *          nav_t  *nav        IO  navigation data (sfts populated)
 * return : status (0:ok, -1:error)
 *-----------------------------------------------------------------------------*/
extern int readL2C(const char *file, nav_t *nav)
{
    FILE *fp;
    char buff[1024], ori[33] = "Quarter-Cycle Phase Shifts Table";
    int countheader = 1, nod = 0;
    int lenBuf = 0;

    trace(NULL, 3, "readL2C: file=%s\n", file);

    nav->sfts.n = 0;
    memset(nav->sfts.rectyp, 0, sizeof(nav->sfts.rectyp));

    if ((fp = fopen(file, "r")) == NULL) {
        trace(NULL, 1, "l2c file open error: %s\n", file);
        return -1;
    }

    while (fgets(buff, sizeof(buff), fp)) {
        if (countheader == 1) {
            if (strncmp(buff, ori, 32) != 0) {
                trace(NULL, 1, "l2c file title error: %s\n", file);
                fclose(fp);
                return -1;
            }
        }
        if (countheader > 4) {
            lenBuf = (int)strlen(buff);
            if (22 < lenBuf && 45 > lenBuf) {
                strncpy(nav->sfts.rectyp[nod], buff, 20);
                trim_str(nav->sfts.rectyp[nod]);
                nav->sfts.bias[nod] = str2num(buff, 21, lenBuf - 21);
            } else if (23 > lenBuf && buff[0] != '\n') {
                strncpy(nav->sfts.rectyp[nod], buff, lenBuf - 1);
                nav->sfts.bias[nod] = PHASE_CYCLE;
            } else {
                trace(NULL, 1, "l2c file format error: %s\n", file);
                fclose(fp);
                return -1;
            }
            nod++;
            nav->sfts.n = nod;
            if (nod >= MAXRECTYP) {
                trace(NULL, 1, "l2c file size error: %s\n", file);
                fclose(fp);
                return -1;
            }
        }
        countheader++;
    }
    fclose(fp);
    trace(NULL, 3, "readL2C: n=%d\n", nav->sfts.n);
    if (nav->sfts.n < 1) {
        trace(NULL, 1, "no l2c data error: %s\n", file);
    }
    return 0;
}

/* check if observation code is L2C --------------------------------------------*/
extern int isL2C(int code)
{
    return (code == CODE_L2S || code == CODE_L2L || code == CODE_L2X) ? 1 : 0;
}
