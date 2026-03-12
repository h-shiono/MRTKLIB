/*------------------------------------------------------------------------------
 * rtklib unit test : RINEX 4.00 CNAV/CNV2 navigation message parsing
 *
 * Test data: BRD400DLR_S_20260680000_01D_MN.rnx.gz (DLR merged broadcast)
 * Contains: GPS LNAV/CNAV, QZSS LNAV/CNAV/CNV2, GAL INAV/FNAV,
 *           BDS D1/D2/CNV1/CNV2/CNV3, GLO FDMA, SBAS, IRN LNAV
 *----------------------------------------------------------------------------*/
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "mrtklib/rtklib.h"

/* count ephemeris by system and type */
static void count_eph(const nav_t *nav, int *n_gps_lnav, int *n_gps_cnav,
                      int *n_qzs_cnv2, int *n_bds_cnv1, int *n_bds_cnv2,
                      int *n_bds_cnv3) {
    int i, sys, prn;
    *n_gps_lnav = *n_gps_cnav = *n_qzs_cnv2 = 0;
    *n_bds_cnv1 = *n_bds_cnv2 = *n_bds_cnv3 = 0;

    for (i = 0; i < nav->n; i++) {
        sys = satsys(nav->eph[i].sat, &prn);
        if (sys == SYS_GPS) {
            if (nav->eph[i].type == 0) (*n_gps_lnav)++;
            else if (nav->eph[i].type == 1) (*n_gps_cnav)++;
        } else if (sys == SYS_QZS && nav->eph[i].type == 2) {
            (*n_qzs_cnv2)++;
        } else if (sys == SYS_CMP) {
            if (nav->eph[i].type == 2) (*n_bds_cnv1)++;
            else if (nav->eph[i].type == 3) (*n_bds_cnv2)++;
            else if (nav->eph[i].type == 4) (*n_bds_cnv3)++;
        }
    }
}

/* test 1: parse BRD400 and verify record counts */
static void test_brd400_counts(void) {
    nav_t nav = {0};
    sta_t sta = {""};
    int stat;
    int n_gps_lnav, n_gps_cnav, n_qzs_cnv2;
    int n_bds_cnv1, n_bds_cnv2, n_bds_cnv3;

    stat = readrnx("../data/rinex4/BRD400DLR_S_20260680000_01D_MN.rnx", 1, "",
                   NULL, &nav, &sta);
    assert(stat == 1);
    assert(nav.n > 0);
    assert(nav.ng > 0);  /* GLONASS */
    assert(nav.ns > 0);  /* SBAS */

    count_eph(&nav, &n_gps_lnav, &n_gps_cnav, &n_qzs_cnv2,
              &n_bds_cnv1, &n_bds_cnv2, &n_bds_cnv3);

    printf("  GPS LNAV=%d CNAV=%d\n", n_gps_lnav, n_gps_cnav);
    printf("  QZS CNV2=%d\n", n_qzs_cnv2);
    printf("  BDS CNV1=%d CNV2=%d CNV3=%d\n", n_bds_cnv1, n_bds_cnv2, n_bds_cnv3);
    printf("  GLO=%d SBAS=%d\n", nav.ng, nav.ns);

    assert(n_gps_lnav > 0);
    assert(n_gps_cnav > 0);
    assert(n_qzs_cnv2 > 0);
    assert(n_bds_cnv1 > 0);
    assert(n_bds_cnv2 > 0);
    assert(n_bds_cnv3 > 0);

    free(nav.eph);
    free(nav.geph);
    free(nav.seph);
    printf("  test_brd400_counts: OK\n");
}

/* test 2: GPS CNAV field sanity */
static void test_gps_cnav_fields(void) {
    nav_t nav = {0};
    int i, sys, prn, found = 0;

    readrnx("../data/rinex4/BRD400DLR_S_20260680000_01D_MN.rnx", 1, "",
            NULL, &nav, NULL);

    for (i = 0; i < nav.n; i++) {
        eph_t *e = &nav.eph[i];
        sys = satsys(e->sat, &prn);
        if (sys != SYS_GPS || e->type != 1) continue; /* CNAV */

        assert(e->week > 2000);    /* reasonable GPS week */
        assert(e->toes >= 0.0 && e->toes < 604800.0);
        assert(e->A > 2.0e7);     /* semi-major axis > 20000 km */
        assert(e->e >= 0.0 && e->e < 0.1);
        /* pseudo-IODE derived from toes for uniqeph dedup */
        assert(e->iode == (int)(e->toes / 300) % 256);
        assert(e->iodc == e->iode);
        found++;
    }
    assert(found > 0);

    free(nav.eph);
    free(nav.geph);
    free(nav.seph);
    printf("  test_gps_cnav_fields: OK (checked %d records)\n", found);
}

/* test 3: BDS CNAV week and IODE */
static void test_bds_cnav_fields(void) {
    nav_t nav = {0};
    int i, sys, prn;
    int cnv1_ok = 0, cnv2_ok = 0, cnv3_ok = 0;

    readrnx("../data/rinex4/BRD400DLR_S_20260680000_01D_MN.rnx", 1, "",
            NULL, &nav, NULL);

    for (i = 0; i < nav.n; i++) {
        eph_t *e = &nav.eph[i];
        sys = satsys(e->sat, &prn);
        if (sys != SYS_CMP) continue;

        if (e->type == 2) { /* CNAV-1 */
            assert(e->week > 800); /* BDT week > 800 (2024+) */
            assert(e->toes >= 0.0 && e->toes < 604800.0);
            assert(e->A > 2.0e7);
            assert(e->iodc > 0 || e->iode > 0); /* at least one should be set */
            cnv1_ok++;
        } else if (e->type == 3) { /* CNAV-2 */
            assert(e->week > 800);
            assert(e->A > 2.0e7);
            cnv2_ok++;
        } else if (e->type == 4) { /* CNAV-3 */
            assert(e->week > 800);
            assert(e->A > 2.0e7);
            /* pseudo-IODE derived from toes (CNV3 has no IODE field) */
            assert(e->iode == (int)(e->toes / 300) % 256);
            cnv3_ok++;
        }
    }
    assert(cnv1_ok > 0);
    assert(cnv2_ok > 0);
    assert(cnv3_ok > 0);

    free(nav.eph);
    free(nav.geph);
    free(nav.seph);
    printf("  test_bds_cnav_fields: OK (CNV1=%d CNV2=%d CNV3=%d)\n",
           cnv1_ok, cnv2_ok, cnv3_ok);
}

/* test 4: QZSS CNV2 fields */
static void test_qzs_cnv2_fields(void) {
    nav_t nav = {0};
    int i, sys, prn, found = 0;

    readrnx("../data/rinex4/BRD400DLR_S_20260680000_01D_MN.rnx", 1, "",
            NULL, &nav, NULL);

    for (i = 0; i < nav.n; i++) {
        eph_t *e = &nav.eph[i];
        sys = satsys(e->sat, &prn);
        if (sys != SYS_QZS || e->type != 2) continue; /* CNV2 */

        assert(e->week > 2000);    /* GPS week */
        assert(e->A > 2.0e7);
        assert(e->toes >= 0.0 && e->toes < 604800.0);
        found++;
    }
    assert(found > 0);

    free(nav.eph);
    free(nav.geph);
    free(nav.seph);
    printf("  test_qzs_cnv2_fields: OK (checked %d records)\n", found);
}

int main(void) {
    printf("t_rinex4: RINEX 4.00 CNAV parsing tests\n");
    test_brd400_counts();
    test_gps_cnav_fields();
    test_bds_cnav_fields();
    test_qzs_cnv2_fields();
    printf("t_rinex4: ALL PASSED\n");
    return 0;
}
