/*------------------------------------------------------------------------------
 * unit test : SBF QZSS SVID 181-190 to satellite number (#471)
 *
 * SBF numbers QZSS J01-J10 (PRN 193-202) as SVID 181-190 (mosaic-G5 Reference
 * Guide 4.1.9). Every QZSS block that takes the SVID through svid2sat()
 * (MeasEpoch, QZSRawL1CA, QZSNav) is fed with real blocks via input_sbf().
 * The other callers of svid2sat() also require the satellite system of their
 * block, so they reject SVID 188-190 before and after the fix.
 * Fixtures (tests/utest/sbfqzs_*) are built by
 * scripts/tests/make_sbf_qzss_svid_fixtures.py:
 *
 *   sbfqzs_g5.sbf     G5P3296u_20m (dataset clas_l6gap-v1): MeasEpoch 4027
 *                     with SVID 187 and 188; QZSRawL1CA 4066 subframes 1-3 of
 *                     SVID 187 and 188 (IODE 113)
 *   sbfqzs_malib.sbf  MALIB_OSS_data_obsnav_240822-1100: QZSNav 4095 of
 *                     SVID 187
 *
 * Each block is checked with SVID 187 (J07, decoded before the fix) and the
 * ends of the new range, 188 (J08) and 190 (J10).
 * No capture holds SVID 190 or a QZSNav of SVID 188-190, so those cases
 * rewrite the SVID of a real block in memory and recompute the SBF CRC. They
 * check the satellite number only: the observations / ephemeris stay those
 * of the original satellite.
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
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

#define ID_MEASEPOCH 4027
#define ID_QZSRAWL1CA 4066
#define ID_QZSNAV 4095

static const int svids[] = {187, 188, 190}, prns[] = {199, 200, 202};

static rtcm_t* rtcm; /* ~7.5 MB: heap only */

/* read a whole fixture into a heap buffer; returns its length, 0 on error */
static long read_file(const char* file, uint8_t** buf) {
    FILE* fp = fopen(file, "rb");
    long len;

    *buf = NULL;
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (len <= 0 || !(*buf = (uint8_t*)malloc(len)) || fread(*buf, 1, len, fp) != (size_t)len) {
        free(*buf);
        *buf = NULL;
        len = 0;
    }
    fclose(fp);
    return len;
}

static int blk_id(const uint8_t* blk) { return (blk[4] | blk[5] << 8) & 0x1FFF; }
static int blk_len(const uint8_t* blk) { return blk[6] | blk[7] << 8; }

/* the buffer is a sequence of whole SBF blocks (each header and block within
 * len), so the loops below can step by blk_len() */
static int blk_all(const uint8_t* buf, long len) {
    long i = 0;
    int n;

    while (i + 8 <= len && (n = blk_len(buf + i)) >= 8 && i + n <= len) i += n;
    return len > 0 && i == len;
}

static void set_crc(uint8_t* blk) {
    uint16_t crc = rtk_crc16(blk + 4, blk_len(blk) - 4);
    blk[2] = (uint8_t)(crc & 0xFF);
    blk[3] = (uint8_t)(crc >> 8);
}

static raw_t* new_raw(void) {
    raw_t* raw = (raw_t*)calloc(1, sizeof(raw_t));
    if (raw) init_raw(raw, STRFMT_SEPT);
    return raw;
}

static void del_raw(raw_t* raw) {
    free_raw(raw);
    free(raw);
}

/* feed one block; returns the decoder result at its last byte */
static int feed(raw_t* raw, const uint8_t* blk) {
    int i, ret = 0;
    for (i = 0; i < blk_len(blk); i++) ret = input_sbf(raw, rtcm, blk[i]);
    return ret;
}

static int has_obs(const raw_t* raw, int sat) {
    int i;
    for (i = 0; i < raw->obs.n; i++) {
        if (raw->obs.data[i].sat == sat) return 1;
    }
    return 0;
}

/* rewrite the SVID of the main-antenna Type-1 sub-blocks (layout as
 * decode_measepoch); returns the number rewritten */
static int measepoch_set_svid(uint8_t* blk, int from, int to) {
    int i, n = 0, n1 = blk[14], len1 = blk[15], len2 = blk[16];
    uint8_t* p = blk + 20;

    for (i = 0; i < n1 && p + 20 <= blk + blk_len(blk); i++) {
        if ((p[1] >> 5) == 0 && p[2] == from) {
            p[2] = (uint8_t)to;
            n++;
        }
        p += len1 + len2 * p[19];
    }
    set_crc(blk);
    return n;
}

/* MeasEpoch with SVID 187 and 188; 190 is the 188 sub-block renumbered */
static void test_measepoch(void) {
    uint8_t *buf, *src = NULL, *blk = NULL;
    long len = read_file("sbfqzs_g5.sbf", &buf), j;
    int i, sat, ok = blk_all(buf, len);
    char msg[64];
    raw_t* raw;

    for (j = 0; ok && j < len && !src; j += blk_len(buf + j)) {
        if (blk_id(buf + j) == ID_MEASEPOCH) src = buf + j;
    }
    CHECK(src != NULL, "measepoch: read the MeasEpoch of sbfqzs_g5.sbf");
    if (src && !(blk = (uint8_t*)malloc(blk_len(src)))) CHECK(0, "measepoch: allocation");
    for (i = 0; blk && i < 3; i++) {
        memcpy(blk, src, blk_len(src));
        if (svids[i] == 190) {
            CHECK(measepoch_set_svid(blk, 188, 190) == 1, "measepoch: one SVID 188 sub-block");
        }
        sat = satno(SYS_QZS, prns[i]);
        snprintf(msg, sizeof(msg), "measepoch: SVID %d -> PRN %d", svids[i], prns[i]);
        if (!(raw = new_raw())) {
            CHECK(0, "measepoch: allocation");
            break;
        }
        CHECK(feed(raw, blk) == 1 && has_obs(raw, sat), msg);
        del_raw(raw);
    }
    free(blk);
    free(buf);
}

/* QZSRawL1CA subframes 1-3 of SVID 187 and 188 complete one ephemeris each;
 * 190 is the 188 subframes renumbered */
static void test_qzsrawl1ca(void) {
    uint8_t *src, *buf = NULL, *blk;
    long len = read_file("sbfqzs_g5.sbf", &src);
    int i, n, sat, from, ok = blk_all(src, len);
    char msg[64];
    raw_t* raw;

    CHECK(ok, "qzsrawl1ca: read the SBF blocks of sbfqzs_g5.sbf");
    if (ok && !(buf = (uint8_t*)malloc(len))) CHECK(0, "qzsrawl1ca: allocation");
    for (i = 0; buf && i < 3; i++) {
        memcpy(buf, src, len);
        from = svids[i] == 190 ? 188 : svids[i];
        sat = satno(SYS_QZS, prns[i]);
        snprintf(msg, sizeof(msg), "qzsrawl1ca: SVID %d -> PRN %d", svids[i], prns[i]);
        if (!(raw = new_raw())) {
            CHECK(0, "qzsrawl1ca: allocation");
            break;
        }
        for (n = 0, blk = buf; blk < buf + len; blk += blk_len(blk)) {
            if (blk_id(blk) != ID_QZSRAWL1CA || blk[14] != from) continue;
            blk[14] = (uint8_t)svids[i];
            set_crc(blk);
            if (feed(raw, blk) == 2 && raw->ephsat == sat) n++;
        }
        CHECK(n == 1 && raw->nav.eph[sat - 1].sat == sat, msg);
        del_raw(raw);
    }
    free(buf);
    free(src);
}

/* QZSNav of SVID 187; 188 and 190 are the same block renumbered */
static void test_qzsnav(void) {
    uint8_t* blk;
    long len = read_file("sbfqzs_malib.sbf", &blk);
    int i, sat, ok = blk_all(blk, len) && blk_id(blk) == ID_QZSNAV;
    char msg[64];
    raw_t* raw;

    CHECK(ok, "qzsnav: read the QZSNav of sbfqzs_malib.sbf");
    for (i = 0; ok && i < 3; i++) {
        blk[14] = (uint8_t)svids[i];
        set_crc(blk);
        sat = satno(SYS_QZS, prns[i]);
        snprintf(msg, sizeof(msg), "qzsnav: SVID %d -> PRN %d", svids[i], prns[i]);
        if (!(raw = new_raw())) {
            CHECK(0, "qzsnav: allocation");
            break;
        }
        CHECK(feed(raw, blk) == 2 && raw->ephsat == sat && raw->nav.eph[sat - 1].sat == sat, msg);
        del_raw(raw);
    }
    free(blk);
}

int main(void) {
    rtcm = (rtcm_t*)calloc(1, sizeof(rtcm_t));
    if (!rtcm) return 1;
    init_rtcm(rtcm);
    test_measepoch();
    test_qzsrawl1ca();
    test_qzsnav();
    free_rtcm(rtcm);
    free(rtcm);
    if (fails) {
        printf("utest_sbf_qzss_svid: %d FAILED\n", fails);
        return 1;
    }
    printf("utest_sbf_qzss_svid: all passed\n");
    return 0;
}
