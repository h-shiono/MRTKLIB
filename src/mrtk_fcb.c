/*------------------------------------------------------------------------------
 * mrtk_fcb.c : fractional cycle bias (FCB) functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_fcb.h"
#include "mrtklib/mrtk_bias_sinex.h"
#include "mrtklib/mrtk_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* local constants -----------------------------------------------------------*/
static const double CLIGHT  = 299792458.0;

#define SYS_GPS 0x01
#define SYS_GAL 0x08
#define SYS_QZS 0x10
#define SYS_CMP 0x20

/*--- forward declarations for legacy functions resolved at link time -------*/
extern void trace(int level, const char *format, ...);
extern int satid2no(const char *id);
extern int satsys(int sat, int *prn);
extern double code2freq(int sys, uint8_t code, int fcn);

static biass_t fcbs = {0};

/* FCB signal and tracking mode IDs ------------------------------------------*/
static const uint8_t fcb_sig_gps[3][16] = {
    {CODE_L1C,CODE_L1P,CODE_L1W,CODE_L1Y,CODE_L1M,       0,       0,       0,
            0,       0,       0,       0,       0,       0,       0,       0},
    {CODE_L2C,CODE_L2D,CODE_L2S,CODE_L2L,CODE_L2X,CODE_L2P,CODE_L2W,CODE_L2Y,
     CODE_L2M,       0,       0,       0,       0,       0,       0,       0},
    {CODE_L5I,CODE_L5Q,CODE_L5X,       0,       0,       0,       0,       0,
            0,       0,       0,       0,       0,       0,       0,       0}
};
static const uint8_t fcb_sig_gal[3][16] = {
    {CODE_L1B,CODE_L1C,CODE_L1X,       0,       0,       0,       0,       0,
            0,       0,       0,       0,       0,       0,       0,       0},
    {       0,       0,       0,       0,       0,       0,       0,       0,
            0,       0,       0,       0,       0,       0,       0,       0},
    {CODE_L5I,CODE_L5Q,CODE_L5X,       0,       0,       0,       0,       0,
            0,       0,       0,       0,       0,       0,       0,       0}
};
static const uint8_t fcb_sig_qzs[3][16] = {
    {CODE_L1C,CODE_L1S,CODE_L1L,CODE_L1X,       0,       0,       0,       0,
            0,       0,       0,       0,       0,       0,       0,       0},
    {CODE_L2S,CODE_L2L,CODE_L2X,       0,       0,       0,       0,       0,
            0,       0,       0,       0,       0,       0,       0,       0},
    {CODE_L5I,CODE_L5Q,CODE_L5X,       0,       0,       0,       0,       0,
            0,       0,       0,       0,       0,       0,       0,       0}
};
static const uint8_t fcb_sig_cmp[3][16] = {
    {CODE_L2I,CODE_L2Q,CODE_L2X,CODE_L1D,CODE_L1P,CODE_L1X,       0,       0,
            0,       0,       0,       0,       0,       0,       0,       0},
    {CODE_L5D,CODE_L5P,CODE_L5X,       0,       0,       0,       0,       0,
            0,       0,       0,       0,       0,       0,       0,       0},
    {CODE_L6I,CODE_L6Q,CODE_L6X,       0,       0,       0,       0,       0}
};

/* get fcbs ------------------------------------------------------------------*/
static biass_t *getfcbs(void)
{
    return &fcbs;
}

/* read satellite fcb file ---------------------------------------------------*/
static int readfcbf(const char *file)
{
    FILE *fp;
    double ep1[6], ep2[6], bias[3] = {0}, std[3] = {0}, cyc2m;
    char buff[1024], str[32], *p;
    int sat, sys;
    bia_t bia;
    biass_t *fcbsp;
    const uint8_t(*sigs)[16];
    int i, j;

    fcbsp = getfcbs();

    trace(3, "readfcbf: file=%s\n", file);

    if(!(fp = fopen(file, "r"))) {
        trace(2, "fcb parameters file open error: %s\n", file);
        return 0;
    }
    while(fgets(buff, sizeof(buff), fp)) {
        memset(&bia, 0x00, sizeof(bia_t));
        bia.type = 2; /* OSB */
        if((p = strchr(buff, '#'))) *p = '\0';
        if(sscanf(buff, "%lf/%lf/%lf %lf:%lf:%lf %lf/%lf/%lf %lf:%lf:%lf %s"
            "%lf %lf %lf %lf %lf %lf",
            ep1, ep1 + 1, ep1 + 2, ep1 + 3, ep1 + 4, ep1 + 5,
            ep2, ep2 + 1, ep2 + 2, ep2 + 3, ep2 + 4, ep2 + 5,
            str, bias, std, bias + 1, std + 1, bias + 2, std + 2) < 17) continue;
        if(!(sat = satid2no(str))) continue;
        sys = satsys(sat, NULL);
        switch(sys) {
            case SYS_GPS: sigs = fcb_sig_gps; break;
            case SYS_GAL: sigs = fcb_sig_gal; break;
            case SYS_QZS: sigs = fcb_sig_qzs; break;
            case SYS_CMP: sigs = fcb_sig_cmp; break;
            default: continue;
        }

        bia.t0 = epoch2time(ep1);
        bia.t1 = epoch2time(ep2);
        for(i = 0; i < 3; i++) { /* L1/L2/L5 */
            for(j = 0; j < 16; j++) {
                if(sigs[i][j] > 0) {
                    bia.code[0] = sigs[i][j];
                    cyc2m = CLIGHT / code2freq(sys, bia.code[0], 0);
                    bia.val     = bias[i] * cyc2m;
                    bia.valstd  = std[i]  * cyc2m;
                    addbia(&bia, &fcbsp->sat.pb[sat - 1],
                        &fcbsp->sat.npb[sat - 1],
                        &fcbsp->sat.npbmax[sat - 1]);
                }
            }
        }
    }
    fclose(fp);
    return 1;
}

/* read FCB file --------------------------------------------------------------
* read the FCB (Fractional Cycle Bias) file and update the internal structure
* args   : char *file       I   FCB file path (wild-card * expanded)
* return : int              O   status (1: success, 0: failure)
*-----------------------------------------------------------------------------*/
int readfcb(const char *file)
{
    char *efiles[MAXEXFILE] = {0};
    int i, n;

    trace(3, "readfcb : file=%s\n", file);

    for(i = 0; i < MAXEXFILE; i++) {
        if(!(efiles[i] = (char *)malloc(1024))) {
            for(i--; i >= 0; i--) free(efiles[i]);
            return 0;
        }
    }
    n = expath(file, efiles, MAXEXFILE);

    for(i = 0; i < n; i++) {
        readfcbf(efiles[i]);
    }
    for(i = 0; i < MAXEXFILE; i++) free(efiles[i]);

    return 1;
}

/* update satellite FCB -------------------------------------------------------
* update the satellite FCB (Fractional Cycle Bias) structure with current biases
* args   : osb_t  *osb      IO  FCB structure to update
*          gtime_t gt       I   current time
*          int    mode      I   mode (0: current, 1: all)
* return : int              O   number of updated biases
*-----------------------------------------------------------------------------*/
int udfcb_sat(osb_t *osb, gtime_t gt, int mode)
{
    biass_t *fcbsp;
    bia_t *bias;
    int i, j, ret = 0;

    fcbsp = getfcbs();

    for(j = 0; j < MAXCODE; j++) {
        for(i = 0; i < MAXSAT; i++) {
            osb->vspb[i][j] = 0;
        }
    }
    for(i = 0; i < MAXSAT; i++) {
        for(j = 0; j < fcbsp->sat.npb[i]; j++) {
            bias = &fcbsp->sat.pb[i][j];
            if(bias->code[0] == CODE_NONE) continue;
            if(mode || ((timediff(bias->t0, gt) <= 0.0) &&
                        (timediff(bias->t1, gt) >= 0.0))) {
                osb->vspb[i][bias->code[0] - 1] = 1;
                osb->spb [i][bias->code[0] - 1] = bias->val;
                ret++;
            }
        }
    }
    osb->gt[0] = gt;
    return ret;
}
