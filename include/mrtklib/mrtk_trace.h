/**
 * @file mrtk_trace.h
 * @brief MRTKLIB Trace Module — debug trace and logging functions.
 *
 * This header declares the legacy RTKLIB trace functions (traceopen, trace,
 * tracet, tracemat, traceobs, tracenav, etc.) that bridge into the modern
 * mrtk_log() system via g_mrtk_legacy_ctx.
 *
 * @note Functions declared here are implemented in mrtk_trace.c.
 */
#ifndef MRTK_TRACE_H
#define MRTK_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"

/*============================================================================
 * Debug Trace Functions
 *===========================================================================*/

/**
 * @brief Open trace output (initializes tick counter for tracet).
 * @param[in] file  Trace file path (ignored in new logging system)
 */
void traceopen(const char *file);

/**
 * @brief Close trace output.
 */
void traceclose(void);

/**
 * @brief Set trace level.
 * @param[in] level  Trace level (0:off, 1:error, 2:warn, 3:info, 4-5:debug)
 */
void tracelevel(int level);

/**
 * @brief Output trace message.
 * @param[in] level   Trace level
 * @param[in] format  printf-style format string
 * @param[in] ...     Format arguments
 */
void trace(int level, const char *format, ...);

/**
 * @brief Output trace message with elapsed time.
 * @param[in] level   Trace level
 * @param[in] format  printf-style format string
 * @param[in] ...     Format arguments
 */
void tracet(int level, const char *format, ...);

/**
 * @brief Output trace matrix.
 * @param[in] level  Trace level
 * @param[in] A      Matrix data (column-major)
 * @param[in] n      Number of rows
 * @param[in] m      Number of columns
 * @param[in] p      Total field width
 * @param[in] q      Decimal places
 */
void tracemat(int level, const double *A, int n, int m, int p, int q);

/**
 * @brief Output trace observation data.
 * @param[in] level  Trace level
 * @param[in] obs    Observation data array
 * @param[in] n      Number of observations
 */
void traceobs(int level, const obsd_t *obs, int n);

/**
 * @brief Output trace navigation data (GPS ephemeris).
 * @param[in] level  Trace level
 * @param[in] nav    Navigation data
 */
void tracenav(int level, const nav_t *nav);

/**
 * @brief Output trace GLONASS navigation data.
 * @param[in] level  Trace level
 * @param[in] nav    Navigation data
 */
void tracegnav(int level, const nav_t *nav);

/**
 * @brief Output trace SBAS navigation data.
 * @param[in] level  Trace level
 * @param[in] nav    Navigation data
 */
void tracehnav(int level, const nav_t *nav);

/**
 * @brief Output trace precise ephemeris data.
 * @param[in] level  Trace level
 * @param[in] nav    Navigation data
 */
void tracepeph(int level, const nav_t *nav);

/**
 * @brief Output trace precise clock data.
 * @param[in] level  Trace level
 * @param[in] nav    Navigation data
 */
void tracepclk(int level, const nav_t *nav);

/**
 * @brief Output trace binary data.
 * @param[in] level  Trace level
 * @param[in] p      Binary data
 * @param[in] n      Data length (bytes)
 */
void traceb(int level, const uint8_t *p, int n);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_TRACE_H */
