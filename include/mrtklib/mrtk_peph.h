/*------------------------------------------------------------------------------
 * mrtk_peph.h : precise ephemeris and ERP type definitions and functions
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
 * @file mrtk_peph.h
 * @brief MRTKLIB Precise Ephemeris Module — Earth rotation parameter types
 *        and ERP I/O functions.
 *
 * This header provides the ERP (Earth Rotation Parameter) data types and
 * the associated I/O and interpolation functions extracted from rtkcmn.c.
 *
 * @note Precise ephemeris types (peph_t, pclk_t) and functions that depend
 *       on nav_t remain in rtklib.h / preceph.c to avoid MAXSAT and nav_t
 *       dependencies in the mrtklib layer.
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from rtkcmn.c with zero algorithmic changes.
 */
#ifndef MRTK_PEPH_H
#define MRTK_PEPH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"

/*============================================================================
 * Earth Rotation Parameter Types
 *===========================================================================*/

/**
 * @brief ERP (earth rotation parameter) data type.
 */
typedef struct {
    double mjd;         /* mjd (days) */
    double xp,yp;       /* pole offset (rad) */
    double xpr,ypr;     /* pole offset rate (rad/day) */
    double ut1_utc;     /* ut1-utc (s) */
    double lod;         /* length of day (s/day) */
} erpd_t;

/**
 * @brief ERP (earth rotation parameter) type.
 */
typedef struct {
    int n,nmax;         /* number and max number of data */
    erpd_t *data;       /* earth rotation parameter data */
} erp_t;

/*============================================================================
 * Earth Rotation Parameter Functions
 *===========================================================================*/

/**
 * @brief Read earth rotation parameters from IGS ERP file (ver.2).
 * @param[in]  file  IGS ERP file path
 * @param[out] erp   Earth rotation parameters
 * @return Status (1:ok, 0:file open error)
 */
int readerp(const char *file, erp_t *erp);

/**
 * @brief Get earth rotation parameter values at specified time.
 * @param[in]  erp   Earth rotation parameters
 * @param[in]  time  Time (GPST)
 * @param[out] erpv  ERP values {xp,yp,ut1_utc,lod} (rad,rad,s,s/d)
 * @return Status (1:ok, 0:error)
 */
int geterp(const erp_t *erp, gtime_t time, double *erpv);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_PEPH_H */
