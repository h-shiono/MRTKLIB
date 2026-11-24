/* RTK measurement error variance under unset error ratios (#556). Includes the
 * implementation to reach the private varerr(). No captured receiver data used.
 *
 * prcopt_default leaves eratio[2] at 0 ("same as L1"). Carrier phase must then
 * weigh like L1 phase (factor 1.0) and code like L1 code (eratio[0]); falling
 * back to eratio[0] for phase made 3rd-frequency phase 100^2 noisier. With
 * baseline and clock terms zero the variance scales exactly with fact^2, so
 * every check compares variances as ratios. */
#include "../../src/pos/mrtk_rtkpos.c"

static int failed = 0;

#define NFRQ 3
#define EL (45.0 * D2R)

/* f < nf: carrier phase of frequency f; f >= nf: code of frequency f - nf */
static double var(const prcopt_t* opt, int sys, int f) { return varerr(1, sys, EL, 45.0, 45.0, 0.0, 0.0, f, opt); }

static void check(const char* name, double got, double expected) {
    if (!(fabs(got - expected) <= 1e-12 * fabs(expected))) {
        printf("FAIL %s: ratio %.6g, expected %.6g\n", name, got, expected);
        failed++;
    }
}

static void run(int sys, const char* sname) {
    prcopt_t opt = prcopt_default;
    char name[128];

    opt.nf = NFRQ;

    /* defaults: eratio = {100, 100, 0} */
    snprintf(name, sizeof(name), "%s unset eratio[2] phase", sname);
    check(name, var(&opt, sys, 2) / var(&opt, sys, 0), 1.0);
    snprintf(name, sizeof(name), "%s unset eratio[2] code", sname);
    check(name, var(&opt, sys, NFRQ + 2) / var(&opt, sys, NFRQ + 0), 1.0);
    snprintf(name, sizeof(name), "%s eratio[1] = eratio[0] phase", sname);
    check(name, var(&opt, sys, 1) / var(&opt, sys, 0), 1.0);
    snprintf(name, sizeof(name), "%s L1 code / phase", sname);
    check(name, var(&opt, sys, NFRQ + 0) / var(&opt, sys, 0), 100.0 * 100.0);

    /* a configured ratio keeps its scaling relative to L1 */
    opt.eratio[2] = 300.0;
    snprintf(name, sizeof(name), "%s eratio[2] = 300 phase", sname);
    check(name, var(&opt, sys, 2) / var(&opt, sys, 0), 3.0 * 3.0);
    snprintf(name, sizeof(name), "%s eratio[2] = 300 code", sname);
    check(name, var(&opt, sys, NFRQ + 2) / var(&opt, sys, NFRQ + 0), 3.0 * 3.0);

    /* negative ratio is treated as unset as well */
    opt.eratio[2] = -1.0;
    snprintf(name, sizeof(name), "%s negative eratio[2] phase", sname);
    check(name, var(&opt, sys, 2) / var(&opt, sys, 0), 1.0);
    snprintf(name, sizeof(name), "%s negative eratio[2] code", sname);
    check(name, var(&opt, sys, NFRQ + 2) / var(&opt, sys, NFRQ + 0), 1.0);
}

int main(void) {
    run(SYS_GPS, "GPS");
    run(SYS_GAL, "GAL");
    run(SYS_QZS, "QZS");

    if (failed) {
        printf("%d check(s) failed\n", failed);
        return 1;
    }
    printf("utest_rtk_varerr: all checks passed\n");
    return 0;
}
