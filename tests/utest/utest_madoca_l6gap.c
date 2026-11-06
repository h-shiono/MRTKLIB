/*------------------------------------------------------------------------------
 * unit test : MADOCA-PPP L6 message assembly across a lost frame (#363)
 *
 * The MADOCA-PPP decoders assemble a message from the frames that follow its
 * subframe indicator. When the last frame of a message and the first frame of
 * the next one are lost (1-2 s receiver input loss), the next message's second
 * frame used to be appended in place of the lost frame and the message decoded
 * from shifted bits. With the receiver frame time (SBF QZSRawL6 TOW, passed by
 * the SBF decoder and by rtksvr for L6D iono) the decoder must discard the
 * message and wait for the next subframe indicator.
 *
 * Real frames from the utest_l6_route fixtures:
 *   l6route_g5.sbf    L6E PRN 205 messages at t0+1 (ST1, 5 frames), t0+8
 *                     (ST7, 3), t0+11 (ST4, 5), t0+18 (ST3, 3) ...
 *   l6route_iono.sbf  L6D PRN 200 coverage messages at t0+1 (2 frames),
 *                     t0+3 (3 frames), t0+6 (1 frame) ...
 * (t0: TOW of the first L6 block). Each case drops the last frame of one
 * message and the head of the next, then checks that
 *   - the frame after the gap does not change the decoded corrections, and
 *   - the messages from the next subframe indicator on decode as without the
 *     loss.
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

static uint32_t u4(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

/* SBF QZSRawL6 block (4069 / 4270 / 4271) at buf[i]: block length, TOW (s)
 * and L6 header PRN; returns 0 if buf[i] does not start such a block */
static int l6block(const uint8_t* buf, long len, long i, int* blen, int* tow, int* prn) {
    int id;

    if (i + 8 > len || buf[i] != '$' || buf[i + 1] != '@') return 0;
    id = (buf[i + 4] | (buf[i + 5] << 8)) & 0x1fff;
    *blen = buf[i + 6] | (buf[i + 7] << 8);
    if (*blen < 272 || i + *blen > len || (id != 4069 && id != 4270 && id != 4271)) return 0;
    *tow = (int)(u4(buf + i + 8) / 1000);
    *prn = (int)((u4(buf + i + 8 + 16) >> 24) & 0xff); /* frame byte 4 */
    return 1;
}

static gtime_t first_time(const uint8_t* buf, long len, int* t0) {
    long i;
    int blen, tow, prn;

    for (i = 0; i < len; i++) {
        if (l6block(buf, len, i, &blen, &tow, &prn)) {
            *t0 = tow;
            return gpst2time(buf[i + 12] | (buf[i + 13] << 8), u4(buf + i + 8) * 1e-3);
        }
    }
    *t0 = 0;
    return gpst2time(0, 0.0);
}

/* L6E: feed the blocks of PRN 205 from t0+1 on, except those of PRN 205 at
 * TOW drop[0..1]; returns the number of messages decoded (code 15) at
 * TOW >= tcount. *changed: rtcm->ssr changed by the PRN 205 frame at TOW tchk */
static int run_l6e(const uint8_t* buf, long len, const int* drop, int tchk, int tcount, int* changed) {
    raw_t* raw = (raw_t*)calloc(1, sizeof(raw_t));
    rtcm_t* rtcm = (rtcm_t*)calloc(1, sizeof(rtcm_t)); /* ~7.5 MB: heap only */
    ssr_t* save = (ssr_t*)malloc(sizeof(rtcm->ssr));
    long i, k;
    int blen, tow, prn, t0, ret, n = 0;
    gtime_t t;

    *changed = 0;
    if (!raw || !rtcm || !save) {
        free(raw);
        free(rtcm);
        free(save);
        return -1;
    }
    init_raw(raw, STRFMT_SEPT);
    init_rtcm(rtcm);
    t = first_time(buf, len, &t0);
    init_mcssr(t);

    for (i = 0; i < len;) {
        if (!l6block(buf, len, i, &blen, &tow, &prn)) {
            i++;
            continue;
        }
        if (tow < t0 + 1 || (prn == 205 && drop && (tow == t0 + drop[0] || tow == t0 + drop[1]))) {
            i += blen;
            continue;
        }
        if (prn == 205 && tow == t0 + tchk) memcpy(save, rtcm->ssr, sizeof(rtcm->ssr));
        for (k = 0, ret = 0; k < blen; k++) {
            ret = input_sbf(raw, rtcm, buf[i + k]);
            if (ret == 15 && tow >= t0 + tcount) n++;
        }
        if (prn == 205 && tow == t0 + tchk && memcmp(save, rtcm->ssr, sizeof(rtcm->ssr))) *changed = 1;
        i += blen;
    }
    free_rtcm(rtcm);
    free_raw(raw);
    free(raw);
    free(rtcm);
    free(save);
    return n;
}

/* L6D iono: route the blocks through input_sbf() and feed each MADOCA iono
 * frame to input_qzssl6d() with its frame time, as rtksvr does. Same drop /
 * check / count arguments as run_l6e(), for PRN 200 and decoded messages */
static int run_iono(const uint8_t* buf, long len, const int* drop, int tchk, int tcount, int* changed) {
    raw_t* raw = (raw_t*)calloc(1, sizeof(raw_t));
    rtcm_t* rtcm = (rtcm_t*)calloc(1, sizeof(rtcm_t));
    mdcl6d_t* mdc = (mdcl6d_t*)calloc(1, sizeof(mdcl6d_t));
    miono_region_t* save = (miono_region_t*)malloc(sizeof(miono_region_t));
    long i, k;
    int blen, tow, prn, t0, ret, n = 0;
    gtime_t t;

    *changed = 0;
    if (!raw || !rtcm || !mdc || !save) {
        free(raw);
        free(rtcm);
        free(mdc);
        free(save);
        return -1;
    }
    init_raw(raw, STRFMT_SEPT);
    init_rtcm(rtcm);
    t = first_time(buf, len, &t0);
    init_miono(t);
    mdc->time = t;

    for (i = 0; i < len;) {
        if (!l6block(buf, len, i, &blen, &tow, &prn)) {
            i++;
            continue;
        }
        if (tow < t0 + 1 || (prn == 200 && drop && (tow == t0 + drop[0] || tow == t0 + drop[1]))) {
            i += blen;
            continue;
        }
        for (k = 0, ret = 0; k < blen; k++) {
            ret = input_sbf(raw, rtcm, buf[i + k]);
        }
        if (ret == 16) {
            if (tow == t0 + tchk) memcpy(save, &mdc->re, sizeof(mdc->re));
            mdc->ftag = raw->l6tag;
            for (k = 0; k < L6_FRAME_LEN; k++) {
                if (input_qzssl6d(mdc, rtcm->buff[k]) == 10 && tow >= t0 + tcount) n++;
            }
            if (tow == t0 + tchk && memcmp(save, &mdc->re, sizeof(mdc->re))) *changed = 1;
        }
        i += blen;
    }
    free_rtcm(rtcm);
    free_raw(raw);
    free(raw);
    free(rtcm);
    free(mdc);
    free(save);
    return n;
}

int main(void) {
    uint8_t* buf;
    long len;
    int n0, n1, changed;

    printf("utest_madoca_l6gap: MADOCA-PPP L6 message assembly across a lost frame (#363)\n");

    /* (1) L6E: lose the last frame of the ST7 message (t0+10) and the head of
     * the ST4 message (t0+11); its second frame (t0+12) used to complete ST7 */
    {
        static const int drop[2] = {10, 11};

        len = read_file("l6route_g5.sbf", &buf);
        CHECK(len > 0, "read l6route_g5.sbf");
        if (len > 0) {
            n0 = run_l6e(buf, len, NULL, 12, 18, &changed);
            CHECK(n0 > 0, "L6E: messages decoded from the ST3 message at t0+18 on");
            n1 = run_l6e(buf, len, drop, 12, 18, &changed);
            CHECK(!changed, "L6E: the frame after the gap must not change rtcm->ssr");
            CHECK(n1 == n0, "L6E: messages from the next subframe indicator decode as without the loss");
            printf("  (1) L6E PRN 205: decoded from t0+18 %d / %d, ssr changed after gap %d\n", n1, n0, changed);
        }
        free(buf);
    }
    /* (2) L6D iono: lose the second frame of the first coverage message
     * (t0+2) and the head of the next (t0+3); its second frame (t0+4) used to
     * complete the first one */
    {
        static const int drop[2] = {2, 3};

        len = read_file("l6route_iono.sbf", &buf);
        CHECK(len > 0, "read l6route_iono.sbf");
        if (len > 0) {
            n0 = run_iono(buf, len, NULL, 4, 6, &changed);
            CHECK(n0 > 0, "iono: messages decoded from the coverage message at t0+6 on");
            n1 = run_iono(buf, len, drop, 4, 6, &changed);
            CHECK(!changed, "iono: the frame after the gap must not change the decoded region");
            CHECK(n1 == n0, "iono: messages from the next subframe indicator decode as without the loss");
            printf("  (2) L6D iono PRN 200: decoded from t0+6 %d / %d, region changed after gap %d\n", n1, n0, changed);
        }
        free(buf);
    }

    if (fails) {
        printf("utest_madoca_l6gap: %d FAILED\n", fails);
        return 1;
    }
    printf("utest_madoca_l6gap: all passed\n");
    return 0;
}
