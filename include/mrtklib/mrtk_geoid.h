/*------------------------------------------------------------------------------
 * mrtk_geoid.h : geoid model functions
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
 * @file mrtk_geoid.h
 * @brief MRTKLIB Geoid Module — Geoid height models.
 *
 * This header declares the public geoid functions (opengeoid, closegeoid,
 * geoidh) and geoid model constants extracted from geoid.c.
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from geoid.c with zero algorithmic changes.
 */
#ifndef MRTK_GEOID_H
#define MRTK_GEOID_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Geoid Model Constants
 *===========================================================================*/

#define GEOID_EMBEDDED    0   /**< geoid model: embedded geoid (1x1 deg) */
#define GEOID_EGM96_M150  1   /**< geoid model: EGM96 15x15" */
#define GEOID_EGM2008_M25 2   /**< geoid model: EGM2008 2.5x2.5" */
#define GEOID_EGM2008_M10 3   /**< geoid model: EGM2008 1.0x1.0" */
#define GEOID_GSI2000_M15 4   /**< geoid model: GSI geoid 2000 1.0x1.5" */
#define GEOID_RAF09       5   /**< geoid model: IGN RAF09 for France 1.5"x2" */

/*============================================================================
 * Geoid Functions
 *===========================================================================*/

/**
 * @brief Open geoid model file.
 * @param[in] model  Geoid model type (GEOID_EMBEDDED, GEOID_EGM96_M150, etc.)
 * @param[in] file   Geoid model file path
 * @return Status (1:ok, 0:error)
 * @note The following geoid models can be used:
 *       - WW15MGH.DAC: EGM96 15x15" binary grid height
 *       - Und_min2.5x2.5_egm2008_isw=82_WGS84_TideFree_SE: EGM2008 2.5x2.5"
 *       - Und_min1x1_egm2008_isw=82_WGS84_TideFree_SE: EGM2008 1.0x1.0"
 *       - gsigeome_ver4: GSI geoid 2000 1.0x1.5" (Japanese area)
 *       (byte-order of binary files must be compatible to CPU)
 */
int opengeoid(int model, const char *file);

/**
 * @brief Close geoid model file.
 */
void closegeoid(void);

/**
 * @brief Get geoid height from geoid model.
 * @param[in] pos  Geodetic position {lat, lon} (rad)
 * @return Geoid height (m) (0.0: error)
 * @note To use external geoid model, call opengeoid() before this function.
 *       If no external model is open, the embedded geoid model is used.
 */
double geoidh(const double *pos);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_GEOID_H */
