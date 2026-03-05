/*------------------------------------------------------------------------------
 * mrtk_ppp_rtk.h : PPP-RTK positioning type definitions and functions
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
 * @file mrtk_ppp_rtk.h
 * @brief PPP-RTK positioning engine declarations.
 *
 * References:
 *   [1] claslib (https://github.com/mf-arai/claslib) — upstream implementation
 *       ppprtk.c: PPP-RTK positioning engine with double-differencing
 *
 * Notes:
 *   - Ported from upstream claslib ppprtk.c as an independent positioning
 *     engine alongside the existing MADOCA PPP engine (mrtk_ppp.c).
 *   - MADOCA = PPP (undifferenced), CLAS = PPP-RTK (double-differenced).
 *   - These are fundamentally different positioning methods that coexist.
 */
#ifndef MRTK_PPP_RTK_H
#define MRTK_PPP_RTK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_rtkpos.h"

/**
 * @brief Write PPP-RTK solution status to buffer.
 * @param[in,out] rtk   RTK control/result struct
 * @param[out]    buff  Output buffer
 * @return Number of bytes written
 */
int ppprtk_outstat(rtk_t *rtk, char *buff);

/**
 * @brief Compute number of estimated states for PPP-RTK.
 * @param[in] opt  Processing options
 * @return Total number of states (position + iono + trop + bias)
 */
int ppp_rtk_nx(const prcopt_t *opt);

/**
 * @brief Compute number of non-ambiguity states for PPP-RTK.
 * @param[in] opt  Processing options
 * @return Number of non-ambiguity states (position + iono + trop)
 */
int ppp_rtk_na(const prcopt_t *opt);

/**
 * @brief PPP-RTK positioning for one epoch (CLAS).
 *
 * Main entry point for the CLAS PPP-RTK positioning engine, ported from
 * upstream claslib ppprtk.c. Uses double-differencing with CLAS grid
 * corrections (STEC, ZWD) and iterative Kalman filter with LAMBDA AR.
 *
 * @param[in,out] rtk  RTK control/result struct
 * @param[in]     obs  Observation data for epoch (rover only)
 * @param[in]     n    Number of observations
 * @param[in,out] nav  Navigation data (includes SSR via nav->ssr[])
 */
void ppp_rtk_pos(rtk_t *rtk, const obsd_t *obs, int n, nav_t *nav);

/**
 * @brief Initialize RTK control struct for PPP-RTK positioning.
 * @param[in,out] rtk  RTK control struct to initialize
 * @param[in]     opt  Processing options
 */
void rtkinit_ppp_rtk(rtk_t *rtk, const prcopt_t *opt);

/**
 * @brief Free RTK control struct for PPP-RTK positioning.
 * @param[in,out] rtk  RTK control struct to free
 */
void rtkfree_ppp_rtk(rtk_t *rtk);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_PPP_RTK_H */
