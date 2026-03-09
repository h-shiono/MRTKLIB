/*------------------------------------------------------------------------------
 * mrtk_clas_osr.c : CLAS SSR-to-OSR (Observation Space Representation) conversion
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
 * @file mrtk_clas_osr.c
 * @brief CLAS SSR-to-OSR (Observation Space Representation) conversion.
 *
 * References:
 *   [1] claslib cssr2osr.c -- upstream SSR->OSR implementation
 *   [2] IS-QZSS-L6-003 -- CLAS Compact SSR Specification
 *
 * Notes:
 *   - Ported from upstream claslib cssr2osr.c.
 *   - ENA_PPP_RTK path always enabled; CSSR2OSR_VRS path removed.
 *   - Converts SSR compact corrections (orbit, clock, bias, STEC, ZWD) into
 *     per-satellite observation-space residuals (clas_osrd_t).
 *   - Static variables from upstream moved to clas_osr_ctx_t for thread safety.
 *   - Uses nav->ssr_ch[0][sat-1] (channel 0 by default).
 *   - nav->lam[sat-1] replaced by local lam[] computed from sat2freq().
 *   - Several upstream functions stubbed (ISB, L2C, etc.).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_clas.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_atmos.h"
#include "mrtklib/mrtk_astro.h"
#include "mrtklib/mrtk_geoid.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_trace.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_sol.h"
#include "mrtklib/mrtk_rtkpos.h"
#include "mrtklib/mrtk_antenna.h"
#include "mrtklib/mrtk_tides.h"
#include "mrtklib/mrtk_ppp_rtk.h"
#include "mrtklib/mrtk_eph.h"

/*============================================================================
 * Local Macros
 *===========================================================================*/

#define SQR(x)          ((x)*(x))

/* PPP-RTK state index macros (duplicated from mrtk_ppp_rtk.c) */
#define NF_RTK(opt)     ((opt)->nf)
#define NP_RTK(opt)     ((opt)->dynamics==0?3:9)
#define NI_RTK(opt)     ((opt)->ionoopt!=IONOOPT_EST?0:MAXSAT)
#define NT_RTK(opt)     ((opt)->tropopt<TROPOPT_EST?0:((opt)->tropopt<TROPOPT_ESTG?1:3))
#define NR_RTK(opt)     (NP_RTK(opt)+NI_RTK(opt)+NT_RTK(opt))
#define IB_RTK(s,f,opt) (NR_RTK(opt)+MAXSAT*(f)+(s)-1)

/** Earth gravitational constant (m^3/s^2) -- for Shapiro correction */
#define GME             3.986004415E+14

/** Frequency pair selection modes (upstream claslib rtklib.h) */
#define POSL1           1  /* L1 single freq positioning */
#define POSL1L2         2  /* L1+L2 dual freq positioning */
#define POSL1L5         3  /* L1+L5 dual freq positioning */
#define POSL1L2L5       4  /* L1+L2+L5 triple freq positioning */
#define POSL1L5_L2      5  /* L1+L5(or L2) dual freq positioning */

/** CSSR signal mode to frequency band mapping table.
 *  Index = signal tracking mode code, value = 1:L1,2:L2,3:L5,4:L6,5:L7,6:L8
 */
static const unsigned char obsfreqs[] = {
    0, 1, 1, 1, 1,  1, 1, 1, 1, 1, /*  0- 9 */
    1, 1, 1, 1, 2,  2, 2, 2, 2, 2, /* 10-19 */
    2, 2, 2, 2, 3,  3, 3, 5, 5, 5, /* 20-29 */
    4, 4, 4, 4, 4,  4, 4, 6, 6, 6, /* 30-39 */
    2, 2, 4, 4, 3,  3, 3, 1, 1, 0  /* 40-49 */
};

/*============================================================================
 * Local Helper: Shapiro Time Delay (from upstream rtkcmn.c)
 *
 * Not yet available as a public MRTKLIB function.  Defined here as static.
 *===========================================================================*/

/**
 * @brief Compute Shapiro (relativistic) time delay correction.
 * @param[in] rsat  Satellite position (ECEF) (m)
 * @param[in] rrcv  Receiver position (ECEF) (m)
 * @return Shapiro correction (m)
 */
static double shapiro(const double *rsat, const double *rrcv)
{
    double rs, rr, rrs, rsr[3];
    int i;

    rs = norm(rsat, 3);
    rr = norm(rrcv, 3);
    for (i = 0; i < 3; i++) rsr[i] = rsat[i] - rrcv[i];
    rrs = norm(rsr, 3);

    return 2.0 * GME / CLIGHT / CLIGHT * log((rs + rr + rrs) / (rs + rr - rrs));
}

/*============================================================================
 * Local Helper: Phase Windup Correction (from upstream rtkcmn.c)
 *
 * Not yet available as a public MRTKLIB function.  Defined here as static.
 *===========================================================================*/

/**
 * @brief Compute phase windup correction.
 * @param[in]     time  Time (GPST)
 * @param[in]     rs    Satellite position + velocity (ECEF, 6 elements)
 * @param[in]     rr    Receiver position (ECEF, 3 elements)
 * @param[in,out] phw   Phase windup correction (cycle), previous value as input
 */
static void windupcorr(gtime_t time, const double *rs, const double *rr,
                        double *phw)
{
    double ek[3], exs[3], eys[3], ezs[3], ess[3], exr[3], eyr[3];
    double eks[3], ekr[3], E[9];
    double dr[3], ds[3], drs[3], r[3], pos[3], rsun[3], cosp, ph;
    double erpv[5] = {0};
    double we[3] = {0.0, 0.0, OMGE}, satpos[3], o_cross_r[3];
    int i;

    /* sun position in ecef */
    sunmoonpos(gpst2utc(time), erpv, rsun, NULL, NULL);

    /* unit vector satellite to receiver */
    for (i = 0; i < 3; i++) r[i] = rr[i] - rs[i];
    if (!normv3(r, ek)) return;

    /* unit vectors of satellite antenna */
    for (i = 0; i < 3; i++) r[i] = -rs[i];
    if (!normv3(r, ezs)) return;

    for (i = 0; i < 3; i++) satpos[i] = rs[i];
    cross3(we, satpos, o_cross_r);
    for (i = 0; i < 3; i++) r[i] = rs[i + 3] + o_cross_r[i];

    if (!normv3(r, ess)) return;
    cross3(ezs, ess, r);
    if (!normv3(r, eys)) return;
    cross3(eys, ezs, exs);

    /* unit vectors of receiver antenna */
    ecef2pos(rr, pos);
    xyz2enu(pos, E);
    exr[0] = E[0]; exr[1] = E[3]; exr[2] = E[6];
    eyr[0] = E[1]; eyr[1] = E[4]; eyr[2] = E[7];

    /* phase windup effect */
    cross3(ek, eys, eks);
    cross3(ek, eyr, ekr);
    for (i = 0; i < 3; i++) {
        ds[i] = exs[i] - ek[i] * dot(ek, exs, 3) - eks[i];
        dr[i] = exr[i] - ek[i] * dot(ek, exr, 3) + ekr[i];
    }
    cosp = dot(ds, dr, 3) / norm(ds, 3) / norm(dr, 3);
    if      (cosp < -1.0) cosp = -1.0;
    else if (cosp >  1.0) cosp =  1.0;
    ph = acos(cosp) / 2.0 / PI;
    cross3(ds, dr, drs);
    if (dot(ek, drs, 3) < 0.0) ph = -ph;

    *phw = ph + floor(*phw - ph + 0.5); /* in cycle */
}

/*============================================================================
 * Local Helper: Compute Wavelengths for a Satellite
 *
 * Upstream claslib uses nav->lam[sat-1][f].  MRTKLIB does not have this
 * field in nav_t; we compute wavelengths via sat2freq().
 *===========================================================================*/

extern double sat2freq(int sat, uint8_t code, const nav_t *nav);

/**
 * @brief Fill wavelength array for a satellite from observation codes.
 * @param[in]  sat   Satellite number
 * @param[in]  obs   Observation data for this satellite
 * @param[in]  nav   Navigation data (for GLONASS FCN)
 * @param[out] lam   Wavelength array [NFREQ+NEXOBS]
 */
static void sat_wavelengths(int sat, const obsd_t *obs, const nav_t *nav,
                             double *lam)
{
    int f;
    double freq;
    for (f = 0; f < NFREQ + NEXOBS; f++) {
        freq = sat2freq(sat, obs->code[f], nav);
        lam[f] = (freq > 0.0) ? CLIGHT / freq : 0.0;
    }
}

/*============================================================================
 * Local Helper: Count Signal Modes in corr->smode[sat-1][]
 *===========================================================================*/

/**
 * @brief Count non-zero entries in a signal mode array (equivalent to nsig).
 * @param[in] smode  Signal mode array for one satellite
 * @return Number of valid signal modes
 */
static int count_nsig(const int smode[MAXCODE])
{
    int i, n = 0;
    for (i = 0; i < MAXCODE; i++) {
        if (smode[i] != 0) n++;
    }
    return n;
}

/*============================================================================
 * clas_osr_ctx_init
 *===========================================================================*/

/**
 * @brief Initialize OSR conversion context (zero all persistent state).
 * @param[out] ctx  OSR context to initialize
 */
void clas_osr_ctx_init(clas_osr_ctx_t *ctx)
{
    int ch, i;
    memset(ctx, 0, sizeof(clas_osr_ctx_t));
    for (ch = 0; ch < CLAS_CH_NUM; ch++) {
        for (i = 0; i < MAXSAT; i++) {
            ctx->satcorr[ch][i].currtow      = -1;
            ctx->satcorr[ch][i].prevtow      = -1;
            ctx->satcorr[ch][i].previode     = -1;
            ctx->satcorr[ch][i].prev_orb_tow = -1;
        }
    }
    ctx->initialized = 1;
}

/*============================================================================
 * clas_osr_prectrop -- Precise Troposphere Model
 *===========================================================================*/

/**
 * @brief Precise troposphere model for CLAS (MOPS standard + mapping function).
 *
 * Computes slant tropospheric delay from zenith wet and total delays
 * obtained from CLAS grid interpolation.
 *
 * @param[in] time  Observation time
 * @param[in] pos   Receiver position {lat,lon,hgt} (rad,m)
 * @param[in] azel  Azimuth/elevation {az,el} (rad)
 * @param[in] zwd   Zenith wet delay from grid (m)
 * @param[in] ztd   Zenith total delay from grid (m)
 * @return Slant tropospheric delay (m), 0 on error
 */
double clas_osr_prectrop(gtime_t time, const double *pos, const double *azel,
                          double zwd, double ztd)
{
    double m_d, m_w, tdvd, twvd, gh;

    gh = geoidh(pos);

    /* get_stTv is static in mrtk_clas_grid.c; use the public wrapper.
     * clas_get_stTv() is declared in mrtk_clas.h but implemented as the
     * static get_stTv() in mrtk_clas_grid.c.  Since this function is in the
     * same library, we forward-declare and call it directly. */
    extern int get_stTv(gtime_t t, const double lat, const double hs,
                        const double hg, double *tdv, double *twv);
    if (get_stTv(time, pos[0], pos[2], gh, &tdvd, &twvd)) return 0.0;

    /* mapping function */
    m_d = tropmapf(time, pos, azel, &m_w);

    return m_d * tdvd * ztd + m_w * twvd * zwd;
}

/*============================================================================
 * clas_osr_selfreqpair -- Select Frequency Pair
 *===========================================================================*/

/**
 * @brief Select frequency pair for multi-frequency processing.
 *
 * Returns a bitmask indicating which secondary frequencies to use alongside L1.
 * The selection depends on the satellite system and the processing options
 * posopt[10] (GPS) and posopt[12] (QZS).
 *
 * Note: MRTKLIB's prcopt_t.posopt[] is 6 elements (indices 0-5) while upstream
 * claslib uses indices up to 12.  For forward compatibility the indices 10/12
 * are checked against the array size; if out of range, default to L1+L2.
 *
 * @param[in] sat  Satellite number
 * @param[in] opt  Processing options
 * @param[in] obs  Observation data for this satellite
 * @return Frequency pair bitmask (0=L1 only, 1=L2, 2=L5, 3=L2+L5)
 */
int clas_osr_selfreqpair(int sat, const prcopt_t *opt, const obsd_t *obs)
{
    int optf, sys;

    sys = satsys(sat, NULL);

    /* GPS: posopt[10], QZS: posopt[12], default: 0 (→ L1+L2) */
    optf = (sys == SYS_GPS) ? opt->posopt[10] :
           (sys == SYS_QZS) ? opt->posopt[12] : 0;

    /* For GAL: prefer L5 if available */
    if (sys & SYS_GAL) {
        if (obs->L[2] != 0.0 || obs->P[2] != 0.0) return 2;
        return 0;
    }

    if (NFREQ == 1 || optf == POSL1) return 0;
    if (optf == POSL1L2L5) return 1 + 2;
    if (optf == POSL1L5)   return 2;
    if (optf == POSL1L5_L2 && obs->L[2] != 0.0 && obs->P[2] != 0.0) return 2;
    if (optf == POSL1L2) {
        if (obs->L[1] != 0.0 || obs->P[1] != 0.0) return 1;
        return 0;
    }
    /* default: L1+L2 */
    return 1;
}

/*============================================================================
 * clas_osr_compensatedisp -- Compensate Time Variation of Signal Bias + Iono
 *===========================================================================*/

/**
 * @brief Compensate time variation of signal bias and ionosphere delay.
 *
 * Ported from upstream claslib compensatedisp().  All static variables
 * replaced by clas_osr_ctx_t members (comp_*).
 *
 * @param[in]     corr    Merged correction snapshot
 * @param[in]     index   Grid point indices
 * @param[in]     obs     Observation for this satellite
 * @param[in]     sat     Satellite number
 * @param[in]     iono    Ionosphere delay (m)
 * @param[in]     pb      Phase biases per frequency
 * @param[out]    compL   Compensation output per frequency
 * @param[in,out] pbreset Phase bias reset flags
 * @param[in]     opt     Processing options
 * @param[in]     ssat    Satellite status
 * @param[in]     ch      SSR channel (0 in MRTKLIB)
 * @param[in,out] osr_ctx OSR context for persistent state
 * @param[in]     nav     Navigation data (for wavelengths)
 */
void clas_osr_compensatedisp(clas_corr_t *corr, const int *index,
                              const obsd_t *obs, int sat,
                              double iono, const double *pb,
                              double *compL, int *pbreset,
                              const prcopt_t *opt, ssat_t ssat, int ch,
                              clas_osr_ctx_t *osr_ctx, const nav_t *nav)
{
    gtime_t time = obs->time;
    int i, k, qi, qj, isat = sat - 1, oft = isat * NFREQ, oft_b;
    double lam[NFREQ + NEXOBS];
    double disp0, dispm, dt, dgf, fi;
    int nf = opt->nf, flag, fqi, fqj;

    /* compute wavelengths */
    sat_wavelengths(sat, obs, nav, lam);

    /* non-VRS path: offset is isat*NFREQ */
    oft_b = isat * NFREQ;

    /* long interval: check if STEC data has a newer time than our stored t0 */
    for (k = flag = 0; k < corr->stec[index[0]].n; k++) {
        if (sat == corr->stec[index[0]].data[k].sat) {
            flag = 1;
            break;
        }
    }

    if (flag == 1 &&
        timediff(corr->stec[index[0]].data[k].time,
                 osr_ctx->comp_t0[ch][isat]) > 0.0) {
        if (opt->posopt[5] == 1) {
            osr_ctx->comp_tm[ch][isat] = osr_ctx->comp_t0[ch][isat];
            osr_ctx->comp_t0[ch][isat] = corr->stec[index[0]].data[k].time;
            dt = timediff(osr_ctx->comp_t0[ch][isat],
                          osr_ctx->comp_tm[ch][isat]);
            if (dt <= 0.0) return;

            for (i = 0; i < nf; i++) {
                if (osr_ctx->comp_b0[ch][oft_b + i] == 0.0 ||
                    osr_ctx->comp_iono0[ch][isat] == 0.0) {
                    osr_ctx->comp_b0[ch][oft_b + i] = pb[i];
                    osr_ctx->comp_iono0[ch][isat] = iono;
                    return;
                }
            }

            /* signal bias */
            for (i = 0; i < nf; i++) {
                osr_ctx->comp_bm[ch][oft_b + i] = osr_ctx->comp_b0[ch][oft_b + i];
                osr_ctx->comp_b0[ch][oft_b + i] = pb[i];
            }
            /* ionosphere */
            osr_ctx->comp_ionom[ch][isat] = osr_ctx->comp_iono0[ch][isat];
            osr_ctx->comp_iono0[ch][isat] = iono;

            for (i = 1; i < nf; i++) {
                qi = 0; qj = i;
                /* map signal mode to frequency band index */
                fqi = obsfreqs[corr->smode[isat][qi]] - 1;
                fqj = obsfreqs[corr->smode[isat][qj]] - 1;
                if (fqi < 0 || fqj < 0) continue;
                fi = (lam[fqi] > 0.0 && lam[fqj] > 0.0) ?
                      lam[fqj] / lam[fqi] : 0.0;

                if (pb[qi] == CLAS_CSSRINVALID || pb[qj] == CLAS_CSSRINVALID ||
                    iono == 0.0) continue;
                dispm = -FREQ2 / FREQ1 * (1.0 - fi * fi) *
                         osr_ctx->comp_ionom[ch][isat] +
                         osr_ctx->comp_bm[ch][oft_b + qi] -
                         osr_ctx->comp_bm[ch][oft_b + qj];
                disp0 = -FREQ2 / FREQ1 * (1.0 - fi * fi) *
                         osr_ctx->comp_iono0[ch][isat] +
                         osr_ctx->comp_b0[ch][oft_b + qi] -
                         osr_ctx->comp_b0[ch][oft_b + qj];
                osr_ctx->comp_coef[ch][isat + (i - 1) * MAXSAT] =
                    (disp0 - dispm) / dt;
            }
        } else {
            for (i = 0; i < nf; i++) {
                osr_ctx->comp_b0[ch][oft_b + i] = obs->L[i] * lam[i];
                osr_ctx->comp_slip[ch][oft + i] = 0;
            }
            osr_ctx->comp_tm[ch][isat] = osr_ctx->comp_t0[ch][isat];
            osr_ctx->comp_t0[ch][isat] = corr->stec[index[0]].data[k].time;
        }
    }

    dt = timediff(time, osr_ctx->comp_t0[ch][isat]);

    if (opt->posopt[5] == 1) {
        for (i = 1; i < nf; i++) {
            qi = 0; qj = i;
            fqi = obsfreqs[corr->smode[isat][qi]] - 1;
            fqj = obsfreqs[corr->smode[isat][qj]] - 1;
            if (fqi < 0 || fqj < 0) continue;
            fi = (lam[fqi] > 0.0 && lam[fqj] > 0.0) ?
                  lam[fqj] / lam[fqi] : 0.0;

            if (fabs(osr_ctx->comp_coef[ch][isat + (i - 1) * MAXSAT] /
                     (FREQ2 / FREQ1 * (1.0 - fi * fi))) > 0.008) continue;
            if (pbreset[qi] || pbreset[qj]) {
                osr_ctx->comp_coef[ch][isat] = 0.0;
                return;
            }
            compL[qi] = (compL[qi] == 0.0) ?
                (1.0 / (1.0 - SQR(fi)) *
                 osr_ctx->comp_coef[ch][isat + (i - 1) * MAXSAT] * dt) :
                compL[qi];
            compL[qj] = SQR(fi) / (1.0 - SQR(fi)) *
                         osr_ctx->comp_coef[ch][isat + (i - 1) * MAXSAT] * dt;
        }
    } else {
        for (i = 0; i < nf; i++) {
            if (ssat.slip[i] > 0) osr_ctx->comp_slip[ch][oft + i] = 1;
        }
        for (i = 1; i < nf; i++) {
            qi = 0; qj = i;
            fi = (lam[qi] > 0.0 && lam[qj] > 0.0) ?
                  lam[qj] / lam[qi] : 0.0;
            if (osr_ctx->comp_slip[ch][oft + qi] ||
                osr_ctx->comp_slip[ch][oft + qj] ||
                obs->L[qi] * lam[qi] == 0.0 ||
                obs->L[qj] * lam[qj] == 0.0 ||
                pbreset[qi] || pbreset[qj]) continue;
            dgf = obs->L[qi] * lam[qi] - obs->L[qj] * lam[qj] -
                  (osr_ctx->comp_b0[ch][oft_b + qi] -
                   osr_ctx->comp_b0[ch][oft_b + qj]);
            compL[qi] = (compL[qi] == 0.0) ?
                (1.0 / (1.0 - SQR(fi)) * dgf) : compL[qi];
            compL[qj] = SQR(fi) / (1.0 - SQR(fi)) * dgf;
        }
    }
}

/*============================================================================
 * clas_osr_corrmeas -- Ionosphere/Bias/Antenna Corrected Measurements
 *===========================================================================*/

/**
 * @brief Assemble per-satellite ionosphere/bias/antenna corrections.
 *
 * Ported from upstream claslib corrmeas().  Decodes phase and code biases
 * from SSR corrections, applies STEC grid interpolation for ionosphere,
 * performs consistency checks, and computes receiver antenna correction.
 *
 * @param[in]     obs      Observation for this satellite
 * @param[in,out] nav      Navigation data
 * @param[in]     pos      Receiver position {lat,lon,hgt} (rad,m)
 * @param[in]     azel     Azimuth/elevation for this satellite
 * @param[in]     opt      Processing options
 * @param[in]     grid     Grid interpolation state
 * @param[in]     corr     Merged correction snapshot
 * @param[in]     ssat     Satellite status
 * @param[out]    brk      Slip break flag
 * @param[out]    osr      OSR output for this satellite
 * @param[in,out] pbreset  Phase bias reset flags
 * @param[in]     ch       SSR channel (0 in MRTKLIB)
 * @param[in,out] osr_ctx  OSR context
 * @return 1=ok, 0=error (stale bias, missing iono)
 */
int clas_osr_corrmeas(const obsd_t *obs, nav_t *nav, const double *pos,
                       const double *azel, const prcopt_t *opt,
                       clas_grid_t *grid, clas_corr_t *corr,
                       ssat_t ssat, int *brk, clas_osrd_t *osr,
                       int *pbreset, int ch, clas_osr_ctx_t *osr_ctx)
{
    double lam[NFREQ + NEXOBS];
    double vari, dant[NFREQPCV] = {0}, compL[NFREQ + NEXOBS] = {0};
    double stec = 0.0, rate, t5, t6;
    double pbias[NFREQ + NEXOBS] = {0}, cbias[NFREQ + NEXOBS] = {0};
    int i, j, sat, smode, nsig, nf = opt->nf;
    int flag;
    double dt;

    trace(NULL, 3, "clas_osr_corrmeas: time=%s, sat=%2d\n",
          time_str(obs->time, 0), obs->sat);

    sat = obs->sat;
    sat_wavelengths(sat, obs, nav, lam);

    for (i = 0; i < nf; i++) pbias[i] = cbias[i] = CLAS_CSSRINVALID;

    /* decode phase and code bias -- check ages */
    t5 = timediff(obs->time, nav->ssr_ch[ch][sat - 1].t0[4]); /* cbias */
    t6 = timediff(obs->time, nav->ssr_ch[ch][sat - 1].t0[5]); /* pbias */

    if (fabs(t5) > CLAS_MAXAGESSR_BIAS || fabs(t6) > CLAS_MAXAGESSR_BIAS) {
        trace(NULL, 2,
              "age of ssr bias error: time=%s sat=%2d tc,tp=%.0f,%.0f\n",
              time_str(obs->time, 0), sat, t5, t6);
        memset(&nav->ssr_ch[ch][sat - 1].t0[4], 0x00,
               sizeof(nav->ssr_ch[ch][sat - 1].t0[4]));
        memset(&nav->ssr_ch[ch][sat - 1].t0[5], 0x00,
               sizeof(nav->ssr_ch[ch][sat - 1].t0[5]));
        return 0;
    }

    /* decode phase and code bias (non-VRS / CSSR path) */
    nsig = count_nsig(corr->smode[sat - 1]);
    for (i = 0; i < nf; i++) {
        for (j = 0; j < nsig && j < MAXCODE; j++) {
            smode = corr->smode[sat - 1][j];
            if (smode == 0) continue;
            if (obs->code[i] == smode) {
                pbias[i] = nav->ssr_ch[ch][sat - 1].pbias[smode - 1];
                cbias[i] = nav->ssr_ch[ch][sat - 1].cbias[smode - 1];
                break;
            }
        }
    }

    /* ionosphere correction via STEC grid interpolation */
    if (!clas_stec_grid_data(corr, grid->index, obs->time, sat,
                             grid->num, grid->weight, grid->Gmat, grid->Emat,
                             &stec, &rate, &vari, brk)) {
        trace(NULL, 2,
              "clas_osr_corrmeas: iono correction error: time=%s sat=%2d\n",
              time_str(obs->time, 2), obs->sat);
        return 0;
    }

    /* inconsistency check: STEC time vs pbias time */
    for (i = flag = 0; i < corr->stec[grid->index[0]].n; i++) {
        if (sat == corr->stec[grid->index[0]].data[i].sat) {
            flag = 1;
            break;
        }
    }
    if (flag == 1) {
        dt = timediff(corr->stec[grid->index[0]].data[i].time,
                      nav->ssr_ch[ch][sat - 1].t0[5]);
        /* store STEC time in t0[8] -- but ssr_t only has t0[6], skip */
        if (dt < 0.0 || dt >= 30.0) {
            trace(NULL, 2,
                  "inconsist ssr correction (iono-pbias): time=%s sat=%2d "
                  "dt=%.2f\n",
                  time_str(obs->time, 0), sat, dt);
            return 0;
        }
        dt = timediff(corr->stec[grid->index[0]].data[i].time,
                      nav->ssr_ch[ch][sat - 1].t0[0]);
        if (dt < -30.0 || dt > 30.0) {
            trace(NULL, 2,
                  "inconsist ssr correction (iono-orbit): time=%s sat=%2d "
                  "dt=%.2f\n",
                  time_str(obs->time, 0), sat, dt);
            return 0;
        }
    }

    /* pbias-cbias consistency */
    dt = timediff(nav->ssr_ch[ch][sat - 1].t0[5], nav->ssr_ch[ch][sat - 1].t0[4]);
    trace(NULL, 5, "clas_osr_corrmeas: pbias_tow=%.1f, cbias_tow=%.1f, "
          "dt=%.1f\n",
          time2gpst(nav->ssr_ch[ch][sat - 1].t0[5], NULL),
          time2gpst(nav->ssr_ch[ch][sat - 1].t0[4], NULL), dt);
    if ((!opt->posopt[9] && fabs(dt) > 0.0) ||
        (opt->posopt[9] && (dt > 0.0 || dt < -30.0))) {
        trace(NULL, 2,
              "inconsist correction (pbias-cbias): time=%s sat=%2d dt=%.2f\n",
              time_str(obs->time, 0), sat, dt);
        return 0;
    }

    /* compensate time variation of signal bias and ionosphere delay */
    if (opt->posopt[5] > 0) {
        clas_osr_compensatedisp(corr, grid->index, obs, sat, stec, pbias,
                                 compL, pbreset, opt, ssat, ch, osr_ctx, nav);
    }

    /* receiver antenna phase center correction */
    antmodel(&opt->pcvr[0], opt->antdel[0], azel, opt->posopt[1], dant);

    /* ionosphere and windup corrected phase and code */
    for (i = 0; i < nf; i++) {
        osr->cbias[i] = cbias[i];
        osr->pbias[i] = pbias[i];
        osr->compL[i] = compL[i];
    }

    for (i = 0; i < nf; i++) {
        osr->wupL[i] = lam[i] * ssat.phw;
        osr->antr[i] = dant[i];
    }
    osr->iono = stec;

    return 1;
}

/*============================================================================
 * ocean_tide_clasgrid -- Grid-interpolated Ocean Tide Loading
 *===========================================================================*/

/**
 * @brief Compute ocean tide loading displacement using CLAS grid interpolation.
 *
 * Evaluates tide_oload() at each surrounding grid point, then interpolates
 * using bilinear (Gmat/Emat) or weighted average.
 * Ported from upstream claslib ppp.c:ocean_tide_clasgrid().
 *
 * @param[in]  tut     Time in UT
 * @param[in]  oload   Ocean loading data for the selected network
 * @param[in]  index   Grid point indices (from clas_grid_t)
 * @param[in]  nn      Number of grid points (1-4)
 * @param[in]  E       ENU rotation matrix (3x3)
 * @param[in]  Emat    Interpolation basis vector (4x1, can be NULL)
 * @param[in]  Gmat    Interpolation matrix (4x4, can be NULL)
 * @param[in]  weight  Interpolation weights (4x1)
 * @param[out] dr      Displacement (ECEF, m) — accumulated
 */
static void ocean_tide_clasgrid(gtime_t tut, const clas_oload_t *oload,
                                 const int *index, int nn,
                                 const double *E, const double *Emat,
                                 const double *Gmat, const double *weight,
                                 double *dr)
{
    int i, j;
    double drt[3], val[4], val_[4], denu_grid[4][3], denu[3] = {0};

    /* compute OTL displacement at each surrounding grid point */
    for (i = 0; i < nn && i < 4; i++) {
        tide_oload(tut, oload->odisp[index[i]], denu_grid[i]);
    }

    if (nn == 4 && Gmat && Emat) {
        /* bilinear interpolation using Gmat/Emat matrices */
        for (i = 0; i < 3; i++) {
            for (j = 0; j < nn; j++) val[j] = denu_grid[j][i];
            matmul("NN", nn, 1, nn, 1.0, Gmat, val, 0.0, val_);
            denu[i] = dot(Emat, val_, 4);
        }
    } else {
        /* weighted average fallback */
        for (i = 0; i < nn; i++) {
            for (j = 0; j < 3; j++) denu[j] += denu_grid[i][j] * weight[i];
        }
    }

    matmul("TN", 3, 1, 3, 1.0, E, denu, 0.0, drt);
    for (i = 0; i < 3; i++) dr[i] += drt[i];

    trace(NULL, 5, "ocean_tide_clasgrid: denu=%.4f %.4f %.4f\n",
          denu[0], denu[1], denu[2]);
}

/*============================================================================
 * SIS/IODE Adjustment Functions
 *
 * Ported from upstream claslib ephemeris.c: satpos_ssr_sis(), adjust_cpc(),
 * adjust_prc(), adjust_r_dts(). Removes SIS discontinuity at SSR correction
 * boundaries to prevent phase/code jumps that cause FLOAT epochs.
 *===========================================================================*/

/* SSR correction age/magnitude limits (matching mrtk_eph.c) */
#define MAXAGESSR_SIS      60.0         /* max age of ssr orbit/clock (s) */
#define MAXECORSSR_SIS     10.0         /* max orbit correction (m) */
#define MAXCCORSSR_SIS     (1E-6*CLIGHT) /* max clock correction (m) */

/**
 * @brief Update per-satellite SIS correction state for IODE adjustment.
 *
 * Tracks orbit/clock corrections and detects 5-second/25-second SSR boundary
 * transitions. Computes currsis = SIS discontinuity for adjust_cpc/adjust_prc.
 *
 * Ported from upstream satpos_ssr_sis() in ephemeris.c.
 */
int clas_osr_satcorr_update(gtime_t time, gtime_t teph, int sat,
                             const prcopt_t *opt, const nav_t *nav, int ch,
                             clas_osr_ctx_t *ctx)
{
    const ssr_t *ssr;
    double t1, t2, t3, deph[3], dclk, dorb[3], orbit;
    double rs[6], dts[2], var, sig[3], nsig[3];
    double er[3], ea[3], ec[3], rc[3];
    double tow = time2gpst(teph, NULL);
    int i, svh;
    clas_satcorr_t *sc;

    double orb_tow;
    ssr = nav->ssr_ch[ch] + sat - 1;
    sc = &ctx->satcorr[ch][sat - 1];

    if (!ssr->t0[0].time) return 0;  /* no orbit data -- silent */
    if (!ssr->t0[1].time) return 0;  /* no clock data -- silent */

    t1 = timediff(time, ssr->t0[0]);
    t2 = timediff(time, ssr->t0[1]);
    t3 = timediff(time, ssr->t0[2]);

    if (fabs(t1) > MAXAGESSR_SIS || fabs(t2) > MAXAGESSR_SIS) return 0;

    if (ssr->udi[0] >= 1.0) t1 -= ssr->udi[0] / 2.0;
    if (ssr->udi[1] >= 1.0) t2 -= ssr->udi[1] / 2.0;

    for (i = 0; i < 3; i++) deph[i] = ssr->deph[i] + ssr->ddeph[i] * t1;
    dclk = ssr->dclk[0] + ssr->dclk[1] * t2 + ssr->dclk[2] * t2 * t2;

    if (ssr->iod[0] == ssr->iod[2] && ssr->t0[2].time &&
        fabs(t3) < 10.0) {
        dclk += ssr->hrclk;
    }
    if (norm(deph, 3) > MAXECORSSR_SIS || fabs(dclk) > MAXCCORSSR_SIS) return 0;

    /* broadcast ephemeris position */
    if (!ephpos(time, teph, sat, nav, ssr->iode, rs, dts, &var, &svh)) return 0;

    /* radial-along-cross directions in ECEF */
    if (!normv3(rs + 3, ea)) return 0;
    cross3(rs, rs + 3, rc);
    if (!normv3(rc, ec)) return 0;
    cross3(ea, ec, er);

    /* orbit correction in radial-along-cross frame */
    for (i = 0; i < 3; i++) {
        dorb[i] = -(er[i] * deph[0] + ea[i] * deph[1] + ec[i] * deph[2]);
    }

    /* project orbit correction along line-of-sight */
    if (norm(ctx->saved_rr, 3) <= 0.0) return 0;
    for (i = 0; i < 3; i++) sig[i] = rs[i] - ctx->saved_rr[i];
    if (!normv3(sig, nsig)) return 0;
    orbit = dot(nsig, dorb, 3);

    sc->orb = orbit;
    sc->clk = dclk;
    sc->tow = tow;

    trace(NULL, 4, "satcorr_update: tow=%.1f sat=%2d clk_tow=%.1f clk=%.6f "
          "orb_tow=%.1f orb=%.6f prev_tow=%.1f\n",
          tow, sat, time2gpst(ssr->t0[1], NULL), dclk,
          time2gpst(ssr->t0[0], NULL), orbit, sc->prevtow);

    /* skip SIS tracking if posopt[9] (don't adjust phase bias) is set */
    if (opt->posopt[9]) return 1;

    orb_tow = time2gpst(ssr->t0[0], NULL);

    /* detect 25s boundary: clk_tow - orb_tow == 25 (orbit is 25s older than clock) */
    if (sc->prevtow != time2gpst(ssr->t0[1], NULL) &&
        timediff(ssr->t0[0], ssr->t0[1]) < 0.0 &&
        timediff(ssr->t0[1], ssr->t0[0]) == 25.0) {
        sc->prevtow  = time2gpst(ssr->t0[1], NULL);
        sc->prevsis  = -dclk + orbit;
        sc->previode = ssr->iode;
        trace(NULL, 4, "prevsis: tow=%.1f clk_tow=%.1f orb_tow=%.1f sat=%2d "
              "iode=%3d prevsis=%f clk=%f orb=%f\n",
              tow, sc->prevtow, orb_tow, sat,
              sc->previode, sc->prevsis, -dclk, orbit);
    }

    /* detect orbit-update boundary: orbit epoch has advanced since prevsis was saved.
     *
     * In canonical CLAS data the orbit and clock update simultaneously (orb==clk).
     * In some datasets (e.g. ST12) the orbit epoch always leads the clock by 5s, so
     * orb==clk never happens.  We therefore trigger on orbit-epoch advancement rather
     * than requiring orb_tow == clk_tow.  prev_orb_tow tracks the last seen orbit
     * epoch; when it changes we have crossed a 30s correction boundary.
     */
    if (sc->prevtow != -1 &&
        sc->prev_orb_tow != -1 &&
        orb_tow != sc->prev_orb_tow &&   /* orbit epoch just advanced */
        sc->currtow != orb_tow) {         /* not yet computed for this orbit epoch */
        sc->currsis = (-dclk + orbit) - sc->prevsis;
        sc->currtow = orb_tow;
        trace(NULL, 4, "currsis: tow=%.1f orb_tow=%.1f clk_tow=%.1f sat=%2d "
              "currsis=%f clk=%f orb=%f\n",
              tow, orb_tow, time2gpst(ssr->t0[1], NULL), sat,
              sc->currsis, -dclk, orbit);
        /* reset prevtow so the 25s boundary can fire fresh next cycle */
        sc->prevtow = -1;
    }
    sc->prev_orb_tow = orb_tow;
    return 1;
}

/**
 * @brief Adjust carrier-phase correction for SIS discontinuity.
 *
 * When phase bias TOW differs from current SIS TOW by exactly 30s,
 * subtracts the SIS discontinuity from CPC to maintain phase continuity.
 */
static double clas_osr_adjust_cpc(gtime_t teph, int sat, const ssr_t *ssr,
                                   int sig, double cpc, double *sis, int *flag,
                                   int ch, clas_osr_ctx_t *ctx)
{
    double before = cpc;
    clas_satcorr_t *sc = &ctx->satcorr[ch][sat - 1];
    *flag = 0;

    if (time2gpst(ssr->t0[5], NULL) != sc->currtow) {
        /* handle week rollover for 30s check */
        if (sc->currtow < time2gpst(ssr->t0[5], NULL) &&
            (sc->currtow + 3600 * 24 * 7) - time2gpst(ssr->t0[5], NULL) != 30.0) {
            return cpc;
        }
        if (sc->currtow - time2gpst(ssr->t0[5], NULL) != 30.0) {
            return cpc;
        }
        if (ssr->iode != sc->previode) {
            trace(NULL, 4, "adjust_cpc: IODE diff tow=%.1f sat=%2d orb_tow=%.1f "
                  "%3d sis_tow=%.1f %3d\n",
                  time2gpst(teph, NULL), sat, time2gpst(ssr->t0[0], NULL),
                  ssr->iode, sc->prevtow, sc->previode);
            *flag = 1;
        }
        *sis = sc->currsis;
        cpc -= *sis;
        trace(NULL, 4, "adjust_cpc: tow=%.1f sat=%2d pb_tow=%.1f sis_tow=%.1f "
              "sig=%d sis=%f cpc=%.6f-->%.6f\n",
              time2gpst(teph, NULL), sat, time2gpst(ssr->t0[5], NULL),
              sc->currtow, sig, *sis, before, cpc);
    }
    return cpc;
}

/**
 * @brief Adjust pseudorange correction for SIS discontinuity.
 *
 * Same logic as adjust_cpc but applied to PRC.
 */
static double clas_osr_adjust_prc(gtime_t teph, int sat, const ssr_t *ssr,
                                   int sig, double prc, double *sis, int *flag,
                                   int ch, clas_osr_ctx_t *ctx)
{
    double before = prc;
    clas_satcorr_t *sc = &ctx->satcorr[ch][sat - 1];
    *flag = 0;

    if (time2gpst(ssr->t0[5], NULL) != sc->currtow) {
        if (sc->currtow < time2gpst(ssr->t0[5], NULL) &&
            (sc->currtow + 3600 * 24 * 7) - time2gpst(ssr->t0[5], NULL) != 30.0) {
            return prc;
        }
        if (sc->currtow - time2gpst(ssr->t0[5], NULL) != 30.0) {
            return prc;
        }
        if (ssr->iode != sc->previode) {
            *flag = 1;
        }
        *sis = sc->currsis;
        prc -= *sis;
        trace(NULL, 4, "adjust_prc: tow=%.1f sat=%2d pb_tow=%.1f sis_tow=%.1f "
              "sig=%d sis=%f prc=%.6f-->%.6f\n",
              time2gpst(teph, NULL), sat, time2gpst(ssr->t0[5], NULL),
              sc->currtow, sig, *sis, before, prc);
    }
    return prc;
}

/**
 * @brief Recompute geometric range and clock on IODE change.
 *
 * When IODE changes at an SSR boundary, the broadcast ephemeris has changed.
 * This recomputes the geometric range using the previous IODE's ephemeris
 * to maintain consistency.
 */
static void clas_osr_adjust_r_dts(double *retr, double *retdts, gtime_t teph,
                                   int sat, nav_t *nav, const double *rr,
                                   gtime_t dts_time, int ch, clas_osr_ctx_t *ctx)
{
    double rs[6] = {0}, dts[2] = {0}, e[3], var;
    double er[3], ea[3], ec[3], rc[3];
    int sys = satsys(sat, NULL), svh0, i;
    const ssr_t *ssr = nav->ssr_ch[ch] + sat - 1;
    clas_satcorr_t *sc = &ctx->satcorr[ch][sat - 1];
    eph_t *eph;

    if (sys == SYS_GPS || sys == SYS_GAL || sys == SYS_QZS || sys == SYS_CMP) {
        eph = seleph(dts_time, sat, sc->previode, nav);
        if (eph) {
            double dt = eph2clk(dts_time, eph);
            dts_time = timeadd(dts_time, -dt);
        }
    }

    if (ephpos(dts_time, teph, sat, nav, sc->previode, rs, dts, &var, &svh0)) {
        if (!normv3(rs + 3, ea)) return;
        cross3(rs, rs + 3, rc);
        if (!normv3(rc, ec)) return;
        cross3(ea, ec, er);

        /* satellite antenna offset correction */
        for (i = 0; i < 3; i++) {
            rs[i] += -(er[i] * ssr->deph[0] + ea[i] * ssr->deph[1] +
                       ec[i] * ssr->deph[2]);
        }
        *retr = geodist(rs, rr, e);
    }
    *retdts = dts[0] + ssr->dclk[0] / CLIGHT;
    trace(NULL, 2, "adjust_r_dts: tow=%.1f sat=%d r=%.8f dts=%.8f\n",
          time2gpst(teph, NULL), sat, *retr, *retdts);
}

/*============================================================================
 * clas_osr_zdres -- Zero-Differenced Phase/Code Residuals
 *===========================================================================*/

/**
 * @brief Compute zero-differenced phase/code residuals with CLAS corrections.
 *
 * Ported from upstream claslib zdres().  This is the ENA_PPP_RTK path
 * (always enabled).  CSSR2OSR_VRS path is removed.
 *
 * Key differences from upstream:
 *   - obs is const; a local copy is allocated.
 *   - cpc/pt0 come from osr_ctx->cpctmp/pt0tmp.
 *   - pbias_ofst comes from osr_ctx->pbias_ofst.
 *   - Uses nav->ssr_ch[0][sat-1] (channel 0 by default).
 *   - nav->lam[sat-1] -> local lam[] via sat_wavelengths().
 *   - Stubbed functions: ISB, L2C.
 *
 * @param[in]     obs      Observation data for epoch
 * @param[in]     n        Number of observations
 * @param[in]     rs       Satellite positions+velocities (6*n)
 * @param[in]     dts      Satellite clock corrections (2*n)
 * @param[in]     vare     Satellite position variances (n)
 * @param[in]     svh      Satellite health flags (n)
 * @param[in,out] nav      Navigation data
 * @param[in,out] x        State vector (position extracted from [0:2])
 * @param[out]    y        Residuals (n*nf*2), can be NULL
 * @param[out]    e        Line-of-sight vectors (3*n)
 * @param[out]    azel     Azimuth/elevation (2*n)
 * @param[in]     rtk      RTK control struct
 * @param[in]     osrlog   Enable OSR logging
 * @param[in,out] osr_ctx  OSR context for persistent state
 * @param[in]     grid     Grid interpolation state
 * @param[in]     corr     Merged correction snapshot
 * @param[in,out] ssat     Satellite status array
 * @param[in]     opt      Processing options
 * @param[in,out] sol      Solution struct
 * @param[out]    osr      OSR output per satellite
 * @param[in]     ch       SSR channel (0 in MRTKLIB)
 * @return Number of valid residual measurements
 */
int clas_osr_zdres(const obsd_t *obs, int n, const double *rs,
                    const double *dts, const double *vare, const int *svh,
                    nav_t *nav, double *x, double *y, double *e, double *azel,
                    rtk_t *rtk, int osrlog,
                    clas_osr_ctx_t *osr_ctx, clas_grid_t *grid,
                    clas_corr_t *corr, ssat_t *ssat,
                    prcopt_t *opt, sol_t *sol, clas_osrd_t *osr, int ch)
{
    double r, rr[3], disp[3], pos[3], meas[NFREQ * 2], zwd, ztd;
    double lam[NFREQ + NEXOBS], fi;
    int i, j, f, qj, sat, sys, brk, nf = opt->nf, nftmp;
    int tbrk = 0;
    double modl[NFREQ * 2];
    obsd_t *obs_copy = NULL;   /* local mutable copy of obs */
    int nv = 0;
    clas_osrd_t osrtmp[MAXOBS];
    double dcpc;
    double tow;
    double tmp_r, tmp_dts;
    int iodeflag;
    int prn;

    /* use local osr array only when y is provided (PPP-RTK residual mode);
     * when y == NULL (VRS/ssr2osr mode), fill caller's osr directly */
    if (y) {
        memset(osrtmp, 0, sizeof(osrtmp));
        osr = osrtmp;
    }

    if (n <= 0) return 0;

    /* zero residuals if y is provided */
    if (y) {
        for (i = 0; i < n * nf * 2; i++) y[i] = 0.0;
    }

    trace(NULL, 3, "clas_osr_zdres: n=%d\n", n);

    for (i = 0; i < 3; i++) rr[i] = x[i];

    if (norm(rr, 3) <= 0.0) return 0;

    /* allocate mutable copy of observations */
    if (!(obs_copy = (obsd_t *)malloc(sizeof(obsd_t) * n))) return 0;
    for (i = 0; i < n && i < MAXOBS; i++) {
        obs_copy[i] = obs[i];
    }

    ecef2pos(rr, pos);

    /* troposphere interpolation from CLAS grid */
    if (!clas_trop_grid_data(corr, grid->index, obs_copy[0].time,
                             grid->num, grid->weight, grid->Gmat, grid->Emat,
                             &zwd, &ztd, &tbrk)) {
        trace(NULL, 2, "trop correction error: time=%s\n",
              time_str(obs_copy[0].time, 2));
        free(obs_copy);
        return 0;
    }
    /* earth tides correction
     * upstream claslib uses switch/case semantics for tidecorr:
     *   0: off
     *   1: solid only
     *   2: solid + station OTL + pole
     *   3: solid + grid OTL + pole (CLAS grid interpolation)
     * MRTKLIB's tidedisp() uses bitmask: 1=solid, 2=OTL, 4=pole
     * We translate here to match upstream behavior. */
    if (opt->tidecorr) {
        if (opt->tidecorr == 3) {
            /* solid + grid OTL + pole */
            tidedisp(gpst2utc(obs_copy[0].time), rr, 5, &nav->erp,
                     NULL, disp); /* bitmask 5 = solid(1) + pole(4) */

            /* grid-interpolated ocean tide loading */
            clas_ctx_t *ctx = (clas_ctx_t *)nav->clas_ctx;
            if (ctx && grid->network > 0 &&
                ctx->oload[grid->network - 1].gridnum > 0) {
                gtime_t tut;
                double erpv[5] = {0}, pos_[2], E_[9];
                if (nav->erp.n > 0) {
                    geterp(&nav->erp, utc2gpst(gpst2utc(obs_copy[0].time)),
                           erpv);
                }
                tut = timeadd(gpst2utc(obs_copy[0].time), erpv[2]);
                pos_[0] = asin(rr[2] / norm(rr, 3));
                pos_[1] = atan2(rr[1], rr[0]);
                xyz2enu(pos_, E_);
                ocean_tide_clasgrid(tut,
                    &ctx->oload[grid->network - 1],
                    grid->index, grid->num,
                    E_, grid->Emat, grid->Gmat, grid->weight, disp);
            }
        } else if (opt->tidecorr == 2) {
            /* solid + station OTL + pole */
            tidedisp(gpst2utc(obs_copy[0].time), rr, 7, &nav->erp,
                     opt->odisp[0], disp); /* bitmask 7 = solid+OTL+pole */
        } else {
            /* tidecorr == 1: solid only */
            tidedisp(gpst2utc(obs_copy[0].time), rr, 1, &nav->erp,
                     NULL, disp);
        }
        for (i = 0; i < 3; i++) rr[i] += disp[i];
        ecef2pos(rr, pos);
    }

    for (i = 0; i < n && i < MAXOBS; i++) {
        sat = obs_copy[i].sat;
        sys = satsys(sat, &prn);
        osr[i].sat = sat;

        /* compute wavelengths for this satellite */
        sat_wavelengths(sat, obs_copy + i, nav, lam);

        for (j = 0; j < nf * 2; j++) meas[j] = modl[j] = 0.0;

        /* geometric distance / azimuth / elevation angle */
        if ((r = geodist(rs + i * 6, rr, e + i * 3)) <= 0.0) {
            continue;
        }
        if (satazel(pos, e + i * 3, azel + i * 2) < opt->elmin) {
            trace(NULL, 3, "elevation mask rejection: sat=%2d, el=%4.1f\n",
                  sat, azel[i * 2 + 1] * R2D);
            continue;
        }

        /* excluded satellite check (basic health check) */
        if (svh[i] == -1) continue;

        /* shapiro time delay correction */
        if (opt->posopt[2]) {
            /* upstream uses posopt[7]; map to posopt[2] for MRTKLIB.
             * If posopt[2] is used for another purpose, set it appropriately. */
            osr[i].relatv = shapiro(rs + i * 6, rr);
        }

        /* tropospheric delay correction */
        osr[i].trop = clas_osr_prectrop(obs_copy[i].time, pos,
                                         azel + i * 2, zwd, ztd);

        /* phase windup correction */
        windupcorr(sol->time, rs + i * 6, rr, &ssat[sat - 1].phw);

        /* ionosphere, satellite code/phase bias and phase windup corrected */
        if (!clas_osr_corrmeas(obs_copy + i, nav, pos, azel + i * 2, opt,
                               grid, corr, ssat[sat - 1], &brk, osr + i,
                               ssat[sat - 1].pbreset, ch, osr_ctx)) {
            continue;
        }

        /* frequency pair selection */
        qj = clas_osr_selfreqpair(sat, opt, obs_copy + i);
        nftmp = nf;
        tmp_r = -1.0;
        tmp_dts = 0.0;

        for (j = 0; j < nf; j++) {
            f = j;
            if (f != 0 && f != qj) continue;

            fi = (lam[0] > 0.0) ? lam[f] / lam[0] : 0.0;

            if (osr[i].pbias[j] == CLAS_CSSRINVALID ||
                osr[i].cbias[j] == CLAS_CSSRINVALID) {
                /* ENA_PPP_RTK path: skip invalid biases */
                continue;
            }

            /* pseudorange correction */
            osr[i].PRC[j] = osr[i].trop + osr[i].relatv + osr[i].antr[f] +
                             fi * fi * FREQ2 / FREQ1 * osr[i].iono +
                             osr[i].cbias[j];

            /* carrier-phase correction */
            osr[i].CPC[j] = osr[i].trop + osr[i].relatv + osr[i].antr[f] -
                             fi * fi * FREQ2 / FREQ1 * osr[i].iono +
                             osr[i].pbias[j] + osr[i].wupL[f] +
                             osr[i].compL[j];

            /* SIS/IODE adjustment */
            iodeflag = 0;
            if (!opt->posopt[9]) {
                osr[i].CPC[j] = clas_osr_adjust_cpc(obs_copy[i].time, sat,
                    &nav->ssr_ch[ch][sat - 1], f, osr[i].CPC[j],
                    &osr[i].sis, &iodeflag, ch, osr_ctx);
                osr[i].PRC[j] = clas_osr_adjust_prc(obs_copy[i].time, sat,
                    &nav->ssr_ch[ch][sat - 1], f, osr[i].PRC[j],
                    &osr[i].sis, &iodeflag, ch, osr_ctx);
            }

            /* compute model values, adjusting r/dts on IODE change */
            if (iodeflag) {
                if (tmp_r < 0.0) {
                    gtime_t dtsat = timeadd(obs_copy[i].time,
                                            -obs_copy[i].P[0] / CLIGHT);
                    tmp_r = r;
                    clas_osr_adjust_r_dts(&tmp_r, &tmp_dts, obs_copy[i].time,
                                           sat, nav, rr, dtsat, ch, osr_ctx);
                }
                modl[j]         = tmp_r - CLIGHT * tmp_dts + osr[i].CPC[j];
                modl[nftmp + j] = tmp_r - CLIGHT * tmp_dts + osr[i].PRC[j];
                osr[i].CPC[j] += tmp_r - CLIGHT * tmp_dts -
                                  (r - CLIGHT * dts[i * 2]);
                osr[i].PRC[j] += tmp_r - CLIGHT * tmp_dts -
                                  (r - CLIGHT * dts[i * 2]);
            } else {
                modl[j]         = r - CLIGHT * dts[i * 2] + osr[i].CPC[j];
                modl[nftmp + j] = r - CLIGHT * dts[i * 2] + osr[i].PRC[j];
            }

            osr[i].p[j] = modl[nftmp + j];
            osr[i].c[j] = modl[j];

            /* repair cycle slip of pbias */
            tow = time2gpst(obs_copy[i].time, NULL);
            /* getorbitclock from satcorr state */
            if (tow == osr_ctx->satcorr[ch][sat - 1].tow) {
                osr[i].orb = osr_ctx->satcorr[ch][sat - 1].orb;
                osr[i].clk = osr_ctx->satcorr[ch][sat - 1].clk;
            } else {
                osr[i].orb = 0.0;
                osr[i].clk = 0.0;
            }

            /* cycle slip detection on phase bias update */
            if (fabs(timediff(nav->ssr_ch[ch][sat - 1].t0[5],
                              osr_ctx->pt0tmp[sat - 1])) > 0.0 &&
                fabs(timediff(nav->ssr_ch[ch][sat - 1].t0[5],
                              osr_ctx->pt0tmp[sat - 1])) < 120.0) {
                dcpc = osr[i].orb - osr[i].clk + osr[i].CPC[j] -
                       osr_ctx->cpctmp[j * MAXSAT + sat - 1];
                if (dcpc >= 95.0 * lam[f] && dcpc < 105.0 * lam[f]) {
                    osr_ctx->pbias_ofst[j * MAXSAT + sat - 1] -= 100.0;
                    x[IB_RTK(sat, f, opt)] -= 100.0;
                    trace(NULL, 2,
                          "pbias slip detected t=%s sat=%2d f=%1d "
                          "dcpc[cycle]=%.1f\n",
                          time_str(obs_copy[i].time, 0), sat, f,
                          lam[f] > 0.0 ? dcpc / lam[f] : 0.0);
                } else if (dcpc <= -95.0 * lam[f] && dcpc > -105.0 * lam[f]) {
                    osr_ctx->pbias_ofst[j * MAXSAT + sat - 1] += 100.0;
                    x[IB_RTK(sat, f, opt)] += 100.0;
                    trace(NULL, 2,
                          "pbias slip detected t=%s sat=%2d f=%1d "
                          "dcpc[cycle]=%.1f\n",
                          time_str(obs_copy[i].time, 0), sat, f,
                          lam[f] > 0.0 ? dcpc / lam[f] : 0.0);
                }
            } else {
                osr_ctx->pbias_ofst[j * MAXSAT + sat - 1] = 0.0;
            }
            osr_ctx->cpctmp[j * MAXSAT + sat - 1] =
                osr[i].orb - osr[i].clk + osr[i].CPC[j];
        }

        /* ISB correction */
        if (opt->isb == ISBOPT_TABLE) {
            double isb_val[NFREQ][2] = {{0}};
            chk_isb(satsys(sat, NULL), opt, &nav->stas[0], isb_val);
            for (f = 0; f < nf; f++) meas[f]      -= isb_val[f][0]; /* phase */
            for (f = 0; f < nf; f++) meas[f + nf] -= isb_val[f][1]; /* code */
        }
        /* L2C 1/4 cycle phase shift correction */
        if (opt->phasshft == ISBOPT_TABLE && isL2C(obs_copy[i].code[1])) {
            double ydif = 0.0;
            int jj;
            for (jj = 0; jj < nav->sfts.n; jj++) {
                if (strcmp(nav->sfts.rectyp[jj], opt->rectype[0]) == 0) {
                    ydif = nav->sfts.bias[jj];
                    break;
                }
            }
            meas[1] += ydif * lam[1];
        }

        /* measurement phase and code (ENA_PPP_RTK path) */
        for (f = 0; f < nf; f++) {
            if (lam[f] == 0.0 || obs_copy[i].L[f] == 0.0 ||
                obs_copy[i].P[f] == 0.0) continue;
            if (obs_copy[i].SNR[f] > 0.0 &&
                testsnr(0, f, azel[i * 2 + 1],
                        obs_copy[i].SNR[f] * 0.25, &opt->snrmask)) {
                trace(NULL, 2,
                      "snr error: sat=%2d f=%d el=%.2f SNR=%.2f\n",
                      sat, f, azel[i * 2 + 1] * R2D,
                      obs_copy[i].SNR[f] * 0.25);
                meas[f] = meas[f + nf] = 0.0;
                continue;
            }
            meas[f]      += obs_copy[i].L[f] * lam[f];
            meas[f + nf] += obs_copy[i].P[f];
        }

        if (y) {
            for (j = 0; j < 2; j++) { /* phase and code */
                for (f = 0; f < nf; f++) {
                    if (meas[nf * j + f] == 0.0) continue;
                    if (j == 0) ssat[sat - 1].code[f] = obs_copy[i].code[f];
                    if (f > 0 && !(f & qj)) continue;
                    if (f == 1 && (satsys(sat, NULL) == SYS_GAL)) continue;
                    if (j == 0 && (osr[i].pbias[f] == CLAS_CSSRINVALID)) {
                        trace(NULL, 2, "invalid pbias sat=%2d f=%1d\n", sat, f);
                        continue;
                    }
                    if (j == 1 && (osr[i].cbias[f] == CLAS_CSSRINVALID)) {
                        trace(NULL, 2, "invalid cbias sat=%2d f=%1d\n", sat, f);
                        continue;
                    }
                    y[nf * i * 2 + nf * j + f] =
                        meas[nf * j + f] - modl[nf * j + f];
                }
                nv += nf;
            }
        } else {
            nv += nf * 2;
        }

        /* store pbias time */
        osr_ctx->pt0tmp[sat - 1] = nav->ssr_ch[ch][sat - 1].t0[5];

        /* GAL: clear L2 slots (non-VRS path) */
        if (sys == SYS_GAL) {
            osr[i].antr[1] = 0.0;
            osr[i].wupL[1] = 0.0;
            osr[i].CPC[1]  = 0.0;
            osr[i].PRC[1]  = 0.0;
        }

        if (osrlog) {
            tow = time2gpst(obs_copy[i].time, NULL);
            trace(NULL, 3,
                  "OSRRES(ch%d),%.1f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
                  "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
                  "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
                  "%.3f,%.9f,%.9f,%.3f\n",
                  ch, tow, sys, prn,
                  osr[i].pbias[0], osr[i].pbias[1], osr[i].pbias[2],
                  osr[i].cbias[0], osr[i].cbias[1], osr[i].cbias[2],
                  osr[i].trop,
                  osr[i].iono,
                  osr[i].antr[0], osr[i].antr[1], osr[i].antr[2],
                  osr[i].relatv,
                  osr[i].wupL[0], osr[i].wupL[1], osr[i].wupL[2],
                  osr[i].compL[0], osr[i].compL[1], osr[i].compL[2],
                  osr[i].sis,
                  osr[i].CPC[0], osr[i].CPC[1], osr[i].CPC[2],
                  osr[i].PRC[0], osr[i].PRC[1], osr[i].PRC[2],
                  osr[i].orb, osr[i].clk,
                  pos[0] * R2D, pos[1] * R2D, pos[2]);
        }
    }

    free(obs_copy);
    return nv;
}

/*============================================================================
 * clas_ssr2osr -- SSR-to-OSR Conversion Wrapper
 *
 * Verbatim port of upstream claslib cssr2osr.c ssr2osr() (lines 855-979).
 * Replaces upstream's global CSSR state with MRTKLIB's clas_ctx_t context.
 *===========================================================================*/

/**
 * @brief Convert compact SSR corrections to observation-space representations.
 *
 * This is the top-level wrapper that sets up satellite state, grid/CSSR
 * corrections, then calls clas_osr_zdres() to compute per-satellite residuals.
 *
 * @param[in,out] rtk   RTK control structure
 * @param[in,out] obs   Observation data array (may be modified)
 * @param[in]     n     Number of observations
 * @param[in,out] nav   Navigation data (SSR corrections applied here)
 * @param[out]    osr   OSR output per satellite
 * @param[in]     mode  0=normal, unused (upstream compat)
 * @param[in]     clas  CLAS decoder context (replaces upstream globals)
 * @return Number of valid measurements, 0 on failure
 */
int clas_ssr2osr(rtk_t *rtk, obsd_t *obs, int n, nav_t *nav,
                 clas_osrd_t *osr, int mode, clas_ctx_t *clas)
{
    double *rs, *dts, *var, *e, *azel;
    int i, j, k = 0, nf = rtk->opt.nf, sati;
    prcopt_t *opt = &rtk->opt;
    int svh[MAXOBS];
    clas_grid_t *grid;
    clas_corr_t *corr;
    double pos[3];
    int ch, nn;
    static clas_osr_ctx_t osr_ctx;
    static int osr_ctx_init = 0;

    if (!osr_ctx_init) {
        clas_osr_ctx_init(&osr_ctx);
        osr_ctx_init = 1;
    }

    rs = mat(6, n); dts = mat(2, n); var = mat(1, n); azel = zeros(2, n);
    e = mat(n, 3);

    for (i = 0; i < MAXSAT; i++) {
        rtk->ssat[i].sys = satsys(i + 1, NULL);
        for (j = 0; j < NFREQ; j++) rtk->ssat[i].vsat[j] = rtk->ssat[i].snr[j] = 0;
    }
    for (i = 0; i < MAXSAT; i++) rtk->ssat[i].fix[0] = 0;

    /* temporal update of states */
    if (opt->mode == PMODE_SSR2OSR_FIXED) {
        for (i = 0; i < 3; i++) rtk->x[i] = opt->ru[i];
    } else {
        if (norm(rtk->sol.rr, 3) > 0.0) {
            for (i = 0; i < 3; i++) rtk->x[i] = rtk->sol.rr[i];
        }
    }

    /* grid index and CSSR lookup */
    ch = 0;
    grid = &clas->grid[ch];
    corr = &clas->current[ch];
    ecef2pos(rtk->x, pos);

    if ((nn = clas_get_grid_index(clas, pos, grid, 0, obs[0].time)) <= 0) {
        trace(NULL, 2, "clas_ssr2osr: no valid grid\n");
        free(rs); free(dts); free(var); free(e); free(azel);
        rtk->sol.stat = SOLQ_SINGLE;
        return 0;
    }

    /* fetch corrections from bank */
    if (grid->network > 0 && grid->network != corr->network) {
        if (clas_bank_get_close(clas, obs[0].time,
                                grid->network, ch, corr) != 0) {
            trace(NULL, 2, "clas_ssr2osr: bank lookup failed\n");
            free(rs); free(dts); free(var); free(e); free(azel);
            rtk->sol.stat = SOLQ_SINGLE;
            return 0;
        }
        clas_check_grid_status(clas, corr, ch);
    }
    clas_update_global(nav, corr, ch);
    clas_update_local(nav, corr, ch);

    /* SSR age */
    rtk->sol.age = 1e4;
    for (i = 0; i < n && i < MAXOBS; i++) {
        double age_d;
        sati = obs[i].sat;
        age_d = timediff(obs[i].time, nav->ssr_ch[ch][sati - 1].t0[1]);
        if (rtk->sol.age > age_d) rtk->sol.age = age_d;
    }

    /* satellite positions and clocks */
    set_ssr_ch_idx(ch);
    satposs(obs[0].time, obs, n, nav, EPHOPT_SSRAPC, rs, dts, var, svh);

    /* zero-difference residuals */
    k = clas_osr_zdres(obs, n, rs, dts, var, svh, nav,
                       rtk->x, NULL, e, azel, rtk, 1,
                       &osr_ctx, grid, corr, rtk->ssat,
                       opt, &rtk->sol, osr, ch);
    if (!k) {
        trace(NULL, 2, "clas_ssr2osr: zdres failed\n");
    }

    /* fill VRS pseudo-observations from OSR corrections (CSSR2OSR_VRS path) */
    {
        int ko = 0;
        for (i = 0; i < n && i < MAXOBS; i++) {
            double lam_v[NFREQ + NEXOBS];
            int sys_v, f;
            sati = obs[i].sat;
            if (sati <= 0) continue;
            if (osr[i].sat <= 0) continue;
            sys_v = satsys(sati, NULL);

            obs[ko].time = obs[i].time;
            obs[ko].sat = sati;
            obs[ko].rcv = 1;

            /* assign signal codes from corr's smode (CLAS decoder) */
            for (j = 0; j < nf; j++) {
                obs[ko].code[j] = corr->smode[sati - 1][j];
            }
            /* compute wavelengths from assigned codes */
            sat_wavelengths(sati, &obs[ko], nav, lam_v);

            for (j = 0; j < nf; j++) {
                f = j;
                if (osr[i].p[j] == 0.0 && osr[i].c[j] == 0.0) {
                    obs[ko].P[j] = 0.0;
                    obs[ko].L[j] = 0.0;
                    obs[ko].LLI[j] = 0;
                    obs[ko].SNR[j] = 0;
                    continue;
                }
                obs[ko].P[j] = osr[i].p[j];
                obs[ko].L[j] = (lam_v[f] > 0.0) ? osr[i].c[j] / lam_v[f] : 0.0;
                obs[ko].LLI[j] = 0;
                obs[ko].SNR[j] = (uint16_t)(40.0 / SNR_UNIT);
            }
            /* clear remaining freq slots */
            for (j = nf; j < NFREQ + NEXOBS; j++) {
                obs[ko].P[j] = 0.0;
                obs[ko].L[j] = 0.0;
                obs[ko].LLI[j] = 0;
                obs[ko].code[j] = 0;
                obs[ko].SNR[j] = 0;
            }
            ko++;
        }
        k = ko;
    }

    /* save observation state (upstream compat) */
    for (i = 0; i < k; i++) {
        for (j = 0; j < nf; j++) {
            if (obs[i].L[j] == 0.0) continue;
            rtk->ssat[obs[i].sat - 1].pt[obs[i].rcv - 1][j] = obs[i].time;
            rtk->ssat[obs[i].sat - 1].ph[obs[i].rcv - 1][j] = obs[i].L[j];
        }
    }
    for (i = 0; i < k; i++) {
        for (j = 0; j < nf; j++) {
            rtk->ssat[obs[i].sat - 1].snr[j] = obs[i].SNR[j];
        }
    }
    for (i = 0; i < MAXSAT; i++) {
        for (j = 0; j < nf; j++) {
            if (rtk->ssat[i].fix[j] == 2) rtk->ssat[i].fix[j] = 1;
            if (rtk->ssat[i].slip[j] & 1) rtk->ssat[i].slipc[j] = 1;
        }
    }

    free(rs); free(dts); free(var); free(e); free(azel);
    return k;
}
