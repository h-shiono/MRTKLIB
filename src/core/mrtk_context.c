/*------------------------------------------------------------------------------
 * mrtk_context.c : MRTKLIB runtime context management
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_context.h"

#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Global Context (transitional)
 *===========================================================================*/

mrtk_ctx_t *g_mrtk_ctx = NULL;

/*============================================================================
 * Lifecycle Functions
 *===========================================================================*/

mrtk_ctx_t *mrtk_ctx_create(void)
{
    mrtk_ctx_t *ctx = (mrtk_ctx_t *)calloc(1, sizeof(mrtk_ctx_t));
    if (!ctx) return NULL;

    ctx->is_running       = 0;
    ctx->cancel_requested = 0;
    ctx->last_err_code    = MRTK_OK;
    ctx->last_err_msg[0]  = '\0';
    ctx->trace_level      = 0;
    ctx->trace_fp         = NULL;
    ctx->tick_trace       = 0;
    ctx->user_data        = NULL;
    ctx->cb_showmsg       = NULL;

    return ctx;
}

void mrtk_ctx_destroy(mrtk_ctx_t *ctx)
{
    if (!ctx) return;

    /* Close trace file if not stdout/stderr */
    if (ctx->trace_fp && ctx->trace_fp != stdout && ctx->trace_fp != stderr) {
        fclose(ctx->trace_fp);
        ctx->trace_fp = NULL;
    }

    memset(ctx, 0, sizeof(mrtk_ctx_t));
    free(ctx);
}
