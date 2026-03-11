/*------------------------------------------------------------------------------
 * mrtk_ppp_rtk.c : PPP-RTK positioning engine
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
 * @file mrtk_ppp_rtk.c
 * @brief PPP-RTK positioning engine.
 *
 * References:
 *   [1] claslib ppprtk.c — upstream PPP-RTK implementation
 *   [2] claslib rtkcmn.c — upstream filter2/residual_test
 *   [6] MacMillan et al., Atmospheric gradients and the VLBI terrestrial and
 *       celestial reference frames, Geophys. Res. Let., 1997
 *
 * Notes:
 *   - Ported from upstream claslib ppprtk.c as an independent positioning
 *     engine.  Uses double-differencing (DD) with CLAS grid corrections,
 *     unlike the undifferenced PPP engine in mrtk_ppp.c.
 *   - Single channel only (no l6mrg multi-channel support).
 */
#include "mrtklib/mrtk_ppp_rtk.h"
#include "mrtklib/mrtk_clas.h"
#include "mrtklib/mrtk_trace.h"
#include "mrtklib/mrtk_atmos.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_lambda.h"
#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_eph.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*============================================================================
 * Local Macros
 *===========================================================================*/
#define SQR(x)      ((x)*(x))
#define SQRT(x)     ((x)<=0.0||(x)!=(x)?0.0:sqrt(x))
#define MIN(x,y)    ((x)<=(y)?(x):(y))

#define MAXREF      2           /* max iterations searching reference sat */

/* initial variances */
#define VAR_POS     SQR(30.0)   /* initial variance of receiver pos (m^2) */
#define VAR_VEL     SQR(1.0)    /* initial variance of receiver vel (m/s)^2 */
#define VAR_ACC     SQR(1.0)    /* initial variance of receiver acc (m/ss)^2 */
#define VAR_GRA     SQR(0.001)  /* gradient variance (m^2) */
#define INIT_ZWD    0.001       /* initial ZWD (m) */

#define BASELEN     1000.0      /* baseline length for iono update (m) */

#define MAXPBCORSSR 20.0        /* max phase bias correction of SSR (m) */

/* compile-time defaults for missing prcopt fields */
#define VARHOLDAMB          0.001   /* hold-ambiguity constraint variance */
#define ALPHAAR_DEFAULT     3       /* F-test significance index (5%) */
#define MINAMB_DEFAULT      2       /* minimum ambiguities for AR */
#define MAXOBSLOSS_DEFAULT  300.0   /* max obs gap to reset states (s) */

/* carrier wavelengths */
#define LAM_CARR_L1  (CLIGHT/FREQ1)
#define LAM_CARR_L2  (CLIGHT/FREQ2)
#define LAM_CARR_L5  (CLIGHT/FREQ5)

static const double lam_carr[3] = {LAM_CARR_L1, LAM_CARR_L2, LAM_CARR_L5};

/*============================================================================
 * State Index Macros (from upstream ppprtk.c)
 *
 * State vector layout:
 *   [0..NP-1]       : position (3 or 9 with dynamics)
 *   [NP..NP+NI-1]   : ionosphere per satellite (MAXSAT)
 *   [NP+NI..NR-1]   : troposphere (1 or 3)
 *   [NR..NR+NB-1]   : phase bias per satellite per frequency
 *===========================================================================*/

#define NF_RTK(opt)   ((opt)->nf)
#define NP_RTK(opt)   ((opt)->dynamics==0?3:9)
#define NI_RTK(opt)   (((opt)->ionoopt==IONOOPT_EST||(opt)->ionoopt==IONOOPT_EST_ADPT)?MAXSAT:0)
#define NT_RTK(opt)   ((opt)->tropopt<TROPOPT_EST?0:((opt)->tropopt<TROPOPT_ESTG?1:3))
#define NR_RTK(opt)   (NP_RTK(opt)+NI_RTK(opt)+NT_RTK(opt))
#define NB_RTK(opt)   (MAXSAT*NF_RTK(opt))
#define NX_RTK(opt)   (NR_RTK(opt)+NB_RTK(opt))

/* state index accessors */
#define II_RTK(s,opt) (NP_RTK(opt)+(s)-1)                    /* iono state index */
#define IT_RTK(opt)   (NP_RTK(opt)+NI_RTK(opt))              /* trop state index */
#define IB_RTK(s,f,opt) (NR_RTK(opt)+MAXSAT*(f)+(s)-1)       /* bias state index */

/*============================================================================
 * Chi-squared Table (alpha=0.001)
 *===========================================================================*/
static const double chisqr[100] = {
    10.8,13.8,16.3,18.5,20.5,22.5,24.3,26.1,27.9,29.6,
    31.3,32.9,34.5,36.1,37.7,39.3,40.8,42.3,43.8,45.3,
    46.8,48.3,49.7,51.2,52.6,54.1,55.5,56.9,58.3,59.7,
    61.1,62.5,63.9,65.2,66.6,68.0,69.3,70.7,72.1,73.4,
    74.7,76.0,77.3,78.6,80.0,81.3,82.6,84.0,85.4,86.7,
    88.0,89.3,90.6,91.9,93.3,94.7,96.0,97.4,98.7,100 ,
    101 ,102 ,103 ,104 ,105 ,107 ,108 ,109 ,110 ,112 ,
    113 ,114 ,115 ,116 ,118 ,119 ,120 ,122 ,123 ,125 ,
    126 ,127 ,128 ,129 ,131 ,132 ,133 ,134 ,135 ,137 ,
    138 ,139 ,140 ,142 ,143 ,144 ,145 ,147 ,148 ,149
};

/*============================================================================
 * F Inverse Distribution Table
 *===========================================================================*/
static const double qf[6][60] = {
    /* significance level, alpha=0.001(0.1%) */
    {405284.07,999.00,141.11,53.44,29.75,20.03,15.02,12.05,10.11,8.75,
     7.76,7.00,6.41,5.93,5.54,5.20,4.92,4.68,4.47,4.29,
     4.13,3.98,3.85,3.74,3.63,3.53,3.44,3.36,3.29,3.22,
     3.15,3.09,3.04,2.98,2.93,2.89,2.84,2.80,2.76,2.73,
     2.69,2.66,2.63,2.60,2.57,2.54,2.51,2.49,2.46,2.44,
     2.42,2.40,2.38,2.36,2.34,2.32,2.30,2.28,2.27,2.25},
    /* significance level, alpha=0.005(0.5%) */
    {16210.72,199.00,47.47,23.15,14.94,11.07,8.89,7.50,6.54,5.85,
     5.32,4.91,4.57,4.30,4.07,3.87,3.71,3.56,3.43,3.32,
     3.22,3.12,3.04,2.97,2.90,2.84,2.78,2.72,2.67,2.63,
     2.58,2.54,2.51,2.47,2.44,2.41,2.38,2.35,2.32,2.30,
     2.27,2.25,2.23,2.21,2.19,2.17,2.15,2.13,2.11,2.10,
     2.08,2.07,2.05,2.04,2.02,2.01,2.00,1.99,1.97,1.96},
    /* significance level, alpha=0.010(1.0%) */
    {4052.18,99.00,29.46,15.98,10.97,8.47,6.99,6.03,5.35,4.85,
     4.46,4.16,3.91,3.70,3.52,3.37,3.24,3.13,3.03,2.94,
     2.86,2.78,2.72,2.66,2.60,2.55,2.51,2.46,2.42,2.39,
     2.35,2.32,2.29,2.26,2.23,2.21,2.18,2.16,2.14,2.11,
     2.09,2.08,2.06,2.04,2.02,2.01,1.99,1.98,1.96,1.95,
     1.94,1.92,1.91,1.90,1.89,1.88,1.87,1.86,1.85,1.84},
    /* significance level, alpha=0.050(5.0%) */
    {161.45,19.00,9.28,6.39,5.05,4.28,3.79,3.44,3.18,2.98,
     2.82,2.69,2.58,2.48,2.40,2.33,2.27,2.22,2.17,2.12,
     2.08,2.05,2.01,1.98,1.96,1.93,1.90,1.88,1.86,1.84,
     1.82,1.80,1.79,1.77,1.76,1.74,1.73,1.72,1.70,1.69,
     1.68,1.67,1.66,1.65,1.64,1.63,1.62,1.62,1.61,1.60,
     1.59,1.58,1.58,1.57,1.56,1.56,1.55,1.55,1.54,1.53},
    /* significance level, alpha=0.100(10.0%) */
    {39.86,9.00,5.39,4.11,3.45,3.05,2.78,2.59,2.44,2.32,
     2.23,2.15,2.08,2.02,1.97,1.93,1.89,1.85,1.82,1.79,
     1.77,1.74,1.72,1.70,1.68,1.67,1.65,1.63,1.62,1.61,
     1.59,1.58,1.57,1.56,1.55,1.54,1.53,1.52,1.51,1.51,
     1.50,1.49,1.48,1.48,1.47,1.46,1.46,1.45,1.45,1.44,
     1.44,1.43,1.43,1.42,1.42,1.41,1.41,1.40,1.40,1.40},
    /* significance level, alpha=0.200(20.0%) */
    {9.47,4.00,2.94,2.48,2.23,2.06,1.94,1.86,1.79,1.73,
     1.69,1.65,1.61,1.58,1.56,1.54,1.52,1.50,1.48,1.47,
     1.45,1.44,1.43,1.42,1.41,1.40,1.39,1.38,1.37,1.36,
     1.36,1.35,1.34,1.34,1.33,1.33,1.32,1.32,1.31,1.31,
     1.30,1.30,1.30,1.29,1.29,1.28,1.28,1.28,1.27,1.27,
     1.27,1.26,1.26,1.26,1.26,1.25,1.25,1.25,1.25,1.24}
};

/*============================================================================
 * File-scope persistent state (replaces static variables from upstream)
 *===========================================================================*/
static struct {
    /* DD reference satellite tracking */
    int refsat[NFREQ*2*MAXREF*6+1];
    int refsat2[NFREQ*2*6+1];

    /* CPC/pt0 persistent state from zdres */
    double cpc[MAXSAT*NFREQ];
    gtime_t pt0[MAXSAT];

    /* OSR context */
    clas_osr_ctx_t osr_ctx;

    /* chi-squared validation value (sol_t has no chisq field) */
    double chisq;

    /* innovation covariance Qp = K*v*(K*v)' from filter2_ (adaptive noise) */
    double *Qp;
    int Qp_nx;

    /* consecutive FLOAT epoch counter for filter reset (floatcnt mechanism) */
    int float_count;

    /* consecutive position error counter for maxdiffp reset */
    int cntdiffp;

    /* flag: floatcnt just triggered a mass reset (suppress pbreset) */
    int floatcnt_reset;

    int initialized;
} prtk_ctx;

/*============================================================================
 * Helper: satellite-specific wavelength
 *===========================================================================*/

/**
 * @brief Return default observation code for frequency index and system.
 * @param[in] sat Satellite number
 * @param[in] f   Frequency index (0=L1, 1=L2, 2=L5)
 * @return Observation code (CODE_L*)
 */
static uint8_t obs_code_for_freq(int sat, int f)
{
    int sys = satsys(sat, NULL);
    switch (f) {
    case 0:
        return (sys == SYS_CMP) ? CODE_L2I : CODE_L1C;
    case 1:
        if (sys == SYS_GPS) {
            return CODE_L2W;
        }
        if (sys == SYS_QZS) {
            return CODE_L5Q; /* obsdef: QZS slot1 = L5 */
        }
        if (sys == SYS_GAL) {
            return CODE_L5Q;
        }
        if (sys == SYS_CMP) {
            return CODE_L7I;
        }
        return CODE_L2W;
    case 2:
        if (sys == SYS_CMP) {
            return CODE_L6I;
        }
        return CODE_L5Q;
    }
    return CODE_L1C;
}

/**
 * @brief Compute satellite-specific wavelength for frequency index.
 *
 * Uses direct obs code to frequency mapping, bypassing obsdef tables which
 * may be rearranged by apply_pppsig() for MADOCA PPP signal selection.
 *
 * @param[in] sat Satellite number
 * @param[in] f   Frequency index (0,1,2)
 * @param[in] nav Navigation data (unused, kept for API compatibility)
 * @return Wavelength in meters, or 0.0 if invalid
 */
static double sat_lambda(int sat, int f, const nav_t *nav)
{
    uint8_t code = obs_code_for_freq(sat, f);
    char *obs = code2obs(code);
    double freq = 0.0;

    if (!obs || strlen(obs) < 2) {
        return 0.0;
    }

    switch (obs[0]) {
    case '1': freq = FREQ1; break;
    case '2': freq = FREQ2; break;
    case '5': freq = FREQ5; break;
    case '6': freq = FREQ6; break;
    case '7': freq = FREQ7; break;
    case '8': freq = FREQ8; break;
    default: return 0.0;
    }
    return CLIGHT / freq;
}

/*============================================================================
 * Measurement error variance
 *===========================================================================*/

/**
 * @brief Compute measurement error variance.
 * @param[in] sat  Satellite number
 * @param[in] sys  Satellite system
 * @param[in] el   Elevation angle (rad)
 * @param[in] type Measurement type (0:phase, 1:code, 2:iono, 3:trop)
 * @param[in] opt  Processing options
 * @return Variance (m^2)
 */
static double varerr(int sat, int sys, double el, int type,
                     const prcopt_t *opt)
{
    double a, b, c = 0.0, d = 0.0, fact = 1.0;
    double sinel = sin(el);

    (void)sat;

    if (type == 2) {       /* iono */
        return 0.0;
    }
    else if (type == 3) {  /* trop */
        return 0.0;
    }
    else {
        /* normal error model */
        if (type == 1) {
            fact *= opt->eratio[0];
        }
        fact *= sys==SYS_GLO?EFACT_GLO:(sys==SYS_GAL?EFACT_GAL:
                (sys==SYS_QZS?EFACT_QZS:(sys==SYS_CMP?EFACT_CMP:
                (sys==SYS_IRN?EFACT_IRN:(sys==SYS_SBS?EFACT_SBS:EFACT_GPS)))));
        if (opt->ionoopt == IONOOPT_IFLC) {
            fact *= 3.0;
        }
        a = fact * opt->err[1];
        b = fact * opt->err[2];
        if (opt->ionoopt == IONOOPT_EST) {
            c = opt->err[5];
        }
        if (opt->tropopt >= TROPOPT_EST) {
            d = opt->err[6];
        }
    }
    return a * a + b * b / sinel / sinel + c * c + d * d;
}

/*============================================================================
 * State initialization
 *===========================================================================*/

/**
 * @brief Initialize a state and its covariance.
 * @param[in,out] rtk RTK control struct
 * @param[in]     xi  Initial state value
 * @param[in]     var Initial variance
 * @param[in]     i   State index
 */
static void initx(rtk_t *rtk, double xi, double var, int i)
{
    int j;
    rtk->x[i] = xi;
    for (j = 0; j < rtk->nx; j++) {
        rtk->P[i + j * rtk->nx] = rtk->P[j + i * rtk->nx] = i == j ? var : 0.0;
    }
}

/*============================================================================
 * Position time update
 *===========================================================================*/

/**
 * @brief Temporal update of position states.
 * @param[in,out] rtk RTK control struct
 * @param[in]     obs Observation data
 * @param[in]     n   Number of observations
 * @param[in]     tt  Time step (s)
 */
static void udpos_ppp(rtk_t *rtk, const obsd_t *obs, int n, double tt)
{
    double *F, *FP, *xp, pos[3], Q[9] = {0}, Qv[9], var = 0.0;
    int i, j, nx;

    (void)obs; (void)n;

    trace(NULL, 3, "udpos_ppp:\n");

    nx = rtk->nx;

    /* fixed mode */
    if (rtk->opt.mode == PMODE_PPP_FIXED) {
        for (i = 0; i < 3; i++) {
            initx(rtk, rtk->opt.ru[i], 1E-8, i);
        }
        return;
    }

    /* initialize position for first epoch */
    if (norm(rtk->x, 3) <= 0.0) {
        for (i = 0; i < 3; i++) {
            initx(rtk, rtk->sol.rr[i], VAR_POS, i);
        }
        if (rtk->opt.dynamics) {
            for (i = 3; i < 6; i++) {
                initx(rtk, rtk->sol.rr[i], VAR_VEL, i);
            }
            for (i = 6; i < 9; i++) {
                initx(rtk, 1E-6, VAR_ACC, i);
            }
        }
        return;
    }

    /* check variance of estimated position */
    for (i = 0; i < 3; i++) {
        var += rtk->P[i + i * nx];
    }
    var /= 3.0;

    if (var > VAR_POS) {
        /* reset position with large variance */
        for (i = 0; i < 3; i++) {
            initx(rtk, rtk->sol.rr[i], VAR_POS, i);
        }
        for (i = 3; i < 6; i++) {
            initx(rtk, rtk->sol.rr[i], VAR_VEL, i);
        }
        for (i = 6; i < 9; i++) {
            initx(rtk, 1E-6, VAR_ACC, i);
        }
        trace(NULL, 2, "reset rtk position due to large variance: var=%.3f\n", var);
        return;
    }

    /* state transition of position/velocity/acceleration */
    if (rtk->opt.dynamics) {
        /* x=Fx, P=FPF' for pos/vel/acc */
        F = eye(9); xp = mat(9, 1); FP = mat(nx, nx);

        for (i = 0; i < 6; i++) {
            F[i + (i + 3) * 9] = tt;
        }
        for (i = 0; i < 3; i++) {
            F[i + (i + 6) * 9] = SQR(tt) / 2.0;
        }

        /* x=F*x */
        matmul("NN", 9, 1, 9, 1.0, F, rtk->x, 0.0, xp);
        matcpy(rtk->x, xp, 9, 1);

        /* P=F*P */
        matcpy(FP, rtk->P, nx, nx);
        for (j = 0; j < nx; j++) {
            for (i = 0; i < 6; i++) {
                FP[i + j * nx] += rtk->P[i + 3 + j * nx] * tt;
            }
        }

        /* P=FP*F' */
        matcpy(rtk->P, FP, nx, nx);
        for (j = 0; j < nx; j++) {
            for (i = 0; i < 6; i++) {
                rtk->P[j + i * nx] += rtk->P[j + (i + 3) * nx] * tt;
            }
        }

        free(F); free(FP); free(xp);
    }

    if (rtk->opt.dynamics) {
        /* process noise added to acceleration */
        Q[0] = Q[4] = SQR(rtk->opt.prn[3]) * fabs(tt);
        Q[8] = SQR(rtk->opt.prn[4]) * fabs(tt);
        ecef2pos(rtk->x, pos);
        covecef(pos, Q, Qv);
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                rtk->P[i + 6 + (j + 6) * nx] += Qv[i + j * 3];
            }
        }
    } else {
        /* process noise added to position */
        Q[0] = Q[4] = SQR(rtk->opt.prn[5]) * fabs(tt);
        Q[8] = SQR(rtk->opt.prn[6]) * fabs(tt);
        ecef2pos(rtk->x, pos);
        covecef(pos, Q, Qv);
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                rtk->P[i + j * nx] += Qv[i + j * 3];
            }
        }
    }
}

/*============================================================================
 * Troposphere time update
 *===========================================================================*/

/**
 * @brief Temporal update of tropospheric parameters.
 * @param[in,out] rtk RTK control struct
 * @param[in]     tt  Time step (s)
 */
static void udtrop(rtk_t *rtk, double tt)
{
    int j, k;

    trace(NULL, 3, "udtrop  : tt=%.1f\n", tt);

    j = IT_RTK(&rtk->opt);

    if (rtk->x[j] == 0.0) {
        initx(rtk, INIT_ZWD, SQR(rtk->opt.std[2]), j); /* initial ZWD */
        if (rtk->opt.tropopt >= TROPOPT_ESTG) {
            for (k = 0; k < 2; k++) {
                initx(rtk, 1E-6, VAR_GRA, ++j);
            }
        }
    } else {
        rtk->P[j + j * rtk->nx] += SQR(rtk->opt.prn[2]) * tt;
        if (rtk->opt.tropopt >= TROPOPT_ESTG) {
            for (k = 0; k < 2; k++) {
                rtk->P[++j * (1 + rtk->nx)] += SQR(rtk->opt.prn[2] * 0.3) * fabs(rtk->tt);
            }
        }
    }
}

/*============================================================================
 * Ionosphere time update
 *===========================================================================*/

/**
 * @brief Temporal update of ionospheric parameters.
 * @param[in,out] rtk RTK control struct
 * @param[in]     tt  Time step (s)
 * @param[in]     bl  Baseline length (m)
 * @param[in]     obs Observation data
 * @param[in]     ns  Number of observations
 */
static void udion(rtk_t *rtk, double tt, double bl, const obsd_t *obs, int ns)
{
    double el, fact, qi;
    int i, j, k, f, m;

    (void)bl;

    trace(NULL, 3, "udion   : tt=%.1f ns=%d\n", tt, ns);

    for (i = 1; i <= MAXSAT; i++) {
        j = II_RTK(i, &rtk->opt);
        if (rtk->x[j] == 0.0) {
            continue;
        }
        for (k = 0; k < ns; k++) {
            if (obs[k].sat == i) {
                break;
            }
        }
        if (ns == k) {
            continue;
        }
        m = clas_osr_selfreqpair(i, &rtk->opt, obs + k);
        for (f = 0; f < rtk->opt.nf; f++) {
            if (f > 0 && !(f & m)) {
                continue;
            }
            if ((rtk->ssat[i - 1].outc[f] > (unsigned int)rtk->opt.maxout)) {
                trace(NULL, 3, "reset iono estimation, sat=%2d outc=%6d\n",
                      i, rtk->ssat[i - 1].outc[f]);
                rtk->x[j] = 0.0;
            }
        }
    }
    for (i = 0; i < ns; i++) {
        j = II_RTK(obs[i].sat, &rtk->opt);
        if (rtk->x[j] == 0.0) {
            initx(rtk, 1E-6, SQR(rtk->opt.std[1]), j);
            rtk->Q[j + j * rtk->nx] = 0.0;
        } else {
            /* elevation dependent factor of process noise */
            el = rtk->ssat[obs[i].sat - 1].azel[1];
            fact = cos(el);
            if (rtk->opt.ionoopt == IONOOPT_EST_ADPT) {
                /* adaptive: clamp Q within [prn[1]^2, prnionomax^2] */
                if (rtk->Q[j + j * rtk->nx] == 0.0) {
                    rtk->Q[j + j * rtk->nx] = SQR(rtk->opt.prn[1] * fact);
                } else {
                    if (rtk->Q[j + j * rtk->nx] > SQR(rtk->opt.prnionomax)) {
                        rtk->Q[j + j * rtk->nx] = SQR(rtk->opt.prnionomax);
                    } else if (rtk->Q[j + j * rtk->nx] < SQR(rtk->opt.prn[1])) {
                        rtk->Q[j + j * rtk->nx] = SQR(rtk->opt.prn[1]);
                    }
                }
                qi = rtk->Q[j + j * rtk->nx];
            } else {
                qi = SQR(rtk->opt.prn[1] * fact);
                rtk->Q[j + j * rtk->nx] = qi;
            }
            rtk->P[j + j * rtk->nx] += qi * tt;
        }
    }
}

/*============================================================================
 * Cycle slip detection: LLI
 *===========================================================================*/

/**
 * @brief Detect cycle slip by LLI flag.
 * @param[in,out] rtk RTK control struct
 * @param[in]     obs Observation data
 * @param[in]     n   Number of observations
 */
/* detect cycle slip by Doppler/phase rate comparison (demo5, single-receiver) */
static void detslp_dop(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    (void)nav;
    int i, f, sat, ndop = 0, nf = rtk->opt.nf < NFREQ ? rtk->opt.nf : NFREQ;
    double dph, dpt, tt, mean_dop = 0.0;
    double dopdif[MAXOBS][NFREQ] = {{0}}, tts[MAXOBS][NFREQ] = {{0}};

    if (rtk->opt.thresdop <= 0.0) {
        return; /* disabled */
    }

    for (i = 0; i < n && i < MAXOBS; i++) {
        sat = obs[i].sat;
        for (f = 0; f < nf; f++) {
            if (obs[i].L[f] == 0.0 || obs[i].D[f] == 0.0 || rtk->ssat[sat - 1].ph[0][f] == 0.0) {
                continue;
            }
            tt = timediff(obs[i].time, rtk->ssat[sat-1].pt[0][f]);
            if (fabs(tt) < DTTOL) {
                continue;
            }
            dph = (obs[i].L[f] - rtk->ssat[sat-1].ph[0][f]) / tt;
            dpt = -obs[i].D[f];
            dopdif[i][f] = dph - dpt;
            tts[i][f] = tt;
            if (fabs(dopdif[i][f]) < 3.0 * rtk->opt.thresdop) {
                mean_dop += dopdif[i][f]; ndop++;
            }
        }
    }
    if (ndop == 0) {
        return;
    }
    mean_dop /= ndop;

    for (i = 0; i < n && i < MAXOBS; i++) {
        sat = obs[i].sat;
        for (f = 0; f < nf; f++) {
            if (dopdif[i][f] == 0.0) {
                continue;
            }
            if (fabs(dopdif[i][f] - mean_dop) > rtk->opt.thresdop) {
                rtk->ssat[sat-1].slip[f] |= 1;
                trace(NULL, 3, "ppprtk detslp_dop: sat=%2d F=%d dL=%.3f off=%.3f tt=%.2f\n",
                      sat, f+1, dopdif[i][f]-mean_dop, mean_dop, tts[i][f]);
            }
        }
    }
}
/* detect cycle slip by observation code change (demo5, single-receiver) */
static void detslp_code(rtk_t *rtk, const obsd_t *obs, int n)
{
    int i, f, sat, nf = rtk->opt.nf < NFREQ ? rtk->opt.nf : NFREQ;
    for (i = 0; i < n && i < MAXOBS; i++) {
        sat = obs[i].sat;
        for (f = 0; f < nf; f++) {
            uint8_t code = obs[i].code[f];
            if (code == CODE_NONE) {
                continue;
            }
            uint8_t ccode = rtk->ssat[sat-1].codeprev[f][0];
            if (code != ccode) {
                rtk->ssat[sat-1].codeprev[f][0] = code;
                if (ccode != CODE_NONE) {
                    rtk->ssat[sat-1].slip[f] |= 1;
                    trace(NULL, 3, "ppprtk detslp_code: sat=%2d F=%d %s->%s\n",
                          sat, f+1, code2obs(ccode), code2obs(code));
                }
            }
        }
    }
}
static void detslp_ll(rtk_t *rtk, const obsd_t *obs, int n)
{
    int i, j;

    for (i = 0; i < n && i < MAXOBS; i++) {
        for (j = 0; j < rtk->opt.nf; j++) {
            if (obs[i].L[j] == 0.0 || !(obs[i].LLI[j] & 3)) {
                continue;
            }
            trace(NULL, 2, "detslp_ll: slip detected sat=%2d f=%d\n",
                  obs[i].sat, j + 1);
            rtk->ssat[obs[i].sat - 1].slip[j] = 1;
        }
    }
}

/*============================================================================
 * Geometry-free phase measurements
 *===========================================================================*/

/**
 * @brief L1/L2 geometry-free phase measurement.
 */
static double gfmeas_L1L2(const obsd_t *obs, const nav_t *nav)
{
    double lam0 = sat_lambda(obs->sat, 0, nav);
    double lam1 = sat_lambda(obs->sat, 1, nav);
    if (lam0 == 0.0 || lam1 == 0.0 || obs->L[0] == 0.0 || obs->L[1] == 0.0) {
        return 0.0;
    }
    return lam0 * obs->L[0] - lam1 * obs->L[1];
}

/**
 * @brief L1/L5 geometry-free phase measurement.
 */
static double gfmeas_L1L5(const obsd_t *obs, const nav_t *nav)
{
    double lam0 = sat_lambda(obs->sat, 0, nav);
    double lam2 = sat_lambda(obs->sat, 2, nav);
    if (lam0 == 0.0 || lam2 == 0.0 || obs->L[0] == 0.0 || obs->L[2] == 0.0) {
        return 0.0;
    }
    return lam0 * obs->L[0] - lam2 * obs->L[2];
}

/*============================================================================
 * Cycle slip detection: geometry-free
 *===========================================================================*/

/**
 * @brief Detect cycle slip by geometry-free phase jump.
 * @param[in,out] rtk RTK control struct
 * @param[in]     obs Observation data
 * @param[in]     n   Number of observations
 * @param[in]     nav Navigation data
 */
static void detslp_gf(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    double g0, g1;
    int i;

    trace(NULL, 3, "detslp_gf_L1_L2\n");

    /* slip detection for L1-L2 GF */
    for (i = 0; i < n && i < MAXOBS; i++) {
        if ((g1 = gfmeas_L1L2(obs + i, nav)) == 0.0) {
            continue;
        }
        g0 = rtk->ssat[obs[i].sat - 1].gf[0]; /* gf[0] = L1-L2 */
        rtk->ssat[obs[i].sat - 1].gf[0] = g1;
        if (g0 != 0.0 && fabs(g1 - g0) > rtk->opt.thresslip) {
            trace(NULL, 2,
                  "detslp_gf_L1_L2: slip detected sat=%2d gf=%8.3f->%8.3f\n",
                  obs[i].sat, g0, g1);
            rtk->ssat[obs[i].sat - 1].slip[0] |= 1;
            rtk->ssat[obs[i].sat - 1].slip[1] |= 1;
        }
    }

    trace(NULL, 3, "detslp_gf_L1_L5\n");

    /* slip detection for L1-L5 GF */
    if (rtk->opt.nf <= 2) {
        return;
    }
    for (i = 0; i < n && i < MAXOBS; i++) {
        if ((g1 = gfmeas_L1L5(obs + i, nav)) == 0.0) {
            continue;
        }
        g0 = rtk->ssat[obs[i].sat - 1].gf[1]; /* gf[1] = L1-L5 */
        rtk->ssat[obs[i].sat - 1].gf[1] = g1;
        if (g0 != 0.0 && fabs(g1 - g0) > rtk->opt.thresslip) {
            trace(NULL, 2,
                  "detslp_gf_L1_L5: slip detected sat=%2d gf=%8.3f->%8.3f\n",
                  obs[i].sat, g0, g1);
            rtk->ssat[obs[i].sat - 1].slip[0] |= 1;
            rtk->ssat[obs[i].sat - 1].slip[2] |= 1;
        }
    }
}

/*============================================================================
 * Phase bias time update
 *===========================================================================*/

/**
 * @brief Temporal update of phase biases.
 * @param[in,out] rtk RTK control struct
 * @param[in]     tt  Time step (s)
 * @param[in]     obs Observation data
 * @param[in]     n   Number of observations
 * @param[in]     nav Navigation data
 */
static void udbias_ppp(rtk_t *rtk, double tt, const obsd_t *obs, int n,
                        const nav_t *nav)
{
    double offset, lami, cp, pr, com_bias;
    double *bias;
    int i, j, f, slip, nf = NF_RTK(&rtk->opt), reset, isat;

    trace(NULL, 3, "udbias  : tt=%.1f n=%d\n", tt, n);

    for (i = 0; i < MAXSAT; i++) {
        for (j = 0; j < rtk->opt.nf; j++) {
            rtk->ssat[i].slip[j] = 0;
            rtk->ssat[i].pbreset[j] = 0;
        }
    }

    /* detect cycle slip by LLI */
    detslp_ll(rtk, obs, n);

    /* detect cycle slip by geometry-free phase jump */
    detslp_gf(rtk, obs, n, nav);

    /* detect cycle slip by Doppler rate (demo5) */
    detslp_dop(rtk, obs, n, nav);

    /* detect cycle slip by observation code change (demo5) */
    detslp_code(rtk, obs, n);

    for (f = 0; f < nf; f++) {
        /* reset phase-bias if expire obs outage counter */
        for (i = 0; i < MAXSAT; i++) {
            reset = ++rtk->ssat[i].outc[f] > (unsigned int)rtk->opt.maxout;

            if (rtk->opt.modear == ARMODE_INST &&
                rtk->x[IB_RTK(i + 1, f, &rtk->opt)] != 0.0) {
                initx(rtk, 0.0, 0.0, IB_RTK(i + 1, f, &rtk->opt));
            }
            else if (reset && rtk->x[IB_RTK(i + 1, f, &rtk->opt)] != 0.0) {
                initx(rtk, 0.0, 0.0, IB_RTK(i + 1, f, &rtk->opt));
                trace(NULL, 3,
                      "udbias : obs outage counter overflow (sat=%3d L%d n=%5d)\n",
                      i + 1, f + 1, rtk->ssat[i].outc[f]);
            }
            if (rtk->opt.modear != ARMODE_INST && reset) {
                rtk->ssat[i].lock[f] = -rtk->opt.minlock;
            }
        }

        /* reset phase-bias if detecting cycle slip */
        for (i = 0; i < n && i < MAXOBS; i++) {
            j = IB_RTK(obs[i].sat, f, &rtk->opt);
            rtk->P[j + j * rtk->nx] += rtk->opt.prn[0] * rtk->opt.prn[0] * tt;
            slip = rtk->ssat[obs[i].sat - 1].slip[f];
            if (rtk->opt.ionoopt == IONOOPT_IFLC) {
                slip |= rtk->ssat[obs[i].sat - 1].slip[1];
            }
            if (rtk->opt.modear == ARMODE_INST || !(slip & 1)) {
                continue;
            }
            if (!(slip & 1)) {
                continue;
            }
            rtk->x[j] = 0.0;
            rtk->ssat[obs[i].sat - 1].lock[f] = -rtk->opt.minlock;
        }

        bias = zeros(n, 1);

        /* estimate approximate phase-bias by phase - code */
        for (i = j = 0, offset = 0.0; i < n; i++) {
            cp = obs[i].L[f];
            pr = obs[i].P[f];
            lami = sat_lambda(obs[i].sat, f, nav);
            if (cp == 0.0 || pr == 0.0 || lami == 0.0) {
                continue;
            }

            bias[i] = cp * lami - pr;

            /* offset = sum of (bias - phase-bias) for all valid sats in meters */
            if (rtk->x[IB_RTK(obs[i].sat, f, &rtk->opt)] != 0.0) {
                lami = sat_lambda(obs[i].sat, f, nav);
                offset += bias[i] - rtk->x[IB_RTK(obs[i].sat, f, &rtk->opt)] * lami;
                j++;
            }
        }

        com_bias = j > 0 ? offset / j : 0.0;

        /* set initial states of phase-bias */
        for (i = 0; i < n; i++) {
            isat = obs[i].sat;
            if (bias[i] == 0.0 || rtk->x[IB_RTK(isat, f, &rtk->opt)] != 0.0) {
                continue;
            }
            trace(NULL, 3, "set initial states of phase-bias, sat=%2d f=%2d\n",
                  obs[i].sat, f + 1);
            lami = sat_lambda(isat, f, nav);
            if (lami == 0.0) {
                continue;
            }
            initx(rtk, (bias[i] - com_bias) / lami, SQR(rtk->opt.std[0]),
                  IB_RTK(obs[i].sat, f, &rtk->opt));
            rtk->ssat[isat - 1].lock[f] = -rtk->opt.minlock;
            /* suppress pbreset after floatcnt mass reset: not an actual
             * phase bias change, so compensatedisp should continue normally */
            if (!prtk_ctx.floatcnt_reset) {
                rtk->ssat[isat - 1].pbreset[f] = 1;
            }
        }

        free(bias);
    }
    /* clear floatcnt_reset flag after bias update */
    prtk_ctx.floatcnt_reset = 0;
}

/*============================================================================
 * State update dispatcher
 *===========================================================================*/

/**
 * @brief Temporal update of all PPP-RTK states.
 * @param[in,out] rtk RTK control struct
 * @param[in]     obs Observation data
 * @param[in]     n   Number of observations
 * @param[in]     nav Navigation data
 */
static void udstate_ppp(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    double tt = fabs(rtk->tt);

    trace(NULL, 3, "udstate_ppp: n=%d tt=%f\n", n, tt);

    /* temporal update of position */
    udpos_ppp(rtk, obs, n, tt);

    /* temporal update of tropospheric parameters */
    if (rtk->opt.tropopt >= TROPOPT_EST) {
        udtrop(rtk, tt);
    }

    /* temporal update of phase-bias */
    udbias_ppp(rtk, tt, obs, n, nav);

    /* temporal update of ionospheric parameters */
    if (rtk->opt.ionoopt >= IONOOPT_EST) {
        udion(rtk, tt, BASELEN, obs, n);
    }
}

/*============================================================================
 * Precise troposphere model for KF estimation
 *===========================================================================*/

/**
 * @brief Compute precise tropospheric delay for KF.
 * @param[in]  time  Observation time
 * @param[in]  pos   Receiver position (lat/lon/hgt)
 * @param[in]  r     Unused (receiver index, always 0)
 * @param[in]  azel  Azimuth/elevation (rad)
 * @param[in]  opt   Processing options
 * @param[in]  x     State vector
 * @param[out] dtdx  Partial derivatives [3]
 * @return Tropospheric delay (m)
 */
static double prectrop2(gtime_t time, const double *pos, int r,
                        const double *azel, const prcopt_t *opt,
                        const double *x, double *dtdx)
{
    double m_w = 0.0, cotz, grad_n, grad_e;
    int i = IT_RTK(opt);

    (void)r;

    /* wet mapping function */
    tropmapf(time, pos, azel, &m_w);

    if (opt->tropopt >= TROPOPT_ESTG && azel[1] > 0.0) {
        /* m_w=m_0+m_0*cot(el)*(Gn*cos(az)+Ge*sin(az)): ref [6] */
        cotz = 1.0 / tan(azel[1]);
        grad_n = m_w * cotz * cos(azel[0]);
        grad_e = m_w * cotz * sin(azel[0]);
        m_w += grad_n * x[i + 1] + grad_e * x[i + 2];
        dtdx[1] = grad_n * x[i];
        dtdx[2] = grad_e * x[i];
    } else {
        dtdx[1] = dtdx[2] = 0.0;
    }
    dtdx[0] = m_w;
    return m_w * x[i];
}

/*============================================================================
 * Validity check for ZD observation
 *===========================================================================*/

/**
 * @brief Check if zero-difference observation is valid.
 * @param[in] i   Observation index
 * @param[in] f   Frequency index (0..nf*2-1)
 * @param[in] nf  Number of frequencies
 * @param[in] y   ZD residual array
 * @return 1 if valid, 0 otherwise
 */
static int validobs(int i, int f, int nf, double *y)
{
    return y[f + i * nf * 2] != 0.0 && (f < nf || (y[f - nf + i * nf * 2] != 0.0));
}

/*============================================================================
 * DD covariance matrix
 *===========================================================================*/

/**
 * @brief Build DD covariance from SD variances.
 * @param[in]  nb  Block sizes array
 * @param[in]  n   Number of blocks
 * @param[in]  Ri  Reference satellite variances
 * @param[in]  Rj  Non-reference satellite variances
 * @param[in]  nv  Total number of DD observations
 * @param[out] R   DD covariance matrix (nv x nv)
 */
static void ddcov(const int *nb, int n, const double *Ri, const double *Rj,
                  int nv, double *R)
{
    int i, j, k = 0, b;

    for (i = 0; i < nv * nv; i++) {
        R[i] = 0.0;
    }
    for (b = 0; b < n; k += nb[b++]) {
        for (i = 0; i < nb[b]; i++) {
            for (j = 0; j < nb[b]; j++) {
                R[k + i + (k + j) * nv] = Ri[k + i] + ((i == j) ? Rj[k + i] : 0.0);
            }
        }
    }
}

/*============================================================================
 * Simplified system test
 *===========================================================================*/

/**
 * @brief Test if satellite system matches group index.
 * @param[in] sys        Satellite system (SYS_*)
 * @param[in] m          Group index (0:GPS/SBS, 1:GLO, 2:GAL, 3:BDS, 4:QZS, 5:GPS-L2C)
 * @param[in] qzsmodear  QZSS AR mode (0:off, 1:on, 2:gps-qzs)
 * @param[in] code       Observation code indicator (CODE_*)
 * @return 1 if match, 0 otherwise
 */
static int test_sys(int sys, int m, int qzsmodear, int code)
{
    switch (sys) {
    case SYS_GPS: return m == (code == CODE_L2X ? 5 : 0);
    case SYS_QZS: return m == (qzsmodear == 2 ? 0 : 4);
    case SYS_SBS: return m == 0;
    case SYS_GLO: return m == 1;
    case SYS_GAL: return m == 2;
    case SYS_CMP: return m == 3;
    }
    return 0;
}

/*============================================================================
 * Residual test (chi-squared outlier rejection)
 *===========================================================================*/

/**
 * @brief Validate residuals and reject outliers.
 * @param[in,out] rtk   RTK control struct
 * @param[in]     vflg  Residual flags (sat/type/freq encoded)
 * @param[in]     v     Residual vector
 * @param[in]     Q     Innovation covariance matrix (m x m)
 * @param[out]    iv    Indices of valid residuals
 * @param[in]     m     Total number of residuals
 * @param[out]    ns    Count per frequency/type
 * @param[in]     flg   Stage flag (0:prefit, 1:postfit float, 2:postfit fix)
 * @return Number of valid residuals
 */
static int residual_test(rtk_t *rtk, const int *vflg, const double *v,
                         const double *Q, int *iv, const int m, int *ns,
                         const int flg)
{
    int i, k, sat2, type, freq, nf = rtk->opt.nf;
    double inno0, inno1, inno2, gamma;
    double *resc, *resp, sig2, vv0_, vvf_, vv = 0.0;
    int cnt, cnt2;
    char *stype;

    /* per-component rejection thresholds (rejionno1-3) or fallback to maxinno */
    inno0 = rtk->opt.maxinno_ext[0] > 0.0 ? rtk->opt.maxinno_ext[0] : rtk->opt.maxinno;
    inno1 = rtk->opt.maxinno_ext[1] > 0.0 ? rtk->opt.maxinno_ext[1] : rtk->opt.maxinno;
    inno2 = rtk->opt.maxinno_ext[2] > 0.0 ? rtk->opt.maxinno_ext[2] : rtk->opt.maxinno;

    resc = zeros(MAXSAT * nf, 1);
    resp = zeros(MAXSAT * nf, 1);

    /* collect carrier phase residuals */
    for (i = cnt2 = 0; i < m; i++) {
        sat2 = (vflg[i] >> 8) & 0xFF;
        type = (vflg[i] >> 4) & 0xF;
        freq = (vflg[i]) & 0xF;
        if (type == 0) {
            resc[(sat2 - 1) * nf + freq] = v[i];
            resp[(sat2 - 1) * nf + freq] = Q[i + i * m];
            cnt2++;
        }
    }

    /* multi-frequency dispersive/non-dispersive validation */
    for (i = 0; i < MAXSAT; i++) {
        for (k = 1; k < nf; k++) {
            int qi = 0, qj = k;
            gamma = SQR(lam_carr[qj]) / SQR(lam_carr[qi]);
            if (resp[i * nf + qi] == 0.0 || resp[i * nf + qj] == 0.0) {
                continue;
            }
            vvf_ = FREQ1 / FREQ2 * (resc[i * nf + qi] - resc[i * nf + qj]) / (1.0 - gamma);
            vv0_ = (gamma * resc[i * nf + qi] - resc[i * nf + qj]) / (gamma - 1.0);
            sig2 = resp[i * nf + qi] > resp[i * nf + qj] ?
                   resp[i * nf + qi] : resp[i * nf + qj];

            /* dispersive term validation */
            if ((inno1 > 0.0) && (vvf_ * vvf_ > inno1 * inno1 * sig2)) {
                rtk->ssat[i].vsat[qi] = rtk->ssat[i].vsat[qj] = 0;
                rtk->ssat[i].rejc[qi]++;
                rtk->ssat[i].rejc[qj]++;
                trace(NULL, 2,
                      "Residuals validation:freq outlier detected "
                      "(stage=%1d sat=%2d f=%d %d v=%6.3f sig=%.3f)\n",
                      flg, i + 1, qi, qj, vvf_, sqrt(sig2));
                continue;
            }
            /* non-dispersive (L0) term validation */
            if ((inno2 > 0.0) && (vv0_ * vv0_ > inno2 * inno2 * sig2)) {
                rtk->ssat[i].vsat[qi] = rtk->ssat[i].vsat[qj] = 0;
                rtk->ssat[i].rejc[qi]++;
                rtk->ssat[i].rejc[qj]++;
                trace(NULL, 2,
                      "Residuals validation:L0 outlier detected "
                      "(stage=%1d sat=%2d v=%6.3f sig=%.3f)\n",
                      flg, i + 1, vv0_, sqrt(sig2));
                continue;
            }
        }
    }

    /* pre/post residuals test */
    for (i = k = cnt = 0; i < m; i++) {
        sat2 = (vflg[i] >> 8) & 0xFF;
        type = (vflg[i] >> 4) & 0xF;
        freq = (vflg[i]) & 0xF;
        if (type == 0) {
            stype = "L";
        } else if (type == 1) {
            stype = "P";
        } else {
            stype = "C";
        }
        (void)stype;

        /* validate individual residuals */
        /* D2: inflate threshold 10x for newly-initialized phase bias (demo5) */
        double inno0_eff = inno0;
        if (type == 0 && rtk->opt.std[0] > 0.0) {
            int ib = IB_RTK(sat2, freq, &rtk->opt);
            if (rtk->P[ib + ib * rtk->nx] == SQR(rtk->opt.std[0])) {
                inno0_eff *= 10.0;
            }
        }
        if (!Q[i + i * m] || inno0 == 0 ||
            (v[i] * v[i] < Q[i + i * m] * inno0_eff * inno0_eff)) {
            if (type == 0) {
                if (!rtk->ssat[sat2 - 1].vsat[freq]) {
                    continue;
                }
                vv += v[i] * v[i] / Q[i + i * m];
                cnt++;
            }
            ns[type < 1 ? freq : freq * type + nf]++;
            iv[k++] = i;
        } else {
            if (type == 0) {
                rtk->ssat[sat2 - 1].vsat[freq] = 0;
            }
            trace(NULL, 2,
                  "meas update outlier detected "
                  "(sat=%2d %s%d v=%6.3f sig=%6.3f outc=%2d)\n",
                  sat2, stype, freq + 1, v[i], sqrt(Q[i + i * m]),
                  rtk->ssat[sat2 - 1].outc[freq]);
        }
    }

    /* chi-square validation for carrier residuals */
    if (cnt > NP_RTK(&rtk->opt)) {
        if (vv > 1.0 * chisqr[cnt - NP_RTK(&rtk->opt) - 1]) {
            trace(NULL, 2,
                  "L1/L2 chi-square validation failed "
                  "(stage=%d cnt=%d np=%d vv=%.2f cs=%5.3f)\n",
                  flg, cnt, NP_RTK(&rtk->opt), vv,
                  chisqr[cnt - NP_RTK(&rtk->opt) - 1]);
        }
        prtk_ctx.chisq = vv / chisqr[cnt - NP_RTK(&rtk->opt) - 1];
    } else {
        prtk_ctx.chisq = ((double)cnt / (cnt2 > 0 ? cnt2 : 1)) < 0.5 ? 100.0 : 0.0;
    }

    free(resc);
    free(resp);

    return k;
}

/*============================================================================
 * Inner Kalman filter update with outlier rejection
 *===========================================================================*/

/**
 * @brief Inner KF measurement update with residual test.
 * @param[in,out] rtk  RTK control struct
 * @param[in]     x    State vector (n)
 * @param[in]     P    Covariance matrix (n x n)
 * @param[in]     H    Observation matrix (n x m)
 * @param[in]     v    Residual vector (m)
 * @param[in]     R    Measurement noise (m x m)
 * @param[in]     n    Number of states
 * @param[in]     m    Number of measurements
 * @param[out]    xp   Updated state vector (n)
 * @param[out]    Pp   Updated covariance (n x n)
 * @param[in]     vflg Residual flags
 * @param[in]     flg  Stage flag (0:prefit, 1+:postfit)
 * @return 0 on success, non-zero on error
 */
static int filter2_(rtk_t *rtk, const double *x, const double *P,
                    const double *H, const double *v, const double *R,
                    const int n, const int m, double *xp, double *Pp,
                    const int *vflg, const int flg)
{
    double *K, *v_, *Q_, *F_, *H_, *I, *F, *Q;
    int i, j, k, info = 0, *iv, nn[6];

    Q = mat(m, m); F = mat(n, m); iv = imat(m, 1);
    memset(nn, 0, sizeof(nn));
    matcpy(Q, R, m, m);
    matcpy(xp, x, n, 1);
    matmul("NN", n, m, n, 1.0, P, H, 0.0, F);       /* F=P*H */
    matmul("TN", m, m, n, 1.0, H, F, 1.0, Q);       /* Q=H'*P*H+R */

    /* prefit/postfit residuals test */
    if (!(k = residual_test(rtk, vflg, v, Q, iv, m, nn, flg))) {
        trace(NULL, 2, "filter status (all measurements rejected): stage=%1d\n", flg);
    } else {
        trace(NULL, 2,
              "filter status: stage=%1d refsat=%2d states=%2d obs=%2d "
              "vobs=%2d vsat(L1)=%2d chi-val=%5.3f\n",
              flg, (vflg[iv[0]] >> 16) & 0xFF, n, m, k, nn[0], prtk_ctx.chisq);
    }

    if (flg > 0 || k == 0) {
        matcpy(Pp, P, n, n);
        free(Q); free(F); free(iv);
        return info;
    }

    v_ = mat(k, 1); Q_ = mat(k, k); F_ = mat(n, k); H_ = mat(n, k);
    K = mat(n, k);
    for (i = 0; i < k; i++) {
        v_[i] = v[iv[i]];
        for (j = 0; j < k; j++) {
            Q_[i + j * k] = Q[iv[i] + iv[j] * m];
        }
        for (j = 0; j < n; j++) {
            F_[j + i * n] = F[j + iv[i] * n];
        }
        for (j = 0; j < n; j++) {
            H_[j + i * n] = H[j + iv[i] * n];
        }
    }

    I = eye(n);
    if (!(info = matinv(Q_, k))) {
        matmul("NN", n, k, k, 1.0, F_, Q_, 0.0, K);    /* K=P*H*Q^-1 */
        matmul("NN", n, 1, k, 1.0, K, v_, 1.0, xp);    /* xp=x+K*v */
        matmul("NT", n, n, k, -1.0, K, H_, 1.0, I);    /* I=I-K*H' */
        matmul("NN", n, n, n, 1.0, I, P, 0.0, Pp);     /* Pp=(I-K*H')*P */

        /* compute innovation covariance Qp = K*v * (K*v)^T for adaptive noise */
        if (rtk->opt.ionoopt == IONOOPT_EST_ADPT || rtk->opt.prnadpt) {
            double *vd = mat(n, 1);
            /* allocate/reallocate Qp if needed */
            if (prtk_ctx.Qp_nx != n) {
                free(prtk_ctx.Qp);
                prtk_ctx.Qp = zeros(n, n);
                prtk_ctx.Qp_nx = n;
            }
            matmul("NN", n, 1, k, 1.0, K, v_, 0.0, vd);     /* vd=K*v */
            matmul("NT", n, n, 1, 1.0, vd, vd, 0.0, prtk_ctx.Qp); /* Qp=vd*vd' */
            free(vd);
        }
    }
    free(I);

    free(iv); free(v_); free(Q_); free(F_); free(H_); free(K);
    free(Q); free(F);

    return info;
}

/*============================================================================
 * Outer KF wrapper (state compaction)
 *===========================================================================*/

/**
 * @brief KF measurement update with state compaction.
 *
 * Compacts the state vector to exclude zero/invalid states before calling
 * the inner filter, then maps results back.
 *
 * @param[in,out] rtk  RTK control struct
 * @param[in,out] x    State vector (n)
 * @param[in,out] P    Covariance matrix (n x n)
 * @param[in]     H    Observation matrix (n x m)
 * @param[in]     v    Residual vector (m)
 * @param[in]     R    Measurement noise (m x m)
 * @param[in]     n    Number of states
 * @param[in]     m    Number of measurements
 * @param[in]     vflg Residual flags
 * @param[in]     flg  Stage flag
 * @return 0 on success, non-zero on error
 */
static int filter2(rtk_t *rtk, double *x, double *P, const double *H,
                   const double *v, const double *R, const int n,
                   const int m, const int *vflg, const int flg)
{
    double *x_, *xp_, *P_, *Pp_, *H_;
    int i, j, k, info, *ix;

    ix = imat(n, 1);
    for (i = k = 0; i < n; i++) {
        if (x[i] != 0.0 && P[i + i * n] > 0.0) {
            ix[k++] = i;
        }
    }
    x_ = mat(k, 1); xp_ = mat(k, 1); P_ = mat(k, k); Pp_ = mat(k, k);
    H_ = mat(k, m);

    for (i = 0; i < k; i++) {
        x_[i] = x[ix[i]];
        for (j = 0; j < k; j++) {
            P_[i + j * k] = P[ix[i] + ix[j] * n];
        }
        for (j = 0; j < m; j++) {
            H_[i + j * k] = H[ix[i] + j * n];
        }
    }

    info = filter2_(rtk, x_, P_, H_, v, R, k, m, xp_, Pp_, vflg, flg);

    for (i = 0; i < k; i++) {
        x[ix[i]] = xp_[i];
        for (j = 0; j < k; j++) {
            P[ix[i] + ix[j] * n] = Pp_[i + j * k];
        }
    }

    free(ix); free(x_); free(xp_); free(P_); free(Pp_); free(H_);
    return info;
}

/*============================================================================
 * DD residuals (simplified single-channel)
 *===========================================================================*/

/**
 * @brief Compute DD residuals and measurement matrix.
 *
 * Simplified single-channel version (no l6mrg, no multi-ch).
 *
 * @param[in,out] rtk    RTK control struct
 * @param[in]     nav    Navigation data
 * @param[in]     x      State vector
 * @param[in,out] pbslip Phase bias slip corrections (NULL for postfit)
 * @param[in]     P      Covariance matrix (may be NULL)
 * @param[in]     obs    Observation data
 * @param[in]     y      ZD residuals
 * @param[in]     e      Line-of-sight vectors (3*n)
 * @param[in]     azel   Azimuth/elevation (2*n)
 * @param[in]     n      Number of observations
 * @param[out]    v      DD residuals
 * @param[out]    H      Measurement matrix (may be NULL)
 * @param[out]    R      Measurement covariance
 * @param[out]    vflg   Residual flags
 * @param[in]     niter  Reference satellite search iteration
 * @return Number of DD residuals
 */
static int ddres(rtk_t *rtk, const nav_t *nav, double *x, double *pbslip,
                 const double *P, const obsd_t *obs, double *y, double *e,
                 double *azel, int n, double *v, double *H, double *R,
                 int *vflg, int niter)
{
    prcopt_t *opt = &rtk->opt;
    double pos[3], lami, lamj, *Ri, *Rj, *Hi = NULL;
    double *tropu, *im, *dtdxu, didxi = 0.0, didxj = 0.0, fi, fj;
    int i, j, k, m, f, nv = 0, nb[NFREQ * 4 * 2 + 1] = {0}, b = 0;
    int sati, satj, sysi, sysj, nf = NF_RTK(opt), flg = 0;
    int ff;

    (void)P;

    trace(NULL, 3, "ddres   :niter=%2d nx=%d n=%d\n", niter, rtk->nx, n);

    im = mat(n, 1); tropu = mat(n, 1); dtdxu = mat(n, 3);
    Ri = mat(n * nf * 2 * 2, 1); Rj = mat(n * nf * 2 * 2, 1);
    ecef2pos(x, pos);

    for (i = 0; i < MAXSAT; i++) {
        for (j = 0; j < NFREQ; j++) {
            rtk->ssat[i].resp[j] = rtk->ssat[i].resc[j] = 0.0;
        }
    }

    /* compute ionospheric and tropospheric mapping factors */
    for (i = 0; i < n; i++) {
        if (opt->ionoopt >= IONOOPT_EST) {
            im[i] = ionmapf(pos, azel + i * 2);
        }
        if (opt->tropopt >= TROPOPT_EST) {
            tropu[i] = prectrop2(rtk->sol.time, pos, 0, azel + i * 2,
                                 opt, x, dtdxu + i * 3);
        }
    }

    /* system loop: 0=GPS/SBS, 1=GLO, 2=GAL, 3=BDS, 4=QZS, 5=GPS(L2C) */
    for (m = 0; m < 6; m++) {
        if (m == 1 && rtk->opt.glomodear == 0) {
            continue;
        }
        if (m == 4 && rtk->opt.qzsmodear != 1) {
            continue;
        }

        for (f = opt->mode > PMODE_DGPS ? 0 : nf; f < nf * 2; f++) {

            /* search reference satellite with highest elevation */
            for (i = -1, j = 0; j < n; j++) {
                flg = 0;
                sati = obs[j].sat;
                sysi = rtk->ssat[sati - 1].sys;
                if (!test_sys(sysi, m, rtk->opt.qzsmodear, rtk->ssat[sati - 1].code[f % nf])) {
                    continue;
                }
                if (niter > 0) {
                    for (k = 0; k < niter; k++) {
                        if (prtk_ctx.refsat[NFREQ * 2 * MAXREF * m + NFREQ * 2 * k + f] == sati) {
                            flg = 1;
                        }
                    }
                    if (flg == 1) {
                        continue;
                    }
                }
                if (!validobs(j, f, nf, y)) {
                    continue;
                }
                if (i < 0 || azel[1 + j * 2] >= azel[1 + i * 2]) {
                    i = j;
                }
            }
            if (i < 0) {
                continue;
            }

            sati = obs[i].sat;
            prtk_ctx.refsat[NFREQ * 2 * MAXREF * m + NFREQ * 2 * niter + f] = sati;
            prtk_ctx.refsat2[NFREQ * 2 * m + f] = sati;

            if (niter > 0) {
                trace(NULL, 2, "refsat changed %s L%d sat=%2d -> %2d\n",
                      time_str(rtk->sol.time, 0), f % nf + 1,
                      prtk_ctx.refsat[NFREQ * MAXREF * 2 * m +
                                      NFREQ * 2 * (niter - 1) + f],
                      prtk_ctx.refsat[NFREQ * MAXREF * 2 * m +
                                      NFREQ * 2 * niter + f]);
            }

            if (f < nf) {
                trace(NULL, 4, "refsat m:%d L%d %s: %d niter:%d\n",
                      m, f + 1, time_str(rtk->sol.time, 0), sati, niter);
            }

            /* apply reference satellite phase bias slip correction */
            if (f < nf && pbslip != NULL) {
                x[IB_RTK(sati, f, opt)] += pbslip[IB_RTK(sati, f, opt)];
                pbslip[IB_RTK(sati, f, opt)] = 0.0;
            }

            /* form DD with each non-reference satellite */
            for (j = 0; j < n; j++) {
                if (i == j) {
                    continue;
                }
                satj = obs[j].sat;
                sysi = rtk->ssat[sati - 1].sys;
                sysj = rtk->ssat[satj - 1].sys;
                if (!test_sys(sysj, m, rtk->opt.qzsmodear,
                              rtk->ssat[satj - 1].code[f % nf])) {
                    continue;
                }
                if (!validobs(j, f, nf, y)) {
                    continue;
                }

                ff = f % nf;
                lami = sat_lambda(sati, ff, nav);
                lamj = sat_lambda(satj, ff, nav);
                if (lami <= 0.0 || lamj <= 0.0) {
                    continue;
                }

                /* apply non-ref satellite phase bias slip correction */
                if (f < nf && pbslip != NULL) {
                    x[IB_RTK(satj, f, opt)] += pbslip[IB_RTK(satj, f, opt)];
                    pbslip[IB_RTK(satj, f, opt)] = 0.0;
                }

                if (H) {
                    Hi = H + nv * rtk->nx;
                    for (k = 0; k < rtk->nx; k++) {
                        Hi[k] = 0.0;
                    }
                }

                /* DD residual */
                v[nv] = y[f + i * nf * 2] - y[f + j * nf * 2];

                /* partial derivatives by rover position */
                if (H) {
                    for (k = 0; k < 3; k++) {
                        Hi[k] = -e[k + i * 3] + e[k + j * 3];
                    }
                }

                /* DD ionospheric delay term */
                if (opt->ionoopt == IONOOPT_EST ||
                    opt->ionoopt == IONOOPT_EST_ADPT) {
                    fi = lami / lam_carr[0];
                    fj = lamj / lam_carr[0];
                    didxi = (f < nf ? -1.0 : 1.0) * fi * fi * im[i];
                    didxj = (f < nf ? -1.0 : 1.0) * fj * fj * im[j];
                    v[nv] -= didxi * x[II_RTK(sati, opt)] -
                             didxj * x[II_RTK(satj, opt)];
                    if (H) {
                        Hi[II_RTK(sati, opt)] =  didxi;
                        Hi[II_RTK(satj, opt)] = -didxj;
                    }
                }

                /* DD tropospheric delay term */
                if (opt->tropopt == TROPOPT_EST || opt->tropopt == TROPOPT_ESTG) {
                    v[nv] -= (tropu[i] - tropu[j]);
                    for (k = 0; k < (opt->tropopt < TROPOPT_ESTG ? 1 : 3); k++) {
                        if (!H) {
                            continue;
                        }
                        Hi[IT_RTK(opt) + k] = (dtdxu[k + i * 3] - dtdxu[k + j * 3]);
                    }
                }

                /* DD phase-bias term */
                if (f < nf) {
                    v[nv] -= lami * x[IB_RTK(sati, f, opt)] -
                             lamj * x[IB_RTK(satj, f, opt)];
                    if (H) {
                        Hi[IB_RTK(sati, f, opt)] =  lami;
                        Hi[IB_RTK(satj, f, opt)] = -lamj;
                    }
                }

                /* measurement error variance */
                if (f < nf) { /* phase */
                    rtk->ssat[satj - 1].resc[f] = v[nv];
                    Ri[nv] = varerr(sati, sysi, azel[1 + i * 2], 0, opt);
                    Rj[nv] = varerr(satj, sysj, azel[1 + j * 2], 0, opt);
                    /* L2 scaling factor */
                    if (f == 1) {
                        Ri[nv] *= SQR(2.55 / 1.55);
                        Rj[nv] *= SQR(2.55 / 1.55);
                    }
                } else { /* code */
                    rtk->ssat[satj - 1].resp[f - nf] = v[nv];
                    Ri[nv] = varerr(sati, sysi, azel[1 + i * 2], 1, opt);
                    Rj[nv] = varerr(satj, sysj, azel[1 + j * 2], 1, opt);
                }

                /* set valid data flags */
                if (f < nf) {
                    rtk->ssat[sati - 1].vsat[f] = 1;
                    rtk->ssat[satj - 1].vsat[f] = 1;
                }

                vflg[nv++] = (sati << 16) | (satj << 8) |
                             ((f < nf ? 0 : 1) << 4) | (f % nf);
                nb[b]++;
            }
            b++;
        }
    } /* end of system loop */

    /* measurement error covariance */
    ddcov(nb, b, Ri, Rj, nv, R);

    free(Ri); free(Rj); free(im); free(tropu); free(dtdxu);

    return nv;
}

/*============================================================================
 * SD-to-DD transformation matrix
 *===========================================================================*/

/**
 * @brief Build SD-to-DD transformation matrix D.
 * @param[in,out] rtk  RTK control struct
 * @param[out]    D    Transformation matrix (nx x nx)
 * @param[out]    indI Reference satellite bias indices
 * @param[out]    indJ Non-reference satellite bias indices
 * @return Number of DD ambiguities (nb)
 */
static int ddmat(rtk_t *rtk, double *D, int *indI, int *indJ)
{
    int i, j, k, m, f, nb = 0, nx = rtk->nx, na = rtk->na, code;
    int nf = NF_RTK(&rtk->opt);

    trace(NULL, 3, "ddmat   :\n");

    for (i = 0; i < MAXSAT; i++) {
        for (j = 0; j < NFREQ; j++) {
            rtk->ssat[i].fix[j] = 0;
        }
    }
    for (i = 0; i < na; i++) {
        D[i + i * nx] = 1.0;
    }

    for (m = 0; m < 6; m++) { /* 0:GPS/SBS, 1:GLO, 2:GAL, 3:BDS, 4:QZS, 5:GPS(L2C) */
        if (m == 1 && rtk->opt.glomodear == 0) {
            continue;
        }
        if (m == 4 && rtk->opt.qzsmodear != 1) {
            continue;
        }

        for (f = 0, k = na; f < nf; f++, k += MAXSAT) {
            /* find reference satellite (highest elevation with lock) */
            for (i = k; i < k + MAXSAT; i++) {
                code = rtk->ssat[i - k].code[f];
                if (rtk->x[i] == 0.0 ||
                    !test_sys(rtk->ssat[i - k].sys, m,
                              rtk->opt.qzsmodear, code) ||
                    !rtk->ssat[i - k].vsat[f]) {
                    continue;
                }
                if (rtk->ssat[i - k].lock[f] > 0 &&
                    !(rtk->ssat[i - k].slip[f] & 2) &&
                    rtk->ssat[i - k].azel[1] >= rtk->opt.elmaskar) {
                    rtk->ssat[i - k].fix[f] = 2; /* fix */
                    break;
                } else {
                    rtk->ssat[i - k].fix[f] = 1;
                }
            }
            /* form DD pairs */
            for (j = k; j < k + MAXSAT; j++) {
                code = rtk->ssat[j - k].code[f];
                if (i == j || rtk->x[j] == 0.0 ||
                    !test_sys(rtk->ssat[j - k].sys, m,
                              rtk->opt.qzsmodear, code) ||
                    !rtk->ssat[j - k].vsat[f]) {
                    continue;
                }
                if (rtk->ssat[j - k].lock[f] > 0 &&
                    !(rtk->ssat[j - k].slip[f] & 2) &&
                    rtk->ssat[j - k].azel[1] >= rtk->opt.elmaskar) {

                    D[i + (na + nb) * nx] =  1.0;
                    D[j + (na + nb) * nx] = -1.0;
                    indI[nb] = i - na;
                    indJ[nb] = j - na;
                    nb++;
                    rtk->ssat[j - k].fix[f] = 2; /* fix */
                } else {
                    rtk->ssat[j - k].fix[f] = 1;
                }
            }
        }
    }

    trace(NULL, 5, "D=\n");
    tracemat(NULL, 5, D, nx, na + nb, 2, 0);

    return nb;
}

/*============================================================================
 * Restore SD ambiguities from DD
 *===========================================================================*/

/**
 * @brief Restore SD ambiguities from DD fixed values.
 * @param[in,out] rtk  RTK control struct
 * @param[in]     bias DD ambiguity biases
 * @param[in]     nb   Number of DD ambiguities
 * @param[out]    xa   Fixed state vector
 */
static void restamb(rtk_t *rtk, const double *bias, int nb, double *xa)
{
    int i, n, m, f, index[MAXSAT], nv = 0, nf = NF_RTK(&rtk->opt);

    trace(NULL, 3, "restamb :\n");

    (void)nb;

    /* init all fixed states to float state values */
    for (i = 0; i < rtk->nx; i++) {
        xa[i] = rtk->x[i];
    }
    /* overwrite non phase-bias states with fixed values */
    for (i = 0; i < rtk->na; i++) {
        xa[i] = rtk->xa[i];
    }

    for (m = 0; m < 6; m++) {
        if (m == 1 && rtk->opt.glomodear == 0) {
            continue;
        }
        if (m == 4 && rtk->opt.qzsmodear != 1) {
            continue;
        }

        for (f = 0; f < nf; f++) {
            for (n = i = 0; i < MAXSAT; i++) {
                if (!test_sys(rtk->ssat[i].sys, m,
                              rtk->opt.qzsmodear,
                              rtk->ssat[i].code[f]) ||
                    rtk->ssat[i].fix[f] != 2) {
                    continue;
                }
                index[n++] = IB_RTK(i + 1, f, &rtk->opt);
                rtk->ssat[i].lock[f]++; /* increment AR count */
            }
            if (n < 2) {
                continue;
            }

            xa[index[0]] = rtk->x[index[0]];

            for (i = 1; i < n; i++) {
                xa[index[i]] = xa[index[0]] - bias[nv++];
            }
        }
    }
}

/*============================================================================
 * Hold integer ambiguity
 *===========================================================================*/

/**
 * @brief Hold fixed ambiguities via pseudomeasurement constraint.
 * @param[in,out] rtk RTK control struct
 * @param[in]     xa  Fixed state vector
 */
static void holdamb(rtk_t *rtk, const double *xa)
{
    double *v, *H, *R;
    int i, n, m, f, info, index[MAXSAT], nb = rtk->nx - rtk->na;
    int nv = 0, nf = NF_RTK(&rtk->opt);

    trace(NULL, 3, "holdamb :\n");

    v = mat(nb, 1); H = zeros(nb, rtk->nx);

    for (m = 0; m < 6; m++) {
        if (m == 1 && rtk->opt.glomodear == 0) {
            continue;
        }
        if (m == 4 && rtk->opt.qzsmodear != 1) {
            continue;
        }

        for (f = 0; f < nf; f++) {
            for (n = i = 0; i < MAXSAT; i++) {
                if (!test_sys(rtk->ssat[i].sys, m,
                              rtk->opt.qzsmodear,
                              rtk->ssat[i].code[f]) ||
                    rtk->ssat[i].fix[f] != 2 ||
                    rtk->ssat[i].azel[1] < rtk->opt.elmaskhold ||
                    !rtk->ssat[i].vsat[f]) {
                    continue;
                }
                index[n++] = IB_RTK(i + 1, f, &rtk->opt);
                rtk->ssat[i].fix[f] = 3; /* hold */
            }
            /* constraint to fixed ambiguity */
            for (i = 1; i < n; i++) {
                v[nv] = (xa[index[0]] - xa[index[i]]) -
                        (rtk->x[index[0]] - rtk->x[index[i]]);

                H[index[0] + nv * rtk->nx] =  1.0;
                H[index[i] + nv * rtk->nx] = -1.0;
                nv++;
            }
        }
    }
    if (nv > 0) {
        R = zeros(nv, nv);
        {
            double vh = rtk->opt.varholdamb > 0.0 ? rtk->opt.varholdamb : VARHOLDAMB;
            for (i = 0; i < nv; i++) {
                R[i + i * nv] = vh;
            }
        }

        /* update states with constraints */
        if ((info = filter(rtk->x, rtk->P, H, v, R, rtk->nx, nv))) {
            trace(NULL, 2, "filter error (info=%d)\n", info);
        }
        free(R);
    }
    free(v); free(H);
}

/*============================================================================
 * Sort satellites by elevation (ascending) for PAR
 *===========================================================================*/

/**
 * @brief Sort satellites by elevation angle (ascending) for partial AR.
 * @param[in]  rtk  RTK control struct
 * @param[in]  obs  Observation data array
 * @param[out] isat Sorted satellite numbers
 * @param[out] el   Sorted elevation angles (rad)
 * @param[in]  n    Number of observations
 */
static void elsort(rtk_t *rtk, const obsd_t *obs, int *isat, double *el,
                   int n)
{
    double el_;
    int i, j, k, m = 0, sati;

    for (i = 0; i < n; i++) {
        sati = obs[i].sat;
        el_ = rtk->ssat[sati - 1].azel[1];
        if (m <= 0) {
            isat[m] = sati;
            el[m++] = el_;
        } else {
            for (j = 0; j < m; j++) {
                if (el_ < el[j]) {
                    break;
                }
            }
            if (j >= n) {
                continue;
            }
            for (k = m; k > j; k--) {
                isat[k] = isat[k - 1];
                el[k] = el[k - 1];
            }
            isat[j] = sati;
            el[j] = el_;
            if (m < n) {
                m++;
            }
        }
    }
}

/*============================================================================
 * LAMBDA integer ambiguity resolution
 *===========================================================================*/

/**
 * @brief Resolve integer ambiguity by LAMBDA.
 * @param[in,out] rtk  RTK control struct
 * @param[out]    bias DD fixed ambiguity biases
 * @param[out]    xa   Fixed state vector
 * @return Number of resolved ambiguities (0 = failed)
 */
static int resamb_LAMBDA(rtk_t *rtk, double *bias, double *xa)
{
    int i, j, nb, nc, info, nx = rtk->nx, na = rtk->na;
    int indI[MAXOBS * NFREQ], indJ[MAXOBS * NFREQ];
    double *D, *y, *b, *db, *Qb, *Qab, *QQ, s[2], thres;
    double *D_P;

    trace(NULL, 3, "resamb_LAMBDA : nx=%d\n", nx);

    rtk->sol.ratio = 0.0;

    if (rtk->opt.mode <= PMODE_DGPS || rtk->opt.modear == ARMODE_OFF) {
        return 0;
    }

    /* SD-to-DD transformation matrix */
    D = zeros(nx, nx);
    nb = ddmat(rtk, D, indI, indJ);

    if (nb <= 1) {
        trace(NULL, 2, "no valid double-difference\n");
        free(D);
        return 0;
    }
    {
        int minamb = rtk->opt.minamb > 0 ? rtk->opt.minamb : MINAMB_DEFAULT;
        if (nb < minamb) {
            trace(NULL, 2, "less than minimum number of ambiguities nb=%2d minamb=%d\n",
                  nb, minamb);
            free(D);
            return 0;
        }
    }

    y = mat(na + nb, 1);
    b = mat(nb, 2); db = mat(nb, 1); Qb = mat(nb, nb);
    Qab = mat(na, nb); QQ = mat(na, nb);
    nc = nx - na;
    D_P = mat(nb, nc);

    for (i = 0; i < na; i++) {
        y[i] = rtk->x[i];
        for (j = 0; j < nb; j++) {
            Qab[i + j * na] = rtk->P[i + (na + indI[j]) * nx] -
                              rtk->P[i + (na + indJ[j]) * nx];
        }
    }
    for (i = 0; i < nb; i++) {
        y[i + na] = rtk->x[na + indI[i]] - rtk->x[na + indJ[i]];
        for (j = 0; j < nc; j++) {
            D_P[i + j * nb] = rtk->P[na + indI[i] + (na + j) * nx] -
                              rtk->P[na + indJ[i] + (na + j) * nx];
        }
    }
    for (i = 0; i < nb; i++) {
        for (j = 0; j < nb; j++) {
            Qb[i + j * nb] = D_P[i + indI[j] * nb] - D_P[i + indJ[j] * nb];
        }
    }
    free(D_P);

    /* LAMBDA integer least-squares */
    if (!(info = lambda(nb, 2, y + na, Qb, b, s))) {
        trace(NULL, 5, "N(1)=\n");
        tracemat(NULL, 5, b, 1, nb, 10, 3);
        trace(NULL, 5, "N(2)=\n");
        tracemat(NULL, 5, b + nb, 1, nb, 10, 3);

        rtk->sol.ratio = (s[0] > 0) ? (float)(s[1] / s[0]) : 0.0f;
        if (rtk->sol.ratio > 999.9) {
            rtk->sol.ratio = 999.9f;
        }

        /* F-test threshold (significance level from config or default) */
        {
            int alpha = rtk->opt.alphaar > 0 ? rtk->opt.alphaar : ALPHAAR_DEFAULT;
            thres = (alpha < 6 && nb - 1 < 60) ? qf[alpha][nb - 1] : 1.5;
        }

        /* validation by ratio-test */
        if (s[0] <= 0.0 || s[1] / s[0] >= thres) {
            for (i = 0; i < na; i++) {
                rtk->xa[i] = rtk->x[i];
                for (j = 0; j < na; j++) {
                    rtk->Pa[i + j * na] = rtk->P[i + j * nx];
                }
            }
            for (i = 0; i < nb; i++) {
                bias[i] = b[i];
                y[na + i] -= b[i];
            }
            if (!matinv(Qb, nb)) {
                /* transform float to fixed solution: xa=xa-Qab*Qb\(b0-b) */
                matmul("NN", nb, 1, nb, 1.0, Qb, y + na, 0.0, db);
                matmul("NN", na, 1, nb, -1.0, Qab, db, 1.0, rtk->xa);

                /* covariance of fixed solution: Qa=Qa-Qab*Qb^-1*Qab' */
                matmul("NN", na, nb, nb, 1.0, Qab, Qb, 0.0, QQ);
                matmul("NT", na, na, nb, -1.0, QQ, Qab, 1.0, rtk->Pa);

                trace(NULL, 3,
                      "resamb : validation ok (nb=%d ratio=%.2f s=%.2f/%.2f)\n",
                      nb, (s[0] == 0.0) ? 0.0 : s[1] / s[0], s[1], s[0]);

                /* restore SD ambiguities */
                restamb(rtk, bias, nb, xa);
            } else {
                trace(NULL, 2, "lambda error nb=%d\n", nb);
                nb = 0;
            }
        } else {
            trace(NULL, 3,
                  "ambiguity validation failed "
                  "(nb=%d thres=%.2f ratio=%.2f s=%.2f/%.2f)\n",
                  nb, thres, (s[0] == 0.0) ? 0.0 : s[1] / s[0], s[1], s[0]);
            nb = 0;
        }
    } else {
        trace(NULL, 2, "lambda error (info=%d)\n", info);
        nb = 0;
    }

    free(D); free(y);
    free(b); free(db); free(Qb); free(Qab); free(QQ);

    return nb; /* number of ambiguities */
}

/*============================================================================
 * Public API: solution status output
 *===========================================================================*/

/**
 * @brief Write PPP-RTK solution status to buffer.
 *
 * PPP-RTK uses a different state layout from standard PPP (no clock states,
 * iono at NP, trop at NP+NI). This function outputs state information using
 * the correct PPP-RTK indices (II_RTK, IT_RTK, IB_RTK).
 *
 * @param[in,out] rtk   RTK control/result struct
 * @param[out]    buff  Output buffer
 * @return Number of bytes written
 */
extern int ppprtk_outstat(rtk_t *rtk, char *buff)
{
    ssat_t *ssat;
    double tow,pos[3],vel[3],acc[3],*x;
    int i,j,week,nx=rtk->nx;
    char id[32],*p=buff;

    if (!rtk->sol.stat) {
        return 0;
    }

    tow=time2gpst(rtk->sol.time,&week);
    x=rtk->sol.stat==SOLQ_FIX?rtk->xa:rtk->x;

    /* receiver position */
    p+=sprintf(p,"$POS,%d,%.3f,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",week,tow,
               rtk->sol.stat,x[0],x[1],x[2],
               SQRT(rtk->P[0]),SQRT(rtk->P[1+nx]),SQRT(rtk->P[2+2*nx]));

    /* receiver velocity and acceleration */
    if (rtk->opt.dynamics) {
        ecef2pos(rtk->sol.rr,pos);
        ecef2enu(pos,rtk->x+3,vel);
        ecef2enu(pos,rtk->x+6,acc);
        p+=sprintf(p,"$VELACC,%d,%.3f,%d,%.4f,%.4f,%.4f,%.5f,%.5f,%.5f,%.4f,%.4f,"
                   "%.4f,%.5f,%.5f,%.5f\n",week,tow,rtk->sol.stat,vel[0],vel[1],
                   vel[2],acc[0],acc[1],acc[2],0.0,0.0,0.0,0.0,0.0,0.0);
    }
    /* no clock states in PPP-RTK (absorbed by DD) */

    /* tropospheric parameters */
    if (rtk->opt.tropopt==TROPOPT_EST||rtk->opt.tropopt==TROPOPT_ESTG) {
        i=IT_RTK(&rtk->opt);
        if (i<nx) {
            p+=sprintf(p,"$TROP,%d,%.3f,%d,%d,%.4f,%.4f\n",week,tow,rtk->sol.stat,
                       1,x[i],SQRT(rtk->P[i+i*nx]));
        }
    }
    if (rtk->opt.tropopt==TROPOPT_ESTG) {
        i=IT_RTK(&rtk->opt);
        if (i+2<nx) {
            p+=sprintf(p,"$TRPG,%d,%.3f,%d,%d,%.5f,%.5f,%.5f,%.5f\n",week,tow,
                       rtk->sol.stat,1,x[i+1],x[i+2],
                       SQRT(rtk->P[(i+1)+(i+1)*nx]),SQRT(rtk->P[(i+2)+(i+2)*nx]));
        }
    }
    /* ionosphere parameters */
    if (rtk->opt.ionoopt==IONOOPT_EST||rtk->opt.ionoopt==IONOOPT_EST_ADPT) {
        for (i=0;i<MAXSAT;i++) {
            ssat=rtk->ssat+i;
            if (!ssat->vs) {
                continue;
            }
            j=II_RTK(i+1,&rtk->opt);
            if (j >= nx || rtk->x[j] == 0.0) {
                continue;
            }
            satno2id(i+1,id);
            p+=sprintf(p,"$ION,%d,%.3f,%d,%s,%.1f,%.1f,%.4f,%.4f\n",week,tow,
                       rtk->sol.stat,id,ssat->azel[0]*R2D,ssat->azel[1]*R2D,
                       x[j],SQRT(rtk->P[j+j*nx]));
        }
    }
    return (int)(p-buff);
}

/*============================================================================
 * Public API: number of estimated states
 *===========================================================================*/

/**
 * @brief Compute number of estimated states for PPP-RTK.
 * @param[in] opt Processing options
 * @return Total number of states
 */
extern int ppp_rtk_nx(const prcopt_t *opt)
{
    return NX_RTK(opt);
}

extern int ppp_rtk_na(const prcopt_t *opt)
{
    return NR_RTK(opt);
}

/*============================================================================
 * Public API: PPP-RTK positioning for one epoch
 *===========================================================================*/

/**
 * @brief PPP-RTK positioning for one epoch (CLAS).
 *
 * Main entry point for the CLAS PPP-RTK positioning engine. Uses
 * double-differencing with CLAS grid corrections (STEC, ZWD) and iterative
 * Kalman filter with LAMBDA integer ambiguity resolution.
 *
 * Simplified single-channel version (no l6mrg, no backup CSSR).
 *
 * @param[in,out] rtk  RTK control/result struct
 * @param[in]     obs  Observation data for epoch
 * @param[in]     n    Number of observations
 * @param[in,out] nav  Navigation data (includes SSR via nav->ssr_ch[][])
 */
extern void ppp_rtk_pos(rtk_t *rtk, const obsd_t *obs, int n, nav_t *nav)
{
    prcopt_t *opt = &rtk->opt;
    clas_ctx_t *clas;
    clas_grid_t *grid;
    clas_corr_t *corr;
    double *v, *H, *R, *Pp, *bias, *xp, *xa, *azel, *e;
    double *y, *rs, *dts, *var, *pbslip;
    double pos[3];
    int i, j, k, f, l, nv, nvtmp, info, nf = rtk->opt.nf, sati;
    int stat = (rtk->opt.mode <= PMODE_DGPS) ? SOLQ_DGPS : SOLQ_FLOAT;
    int vflg[MAXOBS * NFREQ * 4 + 1], nb, svh[MAXOBS];
    int nn;
    double cpc_[MAXSAT * NFREQ];
    gtime_t pt0_[MAXSAT];

    trace(NULL, 2, "ppp_rtk_pos: time=%s nx=%d n=%d\n",
          time_str(obs[0].time, 0), rtk->nx, n);
    /* check CLAS context availability */
    if (!nav->clas_ctx) {
        trace(NULL, 2, "ppp_rtk_pos: no CLAS context\n");
        rtk->sol.stat = SOLQ_NONE;
        return;
    }
    clas = (clas_ctx_t *)nav->clas_ctx;

    azel = zeros(2, n);
    e = mat(n, 3);

    nv = n * nf * 2 * 3;

    y = zeros(nv, 1);
    rs = mat(6, n);
    dts = mat(2, n);
    var = mat(1, n);

    /* initialize satellite system info */
    for (i = 0; i < MAXSAT; i++) {
        rtk->ssat[i].sys = satsys(i + 1, NULL);
        for (j = 0; j < NFREQ; j++) {
            rtk->ssat[i].vsat[j] = 0;
            rtk->ssat[i].snr[j] = 0;
        }
    }
    for (i = 0; i < MAXSAT; i++) {
        rtk->ssat[i].fix[0] = 0;
    }

    /* reset all states if time gap exceeds threshold */
    if (rtk->tt > (opt->maxobsloss_s > 0.0 ? opt->maxobsloss_s : MAXOBSLOSS_DEFAULT)) {
        for (i = 0; i < rtk->nx; i++) {
            rtk->x[i] = 0.0;
        }
    }

    /* reset filter if consecutive FLOAT epochs exceed floatcnt threshold */
    if (opt->floatcnt > 0 && prtk_ctx.float_count >= opt->floatcnt) {
        trace(NULL, 2, "ppp_rtk_pos: float persisted %d epochs, resetting filter\n",
              prtk_ctx.float_count);
        if (opt->dynamics) {
            /* kinematic: reset all states (position re-initialized from SPP) */
            for (i = 0; i < rtk->nx; i++) {
                rtk->x[i] = 0.0;
            }
        } else {
            /* static: keep position, reset iono/trop/bias */
            for (i = NP_RTK(opt); i < rtk->nx; i++) {
                rtk->x[i] = 0.0;
            }
        }
        prtk_ctx.float_count = 0;
        prtk_ctx.floatcnt_reset = 1;
    }

    /* temporal update of states */
    udstate_ppp(rtk, obs, n, nav);

    /* get grid index */
    grid = &clas->grid[0]; /* single channel, ch=0 */
    corr = &clas->current[0];
    ecef2pos(rtk->x, pos);
    if ((nn = clas_get_grid_index(clas, pos, grid, opt->gridsel, obs[0].time)) <= 0) {
        trace(NULL, 2, "ppp_rtk_pos: no valid grid\n");
        free(azel); free(e); free(y); free(rs); free(dts); free(var);
        rtk->sol.stat = SOLQ_SINGLE;
        return;
    }

    /* fetch and apply corrections for all active channels */
    {
        int nch = opt->l6mrg ? SSR_CH_NUM : 1;
        int ch;
        for (ch = 0; ch < nch; ch++) {
            /* re-fetch corrections closest to observation time,
             * falling back to L6 buffer time if obs lookup fails */
            if (grid->network > 0) {
                int bgrc=clas_bank_get_close(clas, obs[0].time,
                                        grid->network, ch,
                                        &clas->current[ch]);
                if (bgrc != 0) {
                    /* fallback: try L6 buffer time */
                    bgrc=clas_bank_get_close(clas, clas->l6buf[ch].time,
                                        grid->network, ch,
                                        &clas->current[ch]);
                }
                if (bgrc != 0) {
                    trace(NULL, 3, "ppp_rtk_pos: bank lookup failed ch=%d\n", ch);
                    continue;
                }
                clas_check_grid_status(clas, &clas->current[ch], ch);
            }
            clas_update_global(nav, &clas->current[ch], ch);
            clas_update_local(nav, &clas->current[ch], ch);
        }
        corr = &clas->current[0];

        /* dual-channel: merge freshest orbit/clock into ch=0 for satposs */
        if (opt->l6mrg) {
            int sat;
            for (sat = 0; sat < MAXSAT; sat++) {
                ssr_t *s0 = &nav->ssr_ch[0][sat];
                ssr_t *s1 = &nav->ssr_ch[1][sat];
                /* if ch1 has fresher clock correction, use it */
                if (s1->t0[1].time &&
                    (!s0->t0[1].time ||
                     timediff(s1->t0[1], s0->t0[1]) > 0)) {
                    *s0 = *s1;
                }
            }
        }
    }

    trace(NULL, 4, "x(0)=\n");
    tracemat(NULL, 4, rtk->x, 1, NR_RTK(opt), 13, 4);

    /* satellite positions and clocks (uses ssr_ch[0], merged if l6mrg) */
    set_ssr_ch_idx(0);
    satposs(obs[0].time, obs, n, nav, rtk->opt.sateph, rs, dts, var, svh);

    /* save receiver position and update SIS correction state per satellite */
    for (i = 0; i < 3; i++) {
        prtk_ctx.osr_ctx.saved_rr[i] = rtk->x[i];
    }
    for (i = 0; i < MAXSAT; i++) {
        clas_osr_satcorr_update(obs[0].time, obs[0].time, i + 1,
                                 &rtk->opt, nav, 0, &prtk_ctx.osr_ctx);
    }

    xp = mat(rtk->nx, 1);
    Pp = zeros(rtk->nx, rtk->nx);
    xa = mat(rtk->nx, 1);
    pbslip = zeros(rtk->nx, 1);

    matcpy(xp, rtk->x, rtk->nx, 1);

    v = mat(nv, 1);
    R = mat(nv, nv);
    H = mat(rtk->nx, nv);
    bias = mat(rtk->nx, 1);

    /* iterative measurement update */
    for (i = 0; i < rtk->opt.niter; i++) {
        for (k = 0; k < MAXREF; k++) {
            /* save persistent zdres state */
            for (j = 0; j < MAXSAT; j++) {
                pt0_[j] = prtk_ctx.pt0[j];
                for (f = 0; f < NFREQ; f++) {
                    cpc_[j * NFREQ + f] = prtk_ctx.cpc[j * NFREQ + f];
                }
            }

            /* compute zero-difference residuals (prefit) */
            for (j = 0; j < 3; j++) {
                pbslip[j] = xp[j];
            }
            nvtmp = clas_osr_zdres(obs, n, rs, dts, var, svh, nav,
                                   pbslip, y, e, azel, rtk, 1,
                                   &prtk_ctx.osr_ctx, grid, corr,
                                   rtk->ssat, &rtk->opt, &rtk->sol,
                                   NULL, 0);

            if (!nvtmp) {
                trace(NULL, 2, "rover initial position error\n");
                stat = SOLQ_NONE;
                break;
            }
            /* DD residuals and partial derivatives */
            if ((nv = ddres(rtk, nav, xp, pbslip, Pp, obs, y, e, azel, n,
                            v, H, R, vflg, k)) <= 0) {
                trace(NULL, 2, "no double-differenced residual\n");
                stat = SOLQ_NONE;
                break;
            }
            /* Kalman filter measurement update */
            matcpy(Pp, rtk->P, rtk->nx, rtk->nx);
            matcpy(xp, rtk->x, rtk->nx, 1);
            if ((info = filter2(rtk, xp, Pp, H, v, R, rtk->nx, nv,
                                vflg, 0)) != 0) {
                trace(NULL, 2, "ppp-rtk filter error %s info=%d\n",
                      time_str(rtk->sol.time, 0), info);
                if (i == (rtk->opt.niter - 1) && k == (MAXREF - 1)) {
                    stat = SOLQ_NONE;
                }
                continue;
            }

            /* recompute zero-difference residuals (postfit) */
            nvtmp = clas_osr_zdres(obs, n, rs, dts, var, svh, nav,
                                   xp, y, e, azel, rtk, 0,
                                   &prtk_ctx.osr_ctx, grid, corr,
                                   rtk->ssat, &rtk->opt, &rtk->sol,
                                   NULL, 0);

            if (nvtmp) {
                nv = ddres(rtk, nav, xp, NULL, Pp, obs, y, e, azel, n,
                           v, NULL, R, vflg, k);
                /* validation of float solution */
                filter2(rtk, xp, Pp, H, v, R, rtk->nx, nv, vflg, 1);
                if (prtk_ctx.chisq < 100.0) {
                    break;
                }
            }
            matcpy(xp, rtk->x, rtk->nx, 1);
            if (i == (rtk->opt.niter - 1) && k == (MAXREF - 1)) {
                stat = SOLQ_NONE;
                trace(NULL, 2, "measurement update failed\n");
            }
        }
    }

    /* SSR age (check merged ch=0) */
    rtk->sol.age = 1e4;
    for (i = 0; i < n && i < MAXOBS; i++) {
        float age;
        sati = obs[i].sat;
        age = (float)timediff(obs[i].time, nav->ssr_ch[0][sati - 1].t0[1]);
        if (rtk->sol.age > age) {
            rtk->sol.age = age;
        }
    }

    if (stat != SOLQ_NONE) {
        /* update state and covariance matrix */
        matcpy(rtk->x, xp, rtk->nx, 1);
        matcpy(rtk->P, Pp, rtk->nx, rtk->nx);

        /* adaptive process noise update for ionosphere (uses Qp = K*v*(K*v)') */
        if (rtk->opt.ionoopt == IONOOPT_EST_ADPT && prtk_ctx.Qp) {
            for (i = 0; i < n; i++) {
                j = II_RTK(obs[i].sat, &rtk->opt);
                if (j < prtk_ctx.Qp_nx) {
                    rtk->Q[j + j * rtk->nx] =
                        rtk->opt.forgetion * rtk->Q[j + j * rtk->nx] +
                        (1.0 - rtk->opt.forgetion) * SQR(rtk->opt.afgainion) *
                        prtk_ctx.Qp[j + j * prtk_ctx.Qp_nx];
                }
            }
        }
        /* adaptive process noise update for pos/vel/acc (uses Qp = K*v*(K*v)') */
        if (rtk->opt.prnadpt && prtk_ctx.Qp) {
            int np = NP_RTK(&rtk->opt);
            for (i = 0; i < np; i++) {
                for (j = 0; j < np; j++) {
                    if (i < prtk_ctx.Qp_nx && j < prtk_ctx.Qp_nx) {
                        rtk->Q[i + j * rtk->nx] =
                            rtk->opt.forgetpva * rtk->Q[i + j * rtk->nx] +
                            (1.0 - rtk->opt.forgetpva) * SQR(rtk->opt.afgainpva) *
                            prtk_ctx.Qp[i + j * prtk_ctx.Qp_nx];
                    }
                }
            }
        }

        /* update ambiguity control struct */
        rtk->sol.ns = 0;
        for (i = 0; i < n; i++) {
            sati = obs[i].sat;
            for (f = 0; f < nf; f++) {
                if (!rtk->ssat[sati - 1].vsat[f]) {
                    continue;
                }
                rtk->ssat[sati - 1].lock[f]++;
                rtk->ssat[sati - 1].outc[f] = 0;
                if (f == 0) {
                    rtk->sol.ns++;
                }
            }
        }
        trace(NULL, 5, "valid sat(L1=%d)\n", rtk->sol.ns);

        /* lack of valid satellites */
        if (rtk->sol.ns < 4) {
            trace(NULL, 2, "lack of valid satellites.\n");
            stat = SOLQ_NONE;
        }
    }

    /* keep symmetry of covariance matrix */
    for (i = 0; i < rtk->nx; i++) {
        for (j = 0; j < i; j++) {
            rtk->P[i + j * rtk->nx] = rtk->P[j + i * rtk->nx] =
                (rtk->P[i + j * rtk->nx] + rtk->P[j + i * rtk->nx]) * 0.5;
        }
    }

    /* resolve integer ambiguity by LAMBDA */
    if (stat != SOLQ_NONE) {
        /* B2: position variance gate — skip AR if filter not yet converged */
        {
            double posvar = 0.0;
            for (i = 0; i < 3; i++) {
                posvar += rtk->P[i + i * rtk->nx];
            }
            posvar /= 3.0;
            if (rtk->opt.thresar[1] > 0.0 && posvar > rtk->opt.thresar[1]) {
                trace(NULL, 3, "ppprtk: skip AR (posvar=%.4f>thresar1=%.4f)\n",
                      posvar, rtk->opt.thresar[1]);
                nb = 0; goto ppprtk_ar_done;
            }
        }
        nb = resamb_LAMBDA(rtk, bias, xa);

        /* partial AR: exclude satellites to improve fix (posopt[6]=on) */
        if (nb <= 1 && rtk->opt.posopt[6]) {
            int isat[MAXOBS], exsat = 0, sati, nf2 = NF_RTK(&rtk->opt);
            double el[MAXOBS], ratio = 0.0;
            elsort(rtk, obs, isat, el, n);
            for (l = 0; l < rtk->opt.armaxdelsat; l++) {
                ratio = 0.0;
                for (i = 0; i < n; i++) {
                    sati = isat[i];
                    if (el[i] <= rtk->opt.elmaskar) {
                        continue;
                    }
                    /* skip satellites with >1 frequency missing lock */
                    for (f = j = 0; f < nf2; f++) {
                        if (!rtk->ssat[sati - 1].vsat[f] || rtk->ssat[sati - 1].lock[f] <= 0) {
                            j++;
                        }
                    }
                    if (j > 1) {
                        continue;
                    }
                    /* temporarily exclude this satellite */
                    for (f = 0; f < nf2; f++) {
                        rtk->ssat[sati - 1].vsat[f] = 0;
                    }
                    if ((nb = resamb_LAMBDA(rtk, bias, xa)) > 1) {
                        trace(NULL, 2, "PAR OK: excluded sat=%2d el=%.1f nb=%d ratio=%.2f\n",
                              sati, el[i] * R2D, nb, rtk->sol.ratio);
                        break;
                    } else {
                        /* restore satellite and track best ratio */
                        for (f = 0; f < nf2; f++) {
                            rtk->ssat[sati - 1].vsat[f] = 1;
                        }
                        if (ratio < rtk->sol.ratio) {
                            exsat = sati;
                            ratio = rtk->sol.ratio;
                        }
                    }
                }
                if (nb > 1) {
                    break;
                }
                /* permanently exclude satellite with best ratio */
                if (exsat > 0) {
                    for (f = 0; f < nf2; f++) {
                        rtk->ssat[exsat - 1].vsat[f] = 0;
                    }
                }
            }
        }

        /* B3: arfilter — if AR still failing, exclude newly-locked sats and retry */
        if (nb <= 1 && rtk->opt.arfilter) {
            int nf2 = NF_RTK(&rtk->opt), rerun = 0, dly = 2, sat;
            for (sat = 0; sat < MAXSAT; sat++) {
                for (f = 0; f < nf2; f++) {
                    if (rtk->ssat[sat].vsat[f] && rtk->ssat[sat].lock[f] == 0) {
                        rtk->ssat[sat].lock[f] = -(rtk->opt.minlock + dly);
                        dly += 2; rerun = 1;
                    }
                }
            }
            if (rerun) {
                trace(NULL, 3, "ppprtk arfilter: rerun with newly-locked sats excluded\n");
                nb = resamb_LAMBDA(rtk, bias, xa);
            }
        }
        ppprtk_ar_done:;

        if (nb > 1) {
            /* recompute zdres with fixed solution */
            nvtmp = clas_osr_zdres(obs, n, rs, dts, var, svh, nav,
                                   xa, y, e, azel, rtk, 0,
                                   &prtk_ctx.osr_ctx, grid, corr,
                                   rtk->ssat, &rtk->opt, &rtk->sol,
                                   NULL, 0);
            if (nvtmp) {
                /* post-fix DD residuals */
                nv = ddres(rtk, nav, xa, NULL, NULL, obs, y, e, azel, n,
                           v, NULL, R, vflg, k);
                /* validation of fixed solution */
                filter2(rtk, xp, Pp, H, v, R, rtk->nx, nv, vflg, 2);

                {
                    /* chi-square thresholds from rejionno4/5 or defaults */
                    double thres_hold = rtk->opt.maxinno_ext[3] > 0.0 ?
                                        rtk->opt.maxinno_ext[3] : 0.5;
                    double thres_fix  = rtk->opt.maxinno_ext[4] > 0.0 ?
                                        rtk->opt.maxinno_ext[4] : 5.0;

                    if (prtk_ctx.chisq < thres_hold) {
                        /* hold integer ambiguity */
                        if (++rtk->nfix >= rtk->opt.minfix &&
                            rtk->opt.modear == ARMODE_FIXHOLD) {
                            holdamb(rtk, xa);
                        }
                    }
                    if (prtk_ctx.chisq < thres_fix) {
                        stat = SOLQ_FIX;
                    }
                }
            }
        }
    }

    /* reset all states due to large difference between PPP-RTK and SPP positions */
    if (opt->maxdiffp > 0.0 && norm(rtk->x, 3) > 0.0 && norm(rtk->sol.rr, 3) > 0.0) {
        double dist2 = SQR(rtk->x[0] - rtk->sol.rr[0]) +
                        SQR(rtk->x[1] - rtk->sol.rr[1]) +
                        SQR(rtk->x[2] - rtk->sol.rr[2]);
        if (dist2 > SQR(opt->maxdiffp)) {
            ++prtk_ctx.cntdiffp;
            if (prtk_ctx.cntdiffp > opt->poserrcnt) {
                trace(NULL, 2, "reset all states due to large pos error dist=%.2f\n",
                      sqrt(dist2));
                stat = SOLQ_NONE;
                for (i = 0; i < 3; i++) {
                    rtk->x[i] = rtk->sol.rr[i];
                    rtk->P[i + i * rtk->nx] = (double)rtk->sol.qr[i];
                }
                prtk_ctx.cntdiffp = 0;
            }
        } else {
            prtk_ctx.cntdiffp = 0;
        }
    }

    /* save solution status */
    if (stat == SOLQ_FIX) {
        for (i = 0; i < 3; i++) {
            rtk->sol.rr[i] = rtk->xa[i];
            rtk->sol.qr[i] = (float)rtk->Pa[i + i * rtk->na];
        }
        rtk->sol.qr[3] = (float)rtk->Pa[1];
        rtk->sol.qr[4] = (float)rtk->Pa[1 + 2 * rtk->na];
        rtk->sol.qr[5] = (float)rtk->Pa[2];
    } else {
        for (i = 0; i < 3; i++) {
            rtk->sol.rr[i] = rtk->x[i];
            rtk->sol.qr[i] = (float)rtk->P[i + i * rtk->nx];
        }
        rtk->sol.qr[3] = (float)rtk->P[1];
        rtk->sol.qr[4] = (float)rtk->P[1 + 2 * rtk->nx];
        rtk->sol.qr[5] = (float)rtk->P[2];
        rtk->nfix = 0;
    }

    /* save carrier phase observations */
    for (i = 0; i < n; i++) {
        for (j = 0; j < nf; j++) {
            if (obs[i].L[j] == 0.0) {
                continue;
            }
            rtk->ssat[obs[i].sat - 1].pt[obs[i].rcv - 1][j] = obs[i].time;
            rtk->ssat[obs[i].sat - 1].ph[obs[i].rcv - 1][j] = obs[i].L[j];
        }
    }
    /* save SNR */
    for (i = 0; i < n; i++) {
        for (j = 0; j < nf; j++) {
            rtk->ssat[obs[i].sat - 1].snr[j] = obs[i].SNR[j];
        }
    }
    /* update fix/slip status */
    for (i = 0; i < MAXSAT; i++) {
        for (j = 0; j < nf; j++) {
            if (rtk->ssat[i].fix[j] == 2 && stat != SOLQ_FIX) {
                rtk->ssat[i].fix[j] = 1;
            }
            if (rtk->ssat[i].slip[j] & 1) {
                rtk->ssat[i].slipc[j] = 1;
            }
        }
    }

    if (stat != SOLQ_NONE) {
        rtk->sol.stat = stat;
    } else {
        rtk->sol.stat = SOLQ_SINGLE;
    }

    /* update consecutive FLOAT counter for filter reset */
    if (stat == SOLQ_FIX) {
        prtk_ctx.float_count = 0;
    } else if (stat == SOLQ_FLOAT) {
        prtk_ctx.float_count++;
    }

    /* reset states on failure */
    if (stat == SOLQ_NONE) {
        if (rtk->opt.dynamics) {
            for (i = 0; i < rtk->nx; i++) {
                rtk->x[i] = 0.0;
            }
        } else {
            for (i = NP_RTK(opt); i < rtk->nx; i++) {
                rtk->x[i] = 0.0;
            }
        }
    }

    /* save persistent zdres state */
    for (j = 0; j < MAXSAT; j++) {
        prtk_ctx.pt0[j] = pt0_[j];
        for (f = 0; f < NFREQ; f++) {
            prtk_ctx.cpc[j * NFREQ + f] = cpc_[j * NFREQ + f];
        }
    }

    /* compute DOPs (sol_t has no dop[] field in MRTKLIB, skip) */

    free(azel); free(e);
    free(xp); free(Pp); free(xa); free(v); free(H); free(R); free(bias);
    free(pbslip);
    free(y); free(rs); free(dts); free(var);
}

/*============================================================================
 * Public API: initialization
 *===========================================================================*/

/**
 * @brief Initialize RTK control struct for PPP-RTK positioning.
 * @param[in,out] rtk RTK control struct
 * @param[in]     opt Processing options
 */
extern void rtkinit_ppp_rtk(rtk_t *rtk, const prcopt_t *opt)
{
    trace(NULL, 3, "rtkinit_ppp_rtk:\n");
    (void)rtk;
    (void)opt;

    /* zero out file-scope persistent state */
    memset(&prtk_ctx, 0, sizeof(prtk_ctx));
    prtk_ctx.initialized = 1;
}

/*============================================================================
 * Public API: cleanup
 *===========================================================================*/

/**
 * @brief Free RTK control struct for PPP-RTK positioning.
 * @param[in,out] rtk RTK control struct
 */
extern void rtkfree_ppp_rtk(rtk_t *rtk)
{
    trace(NULL, 3, "rtkfree_ppp_rtk:\n");
    (void)rtk;

    /* reset file-scope persistent state */
    memset(&prtk_ctx, 0, sizeof(prtk_ctx));
}
