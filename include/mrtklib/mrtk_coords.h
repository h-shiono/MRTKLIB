/*------------------------------------------------------------------------------
 * mrtk_coords.h : coordinate transformation functions
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
 * @file mrtk_coords.h
 * @brief MRTKLIB Coordinates Module — Coordinate transformations and geometry.
 *
 * This header provides coordinate transformation functions (ECEF, geodetic,
 * ENU), rotation matrices, degree/DMS conversions, ECI-to-ECEF transformation,
 * and positioning geometry functions (geometric distance, satellite azimuth/
 * elevation, DOP computation).
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from rtkcmn.c with zero algorithmic changes.
 */
#ifndef MRTK_COORDS_H
#define MRTK_COORDS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_time.h"

/*============================================================================
 * Degree / DMS Conversions
 *===========================================================================*/

/**
 * @brief Convert degree to degree-minute-second.
 * @param[in]  deg   Degree
 * @param[out] dms   Degree-minute-second {deg, min, sec}
 * @param[in]  ndec  Number of decimals of second
 */
void deg2dms(double deg, double *dms, int ndec);

/**
 * @brief Convert degree-minute-second to degree.
 * @param[in] dms  Degree-minute-second {deg, min, sec}
 * @return Degree
 */
double dms2deg(const double *dms);

/*============================================================================
 * ECEF / Geodetic Conversions
 *===========================================================================*/

/**
 * @brief Transform ECEF position to geodetic position.
 * @param[in]  r    ECEF position {x, y, z} (m)
 * @param[out] pos  Geodetic position {lat, lon, h} (rad, m)
 * @note WGS84, ellipsoidal height.
 */
void ecef2pos(const double *r, double *pos);

/**
 * @brief Transform geodetic position to ECEF position.
 * @param[in]  pos  Geodetic position {lat, lon, h} (rad, m)
 * @param[out] r    ECEF position {x, y, z} (m)
 * @note WGS84, ellipsoidal height.
 */
void pos2ecef(const double *pos, double *r);

/*============================================================================
 * ENU / ECEF Transformations
 *===========================================================================*/

/**
 * @brief Compute ECEF to local coordinate transformation matrix.
 * @param[in]  pos  Geodetic position {lat, lon} (rad)
 * @param[out] E    ECEF to local coord transformation matrix (3x3)
 * @note Matrix stored by column-major order (Fortran convention).
 */
void xyz2enu(const double *pos, double *E);

/**
 * @brief Transform ECEF vector to local tangential coordinate.
 * @param[in]  pos  Geodetic position {lat, lon} (rad)
 * @param[in]  r    Vector in ECEF coordinate {x, y, z}
 * @param[out] e    Vector in local tangential coordinate {e, n, u}
 */
void ecef2enu(const double *pos, const double *r, double *e);

/**
 * @brief Transform local tangential coordinate vector to ECEF.
 * @param[in]  pos  Geodetic position {lat, lon} (rad)
 * @param[in]  e    Vector in local tangential coordinate {e, n, u}
 * @param[out] r    Vector in ECEF coordinate {x, y, z}
 */
void enu2ecef(const double *pos, const double *e, double *r);

/**
 * @brief Transform ECEF covariance to local tangential coordinate.
 * @param[in]  pos  Geodetic position {lat, lon} (rad)
 * @param[in]  P    Covariance in ECEF coordinate
 * @param[out] Q    Covariance in local tangential coordinate
 */
void covenu(const double *pos, const double *P, double *Q);

/**
 * @brief Transform local ENU covariance to ECEF coordinate.
 * @param[in]  pos  Geodetic position {lat, lon} (rad)
 * @param[in]  Q    Covariance in local ENU coordinate
 * @param[out] P    Covariance in ECEF coordinate
 */
void covecef(const double *pos, const double *Q, double *P);

/*============================================================================
 * ECI / ECEF Transformation
 *===========================================================================*/

/**
 * @brief Compute ECI to ECEF transformation matrix.
 * @param[in]     tutc  Time in UTC
 * @param[in]     erpv  ERP values {xp, yp, ut1_utc, lod} (rad, rad, s, s/d)
 * @param[out]    U     ECI to ECEF transformation matrix (3x3)
 * @param[in,out] gmst  Greenwich mean sidereal time (rad) (NULL: no output)
 * @note Not thread-safe (uses static cache).
 */
void eci2ecef(gtime_t tutc, const double *erpv, double *U, double *gmst);

/*============================================================================
 * Positioning Geometry
 *===========================================================================*/

/**
 * @brief Compute geometric distance and receiver-to-satellite unit vector.
 * @param[in]  rs  Satellite position (ECEF at transmission) (m)
 * @param[in]  rr  Receiver position (ECEF at reception) (m)
 * @param[out] e   Line-of-sight vector (ECEF)
 * @return Geometric distance (m) (<0: error/no satellite position)
 * @note Distance includes Sagnac effect correction.
 */
double geodist(const double *rs, const double *rr, double *e);

/**
 * @brief Compute satellite azimuth/elevation angle.
 * @param[in]     pos   Geodetic position {lat, lon, h} (rad, m)
 * @param[in]     e     Receiver-to-satellite unit vector (ECEF)
 * @param[in,out] azel  Azimuth/elevation {az, el} (rad) (NULL: no output)
 *                      (0.0<=azel[0]<2*pi, -pi/2<=azel[1]<=pi/2)
 * @return Elevation angle (rad)
 */
double satazel(const double *pos, const double *e, double *azel);

/**
 * @brief Compute DOP (dilution of precision).
 * @param[in]  ns     Number of satellites
 * @param[in]  azel   Satellite azimuth/elevation angle (rad)
 * @param[in]  elmin  Elevation cutoff angle (rad)
 * @param[out] dop    DOPs {GDOP, PDOP, HDOP, VDOP}
 * @note dop[0]-[3] return 0 in case of DOP computation error.
 */
void dops(int ns, const double *azel, double elmin, double *dop);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_COORDS_H */
