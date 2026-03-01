/*------------------------------------------------------------------------------
 * mrtklib.h : MRTKLIB public API for context management and logging
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
 * @file mrtklib.h
 * @brief MRTKLIB Public API — Context Management and Logging
 *
 * This header defines the public API for MRTKLIB, a modernized and refactored
 * version of RTKLIB targeting JAXA QZSS/MADOCA processing. The library is
 * designed to be:
 * - Thread-safe through context-based state management
 * - Re-entrant, allowing multiple simultaneous processing instances
 * - Library-first with no global state
 *
 * @note All public functions require a valid mrtk_context_t pointer unless
 *       otherwise specified.
 */
#ifndef MRTKLIB_H
#define MRTKLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/*============================================================================
 * Version Information
 *===========================================================================*/

#include "mrtklib/mrtk_version.h"

/*============================================================================
 * Opaque Context
 *===========================================================================*/

/**
 * @brief Opaque library context structure.
 *
 * Holds all library state including logging configuration, error state,
 * and user-defined data. This structure is intentionally opaque to allow
 * internal changes without breaking API compatibility.
 *
 * @note Users must create contexts via mrtk_context_new() and free them
 *       via mrtk_context_free(). Never allocate this structure directly.
 */
typedef struct mrtk_context_s mrtk_context_t;

/*============================================================================
 * Logging
 *===========================================================================*/

/**
 * @brief Log severity levels.
 *
 * Levels are ordered by severity (DEBUG < INFO < WARN < ERROR < NONE).
 * Setting a minimum log level filters out all messages below that severity.
 * MRTK_LOG_NONE disables all output.
 */
typedef enum {
    MRTK_LOG_DEBUG = 0, /**< Detailed debug information for development */
    MRTK_LOG_INFO,      /**< General informational messages */
    MRTK_LOG_WARN,      /**< Warning conditions that may require attention */
    MRTK_LOG_ERROR,     /**< Error conditions that affect processing */
    MRTK_LOG_NONE       /**< Sentinel — disables all logging output */
} mrtk_log_level_t;

/**
 * @brief Log callback function signature.
 *
 * Custom log handlers must conform to this signature. The callback is invoked
 * for each log message that passes the minimum log level filter.
 *
 * @param ctx   Context instance that generated the log message
 * @param level Severity level of the message
 * @param msg   Null-terminated formatted message string
 *
 * @note The msg pointer is only valid during the callback invocation.
 *       Copy the string if you need to retain it.
 * @note The callback is invoked synchronously from the calling thread.
 */
typedef void (*mrtk_log_cb_t)(mrtk_context_t* ctx, mrtk_log_level_t level,
                               const char* msg);

/*============================================================================
 * Lifecycle Management
 *===========================================================================*/

/**
 * @brief Create a new library context.
 *
 * Allocates and initializes a new context with default settings:
 * - Log level:    MRTK_LOG_INFO
 * - Log callback: Default stdout/stderr logger
 * - User data:    NULL
 *
 * @return Pointer to newly created context, or NULL if allocation fails.
 *
 * @note The caller is responsible for freeing the context via mrtk_context_free().
 *
 * @code{.c}
 * mrtk_context_t* ctx = mrtk_context_new();
 * if (ctx == NULL) { ... }
 * // ... use context ...
 * mrtk_context_free(ctx);
 * @endcode
 */
mrtk_context_t* mrtk_context_new(void);

/**
 * @brief Free the library context and release all associated resources.
 *
 * Safely frees a context created by mrtk_context_new(). After this call,
 * the context pointer is invalid and must not be used.
 *
 * @param ctx Context to free. If NULL, this function does nothing.
 */
void mrtk_context_free(mrtk_context_t* ctx);

/*============================================================================
 * Logging Configuration
 *===========================================================================*/

/**
 * @brief Set a custom log callback handler.
 *
 * Replaces the default logging behavior with a custom callback. This allows
 * integration with external logging systems, GUI log windows, or file-based
 * logging.
 *
 * @param ctx Context instance
 * @param cb  Callback function to handle log messages. Pass NULL to restore
 *            the default stdout/stderr logger.
 */
void mrtk_context_set_log_callback(mrtk_context_t* ctx, mrtk_log_cb_t cb);

/**
 * @brief Set the minimum log level threshold.
 *
 * Messages below this severity level will be silently discarded.
 * Setting MRTK_LOG_NONE disables all logging output.
 *
 * @param ctx   Context instance
 * @param level Minimum level to log (inclusive)
 */
void mrtk_context_set_log_level(mrtk_context_t* ctx, mrtk_log_level_t level);

/**
 * @brief Get the current log level threshold.
 *
 * @param ctx Context instance
 * @return Current minimum log level, or MRTK_LOG_NONE if ctx is NULL.
 */
mrtk_log_level_t mrtk_context_get_log_level(const mrtk_context_t* ctx);

/**
 * @brief Log a formatted message.
 *
 * Formats and dispatches a log message through the registered callback
 * if the message level meets or exceeds the minimum threshold.
 *
 * @param ctx   Context instance
 * @param level Severity level of this message
 * @param fmt   printf-style format string
 * @param ...   Format arguments
 */
void mrtk_log(mrtk_context_t* ctx, mrtk_log_level_t level, const char* fmt, ...);

/**
 * @brief Log a formatted message using a pre-initialized va_list.
 *
 * This is the va_list variant of mrtk_log(), intended for use by wrapper
 * functions (such as the legacy RTKLIB trace() bridge) that receive variadic
 * arguments from their own callers.
 *
 * @param ctx   Context instance
 * @param level Severity level of this message
 * @param fmt   printf-style format string
 * @param args  Pre-initialized va_list
 */
void mrtk_log_v(mrtk_context_t* ctx, mrtk_log_level_t level, const char* fmt,
                va_list args);

/*============================================================================
 * Legacy Migration Support
 *===========================================================================*/

/**
 * @brief Global context for legacy RTKLIB trace() bridge.
 *
 * During the migration period, this global pointer allows the existing
 * trace() / tracet() functions in rtkcmn.c to route their output through
 * the new context-based logging system without modifying thousands of
 * call sites.
 *
 * @note Applications must set this to mrtk_context_new() at startup and
 *       free it with mrtk_context_free() at shutdown.
 * @note This pointer is intentionally global and will be removed once
 *       all modules have been migrated to context-based APIs.
 */
extern mrtk_context_t* g_mrtk_legacy_ctx;

/*============================================================================
 * Error Handling
 *===========================================================================*/

/**
 * @brief Get the last error message.
 *
 * Retrieves the most recent error message stored in the context by an
 * internal library function. Returns an empty string if no error has occurred.
 *
 * @param ctx Context instance
 * @return Pointer to the error message string, or "Invalid context" if ctx is NULL.
 *
 * @note The returned pointer remains valid until the next error occurs or
 *       the context is freed.
 */
const char* mrtk_get_last_error(const mrtk_context_t* ctx);

/**
 * @brief Clear the last error message.
 *
 * Resets the error state, clearing any previously stored error message.
 *
 * @param ctx Context instance
 */
void mrtk_clear_error(mrtk_context_t* ctx);

/*============================================================================
 * User Data Management
 *===========================================================================*/

/**
 * @brief Associate user-defined data with the context.
 *
 * Stores an arbitrary pointer that can be retrieved later. Useful for
 * passing application-specific state through callbacks.
 *
 * @param ctx       Context instance
 * @param user_data Pointer to user data (may be NULL)
 *
 * @note The library does not manage the lifetime of user_data.
 */
void mrtk_context_set_user_data(mrtk_context_t* ctx, void* user_data);

/**
 * @brief Retrieve user-defined data from the context.
 *
 * @param ctx Context instance
 * @return Previously stored user data pointer, or NULL if none was set
 *         or if ctx is NULL.
 */
void* mrtk_context_get_user_data(const mrtk_context_t* ctx);

/*============================================================================
 * Version Information
 *===========================================================================*/

/**
 * @brief Get the library version string.
 *
 * @return Static string containing the version (e.g., "1.2.0")
 */
const char* mrtk_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* MRTKLIB_H */
