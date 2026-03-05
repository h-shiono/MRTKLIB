/*------------------------------------------------------------------------------
 * mrtk_core.c : MRTKLIB core context management and logging
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
 * @file mrtk_core.c
 * @brief Core implementation of MRTKLIB context management and logging.
 *
 * This file contains the implementation of the opaque context structure
 * and all associated lifecycle, logging, and error handling functions.
 */

#include "mrtklib/mrtklib.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Legacy Migration Support
 *===========================================================================*/

/**
 * @brief Global context for legacy RTKLIB trace() bridge.
 *
 * @see g_mrtk_legacy_ctx declaration in mrtklib.h
 */
mrtk_context_t* g_mrtk_legacy_ctx = NULL;

/*============================================================================
 * Private Constants
 *===========================================================================*/

/** @brief Maximum length of error messages stored in context */
#define MAX_ERROR_LEN 256

/** @brief Maximum length of formatted log messages */
#define MAX_LOG_LEN 1024

/*============================================================================
 * Private Structure Definition
 *===========================================================================*/

/**
 * @brief Internal context structure.
 *
 * This structure holds all per-instance state for the library.
 * It is intentionally hidden from users to maintain API stability.
 */
struct mrtk_context_s {
    mrtk_log_level_t log_level;             /**< Minimum log level threshold */
    mrtk_log_cb_t    log_callback;          /**< User-defined log handler */
    char             last_error[MAX_ERROR_LEN]; /**< Most recent error message */
    void*            user_data;             /**< Application-specific data pointer */
};

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Default log callback implementation.
 *
 * Outputs log messages to stdout (DEBUG, INFO, WARN) or stderr (ERROR).
 * This is the default handler used when no custom callback is registered.
 *
 * @param ctx   Context instance (unused in default implementation)
 * @param level Severity level of the message
 * @param msg   Formatted message string
 */
static void default_log_callback(mrtk_context_t* ctx, mrtk_log_level_t level,
                                  const char* msg) {
    const char* level_str;
    FILE*       output;

    (void)ctx; /* Unused in default implementation */

    switch (level) {
        case MRTK_LOG_DEBUG:
            level_str = "[DEBUG]";
            output    = stdout;
            break;
        case MRTK_LOG_INFO:
            level_str = "[INFO] ";
            output    = stdout;
            break;
        case MRTK_LOG_WARN:
            level_str = "[WARN] ";
            output    = stdout;
            break;
        case MRTK_LOG_ERROR:
            level_str = "[ERROR]";
            output    = stderr;
            break;
        case MRTK_LOG_NONE:
        default:
            /* Should not reach here; MRTK_LOG_NONE messages are filtered earlier */
            return;
    }

    fprintf(output, "%s %s\n", level_str, msg);
}

/**
 * @brief Set the internal error message.
 *
 * Stores a formatted error message in the context for later retrieval
 * via mrtk_get_last_error(). Also logs the error message at ERROR level.
 *
 * @param ctx Context instance
 * @param fmt printf-style format string
 * @param ... Format arguments
 *
 * @note This is an internal function not exposed in the public API.
 *       Library modules use this to report errors.
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused, format(printf, 2, 3)))
#endif
static void mrtk_set_error(mrtk_context_t* ctx, const char* fmt, ...) {
    va_list args;

    if (ctx == NULL || fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(ctx->last_error, MAX_ERROR_LEN, fmt, args);
    va_end(args);

    /* Also log the error */
    mrtk_log(ctx, MRTK_LOG_ERROR, "%s", ctx->last_error);
}

/*============================================================================
 * Lifecycle Management Implementation
 *===========================================================================*/

/**
 * @brief Create a new library context.
 *
 * Allocates memory for a new context and initializes it with default values.
 * The default configuration is:
 * - log_level: MRTK_LOG_INFO
 * - log_callback: default_log_callback (stdout/stderr output)
 * - last_error: empty string
 * - user_data: NULL
 *
 * @return Newly allocated context, or NULL if memory allocation fails.
 */
mrtk_context_t* mrtk_context_new(void) {
    mrtk_context_t* ctx = (mrtk_context_t*)calloc(1, sizeof(mrtk_context_t));
    if (ctx == NULL) {
        return NULL;
    }

    /* Initialize with default settings */
    ctx->log_level     = MRTK_LOG_INFO;
    ctx->log_callback  = default_log_callback;
    ctx->last_error[0] = '\0';
    ctx->user_data     = NULL;

    return ctx;
}

/**
 * @brief Free the library context.
 *
 * Releases all memory associated with the context. Safe to call with NULL.
 *
 * @param ctx Context to free, or NULL (no-op)
 */
void mrtk_context_free(mrtk_context_t* ctx) {
    if (ctx != NULL) {
        /* Clear sensitive data before freeing (defense in depth) */
        memset(ctx, 0, sizeof(mrtk_context_t));
        free(ctx);
    }
}

/*============================================================================
 * Logging Configuration Implementation
 *===========================================================================*/

/**
 * @brief Set a custom log callback.
 *
 * @param ctx Context instance
 * @param cb  Callback function, or NULL to restore default logger
 */
void mrtk_context_set_log_callback(mrtk_context_t* ctx, mrtk_log_cb_t cb) {
    if (ctx == NULL) {
        return;
    }

    if (cb != NULL) {
        ctx->log_callback = cb;
    } else {
        /* Restore default logger when NULL is passed */
        ctx->log_callback = default_log_callback;
    }
}

/**
 * @brief Set the minimum log level.
 *
 * @param ctx   Context instance
 * @param level New minimum log level
 */
void mrtk_context_set_log_level(mrtk_context_t* ctx, mrtk_log_level_t level) {
    if (ctx != NULL) {
        ctx->log_level = level;
    }
}

/**
 * @brief Get the current log level.
 *
 * @param ctx Context instance
 * @return Current log level, or MRTK_LOG_NONE if ctx is NULL
 */
mrtk_log_level_t mrtk_context_get_log_level(const mrtk_context_t* ctx) {
    if (ctx == NULL) {
        return MRTK_LOG_NONE;
    }
    return ctx->log_level;
}

/**
 * @brief Log a formatted message using a pre-initialized va_list.
 *
 * This is the core formatting and dispatch function. Both mrtk_log() and
 * the legacy trace() bridge delegate to this function.
 *
 * @param ctx   Context instance
 * @param level Message severity level
 * @param fmt   printf-style format string
 * @param args  Pre-initialized va_list
 */
void mrtk_log_v(mrtk_context_t* ctx, mrtk_log_level_t level, const char* fmt,
                va_list args) {
    char buf[MAX_LOG_LEN];
    int  written;

    /* Validate parameters and check log level threshold */
    if (ctx == NULL || ctx->log_callback == NULL) {
        return;
    }

    /* Filter messages below the minimum level.
     * MRTK_LOG_NONE is the maximum sentinel value — setting log_level to NONE
     * disables all output, and passing level=NONE to this function is a no-op. */
    if (level < ctx->log_level || level == MRTK_LOG_NONE) {
        return;
    }

    /* Format the message */
    written = vsnprintf(buf, sizeof(buf), fmt, args);

    /* Ensure null termination on encoding error */
    if (written < 0) {
        buf[0] = '\0';
    }

    /* Dispatch to the registered callback */
    ctx->log_callback(ctx, level, buf);
}

/**
 * @brief Log a formatted message.
 *
 * Convenience wrapper around mrtk_log_v() that accepts variadic arguments.
 *
 * @param ctx   Context instance
 * @param level Message severity level
 * @param fmt   printf-style format string
 * @param ...   Format arguments
 */
void mrtk_log(mrtk_context_t* ctx, mrtk_log_level_t level, const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    mrtk_log_v(ctx, level, fmt, args);
    va_end(args);
}

/*============================================================================
 * Error Handling Implementation
 *===========================================================================*/

/**
 * @brief Get the last error message.
 *
 * @param ctx Context instance
 * @return Error message string, or "Invalid context" if ctx is NULL
 */
const char* mrtk_get_last_error(const mrtk_context_t* ctx) {
    if (ctx == NULL) {
        return "Invalid context";
    }
    return ctx->last_error;
}

/**
 * @brief Clear the last error message.
 *
 * @param ctx Context instance
 */
void mrtk_clear_error(mrtk_context_t* ctx) {
    if (ctx != NULL) {
        ctx->last_error[0] = '\0';
    }
}

/*============================================================================
 * User Data Management Implementation
 *===========================================================================*/

/**
 * @brief Set user-defined data.
 *
 * @param ctx       Context instance
 * @param user_data Pointer to associate with context
 */
void mrtk_context_set_user_data(mrtk_context_t* ctx, void* user_data) {
    if (ctx != NULL) {
        ctx->user_data = user_data;
    }
}

/**
 * @brief Get user-defined data.
 *
 * @param ctx Context instance
 * @return Associated user data, or NULL if ctx is NULL or no data was set
 */
void* mrtk_context_get_user_data(const mrtk_context_t* ctx) {
    if (ctx == NULL) {
        return NULL;
    }
    return ctx->user_data;
}

/*============================================================================
 * Version Information Implementation
 *===========================================================================*/

/**
 * @brief Get the library version string.
 *
 * @return Static version string (e.g., "0.1.0")
 */
const char* mrtk_version_string(void) {
    return MRTKLIB_VERSION_STRING;
}
