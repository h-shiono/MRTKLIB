/*------------------------------------------------------------------------------
 * unit test : mrtk cssr2rtcm3 output epoch scheduling (issue #442)
 *
 * c2r_next_epoch() picks the RTCM3 output epoch from the latest valid PVT
 * time. The converter used to step one interval per PVT, so after an input
 * gap it lagged the receiver and jumped once the lag passed 10 s. Coverage:
 *
 *   (1) 1 Hz: consecutive PVTs give consecutive epochs.
 *   (2) after a short gap and after a gap over 10 s, the first PVT gives its
 *       own epoch at once (no catch-up through the missed epochs).
 *   (3) duplicate and backward PVT times give no epoch.
 *   (4) a fractional PVT time maps to the start of its interval, and a tow
 *       just below a second boundary is not pushed back one interval.
 *   (5) GPS week rollover.
 *   (6) non-default intervals (-t 5, -t 0.5).
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <math.h>
#include <stdio.h>

#include "../../apps/cssr2rtcm3/cssr2rtcm3_sched.h"
#include "mrtklib/rtklib.h"

static int fails = 0;

#define CHECK(cond, msg)                 \
    do {                                 \
        if (!(cond)) {                   \
            printf("FAIL: %s\n", (msg)); \
            fails++;                     \
        } else {                         \
            printf("ok:   %s\n", (msg)); \
        }                                \
    } while (0)

#define WEEK 2441

static gtime_t T(int week, double tow) { return gpst2time(week, tow); }

/* epoch as tow of WEEK (or -1 if none) */
static double next(double pvt, double iv, gtime_t* last) {
    gtime_t t;
    if (!c2r_next_epoch(T(WEEK, pvt), iv, *last, &t)) {
        return -1.0;
    }
    *last = t;
    return time2gpst(t, NULL);
}

int main(void) {
    gtime_t last = {0}, t;
    int week;
    double tow;

    /* (1) 1 Hz */
    CHECK(next(504300.0, 1.0, &last) == 504300.0, "first PVT gives its own epoch");
    CHECK(next(504301.0, 1.0, &last) == 504301.0, "next PVT gives the next epoch");

    /* (2) gaps */
    CHECK(next(504304.0, 1.0, &last) == 504304.0, "after a 3 s gap: latest epoch, no catch-up");
    CHECK(next(504305.0, 1.0, &last) == 504305.0, "then 1 Hz again");
    CHECK(next(504330.0, 1.0, &last) == 504330.0, "after a 25 s gap: latest epoch");

    /* (3) duplicate / backward */
    CHECK(next(504330.0, 1.0, &last) == -1.0, "duplicate PVT time: no epoch");
    CHECK(next(504329.0, 1.0, &last) == -1.0, "backward PVT time: no epoch");
    CHECK(time2gpst(last, NULL) == 504330.0, "last epoch unchanged by a duplicate / backward PVT");
    CHECK(next(504331.0, 1.0, &last) == 504331.0, "forward again after a backward PVT");

    /* (4) rounding */
    CHECK(next(504332.4, 1.0, &last) == 504332.0, "fractional PVT time: start of its interval");
    CHECK(next(504332.9, 1.0, &last) == -1.0, "second PVT in the same interval: no epoch");
    CHECK(next(504333.0 - 1e-8, 1.0, &last) == 504333.0, "tow just below a boundary counts as the boundary");

    /* (5) week rollover */
    last = T(WEEK, 604799.0);
    CHECK(c2r_next_epoch(T(WEEK + 1, 0.0), 1.0, last, &t), "week rollover: epoch given");
    tow = time2gpst(t, &week);
    CHECK(week == WEEK + 1 && tow == 0.0, "week rollover: tow 0 of the next week");
    CHECK(timediff(t, last) == 1.0, "week rollover: one second after the last epoch");

    /* (6) non-default intervals */
    last.time = 0;
    last.sec = 0.0;
    CHECK(next(504303.0, 5.0, &last) == 504300.0, "-t 5: PVT 504303 -> epoch 504300");
    CHECK(next(504304.0, 5.0, &last) == -1.0, "-t 5: same interval -> no epoch");
    CHECK(next(504305.0, 5.0, &last) == 504305.0, "-t 5: next interval -> 504305");
    CHECK(next(504322.0, 5.0, &last) == 504320.0, "-t 5: after a gap -> latest interval 504320");
    last.time = 0;
    last.sec = 0.0;
    CHECK(next(504300.2, 0.5, &last) == 504300.0, "-t 0.5: 504300.2 -> 504300.0");
    CHECK(next(504300.7, 0.5, &last) == 504300.5, "-t 0.5: 504300.7 -> 504300.5");

    printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
