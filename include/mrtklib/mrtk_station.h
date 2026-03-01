/*------------------------------------------------------------------------------
 * mrtk_station.h : station position and BLQ functions
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
 * @file mrtk_station.h
 * @brief MRTKLIB Station Module — Site environment parameter I/O.
 *
 * This header provides functions for reading station-related files:
 * elevation mask, reference positions, and ocean tide loading (BLQ)
 * parameters, extracted from rtkcmn.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_station.c.
 */
#ifndef MRTK_STATION_H
#define MRTK_STATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*============================================================================
 * Station Parameter I/O Functions
 *===========================================================================*/

/**
 * @brief Read elevation mask file.
 * @param[in]  file    Elevation mask file
 * @param[out] elmask  Elevation mask vector (360 x 1) (0.1 deg)
 *                     elmask[i]: elevation mask at azimuth i (deg)
 * @return Status (1:ok, 0:file open error)
 * @note Text format: AZ EL per line. Text after %% or # is comment.
 */
int readelmask(const char *file, int16_t *elmask);

/**
 * @brief Read station positions from file.
 * @param[in]  file  Station position file (lat lon height name per line)
 * @param[in]  rcv   Station name to search
 * @param[out] pos   Station position {lat,lon,h} (rad/m)
 *                   (all 0 if search error)
 */
void readpos(const char *file, const char *rcv, double *pos);

/**
 * @brief Read BLQ ocean tide loading parameters.
 * @param[in]  file   BLQ ocean tide loading parameter file
 * @param[in]  sta    Station name
 * @param[out] odisp  Ocean tide loading parameters
 * @return Status (1:ok, 0:file open error)
 */
int readblq(const char *file, const char *sta, double *odisp);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_STATION_H */
