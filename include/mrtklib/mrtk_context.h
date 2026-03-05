/*------------------------------------------------------------------------------
 * mrtk_context.h : MRTKLIB legacy context type definitions
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
 * @file mrtk_context.h
 * @brief MRTKLIB Context — runtime context for processing, trace, and error handling.
 *
 * This header defines the non-opaque runtime context structure that replaces
 * global variables for trace output, error state, and processing control.
 * All trace functions and major processing pipeline functions accept a
 * pointer to this context as their first argument.
 */
#ifndef MRTK_CONTEXT_H
#define MRTK_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

/*============================================================================
 * Error Codes
 *===========================================================================*/

/**
 * @brief Error code enumeration for MRTKLIB functions.
 */
typedef enum {
    MRTK_OK          =  0,  /**< Success */
    MRTK_ERR_VALUE   = -1,  /**< Invalid parameter value */
    MRTK_ERR_IO      = -2,  /**< I/O error (file, stream) */
    MRTK_ERR_MEMORY  = -3,  /**< Memory allocation failure */
    MRTK_ERR_FORMAT  = -4,  /**< Data format error */
    MRTK_ERR_CANCEL  = -5   /**< Processing cancelled */
} mrtk_err_t;

/*============================================================================
 * Callback Types
 *===========================================================================*/

/**
 * @brief Message display callback (for GUI/app-level status messages).
 * @param[in] msg  Null-terminated message string
 */
typedef void (*mrtk_showmsg_cb_t)(const char *msg);

/*============================================================================
 * Context Structure
 *===========================================================================*/

/**
 * @brief Runtime context for MRTKLIB processing.
 *
 * Holds per-session state: runtime control flags, error state, trace
 * configuration, user data, and application callbacks.  All major
 * processing and trace functions accept a pointer to this structure
 * as their first argument.
 */
typedef struct {
    /* Runtime control */
    volatile int is_running;        /**< Non-zero while processing is active */
    volatile int cancel_requested;  /**< Non-zero to request cancellation */

    /* Error state */
    mrtk_err_t last_err_code;       /**< Most recent error code */
    char       last_err_msg[256];   /**< Most recent error message */

    /* Trace / debug */
    int      trace_level;   /**< Trace level (0:off, 1:error..5:verbose) */
    FILE    *trace_fp;      /**< Trace output stream (NULL = disabled) */
    uint32_t tick_trace;    /**< Tick count at traceopen (for elapsed time) */

    /* User data and callbacks */
    void             *user_data;    /**< Application-specific data pointer */
    mrtk_showmsg_cb_t cb_showmsg;   /**< Status message callback (may be NULL) */
} mrtk_ctx_t;

/*============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Create a new runtime context.
 *
 * Allocates and zero-initialises a context.  Default settings:
 * - trace_level: 0 (off)
 * - trace_fp: NULL (no trace output)
 * - All other members: zero / NULL
 *
 * @return Newly allocated context, or NULL on allocation failure.
 */
mrtk_ctx_t *mrtk_ctx_create(void);

/**
 * @brief Destroy a runtime context.
 *
 * Closes the trace file (unless stdout/stderr) and frees memory.
 * Safe to call with NULL.
 *
 * @param[in] ctx  Context to destroy, or NULL (no-op)
 */
void mrtk_ctx_destroy(mrtk_ctx_t *ctx);

/*============================================================================
 * Global Context (transitional)
 *===========================================================================*/

/**
 * @brief Global context pointer for transitional migration.
 *
 * Functions that have not yet received an explicit ctx parameter use
 * this global as a fallback.  Applications should set this at startup
 * and clear it at shutdown.
 *
 * @note This pointer will be removed once all modules have been migrated
 *       to explicit context passing.
 */
extern mrtk_ctx_t *g_mrtk_ctx;

#ifdef __cplusplus
}
#endif

#endif /* MRTK_CONTEXT_H */
