/*------------------------------------------------------------------------------
 * mrtk_rinex.h : RINEX file I/O type definitions and functions
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
 * @file mrtk_rinex.h
 * @brief MRTKLIB RINEX Module — RINEX file read/write types and functions.
 *
 * This header provides the RINEX option type (rnxopt_t), RINEX control
 * struct (rnxctr_t), and all public RINEX read/write function declarations
 * extracted from rinex.c.
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from rinex.c with zero algorithmic changes.
 */
#ifndef MRTK_RINEX_H
#define MRTK_RINEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"

/*============================================================================
 * RINEX Types
 *===========================================================================*/

/**
 * @brief RINEX options type.
 */
typedef struct {        /* RINEX options type */
    gtime_t ts,te;      /* time start/end */
    double tint;        /* time interval (s) */
    double ttol;        /* time tolerance (s) */
    double tunit;       /* time unit for multiple-session (s) */
    int rnxver;         /* RINEX version (x100) */
    int navsys;         /* navigation system */
    int obstype;        /* observation type */
    int freqtype;       /* frequency type */
    char mask[7][MAXCODE]; /* code mask {GPS,GLO,GAL,QZS,SBS,CMP,IRN} */
    char staid [32];    /* station id for RINEX file name */
    char prog  [32];    /* program */
    char runby [32];    /* run-by */
    char marker[64];    /* marker name */
    char markerno[32];  /* marker number */
    char markertype[32]; /* marker type (ver.3) */
    char name[2][32];   /* observer/agency */
    char rec [3][32];   /* receiver #/type/vers */
    char ant [3][32];   /* antenna #/type */
    double apppos[3];   /* approx position x/y/z */
    double antdel[3];   /* antenna delta h/e/n */
    double glo_cp_bias[4]; /* GLONASS code-phase biases (m) */
    char comment[MAXCOMMENT][64]; /* comments */
    char rcvopt[256];   /* receiver dependent options */
    uint8_t exsats[MAXSAT]; /* excluded satellites */
    int glofcn[32];     /* glonass fcn+8 */
    int outiono;        /* output iono correction */
    int outtime;        /* output time system correction */
    int outleaps;       /* output leap seconds */
    int autopos;        /* auto approx position */
    int phshift;        /* phase shift correction */
    int halfcyc;        /* half cycle correction */
    int sep_nav;        /* separated nav files */
    gtime_t tstart;     /* first obs time */
    gtime_t tend;       /* last obs time */
    gtime_t trtcm;      /* approx log start time for rtcm */
    char tobs[7][MAXOBSTYPE][4]; /* obs types {GPS,GLO,GAL,QZS,SBS,CMP,IRN} */
    double shift[7][MAXOBSTYPE]; /* phase shift (cyc) {GPS,GLO,GAL,QZS,SBS,CMP,IRN} */
    int nobs[7];        /* number of obs types {GPS,GLO,GAL,QZS,SBS,CMP,IRN} */
} rnxopt_t;

/**
 * @brief RINEX control struct type.
 */
typedef struct {        /* RINEX control struct type */
    gtime_t time;       /* message time */
    int    ver;         /* RINEX version * 100 */
    char   type;        /* RINEX file type ('O','N',...) */
    int    sys;         /* navigation system */
    int    tsys;        /* time system */
    char   tobs[8][MAXOBSTYPE][4]; /* rinex obs types */
    obs_t  obs;         /* observation data */
    nav_t  nav;         /* navigation data */
    sta_t  sta;         /* station info */
    int    ephsat;      /* input ephemeris satellite number */
    int    ephset;      /* input ephemeris set (0-1) */
    char   opt[256];    /* rinex dependent options */
} rnxctr_t;

/*============================================================================
 * RINEX Read Functions
 *===========================================================================*/

/**
 * @brief Read RINEX OBS and NAV files.
 * @param[in]    file  File path (wild-card * expanded) ("": stdin)
 * @param[in]    rcv   Receiver number for obs data
 * @param[in]    opt   RINEX options ("": no option)
 * @param[in,out] obs  Observation data (NULL: no input)
 * @param[in,out] nav  Navigation data (NULL: no input)
 * @param[in,out] sta  Station parameters (NULL: no input)
 * @return Status (1:ok, 0:no data, -1:error)
 */
int readrnx(const char *file, int rcv, const char *opt, obs_t *obs,
            nav_t *nav, sta_t *sta);

/**
 * @brief Read RINEX OBS and NAV files with time span.
 * @param[in]    file  File path (wild-card * expanded) ("": stdin)
 * @param[in]    rcv   Receiver number for obs data
 * @param[in]    ts    Observation time start (ts.time==0: no limit)
 * @param[in]    te    Observation time end (te.time==0: no limit)
 * @param[in]    tint  Observation time interval (s) (0:all)
 * @param[in]    opt   RINEX options ("": no option)
 * @param[in,out] obs  Observation data (NULL: no input)
 * @param[in,out] nav  Navigation data (NULL: no input)
 * @param[in,out] sta  Station parameters (NULL: no input)
 * @return Status (1:ok, 0:no data, -1:error)
 */
int readrnxt(const char *file, int rcv, gtime_t ts, gtime_t te, double tint,
             const char *opt, obs_t *obs, nav_t *nav, sta_t *sta);

/**
 * @brief Read RINEX clock files.
 * @param[in]    file  File path (wild-card * expanded)
 * @param[in,out] nav  Navigation data (NULL: no input)
 * @return Number of precise clock records
 */
int readrnxc(const char *file, nav_t *nav);

/*============================================================================
 * RINEX Write Functions
 *===========================================================================*/

/**
 * @brief Output RINEX observation data file header.
 * @param[in] fp   Output file pointer
 * @param[in] opt  RINEX options
 * @param[in] nav  Navigation data
 * @return Status (1:ok, 0:output error)
 */
int outrnxobsh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);

/**
 * @brief Output RINEX observation data body.
 * @param[in] fp    Output file pointer
 * @param[in] opt   RINEX options
 * @param[in] obs   Observation data
 * @param[in] n     Number of observation data
 * @param[in] flag  Epoch flag (0:ok,1:power failure,>1:event flag)
 * @return Status (1:ok, 0:output error)
 */
int outrnxobsb(FILE *fp, const rnxopt_t *opt, const obsd_t *obs, int n,
               int flag);

/**
 * @brief Output RINEX navigation data file header.
 * @param[in] fp   Output file pointer
 * @param[in] opt  RINEX options
 * @param[in] nav  Navigation data (NULL: no input)
 * @return Status (1:ok, 0:output error)
 */
int outrnxnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);

/**
 * @brief Output RINEX navigation data file body.
 * @param[in] fp   Output file pointer
 * @param[in] opt  RINEX options
 * @param[in] eph  Ephemeris
 * @return Status (1:ok, 0:output error)
 */
int outrnxnavb(FILE *fp, const rnxopt_t *opt, const eph_t *eph);

/**
 * @brief Output RINEX GLONASS navigation data file header.
 * @param[in] fp   Output file pointer
 * @param[in] opt  RINEX options
 * @param[in] nav  Navigation data (NULL: no input)
 * @return Status (1:ok, 0:output error)
 */
int outrnxgnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);

/**
 * @brief Output RINEX GLONASS navigation data file body.
 * @param[in] fp    Output file pointer
 * @param[in] opt   RINEX options
 * @param[in] geph  GLONASS ephemeris
 * @return Status (1:ok, 0:output error)
 */
int outrnxgnavb(FILE *fp, const rnxopt_t *opt, const geph_t *geph);

/**
 * @brief Output RINEX GEO navigation data file header.
 * @param[in] fp   Output file pointer
 * @param[in] opt  RINEX options
 * @param[in] nav  Navigation data (NULL: no input)
 * @return Status (1:ok, 0:output error)
 */
int outrnxhnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);

/**
 * @brief Output RINEX GEO navigation data file body.
 * @param[in] fp    Output file pointer
 * @param[in] opt   RINEX options
 * @param[in] seph  SBAS ephemeris
 * @return Status (1:ok, 0:output error)
 */
int outrnxhnavb(FILE *fp, const rnxopt_t *opt, const seph_t *seph);

/**
 * @brief Output RINEX Galileo navigation data file header.
 * @param[in] fp   Output file pointer
 * @param[in] opt  RINEX options
 * @param[in] nav  Navigation data (NULL: no input)
 * @return Status (1:ok, 0:output error)
 */
int outrnxlnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);

/**
 * @brief Output RINEX QZSS navigation data file header.
 * @param[in] fp   Output file pointer
 * @param[in] opt  RINEX options
 * @param[in] nav  Navigation data (NULL: no input)
 * @return Status (1:ok, 0:output error)
 */
int outrnxqnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);

/**
 * @brief Output RINEX BDS navigation data file header.
 * @param[in] fp   Output file pointer
 * @param[in] opt  RINEX options
 * @param[in] nav  Navigation data (NULL: no input)
 * @return Status (1:ok, 0:output error)
 */
int outrnxcnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);

/**
 * @brief Output RINEX NavIC/IRNSS navigation data file header.
 * @param[in] fp   Output file pointer
 * @param[in] opt  RINEX options
 * @param[in] nav  Navigation data (NULL: no input)
 * @return Status (1:ok, 0:output error)
 */
int outrnxinavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);

/*============================================================================
 * RINEX Control Functions
 *===========================================================================*/

/**
 * @brief Initialize RINEX control struct.
 * @param[in,out] rnx  RINEX control struct
 * @return Status (1:ok, 0:memory allocation error)
 */
int init_rnxctr(rnxctr_t *rnx);

/**
 * @brief Free RINEX control struct buffers.
 * @param[in,out] rnx  RINEX control struct
 */
void free_rnxctr(rnxctr_t *rnx);

/**
 * @brief Open RINEX data (read header).
 * @param[in,out] rnx  RINEX control struct
 * @param[in]     fp   File pointer
 * @return Status (-2:end of file, 0:no message, 1:ok)
 */
int open_rnxctr(rnxctr_t *rnx, FILE *fp);

/**
 * @brief Input RINEX control (read next message).
 * @param[in,out] rnx  RINEX control struct
 * @param[in]     fp   File pointer
 * @return Status (-2:end of file, 0:no message, 1:obs data, 2:nav data)
 */
int input_rnxctr(rnxctr_t *rnx, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_RINEX_H */
