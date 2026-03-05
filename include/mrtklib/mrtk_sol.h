/*------------------------------------------------------------------------------
 * mrtk_sol.h : solution type definitions and I/O functions
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
 * @file mrtk_sol.h
 * @brief MRTKLIB Solution Module — Solution data types and I/O functions.
 *
 * This header provides the solution data structures (sol_t, solbuf_t,
 * solstat_t, solstatbuf_t, ssat_t) and solution I/O functions extracted
 * from rtklib.h/solution.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_sol.c.
 */
#ifndef MRTK_SOL_H
#define MRTK_SOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_opt.h"

/*============================================================================
 * Solution Data Types
 *===========================================================================*/

typedef struct {        /* solution type */
    gtime_t time;       /* time (GPST) */
    double rr[6];       /* position/velocity (m|m/s) */
                        /* {x,y,z,vx,vy,vz} or {e,n,u,ve,vn,vu} */
    float  qr[6];       /* position variance/covariance (m^2) */
                        /* {c_xx,c_yy,c_zz,c_xy,c_yz,c_zx} or */
                        /* {c_ee,c_nn,c_uu,c_en,c_nu,c_ue} */
    float  qv[6];       /* velocity variance/covariance (m^2/s^2) */
    double dtr[NSYS+1]; /* receiver clock bias to time systems (s) */
    uint8_t type;       /* type (0:xyz-ecef,1:enu-baseline) */
    uint8_t stat;       /* solution status (SOLQ_???) */
    uint8_t ns;         /* number of valid satellites */
    float age;          /* age of differential (s) */
    float ratio;        /* AR ratio factor for valiation */
    float thres;        /* AR ratio threshold for valiation */
} sol_t;

typedef struct {        /* solution buffer type */
    int n,nmax;         /* number of solution/max number of buffer */
    int cyclic;         /* cyclic buffer flag */
    int start,end;      /* start/end index */
    gtime_t time;       /* current solution time */
    sol_t *data;        /* solution data */
    double rb[3];       /* reference position {x,y,z} (ecef) (m) */
    uint8_t buff[MAXSOLMSG+1]; /* message buffer */
    int nb;             /* number of byte in message buffer */
} solbuf_t;

typedef struct {        /* solution status type */
    gtime_t time;       /* time (GPST) */
    uint8_t sat;        /* satellite number */
    uint8_t frq;        /* frequency (1:L1,2:L2,...) */
    float az,el;        /* azimuth/elevation angle (rad) */
    float resp;         /* pseudorange residual (m) */
    float resc;         /* carrier-phase residual (m) */
    uint8_t flag;       /* flags: (vsat<<5)+(slip<<3)+fix */
    uint16_t snr;       /* signal strength (*SNR_UNIT dBHz) */
    uint16_t lock;      /* lock counter */
    uint16_t outc;      /* outage counter */
    uint16_t slipc;     /* slip counter */
    uint16_t rejc;      /* reject counter */
} solstat_t;

typedef struct {        /* solution status buffer type */
    int n,nmax;         /* number of solution/max number of buffer */
    solstat_t *data;    /* solution status data */
} solstatbuf_t;

/*============================================================================
 * Satellite Status Type
 *===========================================================================*/

typedef struct {        /* satellite status type */
    uint8_t sys;        /* navigation system */
    uint8_t vs;         /* valid satellite flag single */
    double azel[2];     /* azimuth/elevation angles {az,el} (rad) */
    double resp[NFREQ]; /* residuals of pseudorange (m) */
    double resc[NFREQ]; /* residuals of carrier-phase (m) */
    uint8_t vsat[NFREQ]; /* valid satellite flag */
    uint16_t snr[NFREQ]; /* signal strength (*SNR_UNIT dBHz) */
    uint8_t fix [NFREQ]; /* ambiguity fix flag (1:fix,2:float,3:hold) */
    uint8_t slip[NFREQ]; /* cycle-slip flag */
    uint8_t half[NFREQ]; /* half-cycle valid flag */
    int lock [NFREQ];   /* lock counter of phase */
    uint32_t outc [NFREQ]; /* obs outage counter of phase */
    uint32_t slipc[NFREQ]; /* cycle-slip counter */
    uint32_t rejc [NFREQ]; /* reject counter */
    double gf[NFREQ];   /* geometry-free phase (m) */
    double mw[NFREQ];   /* MW-LC (m) */
    double phw;         /* phase windup (cycle) */
    gtime_t pt[2][NFREQ]; /* previous carrier-phase time */
    double ph[2][NFREQ]; /* previous carrier-phase observable (cycle) */
    int discont[NFREQ]; /* SSR phase bias discontinuity counter */
    double ionc;        /* ionospheric delay by carrier phase (m) */
    uint8_t code[NFREQ]; /* observation code indicator (CODE_???) */
    int pbreset[NFREQ]; /* phase bias reset flag */
} ssat_t;

/*============================================================================
 * Solution Buffer Functions
 *===========================================================================*/

/**
 * @brief Initialize solution buffer.
 * @param[out] solbuf  Solution buffer
 * @param[in]  cyclic  Cyclic buffer flag (0:off, 1:on)
 * @param[in]  nmax    Max number of solutions (0:default)
 */
void initsolbuf(solbuf_t *solbuf, int cyclic, int nmax);

/**
 * @brief Free solution buffer memory.
 * @param[in,out] solbuf  Solution buffer
 */
void freesolbuf(solbuf_t *solbuf);

/**
 * @brief Free solution status buffer memory.
 * @param[in,out] solstatbuf  Solution status buffer
 */
void freesolstatbuf(solstatbuf_t *solstatbuf);

/**
 * @brief Get solution from buffer.
 * @param[in,out] solbuf  Solution buffer
 * @param[in]     index   Solution index (0 to n-1)
 * @return Solution data pointer (NULL: out of range)
 */
sol_t *getsol(solbuf_t *solbuf, int index);

/**
 * @brief Add solution to buffer.
 * @param[in,out] solbuf  Solution buffer
 * @param[in]     sol     Solution data
 * @return Status (1:ok, 0:error)
 */
int addsol(solbuf_t *solbuf, const sol_t *sol);

/*============================================================================
 * Solution I/O Functions
 *===========================================================================*/

/**
 * @brief Read solutions from files.
 * @param[in]  files   Solution file paths
 * @param[in]  nfile   Number of files
 * @param[out] sol     Solution buffer
 * @return Status (1:ok, 0:error)
 */
int readsol(char *files[], int nfile, solbuf_t *sol);

/**
 * @brief Read solutions from files with time filter.
 * @param[in]  files   Solution file paths
 * @param[in]  nfile   Number of files
 * @param[in]  ts      Start time (GPST) (ts.time==0: no limit)
 * @param[in]  te      End time (GPST) (te.time==0: no limit)
 * @param[in]  tint    Time interval (s) (0.0: no limit)
 * @param[in]  qflag   Quality flag (0:all)
 * @param[out] sol     Solution buffer
 * @return Status (1:ok, 0:error)
 */
int readsolt(char *files[], int nfile, gtime_t ts, gtime_t te, double tint,
             int qflag, solbuf_t *sol);

/**
 * @brief Read solution status from files.
 * @param[in]  files    Solution status file paths
 * @param[in]  nfile    Number of files
 * @param[out] statbuf  Solution status buffer
 * @return Status (1:ok, 0:error)
 */
int readsolstat(char *files[], int nfile, solstatbuf_t *statbuf);

/**
 * @brief Read solution status from files with time filter.
 * @param[in]  files    Solution status file paths
 * @param[in]  nfile    Number of files
 * @param[in]  ts       Start time (GPST)
 * @param[in]  te       End time (GPST)
 * @param[in]  tint     Time interval (s)
 * @param[out] statbuf  Solution status buffer
 * @return Status (1:ok, 0:error)
 */
int readsolstatt(char *files[], int nfile, gtime_t ts, gtime_t te, double tint,
                 solstatbuf_t *statbuf);

/**
 * @brief Input solution data from stream.
 * @param[in]     data    Stream data byte
 * @param[in]     ts      Start time (GPST)
 * @param[in]     te      End time (GPST)
 * @param[in]     tint    Time interval (s)
 * @param[in]     qflag   Quality flag (0:all)
 * @param[in]     opt     Solution options
 * @param[in,out] solbuf  Solution buffer
 * @return Status (0:no data, 1:new solution)
 */
int inputsol(uint8_t data, gtime_t ts, gtime_t te, double tint, int qflag,
             const solopt_t *opt, solbuf_t *solbuf);

/*============================================================================
 * Solution Output Functions
 *===========================================================================*/

/**
 * @brief Output processing options to buffer.
 * @param[out] buff  Output buffer
 * @param[in]  opt   Processing options
 * @return Number of output bytes
 */
int outprcopts(uint8_t *buff, const prcopt_t *opt);

/**
 * @brief Output solution header to buffer.
 * @param[out] buff  Output buffer
 * @param[in]  opt   Solution options
 * @return Number of output bytes
 */
int outsolheads(uint8_t *buff, const solopt_t *opt);

/**
 * @brief Output solution to buffer.
 * @param[out] buff  Output buffer
 * @param[in]  sol   Solution data
 * @param[in]  rb    Reference position (ecef) (m)
 * @param[in]  opt   Solution options
 * @return Number of output bytes
 */
int outsols(uint8_t *buff, const sol_t *sol, const double *rb,
            const solopt_t *opt);

/**
 * @brief Output solution extended data to buffer.
 * @param[out] buff  Output buffer
 * @param[in]  sol   Solution data
 * @param[in]  ssat  Satellite status
 * @param[in]  opt   Solution options
 * @return Number of output bytes
 */
int outsolexs(uint8_t *buff, const sol_t *sol, const ssat_t *ssat,
              const solopt_t *opt);

/**
 * @brief Output processing options to file.
 * @param[in] fp   Output file pointer
 * @param[in] opt  Processing options
 */
void outprcopt(FILE *fp, const prcopt_t *opt);

/**
 * @brief Output solution header to file.
 * @param[in] fp   Output file pointer
 * @param[in] opt  Solution options
 */
void outsolhead(FILE *fp, const solopt_t *opt);

/**
 * @brief Output solution to file.
 * @param[in] fp   Output file pointer
 * @param[in] sol  Solution data
 * @param[in] rb   Reference position (ecef) (m)
 * @param[in] opt  Solution options
 */
void outsol(FILE *fp, const sol_t *sol, const double *rb, const solopt_t *opt);

/**
 * @brief Output solution extended data to file.
 * @param[in] fp    Output file pointer
 * @param[in] sol   Solution data
 * @param[in] ssat  Satellite status
 * @param[in] opt   Solution options
 */
void outsolex(FILE *fp, const sol_t *sol, const ssat_t *ssat,
              const solopt_t *opt);

/*============================================================================
 * NMEA Output Functions
 *===========================================================================*/

/**
 * @brief Output NMEA RMC sentence.
 * @param[out] buff  Output buffer
 * @param[in]  sol   Solution data
 * @return Number of output bytes
 */
int outnmea_rmc(uint8_t *buff, const sol_t *sol);

/**
 * @brief Output NMEA GGA sentence.
 * @param[out] buff  Output buffer
 * @param[in]  sol   Solution data
 * @return Number of output bytes
 */
int outnmea_gga(uint8_t *buff, const sol_t *sol);

/**
 * @brief Output NMEA GSA sentence.
 * @param[out] buff  Output buffer
 * @param[in]  sol   Solution data
 * @param[in]  ssat  Satellite status
 * @return Number of output bytes
 */
int outnmea_gsa(uint8_t *buff, const sol_t *sol, const ssat_t *ssat);

/**
 * @brief Output NMEA GSV sentence.
 * @param[out] buff  Output buffer
 * @param[in]  sol   Solution data
 * @param[in]  ssat  Satellite status
 * @return Number of output bytes
 */
int outnmea_gsv(uint8_t *buff, const sol_t *sol, const ssat_t *ssat);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_SOL_H */
