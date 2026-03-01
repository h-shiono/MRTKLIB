/*------------------------------------------------------------------------------
 * mrtk_madoca_local_corr.h : MADOCA local correction type definitions and functions
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
 * @file mrtk_madoca_local_corr.h
 * @brief MRTKLIB MADOCA Local Correction Module — local correction common
 *        functions and constants.
 *
 * This header provides functions for local tropospheric and ionospheric
 * correction data processing, extracted from lclcmn.c with zero algorithmic
 * changes.
 *
 * @note Functions declared here are implemented in mrtk_madoca_local_corr.c.
 */
#ifndef MRTK_MADOCA_LOCAL_CORR_H
#define MRTK_MADOCA_LOCAL_CORR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_rtcm.h"

/*============================================================================
 * Local Correction Constants
 *===========================================================================*/

#define TYPE_TRP        0               /* data type of troposphere */
#define TYPE_ION        1               /* data type of ionosphere */
#define MAX_DIST_TRP    100.0           /* max distance for tropospheric correction (km) */
#define MAX_DIST_ION    50.0            /* max distance for ionospheric correction (km) */
#define MODE_PLANE      0               /* interpolation mode planer fitting */
#define MODE_DISTWGT    1               /* interpolation mode weighted average of distance */
#define BTYPE_GRID      0               /* block type grid */
#define BTYPE_STA       1               /* block type station */
#define ION_BLKSIZE     2.0             /* ionosphere block size */
#define TRP_BLKSIZE     4.0             /* troposphere block size */
#define MAX_STANAME     16              /* max length of station name */

/*============================================================================
 * Local Correction Common Functions
 *===========================================================================*/

/**
 * @brief Search for a point by name or add a new point if not found.
 * @param[in,out] stat      Local correction data
 * @param[in]     pntname   Point name
 * @return Point index (>=0: found or added, 0: error)
 */
int getstano(stat_t *stat, const char *pntname);

/**
 * @brief Stack stat message and synchronize frame with stat preamble.
 * @param[in,out] sstat  Local correction data
 * @param[in]     data   Stat 1-byte data
 * @param[out]    buff   Buffer for message
 * @param[in,out] nbyte  Number of bytes
 * @return Status (-1: error, 0: no message, 11: input stat messages)
 */
int input_stat(sstat_t *sstat, const char data, char *buff, int *nbyte);

/**
 * @brief Fetch next stat message and input a message from file.
 * @param[in,out] sstat  Local correction data
 * @param[in]     fp     File pointer
 * @param[out]    buff   Buffer for message
 * @param[in,out] nbyte  Number of bytes
 * @return Status (-2: end of file, -1...11: same as input_stat())
 */
int input_statf(sstat_t *sstat, FILE *fp, char *buff, int *nbyte);

/**
 * @brief Calculate the Euclidean distance between two ECEF positions.
 * @param[in] ecef1  Position 1 (ECEF coordinates)
 * @param[in] ecef2  Position 2 (ECEF coordinates)
 * @return Distance between the two positions (m)
 */
double posdist(const double *ecef1, const double *ecef2);

/**
 * @brief Get tropospheric correction for a point.
 * @param[in]  time  Time (GPST)
 * @param[in]  trpd  Tropospheric correction data
 * @param[out] trp   Tropospheric parameters {ztd, grade, gradn} (m)
 * @param[out] std   Standard deviation (m)
 * @return Status (1:ok, 0:error)
 */
int get_trop_sta(gtime_t time, const trp_t *trpd, double *trp, double *std);

/**
 * @brief Get ionospheric correction for a point.
 * @param[in]  time  Time (GPST)
 * @param[in]  iond  Ionospheric correction data (MAXSAT elements)
 * @param[out] ion   Ionospheric delay for each satellite (m)
 * @param[out] std   Standard deviation (m)
 * @param[out] el    Elevation angle (rad)
 * @return Number of valid corrections
 */
int get_iono_sta(gtime_t time, const ion_t *iond, double *ion, double *std,
                 double *el);

/**
 * @brief Calculate tropospheric correction using weighted interpolation by
 *        distance.
 * @param[in]  stat     Correction data
 * @param[in]  time     Time (GPST)
 * @param[in]  llh      Receiver position {lat, lon, hgt} (rad, m)
 * @param[in]  maxdist  Max distance threshold (km)
 * @param[out] trp      Tropospheric parameters {ztd, grade, gradn} (m)
 * @param[out] std      Standard deviation (m)
 * @return Status (1:ok, 0:error)
 */
int corr_trop_distwgt(const stat_t *stat, gtime_t time, const double *llh,
                      double maxdist, double *trp, double *std);

/**
 * @brief Calculate ionospheric correction using weighted interpolation by
 *        distance.
 * @param[in]  stat     Local correction data
 * @param[in]  time     Time (GPST)
 * @param[in]  llh      Receiver position {lat, lon, hgt} (rad, m)
 * @param[in]  el       Receiver to satellite elevation (rad)
 * @param[in]  maxdist  Max distance threshold (km)
 * @param[out] ion      L1 slant ionospheric delay for each sat (MAXSAT x 1)
 * @param[out] std      Standard deviation (m)
 * @param[in]  maxres   Maximum residual threshold (m)
 * @param[out] nrej     Number of rejected stations
 * @param[out] ncnt     Number of counted stations
 * @return Status (1:ok, 0:error)
 */
int corr_iono_distwgt(const stat_t *stat, gtime_t time, const double *llh,
                      const double *el, double maxdist, double *ion,
                      double *std, double maxres, int *nrej, int *ncnt);

/**
 * @brief Convert RTCM3 block data to station statistics.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in,out] stat  Station data struct
 * @return Status (0: error, 1: success)
 */
int block2stat(rtcm_t *rtcm, stat_t *stat);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_MADOCA_LOCAL_CORR_H */
