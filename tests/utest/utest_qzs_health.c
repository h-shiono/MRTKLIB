/* satexclude() QZS LNAV health (#512): the health bit of the L1 signal not
 * broadcast (0x10 L1C/A on QZS-1R/6, 0x01 L1C/B on QZS-2/3/4) is ignored when
 * exactly one of the two is set; every other bit excludes the satellite. */
#include <stdio.h>

#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_sys.h"

static int failed = 0;

static void check(int sat, int svh, int expect) {
    int got = satexclude(sat, 0.0, svh, NULL);
    if (got != expect) {
        printf("FAIL sat=%d svh=0x%02X: satexclude=%d, expected %d\n", sat, svh, got, expect);
        failed++;
    }
}

int main(void) {
    int qzs = satno(SYS_QZS, 196);
    int gps = satno(SYS_GPS, 5);
    prcopt_t opt = prcopt_default;

    check(qzs, 0x00, 0);
    check(qzs, 0x01, 0); /* QZS-2/3/4: L1C/B not broadcast */
    check(qzs, 0x10, 0); /* QZS-1R/6: L1C/A not broadcast */
    check(qzs, 0x11, 1); /* broadcast L1 side unhealthy too */
    check(qzs, 0x21, 1); /* L1 health */
    check(qzs, 0x30, 1);
    check(qzs, 0x18, 1); /* L2C */
    check(qzs, 0x14, 1); /* L5 */
    check(qzs, 0x12, 1); /* L1C */
    check(qzs, 0x08, 1);
    check(qzs, 0x04, 1);
    check(qzs, 0x02, 1);
    check(qzs, -1, 1);

    check(gps, 0x00, 0);
    check(gps, 0x01, 1);
    check(gps, 0x10, 1);

    /* exsats 2 (force-include) still bypasses the health check */
    opt.navsys = SYS_ALL;
    opt.exsats[qzs - 1] = 2;
    if (satexclude(qzs, 0.0, 0x11, &opt) != 0) {
        printf("FAIL exsats=2 did not bypass QZS svh=0x11\n");
        failed++;
    }

    printf("%s: %d failure(s)\n", failed ? "FAIL" : "PASS", failed);
    return failed ? 1 : 0;
}
