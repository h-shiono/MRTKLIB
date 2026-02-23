/*------------------------------------------------------------------------------
 * mrtk_ppp_ar.h : PPP ambiguity resolution functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_ppp_ar.h
 * @brief MRTKLIB PPP Ambiguity Resolution Module.
 *
 * This header provides the PPP ambiguity resolution function using
 * integer least-squares estimation (ILS) with the LAMBDA/MLAMBDA method.
 *
 * @note Functions declared here are implemented in mrtk_ppp_ar.c.
 */
#ifndef MRTK_PPP_AR_H
#define MRTK_PPP_AR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_rtkpos.h"

/**
 * @brief Ambiguity resolution in PPP.
 * @param[in]     ctx   Runtime context (NULL = use global)
 * @param[in,out] rtk   RTK control/result struct
 * @param[in]     obs   Observation data
 * @param[in]     n     Number of observation data
 * @param[in,out] exc   Satellite exclusion flags
 * @param[in]     nav   Navigation data
 * @param[in]     azel  Azimuth/elevation angles (rad)
 * @param[in,out] x     Float states
 * @param[in,out] P     Float covariance
 * @return Status (0: no resolution, 1: success)
 */
int ppp_ar(mrtk_ctx_t *ctx, rtk_t *rtk, const obsd_t *obs, int n, int *exc,
           const nav_t *nav, const double *azel, double *x, double *P);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_PPP_AR_H */
