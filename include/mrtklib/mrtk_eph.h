/*------------------------------------------------------------------------------
 * mrtk_eph.h : satellite ephemeris type definitions and functions
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
 * @file mrtk_eph.h
 * @brief MRTKLIB Broadcast Ephemeris Module — Satellite orbit/clock types and
 *        Kepler/GLONASS/SBAS orbit computation functions.
 *
 * This header provides the fundamental broadcast ephemeris struct types
 * (eph_t, geph_t, seph_t, alm_t, galm_t) and the pure orbit/clock
 * computation functions extracted from ephemeris.c.
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from ephemeris.c with zero algorithmic changes.
 */
#ifndef MRTK_EPH_H
#define MRTK_EPH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"

/*============================================================================
 * Broadcast Ephemeris Types
 *===========================================================================*/

/**
 * @brief GPS/GAL/QZS/BDS/IRN almanac type.
 */
typedef struct {
    int sat;            /* satellite number */
    int svh;            /* SV health (0:ok) */
    int svconf;         /* AS and SV config */
    int week;           /* GPS/QZS: GPS week, GAL: Galileo week */
    gtime_t toa;        /* Toa */
    double A,e,i0,OMG0,omg,M0,OMGd; /* SV orbit parameters */
    double toas;        /* Toa (s) in week */
    double f0,f1;       /* SV clock parameters (af0,af1) */
} alm_t;

/**
 * @brief GLONASS almanac type.
 */
typedef struct {
    int sat;            /* satellite number */
    int Cn;             /* unhealthy flag (1:unhealthy) */
    int Mn;             /* satellite type (0:GLONASS,1:GLONASS-M) */
    int Hn;             /* carrier-frequency number */
    gtime_t toa;        /* Toa */
    double tn;          /* time of first acsending node passage (s) */
    double lamn,din,eccn,omgn,dTn,ddTn; /* SV orbit parameters */
    double taun;        /* SV clock parameters */
} galm_t;

/**
 * @brief GPS/GAL/QZS/BDS/IRN broadcast ephemeris type.
 */
typedef struct {
    int sat;            /* satellite number */
    int iode,iodc;      /* IODE,IODC */
    int sva;            /* SV accuracy (URA index) */
    int svh;            /* SV health (0:ok) */
    int week;           /* GPS/QZS: gps week, GAL: galileo week */
    int code;           /* GPS/QZS: code on L2 */
                        /* GAL: data source defined as rinex 3.03 */
                        /* BDS: data source (0:unknown,1:B1I,2:B1Q,3:B2I,4:B2Q,5:B3I,6:B3Q) */
    int flag;           /* GPS/QZS: L2 P data flag */
                        /* BDS: nav type (0:unknown,1:IGSO/MEO,2:GEO) */
    gtime_t toe,toc,ttr; /* Toe,Toc,T_trans */
                        /* SV orbit parameters */
    double A,e,i0,OMG0,omg,M0,deln,OMGd,idot;
    double crc,crs,cuc,cus,cic,cis;
    double toes;        /* Toe (s) in week */
    double fit;         /* fit interval (h) */
    double f0,f1,f2;    /* SV clock parameters (af0,af1,af2) */
    double tgd[6];      /* group delay parameters */
                        /* GPS/QZS:tgd[0]=TGD */
                        /* GAL:tgd[0]=BGD_E1E5a,tgd[1]=BGD_E1E5b */
                        /* BDS:tgd[0]=TGD_B1I ,tgd[1]=TGD_B2I/B2b,tgd[2]=TGD_B1Cp */
                        /*     tgd[3]=TGD_B2ap,tgd[4]=ISC_B1Cd   ,tgd[5]=ISC_B2ad */
    int type;           /* ephemeris type */
                        /* GPS/QZS: 0=LNAV,1=CNAV,2=CNAV-2 */
                        /* GAL    : 0=INAV,1=FNAV */
                        /* BDS    : 0=D1,1=D2,2=CNAV-1,3=CNAV-2,4=CNAV-3 */
                        /* IRN    : 0=LNAV */
    double Adot,ndot;   /* Adot,ndot for CNAV */
} eph_t;

/**
 * @brief GLONASS broadcast ephemeris type.
 */
typedef struct {
    int sat;            /* satellite number */
    int iode;           /* IODE (0-6 bit of tb field) */
    int frq;            /* FCN (frequency channel number) */
    int svh;            /* extended SVH (b3:ln,b2:Cn_a,b1:Cn,b0:Bn) */
    int flags;          /* status flags (b78:M,b6:P4,b5:P3,b4:P2,b23:P1,b01:P) */
    int sva,age;        /* URA index (FT), age of operation (En) */
    gtime_t toe;        /* epoch of ephemerides (gpst) */
    gtime_t tof;        /* message frame time (gpst) */
    double pos[3];      /* satellite position (ecef) (m) */
    double vel[3];      /* satellite velocity (ecef) (m/s) */
    double acc[3];      /* satellite acceleration (ecef) (m/s^2) */
    double taun,gamn;   /* SV clock bias (s)/relative freq bias */
    double dtaun;       /* delay between L1 and L2 (s) */
} geph_t;

/**
 * @brief SBAS ephemeris type.
 */
typedef struct {
    int sat;            /* satellite number */
    gtime_t t0;         /* reference epoch time (GPST) */
    gtime_t tof;        /* time of message frame (GPST) */
    int sva;            /* SV accuracy (URA index) */
    int svh;            /* SV health (0:ok) */
    double pos[3];      /* satellite position (m) (ecef) */
    double vel[3];      /* satellite velocity (m/s) (ecef) */
    double acc[3];      /* satellite acceleration (m/s^2) (ecef) */
    double af0,af1;     /* satellite clock-offset/drift (s,s/s) */
} seph_t;

/*============================================================================
 * Broadcast Ephemeris Functions
 *===========================================================================*/

/**
 * @brief Compute satellite clock bias with broadcast ephemeris (GPS/GAL/QZS).
 * @param[in] time  Time by satellite clock (GPST)
 * @param[in] eph   Broadcast ephemeris
 * @return Satellite clock bias (s) without relativity correction
 */
double eph2clk(gtime_t time, const eph_t *eph);

/**
 * @brief Compute satellite position and clock with broadcast ephemeris
 *        (GPS/GAL/QZS/BDS/IRN).
 * @param[in]  time  Time (GPST)
 * @param[in]  eph   Broadcast ephemeris
 * @param[out] rs    Satellite position (ecef) {x,y,z} (m)
 * @param[out] dts   Satellite clock bias (s)
 * @param[out] var   Satellite position and clock variance (m^2)
 */
void eph2pos(gtime_t time, const eph_t *eph, double *rs, double *dts,
             double *var);

/**
 * @brief Compute satellite clock bias with GLONASS ephemeris.
 * @param[in] time  Time by satellite clock (GPST)
 * @param[in] geph  GLONASS ephemeris
 * @return Satellite clock bias (s)
 */
double geph2clk(gtime_t time, const geph_t *geph);

/**
 * @brief Compute satellite position and clock with GLONASS ephemeris.
 * @param[in]  time  Time (GPST)
 * @param[in]  geph  GLONASS ephemeris
 * @param[out] rs    Satellite position {x,y,z} (ecef) (m)
 * @param[out] dts   Satellite clock bias (s)
 * @param[out] var   Satellite position and clock variance (m^2)
 */
void geph2pos(gtime_t time, const geph_t *geph, double *rs, double *dts,
              double *var);

/**
 * @brief Compute satellite clock bias with SBAS ephemeris.
 * @param[in] time  Time by satellite clock (GPST)
 * @param[in] seph  SBAS ephemeris
 * @return Satellite clock bias (s)
 */
double seph2clk(gtime_t time, const seph_t *seph);

/**
 * @brief Compute satellite position and clock with SBAS ephemeris.
 * @param[in]  time  Time (GPST)
 * @param[in]  seph  SBAS ephemeris
 * @param[out] rs    Satellite position {x,y,z} (ecef) (m)
 * @param[out] dts   Satellite clock bias (s)
 * @param[out] var   Satellite position and clock variance (m^2)
 */
void seph2pos(gtime_t time, const seph_t *seph, double *rs, double *dts,
              double *var);

/**
 * @brief Compute satellite position and clock bias with almanac.
 * @param[in]  time  Time (GPST)
 * @param[in]  alm   Almanac
 * @param[out] rs    Satellite position (ecef) {x,y,z} (m)
 * @param[out] dts   Satellite clock bias (s)
 */
void alm2pos(gtime_t time, const alm_t *alm, double *rs, double *dts);

/*============================================================================
 * Ephemeris Selection Functions
 *===========================================================================*/

/**
 * @brief Set selected satellite ephemeris for multiple ones.
 * @param[in] sys  Satellite system (SYS_???)
 * @param[in] sel  Selection of ephemeris
 *                   GPS,QZS : 0:LNAV ,1:CNAV  (default: LNAV)
 *                   GAL     : 0:I/NAV,1:F/NAV (default: I/NAV)
 *                   others  : undefined
 */
void setseleph(int sys, int sel);

/**
 * @brief Get selected satellite ephemeris.
 * @param[in] sys  Satellite system (SYS_???)
 * @return Selected ephemeris (refer setseleph())
 */
int getseleph(int sys);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_EPH_H */
