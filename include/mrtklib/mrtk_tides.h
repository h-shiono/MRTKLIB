/*------------------------------------------------------------------------------
 * mrtk_tides.h : tide displacement correction functions
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
 * @file mrtk_tides.h
 * @brief MRTKLIB Tides Module — Tidal displacement corrections.
 *
 * This header declares the public tidal displacement function (tidedisp)
 * extracted from tides.c.
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from tides.c with zero algorithmic changes.
 */
#ifndef MRTK_TIDES_H
#define MRTK_TIDES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_peph.h"

/*============================================================================
 * Tidal Displacement Functions
 *===========================================================================*/

/**
 * @brief Compute displacements by earth tides.
 * @param[in]  tutc   Time in UTC
 * @param[in]  rr     Site position (ECEF) (m)
 * @param[in]  opt    Options (bitwise OR):
 *                      1: solid earth tide
 *                      2: ocean tide loading
 *                      4: pole tide
 *                      8: eliminate permanent deformation
 * @param[in]  erp    Earth rotation parameters (NULL: not used)
 * @param[in]  odisp  Ocean loading parameters (NULL: not used)
 *                      odisp[0+i*6]: constituent i amplitude radial (m)
 *                      odisp[1+i*6]: constituent i amplitude west   (m)
 *                      odisp[2+i*6]: constituent i amplitude south  (m)
 *                      odisp[3+i*6]: constituent i phase radial  (deg)
 *                      odisp[4+i*6]: constituent i phase west    (deg)
 *                      odisp[5+i*6]: constituent i phase south   (deg)
 *                     (i=0:M2,1:S2,2:N2,3:K2,4:K1,5:O1,6:P1,7:Q1,
 *                        8:Mf,9:Mm,10:Ssa)
 * @param[out] dr     Displacement by earth tides (ECEF) (m)
 */
void tidedisp(gtime_t tutc, const double *rr, int opt, const erp_t *erp,
              const double *odisp, double *dr);

/**
 * @brief Compute displacement by ocean tide loading (11 constituents).
 * @param[in]  tut    Time in UT
 * @param[in]  odisp  Ocean tide loading parameters (6*11 values)
 * @param[out] denu   Displacement in ENU (m)
 */
void tide_oload(gtime_t tut, const double *odisp, double *denu);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_TIDES_H */
