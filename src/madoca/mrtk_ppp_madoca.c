/*------------------------------------------------------------------------------
 * mrtk_ppp_madoca.c : MADOCA PPP engine stub
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
 * @file mrtk_ppp_madoca.c
 * @brief MADOCA PPP engine compute stub.
 *
 * This file provides a placeholder implementation for the MADOCALIB-based PPP
 * engine. It will be replaced by the actual MADOCALIB integration in a later
 * phase.
 */
#include "mrtklib/rtklib.h"

/**
 * @brief MADOCA PPP engine compute stub.
 *
 * Currently outputs a trace message and sets solution status to SOLQ_NONE.
 * Will be replaced by actual MADOCALIB PPP processing logic.
 *
 * @param[in]     ctx  MRTKLIB context
 * @param[in,out] rtk  RTK control/result struct
 * @param[in]     obs  Observation data array
 * @param[in]     n    Number of observations
 * @param[in]     nav  Navigation data
 */
extern void mrtk_ppp_madoca_compute(mrtk_ctx_t *ctx, rtk_t *rtk,
                                    const obsd_t *obs, int n, const nav_t *nav)
{
    trace(ctx, 1, "mrtk_ppp_madoca_compute: MADOCA PPP engine is not yet "
                  "implemented.\n");
    rtk->sol.stat = SOLQ_NONE;
}
