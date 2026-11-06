/*------------------------------------------------------------------------------
 * cssr2rtcm3_sched.h : RTCM3 output epoch scheduling for mrtk cssr2rtcm3
 *
 * Header-only so the unit test (tests/utest/utest_cssr2rtcm3_sched.c) can
 * exercise the same code as the converter.
 *----------------------------------------------------------------------------*/
#ifndef CSSR2RTCM3_SCHED_H
#define CSSR2RTCM3_SCHED_H

#include <math.h>

#include "mrtklib/mrtk_time.h"

/**
 * @brief Output epoch for the latest valid PVT time (#442).
 *
 * The epoch is the start of the interval that holds @p pvt on the GPST
 * time-of-week grid (floor(tow / iv) * iv), so it follows the receiver
 * after any input gap: epochs missed in the gap are not output late. If iv
 * does not divide one week, the last interval of the week is shorter.
 *
 * @param[in]  pvt   Latest valid PVT time.
 * @param[in]  iv    Output interval (s), > 0.
 * @param[in]  last  Last output epoch ({0} if none yet).
 * @param[out] t     Output epoch (set only when 1 is returned).
 * @return 1 if the epoch is after @p last, 0 if it is the same or earlier
 *         (duplicate or backward PVT time).
 */
static int c2r_next_epoch(gtime_t pvt, double iv, gtime_t last, gtime_t* t) {
    int week;
    double tow = time2gpst(pvt, &week);
    /* the epsilon puts a tow rounded just below a boundary (504300.9999999) on it */
    gtime_t e = gpst2time(week, floor(tow / iv + 1e-6) * iv);

    if (last.time != 0 && timediff(e, last) < 1e-3) {
        return 0;
    }
    *t = e;
    return 1;
}

#endif /* CSSR2RTCM3_SCHED_H */
