/*------------------------------------------------------------------------------
 * mrtk_rtksvr.h : real-time RTK server type definitions and functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_rtksvr.h
 * @brief MRTKLIB RTK Server Module — real-time positioning server.
 *
 * This header provides the rtksvr_t type and functions for managing a
 * real-time RTK positioning server that reads receiver data streams,
 * performs positioning computation, and outputs solutions.
 *
 * @note Functions declared here are implemented in mrtk_rtksvr.c.
 */
#ifndef MRTK_RTKSVR_H
#define MRTK_RTKSVR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_sol.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_rcvraw.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_rtkpos.h"
#include "mrtklib/mrtk_stream.h"

/*============================================================================
 * RTK Server Type
 *===========================================================================*/

typedef struct {        /* RTK server type */
    int state;          /* server state (0:stop,1:running) */
    int cycle;          /* processing cycle (ms) */
    int nmeacycle;      /* NMEA request cycle (ms) (0:no req) */
    int nmeareq;        /* NMEA request (0:no,1:nmeapos,2:single sol) */
    double nmeapos[3];  /* NMEA request position (ecef) (m) */
    int buffsize;       /* input buffer size (bytes) */
    int format[3];      /* input format {rov,base,corr} */
    solopt_t solopt[2]; /* output solution options {sol1,sol2} */
    int navsel;         /* ephemeris select (0:all,1:rover,2:base,3:corr) */
    int nsbs;           /* number of sbas message */
    int nsol;           /* number of solution buffer */
    rtk_t rtk;          /* RTK control/result struct */
    int nb [3];         /* bytes in input buffers {rov,base} */
    int nsb[2];         /* bytes in soulution buffers */
    int npb[3];         /* bytes in input peek buffers */
    uint8_t *buff[3];   /* input buffers {rov,base,corr} */
    uint8_t *sbuf[2];   /* output buffers {sol1,sol2} */
    uint8_t *pbuf[3];   /* peek buffers {rov,base,corr} */
    sol_t solbuf[MAXSOLBUF]; /* solution buffer */
    uint32_t nmsg[3][12]; /* input message counts */
    raw_t  raw [3];     /* receiver raw control {rov,base,corr} */
    rtcm_t rtcm[3];     /* RTCM control {rov,base,corr} */
    sstat_t sstat;      /* single stat control */
    gtime_t ftime[3];   /* download time {rov,base,corr} */
    char files[3][MAXSTRPATH]; /* download paths {rov,base,corr} */
    obs_t obs[3][MAXOBSBUF]; /* observation data {rov,base,corr} */
    nav_t nav;          /* navigation data */
    sbsmsg_t sbsmsg[MAXSBSMSG]; /* SBAS message buffer */
    stream_t stream[8]; /* streams {rov,base,corr,sol1,sol2,logr,logb,logc} */
    stream_t *moni;     /* monitor stream */
    uint32_t tick;      /* start tick */
    rtk_thread_t thread; /* server thread */
    int cputime;        /* CPU time (ms) for a processing cycle */
    int prcout;         /* missing observation data count */
    int nave;           /* number of averaging base pos */
    double rb_ave[3];   /* averaging base pos */
    char cmds_periodic[3][MAXRCVCMD]; /* periodic commands */
    char cmd_reset[MAXRCVCMD]; /* reset command */
    double bl_reset;    /* baseline length to reset (km) */
    rtk_lock_t lock;    /* lock flag */
    mrtk_ctx_t *ctx;    /* runtime context */
} rtksvr_t;

/*============================================================================
 * RTK Server Functions
 *===========================================================================*/

/**
 * @brief Initialize RTK server.
 * @param[in,out] svr  RTK server struct
 * @return status (0:error,1:ok)
 */
int rtksvrinit(rtksvr_t *svr);

/**
 * @brief Free RTK server resources.
 * @param[in,out] svr  RTK server struct
 */
void rtksvrfree(rtksvr_t *svr);

/**
 * @brief Start RTK server.
 * @param[in,out] svr           RTK server struct
 * @param[in]     cycle         processing cycle (ms)
 * @param[in]     buffsize      input buffer size (bytes)
 * @param[in]     strs          stream types (8)
 * @param[in]     paths         stream paths (8)
 * @param[in]     formats       input formats (3)
 * @param[in]     navsel        navigation select (0:all,1:rov,2:base,3:corr)
 * @param[in]     cmds          startup commands (3)
 * @param[in]     cmds_periodic periodic commands (3)
 * @param[in]     rcvopts       receiver options (3)
 * @param[in]     nmeacycle     NMEA request cycle (ms) (0:no)
 * @param[in]     nmeareq       NMEA request type (0:no,1:nmeapos,2:single)
 * @param[in]     nmeapos       NMEA request position (ecef) (m) (3)
 * @param[in]     prcopt        processing options
 * @param[in]     solopt        solution options (2)
 * @param[in]     moni          monitor stream (NULL: no monitor)
 * @param[in]     rst           reset time
 * @param[out]    errmsg        error message
 * @return status (0:error,1:ok)
 */
int rtksvrstart(rtksvr_t *svr, int cycle, int buffsize, int *strs, char **paths,
                int *formats, int navsel, char **cmds, char **cmds_periodic,
                char **rcvopts, int nmeacycle, int nmeareq,
                const double *nmeapos, prcopt_t *prcopt, solopt_t *solopt,
                stream_t *moni, gtime_t rst, char *errmsg);

/**
 * @brief Stop RTK server.
 * @param[in,out] svr   RTK server struct
 * @param[in]     cmds  shutdown commands (3)
 */
void rtksvrstop(rtksvr_t *svr, char **cmds);

/**
 * @brief Open RTK server output/log stream.
 * @param[in,out] svr     RTK server struct
 * @param[in]     index   stream index
 * @param[in]     str     stream type
 * @param[in]     path    stream path
 * @param[in]     solopt  solution options
 * @return status (0:error,1:ok)
 */
int rtksvropenstr(rtksvr_t *svr, int index, int str, const char *path,
                  const solopt_t *solopt);

/**
 * @brief Close RTK server output/log stream.
 * @param[in,out] svr    RTK server struct
 * @param[in]     index  stream index
 */
void rtksvrclosestr(rtksvr_t *svr, int index);

/**
 * @brief Lock RTK server mutex.
 * @param[in,out] svr  RTK server struct
 */
void rtksvrlock(rtksvr_t *svr);

/**
 * @brief Unlock RTK server mutex.
 * @param[in,out] svr  RTK server struct
 */
void rtksvrunlock(rtksvr_t *svr);

/**
 * @brief Get observation data status.
 * @param[in]  svr   RTK server struct
 * @param[in]  type  receiver type (0:rover,1:base,2:corr)
 * @param[out] time  observation time
 * @param[out] sat   satellite numbers
 * @param[out] az    azimuths (rad)
 * @param[out] el    elevations (rad)
 * @param[out] snr   SNR (0.001 dBHz)
 * @param[out] vsat  valid satellite flags
 * @return number of satellites
 */
int rtksvrostat(rtksvr_t *svr, int type, gtime_t *time, int *sat, double *az,
                double *el, int **snr, int *vsat);

/**
 * @brief Get stream status.
 * @param[in,out] svr    RTK server struct
 * @param[out]    sstat  stream status
 * @param[out]    msg    status message
 */
void rtksvrsstat(rtksvr_t *svr, int *sstat, char *msg);

/**
 * @brief Mark current position in solution.
 * @param[in,out] svr      RTK server struct
 * @param[in]     name     mark name
 * @param[in]     comment  mark comment
 * @return status (0:error,1:ok)
 */
int rtksvrmark(rtksvr_t *svr, const char *name, const char *comment);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_RTKSVR_H */
