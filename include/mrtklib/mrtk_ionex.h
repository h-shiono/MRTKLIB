/**
 * @file mrtk_ionex.h
 * @brief MRTKLIB IONEX Module — IONEX ionospheric TEC map I/O and interpolation.
 *
 * This header declares the IONEX-related functions extracted from rtklib.h
 * with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_ionex.c.
 */
#ifndef MRTK_IONEX_H
#define MRTK_IONEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"

/*============================================================================
 * IONEX Functions
 *===========================================================================*/

/**
 * @brief Read IONEX ionospheric TEC grid file.
 * @param[in]     file  IONEX TEC grid file path (wild-card * expanded)
 * @param[in,out] nav   Navigation data (tec grid data appended)
 * @param[in]     opt   Read option (1: no clear of old TEC data, 0: clear)
 */
void readtec(const char *file, nav_t *nav, int opt);

/**
 * @brief Compute ionospheric delay by TEC grid data.
 * @param[in]  time   Time (GPST)
 * @param[in]  nav    Navigation data (with TEC grid)
 * @param[in]  pos    Receiver position {lat,lon,h} (rad,m)
 * @param[in]  azel   Azimuth/elevation angle {az,el} (rad)
 * @param[in]  opt    Model option (bit 0: 0=E-layer, 1=F2-layer;
 *                    bit 1: 0=single-layer, 1=modified single-layer)
 * @param[out] delay  Ionospheric delay (L1) (m)
 * @param[out] var    Ionospheric delay variance (L1) (m^2)
 * @return Status (1:ok, 0:error)
 */
int iontec(gtime_t time, const nav_t *nav, const double *pos,
           const double *azel, int opt, double *delay, double *var);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_IONEX_H */
