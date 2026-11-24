/* RTK innovation thresholds for carrier phase and pseudorange (#563). Includes
 * the implementation to reach the private ddres(). No captured receiver data
 * used.
 *
 * maxinno is the phase threshold; maxinno_code the code threshold, < 0 meaning
 * "same as maxinno". Each threshold is chosen per observation type first and
 * 0 then disables that test, so a zero phase threshold must not switch off the
 * code test and a zero code threshold must not reject every code residual.
 * Three GPS satellites, one frequency; the DD residuals of satellite 2 against
 * reference satellite 1 are set per case, all biases are existing ones (no
 * 10x gate). */
#include "../../src/pos/mrtk_rtkpos.c"

static int failed = 0;

#define NS 3

static void check(const char* name, int got, int expected) {
    if (got != expected) {
        printf("FAIL %s: %d, expected %d\n", name, got, expected);
        failed++;
    }
}

/* Run ddres() once; report whether the satellite 1-2 phase and code rows are
 * returned. */
static void run(const char* name, double maxinno, double maxinno_code, double phase_dd, double code_dd,
                int expect_phase, int expect_code) {
    prcopt_t opt = prcopt_default;
    opt.mode = PMODE_KINEMA;
    opt.nf = 1;
    opt.navsys = SYS_GPS;
    opt.maxinno = maxinno;
    opt.maxinno_code = maxinno_code;
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
        int b = IB(sat[i], 0, &rtk->opt); /* existing bias: no 10x gate */
        P[b + b * rtk->nx] = 1.0;
    }
    y[0 + iu[1] * 2] += phase_dd;
    y[1 + iu[1] * 2] += code_dd;

    double* v = mat(NS * 2 * 2, 1);
    double* H = zeros(rtk->nx, NS * 2 * 2);
    double* R = mat(NS * 2 * 2, NS * 2 * 2);
    int vflg[NS * 2 * 2];
    int nv = ddres(rtk, NULL, obs, 0.0, x, P, P, sat, y, e, azel, freq, iu, ir, NS, v, H, R, vflg);
    int pair = (sat[0] << 16) | (sat[1] << 8), phase_row = 0, code_row = 0;
    for (int k = 0; k < nv; k++) {
        phase_row |= vflg[k] == pair;
        code_row |= vflg[k] == (pair | (1 << 4));
    }
    char label[160];
    snprintf(label, sizeof(label), "%s: phase row", name);
    check(label, phase_row, expect_phase);
    snprintf(label, sizeof(label), "%s: code row", name);
    check(label, code_row, expect_code);

    free(v);
    free(H);
    free(R);
    free(x);
    free(P);
    rtkfree(rtk);
    free(rtk);
}

int main(void) {
    /* code threshold unset: code uses the phase threshold (behaviour before #563) */
    run("unset code, 30 m: 50 m code rejected", 30.0, -1.0, 0.0, 50.0, 1, 0);
    run("unset code, 1 m: 5 m code rejected", 1.0, -1.0, 0.0, 5.0, 1, 0);
    /* separate thresholds */
    run("phase 1 m, code 10 m: 5 m code kept", 1.0, 10.0, 0.0, 5.0, 1, 1);
    run("phase 1 m, code 10 m: 5 m phase rejected", 1.0, 10.0, 5.0, 0.0, 0, 1);
    run("phase 1 m, code 10 m: 50 m code rejected", 1.0, 10.0, 0.0, 50.0, 1, 0);
    /* zero disables only its own test */
    run("phase 0, code 10 m: 50 m code still rejected", 0.0, 10.0, 50.0, 50.0, 1, 0);
    run("phase 30 m, code 0: 500 m code kept", 30.0, 0.0, 0.0, 500.0, 1, 1);
    run("phase 30 m, code 0: 50 m phase still rejected", 30.0, 0.0, 50.0, 0.0, 0, 1);
    run("phase 0, code unset: no test", 0.0, -1.0, 500.0, 500.0, 1, 1);

    if (failed) {
        printf("utest_rtk_innovation: %d check(s) FAILED\n", failed);
        return 1;
    }
    printf("utest_rtk_innovation: all checks passed\n");
    return 0;
}
