/*------------------------------------------------------------------------------
 * mrtk_time.c : time and date functions
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
 * @file mrtk_time.c
 * @brief MRTKLIB Time Module — Implementation.
 *
 * All functions and static data in this file are cut-and-paste extractions
 * from the legacy rtkcmn.c with zero algorithmic changes.
 */

#include "mrtklib/mrtk_time.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

/*============================================================================
 * Private Constants
 *===========================================================================*/

#ifndef PI
#define PI 3.1415926535897932 /* pi */
#endif

#define MAXLEAPS 64 /* max number of leap seconds table */

/** @brief GPS time reference epoch */
static const double gpst0[] = {1980, 1, 6, 0, 0, 0};
/** @brief Galileo system time reference epoch */
static const double gst0[] = {1999, 8, 22, 0, 0, 0};
/** @brief BeiDou time reference epoch */
static const double bdt0[] = {2006, 1, 1, 0, 0, 0};

/** @brief Leap seconds table (y,m,d,h,m,s,utc-gpst) */
static double leaps[MAXLEAPS + 1][7] = {{2017, 1, 1, 0, 0, 0, -18},
                                        {2015, 7, 1, 0, 0, 0, -17},
                                        {2012, 7, 1, 0, 0, 0, -16},
                                        {2009, 1, 1, 0, 0, 0, -15},
                                        {2006, 1, 1, 0, 0, 0, -14},
                                        {1999, 1, 1, 0, 0, 0, -13},
                                        {1997, 7, 1, 0, 0, 0, -12},
                                        {1996, 1, 1, 0, 0, 0, -11},
                                        {1994, 7, 1, 0, 0, 0, -10},
                                        {1993, 7, 1, 0, 0, 0, -9},
                                        {1992, 7, 1, 0, 0, 0, -8},
                                        {1991, 1, 1, 0, 0, 0, -7},
                                        {1990, 1, 1, 0, 0, 0, -6},
                                        {1988, 1, 1, 0, 0, 0, -5},
                                        {1985, 7, 1, 0, 0, 0, -4},
                                        {1983, 7, 1, 0, 0, 0, -3},
                                        {1982, 7, 1, 0, 0, 0, -2},
                                        {1981, 7, 1, 0, 0, 0, -1},
                                        {0}};

/*============================================================================
 * Private Variables
 *===========================================================================*/

/** @brief Time offset (s) for timeget/timeset */
static double timeoffset_ = 0.0;

/*============================================================================
 * Calendar / Epoch Conversions
 *===========================================================================*/

/* string to time --------------------------------------------------------------
 * convert substring in string to gtime_t struct
 * args   : char   *s        I   string ("... yyyy mm dd hh mm ss ...")
 *          int    i,n       I   substring position and width
 *          gtime_t *t       O   gtime_t struct
 * return : status (0:ok,0>:error)
 *-----------------------------------------------------------------------------*/
extern int str2time(const char* s, int i, int n, gtime_t* t) {
    double ep[6];
    char str[256], *p = str;

    if (i < 0 || (int)strlen(s) < i || (int)sizeof(str) - 1 < i) {
        return -1;
    }
    for (s += i; *s && --n >= 0;) {
        *p++ = *s++;
    }
    *p = '\0';
    if (sscanf(str, "%lf %lf %lf %lf %lf %lf", ep, ep + 1, ep + 2, ep + 3, ep + 4, ep + 5) < 6) {
        return -1;
    }
    if (ep[0] < 100.0) {
        ep[0] += ep[0] < 80.0 ? 2000.0 : 1900.0;
    }
    *t = epoch2time(ep);
    return 0;
}
/* convert calendar day/time to time -------------------------------------------
 * convert calendar day/time to gtime_t struct
 * args   : double *ep       I   day/time {year,month,day,hour,min,sec}
 * return : gtime_t struct
 * notes  : proper in 1970-2037 or 1970-2099 (64bit time_t)
 *-----------------------------------------------------------------------------*/
extern gtime_t epoch2time(const double* ep) {
    const int doy[] = {1, 32, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
    gtime_t time = {0};
    int days, sec, year = (int)ep[0], mon = (int)ep[1], day = (int)ep[2];

    if (year < 1970 || 2099 < year || mon < 1 || 12 < mon) {
        return time;
    }

    /* leap year if year%4==0 in 1901-2099 */
    days = (year - 1970) * 365 + (year - 1969) / 4 + doy[mon - 1] + day - 2 + (year % 4 == 0 && mon >= 3 ? 1 : 0);
    sec = (int)floor(ep[5]);
    time.time = (time_t)days * 86400 + (int)ep[3] * 3600 + (int)ep[4] * 60 + sec;
    time.sec = ep[5] - sec;
    return time;
}
/* time to calendar day/time ---------------------------------------------------
 * convert gtime_t struct to calendar day/time
 * args   : gtime_t t        I   gtime_t struct
 *          double *ep       O   day/time {year,month,day,hour,min,sec}
 * return : none
 * notes  : proper in 1970-2037 or 1970-2099 (64bit time_t)
 *-----------------------------------------------------------------------------*/
extern void time2epoch(gtime_t t, double* ep) {
    const int mday[] = {/* # of days in a month */
                        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
                        31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int days, sec, mon, day;

    /* leap year if year%4==0 in 1901-2099 */
    days = (int)(t.time / 86400);
    sec = (int)(t.time - (time_t)days * 86400);
    for (day = days % 1461, mon = 0; mon < 48; mon++) {
        if (day >= mday[mon]) {
            day -= mday[mon];
        } else {
            break;
        }
    }
    ep[0] = 1970 + days / 1461 * 4 + mon / 12;
    ep[1] = mon % 12 + 1;
    ep[2] = day + 1;
    ep[3] = sec / 3600;
    ep[4] = sec % 3600 / 60;
    ep[5] = sec % 60 + t.sec;
}
/* gps time to time ------------------------------------------------------------
 * convert week and tow in gps time to gtime_t struct
 * args   : int    week      I   week number in gps time
 *          double sec       I   time of week in gps time (s)
 * return : gtime_t struct
 *-----------------------------------------------------------------------------*/
extern gtime_t gpst2time(int week, double sec) {
    gtime_t t = epoch2time(gpst0);

    if (sec < -1E9 || 1E9 < sec) {
        sec = 0.0;
    }
    t.time += (time_t)86400 * 7 * week + (int)sec;
    t.sec = sec - (int)sec;
    return t;
}
/* time to gps time ------------------------------------------------------------
 * convert gtime_t struct to week and tow in gps time
 * args   : gtime_t t        I   gtime_t struct
 *          int    *week     IO  week number in gps time (NULL: no output)
 * return : time of week in gps time (s)
 *-----------------------------------------------------------------------------*/
extern double time2gpst(gtime_t t, int* week) {
    gtime_t t0 = epoch2time(gpst0);
    time_t sec = t.time - t0.time;
    int w = (int)(sec / (86400 * 7));

    if (week) {
        *week = w;
    }
    return (double)(sec - (double)w * 86400 * 7) + t.sec;
}
/* galileo system time to time -------------------------------------------------
 * convert week and tow in galileo system time (gst) to gtime_t struct
 * args   : int    week      I   week number in gst
 *          double sec       I   time of week in gst (s)
 * return : gtime_t struct
 *-----------------------------------------------------------------------------*/
extern gtime_t gst2time(int week, double sec) {
    gtime_t t = epoch2time(gst0);

    if (sec < -1E9 || 1E9 < sec) {
        sec = 0.0;
    }
    t.time += (time_t)86400 * 7 * week + (int)sec;
    t.sec = sec - (int)sec;
    return t;
}
/* time to galileo system time -------------------------------------------------
 * convert gtime_t struct to week and tow in galileo system time (gst)
 * args   : gtime_t t        I   gtime_t struct
 *          int    *week     IO  week number in gst (NULL: no output)
 * return : time of week in gst (s)
 *-----------------------------------------------------------------------------*/
extern double time2gst(gtime_t t, int* week) {
    gtime_t t0 = epoch2time(gst0);
    time_t sec = t.time - t0.time;
    int w = (int)(sec / (86400 * 7));

    if (week) {
        *week = w;
    }
    return (double)(sec - (double)w * 86400 * 7) + t.sec;
}
/* beidou time (bdt) to time ---------------------------------------------------
 * convert week and tow in beidou time (bdt) to gtime_t struct
 * args   : int    week      I   week number in bdt
 *          double sec       I   time of week in bdt (s)
 * return : gtime_t struct
 *-----------------------------------------------------------------------------*/
extern gtime_t bdt2time(int week, double sec) {
    gtime_t t = epoch2time(bdt0);

    if (sec < -1E9 || 1E9 < sec) {
        sec = 0.0;
    }
    t.time += (time_t)86400 * 7 * week + (int)sec;
    t.sec = sec - (int)sec;
    return t;
}
/* time to beidouo time (bdt) --------------------------------------------------
 * convert gtime_t struct to week and tow in beidou time (bdt)
 * args   : gtime_t t        I   gtime_t struct
 *          int    *week     IO  week number in bdt (NULL: no output)
 * return : time of week in bdt (s)
 *-----------------------------------------------------------------------------*/
extern double time2bdt(gtime_t t, int* week) {
    gtime_t t0 = epoch2time(bdt0);
    time_t sec = t.time - t0.time;
    int w = (int)(sec / (86400 * 7));

    if (week) {
        *week = w;
    }
    return (double)(sec - (double)w * 86400 * 7) + t.sec;
}
/*============================================================================
 * Time Arithmetic
 *===========================================================================*/

/* add time --------------------------------------------------------------------
 * add time to gtime_t struct
 * args   : gtime_t t        I   gtime_t struct
 *          double sec       I   time to add (s)
 * return : gtime_t struct (t+sec)
 *-----------------------------------------------------------------------------*/
extern gtime_t timeadd(gtime_t t, double sec) {
    double tt;

    t.sec += sec;
    tt = floor(t.sec);
    t.time += (int)tt;
    t.sec -= tt;
    return t;
}
/* time difference -------------------------------------------------------------
 * difference between gtime_t structs
 * args   : gtime_t t1,t2    I   gtime_t structs
 * return : time difference (t1-t2) (s)
 *-----------------------------------------------------------------------------*/
extern double timediff(gtime_t t1, gtime_t t2) { return difftime(t1.time, t2.time) + t1.sec - t2.sec; }
/*============================================================================
 * System Clock and Utilities
 *===========================================================================*/

/* get current time in utc -----------------------------------------------------
 * get current time in utc
 * args   : none
 * return : current time in utc
 *-----------------------------------------------------------------------------*/
extern gtime_t timeget(void) {
    gtime_t time;
    double ep[6] = {0};
    struct timeval tv;
    struct tm* tt;

    if (!gettimeofday(&tv, NULL) && (tt = gmtime(&tv.tv_sec))) {
        ep[0] = tt->tm_year + 1900;
        ep[1] = tt->tm_mon + 1;
        ep[2] = tt->tm_mday;
        ep[3] = tt->tm_hour;
        ep[4] = tt->tm_min;
        ep[5] = tt->tm_sec + tv.tv_usec * 1E-6;
    }
    time = epoch2time(ep);

#ifdef CPUTIME_IN_GPST /* cputime operated in gpst */
    time = gpst2utc(time);
#endif
    return timeadd(time, timeoffset_);
}
/* set current time in utc -----------------------------------------------------
 * set current time in utc
 * args   : gtime_t          I   current time in utc
 * return : none
 * notes  : just set time offset between cpu time and current time
 *          the time offset is reflected to only timeget()
 *          not reentrant
 *-----------------------------------------------------------------------------*/
extern void timeset(gtime_t t) { timeoffset_ += timediff(t, timeget()); }
/* reset current time ----------------------------------------------------------
 * reset current time
 * args   : none
 * return : none
 *-----------------------------------------------------------------------------*/
extern void timereset(void) { timeoffset_ = 0.0; }
/*============================================================================
 * Leap-Second-Aware Conversions
 *===========================================================================*/

/* read leap seconds table by text -------------------------------------------*/
static int read_leaps_text(FILE* fp) {
    char buff[256], *p;
    int i, n = 0, ep[6], ls;

    rewind(fp);

    while (fgets(buff, sizeof(buff), fp) && n < MAXLEAPS) {
        if ((p = strchr(buff, '#'))) {
            *p = '\0';
        }
        if (sscanf(buff, "%d %d %d %d %d %d %d", ep, ep + 1, ep + 2, ep + 3, ep + 4, ep + 5, &ls) < 7) {
            continue;
        }
        for (i = 0; i < 6; i++) {
            leaps[n][i] = ep[i];
        }
        leaps[n++][6] = ls;
    }
    return n;
}
/* read leap seconds table by usno -------------------------------------------*/
static int read_leaps_usno(FILE* fp) {
    static const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    double jd, tai_utc;
    char buff[256], month[32], ls[MAXLEAPS][7] = {{0}};
    int i, j, y, m, d, n = 0;

    rewind(fp);

    while (fgets(buff, sizeof(buff), fp) && n < MAXLEAPS) {
        if (sscanf(buff, "%d %s %d =JD %lf TAI-UTC= %lf", &y, month, &d, &jd, &tai_utc) < 5) {
            continue;
        }
        if (y < 1980) {
            continue;
        }
        for (m = 1; m <= 12; m++) {
            if (!strcmp(months[m - 1], month)) {
                break;
            }
        }
        if (m >= 13) {
            continue;
        }
        ls[n][0] = y;
        ls[n][1] = m;
        ls[n][2] = d;
        ls[n++][6] = (char)(19.0 - tai_utc);
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < 7; j++) {
            leaps[i][j] = ls[n - i - 1][j];
        }
    }
    return n;
}
/* read leap seconds table -----------------------------------------------------
 * read leap seconds table
 * args   : char    *file    I   leap seconds table file
 * return : status (1:ok,0:error)
 * notes  : The leap second table should be as follows or leapsec.dat provided
 *          by USNO.
 *          (1) The records in the table file cosist of the following fields:
 *              year month day hour min sec UTC-GPST(s)
 *          (2) The date and time indicate the start UTC time for the UTC-GPST
 *          (3) The date and time should be descending order.
 *-----------------------------------------------------------------------------*/
extern int read_leaps(const char* file) {
    FILE* fp;
    int i, n;

    if (!(fp = fopen(file, "r"))) {
        return 0;
    }

    /* read leap seconds table by text or usno */
    if (!(n = read_leaps_text(fp)) && !(n = read_leaps_usno(fp))) {
        fclose(fp);
        return 0;
    }
    for (i = 0; i < 7; i++) {
        leaps[n][i] = 0.0;
    }
    fclose(fp);
    return 1;
}
/* gpstime to utc --------------------------------------------------------------
 * convert gpstime to utc considering leap seconds
 * args   : gtime_t t        I   time expressed in gpstime
 * return : time expressed in utc
 * notes  : ignore slight time offset under 100 ns
 *-----------------------------------------------------------------------------*/
extern gtime_t gpst2utc(gtime_t t) {
    gtime_t tu;
    int i;

    for (i = 0; leaps[i][0] > 0; i++) {
        tu = timeadd(t, leaps[i][6]);
        if (timediff(tu, epoch2time(leaps[i])) >= 0.0) {
            return tu;
        }
    }
    return t;
}
/* utc to gpstime --------------------------------------------------------------
 * convert utc to gpstime considering leap seconds
 * args   : gtime_t t        I   time expressed in utc
 * return : time expressed in gpstime
 * notes  : ignore slight time offset under 100 ns
 *-----------------------------------------------------------------------------*/
extern gtime_t utc2gpst(gtime_t t) {
    int i;

    for (i = 0; leaps[i][0] > 0; i++) {
        if (timediff(t, epoch2time(leaps[i])) >= 0.0) {
            return timeadd(t, -leaps[i][6]);
        }
    }
    return t;
}
/* gpstime to bdt --------------------------------------------------------------
 * convert gpstime to bdt (beidou navigation satellite system time)
 * args   : gtime_t t        I   time expressed in gpstime
 * return : time expressed in bdt
 * notes  : ref [8] 3.3, 2006/1/1 00:00 BDT = 2006/1/1 00:00 UTC
 *          no leap seconds in BDT
 *          ignore slight time offset under 100 ns
 *-----------------------------------------------------------------------------*/
extern gtime_t gpst2bdt(gtime_t t) { return timeadd(t, -14.0); }
/* bdt to gpstime --------------------------------------------------------------
 * convert bdt (beidou navigation satellite system time) to gpstime
 * args   : gtime_t t        I   time expressed in bdt
 * return : time expressed in gpstime
 * notes  : see gpst2bdt()
 *-----------------------------------------------------------------------------*/
extern gtime_t bdt2gpst(gtime_t t) { return timeadd(t, 14.0); }
/*============================================================================
 * Derived Time Quantities
 *===========================================================================*/

/* time to day and sec -------------------------------------------------------*/
static double time2sec(gtime_t time, gtime_t* day) {
    double ep[6], sec;
    time2epoch(time, ep);
    sec = ep[3] * 3600.0 + ep[4] * 60.0 + ep[5];
    ep[3] = ep[4] = ep[5] = 0.0;
    *day = epoch2time(ep);
    return sec;
}
/* utc to gmst -----------------------------------------------------------------
 * convert utc to gmst (Greenwich mean sidereal time)
 * args   : gtime_t t        I   time expressed in utc
 *          double ut1_utc   I   UT1-UTC (s)
 * return : gmst (rad)
 *-----------------------------------------------------------------------------*/
extern double utc2gmst(gtime_t t, double ut1_utc) {
    const double ep2000[] = {2000, 1, 1, 12, 0, 0};
    gtime_t tut, tut0;
    double ut, t1, t2, t3, gmst0, gmst;

    tut = timeadd(t, ut1_utc);
    ut = time2sec(tut, &tut0);
    t1 = timediff(tut0, epoch2time(ep2000)) / 86400.0 / 36525.0;
    t2 = t1 * t1;
    t3 = t2 * t1;
    gmst0 = 24110.54841 + 8640184.812866 * t1 + 0.093104 * t2 - 6.2E-6 * t3;
    gmst = gmst0 + 1.002737909350795 * ut;

    return fmod(gmst, 86400.0) * PI / 43200.0; /* 0 <= gmst <= 2*PI */
}
/* time to string --------------------------------------------------------------
 * convert gtime_t struct to string
 * args   : gtime_t t        I   gtime_t struct
 *          char   *s        O   string ("yyyy/mm/dd hh:mm:ss.ssss")
 *          int    n         I   number of decimals
 * return : none
 *-----------------------------------------------------------------------------*/
extern void time2str(gtime_t t, char* s, int n) {
    double ep[6];

    if (n < 0) {
        n = 0;
    } else if (n > 12) {
        n = 12;
    }
    if (1.0 - t.sec < 0.5 / pow(10.0, n)) {
        t.time++;
        t.sec = 0.0;
    }
    time2epoch(t, ep);
    sprintf(s, "%04.0f/%02.0f/%02.0f %02.0f:%02.0f:%0*.*f", ep[0], ep[1], ep[2], ep[3], ep[4], n <= 0 ? 2 : n + 3,
            n <= 0 ? 0 : n, ep[5]);
}
/* get time string -------------------------------------------------------------
 * get time string
 * args   : gtime_t t        I   gtime_t struct
 *          int    n         I   number of decimals
 * return : time string
 * notes  : not reentrant, do not use multiple in a function
 *-----------------------------------------------------------------------------*/
extern char* time_str(gtime_t t, int n) {
    static char buff[64];
    time2str(t, buff, n);
    return buff;
}
/* time to day of year ---------------------------------------------------------
 * convert time to day of year
 * args   : gtime_t t        I   gtime_t struct
 * return : day of year (days)
 *-----------------------------------------------------------------------------*/
extern double time2doy(gtime_t t) {
    double ep[6];

    time2epoch(t, ep);
    ep[1] = ep[2] = 1.0;
    ep[3] = ep[4] = ep[5] = 0.0;
    return timediff(t, epoch2time(ep)) / 86400.0 + 1.0;
}
/* adjust gps week number ------------------------------------------------------
 * adjust gps week number using cpu time
 * args   : int   week       I   not-adjusted gps week number (0-1023)
 * return : adjusted gps week number
 *-----------------------------------------------------------------------------*/
extern int adjgpsweek(int week) {
    int w;
    (void)time2gpst(utc2gpst(timeget()), &w);
    if (w < 1560) {
        w = 1560; /* use 2009/12/1 if time is earlier than 2009/12/1 */
    }
    return week + (w - week + 1) / 1024 * 1024;
}
/* get tick time ---------------------------------------------------------------
 * get current tick in ms
 * args   : none
 * return : current tick in ms
 *-----------------------------------------------------------------------------*/
extern uint32_t tickget(void) {
    struct timespec tp = {0};
    struct timeval tv = {0};

#ifdef CLOCK_MONOTONIC_RAW
    /* linux kernel > 2.6.28 */
    if (!clock_gettime(CLOCK_MONOTONIC_RAW, &tp)) {
        return tp.tv_sec * 1000u + tp.tv_nsec / 1000000u;
    } else {
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000u + tv.tv_usec / 1000u;
    }
#else
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000u + tv.tv_usec / 1000u;
#endif
}
/* sleep ms --------------------------------------------------------------------
 * sleep ms
 * args   : int   ms         I   miliseconds to sleep (<0:no sleep)
 * return : none
 *-----------------------------------------------------------------------------*/
extern void sleepms(int ms) {
    struct timespec ts;
    if (ms <= 0) {
        return;
    }
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000 * 1000000);
    nanosleep(&ts, NULL);
}
