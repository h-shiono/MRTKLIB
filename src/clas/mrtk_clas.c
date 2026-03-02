/*------------------------------------------------------------------------------
 * mrtk_clas.c : QZSS CLAS (L6D) CSSR decoder and bank management
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
 *
 * References:
 *   [1] claslib (https://github.com/mf-arai/claslib) — upstream implementation
 *   [2] IS-QZSS-L6-003 — CLAS Compact SSR Specification
 *----------------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_clas.h"

/*============================================================================
 * Context Management
 *===========================================================================*/

extern int clas_ctx_init(clas_ctx_t *ctx)
{
    int i;
    if (!ctx) return -1;

    memset(ctx, 0, sizeof(clas_ctx_t));

    for (i = 0; i < CLAS_CH_NUM; i++) {
        ctx->bank[i] = (clas_bank_ctrl_t *)calloc(1, sizeof(clas_bank_ctrl_t));
        if (!ctx->bank[i]) {
            clas_ctx_free(ctx);
            return -1;
        }
    }
    ctx->initialized = 1;
    return 0;
}

extern void clas_ctx_free(clas_ctx_t *ctx)
{
    int i;
    if (!ctx) return;

    for (i = 0; i < CLAS_CH_NUM; i++) {
        free(ctx->bank[i]);
        ctx->bank[i] = NULL;
    }
    ctx->initialized = 0;
}
