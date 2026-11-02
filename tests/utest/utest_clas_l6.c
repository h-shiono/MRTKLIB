/*------------------------------------------------------------------------------
 * unit test : CLAS L6 subframe assembly and CSSR epoch reconstruction
 *             (#363, #392)
 *
 * Frames are built from hand-written CSSR messages (ST1 mask: GPS G01, one
 * signal; ST3 clock) and input through clas_input_cssr()/clas_decode_msg().
 *
 * #363  A subframe is 5 frames of 1695 bits, assembled by counting frames after
 *       the subframe indicator. A lost frame shifts the bits of the following
 *       frames. With per-frame receiver time tags the rest of the subframe must
 *       be discarded:
 *         (1) consecutive tags: every message decodes
 *         (2) frame 2 tagged 2 s after frame 1: messages from frame 2 on dropped
 *         (3) no tags (raw L6 byte stream): unchecked, every message decodes
 *         (4) tag going backwards: dropped
 *         (9) a mask that fits in frame 1 decodes before frame 2 arrives, so a
 *             frame lost after it does not cost the mask (upstream read ngnss
 *             from the satellite mask and waited for more bits)
 *        (10) the same with cell masks (upstream read the satellite and signal
 *             masks again from the cell-mask bits)
 *
 * #392  ST2-ST12 carry only the hourly epoch; the full TOW is tow0 + hourly and
 *       tow0 advances only on ST1. When the ST1 at the top of the hour is lost:
 *         (5) a clock with hourly epoch 5 after mask 7170 is dated 7205, not 3605
 *         (6) the next ST1 keeps tow0 consistent (7230)
 *         (7) at the end of the week, epoch 5 is dated week+1 TOW 5
 *         (8) epochs within the hour are unchanged
 *
 * NMEA GGA station id: each clock bank set records the L6 PRN that delivered
 * it, and keeps it when a later set arrives from another PRN (QZSS handover):
 *        (11) clock 7170 from PRN 194, clock 7205 from PRN 195
 *
 * #436  The GPS week of ST1-ST12 is tracked per subtype from the TOW sequence.
 *       With two channels (l6_merge), ch1 can resume after a long silence
 *       across a week rollover with tow0 still from the previous week, so its
 *       first clock before an ST1 is dated near the end of that week. The week
 *       reference is per channel, as upstream keeps it per L6 stream:
 *        (12) ch0's next clock after that stays in week+1 (a shared reference
 *             was moved to week+2: clocks a week ahead, never found again)
 *        (13) ch1 reaches week+1 with its own next ST1
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

/* subframe bit buffer and write cursor */
static uint8_t sb[(SFBITS + 7) / 8];
static int sbn;
static uint8_t frame_prn = 193; /* L6 PRN written into the frame headers */

static void sb_reset(void) {
    memset(sb, 0, sizeof(sb));
    sbn = 0;
}

static void put(int nbit, uint32_t v) {
    setbitu(sb, sbn, nbit, v);
    sbn += nbit;
}

static void put_mask(int tow) {
    put(12, 4073);
    put(4, 1);
    put(20, (uint32_t)tow);
    put(4, 0);    /* update interval */
    put(1, 1);    /* multiple message */
    put(4, IOD);  /* IOD SSR */
    put(4, 1);    /* ngnss */
    put(4, 0);    /* GPS */
    put(8, 0x80); /* svmask: G01 */
    put(32, 0);
    put(16, 0x8000); /* sigmask: one signal */
    put(1, 0);       /* no cell mask */
}

/* GPS 20, Galileo 20, QZSS 7 satellites x 16 signals, every cell set: 984 bits */
static void put_dense_mask(int tow) {
    static const struct {
        int id, nsat;
        uint32_t hi, lo;
    } g[3] = {{0, 20, 0xFF, 0xFFF00000}, {2, 20, 0xFF, 0xFFF00000}, {4, 7, 0xFE, 0}};
    int k, j;
    put(12, 4073);
    put(4, 1);
    put(20, (uint32_t)tow);
    put(4, 0);
    put(1, 1);
    put(4, IOD);
    put(4, 3);
    for (k = 0; k < 3; k++) {
        put(4, (uint32_t)g[k].id);
        put(8, g[k].hi);
        put(32, g[k].lo);
        put(16, 0xFFFF);
        put(1, 1); /* cell mask follows */
        for (j = 0; j < g[k].nsat; j++) {
            put(16, 0xFFFF);
        }
    }
}

static void put_clock(int hourly) {
    put(12, 4073);
    put(4, 3);
    put(12, (uint32_t)hourly);
    put(4, 0);
    put(1, 0); /* last message: decode returns 10 */
    put(4, IOD);
    put(15, 100); /* G01 clock */
}

/* QZS PRN 195 L6D frame, GPST week 2441 TOW 505141 */
static const uint8_t live_sf1_head[250] = {
    0x1A, 0xCF, 0xFC, 0x1D, 0xC3, 0xA1, 0x7F, 0x48, 0xBD, 0xA9, 0xA2, 0x88, 0xC0, 0x3A, 0x88, 0x60, 0x30, 0x02,
    0x02, 0x93, 0xDF, 0xDF, 0xFD, 0xFF, 0xDE, 0x40, 0x61, 0x40, 0x09, 0x00, 0x04, 0x80, 0x04, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x82, 0x40, 0x7F, 0x49, 0xA3, 0xA1, 0x0B, 0xFF, 0x07, 0xD6, 0x2F, 0xFD, 0x1F, 0x98, 0x81, 0x1A,
    0x00, 0x1B, 0x03, 0x70, 0x0B, 0xDB, 0xFA, 0x57, 0xCC, 0xBF, 0xE0, 0x40, 0x63, 0xFF, 0x9C, 0x80, 0xFC, 0x01,
    0x30, 0x02, 0x3B, 0xF9, 0x88, 0x02, 0xCF, 0xE9, 0x24, 0x74, 0x51, 0x1C, 0x7E, 0x5E, 0x02, 0xC0, 0x04, 0x1F,
    0x7E, 0x10, 0x00, 0x14, 0x0D, 0x17, 0x5F, 0x77, 0x82, 0xAC, 0x04, 0xE2, 0xBF, 0x86, 0xC0, 0x50, 0xFC, 0xC3,
    0x07, 0xB7, 0x50, 0x20, 0x7F, 0x99, 0x6B, 0xF5, 0x80, 0x31, 0x3F, 0xDA, 0xBF, 0xEE, 0xD0, 0x0F, 0xC0, 0x21,
    0x0F, 0xF7, 0xD4, 0x01, 0xA0, 0x2B, 0xA8, 0x7E, 0x69, 0x03, 0xA0, 0x09, 0x18, 0x3E, 0x1B, 0x02, 0x23, 0xF8,
    0x62, 0x77, 0xE6, 0x6F, 0xE7, 0x80, 0xDC, 0x4E, 0xFB, 0xC3, 0xFD, 0x90, 0x0B, 0x89, 0x7F, 0x69, 0xC0, 0x48,
    0x01, 0x71, 0x3B, 0xEF, 0xF8, 0x03, 0x40, 0x20, 0x25, 0xFE, 0x56, 0x00, 0x2F, 0xFD, 0xC4, 0xEF, 0xCD, 0xBF,
    0xEC, 0x00, 0xFB, 0x8F, 0xDB, 0xFF, 0xF9, 0x7F, 0x95, 0xC7, 0xEC, 0xE7, 0xFD, 0xBF, 0xA3, 0xFD, 0x28, 0x8E,
    0x8A, 0x20, 0x01, 0xFC, 0xBF, 0xD0, 0x00, 0x05, 0x40, 0x87, 0xE3, 0x00, 0x0F, 0xB5, 0xFA, 0x80, 0x07, 0xF3,
    0x00, 0x9E, 0xB5, 0xCA, 0x73, 0xDD, 0x61, 0x58, 0x30, 0xBB, 0x7B, 0xBF, 0xAB, 0x4B, 0x8A, 0x29, 0xD2, 0x0B,
    0x70, 0xD9, 0x82, 0xE6, 0x8E, 0x08, 0xC9, 0xE8, 0x40, 0xDE, 0x47, 0xFF, 0xF0, 0xBD, 0x7F, 0x3F,
};
/* QZS PRN 195 L6D frame, GPST week 2441 TOW 505144 */
static const uint8_t live_sf1_after_gap[250] = {
    0x1A, 0xCF, 0xFC, 0x1D, 0xC3, 0xA0, 0x03, 0x82, 0x20, 0x1C, 0x74, 0x40, 0x34, 0x44, 0x44, 0x3F, 0xE1, 0x40,
    0x83, 0x00, 0x5F, 0xF3, 0x38, 0x00, 0x44, 0xC8, 0x47, 0x42, 0x2F, 0xFF, 0x6F, 0xDD, 0x01, 0x90, 0x28, 0x37,
    0xB4, 0x1E, 0x07, 0x1E, 0x1E, 0x24, 0x18, 0x40, 0x08, 0xC1, 0xF1, 0xA3, 0x24, 0x30, 0xDC, 0x8A, 0x01, 0x67,
    0xFA, 0x80, 0x0C, 0x0A, 0x3B, 0xFD, 0xC3, 0xC3, 0x80, 0x7E, 0x2C, 0x00, 0x8F, 0xD6, 0x01, 0x30, 0x2C, 0x47,
    0xD0, 0x0D, 0x06, 0x0D, 0x0D, 0x12, 0xE8, 0x50, 0x0F, 0x00, 0x1B, 0xFE, 0xC7, 0x0D, 0xAD, 0x3D, 0x5D, 0x29,
    0x84, 0x06, 0x44, 0x10, 0x11, 0x32, 0x43, 0x0D, 0x94, 0xD0, 0x48, 0xBF, 0xC4, 0x01, 0x40, 0x01, 0x00, 0x00,
    0x00, 0x21, 0x48, 0x01, 0x10, 0x26, 0x11, 0xE1, 0xBE, 0xAC, 0x54, 0x50, 0x4E, 0xFF, 0xC4, 0x00, 0x41, 0x00,
    0xDF, 0x0F, 0x00, 0x00, 0x4D, 0xFF, 0xEB, 0xFB, 0x40, 0x28, 0x31, 0x1E, 0xEE, 0xEF, 0x13, 0x04, 0x90, 0x18,
    0xBF, 0xDC, 0x00, 0x05, 0x11, 0xCF, 0xF0, 0x12, 0x42, 0x8D, 0x04, 0xFB, 0xF6, 0x3F, 0xDC, 0x21, 0x2C, 0xEC,
    0xE0, 0x15, 0x19, 0x50, 0x3E, 0xBF, 0x4B, 0xFE, 0x4F, 0x3F, 0x4F, 0x0E, 0xF0, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x2C, 0x62, 0xF4, 0x4D, 0xEB, 0xF2, 0x9B, 0xFD, 0x2A, 0x74, 0x57, 0x83, 0x09, 0xAC, 0x01, 0xA6,
    0x46, 0x09, 0x32, 0x8F, 0x25, 0xEF, 0x00, 0xFA, 0x55, 0xAD, 0xFB, 0xFA, 0xBC, 0x45, 0x6C, 0xC0,
};

/* input a captured frame, return number of messages decoded */
static int input_live(clas_ctx_t* ctx, const uint8_t* fr, double tag) {
    int i, nmsg = 0;
    clas_set_l6tag(ctx, 0, tag);
    for (i = 0; i < 250; i++) {
        clas_input_cssr(ctx, fr[i], 0);
        while (clas_decode_msg(ctx, 0) == 10) {
            nmsg++;
        }
    }
    return nmsg;
}

/* input frame n of the subframe in sb on channel ch, return number of messages decoded */
static int input_frame_ch(clas_ctx_t* ctx, int ch, int n, double tag) {
    uint8_t fr[250] = {0x1A, 0xCF, 0xFC, 0x1D, 0};
    int i, nmsg = 0;

    fr[4] = frame_prn;
    fr[5] = n == 0 ? 0xA1 : 0xA0; /* CLAS, subframe indicator on frame 1 */
    fr[6] = (uint8_t)getbitu(sb, 1695 * n, 7);
    for (i = 0; i < 211; i++) {
        fr[7 + i] = (uint8_t)getbitu(sb, 1695 * n + 7 + 8 * i, 8);
    }
    if (tag > 0.0) {
        clas_set_l6tag(ctx, ch, tag);
    }
    for (i = 0; i < 250; i++) {
        clas_input_cssr(ctx, fr[i], ch);
        while (clas_decode_msg(ctx, ch) == 10) {
            nmsg++;
        }
    }
    return nmsg;
}

static int input_frame(clas_ctx_t* ctx, int n, double tag) { return input_frame_ch(ctx, 0, n, tag); }

static int input_subframe_ch(clas_ctx_t* ctx, int ch, const double* tags) {
    int n, nmsg = 0;
    for (n = 0; n < 5; n++) {
        nmsg += input_frame_ch(ctx, ch, n, tags ? tags[n] : 0.0);
    }
    return nmsg;
}

static int input_subframe(clas_ctx_t* ctx, const double* tags) { return input_subframe_ch(ctx, 0, tags); }

static clas_ctx_t* new_ctx(void) {
    clas_ctx_t* ctx = (clas_ctx_t*)calloc(1, sizeof(clas_ctx_t));
    if (!ctx || clas_ctx_init(ctx) != 0) {
        printf("FAIL: clas_ctx_init\n");
        exit(1);
    }
    clas_set_week_ref(ctx, WEEK);
    return ctx;
}

static void free_ctx(clas_ctx_t* ctx) {
    clas_ctx_free(ctx);
    free(ctx);
}

/* mask + 32 clocks; clock 31 straddles frames 1-2 (110 + 52 * 30 = 1670) */
static void build_continuity_subframe(void) {
    int k;
    sb_reset();
    put_mask(7170);
    for (k = 0; k < 32; k++) {
        put_clock(3570);
    }
}

static void test_continuity(void) {
    const double ok[5] = {100.0, 101.0, 102.0, 103.0, 104.0};
    const double gap[5] = {100.0, 102.0, 103.0, 104.0, 105.0};
    const double back[5] = {100.0, 99.0, 100.0, 101.0, 102.0};
    clas_ctx_t* ctx;
    int n;

    build_continuity_subframe();

    ctx = new_ctx();
    n = input_subframe(ctx, ok);
    printf("      consecutive tags: %d clocks\n", n);
    CHECK(n == 32, "(1) consecutive frames: all 32 clocks decoded");
    free_ctx(ctx);

    ctx = new_ctx();
    n = input_subframe(ctx, gap);
    printf("      gap before frame 2: %d clocks\n", n);
    CHECK(n == 30, "(2) frame gap: only the 30 clocks of frame 1 decoded");
    free_ctx(ctx);

    ctx = new_ctx();
    n = input_subframe(ctx, NULL);
    CHECK(n == 32, "(3) no time tags: unchecked, all 32 clocks decoded");
    free_ctx(ctx);

    ctx = new_ctx();
    n = input_subframe(ctx, back);
    CHECK(n == 30, "(4) tag going backwards: subframe discarded from frame 2");
    free_ctx(ctx);

    /* the next subframe decodes again after a discarded one */
    ctx = new_ctx();
    input_subframe(ctx, gap);
    {
        const double next[5] = {106.0, 107.0, 108.0, 109.0, 110.0};
        n = input_subframe(ctx, next);
    }
    CHECK(n == 32, "(2b) next subframe after a discarded one decodes in full");
    free_ctx(ctx);

    /* live capture (mosaic-G5, 2026-10-23): subframe 1 of frame 505140, the
     * next two frames lost. The mask (272 bits), ST3 and ST2 fit in frame 1. */
    ctx = new_ctx();
    {
        int week;
        clas_set_week_ref(ctx, 2441);
        input_live(ctx, live_sf1_head, 505141.0);
        CHECK(time2gpst(ctx->l6buf[0].time, &week) == 505140.0 && ctx->l6buf[0].nbit > 272,
              "(9) live frame: mask decoded from frame 1 alone");
        input_live(ctx, live_sf1_after_gap, 505144.0);
        CHECK(ctx->cssr[0].iod == 2 && ctx->l6buf[0].nbit == -1, "(9) live frame: mask kept, rest discarded after gap");
    }
    free_ctx(ctx);

    /* a mask with cell masks, the rest of frame 1 all ones: the satellite and
     * signal masks must be read inside each GNSS block, not from the cell-mask
     * bits after it, or the length check runs past frame 1 */
    ctx = new_ctx();
    {
        int week;
        sb_reset();
        put_dense_mask(7170);
        while (sbn < 1695) {
            put(1, 1);
        }
        input_frame(ctx, 0, 100.0);
        CHECK(time2gpst(ctx->l6buf[0].time, &week) == 7170.0 && ctx->cssr[0].iod == IOD,
              "(10) mask with cell masks decoded from frame 1 alone");
    }
    free_ctx(ctx);
}

static int clock_tow(clas_ctx_t* ctx, int* week) { return (int)time2gpst(ctx->l6buf[0].time, week); }

static void test_hour_wrap(void) {
    clas_ctx_t* ctx = new_ctx();
    int tow, week;
    char msg[128];

    /* frame 7170: mask + clock in the same subframe */
    sb_reset();
    put_mask(7170);
    put_clock(3570);
    input_subframe(ctx, NULL);
    tow = clock_tow(ctx, &week);
    sprintf(msg, "(8) clock within the hour dated 7170 (got %d)", tow);
    CHECK(tow == 7170, msg);

    /* frame 7200: its subframe 1 (mask) lost, clock 7205 in a later subframe */
    sb_reset();
    put_clock(5);
    input_subframe(ctx, NULL);
    tow = clock_tow(ctx, &week);
    sprintf(msg, "(5) hour-top ST1 lost: clock dated 7205 (got %d)", tow);
    CHECK(tow == 7205 && week == WEEK, msg);

    /* frame 7230: mask received again */
    sb_reset();
    put_mask(7230);
    put_clock(30);
    input_subframe(ctx, NULL);
    tow = clock_tow(ctx, &week);
    sprintf(msg, "(6) next ST1: clock dated 7230 (got %d)", tow);
    CHECK(tow == 7230, msg);
    free_ctx(ctx);

    /* end of week: mask 604770, then the week-top mask lost */
    ctx = new_ctx();
    sb_reset();
    put_mask(604770);
    put_clock(3570);
    input_subframe(ctx, NULL);
    sb_reset();
    put_clock(5);
    input_subframe(ctx, NULL);
    tow = clock_tow(ctx, &week);
    sprintf(msg, "(7) week-top ST1 lost: clock dated week+1 TOW 5 (got week %d TOW %d)", week, tow);
    CHECK(tow == 5 && week == WEEK + 1, msg);
    free_ctx(ctx);
}

/* L6 PRN of the clock bank set dated tow, -1 if there is none */
static int clock_set_prn(const clas_ctx_t* ctx, int tow) {
    int i;
    for (i = 0; i < CLAS_BANK_NUM; i++) {
        const clas_clock_bank_t* c = &ctx->bank[0]->ClockBank[i];
        if (c->use && (int)time2gpst(c->time, NULL) == tow) {
            return c->l6prn;
        }
    }
    return -1;
}

static void test_l6prn(void) {
    clas_ctx_t* ctx = new_ctx();
    char msg[128];
    int p7170, p7205;

    frame_prn = 194;
    sb_reset();
    put_mask(7170);
    put_clock(3570);
    input_subframe(ctx, NULL);

    frame_prn = 195;
    sb_reset();
    put_mask(7200);
    put_clock(5);
    input_subframe(ctx, NULL);
    frame_prn = 193;

    p7170 = clock_set_prn(ctx, 7170);
    p7205 = clock_set_prn(ctx, 7205);
    sprintf(msg, "(11) clock sets keep their L6 PRN: 7170 from 194, 7205 from 195 (got %d, %d)", p7170, p7205);
    CHECK(p7170 == 194 && p7205 == 195, msg);
    free_ctx(ctx);
}

static void test_week_ref_channels(void) {
    clas_ctx_t* ctx = new_ctx();
    double tow;
    int week;
    char msg[160];

    /* ch1: last subframe of the week before it goes silent (tow0 597600) */
    sb_reset();
    put_mask(601050);
    put_clock(3455);
    input_subframe_ch(ctx, 1, NULL);

    /* ch0: end of the week, then the week-top subframe */
    sb_reset();
    put_mask(604770);
    put_clock(3575);
    input_subframe_ch(ctx, 0, NULL);
    sb_reset();
    put_mask(30);
    put_clock(35);
    input_subframe_ch(ctx, 0, NULL);

    /* ch1 resumes in week+1 with a clock before any ST1: 597600 + 2755 */
    sb_reset();
    put_clock(2755);
    input_subframe_ch(ctx, 1, NULL);

    /* ch0's next clock */
    sb_reset();
    put_clock(65);
    input_subframe_ch(ctx, 0, NULL);
    tow = time2gpst(ctx->l6buf[0].time, &week);
    sprintf(msg, "(12) ch1 resuming with a stale tow0: ch0 clock stays week+1 TOW 65 (got week %+d TOW %.0f)",
            week - WEEK, tow);
    CHECK(week == WEEK + 1 && tow == 65.0, msg);

    /* ch1's next ST1 */
    sb_reset();
    put_mask(90);
    put_clock(95);
    input_subframe_ch(ctx, 1, NULL);
    tow = time2gpst(ctx->l6buf[1].time, &week);
    sprintf(msg, "(13) ch1 next ST1: clock dated week+1 TOW 95 (got week %+d TOW %.0f)", week - WEEK, tow);
    CHECK(week == WEEK + 1 && tow == 95.0, msg);
    free_ctx(ctx);
}

int main(void) {
    printf("utest_clas_l6: L6 subframe continuity (#363), hourly epoch wrap (#392), per-channel week (#436)\n");
    test_continuity();
    test_hour_wrap();
    test_l6prn();
    test_week_ref_channels();
    printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
