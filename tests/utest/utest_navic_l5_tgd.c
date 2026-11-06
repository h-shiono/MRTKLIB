/* NavIC L5-only group-delay regression, ISRO SPS ICD 1.1, section 6.2.1.5.
 * Include the implementation to exercise the private prange helper without
 * adding a public API solely for testing. No captured receiver data used. */
#include "../../src/pos/mrtk_spp.c"

int main(void) {
    nav_t* nav = calloc(1, sizeof(*nav));
    obsd_t obs = {0};
    prcopt_t opt = {0};
    double variance, actual, expected;
    int failed = 0;
    if (!nav) return 2;
    nav->eph = calloc(1, sizeof(*nav->eph));
    if (!nav->eph) {
        free(nav);
        return 2;
    }
    nav->n = 1;
    obs.sat = satno(SYS_IRN, 9);
    nav->eph[0].sat = obs.sat;
    obs.P[0] = 24000000.0;
    obs.code[0] = CODE_L5A;
    nav->eph[0].tgd[0] = 1e-8;
    expected = obs.P[0] - (2492.028 / 1176.45) * (2492.028 / 1176.45) * 299792458.0e-8;
    actual = prange(&obs, nav, &opt, &variance);
    if (!isfinite(actual) || fabs(actual - expected) > 1e-8) failed++;
    obs.code[1] = CODE_L9A;
    actual = prange(&obs, nav, &opt, &variance);
    if (!isfinite(actual) || fabs(actual - expected) > 1e-8) failed++;
    obs.code[1] = CODE_NONE;
    nav->eph[0].tgd[0] = 0.0;
    actual = prange(&obs, nav, &opt, &variance);
    if (!isfinite(actual) || actual != obs.P[0]) failed++;
    free(nav->eph);
    free(nav);
    printf("NavIC L5-only TGD failures: %d\n", failed);
    return failed ? 1 : 0;
}
