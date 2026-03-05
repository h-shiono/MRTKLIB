/*------------------------------------------------------------------------------
 * mrtk_rtcm.h : RTCM type definitions and functions
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
 * @file mrtk_rtcm.h
 * @brief MRTKLIB RTCM Module — RTCM message types and functions.
 *
 * This header provides the RTCM control struct (rtcm_t) and all public
 * RTCM function declarations extracted from rtcm.c, rtcm2.c, rtcm3.c,
 * rtcm3e.c, and rtcm3lcl.c.
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       with zero algorithmic changes.
 */
#ifndef MRTK_RTCM_H
#define MRTK_RTCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_station.h"

/*============================================================================
 * RTCM Types
 *===========================================================================*/

/**
 * @brief RTCM control struct type.
 */
typedef struct {        /* RTCM control struct type */
    int staid;          /* station id */
    int stah;           /* station health */
    int seqno;          /* sequence number for rtcm 2 or iods msm */
    int outtype;        /* output message type */
    gtime_t time;       /* message time */
    gtime_t time_s;     /* message start time */
    obs_t obs;          /* observation data (uncorrected) */
    nav_t nav;          /* satellite ephemerides */
    sta_t sta;          /* station parameters */
    dgps_t *dgps;       /* output of dgps corrections */
    ssr_t ssr[MAXSAT];  /* output of ssr corrections */
    char msg[128];      /* special message */
    char msgtype[256];  /* last message type */
    char msmtype[7][128]; /* msm signal types */
    int obsflag;        /* obs data complete flag (1:ok,0:not complete) */
    int ephsat;         /* input ephemeris satellite number */
    int ephset;         /* input ephemeris set (0-1) */
    double cp[MAXSAT][NFREQ+NEXOBS]; /* carrier-phase measurement */
    uint16_t lock[MAXSAT][NFREQ+NEXOBS]; /* lock time */
    uint16_t loss[MAXSAT][NFREQ+NEXOBS]; /* loss of lock count */
    gtime_t lltime[MAXSAT][NFREQ+NEXOBS]; /* last lock time */
    int nbyte;          /* number of bytes in message buffer */
    int nbit;           /* number of bits in word buffer */
    int len;            /* message length (bytes) */
    uint8_t buff[1200]; /* message buffer */
    uint32_t word;      /* word buffer for rtcm 2 */
    uint32_t nmsg2[100]; /* message count of RTCM 2 (1-99:1-99,0:other) */
    uint32_t nmsg3[400]; /* message count of RTCM 3 (1-299:1001-1299,300-329:4070-4099,0:ohter) */
    char opt[256];      /* RTCM dependent options */
    lclblock_t lclblk;  /* output of iono/trop corrections */
} rtcm_t;

/*============================================================================
 * RTCM Functions
 *===========================================================================*/

/**
 * @brief Initialize RTCM control struct.
 * @param[out] rtcm  RTCM control struct
 * @return Status (1:ok, 0:memory allocation error)
 */
int init_rtcm(rtcm_t *rtcm);

/**
 * @brief Free RTCM control struct.
 * @param[in,out] rtcm  RTCM control struct
 */
void free_rtcm(rtcm_t *rtcm);

/**
 * @brief Input RTCM 2 message from stream (byte-by-byte).
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     data  Stream data (1 byte)
 * @return Status (-1:error, 0:no message, 1:obs data, 2:ephemeris, ...)
 */
int input_rtcm2(rtcm_t *rtcm, uint8_t data);

/**
 * @brief Input RTCM 3 message from stream (byte-by-byte).
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     data  Stream data (1 byte)
 * @return Status (-1:error, 0:no message, 1:obs data, 2:ephemeris, ...)
 */
int input_rtcm3(rtcm_t *rtcm, uint8_t data);

/**
 * @brief Input RTCM 2 message from file.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     fp    File pointer
 * @return Status (-2:end of file, -1:error, 0:no message, 1:obs, 2:eph, ...)
 */
int input_rtcm2f(rtcm_t *rtcm, FILE *fp);

/**
 * @brief Input RTCM 3 message from file.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     fp    File pointer
 * @return Status (-2:end of file, -1:error, 0:no message, 1:obs, 2:eph, ...)
 */
int input_rtcm3f(rtcm_t *rtcm, FILE *fp);

/**
 * @brief Generate RTCM 2 message.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     type  Message type
 * @param[in]     sync  Sync flag (1:another message follows)
 * @return Status (1:ok, 0:error)
 */
int gen_rtcm2(rtcm_t *rtcm, int type, int sync);

/**
 * @brief Generate RTCM 3 message.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     type  Message type
 * @param[in]     subtype  Message subtype (0:no subtype)
 * @param[in]     sync  Sync flag (1:another message follows)
 * @return Status (1:ok, 0:error)
 */
int gen_rtcm3(rtcm_t *rtcm, int type, int subtype, int sync);

/*============================================================================
 * RTCM Internal Functions (cross-file linkage within the module)
 *===========================================================================*/

/**
 * @brief Decode RTCM ver.2 message (called from rtcm.c).
 * @param[in,out] rtcm  RTCM control struct
 * @return Status (-1:error, 0:no message, 1:obs, 2:eph, ...)
 */
int decode_rtcm2(rtcm_t *rtcm);

/**
 * @brief Decode RTCM ver.3 message (called from rtcm.c).
 * @param[in,out] rtcm  RTCM control struct
 * @return Status (-1:error, 0:no message, 1:obs, 2:eph, ...)
 */
int decode_rtcm3(rtcm_t *rtcm);

/**
 * @brief Encode RTCM ver.3 message (called from rtcm.c).
 * @param[in,out] rtcm    RTCM control struct
 * @param[in]     type    Message type
 * @param[in]     subtype Message subtype
 * @param[in]     sync    Sync flag
 * @return Status (1:ok, 0:error)
 */
int encode_rtcm3(rtcm_t *rtcm, int type, int subtype, int sync);

/**
 * @brief Decode local tropospheric delay correction grid RTCM message.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     type  Message type
 * @return Status (1:ok, 0:error)
 */
int decode_lcltrop(rtcm_t *rtcm, int type);

/**
 * @brief Decode local ionospheric delay correction grid RTCM message.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     type  Message type
 * @return Status (1:ok, 0:error)
 */
int decode_lcliono(rtcm_t *rtcm, int type);

/**
 * @brief Encode local tropospheric delay correction grid RTCM message.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     type  Message type
 * @return Status (1:ok, 0:error)
 */
int encode_lcltrop(rtcm_t *rtcm, int type);

/**
 * @brief Encode local ionospheric delay correction grid RTCM message.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     type  Message type
 * @return Status (1:ok, 0:error)
 */
int encode_lcliono(rtcm_t *rtcm, int type);

/**
 * @brief Initialize block information struct.
 * @param[in,out] b  Block information struct
 */
void initblkinf(blkinf_t *b);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_RTCM_H */
