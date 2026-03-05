/*------------------------------------------------------------------------------
 * mrtk_antenna.h : antenna model and PCV functions
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
 * @file mrtk_antenna.h
 * @brief MRTKLIB Antenna Module — Antenna phase center model and parameter I/O.
 *
 * This header provides antenna phase center variation (PCV) model
 * computation and antenna parameter file reading functions, extracted
 * from rtkcmn.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_antenna.c.
 */
#ifndef MRTK_ANTENNA_H
#define MRTK_ANTENNA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_nav.h"

/*============================================================================
 * Antenna Parameter I/O Functions
 *===========================================================================*/

/**
 * @brief Read antenna parameters from file (ANTEX or NGS format).
 * @param[in]     file  Antenna parameter file
 * @param[in,out] pcvs  Antenna parameters
 * @return Status (1:ok, 0:file open error)
 * @note File with extension .atx/.ATX is recognized as ANTEX format;
 *       other files are recognized as NGS format.
 *       Only supports non-azimuth-dependent parameters.
 */
int readpcv(const char *file, pcvs_t *pcvs);

/**
 * @brief Search antenna parameter by satellite number or antenna type.
 * @param[in] sat   Satellite number (0: receiver antenna)
 * @param[in] type  Antenna type for receiver antenna
 * @param[in] time  Time to search parameters (GPST)
 * @param[in] pcvs  Antenna parameters
 * @return Antenna parameter (NULL: no antenna found)
 */
pcv_t *searchpcv(int sat, const char *type, gtime_t time,
                 const pcvs_t *pcvs);

/*============================================================================
 * Antenna Model Functions
 *===========================================================================*/

/**
 * @brief Compute receiver antenna offset by antenna phase center parameters.
 * @param[in]  pcv   Antenna phase center parameters
 * @param[in]  del   Antenna delta {e,n,u} (m)
 * @param[in]  azel  Azimuth/elevation for receiver {az,el} (rad)
 * @param[in]  opt   Option (0:only offset, 1:offset+pcv)
 * @param[out] dant  Range offsets for each frequency (m)
 * @note Current version does not support azimuth dependent terms
 */
void antmodel(const pcv_t *pcv, const double *del, const double *azel,
              int opt, double *dant);

/**
 * @brief Compute satellite antenna phase center parameters.
 * @param[in]  pcv    Antenna phase center parameters
 * @param[in]  nadir  Nadir angle for satellite (rad)
 * @param[out] dant   Range offsets for each frequency (m)
 */
void antmodel_s(const pcv_t *pcv, double nadir, double *dant);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_ANTENNA_H */
