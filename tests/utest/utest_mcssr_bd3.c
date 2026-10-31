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
 *   GNSS ID 0, PRN 33, sigmask bits 0/1, no cell mask (2 cells)
 *     satno() rejects it (GPS PRN > 32): an unsupported satellite, sat 0
 *   GNSS ID 3, C09, sigmask bits 0/3/6/9, no cell mask (4 cells)
 *     BDS-2 table: L2I, L6I, L7I, (undefined)
 *   GNSS ID 7, C19, sigmask bits 0/2/5/10, cell mask 1101 (3 cells)
 *     BDS-3 table: L2I, (undefined), (no cell), L1P
 *
 * Pass 1, set_bds2prn("C09") (C09 is BDS-2):
 *   (1) C09 code / phase biases land on L2I, L6I, L7I; the undefined fourth
 *       cell is skipped, so exactly three codes are valid.
 *   (2) C19 biases decode to the values written after the skipped cell, which
 *       proves the skipped cell still consumed its bits. The cell mask has a
 *       hole after the undefined cell: the decoder must move to the next mask
 *       bit after a skip (upstream re-tested the skipped bit and read the
 *       absent cell, so L1P got the following bits).
 * Pass 2, set_bds2prn("") (default since the BeiDou PRN reassignment, #401;
 * C09 is BDS-3): the same GNSS ID 3 mask is read with the BDS-3 table, so C09
 * biases land on L2I, L6I, L7D, L1D (all four valid).
 * Run under ASan to also catch a regression of the [-1] write.
 * In both passes the unsupported GPS PRN 33 must consume its cells without
 * being stored: decode_mcssr_cb/pb used to write its biases to ssr[sat-1] =
 * ssr[-1], which overlays the rtcm_t members before ssr[] (not an ASan
 * error, so the test compares those bytes before and after, #402).
 *
 * Also checks the BeiDou generation API (set_bds2prn, satsys_bd2, satno,
 * set_bds3igso, bds_gen) the decode depends on.
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

static const int g33_idx[] = {0, 1};
static const int c09_idx[] = {0, 3, 6, 9};
static const int c19_idx[] = {0, 2, 5, 10};
#define C19_CELLMASK 0xD                      /* 1101: idx 5 has no cell */
static const int c09_cb[] = {1, 2, 3, 4};     /* x 0.02 m */
static const int c19_cb[] = {5, 6};           /* decoded: L2I, L1P */
static const int c19_cb_tx[] = {5, 7, 6};     /* transmitted cells; 7 is the undefined idx 2 */
static const int c09_pb[] = {11, 12, 13, 14}; /* x 0.001 m */
static const int c19_pb[] = {15, 16};
static const int c19_pb_tx[] = {15, 17, 16};

static void build_message(void) {
    int k;

    put_head(1, TOW);
    put(4, 3); /* number of GNSS */
    put(4, 0); /* GNSS ID 0: GPS */
    put_svmask(33 - 1);
    put(16, sigbits(g33_idx, 2));
    put(1, 0);
    put(4, 3); /* GNSS ID 3: BDS */
    put_svmask(9 - 1);
    put(16, sigbits(c09_idx, 4));
    put(1, 0);
    put(4, 7); /* GNSS ID 7: BDS-3 (PRN 19-) */
    put_svmask(19 - 19);
    put(16, sigbits(c19_idx, 4));
    put(1, 1); /* cell mask available */
    put(4, C19_CELLMASK);

    put_head(4, 0);
    for (k = 0; k < 2; k++) put(11, 21u); /* PRN 33 */
    for (k = 0; k < 4; k++) put(11, (uint32_t)c09_cb[k]);
    for (k = 0; k < 3; k++) put(11, (uint32_t)c19_cb_tx[k]);

    put_head(5, 0);
    for (k = 0; k < 2; k++) {
        put(15, 31u); /* PRN 33 */
        put(2, 0);
    }
    for (k = 0; k < 4; k++) {
        put(15, (uint32_t)c09_pb[k]);
        put(2, 0);
    }
    for (k = 0; k < 3; k++) {
        put(15, (uint32_t)c19_pb_tx[k]);
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

static const uint8_t c19_codes[] = {CODE_L2I, CODE_L1P};

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

static void check_bds_gen(void) {
    int prn;

    CHECK(set_bds2prn(NULL) == 0 && satsys_bd2(satid2no("C09"), NULL) == SYS_CMP &&
              satsys_bd2(satid2no("C01"), NULL) == SYS_CMP,
          "default: no BDS-2, C01/C09 are BDS-3");
    CHECK(satno(SYS_BD2, 9) == 0, "default: satno(SYS_BD2, 9) rejects a BDS-3 PRN");
    CHECK(set_bds2prn("C09,C10") == 2 && satsys_bd2(satid2no("C09"), &prn) == SYS_BD2 && prn == 9 &&
              satsys_bd2(satid2no("C10"), NULL) == SYS_BD2 && satsys_bd2(satid2no("C11"), NULL) == SYS_CMP,
          "C09,C10: only C09/C10 are BDS-2");
    CHECK(satno(SYS_BD2, 9) == satno(SYS_CMP, 9), "C09,C10: satno(SYS_BD2, 9) == satno(SYS_CMP, 9)");
    CHECK(set_bds2prn("C01-C18") == 18 && bds_gen(1) == BDSGEN_2 && bds_gen(18) == BDSGEN_2 &&
              bds_gen(19) == BDSGEN_3 && bds_gen(0) == 0 && bds_gen(64) == 0,
          "C01-C18: legacy rule, PRN 19+ always BDS-3");
    CHECK(set_bds2prn("C19") == -1 && satsys_bd2(satid2no("C09"), NULL) == SYS_CMP,
          "C19 is rejected (PRN 19+ cannot be BDS-2) and leaves no BDS-2");
    CHECK(set_bds2prn("G09") == -1 && set_bds2prn("C10-C05") == -1, "non-BeiDou / reversed range rejected");
    CHECK(set_bds2prn("C09x") == -1 && set_bds2prn("C09-C10-C11") == -1 && set_bds2prn("C09-C10x") == -1 &&
              set_bds2prn("C") == -1 && set_bds2prn("C09-") == -1 && set_bds2prn("C09-C") == -1,
          "trailing text / incomplete range rejected");
    CHECK(set_bds2prn("c09 C10") == 2 && bds_gen(9) == BDSGEN_2 && bds_gen(10) == BDSGEN_2,
          "space separator and lower-case c accepted");
    CHECK(set_bds3igso(NULL) == 3 && is_bds3igso(6) && is_bds3igso(8) && !is_bds3igso(38),
          "BDS-3 IGSO default C06-C08");
    CHECK(set_bds3igso("C38-C40") == 3 && is_bds3igso(38) && is_bds3igso(40) && !is_bds3igso(6),
          "BDS-3 IGSO C38-C40 replaces the default");
    CHECK(set_bds3igso("X1") == -1 && is_bds3igso(7) && !is_bds3igso(39), "invalid IGSO list falls back to C06-C08");
}

static void run_pass(const char* bds2list, const uint8_t* c09_codes, int nc09) {
    static unsigned char before[sizeof(ssr_t)];
    rtcm_t* rtcm;
    int n, ret = 0;
    char buf[96];

    if (!(rtcm = (rtcm_t*)calloc(1, sizeof(rtcm_t))) || !init_rtcm(rtcm)) {
        printf("FAIL: init_rtcm\n");
        fails++;
        free(rtcm);
        return;
    }
    set_bds2prn(bds2list);
    set_mcssr_ch(0);
    init_mcssr(gpst2time(WEEK, TOW));
    memcpy(before, (unsigned char*)rtcm->ssr - sizeof(ssr_t), sizeof(ssr_t)); /* what ssr[-1] overlays */
    for (n = 0; n < NFRAME; n++) {
        make_frame(rtcm, n);
        ret = decode_qzss_l6emsg(rtcm);
    }
    snprintf(buf, sizeof(buf), "bds2prn \"%s\": 5th frame completes the message (ret 10)", bds2list);
    CHECK(ret == 10, buf);
    snprintf(buf, sizeof(buf), "bds2prn \"%s\": unsupported PRN 33 leaves the bytes before ssr[] intact", bds2list);
    CHECK(!memcmp(before, (unsigned char*)rtcm->ssr - sizeof(ssr_t), sizeof(ssr_t)), buf);

    check_sat(rtcm, "C09", c09_codes, c09_cb, c09_pb, nc09);
    check_sat(rtcm, "C19", c19_codes, c19_cb, c19_pb, 2);

    free_rtcm(rtcm);
    free(rtcm);
}

int main(void) {
    static const uint8_t c09_bd2[] = {CODE_L2I, CODE_L6I, CODE_L7I};
    static const uint8_t c09_bd3[] = {CODE_L2I, CODE_L6I, CODE_L7D, CODE_L1D};

    check_bds_gen();

    build_message();
    CHECK(pos <= NFRAME * L6_DATA_BITS, "message fits in 5 data parts");

    run_pass("C09", c09_bd2, 3);
    run_pass("", c09_bd3, 4);

    printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
