/*------------------------------------------------------------------------------
 * mrtk_madoca.h : MADOCA-PPP processing functions
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
 * @file mrtk_madoca.h
 * @brief MRTKLIB MADOCA-PPP Module — QZSS L6E CSSR message decode functions.
 *
 * This header provides functions for decoding QZSS L6E (MADOCA-PPP) Compact
 * SSR messages and converting them to RTCM SSR format, extracted from
 * mdccssr.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_madoca.c.
 */
#ifndef MRTK_MADOCA_H
#define MRTK_MADOCA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_rtkpos.h"

/*============================================================================
 * MADOCA-PPP Functions
 *===========================================================================*/

/**
 * @brief Set MADOCA-PPP CSSR channel index for multi-L6E.
 * @param[in] ch  Channel index (0 or 1)
 */
void set_mcssr_ch(int ch);

/**
 * @brief Get current MADOCA-PPP CSSR channel index.
 * @return Current channel index
 */
int get_mcssr_ch(void);

/**
 * @brief Initialize MADOCA-PPP CSSR control struct for current channel.
 * @param[in] gt  GPST for week number determination
 */
void init_mcssr(const gtime_t gt);

/**
 * @brief Decode QZSS L6E message and convert to RTCM SSR.
 * @param[in,out] rtcm  RTCM control struct
 * @return Status (-1: error, 0: no message, 10: input SSR messages)
 * @note Before calling this function, call init_mcssr().
 */
int decode_qzss_l6emsg(rtcm_t *rtcm);

/**
 * @brief Stack QZSS L6E message and synchronize frame with L6 preamble.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     data  L6E 1-byte data
 * @return Status (-1: error, 0: no message, 10: input CSSR messages)
 * @note Before calling this function, call init_mcssr().
 */
int input_qzssl6e(rtcm_t *rtcm, const uint8_t data);

/**
 * @brief Fetch next QZSS L6E message and input a message from file.
 * @param[in,out] rtcm  RTCM control struct
 * @param[in]     fp    File pointer
 * @return Status (-2: end of file, -1...10: same as input_qzssl6e())
 * @note Same as input_qzssl6e().
 */
int input_qzssl6ef(rtcm_t *rtcm, FILE *fp);

/**
 * @brief Select MADOCA-PPP code/phase bias code from observation code.
 * @param[in] sys   Navigation system
 * @param[in] code  Observation code
 * @return MADOCA-PPP CB/PB code
 */
int mcssr_sel_biascode(const int sys, const int code);


#ifdef __cplusplus
}
#endif

#endif /* MRTK_MADOCA_H */
