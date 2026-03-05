/*------------------------------------------------------------------------------
 * mrtk_spp.h : standard single-point positioning functions
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
 * @file mrtk_spp.h
 * @brief MRTKLIB Single-Point Positioning Module — SPP functions.
 *
 * This header provides the standard single-point positioning function
 * (pntpos), extracted from pntpos.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_spp.c.
 * @note ionocorr/tropcorr have been moved to mrtk_atmos.h / mrtk_atmos.c.
 */
#ifndef MRTK_SPP_H
#define MRTK_SPP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_sol.h"

/*============================================================================
 * Single-Point Positioning Functions
 *===========================================================================*/

/**
 * @brief Compute receiver position, velocity, clock bias by single-point
 *        positioning with pseudorange and doppler observables.
 * @param[in]    ctx   Runtime context (NULL = use global)
 * @param[in]    obs   Observation data
 * @param[in]    n     Number of observation data
 * @param[in]    nav   Navigation data
 * @param[in]    opt   Processing options
 * @param[in,out] sol  Solution
 * @param[in,out] azel Azimuth/elevation angle (rad) (NULL: no output)
 * @param[in,out] ssat Satellite status (NULL: no output)
 * @param[out]   msg   Error message for error exit
 * @return Status (1:ok, 0:error)
 */
int pntpos(mrtk_ctx_t *ctx, const obsd_t *obs, int n, const nav_t *nav,
           const prcopt_t *opt, sol_t *sol, double *azel, ssat_t *ssat,
           char *msg);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_SPP_H */
