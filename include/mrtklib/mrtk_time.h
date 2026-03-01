/*------------------------------------------------------------------------------
 * mrtk_time.h : time and date type definitions and functions
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
 * @file mrtk_time.h
 * @brief MRTKLIB Time Module — GNSS time types and conversion functions.
 *
 * This header provides the fundamental time type (gtime_t) and all
 * conversion functions between calendar, GPS, Galileo, BeiDou, and UTC
 * time representations.  It is the first module extracted from the legacy
 * rtklib.h monolith.
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from rtkcmn.c with zero algorithmic changes.
 */
#ifndef MRTK_TIME_H
#define MRTK_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include <time.h>

/*============================================================================
 * Time Type
 *===========================================================================*/

/**
 * @brief GNSS time representation.
 *
 * Combines POSIX time_t (integer seconds since 1970-01-01 UTC) with a
 * sub-second fractional part for high-precision timing.
 */
typedef struct {
    time_t time;    /**< time (s) expressed by standard time_t */
    double sec;     /**< fraction of second under 1 s */
} gtime_t;

/*============================================================================
 * Calendar / Epoch Conversions
 *===========================================================================*/

/**
 * @brief Convert substring in string to gtime_t struct.
 * @param[in]  s  String ("... yyyy mm dd hh mm ss ...")
 * @param[in]  i  Substring position
 * @param[in]  n  Substring width
 * @param[out] t  Resulting gtime_t struct
 * @return Status (0:ok, <0:error)
 */
int str2time(const char *s, int i, int n, gtime_t *t);

/**
 * @brief Convert gtime_t struct to string.
 * @param[in]  t    gtime_t struct
 * @param[out] str  String ("yyyy/mm/dd hh:mm:ss.ssss")
 * @param[in]  n    Number of decimals
 */
void time2str(gtime_t t, char *str, int n);

/**
 * @brief Convert calendar day/time to gtime_t struct.
 * @param[in] ep  Day/time {year, month, day, hour, min, sec}
 * @return gtime_t struct
 */
gtime_t epoch2time(const double *ep);

/**
 * @brief Convert gtime_t struct to calendar day/time.
 * @param[in]  t   gtime_t struct
 * @param[out] ep  Day/time {year, month, day, hour, min, sec}
 */
void time2epoch(gtime_t t, double *ep);

/**
 * @brief Get time string (not reentrant).
 * @param[in] t  gtime_t struct
 * @param[in] n  Number of decimals
 * @return Static time string
 */
char *time_str(gtime_t t, int n);

/*============================================================================
 * GNSS Time System Conversions
 *===========================================================================*/

/**
 * @brief Convert GPS week/tow to gtime_t.
 * @param[in] week  GPS week number
 * @param[in] sec   Time of week (s)
 * @return gtime_t struct
 */
gtime_t gpst2time(int week, double sec);

/**
 * @brief Convert gtime_t to GPS week/tow.
 * @param[in]  t     gtime_t struct
 * @param[out] week  GPS week number (NULL: no output)
 * @return Time of week in GPS time (s)
 */
double time2gpst(gtime_t t, int *week);

/**
 * @brief Convert Galileo system time week/tow to gtime_t.
 * @param[in] week  GST week number
 * @param[in] sec   Time of week (s)
 * @return gtime_t struct
 */
gtime_t gst2time(int week, double sec);

/**
 * @brief Convert gtime_t to Galileo system time week/tow.
 * @param[in]  t     gtime_t struct
 * @param[out] week  GST week number (NULL: no output)
 * @return Time of week in GST (s)
 */
double time2gst(gtime_t t, int *week);

/**
 * @brief Convert BeiDou time week/tow to gtime_t.
 * @param[in] week  BDT week number
 * @param[in] sec   Time of week (s)
 * @return gtime_t struct
 */
gtime_t bdt2time(int week, double sec);

/**
 * @brief Convert gtime_t to BeiDou time week/tow.
 * @param[in]  t     gtime_t struct
 * @param[out] week  BDT week number (NULL: no output)
 * @return Time of week in BDT (s)
 */
double time2bdt(gtime_t t, int *week);

/*============================================================================
 * Time Arithmetic
 *===========================================================================*/

/**
 * @brief Add seconds to gtime_t.
 * @param[in] t    gtime_t struct
 * @param[in] sec  Seconds to add
 * @return t + sec
 */
gtime_t timeadd(gtime_t t, double sec);

/**
 * @brief Compute time difference (t1 - t2).
 * @param[in] t1  gtime_t struct
 * @param[in] t2  gtime_t struct
 * @return Difference in seconds (t1 - t2)
 */
double timediff(gtime_t t1, gtime_t t2);

/*============================================================================
 * Leap-Second-Aware Conversions
 *===========================================================================*/

/**
 * @brief Convert GPS time to UTC considering leap seconds.
 * @param[in] t  Time in GPS time
 * @return Time in UTC
 */
gtime_t gpst2utc(gtime_t t);

/**
 * @brief Convert UTC to GPS time considering leap seconds.
 * @param[in] t  Time in UTC
 * @return Time in GPS time
 */
gtime_t utc2gpst(gtime_t t);

/**
 * @brief Convert GPS time to BeiDou time.
 * @param[in] t  Time in GPS time
 * @return Time in BDT
 */
gtime_t gpst2bdt(gtime_t t);

/**
 * @brief Convert BeiDou time to GPS time.
 * @param[in] t  Time in BDT
 * @return Time in GPS time
 */
gtime_t bdt2gpst(gtime_t t);

/**
 * @brief Read leap seconds table from file.
 * @param[in] file  Leap seconds table file path
 * @return Status (1:ok, 0:error)
 */
int read_leaps(const char *file);

/*============================================================================
 * System Clock and Utilities
 *===========================================================================*/

/**
 * @brief Get current time in UTC.
 * @return Current time in UTC
 */
gtime_t timeget(void);

/**
 * @brief Set current time offset in UTC.
 * @param[in] t  Current time in UTC
 */
void timeset(gtime_t t);

/**
 * @brief Reset time offset to zero.
 */
void timereset(void);

/**
 * @brief Convert time to day of year.
 * @param[in] t  gtime_t struct
 * @return Day of year (days)
 */
double time2doy(gtime_t t);

/**
 * @brief Convert UTC to Greenwich Mean Sidereal Time.
 * @param[in] t        Time in UTC
 * @param[in] ut1_utc  UT1-UTC correction (s)
 * @return GMST in radians
 */
double utc2gmst(gtime_t t, double ut1_utc);

/**
 * @brief Adjust GPS week number using CPU time.
 * @param[in] week  Not-adjusted GPS week number (0-1023)
 * @return Adjusted GPS week number
 */
int adjgpsweek(int week);

/**
 * @brief Get current tick in milliseconds.
 * @return Current tick (ms)
 */
uint32_t tickget(void);

/**
 * @brief Sleep for specified milliseconds.
 * @param[in] ms  Milliseconds to sleep (<0: no sleep)
 */
void sleepms(int ms);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_TIME_H */
