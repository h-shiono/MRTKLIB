/*------------------------------------------------------------------------------
 * unit test : DGS QZSS L6 frame container (DATAGNSS QZS-6C / MA-10P)
 *
 * dgs_decode_l6() on the fixtures of scripts/tests/make_dgs_fixtures.py:
 *
 *   l6route_ma10p.dgs    real MA-10P slice, GPS week 2446, TOW 197986..198015:
 *                        60 frames (PRN 195 / 199 alternating), the two frames
 *                        of TOW 198000 with the Reed-Solomon failure flag,
 *                        four RTCM3 1058 messages in between
 *   l6route_dgs_l6e.dgs  30 re-wrapped L6E frames (PRN 205) plus one frame
 *                        with a corrupted checksum, one flagged week and one
 *                        flagged time of week not confirmed
 *
 * The week and time of week are big-endian in an otherwise little-endian
 * container: the expected values below fail if they are read little-endian.
 *
 * input_dgs() (STRFMT_DGS) on the same fixtures: return codes as for the UBX /
 * SBF L6 messages (10 CLAS frame, 15 MADOCA-PPP SSR decoded, 17 QZNMA frame,
 * -1 Reed-Solomon failure), with raw->l6tag, raw->time and raw->ephsat set.
 * The L6E fixture is re-wrapped from l6route_ubx_l6e.ubx, so its MADOCA-PPP SSR
 * and QZNMA counts must equal the ones of input_ubx() on the original, each
 * run from a fresh MADOCA decoder state. A truncated frame right
 * before a complete one must not cost the complete one.
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

/* recompute the checksum of a DGS frame after editing it */
static void fix_checksum(uint8_t* frame) {
    uint8_t c1 = 0, c2 = 0;
    int i;

    for (i = 2; i < DGS_FRAME_LEN - 2; i++) {
        c1 = (uint8_t)(c1 + frame[i]);
        c2 = (uint8_t)(c2 + c1);
    }
    frame[DGS_FRAME_LEN - 2] = c1;
    frame[DGS_FRAME_LEN - 1] = c2;
}

static void test_real_slice(void) {
    uint8_t* buf;
    long len = read_file("l6route_ma10p.dgs", &buf), i;
    dgs_l6_t dgs;
    l6_header_t hdr;
    int n = 0, nrs = 0, nprn[2] = {0, 0}, nhdr = 0, nclas = 0;
    double tow_first = 0.0, tow_last = 0.0, tow_rs = 0.0;

    CHECK(len == 16684, "l6route_ma10p.dgs readable");
    for (i = 0; i + DGS_FRAME_LEN <= len;) {
        if (!dgs_decode_l6(buf + i, &dgs)) {
            i++;
            continue;
        }
        if (n == 0) {
            tow_first = dgs.tow;
            CHECK(dgs.prn == 195 && dgs.week == 2446 && dgs.snr == 34 && dgs.flag == 0, "first frame fields");
            CHECK(dgs.l6 == buf + i + 18, "L6 frame offset in the container");
        }
        tow_last = dgs.tow;
        if (dgs.prn == 195) nprn[0]++;
        if (dgs.prn == 199) nprn[1]++;
        if (dgs.flag & DGS_FLAG_RS) {
            nrs++;
            tow_rs = dgs.tow;
        }
        if (l6_parse_header(dgs.l6, &hdr) && hdr.prn == dgs.prn) nhdr++;
        if (hdr.svc == L6_SVC_CLAS) nclas++;
        n++;
        i += DGS_FRAME_LEN;
    }
    CHECK(n == 60 && nprn[0] == 30 && nprn[1] == 30, "60 frames, 30 per PRN");
    CHECK(nhdr == 60, "SVN - 700 equals the L6 header PRN in every frame");
    CHECK(nclas == 60, "every frame is a CLAS frame");
    CHECK(tow_first == 197986.0 && tow_last == 198015.0, "time of week is big-endian");
    CHECK(nrs == 2 && tow_rs == 198000.0, "two Reed-Solomon failures at TOW 198000");
    free(buf);
}

static void test_rewrapped(void) {
    uint8_t* buf;
    long len = read_file("l6route_dgs_l6e.dgs", &buf), i;
    dgs_l6_t dgs;
    int n = 0, nok = 0, nweek = 0, ntow = 0;

    CHECK(len == 10160, "l6route_dgs_l6e.dgs readable");
    for (i = 0; i + DGS_FRAME_LEN <= len;) {
        if (!dgs_decode_l6(buf + i, &dgs)) {
            i++;
            continue;
        }
        n++;
        if (!(dgs.flag & DGS_FLAG_UNUSABLE)) nok++;
        if (dgs.flag == DGS_FLAG_WEEK) nweek++;
        if (dgs.flag == DGS_FLAG_TOW) ntow++;
        CHECK(dgs.prn == 205 && dgs.week == 2446, "re-wrapped frame PRN and week");
        i += DGS_FRAME_LEN;
    }
    /* 34 sync sequences: one stray, one frame with a corrupted checksum */
    CHECK(n == 32 && nok == 30 && nweek == 1 && ntow == 1, "30 usable + 2 flagged frames");
    free(buf);
}

static void test_invalid(void) {
    uint8_t* buf;
    uint8_t f[DGS_FRAME_LEN];
    long len = read_file("l6route_ma10p.dgs", &buf);
    dgs_l6_t dgs;
    int i;

    if (len < DGS_FRAME_LEN) {
        CHECK(0, "fixture for the invalid-frame cases");
        free(buf);
        return;
    }
    memcpy(f, buf, DGS_FRAME_LEN);
    CHECK(dgs_decode_l6(f, &dgs), "reference frame is valid");

    /* every single-byte corruption of the checksummed part is rejected */
    for (i = 0; i < DGS_FRAME_LEN; i++) {
        memcpy(f, buf, DGS_FRAME_LEN);
        f[i] ^= 0x01;
        if (dgs_decode_l6(f, &dgs)) break;
    }
    CHECK(i == DGS_FRAME_LEN, "single-bit error in any byte rejected");

    /* consistent checksum but another message or a wrong length */
    memcpy(f, buf, DGS_FRAME_LEN);
    f[3] = 0x57;
    fix_checksum(f);
    CHECK(!dgs_decode_l6(f, &dgs), "other message id rejected");

    memcpy(f, buf, DGS_FRAME_LEN);
    f[4] = 0x00;
    fix_checksum(f);
    CHECK(!dgs_decode_l6(f, &dgs), "wrong payload length rejected");

    memcpy(f, buf, DGS_FRAME_LEN);
    f[9] = 64;
    fix_checksum(f);
    CHECK(!dgs_decode_l6(f, &dgs), "wrong data length rejected");

    /* the status flags do not invalidate the container */
    memcpy(f, buf, DGS_FRAME_LEN);
    f[17] = DGS_FLAG_RS | DGS_FLAG_WEEK | DGS_FLAG_TOW;
    fix_checksum(f);
    CHECK(dgs_decode_l6(f, &dgs) && dgs.flag == DGS_FLAG_UNUSABLE, "flags reported, not applied");
    free(buf);
}

/* result of feeding a stream through input_dgs() / input_ubx() */
typedef struct {
    int n[18];     /* count of return codes 10..17 */
    int nerr;      /* count of return code -1 */
    int nframe;    /* frames delivered to the L6 routing (new raw->l6tag or L6 PRN) */
    int nsat;      /* 10 / 16 results with raw->ephsat = satellite of the L6 header PRN */
    double tag[2]; /* first and last raw->l6tag */
    gtime_t time;  /* last raw->time */
} feed_t;

static int feed(const uint8_t* buf, long len, int format, feed_t* r) {
    raw_t* raw = (raw_t*)calloc(1, sizeof(raw_t));
    rtcm_t* rtcm = (rtcm_t*)calloc(1, sizeof(rtcm_t)); /* ~7.5 MB: heap only */
    double tag = 0.0;
    long i;
    int ret, prn = 0;

    memset(r, 0, sizeof(*r));
    if (!raw || !rtcm) {
        free(raw);
        free(rtcm);
        return 0;
    }
    init_raw(raw, format);
    init_rtcm(rtcm);
    init_mcssr(gpst2time(2446, 0.0)); /* MADOCA decoder state is global: a fresh one per stream */
    for (i = 0; i < len; i++) {
        ret = format == STRFMT_DGS ? input_dgs(raw, rtcm, buf[i]) : input_ubx(raw, rtcm, buf[i]);
        if (ret >= 10 && ret <= 17) r->n[ret]++;
        if (ret == -1) r->nerr++;
        if ((ret == 10 || ret == 16) && raw->ephsat == satno(SYS_QZS, rtcm->buff[4])) r->nsat++;
        if (raw->l6tag != tag || rtcm->buff[4] != prn) {
            if (!r->nframe) r->tag[0] = raw->l6tag;
            tag = r->tag[1] = raw->l6tag;
            prn = rtcm->buff[4];
            r->nframe++;
        }
    }
    r->time = raw->time;
    free_rtcm(rtcm);
    free_raw(raw);
    free(raw);
    free(rtcm);
    return 1;
}

static void test_input_real(void) {
    uint8_t *buf, *cut;
    long len = read_file("l6route_ma10p.dgs", &buf);
    feed_t r;
    int week;
    double tow;

    CHECK(len > 0 && feed(buf, len, STRFMT_DGS, &r), "feed l6route_ma10p.dgs");
    CHECK(r.n[10] == 58 && r.nerr == 2, "58 CLAS frames, 2 Reed-Solomon failures");
    CHECK(r.n[15] == 0 && r.n[16] == 0 && r.n[17] == 0, "no other service");
    CHECK(r.nsat == 58, "ephsat is the satellite of the L6 header PRN");
    CHECK(r.nframe == 58 && r.tag[0] == 2446 * 604800.0 + 197986.0 && r.tag[1] == 2446 * 604800.0 + 198015.0,
          "frame time tags");
    tow = time2gpst(r.time, &week);
    CHECK(week == 2446 && tow == 198015.0, "raw->time from the frame");

    /* first 100 bytes of a frame directly followed by a complete frame */
    if (len >= 2 * DGS_FRAME_LEN && (cut = (uint8_t*)malloc(100 + DGS_FRAME_LEN))) {
        memcpy(cut, buf, 100);
        memcpy(cut + 100, buf + DGS_FRAME_LEN, DGS_FRAME_LEN);
        CHECK(feed(cut, 100 + DGS_FRAME_LEN, STRFMT_DGS, &r) && r.n[10] == 1 && r.nerr == 0,
              "complete frame after a truncated one");
        free(cut);
    } else {
        CHECK(0, "truncated-frame case");
    }
    free(buf);
}

static void test_input_rewrapped(void) {
    uint8_t *dgs, *ubx;
    long nd = read_file("l6route_dgs_l6e.dgs", &dgs), nu = read_file("l6route_ubx_l6e.ubx", &ubx);
    feed_t rd, ru;

    CHECK(nu > 0 && feed(ubx, nu, STRFMT_UBX, &ru), "feed l6route_ubx_l6e.ubx");
    CHECK(nd > 0 && feed(dgs, nd, STRFMT_DGS, &rd), "feed l6route_dgs_l6e.dgs");
    /* the stray sync sequence, the corrupted frame and the two frames with
     * an unconfirmed time deliver nothing: 30 frames, as in the UBX original */
    CHECK(rd.nframe == 30 && ru.nframe == 30 && rd.nerr == 0, "30 frames delivered");
    CHECK(rd.n[15] > 0 && rd.n[15] == ru.n[15], "MADOCA-PPP SSR decoded as through input_ubx()");
    CHECK(rd.n[17] > 0 && rd.n[17] == ru.n[17], "QZNMA frames as through input_ubx()");
    CHECK(rd.n[10] == 0 && rd.n[16] == 0, "no L6E frame routed as CLAS or iono");
    free(dgs);
    free(ubx);
}

int main(void) {
    test_real_slice();
    test_rewrapped();
    test_invalid();
    test_input_real();
    test_input_rewrapped();
    if (fails) {
        printf("%d check(s) FAILED\n", fails);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
