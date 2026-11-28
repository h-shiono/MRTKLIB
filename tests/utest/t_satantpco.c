/*------------------------------------------------------------------------------
 * Unit test for the per-frequency satellite PCO relative to the IF center.
 *-----------------------------------------------------------------------------*/
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/rtklib.h"

extern void satantpco(gtime_t teph, const obsd_t* obs, const double* rs, const nav_t* nav, double* dant);

int main(void) {
    const double com_x = 26500000.0;
    const double tol = 1e-7;
    const double c1 = FREQ1 * FREQ1 / (FREQ1 * FREQ1 - FREQ2 * FREQ2), c2 = 1.0 - c1;
    const double los[3] = {1.0, 0.0, 0.0}, updated_los[3] = {0.6, 0.8, 0.0};
    const int sat = satid2no("G01");
    const gtime_t t = gpst2time(2400, 300000.0);
    obsd_t obs = {0};
    nav_t* nav = calloc(1, sizeof(*nav));
    pcv_t* pcv;
    double rs[6], dts[2], var, ref, pco[3 * NFREQPCV] = {0}, d[3];
    int i, svh;

    assert(nav && sat > 0);
    nav->ne = 13;
    nav->peph = calloc(nav->ne, sizeof(*nav->peph));
    nav->n = 1;
    nav->eph = calloc(nav->n, sizeof(*nav->eph));
    assert(nav->peph && nav->eph);
    nav->eph[0].sat = sat;
    nav->eph[0].toe = nav->eph[0].toc = t;
    for (i = 0; i < nav->ne; i++) {
        nav->peph[i].time = timeadd(t, (i - 6) * 30.0);
        nav->peph[i].pos[sat - 1][0] = com_x;
        nav->peph[i].pos[sat - 1][3] = 1e-5;
    }

    obs.sat = sat;
    obs.time = t;
    for (i = 0; i < 3; i++) {
        obs.P[i] = 20000000.0;
        obs.L[i] = 100000000.0;
    }
    obs.code[0] = obs2code("1C");
    obs.code[1] = obs2code("2W");
    obs.code[2] = obs2code("5Q");
    pcv = nav->pcvs + sat - 1;
    pcv->sat = sat;
    /* +Z in the nominal satellite frame points along -ECEF X here. */
    pcv->off[0][2] = 1.18575;
    pcv->off[1][2] = 0.68161;
    pcv->off[2][2] = 0.45;
    ref = c1 * pcv->off[0][2] + c2 * pcv->off[1][2];

    satposs(t, &obs, 1, nav, EPHOPT_PREC, rs, dts, &var, &svh);
    assert(svh == 0 && fabs((com_x - rs[0]) - ref) < tol);
    satantpco(t, &obs, rs, nav, pco);
    for (i = 0; i < 3; i++) {
        d[i] = dot(los, pco + 3 * i, 3);
        assert(fabs(d[i] - (ref - pcv->off[i][2])) < tol);
        assert(fabs(dot(updated_los, pco + 3 * i, 3) - updated_los[0] * d[i]) < tol);
    }
    assert(d[0] > 0.0 && d[1] > d[0]);
    assert(fabs(c1 * d[0] + c2 * d[1]) < tol);

    memset(pco, 0, sizeof(pco));
    obs.code[2] = obs2code("1C");
    satantpco(t, &obs, rs, nav, pco);
    assert(norm(pco + 6, 3) == 0.0 && norm(pco, 3) > 0.0);
    obs.code[2] = obs2code("5Q");
    pcv->off[2][2] = 0.0;
    satantpco(t, &obs, rs, nav, pco);
    assert(norm(pco + 6, 3) == 0.0);
    memset(pco, 0, sizeof(pco));
    pcv->off[1][2] = 0.0;
    satantpco(t, &obs, rs, nav, pco);
    assert(norm(pco, 3 * NFREQPCV) == 0.0);

    free(nav->peph);
    free(nav->eph);
    free(nav);
    puts("GPS L1/L2/L5 relative PCO: slots, LOS sign and IF cancellation OK");
    return 0;
}
