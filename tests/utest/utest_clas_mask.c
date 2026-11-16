/*------------------------------------------------------------------------------
 * unit test : CLAS CSSR ST1 mask bounds (#276)
 *
 * The ST1 satellite mask fixes the satellite lists of ST2-ST12. The decoder
 * stores them in cellmask[CSSR_MAX_SV] and sat[CSSR_MAX_SV] (64) and indexes
 * dec_ssr[sat - 1], so a mask must be dropped when
 *   (1) it holds more than 64 satellites (GPS 32 + Galileo 36): cellmask[64..]
 *       ran into net_svmask[], and sat[64..] past the stack of every decoder
 *   (2) a satellite has no satellite number (GPS PRN 33): dec_ssr[-1]
 *   (3) a GNSS ID is unknown (ID 6): every satellite of it has number 0
 *   (4) a GNSS ID is given twice: cellmask[] no longer follows the satellites
 * A dropped mask leaves no satellite, invalidates the IOD and drops the rest of
 * the subframe, so the clock that follows it does not decode. Accepted:
 *   (5) exactly 64 satellites (GPS 32 + Galileo 32), clock of E32 decoded
 *   (6) a valid mask after a dropped one decodes again
 *
 * The corruption of (1) stays inside clas_ctx_t, where ASan does not see it;
 * a sentinel in net_svmask[] does.
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
        } else {                         \
            printf("ok  : %s\n", (msg)); \
        }                                \
    } while (0)

#define WEEK 2068
#define SFBITS (1695 * 5)
#define IOD 3
#define TOW 7170
#define SENTINEL 0x5A5A5A5A5A5A5A5AULL

static uint8_t sb[(SFBITS + 7) / 8];
static int sbn;

typedef struct {
    int id;      /* CSSR GNSS ID */
    uint32_t hi; /* satellite mask bits 39-32 */
    uint32_t lo; /* satellite mask bits 31-0 */
} gnss_mask_t;

static void put(int nbit, uint32_t v) {
    setbitu(sb, sbn, nbit, v);
    sbn += nbit;
}

/* ST1 mask, one signal per GNSS, no cell mask */
static void put_mask(const gnss_mask_t* g, int ngnss) {
    int k;
    put(12, 4073);
    put(4, 1);
    put(20, TOW);
    put(4, 0);   /* update interval */
    put(1, 1);   /* multiple message */
    put(4, IOD); /* IOD SSR */
    put(4, (uint32_t)ngnss);
    for (k = 0; k < ngnss; k++) {
        put(4, (uint32_t)g[k].id);
        put(8, g[k].hi);
        put(32, g[k].lo);
        put(16, 0x8000);
        put(1, 0);
    }
}

/* ST3 clock of nsat satellites, last message of the subframe */
static void put_clock(int nsat) {
    int j;
    put(12, 4073);
    put(4, 3);
    put(12, TOW % 3600);
    put(4, 0);
    put(1, 0);
    put(4, IOD);
    for (j = 0; j < nsat; j++) {
        put(15, 100);
    }
}

/* input the subframe in sb, return number of messages decoded */
static int input_subframe(clas_ctx_t* ctx) {
    uint8_t fr[250] = {0x1A, 0xCF, 0xFC, 0x1D, 193};
    int i, n, nmsg = 0;

    for (n = 0; n < 5; n++) {
        fr[5] = n == 0 ? 0xA1 : 0xA0; /* CLAS, subframe indicator on frame 1 */
        fr[6] = (uint8_t)getbitu(sb, 1695 * n, 7);
        for (i = 0; i < 211; i++) {
            fr[7 + i] = (uint8_t)getbitu(sb, 1695 * n + 7 + 8 * i, 8);
        }
        for (i = 0; i < 250; i++) {
            clas_input_cssr(ctx, fr[i], 0);
            while (clas_decode_msg(ctx, 0) == 10) {
                nmsg++;
            }
        }
    }
    return nmsg;
}

static clas_ctx_t* new_ctx(void) {
    clas_ctx_t* ctx = (clas_ctx_t*)calloc(1, sizeof(clas_ctx_t));
    int k;
    if (!ctx || clas_ctx_init(ctx) != 0) {
        printf("FAIL: clas_ctx_init\n");
        exit(1);
    }
    clas_set_week_ref(ctx, WEEK);
    for (k = 0; k < CSSR_MAX_NET; k++) {
        ctx->cssr[0].net_svmask[k] = SENTINEL;
    }
    return ctx;
}

static int mask_empty(const cssr_t* cssr) {
    int k;
    for (k = 0; k < CSSR_MAX_GNSS; k++) {
        if (cssr->svmask[k] || cssr->sigmask[k] || cssr->cmi[k]) {
            return 0;
        }
    }
    for (k = 0; k < CSSR_MAX_SV; k++) {
        if (cssr->cellmask[k]) {
            return 0;
        }
    }
    return 1;
}

static int sentinel_intact(const cssr_t* cssr) {
    int k;
    for (k = 0; k < CSSR_MAX_NET; k++) {
        if (cssr->net_svmask[k] != SENTINEL) {
            return 0;
        }
    }
    return 1;
}

/* input a mask and a clock of nclk satellites, check the mask was dropped */
static void check_dropped(const char* name, const gnss_mask_t* g, int ngnss, int nclk) {
    clas_ctx_t* ctx = new_ctx();
    char msg[160];
    int nmsg;

    memset(sb, 0, sizeof(sb));
    sbn = 0;
    put_mask(g, ngnss);
    put_clock(nclk);
    nmsg = input_subframe(ctx);

    snprintf(msg, sizeof(msg), "%s: mask dropped (no satellite left)", name);
    CHECK(mask_empty(&ctx->cssr[0]), msg);
    snprintf(msg, sizeof(msg), "%s: IOD invalidated (%d)", name, ctx->cssr[0].iod);
    CHECK(ctx->cssr[0].iod == -1, msg);
    snprintf(msg, sizeof(msg), "%s: clock after it not decoded (%d msg)", name, nmsg);
    CHECK(nmsg == 0 && !ctx->dec_ssr[0].update_cc, msg);
    snprintf(msg, sizeof(msg), "%s: net_svmask[] after cellmask[] untouched", name);
    CHECK(sentinel_intact(&ctx->cssr[0]), msg);

    clas_ctx_free(ctx);
    free(ctx);
}

static void test_dropped(void) {
    /* (1) GPS G01-G32 + Galileo E01-E36 = 68 satellites */
    static const gnss_mask_t over[2] = {{CSSR_SYS_GPS, 0xFF, 0xFFFFFF00}, {CSSR_SYS_GAL, 0xFF, 0xFFFFFFF0}};
    /* (2) G01 and G33 */
    static const gnss_mask_t prn33[1] = {{CSSR_SYS_GPS, 0x80, 0x80}};
    /* (3) one satellite of GNSS ID 6 */
    static const gnss_mask_t unknown[2] = {{CSSR_SYS_GPS, 0x80, 0}, {6, 0x80, 0}};
    /* (4) GPS twice */
    static const gnss_mask_t twice[2] = {{CSSR_SYS_GPS, 0x80, 0}, {CSSR_SYS_GPS, 0x40, 0}};

    check_dropped("(1) 68 satellites", over, 2, 68);
    check_dropped("(2) GPS PRN 33", prn33, 1, 2);
    check_dropped("(3) GNSS ID 6", unknown, 2, 2);
    check_dropped("(4) GPS given twice", twice, 2, 2);
}

static void test_accepted(void) {
    /* (5) GPS G01-G32 + Galileo E01-E32 = 64 satellites */
    static const gnss_mask_t full[2] = {{CSSR_SYS_GPS, 0xFF, 0xFFFFFF00}, {CSSR_SYS_GAL, 0xFF, 0xFFFFFF00}};
    static const gnss_mask_t g01[1] = {{CSSR_SYS_GPS, 0x80, 0}};
    clas_ctx_t* ctx = new_ctx();
    int nmsg, e32 = satno(SYS_GAL, 32);
    char msg[160];

    memset(sb, 0, sizeof(sb));
    sbn = 0;
    put_mask(full, 2);
    put_clock(64);
    nmsg = input_subframe(ctx);
    snprintf(msg, sizeof(msg), "(5) 64 satellites: mask kept, clock decoded (%d msg)", nmsg);
    CHECK(nmsg == 1 && ctx->cssr[0].iod == IOD, msg);
    CHECK(ctx->dec_ssr[e32 - 1].update_cc && ctx->dec_ssr[e32 - 1].dclk[0] == 100 * 0.0016,
          "(5) 64 satellites: clock of E32 (64th) decoded");
    CHECK(sentinel_intact(&ctx->cssr[0]), "(5) 64 satellites: net_svmask[] untouched");
    clas_ctx_free(ctx);
    free(ctx);

    /* (6) dropped mask, then a valid one in the next subframe */
    ctx = new_ctx();
    memset(sb, 0, sizeof(sb));
    sbn = 0;
    put_mask((const gnss_mask_t[]){{CSSR_SYS_GPS, 0x80, 0x80}}, 1);
    put_clock(2);
    input_subframe(ctx);
    memset(sb, 0, sizeof(sb));
    sbn = 0;
    put_mask(g01, 1);
    put_clock(1);
    nmsg = input_subframe(ctx);
    snprintf(msg, sizeof(msg), "(6) valid mask after a dropped one: G01 clock decoded (%d msg)", nmsg);
    CHECK(nmsg == 1 && ctx->dec_ssr[0].update_cc, msg);
    clas_ctx_free(ctx);
    free(ctx);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); /* keep the output of a case that crashes */
    printf("utest_clas_mask: CSSR ST1 mask bounds (#276)\n");
    test_dropped();
    test_accepted();
    printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
