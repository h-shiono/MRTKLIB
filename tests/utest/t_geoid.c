/*------------------------------------------------------------------------------
 * rtklib unit test driver : geoid functions
 *-----------------------------------------------------------------------------*/
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "mrtklib/rtklib.h"

/* latitude, longitude, geoid height (m) */
/* reference : http://sps.unavco.org/geoid */
static double poss[][3] = {{90.001 * D2R, 80.000 * D2R, 0.000},
                           {-90.001 * D2R, 80.000 * D2R, 0.000},
                           {30.000 * D2R, 360.000 * D2R, 0.000},
                           {-30.000 * D2R, -360.001 * D2R, 0.000},
                           {-90.000 * D2R, 359.999 * D2R, -29.534},
                           {90.000 * D2R, 80.000 * D2R, 13.606},
                           {-90.000 * D2R, -60.000 * D2R, -29.534},
                           {30.000 * D2R, -360.000 * D2R, 35.387},
                           {-30.000 * D2R, 359.999 * D2R, 21.409},
                           {10.000 * D2R, 45.000 * D2R, -20.486},
                           {-60.123 * D2R, 135.123 * D2R, -33.152},
                           {19.999 * D2R, 135.000 * D2R, 41.602},
                           {50.001 * D2R, 135.000 * D2R, 20.555},
                           {35.000 * D2R, 119.999 * D2R, 4.386},
                           {35.000 * D2R, 150.001 * D2R, 14.779},
                           {20.000 * D2R, 120.000 * D2R, 21.269},
                           {50.000 * D2R, 150.000 * D2R, 20.277},
                           {35.000 * D2R, 135.000 * D2R, 36.355},
                           {30.000 * D2R, 135.000 * D2R, 36.017}, /* taiheiyo */
                           {0, 0, 0}};
/* off the 1-degree grid where the geoid is steep: the embedded model differs
   from the EGM96 15' reference by up to 2.2 m here */
static double poss_off[][3] = {{45.402 * D2R, 141.750 * D2R, 27.229}, /* wakkanai */
                               {24.454 * D2R, 122.942 * D2R, 21.652}, /* ishigaki */
                               {33.120 * D2R, 139.797 * D2R, 43.170}, /* hachijo */
                               {0, 0, 0}};

/* opengeoid(), closegeoid() */
void utest1(void) {
    int ret;

    ret = opengeoid(10, "");
    assert(ret == 0); /* no model */
    ret = opengeoid(GEOID_EGM96_M150, "");
    assert(ret == 0); /* no file */
    ret = opengeoid(GEOID_EMBEDDED, "");
    assert(ret == 1);
    closegeoid();

    printf("%s utest1 : OK\n", __FILE__);
}
/* print difference */
void printgeoid(const double* pos, double* h, int n) {
    int i;
    printf("%7.3f %8.3f %9.4f: ", pos[0] * R2D, pos[1] * R2D, pos[2]);
    for (i = 0; i < n; i++) printf(" %9.4f", h[i]);
    printf(" :");
    for (i = 1; i < n; i++) printf(" %9.4f", h[i] != 0.0 ? h[i] - h[0] : 0.0);
    printf("\n");
}
/* geoidh() */
void utest2(void) {
    double h;
    int i;

    opengeoid(GEOID_EMBEDDED, "");
    for (i = 0; poss[i][0] != 0.0; i++) {
        h = geoidh(poss[i]);
        printgeoid(poss[i], &h, 1);
        assert(fabs(h - poss[i][2]) < 1.0);
    }
    for (i = 0; poss_off[i][0] != 0.0; i++) {
        h = geoidh(poss_off[i]);
        printgeoid(poss_off[i], &h, 1);
        assert(fabs(h - poss_off[i][2]) < 10.0);
    }
    closegeoid();
    printf("%s utest2 : OK\n", __FILE__);
}
int main(void) {
    utest1();
    utest2();
    return 0;
}
