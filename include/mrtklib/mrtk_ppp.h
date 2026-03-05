/*------------------------------------------------------------------------------
 * mrtk_ppp.h : precise point positioning functions
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
 * @file mrtk_ppp.h
 * @brief MRTKLIB Precise Point Positioning (PPP) Module.
 *
 * This header provides the PPP positioning functions including the main
 * PPP filter (pppos), number-of-states query (pppnx), and solution
 * status output (pppoutstat).
 *
 * @note Functions declared here are implemented in mrtk_ppp.c.
 */
#ifndef MRTK_PPP_H
#define MRTK_PPP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_rtkpos.h"

/*============================================================================
 * MADOCA-PPP L6D Ionospheric Correction (mdciono)
 *===========================================================================*/

/**
 * @brief QZSS L6D message control struct.
 *
 * Holds state for decoding MADOCA-PPP L6D ionospheric correction messages
 * from a single PRN (200/201).
 */
typedef struct {
    gtime_t time;           /**< decoded message time (GPST) */
    int nbyte;              /**< number of bytes in message buffer */
    uint8_t buff[256];      /**< message buffer */
    char opt[256];          /**< MADOCA-PPP L6D dependent options */
    int rid;                /**< decoded region ID */
    miono_region_t re;      /**< decoded region data */
} mdcl6d_t;

/**
 * @brief Initialize MADOCA-PPP L6D ionospheric correction control.
 * @param[in] gt  GPST for week number determination
 */
extern void init_miono(gtime_t gt);

/**
 * @brief Decode QZSS L6D message.
 * @param[in,out] mdcl6d  L6D message control struct
 * @return status (-1: error, 0: no message, 10: iono correction decoded)
 */
extern int decode_qzss_l6dmsg(mdcl6d_t *mdcl6d);

/**
 * @brief Input QZSS L6D message byte-by-byte.
 * @param[in,out] mdcl6d  L6D message control struct
 * @param[in]     data    L6D 1-byte data
 * @return status (-1: error, 0: no message, 10: iono correction decoded)
 */
extern int input_qzssl6d(mdcl6d_t *mdcl6d, uint8_t data);

/**
 * @brief Input QZSS L6D message from file.
 * @param[in,out] mdcl6d  L6D message control struct
 * @param[in]     fp      file pointer
 * @return status (-2: EOF, -1..10: same as input_qzssl6d)
 */
extern int input_qzssl6df(mdcl6d_t *mdcl6d, FILE *fp);

/*============================================================================
 * PPP Engine Functions
 *===========================================================================*/

/**
 * @brief Precise point positioning.
 * @param[in,out] rtk  RTK control/result struct
 * @param[in]     obs  Observation data
 * @param[in]     n    Number of observation data
 * @param[in]     nav  Navigation data
 */
void pppos(mrtk_ctx_t *ctx, rtk_t *rtk, const obsd_t *obs, int n,
           const nav_t *nav);

/**
 * @brief Number of estimated states for PPP.
 * @param[in] opt  Processing options
 * @return Number of states
 */
int pppnx(const prcopt_t *opt);

/**
 * @brief Write PPP solution status to buffer.
 * @param[in,out] rtk   RTK control/result struct
 * @param[out]    buff  Output buffer
 * @return Number of bytes written
 */
int pppoutstat(rtk_t *rtk, char *buff);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_PPP_H */
