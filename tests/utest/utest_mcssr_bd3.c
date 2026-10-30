/*------------------------------------------------------------------------------
 * unit test : MADOCA-PPP L6E CSSR signal decode for BeiDou GNSS ID 3 (#401)
 *
 * IS-QZSS-MDC-005 (MADOCALIB 2.2) lets a BDS-3 satellite on PRN 1-18 (BeiDou
 * PRN reassignment, April 2026) be sent in GNSS ID 3; its signal mask is then
 * read with the GNSS ID 7 (BDS-3) table. A signal mask bit that is undefined
 * in the table in use maps to code 0, and decode_mcssr_cb/pb used to index
 * cbias[code-1] = cbias[-1] with it (out-of-bounds stack write).
 *
 * The test synthesizes one 5-frame L6E message (ST1 mask, ST4 code bias,
 * ST5 phase bias) and feeds the frames to decode_qzss_l6emsg():
 *
 *   GNSS ID 3, C09, sigmask bits 0/3/6/9, no cell mask (4 cells)
 *     BDS-2 table: L2I, L6I, L7I, (undefined)
 *   GNSS ID 7, C19, sigmask bits 0/10 (2 cells)
 *     BDS-3 table: L2I, L1P
 *
 * Checks (C09 classified BDS-2, i.e. the pre-#401 PRN<19 rule):
 *   (1) C09 code / phase biases land on L2I, L6I, L7I; the undefined fourth
 *       cell is skipped, so exactly three codes are valid.
 *   (2) C19 biases decode to the values written after the skipped cell, which
 *       proves the skipped cell still consumed its bits.
 * Run under ASan to also catch a regression of the [-1] write.
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <math.h>
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
        } else {                         \
            printf("ok  : %s\n", (msg)); \
        }                                \
    } while (0)

#define NFRAME 5
#define TOW 36000 /* GPST TOW of the message (10:00:00) */
#define IOD 5
#define WEEK 2440

static uint8_t msg[NFRAME * L6_DATA_BITS / 8 + 8];
static int pos = 0;

static void put(int len, uint32_t v) {
    setbitu(msg, pos, len, v);
    pos += len;
}

/* ST header after message number / subtype: epoch, UDI, MMI, IOD */
static void put_head(int st, int ep) {
    put(12, 4073);
    put(4, st);
    put(st == 1 ? 20 : 12, ep);
    put(4, 0); /* update interval 1 s */
    put(1, 0); /* multiple message indicator */
    put(4, IOD);
}

/* 40-bit satellite mask for one PRN at index i (0 = first PRN of the GNSS) */
static void put_svmask(int i) {
    uint64_t m = (uint64_t)1 << (39 - i);
    put(8, (uint32_t)(m >> 32));
    put(32, (uint32_t)m);
}

static uint16_t sigbits(const int* idx, int n) {
    uint16_t m = 0;
    int k;
    for (k = 0; k < n; k++) m |= (uint16_t)(1 << (15 - idx[k]));
    return m;
}

static const int c09_idx[] = {0, 3, 6, 9};
static const int c19_idx[] = {0, 10};
static const int c09_cb[] = {1, 2, 3, 4}; /* x 0.02 m */
static const int c19_cb[] = {5, 6};
static const int c09_pb[] = {11, 12, 13, 14}; /* x 0.001 m */
static const int c19_pb[] = {15, 16};

static void build_message(void) {
    int k;

    put_head(1, TOW);
    put(4, 2); /* number of GNSS */
    put(4, 3); /* GNSS ID 3: BDS */
    put_svmask(9 - 1);
    put(16, sigbits(c09_idx, 4));
    put(1, 0);
    put(4, 7); /* GNSS ID 7: BDS-3 (PRN 19-) */
    put_svmask(19 - 19);
    put(16, sigbits(c19_idx, 2));
    put(1, 0);

    put_head(4, 0);
    for (k = 0; k < 4; k++) put(11, (uint32_t)c09_cb[k]);
    for (k = 0; k < 2; k++) put(11, (uint32_t)c19_cb[k]);

    put_head(5, 0);
    for (k = 0; k < 4; k++) {
        put(15, (uint32_t)c09_pb[k]);
        put(2, 0);
    }
    for (k = 0; k < 2; k++) {
        put(15, (uint32_t)c19_pb[k]);
        put(2, 0);
    }
}

/* wrap data part n of the message into an L6 frame in rtcm->buff */
static void make_frame(rtcm_t* rtcm, int n) {
    int i;

    memset(rtcm->buff, 0, L6_RAW_LEN);
    setbitu(rtcm->buff, 0, 32, L6_PREAMBLE);
    setbitu(rtcm->buff, 32, 8, 209);                             /* PRN */
    setbitu(rtcm->buff, 40, 8, (L6_VID_MADOCA << 5) | (n == 0)); /* vid, fac 0, csid 0, sf_ind */
    setbitu(rtcm->buff, 48, 1, 0);                               /* alert */
    for (i = 0; i < L6_DATA_BITS; i++) {
        setbitu(rtcm->buff, L6_HEADER_BITS + i, 1, getbitu(msg, n * L6_DATA_BITS + i, 1));
    }
}

static int nvalid(const int* v) {
    int k, n = 0;
    for (k = 0; k < MAXCODE; k++) n += v[k] != 0;
    return n;
}

static void check_sat(const rtcm_t* rtcm, const char* id, const uint8_t* codes, const int* cb, const int* pb, int n) {
    const ssr_t* s = &rtcm->ssr[satid2no(id) - 1];
    char buf[128];
    int k;

    snprintf(buf, sizeof(buf), "%s: %d valid code biases (got %d)", id, n, nvalid(s->vcbias));
    CHECK(nvalid(s->vcbias) == n, buf);
    snprintf(buf, sizeof(buf), "%s: %d valid phase biases (got %d)", id, n, nvalid(s->vpbias));
    CHECK(nvalid(s->vpbias) == n, buf);
    for (k = 0; k < n; k++) {
        snprintf(buf, sizeof(buf), "%s %s: cbias %.2f, pbias %.3f", id, code2obs(codes[k]), cb[k] * 0.02,
                 pb[k] * 0.001);
        CHECK(s->vcbias[codes[k] - 1] && fabs(s->cbias[codes[k] - 1] - cb[k] * 0.02) < 1E-6 &&
                  s->vpbias[codes[k] - 1] && fabs(s->pbias[codes[k] - 1] - pb[k] * 0.001) < 1E-6,
              buf);
    }
}

int main(void) {
    static const uint8_t c09_codes[] = {CODE_L2I, CODE_L6I, CODE_L7I};
    static const uint8_t c19_codes[] = {CODE_L2I, CODE_L1P};
    rtcm_t* rtcm;
    int n, ret = 0;

    if (!(rtcm = (rtcm_t*)calloc(1, sizeof(rtcm_t))) || !init_rtcm(rtcm)) {
        printf("FAIL: init_rtcm\n");
        return 1;
    }
    build_message();
    CHECK(pos <= NFRAME * L6_DATA_BITS, "message fits in 5 data parts");

    set_mcssr_ch(0);
    init_mcssr(gpst2time(WEEK, TOW));
    for (n = 0; n < NFRAME; n++) {
        make_frame(rtcm, n);
        ret = decode_qzss_l6emsg(rtcm);
    }
    CHECK(ret == 10, "5th frame completes the message (ret 10)");

    check_sat(rtcm, "C09", c09_codes, c09_cb, c09_pb, 3);
    check_sat(rtcm, "C19", c19_codes, c19_cb, c19_pb, 2);

    free_rtcm(rtcm);
    free(rtcm);
    printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
