/*------------------------------------------------------------------------------
 * mrtk_astro.h : astronomical functions
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
 * @file mrtk_astro.h
 * @brief MRTKLIB Astronomy Module — Sun and moon position computation.
 *
 * This header provides functions for computing the positions of the
 * Sun and Moon in ECI and ECEF coordinate frames, extracted from
 * rtkcmn.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_astro.c.
 */
#ifndef MRTK_ASTRO_H
#define MRTK_ASTRO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_time.h"

/*============================================================================
 * Sun / Moon Position Functions
 *===========================================================================*/

/**
 * @brief Compute sun and moon position in ECI.
 * @param[in]  tut    Time in UT1 (GPST)
 * @param[out] rsun   Sun position in ECI (m) (NULL: not output)
 * @param[out] rmoon  Moon position in ECI (m) (NULL: not output)
 * @note See reference [4] 5.1.1, 5.2.1
 */
void sunmoonpos_eci(gtime_t tut, double *rsun, double *rmoon);

/**
 * @brief Compute sun and moon position in ECEF.
 * @param[in]  tutc   Time in UTC (GPST)
 * @param[in]  erpv   ERP value {xp,yp,ut1_utc,lod} (rad,rad,s,s/d)
 * @param[out] rsun   Sun position in ECEF (m) (NULL: not output)
 * @param[out] rmoon  Moon position in ECEF (m) (NULL: not output)
 * @param[out] gmst   GMST (rad) (NULL: not output)
 */
void sunmoonpos(gtime_t tutc, const double *erpv, double *rsun,
                double *rmoon, double *gmst);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_ASTRO_H */
