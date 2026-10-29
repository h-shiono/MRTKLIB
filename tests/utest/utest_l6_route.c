/*------------------------------------------------------------------------------
 * unit test : QZSS L6 frame routing in the UBX/SBF receiver decoders (#351)
 *
 * Feeds short receiver streams through input_sbf()/input_ubx() and counts the
 * return codes: 10 CLAS frame, 15 MADOCA-PPP SSR decoded, 16 MADOCA-PPP iono
 * frame, 17 QZNMA frame. Fixtures (tests/utest/l6route_*) are built by
 * scripts/tests/make_l6_route_fixtures.py, which also prints the per-service
 * frame counts asserted here:
 *
 *   l6route_g5.sbf        G5P3287a 4270/4271  CLAS 60, MADOCA 48, QZNMA 12
 *   l6route_iono.sbf      G5P3186k 4270       MADOCA iono (PRN 200) 30
 *   l6route_mcla.sbf      MCLA286e 4069       CLAS 30 (mosaic-CLAS, Source=1)
 *   l6route_d9c.ubx       D9C1286m RXM-QZSSL6 CLAS 60 (two L6D channels)
 *   l6route_ubx_iono.ubx  PRN 200 iono frames re-wrapped as UBX L6D: 30 (D1,
 *                         was routed to the CLAS demux as 10)
 *   l6route_sbf_197.sbf   CLAS frames re-wrapped as QZS5 / PRN 197: 30 (D2,
 *                         was routed to the iono decoder as 16)
 *   l6route_sbf_src0.sbf  MCLA286e CLAS blocks with SBF Source = 0: 30
 *
 * MADOCA frames return 0 until a message completes, so 15 is only required to
 * occur; the exact 10/16/17 counts prove that no MADOCA or QZNMA frame leaks
 * into the CLAS or iono paths. For SBF, every 10/16 result must set
 * raw->ephsat to the block's satellite whatever the Source field says.
 *
 * Consumers of the routing (not reachable through the receiver decoders):
 *   - clas_input_cssr(): a MADOCA / QZNMA frame must leave the CLAS subframe
 *     assembly and delivery state untouched (D3), using the real frames in
 *     l6_frames.bin (CLAS, MADOCA, MADOCA iono, QZNMA);
 *   - clas_input_cssr(): a null message (#344), alone or inside a CLAS
 *     subframe, must leave the assembly and delivery state untouched;
 *   - rtksvr_ssr_applicable(): in CLAS mode MADOCA SSR from a raw L6E stream
 *     (code 10) or a UBX/SBF L6E frame (code 15) must not be applied to
 *     nav.ssr_ch[0], while RTCM3 SSR still is (D4).
 * Null messages (#344): no capture contains one, so the real fixtures are
 * rewritten in memory with every L6 frame replaced by a null message (UBX
 * checksum / SBF CRC recomputed). L6D null frames must route nowhere (no 10:
 * the CLAS demux would read msgid 0 as transmit pattern 0); L6E null frames go
 * to the MADOCA decoder, which reports the alert without decoding (no 15).
 * Also checks the auxiliary L6 PRN table.
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_clas.h"
#include "mrtklib/rtklib.h"

static int fails = 0;

#define CHECK(cond, msg)                 \
    do {                                 \
        if (!(cond)) {                   \
            printf("FAIL: %s\n", (msg)); \
            fails++;                     \
        }                                \
    } while (0)

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

/* feed a stream through a receiver decoder; count return codes 10/15/16/17.
 * n[0] counts SBF 10/16 results whose raw->ephsat is not the block satellite */
static int route_buf(const uint8_t* buf, long len, int format, int* n) {
    raw_t* raw = (raw_t*)calloc(1, sizeof(raw_t));
    rtcm_t* rtcm = (rtcm_t*)calloc(1, sizeof(rtcm_t)); /* ~7.5 MB: heap only */
    long i;
    int ret;

    n[0] = n[10] = n[15] = n[16] = n[17] = 0;
    if (!raw || !rtcm) {
        free(raw);
        free(rtcm);
        return 0;
    }
    init_raw(raw, format);
    init_rtcm(rtcm);
    for (i = 0; i < len; i++) {
        ret = format == STRFMT_UBX ? input_ubx(raw, rtcm, buf[i]) : input_sbf(raw, rtcm, buf[i]);
        if (ret >= 10 && ret <= 17) n[ret]++;
        /* SBF QZSRawL6: SVID at block byte 14, prn = svid - 180 + 192 */
        if (format == STRFMT_SEPT && (ret == 10 || ret == 16) &&
            raw->ephsat != satno(SYS_QZS, raw->buff[14] - 180 + 192)) {
            n[0]++;
        }
    }
    free_rtcm(rtcm);
    free_raw(raw);
    free(raw);
    free(rtcm);
    return 1;
}

static int route_file(const char* file, int format, int* n) {
    uint8_t* buf;
    long len = read_file(file, &buf);
    int ok = len > 0 && route_buf(buf, len, format, n);

    free(buf);
    return ok;
}

/* overwrite an L6 frame (L6_FRAME_LEN bytes from the preamble) with a null
 * message, keeping the preamble and PRN */
static void make_null(uint8_t* frame) {
    frame[5] = 0;
    memset(frame + 6, 0xAA, L6_FRAME_LEN - 6);
}

/* replace every L6 frame of a UBX / SBF stream with a null message; returns
 * the number of frames replaced */
static int nullify_stream(uint8_t* buf, long len, int format) {
    uint8_t frame[L6_FRAME_LEN];
    long i = 0, n;
    int k, nnull = 0, id;

    while (i + 8 <= len) {
        if (format == STRFMT_UBX) {
            if (buf[i] != 0xB5 || buf[i + 1] != 0x62) {
                i++;
                continue;
            }
            n = buf[i + 4] | (buf[i + 5] << 8);
            if (i + 8 + n > len) break;
            if (buf[i + 2] == 0x02 && buf[i + 3] == 0x73 && n >= 14 + L6_RAW_LEN) {
                uint8_t a = 0, b = 0;
                make_null(buf + i + 6 + 14);
                for (k = 2; k < 6 + n; k++) a += buf[i + k], b += a;
                buf[i + 6 + n] = a;
                buf[i + 7 + n] = b;
                nnull++;
            }
            i += 8 + n;
        } else {
            if (buf[i] != '$' || buf[i + 1] != '@') {
                i++;
                continue;
            }
            id = (buf[i + 4] | (buf[i + 5] << 8)) & 0x1FFF;
            n = buf[i + 6] | (buf[i + 7] << 8);
            if (n < 8 || i + n > len) break;
            if ((id == 4069 || id == 4270 || id == 4271) && n >= 272) {
                /* NAVBits from block byte 20: big-endian 32-bit words stored
                 * little-endian, so frame byte j sits at 20 + 4(j/4) + 3 - j%4 */
                uint16_t crc;
                for (k = 0; k < L6_FRAME_LEN; k++) frame[k] = buf[i + 20 + 4 * (k / 4) + 3 - k % 4];
                make_null(frame);
                for (k = 0; k < L6_FRAME_LEN; k++) buf[i + 20 + 4 * (k / 4) + 3 - k % 4] = frame[k];
                crc = rtk_crc16(buf + i + 4, (int)n - 4);
                buf[i + 2] = crc & 0xFF;
                buf[i + 3] = crc >> 8;
                nnull++;
            }
            i += n;
        }
    }
    return nnull;
}

/* #344: null messages from a receiver container. All frames of the fixture are
 * nulled; L6D must give no 10 / 16, L6E no 15, QZNMA none */
static void test_route_null(const char* file, int format, int nframe) {
    uint8_t* buf;
    long len = read_file(file, &buf);
    int n[18], nnull;
    char msg[160];

    if (!len) {
        snprintf(msg, sizeof(msg), "%s: open", file);
        CHECK(0, msg);
        return;
    }
    nnull = nullify_stream(buf, len, format);
    snprintf(msg, sizeof(msg), "%s: %d frames nulled, expected %d", file, nnull, nframe);
    CHECK(nnull == nframe, msg);
    CHECK(route_buf(buf, len, format, n), "route_buf: alloc");
    snprintf(msg, sizeof(msg), "%s (null): 10=%d 15=%d 16=%d 17=%d, expected none", file, n[10], n[15], n[16], n[17]);
    CHECK(n[10] == 0 && n[15] == 0 && n[16] == 0 && n[17] == 0, msg);
    free(buf);
    printf("ok  : %-22s null frames routed nowhere (%d)\n", file, nnull);
}

static void test_route(const char* file, int format, int n10, int n16, int n17, int need15) {
    int n[18];
    char msg[128];

    if (!route_file(file, format, n)) {
        snprintf(msg, sizeof(msg), "%s: open", file);
        CHECK(0, msg);
        return;
    }
    snprintf(msg, sizeof(msg), "%s: CLAS frames -> 10 (%d, expected %d)", file, n[10], n10);
    CHECK(n[10] == n10, msg);
    snprintf(msg, sizeof(msg), "%s: MADOCA iono frames -> 16 (%d, expected %d)", file, n[16], n16);
    CHECK(n[16] == n16, msg);
    snprintf(msg, sizeof(msg), "%s: QZNMA frames -> 17 (%d, expected %d)", file, n[17], n17);
    CHECK(n[17] == n17, msg);
    snprintf(msg, sizeof(msg), "%s: ephsat set for every L6D frame (%d mismatches)", file, n[0]);
    CHECK(n[0] == 0, msg);
    if (need15) {
        snprintf(msg, sizeof(msg), "%s: MADOCA SSR decoded -> 15 (%d)", file, n[15]);
        CHECK(n[15] > 0, msg);
    }
    printf("ok  : %-22s 10=%d 15=%d 16=%d 17=%d\n", file, n[10], n[15], n[16], n[17]);
}

/* D3: foreign-vendor frames must not touch the CLAS assembly / delivery state */
static void test_clas_input_reject(void) {
    uint8_t buf[4 * L6_FRAME_LEN];
    clas_ctx_t* ctx = (clas_ctx_t*)calloc(1, sizeof(clas_ctx_t));
    clas_l6buf_t* l6;
    FILE* fp;
    int k, i, nframe, havebit;

    if (!ctx || !(fp = fopen("l6_frames.bin", "rb"))) {
        CHECK(0, "clas_input_cssr: setup");
        free(ctx);
        return;
    }
    if (fread(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
        CHECK(0, "clas_input_cssr: read l6_frames.bin");
        fclose(fp);
        free(ctx);
        return;
    }
    fclose(fp);
    l6 = &ctx->l6buf[0];
    ctx->l6delivery[0] = ctx->l6facility[0] = -1;

    /* frames in l6_frames.bin: 0 CLAS (PRN 195, subframe head), 1 MADOCA,
     * 2 MADOCA iono, 3 QZNMA; each is fed as 218 bytes + 32 parity bytes */
    for (k = 1; k < 4; k++) {
        for (i = 0; i < L6_RAW_LEN; i++) clas_input_cssr(ctx, i < L6_FRAME_LEN ? buf[k * L6_FRAME_LEN + i] : 0, 0);
    }
    CHECK(ctx->l6delivery[0] == -1 && ctx->l6facility[0] == -1, "foreign frames leave delivery state untouched");
    CHECK(l6->decode_start == 0 && l6->nframe == 0 && l6->havebit == 0, "foreign frames start no subframe");

    for (i = 0; i < L6_RAW_LEN; i++) clas_input_cssr(ctx, i < L6_FRAME_LEN ? buf[i] : 0, 0);
    CHECK(ctx->l6delivery[0] == 195 && l6->decode_start == 1 && l6->nframe == 1 && l6->havebit == L6_DATA_BITS,
          "CLAS frame is assembled");
    nframe = l6->nframe;
    havebit = l6->havebit;
    for (k = 1; k < 4; k++) {
        for (i = 0; i < L6_RAW_LEN; i++) clas_input_cssr(ctx, i < L6_FRAME_LEN ? buf[k * L6_FRAME_LEN + i] : 0, 0);
    }
    CHECK(ctx->l6delivery[0] == 195 && l6->nframe == nframe && l6->havebit == havebit,
          "foreign frames inside a CLAS subframe are skipped");
    free(ctx);
    printf("ok  : clas_input_cssr rejects MADOCA / iono / QZNMA frames\n");
}

/* #344: a null message on a CLAS input is dropped, before and inside a
 * subframe, without touching the assembly or delivery state */
static void test_clas_input_null(void) {
    uint8_t clas[L6_FRAME_LEN], null[L6_FRAME_LEN];
    clas_ctx_t* ctx = (clas_ctx_t*)calloc(1, sizeof(clas_ctx_t));
    clas_l6buf_t* l6;
    FILE* fp;
    int i, ok;

    if (!ctx || !(fp = fopen("l6_frames.bin", "rb"))) {
        CHECK(0, "clas_input_cssr null: setup");
        free(ctx);
        return;
    }
    ok = fread(clas, 1, sizeof(clas), fp) == sizeof(clas);
    fclose(fp);
    if (!ok) {
        CHECK(0, "clas_input_cssr null: read l6_frames.bin");
        free(ctx);
        return;
    }
    memcpy(null, clas, sizeof(null)); /* PRN 195 */
    make_null(null);
    l6 = &ctx->l6buf[0];
    ctx->l6delivery[0] = ctx->l6facility[0] = -1;

    for (i = 0; i < L6_RAW_LEN; i++) clas_input_cssr(ctx, i < L6_FRAME_LEN ? null[i] : 0, 0);
    CHECK(ctx->l6delivery[0] == -1 && ctx->l6facility[0] == -1, "null frame leaves delivery state untouched");
    CHECK(l6->decode_start == 0 && l6->nframe == 0 && l6->havebit == 0, "null frame starts no subframe");

    for (i = 0; i < L6_RAW_LEN; i++) clas_input_cssr(ctx, i < L6_FRAME_LEN ? clas[i] : 0, 0);
    ctx->l6facility[0] = 99; /* a null frame (msgid 0) must not overwrite it */
    for (i = 0; i < L6_RAW_LEN; i++) clas_input_cssr(ctx, i < L6_FRAME_LEN ? null[i] : 0, 0);
    CHECK(ctx->l6delivery[0] == 195 && ctx->l6facility[0] == 99 && l6->nframe == 1 && l6->havebit == L6_DATA_BITS,
          "null frame inside a CLAS subframe is skipped");
    free(ctx);
    printf("ok  : clas_input_cssr drops null frames\n");
}

/* D4: SSR ownership of nav.ssr_ch[0] in CLAS mode */
static void test_ssr_applicable(void) {
    rtksvr_t* svr = (rtksvr_t*)calloc(1, sizeof(rtksvr_t)); /* ~51 MB: heap only */
    clas_ctx_t* ctx = (clas_ctx_t*)calloc(1, sizeof(clas_ctx_t));

    if (!svr || !ctx) {
        CHECK(0, "rtksvr_ssr_applicable: setup");
        free(svr);
        free(ctx);
        return;
    }
    svr->format[0] = STRFMT_L6E;
    svr->format[1] = STRFMT_RTCM3;
    svr->format[2] = STRFMT_SEPT;

    svr->clas = NULL; /* PPP / MADOCA mode: every SSR source applies */
    CHECK(rtksvr_ssr_applicable(svr, 0, 10), "no CLAS: raw L6E MADOCA SSR applied");
    CHECK(rtksvr_ssr_applicable(svr, 1, 10), "no CLAS: RTCM3 SSR applied");
    CHECK(rtksvr_ssr_applicable(svr, 2, 15), "no CLAS: SBF L6E MADOCA SSR applied");

    svr->clas = ctx; /* CLAS / PPP-RTK mode: CLAS owns ssr_ch[0] */
    CHECK(!rtksvr_ssr_applicable(svr, 0, 10), "CLAS: raw L6E MADOCA SSR not applied");
    CHECK(rtksvr_ssr_applicable(svr, 1, 10), "CLAS: RTCM3 SSR still applied");
    CHECK(!rtksvr_ssr_applicable(svr, 2, 15), "CLAS: SBF L6E MADOCA SSR not applied");
    free(svr);
    free(ctx);
    printf("ok  : rtksvr_ssr_applicable\n");
}

static void test_prn_table(void) {
    const l6_prn_info_t* p;
    l6_header_t h = {0};

    CHECK((p = l6_prn_info(200)) && p->iono_slot == 0 && !p->l6e, "PRN 200: L6D iono slot 0");
    CHECK((p = l6_prn_info(201)) && p->iono_slot == 1, "PRN 201: iono slot 1");
    CHECK((p = l6_prn_info(197)) && p->svc == L6_SVC_CLAS && p->iono_slot < 0, "PRN 197: CLAS, no iono slot");
    CHECK((p = l6_prn_info(211)) && p->l6e && p->pair_prn == 201, "PRN 211: L6E of QZS7");
    CHECK(l6_prn_info(198) == NULL && l6_prn_info(208) == NULL, "PRN 198/208: unassigned");

    h.prn = 195, h.svc = L6_SVC_CLAS;
    CHECK(l6_service_expected(&h), "CLAS on 195 expected");
    h.prn = 205, h.svc = L6_SVC_QZNMA;
    CHECK(l6_service_expected(&h), "QZNMA on L6E 205 expected");
    h.prn = 205, h.svc = L6_SVC_CLAS;
    CHECK(!l6_service_expected(&h), "CLAS on L6E 205 not expected");
    h.prn = 200, h.svc = L6_SVC_CLAS;
    CHECK(!l6_service_expected(&h), "CLAS on iono PRN 200 not expected");
    h.prn = 198, h.svc = L6_SVC_NULL;
    CHECK(l6_service_expected(&h), "null message always expected");
    printf("ok  : L6 PRN table\n");
}

int main(void) {
    test_prn_table();
    test_route("l6route_g5.sbf", STRFMT_SEPT, 60, 0, 12, 1);
    test_route("l6route_iono.sbf", STRFMT_SEPT, 0, 30, 0, 0);
    test_route("l6route_mcla.sbf", STRFMT_SEPT, 30, 0, 0, 0);
    test_route("l6route_d9c.ubx", STRFMT_UBX, 60, 0, 0, 0);
    test_route("l6route_ubx_iono.ubx", STRFMT_UBX, 0, 30, 0, 0); /* D1 */
    test_route("l6route_sbf_197.sbf", STRFMT_SEPT, 30, 0, 0, 0); /* D2 */
    test_route("l6route_sbf_src0.sbf", STRFMT_SEPT, 30, 0, 0, 0);
    test_route_null("l6route_d9c.ubx", STRFMT_UBX, 60);
    test_route_null("l6route_mcla.sbf", STRFMT_SEPT, 30);
    test_route_null("l6route_g5.sbf", STRFMT_SEPT, 120);
    test_clas_input_reject();
    test_clas_input_null();
    test_ssr_applicable();
    if (fails) {
        printf("%d check(s) FAILED\n", fails);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
