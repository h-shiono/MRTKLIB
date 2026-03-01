/*------------------------------------------------------------------------------
 * mrtk_rtkpos.h : RTK positioning type definitions and functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_rtkpos.h
 * @brief MRTKLIB RTK Positioning Module — relative and precise positioning.
 *
 * This header provides the RTK control structure (rtk_t) and functions for
 * relative positioning (DGPS/RTK) and precise point positioning (PPP).
 *
 * @note Functions declared here are implemented in mrtk_rtkpos.c.
 */
#ifndef MRTK_RTKPOS_H
#define MRTK_RTKPOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_sol.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"

typedef struct {        /* ambiguity control type */
    gtime_t epoch[4];   /* last epoch */
    int n[4];           /* number of epochs */
    double LC [4];      /* linear combination average */
    double LCv[4];      /* linear combination variance */
    int fixcnt;         /* fix count */
    char flags[MAXSAT]; /* fix flags */
} ambc_t;

typedef struct {        /* RTK control/result type */
    sol_t  sol;         /* RTK solution */
    double rb[6];       /* base position/velocity (ecef) (m|m/s) */
    int nx,na;          /* number of float states/fixed states */
    double tt;          /* time difference between current and previous (s) */
    double *x, *P;      /* float states and their covariance */
    double *xa,*Pa;     /* fixed states and their covariance */
    int nfix;           /* number of continuous fixes of ambiguity */
    ambc_t ambc[MAXSAT]; /* ambibuity control */
    ssat_t ssat[MAXSAT]; /* satellite status */
    int neb;            /* bytes in error message buffer */
    char errbuf[MAXERRMSG]; /* error message buffer */
    prcopt_t opt;       /* processing options */
    int miono_info[2];  /* MADOCA-PPP iono info (0:region id, 1:area No.) */
    double prev_qr[6];  /* previous position variance/covariance (m^2) */
} rtk_t;

/**
 * @brief Initialize RTK control struct.
 * @param[in,out] rtk  RTK control/result struct
 * @param[in]     opt  Positioning options
 */
void rtkinit(rtk_t *rtk, const prcopt_t *opt);

/**
 * @brief Free memory for RTK control struct.
 * @param[in,out] rtk  RTK control/result struct
 */
void rtkfree(rtk_t *rtk);

/**
 * @brief Compute rover position by precise positioning.
 * @param[in,out] rtk  RTK control/result struct
 * @param[in]     obs  Observation data for an epoch
 * @param[in]     nobs Number of observation data
 * @param[in,out] nav  Navigation messages
 * @return Status (0: no solution, 1: valid solution)
 */
int rtkpos(mrtk_ctx_t *ctx, rtk_t *rtk, const obsd_t *obs, int nobs,
           nav_t *nav);

/**
 * @brief Open solution status file and set output level.
 * @param[in] file  RTK status file path
 * @param[in] level RTK status level (0: off)
 * @return Status (1: ok, 0: error)
 */
int rtkopenstat(const char *file, int level);

/**
 * @brief Close solution status file.
 */
void rtkclosestat(void);

/**
 * @brief Write solution status to buffer.
 * @param[in,out] rtk  RTK control/result struct
 * @param[out]    buff Output buffer
 * @return Number of bytes written
 */
int rtkoutstat(rtk_t *rtk, char *buff);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_RTKPOS_H */
