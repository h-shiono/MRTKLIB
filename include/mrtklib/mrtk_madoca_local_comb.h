/*------------------------------------------------------------------------------
 * mrtk_madoca_local_comb.h : MADOCA local correction data combination functions
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
 * @file mrtk_madoca_local_comb.h
 * @brief MRTKLIB MADOCA Local Combination Module — local correction data
 *        combination functions.
 *
 * This header provides functions for combining local tropospheric and
 * ionospheric correction data (grid interpolation, station selection,
 * and RTCM3 output), extracted from lclcmbcmn.c with zero algorithmic
 * changes.
 *
 * @note Functions declared here are implemented in mrtk_madoca_local_comb.c.
 */
#ifndef MRTK_MADOCA_LOCAL_COMB_H
#define MRTK_MADOCA_LOCAL_COMB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_stream.h"
#include "mrtklib/mrtk_madoca_local_corr.h"

/*============================================================================
 * Input Monitor Stream Type
 *===========================================================================*/

typedef struct {        /* input monitor stream type */
    char sta[32];       /* station */
    int  fmt;           /* format */
    int  stat;          /* input stream status */
    int  dis;           /* disable flag */
    int  type_str;      /* input stream type */
    int  type_log;      /* log stream type */
    char path_str[MAXSTRPATH]; /* input stream path */
    char path_log[MAXSTRPATH]; /* log stream path */
    stream_t str;       /* input stream */
    stream_t log;       /* log stream */
    gtime_t tcon;       /* connect time */
    gtime_t time;       /* time tag of last stat data */
    unsigned int cnt;   /* input data count {stat} */
    sstat_t sstat;      /* stat correction */
    uint8_t buff[4096]; /* stat message buffer */
    int nbyte;          /* number of stat message */
    double rr[3];       /* position(ecef) */
} instat_t;

/*============================================================================
 * Stream Local Combination Server Type
 *===========================================================================*/

typedef struct {        /* stream local combination server */
    gtime_t ts;         /* update time */
    int btype;          /* block type */
    int state;          /* state (0:stop,1:run) */
    int navsys;         /* system nav data */
    int ninp;           /* number of input monitor streams */
    unsigned int cnt;   /* count {rtcm} */
    instat_t ins[MAXSITES]; /* input stat streams */
    stat_t stat;        /* input of iono/trop corrections */
    rtcm_t rtcm;        /* output of iono/trop corrections */
    stream_t ostr;      /* output stream for combined data */
    stream_t olog;      /* log stream for combined data */
    rtk_thread_t thread;/* server thread */
    rtk_lock_t lock;    /* mutex for lock */
} lclcmbsvr_t;

/*============================================================================
 * Local Correction Data Combination Functions
 *===========================================================================*/

/**
 * @brief Initialize grid/station settings from a specified file.
 * @param[in]     setfile  Grid/station setting file path
 * @param[in,out] lclblk   Local block information struct
 * @param[in]     btype    Block type (BTYPE_GRID or BTYPE_STA)
 * @return Status (0: error, 1: success)
 */
int initgridsta(const char *setfile, lclblock_t *lclblk, int btype);

/**
 * @brief Interpolate tropospheric grid data.
 * @param[in]     gt     Time
 * @param[in]     stat   Station data
 * @param[in,out] lclblk Local block information
 */
void grid_intp_trop(const gtime_t gt, const stat_t *stat,
                    lclblock_t *lclblk);

/**
 * @brief Select and assign tropospheric data blocks based on station positions.
 * @param[in]     time   Time
 * @param[in]     stat   Station data
 * @param[in,out] lslblk Local block information struct
 */
void sta_sel_trop(const gtime_t time, const stat_t *stat,
                  lclblock_t *lslblk);

/**
 * @brief Interpolate ionospheric grid data.
 * @param[in]     gt     Time
 * @param[in]     stat   Station data
 * @param[in,out] lclblk Local block information struct
 * @param[in]     maxres Maximum residual
 * @param[out]    nrej   Number of rejections
 * @param[out]    ncnt   Number of counts
 */
void grid_intp_iono(const gtime_t gt, const stat_t *stat,
                    lclblock_t *lclblk, double maxres, int *nrej, int *ncnt);

/**
 * @brief Select and assign ionospheric data blocks based on station positions.
 * @param[in]     gt     Time
 * @param[in]     stat   Station data
 * @param[in,out] lslblk Local block information struct
 */
void sta_sel_iono(const gtime_t gt, const stat_t *stat,
                  lclblock_t *lslblk);

/**
 * @brief Generate and output RTCM3 local combination correction messages.
 * @param[in,out] rtcm   RTCM control struct
 * @param[in]     btype  Block type (BTYPE_GRID or BTYPE_STA)
 * @param[in]     ostr   Output stream
 */
void output_lclcmb(rtcm_t *rtcm, int btype, stream_t *ostr);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_MADOCA_LOCAL_COMB_H */
