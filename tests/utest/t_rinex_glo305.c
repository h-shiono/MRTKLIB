/*------------------------------------------------------------------------------
 * t_rinex_glo305.c : RINEX 3.05 GLONASS records with and without ORBIT-4 (#482)
 *
 * RINEX 3.05 adds a mandatory 4th orbit line (BROADCAST ORBIT-4) to GLONASS
 * records, but IGS BRDC 2022-060 is labelled 3.05 and still uses the 3.04
 * 3-line layout. The reader used to take the record length from the header
 * version alone, so it read the next record's epoch line as ORBIT-4: every
 * second record was lost and tk<<1 was OR-ed into svh. Records below are from
 * that file (R01-R03, 2022-03-01 00:15, plus G01).
 *
 * Explicit checks, not assert(), so the test also runs under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/rtklib.h"

static int fails = 0;

#define CHECK(cond, msg)                 \
    do {                                 \
        if (!(cond)) {                   \
            printf("FAIL: %s\n", (msg)); \
            fails++;                     \
        }                                \
    } while (0)

static const char* hdr305 =
    "     3.05           N: GNSS NAV DATA    M: MIXED            RINEX VERSION / TYPE\n"
    "                                                            END OF HEADER\n";

static const char* glo[3][4] = {{"R01 2022 03 01 00 15 00 7.145106792450E-06-0.000000000000E+00 1.728000000000E+05\n",
                                 "    -2.035722656250E+03 2.404670715332E+00-9.313225746155E-10 0.000000000000E+00\n",
                                 "    -1.203031152344E+04-1.903871536255E+00 1.862645149231E-09 1.000000000000E+00\n",
                                 "    -2.239498242188E+04 8.049907684326E-01 1.862645149231E-09 0.000000000000E+00\n"},
                                {"R02 2022 03 01 00 15 00 5.110967904329E-04 0.000000000000E+00 1.728000000000E+05\n",
                                 "    -1.768408300781E+04 2.083477020264E+00-5.587935447693E-09 0.000000000000E+00\n",
                                 "     2.292668945312E+03-1.386788368225E+00 1.862645149231E-09-4.000000000000E+00\n",
                                 "    -1.816517919922E+04-2.203665733337E+00 0.000000000000E+00 0.000000000000E+00\n"},
                                {"R03 2022 03 01 00 15 00 5.232449620962E-05 0.000000000000E+00 1.728000000000E+05\n",
                                 "    -2.116166259766E+04 4.309415817261E-01-5.587935447693E-09 0.000000000000E+00\n",
                                 "     1.386005029297E+04-1.184549331665E-01 0.000000000000E+00 5.000000000000E+00\n",
                                 "    -2.992396484375E+03-3.545931816101E+00-1.862645149231E-09 0.000000000000E+00\n"}};

/* status flags 183, delta_tau_n, F_T 2, health flags 3 */
static const char* orbit4 = "     1.830000000000E+02 9.313225746155E-09 2.000000000000E+00 3.000000000000E+00\n";

static const char* gps[8] = {"G01 2022 03 01 00 00 00 4.199589602650E-04-9.322320693170E-12 0.000000000000E+00\n",
                             "     4.600000000000E+01 9.162500000000E+01 4.273035131920E-09 1.487053806450E+00\n",
                             "     4.719942808150E-06 1.146077807060E-02 3.010034561160E-06 5.153669487000E+03\n",
                             "     1.728000000000E+05-2.663582563400E-07-2.161205555320E+00 1.303851604460E-07\n",
                             "     9.868948483830E-01 3.374062500000E+02 8.835485616070E-01-8.292845430300E-09\n",
                             "     2.296524230880E-10 1.000000000000E+00 2.199000000000E+03 0.000000000000E+00\n",
                             "     2.000000000000E+00 0.000000000000E+00 5.122274160390E-09 4.600000000000E+01\n",
                             "     1.656060000000E+05 4.000000000000E+00 0.000000000000E+00 0.000000000000E+00\n"};

/* write header + R01..R03 (with or without ORBIT-4), G01 after R02 when gps_mid */
static void write_nav(const char* file, int with_orbit4, int gps_mid) {
    FILE* fp = fopen(file, "w");
    int i, j;

    fputs(hdr305, fp);
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
            fputs(glo[i][j], fp);
        }
        if (with_orbit4) {
            fputs(orbit4, fp);
        }
        if (gps_mid && i == 1) {
            for (j = 0; j < 8; j++) {
                fputs(gps[j], fp);
            }
        }
    }
    fclose(fp);
}

static void run_case(const char* name, int with_orbit4, int gps_mid) {
    const char* file = "t_rinex_glo305.nav";
    const int frq[3] = {1, -4, 5};
    nav_t* nav = (nav_t*)calloc(1, sizeof(nav_t)); /* large: heap only */
    char msg[128];
    int i, prn;

    if (!nav) {
        printf("FAIL: setup\n");
        fails++;
        return;
    }
    write_nav(file, with_orbit4, gps_mid);
    readrnx(file, 1, "", NULL, nav, NULL);
    remove(file);
    printf("%s: n=%d ng=%d\n", name, nav->n, nav->ng);

    sprintf(msg, "%s: all 3 GLONASS records read", name);
    CHECK(nav->ng == 3, msg);
    sprintf(msg, "%s: GPS record count", name);
    CHECK(nav->n == (gps_mid ? 1 : 0), msg);
    for (i = 0; i < nav->ng && i < 3; i++) {
        const geph_t* g = nav->geph + i;
        satsys(g->sat, &prn);
        printf("  R%02d frq=%d svh=%d flags=%d sva=%d\n", prn, g->frq, g->svh, g->flags, g->sva);
        sprintf(msg, "%s: R%02d satellite and frequency", name, i + 1);
        CHECK(prn == i + 1 && g->frq == frq[i], msg);
        sprintf(msg, "%s: R%02d svh/flags/sva", name, i + 1);
        if (with_orbit4) { /* health 0 | health flags 3 << 1 */
            CHECK(g->svh == 6 && g->flags == 183 && g->sva == 2, msg);
        } else {
            CHECK(g->svh == 0 && g->flags == 0 && g->sva == 0, msg);
        }
    }
    freenav(nav, 0xFF);
    free(nav);
}

int main(void) {
    run_case("3.05 label, 3-line records", 0, 0);
    run_case("3.05 label, 3-line records + GPS", 0, 1);
    run_case("3.05 conformant, 4-line records", 1, 0);
    run_case("3.05 conformant, 4-line records + GPS", 1, 1);
    printf("%s\n", fails ? "FAILED" : "all checks passed");
    return fails ? 1 : 0;
}
