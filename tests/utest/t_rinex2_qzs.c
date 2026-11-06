/*------------------------------------------------------------------------------
 * t_rinex2_qzs.c : RINEX 2 navigation QZSS PRN extension (#347)
 *
 * RINEX 2 nav files carry QZSS as 2-digit PRNs 93..99 (J193..J199). Copies the
 * header and first GPS record of a RINEX 2.10 nav file, renumbers the record
 * as PRN 93/97/98/99, and checks that readrnx() maps each to QZSS.
 *
 * Kept out of t_rinex.c: that driver's stack-allocated nav_t locals already
 * bring main() close to the 8 MB stack limit once inlined. Explicit checks,
 * not assert(), so the test also runs under -DNDEBUG.
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

int main(void) {
    const char* src = "../data/rtklib/rinex/07590920.05n";
    const char* file = "t_rinex2_qzs.nav";
    const int prns[] = {93, 97, 98, 99};
    char buff[256], rec[8][256];
    nav_t* nav = (nav_t*)calloc(1, sizeof(nav_t)); /* large: heap only */
    FILE *fi = fopen(src, "r"), *fo = fopen(file, "w");
    int i, j, prn, sys;

    if (!nav || !fi || !fo) {
        printf("FAIL: setup\n");
        return 1;
    }
    while (fgets(buff, sizeof(buff), fi)) { /* header */
        fputs(buff, fo);
        if (strstr(buff, "END OF HEADER")) break;
    }
    for (i = 0; i < 8; i++) { /* first GPS record */
        if (!fgets(rec[i], sizeof(rec[i]), fi)) {
            printf("FAIL: read %s\n", src);
            return 1;
        }
    }
    fclose(fi);
    for (i = 0; i < 4; i++) {
        rec[0][0] = '0' + prns[i] / 10;
        rec[0][1] = '0' + prns[i] % 10;
        for (j = 0; j < 8; j++) fputs(rec[j], fo);
    }
    fclose(fo);

    readrnx(file, 1, "", NULL, nav, NULL);
    remove(file);
    printf("records read: %d\n", nav->n);
    CHECK(nav->n == 4, "4 ephemeris records read");
    for (i = 0; i < nav->n && i < 4; i++) {
        sys = satsys(nav->eph[i].sat, &prn);
        printf("RINEX 2 PRN %d -> sys=%d prn=%d\n", prns[i], sys, prn);
        CHECK(sys == SYS_QZS && prn == prns[i] + 100, "RINEX 2 PRN 93..99 is QZSS J193..J199");
    }
    free(nav->eph);
    free(nav);
    printf("%s\n", fails ? "FAILED" : "all checks passed");
    return fails ? 1 : 0;
}
