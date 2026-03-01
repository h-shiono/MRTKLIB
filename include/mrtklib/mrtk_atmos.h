/*------------------------------------------------------------------------------
 * mrtk_atmos.h : atmosphere model functions
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
 * @file mrtk_atmos.h
 * @brief MRTKLIB Atmosphere Module — Ionospheric and tropospheric models.
 *
 * This header provides broadcast ionosphere (Klobuchar) model, ionospheric
 * mapping function, ionospheric pierce point computation, tropospheric
 * (Saastamoinen) model, and tropospheric mapping function (NMF/GMF).
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from rtkcmn.c with zero algorithmic changes.
 */
#ifndef MRTK_ATMOS_H
#define MRTK_ATMOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_opt.h"

/*============================================================================
 * Ionosphere Models
 *===========================================================================*/

/**
 * @brief Compute ionospheric delay by broadcast ionosphere model (Klobuchar).
 * @param[in] t     Time (GPST)
 * @param[in] ion   Iono model parameters {a0,a1,a2,a3,b0,b1,b2,b3}
 * @param[in] pos   Receiver position {lat,lon,h} (rad,m)
 * @param[in] azel  Azimuth/elevation angle {az,el} (rad)
 * @return Ionospheric delay (L1) (m)
 */
double ionmodel(gtime_t t, const double *ion, const double *pos,
                const double *azel);

/**
 * @brief Compute ionospheric delay mapping function by single layer model.
 * @param[in] pos   Receiver position {lat,lon,h} (rad,m)
 * @param[in] azel  Azimuth/elevation angle {az,el} (rad)
 * @return Ionospheric mapping function
 */
double ionmapf(const double *pos, const double *azel);

/**
 * @brief Compute ionospheric pierce point (IPP) position and slant factor.
 * @param[in]  pos   Receiver position {lat,lon,h} (rad,m)
 * @param[in]  azel  Azimuth/elevation angle {az,el} (rad)
 * @param[in]  re    Earth radius (km)
 * @param[in]  hion  Altitude of ionosphere (km)
 * @param[out] posp  Pierce point position {lat,lon,h} (rad,m)
 * @return Slant factor
 * @note Only valid on the earth surface.
 */
double ionppp(const double *pos, const double *azel, double re, double hion,
              double *posp);

/*============================================================================
 * Troposphere Models
 *===========================================================================*/

/**
 * @brief Compute tropospheric delay by standard atmosphere and Saastamoinen.
 * @param[in] time  Time
 * @param[in] pos   Receiver position {lat,lon,h} (rad,m)
 * @param[in] azel  Azimuth/elevation angle {az,el} (rad)
 * @param[in] humi  Relative humidity
 * @return Tropospheric delay (m)
 */
double tropmodel(gtime_t time, const double *pos, const double *azel,
                 double humi);

/**
 * @brief Compute tropospheric mapping function by NMF.
 * @param[in]     time   Time
 * @param[in]     pos    Receiver position {lat,lon,h} (rad,m)
 * @param[in]     azel   Azimuth/elevation angle {az,el} (rad)
 * @param[in,out] mapfw  Wet mapping function (NULL: not output)
 * @return Dry mapping function
 * @note See ref [5] (NMF) and [9] (GMF).
 */
double tropmapf(gtime_t time, const double *pos, const double *azel,
                double *mapfw);

/*============================================================================
 * Ionosphere / Troposphere Correction (multi-model dispatch)
 *===========================================================================*/

/**
 * @brief Compute ionospheric correction.
 * @param[in]  time     Time
 * @param[in]  nav      Navigation data
 * @param[in]  sat      Satellite number
 * @param[in]  pos      Receiver position {lat,lon,h} (rad|m)
 * @param[in]  azel     Azimuth/elevation angle {az,el} (rad)
 * @param[in]  ionoopt  Ionospheric correction option (IONOOPT_???)
 * @param[out] ion      Ionospheric delay (L1) (m)
 * @param[out] var      Ionospheric delay (L1) variance (m^2)
 * @return Status (1:ok, 0:error)
 */
int ionocorr(gtime_t time, const nav_t *nav, int sat, const double *pos,
             const double *azel, int ionoopt, double *ion, double *var);

/**
 * @brief Compute tropospheric correction.
 * @param[in]  time     Time
 * @param[in]  nav      Navigation data
 * @param[in]  pos      Receiver position {lat,lon,h} (rad|m)
 * @param[in]  azel     Azimuth/elevation angle {az,el} (rad)
 * @param[in]  tropopt  Tropospheric correction option (TROPOPT_???)
 * @param[out] trp      Tropospheric delay (m)
 * @param[out] var      Tropospheric delay variance (m^2)
 * @return Status (1:ok, 0:error)
 */
int tropcorr(gtime_t time, const nav_t *nav, const double *pos,
             const double *azel, int tropopt, double *trp, double *var);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_ATMOS_H */
