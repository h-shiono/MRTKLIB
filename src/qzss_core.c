/**
 * @file qzss_core.c
 * @brief Core implementation of QZSSLIB context management and logging.
 *
 * This file contains the implementation of the opaque context structure
 * and all associated lifecycle, logging, and error handling functions.
 */

#include "qzsslib/qzsslib.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
struct qzss_context_s {
    qzss_log_level_t log_level;             /**< Minimum log level threshold */
    qzss_log_cb_t    log_callback;          /**< User-defined log handler */
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
static void default_log_callback(qzss_context_t* ctx, qzss_log_level_t level,
                                  const char* msg) {
    const char* level_str;
    FILE*       output;

    (void)ctx; /* Unused in default implementation */

    switch (level) {
        case QZSS_LOG_DEBUG:
            level_str = "[DEBUG]";
            output    = stdout;
            break;
        case QZSS_LOG_INFO:
            level_str = "[INFO] ";
            output    = stdout;
            break;
        case QZSS_LOG_WARN:
            level_str = "[WARN] ";
            output    = stdout;
            break;
        case QZSS_LOG_ERROR:
            level_str = "[ERROR]";
            output    = stderr;
            break;
        case QZSS_LOG_NONE:
        default:
            /* Should not reach here; QZSS_LOG_NONE messages are filtered earlier */
            return;
    }

    fprintf(output, "%s %s\n", level_str, msg);
}

/**
 * @brief Set the internal error message.
 *
 * Stores a formatted error message in the context for later retrieval
 * via qzss_get_last_error(). Also logs the error message at ERROR level.
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
static void qzss_set_error(qzss_context_t* ctx, const char* fmt, ...) {
    va_list args;

    if (ctx == NULL || fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(ctx->last_error, MAX_ERROR_LEN, fmt, args);
    va_end(args);

    /* Also log the error */
    qzss_log(ctx, QZSS_LOG_ERROR, "%s", ctx->last_error);
}

/*============================================================================
 * Lifecycle Management Implementation
 *===========================================================================*/

/**
 * @brief Create a new library context.
 *
 * Allocates memory for a new context and initializes it with default values.
 * The default configuration is:
 * - log_level: QZSS_LOG_INFO
 * - log_callback: default_log_callback (stdout/stderr output)
 * - last_error: empty string
 * - user_data: NULL
 *
 * @return Newly allocated context, or NULL if memory allocation fails.
 */
qzss_context_t* qzss_context_new(void) {
    qzss_context_t* ctx = (qzss_context_t*)calloc(1, sizeof(qzss_context_t));
    if (ctx == NULL) {
        return NULL;
    }

    /* Initialize with default settings */
    ctx->log_level     = QZSS_LOG_INFO;
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
void qzss_context_free(qzss_context_t* ctx) {
    if (ctx != NULL) {
        /* Clear sensitive data before freeing (defense in depth) */
        memset(ctx, 0, sizeof(qzss_context_t));
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
void qzss_context_set_log_callback(qzss_context_t* ctx, qzss_log_cb_t cb) {
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
void qzss_context_set_log_level(qzss_context_t* ctx, qzss_log_level_t level) {
    if (ctx != NULL) {
        ctx->log_level = level;
    }
}

/**
 * @brief Get the current log level.
 *
 * @param ctx Context instance
 * @return Current log level, or QZSS_LOG_NONE if ctx is NULL
 */
qzss_log_level_t qzss_context_get_log_level(const qzss_context_t* ctx) {
    if (ctx == NULL) {
        return QZSS_LOG_NONE;
    }
    return ctx->log_level;
}

/**
 * @brief Log a formatted message.
 *
 * Formats the message and dispatches it to the registered callback
 * if the message level meets the minimum threshold.
 *
 * @param ctx   Context instance
 * @param level Message severity level
 * @param fmt   printf-style format string
 * @param ...   Format arguments
 */
void qzss_log(qzss_context_t* ctx, qzss_log_level_t level, const char* fmt, ...) {
    char    buf[MAX_LOG_LEN];
    va_list args;
    int     written;

    /* Validate parameters and check log level threshold */
    if (ctx == NULL || ctx->log_callback == NULL) {
        return;
    }

    /* Filter messages below the minimum level.
     * QZSS_LOG_NONE is the maximum sentinel value — setting log_level to NONE
     * disables all output, and passing level=NONE to this function is a no-op. */
    if (level < ctx->log_level || level == QZSS_LOG_NONE) {
        return;
    }

    /* Format the message */
    va_start(args, fmt);
    written = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /* Ensure null termination on encoding error */
    if (written < 0) {
        buf[0] = '\0';
    }

    /* Dispatch to the registered callback */
    ctx->log_callback(ctx, level, buf);
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
const char* qzss_get_last_error(const qzss_context_t* ctx) {
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
void qzss_clear_error(qzss_context_t* ctx) {
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
void qzss_context_set_user_data(qzss_context_t* ctx, void* user_data) {
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
void* qzss_context_get_user_data(const qzss_context_t* ctx) {
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
 * @return Static version string (e.g., "1.2.0")
 */
const char* qzss_version_string(void) {
    return QZSS_VERSION_STRING;
}
