/* RTK pre-fit innovation gate for newly initialized phase biases (#562).
 * Includes the implementation to reach the private ddres(). No captured
 * receiver data used.
 *
 * ddres() opens the phase innovation threshold 10x when a phase bias of the
 * pair was just initialized, detected by P(bias) == std[0]^2. relpos() must
 * hand it the predicted covariance on the first pre-fit pass; with an all-zero
 * P (the bug) the gate stays at 1x. Three GPS satellites, one frequency: the
 * DD phase residual of satellite 2 against reference satellite 1 is 50 m
 * against a 30 m threshold, so it passes only through the 10x gate. */
#include "../../src/pos/mrtk_rtkpos.c"

static int failed = 0;

#define NS 3
#define PHASE_DD 50.0 /* m, between maxinno and 10 x maxinno */

static void check(const char* name, int got, int expected) {
    if (got != expected) {
        printf("FAIL %s: %d, expected %d\n", name, got, expected);
        failed++;
    }
}

/* Run ddres() once with satellite 2's bias variance set to var2, or with an
 * all-zero P when zero_p; return satellite 2's phase rejection count. */
static int run(double var2, int zero_p) {
    prcopt_t opt = prcopt_default;
    opt.mode = PMODE_KINEMA;
    opt.nf = 1;
    opt.navsys = SYS_GPS;
    opt.maxinno = 30.0;
    rtk_t* rtk = calloc(1, sizeof(rtk_t));
    rtkinit(rtk, &opt);

    const double pos[3] = {-3961904.0, 3348994.0, 3698211.0};
    double* x = zeros(rtk->nx, 1);
    double* P = zeros(rtk->nx, rtk->nx);
    for (int k = 0; k < 3; k++) {
        x[k] = pos[k] + 10.0;
        rtk->rb[k] = pos[k];
    }

    int sat[NS], iu[NS], ir[NS];
    obsd_t obs[NS * 2] = {0};
    double y[NS * 2 * 2] = {0}, e[NS * 2 * 3] = {0}, azel[NS * 2 * 2] = {0}, freq[NS * 2];
    const double el[NS] = {80.0, 50.0, 40.0}; /* satellite 1 = reference */
    for (int i = 0; i < NS; i++) {
        sat[i] = satno(SYS_GPS, i + 1);
        iu[i] = i;
        ir[i] = NS + i;
        rtk->ssat[sat[i] - 1].sys = SYS_GPS;
        for (int r = 0; r < 2; r++) { /* rover, base */
            int o = i + r * NS;
            obs[o].sat = (uint8_t)sat[i];
            y[0 + o * 2] = 1.0; /* phase residual (m) */
            y[1 + o * 2] = 1.0; /* code residual (m) */
            azel[1 + o * 2] = el[i] * D2R;
            e[2 + o * 3] = 1.0;
            freq[o] = FREQ1;
        }
        /* existing biases: variance well away from std[0]^2 */
        int b = IB(sat[i], 0, &rtk->opt);
        P[b + b * rtk->nx] = 1.0;
    }
    y[0 + iu[1] * 2] += PHASE_DD; /* rover phase of satellite 2 */
    int b2 = IB(sat[1], 0, &rtk->opt);
    P[b2 + b2 * rtk->nx] = var2;
    if (zero_p) {
        for (int k = 0; k < rtk->nx * rtk->nx; k++) {
            P[k] = 0.0;
        }
    }

    double* v = mat(NS * 2 * 2, 1);
    double* H = zeros(rtk->nx, NS * 2 * 2);
    double* R = mat(NS * 2 * 2, NS * 2 * 2);
    int vflg[NS * 2 * 2];
    int nv = ddres(rtk, NULL, obs, 0.0, x, P, P, sat, y, e, azel, freq, iu, ir, NS, v, H, R, vflg);
    int rejc = rtk->ssat[sat[1] - 1].rejc[0];
    /* the accepted case must return the satellite 1-2 phase row, not just leave
       rejc at 0 */
    int phase_row = 0;
    for (int k = 0; k < nv; k++) {
        if (vflg[k] == ((sat[0] << 16) | (sat[1] << 8))) { /* phase, frequency 0 */
            phase_row = 1;
        }
    }
    char name[96];
    snprintf(name, sizeof(name), "phase row %s (var2=%g zero_p=%d)", rejc ? "absent" : "present", var2, zero_p);
    check(name, phase_row, rejc == 0);

    free(v);
    free(H);
    free(R);
    free(x);
    free(P);
    rtkfree(rtk);
    free(rtk);
    return rejc;
}

int main(void) {
    const double var_new = SQR(prcopt_default.std[0]);
    check("new bias, predicted P: 10x gate accepts", run(var_new, 0), 0);
    check("existing bias: 1x gate rejects", run(1.0, 0), 1);
    check("new bias, all-zero P (pre-fix first pass): rejected", run(var_new, 1), 1);

    if (failed) {
        printf("utest_rtk_prefit_gate: %d check(s) FAILED\n", failed);
        return 1;
    }
    printf("utest_rtk_prefit_gate: all checks passed\n");
    return 0;
}
