/*------------------------------------------------------------------------------
 * mrtk_fcb.h : fractional cycle bias (FCB) functions
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
 * @file mrtk_fcb.h
 * @brief MRTKLIB FCB Module — Fractional Cycle Bias data types and functions.
 *
 * This header provides FCB (Fractional Cycle Bias) read and update functions,
 * extracted from readfcb.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_fcb.c.
 */
#ifndef MRTK_FCB_H
#define MRTK_FCB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"

/*============================================================================
 * FCB Functions
 *===========================================================================*/

/**
 * @brief Read FCB (Fractional Cycle Bias) file.
 * @param[in] file  FCB file path (wild-card * expanded)
 * @return Status (1: success, 0: failure)
 */
int readfcb(const char *file);

/**
 * @brief Update satellite FCB structure with current biases.
 * @param[out] osb   FCB structure to update
 * @param[in]  gt    Current time
 * @param[in]  mode  Mode (0: current, 1: all)
 * @return Number of updated biases
 */
int udfcb_sat(osb_t *osb, gtime_t gt, int mode);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_FCB_H */
