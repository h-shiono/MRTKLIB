/*------------------------------------------------------------------------------
 * mrtk_vrs.h : VRS-RTK positioning type definitions and functions
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
 * @file mrtk_vrs.h
 * @brief MRTKLIB VRS-RTK Module — double-differenced RTK using CLAS VRS.
 *
 * This header provides the VRS-RTK positioning function that uses
 * pre-generated virtual reference station observations with CLAS SSR
 * corrections for double-differenced RTK positioning with LAMBDA PAR.
 *
 * @note Functions declared here are implemented in mrtk_vrs.c.
 */
#ifndef MRTK_VRS_H
#define MRTK_VRS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_rtkpos.h"

/**
 * @brief VRS-RTK relative positioning.
 *
 * Performs double-differenced RTK positioning using rover observations
 * and pre-generated virtual reference station observations with CLAS
 * SSR orbit/clock corrections.
 *
 * @param[in,out] rtk  RTK control/result struct
 * @param[in]     obs  Observation data (rover + VRS base, sorted by rcv)
 * @param[in]     nu   Number of rover observations (rcv=1)
 * @param[in]     nr   Number of base observations per channel [nr[0], nr[1]]
 * @param[in,out] nav  Navigation data (SSR corrections)
 * @return 1 if solution updated, 0 otherwise
 */
extern int relposvrs(rtk_t *rtk, const obsd_t *obs, int nu, int *nr,
                     nav_t *nav);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_VRS_H */
