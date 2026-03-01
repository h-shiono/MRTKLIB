/*------------------------------------------------------------------------------
 * mrtk_rcvraw.h : receiver raw data type definitions and functions
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
 * @file mrtk_rcvraw.h
 * @brief MRTKLIB Receiver Raw Data Module — Receiver raw data types and
 *        decoder functions for various GNSS receiver protocols.
 *
 * This header provides the raw_t control struct, stream format constants,
 * and all receiver-specific decoder function declarations extracted from
 * rtklib.h/rcvraw.c with zero algorithmic changes.
 *
 * @note Core functions are implemented in mrtk_rcvraw.c.
 *       Receiver-specific decoders are in src/rcv/mrtk_rcv_*.c.
 */
#ifndef MRTK_RCVRAW_H
#define MRTK_RCVRAW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_rtcm.h"

#include <stdio.h>

/*============================================================================
 * Stream Format Constants
 *===========================================================================*/

#define STRFMT_RTCM2 0                  /* stream format: RTCM 2 */
#define STRFMT_RTCM3 1                  /* stream format: RTCM 3 */
#define STRFMT_OEM4  2                  /* stream format: NovAtel OEMV/4 */
#define STRFMT_OEM3  3                  /* stream format: NovAtel OEM3 */
#define STRFMT_UBX   4                  /* stream format: u-blox LEA-*T */
#define STRFMT_SS2   5                  /* stream format: NovAtel Superstar II */
#define STRFMT_CRES  6                  /* stream format: Hemisphere */
#define STRFMT_STQ   7                  /* stream format: SkyTraq S1315F */
#define STRFMT_JAVAD 8                 /* stream format: JAVAD GRIL/GREIS */
#define STRFMT_NVS   9                  /* stream format: NVS NVC08C */
#define STRFMT_BINEX 10                 /* stream format: BINEX */
#define STRFMT_RT17  11                 /* stream format: Trimble RT17 */
#define STRFMT_SEPT  12                 /* stream format: Septentrio */
#define STRFMT_RINEX 13                 /* stream format: RINEX */
#define STRFMT_SP3   14                 /* stream format: SP3 */
#define STRFMT_RNXCLK 15               /* stream format: RINEX CLK */
#define STRFMT_SBAS  16                 /* stream format: SBAS messages */
#define STRFMT_NMEA  17                 /* stream format: NMEA 0183 */
#define STRFMT_STAT  20                 /* stream format: stat */
#define STRFMT_L6E   21                 /* stream format: L6E CSSR */

/*============================================================================
 * Receiver Raw Data Control Type
 *===========================================================================*/

/**
 * @brief Receiver raw data control type.
 */
typedef struct {
    gtime_t time;       /**< message time */
    gtime_t tobs[MAXSAT][NFREQ+NEXOBS]; /**< observation data time */
    obs_t obs;          /**< observation data */
    obs_t obuf;         /**< observation data buffer */
    nav_t nav;          /**< satellite ephemerides */
    sta_t sta;          /**< station parameters */
    int ephsat;         /**< update satellite of ephemeris (0:no satellite) */
    int ephset;         /**< update set of ephemeris (0-1) */
    sbsmsg_t sbsmsg;    /**< SBAS message */
    char msgtype[256];  /**< last message type */
    uint8_t subfrm[MAXSAT][380]; /**< subframe buffer */
    double lockt[MAXSAT][NFREQ+NEXOBS]; /**< lock time (s) */
    double icpp[MAXSAT],off[MAXSAT],icpc; /**< carrier params for ss2 */
    double prCA[MAXSAT],dpCA[MAXSAT]; /**< L1/CA pseudrange/doppler for javad */
    uint8_t halfc[MAXSAT][NFREQ+NEXOBS]; /**< half-cycle add flag */
    char freqn[MAXOBS]; /**< frequency number for javad */
    int nbyte;          /**< number of bytes in message buffer */
    int len;            /**< message length (bytes) */
    int iod;            /**< issue of data */
    int tod;            /**< time of day (ms) */
    int tbase;          /**< time base (0:gpst,1:utc(usno),2:glonass,3:utc(su)) */
    int flag;           /**< general purpose flag */
    int outtype;        /**< output message type */
    uint8_t buff[MAXRAWLEN]; /**< message buffer */
    char opt[256];      /**< receiver dependent options */
    int format;         /**< receiver stream format */
    void *rcv_data;     /**< receiver dependent data */
} raw_t;

/*============================================================================
 * Core Raw Data Functions
 *===========================================================================*/

/**
 * @brief Initialize receiver raw data control struct.
 * @param[out] raw     Receiver raw data control struct
 * @param[in]  format  Stream format (STRFMT_???)
 * @return Status (1:ok, 0:memory allocation error)
 */
int init_raw(raw_t *raw, int format);

/**
 * @brief Free receiver raw data control struct.
 * @param[in,out] raw  Receiver raw data control struct
 */
void free_raw(raw_t *raw);

/**
 * @brief Input receiver raw data from stream (byte-by-byte).
 * @param[in,out] raw    Receiver raw data control struct
 * @param[in,out] rtcm   RTCM control struct
 * @param[in]     format Receiver raw data format (STRFMT_???)
 * @param[in]     data   Stream data (1 byte)
 * @return Status (-1:error, 0:no message, 1:obs data, 2:ephemeris,
 *                 3:sbas message, 9:ion/utc parameter)
 */
int input_raw(raw_t *raw, rtcm_t *rtcm, int format, uint8_t data);

/**
 * @brief Input receiver raw data from file.
 * @param[in,out] raw    Receiver raw data control struct
 * @param[in,out] rtcm   RTCM control struct
 * @param[in]     format Receiver raw data format (STRFMT_???)
 * @param[in]     fp     File pointer
 * @return Status (-2:end of file, -1...31:same as input_raw)
 */
int input_rawf(raw_t *raw, rtcm_t *rtcm, int format, FILE *fp);

/*============================================================================
 * Frame Decoding Functions
 *===========================================================================*/

int decode_frame(const uint8_t *buff, eph_t *eph, alm_t *alm,
                 double *ion, double *utc);
int test_glostr(const uint8_t *buff);
int decode_glostr(const uint8_t *buff, geph_t *geph, double *utc);
int decode_bds_d1(const uint8_t *buff, eph_t *eph, double *ion, double *utc);
int decode_bds_d2(const uint8_t *buff, eph_t *eph, double *utc);
int decode_gal_inav(const uint8_t *buff, eph_t *eph, double *ion, double *utc);
int decode_gal_fnav(const uint8_t *buff, eph_t *eph, double *ion, double *utc);
int decode_irn_nav(const uint8_t *buff, eph_t *eph, double *ion, double *utc);

/*============================================================================
 * Receiver-Specific Initialization Functions
 *===========================================================================*/

int  init_rt17(raw_t *raw);
void free_rt17(raw_t *raw);

/*============================================================================
 * Receiver-Specific Decoder Functions (byte-by-byte input)
 *===========================================================================*/

int input_oem4 (raw_t *raw, uint8_t data);
int input_oem3 (raw_t *raw, uint8_t data);
int input_ubx  (raw_t *raw, rtcm_t *rtcm, uint8_t data);
int input_ss2  (raw_t *raw, uint8_t data);
int input_cres (raw_t *raw, uint8_t data);
int input_stq  (raw_t *raw, uint8_t data);
int input_javad(raw_t *raw, uint8_t data);
int input_nvs  (raw_t *raw, uint8_t data);
int input_bnx  (raw_t *raw, uint8_t data);
int input_rt17 (raw_t *raw, uint8_t data);
int input_sbf  (raw_t *raw, rtcm_t *rtcm, uint8_t data);

/*============================================================================
 * Receiver-Specific Decoder Functions (file input)
 *===========================================================================*/

int input_oem4f (raw_t *raw, FILE *fp);
int input_oem3f (raw_t *raw, FILE *fp);
int input_ubxf  (raw_t *raw, rtcm_t *rtcm, FILE *fp);
int input_ss2f  (raw_t *raw, FILE *fp);
int input_cresf (raw_t *raw, FILE *fp);
int input_stqf  (raw_t *raw, FILE *fp);
int input_javadf(raw_t *raw, FILE *fp);
int input_nvsf  (raw_t *raw, FILE *fp);
int input_bnxf  (raw_t *raw, FILE *fp);
int input_rt17f (raw_t *raw, FILE *fp);
int input_sbff  (raw_t *raw, rtcm_t *rtcm, FILE *fp);

/*============================================================================
 * Message Generation Functions
 *===========================================================================*/

int gen_ubx(const char *msg, uint8_t *buff);
int gen_stq(const char *msg, uint8_t *buff);
int gen_nvs(const char *msg, uint8_t *buff);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_RCVRAW_H */
