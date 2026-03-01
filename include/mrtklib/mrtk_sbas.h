/*------------------------------------------------------------------------------
 * mrtk_sbas.h : SBAS message decoding and correction functions
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
 * @file mrtk_sbas.h
 * @brief MRTKLIB SBAS Module — SBAS message decoding and corrections.
 *
 * This header declares all public SBAS functions extracted from sbas.c.
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from sbas.c with zero algorithmic changes.
 */
#ifndef MRTK_SBAS_H
#define MRTK_SBAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"

/*============================================================================
 * SBAS Global Variables
 *===========================================================================*/

extern const sbsigpband_t igpband1[9][8]; /* SBAS IGP band 0-8 */
extern const sbsigpband_t igpband2[2][5]; /* SBAS IGP band 9-10 */

/*============================================================================
 * SBAS Functions
 *===========================================================================*/

/**
 * @brief Read SBAS message file.
 * @param[in]     file  SBAS message file path
 * @param[in]     sel   SBAS satellite selection (0:all)
 * @param[in,out] sbs   SBAS messages
 * @return Status (1:ok, 0:error)
 */
int sbsreadmsg(const char *file, int sel, sbs_t *sbs);

/**
 * @brief Read SBAS message file with time span.
 * @param[in]     file  SBAS message file path
 * @param[in]     sel   SBAS satellite selection (0:all)
 * @param[in]     ts    Time start (ts.time==0: no limit)
 * @param[in]     te    Time end   (te.time==0: no limit)
 * @param[in,out] sbs   SBAS messages
 * @return Status (1:ok, 0:error)
 */
int sbsreadmsgt(const char *file, int sel, gtime_t ts, gtime_t te,
                sbs_t *sbs);

/**
 * @brief Output SBAS message record.
 * @param[in] fp      Output file pointer
 * @param[in] sbsmsg  SBAS message
 */
void sbsoutmsg(FILE *fp, sbsmsg_t *sbsmsg);

/**
 * @brief Decode SBAS message frame words.
 * @param[in]  time    Reception time
 * @param[in]  prn     SBAS satellite PRN number
 * @param[in]  words   SBAS message frame words (9 words)
 * @param[out] sbsmsg  SBAS message
 * @return Status (1:ok, 0:error)
 */
int sbsdecodemsg(gtime_t time, int prn, const uint32_t *words,
                 sbsmsg_t *sbsmsg);

/**
 * @brief Update SBAS corrections in navigation data.
 * @param[in]     msg  SBAS message
 * @param[in,out] nav  Navigation data
 * @return Message type (0:error)
 */
int sbsupdatecorr(const sbsmsg_t *msg, nav_t *nav);

/**
 * @brief SBAS satellite ephemeris and clock correction.
 * @param[in]     time  Reception time
 * @param[in]     sat   Satellite number
 * @param[in]     nav   Navigation data
 * @param[in,out] rs    Satellite position/velocity (corrected)
 * @param[out]    dts   Satellite clock correction
 * @param[out]    var   Variance of correction
 * @return Status (1:ok, 0:error)
 */
int sbssatcorr(gtime_t time, int sat, const nav_t *nav, double *rs,
               double *dts, double *var);

/**
 * @brief SBAS ionospheric delay correction.
 * @param[in]  time   Reception time
 * @param[in]  nav    Navigation data
 * @param[in]  pos    Receiver position {lat, lon, h} (rad, m)
 * @param[in]  azel   Azimuth/elevation angle {az, el} (rad)
 * @param[out] delay  Ionospheric delay (L1) (m)
 * @param[out] var    Variance of ionospheric delay (m^2)
 * @return Status (1:ok, 0:error)
 */
int sbsioncorr(gtime_t time, const nav_t *nav, const double *pos,
               const double *azel, double *delay, double *var);

/**
 * @brief SBAS tropospheric delay correction.
 * @param[in]  time  Reception time
 * @param[in]  pos   Receiver position {lat, lon, h} (rad, m)
 * @param[in]  azel  Azimuth/elevation angle {az, el} (rad)
 * @param[out] var   Variance of tropospheric delay (m^2)
 * @return Tropospheric delay (m)
 */
double sbstropcorr(gtime_t time, const double *pos, const double *azel,
                   double *var);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_SBAS_H */
