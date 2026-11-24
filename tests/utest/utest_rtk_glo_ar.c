/* RTK integer-search candidates under glonass_ar = "off" / "on" (#561). Includes
 * the implementation to reach the private ddidx() / resamb_LAMBDA(). No captured
 * receiver data used.
 *
 * With GLO_ARMODE_OFF, GLONASS phase biases must not form double differences
 * for the integer search even though manage_amb_LAMBDA() passes glo = 1 (as in
 * RTKLIB / MALIB; demo5 had lost the check). With GLO_ARMODE_ON they must. The
 * float filter is not involved: ddidx() only selects ambiguity pairs. */
#include "../../src/pos/mrtk_rtkpos.c"

static int failed = 0;

static void check(const char* name, int got, int expected) {
    if (got != expected) {
        printf("FAIL %s: %d, expected %d\n", name, got, expected);
        failed++;
    }
}

/* Mark a satellite as tracked, locked and above the AR elevation mask on L1. */
static void add_sat(rtk_t* rtk, int sys, int prn) {
    int sat = satno(sys, prn);
    rtk->x[IB(sat, 0, &rtk->opt)] = 1.0;
    rtk->ssat[sat - 1].sys = (uint8_t)sys;
    rtk->ssat[sat - 1].vsat[0] = 1;
    rtk->ssat[sat - 1].lock[0] = 10;
    rtk->ssat[sat - 1].azel[1] = 45.0 * D2R;
}

/* Count candidate pairs whose satellites belong to sys. */
static int pairs_of(rtk_t* rtk, const int* ix, int nb, int sys) {
    int n = 0;
    for (int i = 0; i < nb; i++) {
        int sat = (ix[i * 2 + 1] - rtk->na) % MAXSAT + 1;
        if (satsys(sat, NULL) == sys) {
            n++;
        }
    }
    return n;
}

static rtk_t* make_rtk(int glomodear, int with_gps) {
    prcopt_t opt = prcopt_default;
    opt.mode = PMODE_KINEMA;
    opt.nf = 1;
    opt.navsys = SYS_GPS | SYS_GLO;
    opt.modear = ARMODE_CONT;
    opt.glomodear = glomodear;
    opt.elmaskar = 15.0 * D2R;
    rtk_t* rtk = calloc(1, sizeof(rtk_t));
    rtkinit(rtk, &opt);
    if (with_gps) {
        for (int prn = 1; prn <= 4; prn++) {
            add_sat(rtk, SYS_GPS, prn);
        }
    }
    for (int prn = 1; prn <= 3; prn++) {
        add_sat(rtk, SYS_GLO, prn);
    }
    return rtk;
}

static void run_ddidx(int glomodear, const char* label, int gps_pairs, int glo_pairs) {
    rtk_t* rtk = make_rtk(glomodear, 1);
    int* ix = imat(rtk->nx, 2);
    char name[96];
    /* glo = 1: what manage_amb_LAMBDA() passes for off and on */
    int nb = ddidx(rtk, ix, 1, 1, 0);
    snprintf(name, sizeof(name), "%s GPS pairs", label);
    check(name, pairs_of(rtk, ix, nb, SYS_GPS), gps_pairs);
    snprintf(name, sizeof(name), "%s GLONASS pairs", label);
    check(name, pairs_of(rtk, ix, nb, SYS_GLO), glo_pairs);
    free(ix);
    rtkfree(rtk);
    free(rtk);
}

int main(void) {
    /* 4 GPS + 3 GLONASS satellites: one reference per constellation */
    run_ddidx(GLO_ARMODE_OFF, "glonass_ar off", 3, 0);
    run_ddidx(GLO_ARMODE_ON, "glonass_ar on", 3, 2);

    /* GLONASS only, AR off: no pairs, and the search stops before lambda() */
    rtk_t* rtk = make_rtk(GLO_ARMODE_OFF, 0);
    double* bias = zeros(rtk->nx, 1);
    double* xa = zeros(rtk->nx, 1);
    check("glonass only, off: resamb_LAMBDA", resamb_LAMBDA(rtk, bias, xa, 1, 1, 0), -1);
    check("glonass only, off: nb_ar", rtk->nb_ar, 0);
    free(bias);
    free(xa);
    rtkfree(rtk);
    free(rtk);

    if (failed) {
        printf("utest_rtk_glo_ar: %d check(s) FAILED\n", failed);
        return 1;
    }
    printf("utest_rtk_glo_ar: all checks passed\n");
    return 0;
}
