/*------------------------------------------------------------------------------
 * mrtk_opt.c : processing and solution option defaults
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
 * @file mrtk_opt.c
 * @brief MRTKLIB Options Module — Default processing and solution options.
 */
#include "mrtklib/mrtk_opt.h"

#include <stdio.h>

/*--- local constants (duplicated to avoid rtklib.h dependency) -------------*/
static const double D2R = 3.1415926535897932 / 180.0;

#define SYS_GPS 0x01

/*--- default option instances -----------------------------------------------*/

const prcopt_t prcopt_default = {
    /* defaults processing options */
    PMODE_SINGLE,
    0,
    2,
    SYS_GPS, /* mode,soltype,nf,navsys */
    15.0 * D2R,
    {{0, 0}}, /* elmin,snrmask */
    0,
    1,
    0,
    0,       /* sateph,modear,glomodear,bdsmodear */
    SYS_GPS, /* arsys */
    5,
    0,
    10,
    1, /* maxout,minlock,minfix,armaxiter */
    0,
    0,
    0,
    0, /* estion,esttrop,dynamics,tidecorr */
    1,
    0,
    0,
    0,
    0, /* niter,codesmooth,intpref,sbascorr,sbassatsel */
    0,
    0,                                   /* rovpos,refpos */
    {100.0, 100.0},                      /* eratio[] */
    {100.0, 0.003, 0.003, 0.0, 1.0},     /* err[] */
    {30.0, 0.03, 0.3},                   /* std[] */
    {1E-4, 1E-3, 1E-4, 1E-1, 1E-2, 0.0}, /* prn[] */
    5E-12,                               /* sclkstab */
    {3.0, 0.9999, 0.25, 0.1, 0.05},      /* thresar */
    0.0,
    0.0,
    0.05,
    0.0, /* elmaskar,almaskhold,thresslip,thresdop */
    30.0,
    30.0,
    30.0, /* maxtdif,maxinno,maxgdop */
    {0},
    {0},
    {0},      /* baseline,ru,rb */
    {"", ""}, /* anttype */
    {{0}},
    {{0}},
    {0} /* antdel,pcv,exsats */
    ,
    0,
    0 /* ign_chierr,bds2bias */
    ,
    0,
    0,
    0,
    0, /* pppsatcb,pppsatpb,unbias,maxbiasdt */
    .correction = CORR_AUTO,
    .enhanced_spp_seed = SEEDENH_BASE, /* default on: C/N0 + TDCP a-priori seed (PPP-RTK/VRS) */
    .rejethres = 0.0,                  /* standalone SPP off; CLAS seed applies upstream auto-default */
    .rejeminsat = 7,
};
const solopt_t solopt_default = {
    /* defaults solution output options */
    SOLF_LLH,   TIMES_GPST, 1, 3,          /* posf,times,timef,timeu */
    0,          1,          0, 0, 0, 0, 0, /* degf,outhead,outopt,outvel,datum,height,geoid */
    0,          0,          0,             /* solstatic,sstat,trace */
    {0.0, 0.0},                            /* nmeaintv */
    " ",        ""                         /* separator/program name */
};

/* resolve and validate correction source ------------------------------------
 * Resolve CORR_AUTO from mode + sateph (backward compatibility for configs that
 * omit `correction`), reject reserved-but-unimplemented sources, and enforce the
 * (mode, correction) validity matrix. See docs/design/configuration.md.
 * args   : prcopt_t *opt   IO  processing options (opt->correction resolved in place)
 *          char     *msg   O   error text on failure (caller buffer)
 *          size_t    msgsz I   size of msg buffer
 * return : status (1:ok, 0:invalid combination -> caller should abort)
 *--------------------------------------------------------------------------*/
/* human-readable names for resolve_correction() error messages -------------*/
static const char* corr_name(int corr) {
    switch (corr) {
        case CORR_AUTO:
            return "auto";
        case CORR_NONE:
            return "none";
        case CORR_IGS:
            return "igs";
        case CORR_IGS_RTS:
            return "igs-rts";
        case CORR_QZS_MADOCA:
            return "qzs-madoca";
        case CORR_GAL_HAS:
            return "gal-has";
        case CORR_BDS_B2B:
            return "bds-b2b";
        case CORR_QZS_CLAS:
            return "qzs-clas";
        default:
            return "?";
    }
}
static const char* mode_name(int mode) {
    switch (mode) {
        case PMODE_SINGLE:
            return "single";
        case PMODE_DGPS:
            return "dgps";
        case PMODE_KINEMA:
            return "kinematic";
        case PMODE_STATIC:
            return "static";
        case PMODE_MOVEB:
            return "movingbase";
        case PMODE_FIXED:
            return "fixed";
        case PMODE_PPP_KINEMA:
            return "ppp-kine";
        case PMODE_PPP_STATIC:
            return "ppp-static";
        case PMODE_PPP_FIXED:
            return "ppp-fixed";
        case PMODE_PPP_RTK:
            return "ppp-rtk";
        case PMODE_SSR2OSR:
            return "ssr2osr";
        case PMODE_SSR2OSR_FIXED:
            return "ssr2osr-fixed";
        case PMODE_VRS_RTK:
            return "vrs-rtk";
        default:
            return "?";
    }
}
extern int resolve_correction(prcopt_t* opt, char* msg, size_t msgsz) {
    int m = opt->mode;

    /* infer when not explicitly configured (correction omitted -> CORR_AUTO) */
    if (opt->correction == CORR_AUTO) {
        if (m == PMODE_PPP_RTK || m == PMODE_VRS_RTK) {
            opt->correction = CORR_QZS_CLAS;
        } else if (m == PMODE_PPP_KINEMA || m == PMODE_PPP_STATIC || m == PMODE_PPP_FIXED) {
            /* igs-rts is never inferred: it shares sateph=brdc+ssrapc with
             * qzs-madoca, so the two are indistinguishable here. Require an
             * explicit `correction = "igs-rts"` to take the RTCM-SSR path. */
            opt->correction = (opt->sateph == EPHOPT_PREC) ? CORR_IGS : CORR_QZS_MADOCA;
        } else {
            opt->correction = CORR_NONE;
        }
    }

    /* reserved sources: present in the schema but not implemented yet.
     * igs-rts is NOT reserved any more (#138): the RTCM-SSR / IGS-SSR(MT4076)
     * decode-and-apply pipeline is inherited from the MADOCA-via-RTCM path and
     * runs through the same SSR branch of corr_meas. gal-has is NOT reserved any
     * more: HAS MT1 corrections decode into nav->ssr_ch[0] and run through the
     * same SSR branch of corr_meas (float PPP). */
    if (opt->correction == CORR_BDS_B2B) {
        if (msg) {
            snprintf(msg, msgsz, "correction source not implemented yet (correction=%s)", corr_name(opt->correction));
        }
        return 0;
    }

    /* validity matrix (see docs/design/configuration.md) */
    switch (m) {
        case PMODE_PPP_KINEMA:
        case PMODE_PPP_STATIC:
        case PMODE_PPP_FIXED:
            if (opt->correction != CORR_IGS && opt->correction != CORR_QZS_MADOCA && opt->correction != CORR_IGS_RTS &&
                opt->correction != CORR_GAL_HAS) {
                if (msg) {
                    snprintf(msg, msgsz, "invalid correction=%s for mode=%s (use igs, igs-rts, qzs-madoca or gal-has)",
                             corr_name(opt->correction), mode_name(m));
                }
                return 0;
            }
            /* igs-rts and gal-has apply broadcast ephemeris + SSR corrections; they
             * need an SSR-aware satellite-ephemeris option (brdc+ssrapc or
             * brdc+ssrcom). */
            if ((opt->correction == CORR_IGS_RTS || opt->correction == CORR_GAL_HAS) && opt->sateph != EPHOPT_SSRAPC &&
                opt->sateph != EPHOPT_SSRCOM) {
                if (msg) {
                    snprintf(msg, msgsz,
                             "correction=%s requires satellite_ephemeris=brdc+ssrapc or brdc+ssrcom (got %d)",
                             corr_name(opt->correction), opt->sateph);
                }
                return 0;
            }
            /* #142: integer PPP-AR for IGS precise products needs the uncombined
             * measurement model. ppp_amb_ILS resolves per-frequency ambiguity
             * states, which only exist when ionosphere=est-stec; with the
             * iono-free combination there is one ambiguity per satellite and
             * ppp_ar() is a no-op. Flag the misconfiguration rather than silently
             * returning a float-only solution. */
            if (opt->correction == CORR_IGS && opt->modear >= ARMODE_CONT && opt->ionoopt != IONOOPT_EST) {
                if (msg) {
                    snprintf(msg, msgsz,
                             "correction=igs with ambiguity_resolution requires ionosphere=est-stec (uncombined)");
                }
                return 0;
            }
            break;
        case PMODE_PPP_RTK:
        case PMODE_VRS_RTK:
            if (opt->correction != CORR_QZS_CLAS) {
                if (msg) {
                    snprintf(msg, msgsz, "invalid correction=%s for mode=%s (use qzs-clas)", corr_name(opt->correction),
                             mode_name(m));
                }
                return 0;
            }
            break;
        default:
            /* single / dgps / rtk-relative / ssr2osr: no augmentation source allowed */
            if (opt->correction != CORR_NONE) {
                if (msg) {
                    snprintf(msg, msgsz, "invalid correction=%s for mode=%s (use none)", corr_name(opt->correction),
                             mode_name(m));
                }
                return 0;
            }
            break;
    }
    return 1;
}
