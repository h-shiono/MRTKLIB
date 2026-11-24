/* RTK fix-and-hold uses [ambiguity_resolution.hold] variance (#564). Includes
 * the implementation to reach the private holdamb(). No captured receiver data
 * used.
 *
 * Three fixed GPS satellites on one frequency; the fixed ambiguity of
 * satellite 2 differs from the float state by one cycle, so holdamb() pulls
 * the float state towards it. A larger constraint variance must pull less.
 * Values <= 0 or not finite fall back to 0.001, as in PPP-RTK (older mrtk run
 * saved pos2-varholdamb = 0 into .conf files). */
#include <math.h>

#include "../../src/pos/mrtk_rtkpos.c"

static int failed = 0;

#define NS 3

static void check(const char* name, int ok) {
    if (!ok) {
        printf("FAIL %s\n", name);
        failed++;
    }
}

/* Run holdamb() with the given variance; return the DD float ambiguity
 * (satellite 1 - satellite 2) after the hold update. */
static double run(double varholdamb) {
    prcopt_t opt = prcopt_default;
    opt.mode = PMODE_KINEMA;
    opt.nf = 1;
    opt.navsys = SYS_GPS;
    opt.modear = ARMODE_FIXHOLD;
    opt.minholdsats = 0;
    opt.elmaskhold = 0.0;
    opt.varholdamb = varholdamb;
    rtk_t* rtk = calloc(1, sizeof(rtk_t));
    rtkinit(rtk, &opt);

    double* xa = zeros(rtk->nx, 1);
    int b[NS];
    for (int i = 0; i < NS; i++) {
        int sat = satno(SYS_GPS, i + 1);
        rtk->ssat[sat - 1].sys = SYS_GPS;
        rtk->ssat[sat - 1].fix[0] = 2;
        rtk->ssat[sat - 1].azel[1] = 45.0 * D2R;
        b[i] = IB(sat, 0, &rtk->opt);
        rtk->x[b[i]] = 10.0;
        rtk->P[b[i] + b[i] * rtk->nx] = 1.0;
        xa[b[i]] = 10.0;
    }
    xa[b[1]] = 9.0; /* fixed DD (1 - 2) = 1 cycle; float DD = 0 */

    holdamb(rtk, xa);
    double dd = rtk->x[b[0]] - rtk->x[b[1]];

    free(xa);
    rtkfree(rtk);
    free(rtk);
    return dd;
}

int main(void) {
    double tight = run(0.001), loose = run(1.0);

    /* the float DD moves from 0 towards the fixed 1 cycle; less with a looser hold */
    check("variance 0.001 pulls the DD close to the fixed value", tight > 0.99 && tight <= 1.0);
    check("variance 1.0 pulls the DD less", loose > 0.1 && loose < tight - 0.1);
    check("variance 0 falls back to 0.001", fabs(run(0.0) - tight) < 1e-12);
    check("negative variance falls back to 0.001", fabs(run(-0.5) - tight) < 1e-12);
    check("NaN variance falls back to 0.001", fabs(run(NAN) - tight) < 1e-12);

    if (failed) {
        printf("utest_rtk_holdamb: %d check(s) FAILED (tight %.6f, loose %.6f)\n", failed, tight, loose);
        return 1;
    }
    printf("utest_rtk_holdamb: all checks passed (tight %.6f, loose %.6f)\n", tight, loose);
    return 0;
}
