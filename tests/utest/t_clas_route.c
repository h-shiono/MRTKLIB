/*------------------------------------------------------------------------------
 * unit test : clas_route_l6frame() L6D demux contract (#197)
 *
 * A multi-channel L6 receiver (u-blox D9C, 2ch; future receivers with more)
 * interleaves L6D frames from several QZS PRNs in one UBX/SBF stream. Each CLAS
 * channel decodes one PRN's subframe sequence statefully, so the demux must:
 *   - lock each channel to a single active source PRN and DROP frames from any
 *     other PRN (mixing two PRNs into one channel corrupts subframe assembly,
 *     which manifested as "L6 Lost" with corrections flowing but not decoding);
 *   - re-lock a channel to a new PRN/pattern only after the active source has
 *     been silent past CLAS_L6_RELOCK_TIMEOUT (QZS satellite handover, #197),
 *     or when the active source still sends frames but no complete subframe
 *     while another PRN delivers clean subframes (degraded source, #414);
 *   - key channels by CLAS Transmit Pattern ID in dual mode (l6mrg!=0) so the
 *     two augmentation patterns map to separate channels for merging, while
 *     single mode (l6mrg=0) collapses to ch0.
 *
 * The routing decision is a pure function of (L6 header, frame tag, l6mrg,
 * now) and the per-channel lock / per-PRN health state, so it is fully testable without a raw L6 stream.
 * Explicit checks (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>

#include "mrtklib/mrtk_clas.h"

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

/* GPST time t seconds past an arbitrary epoch */
static gtime_t at(int t) {
    gtime_t g;
    g.time = (time_t)(1000000000 + t);
    g.sec = 0.0;
    return g;
}

/* fresh context with the demux lock state cleared (banks not needed here) */
static clas_ctx_t* fresh(void) {
    clas_ctx_t* ctx = (clas_ctx_t*)calloc(1, sizeof(clas_ctx_t));
    int i;
    for (i = 0; i < CLAS_CH_NUM; i++) {
        ctx->l6delivery[i] = -1;
        ctx->l6pattern[i] = -1;
        ctx->l6lock_time[i].time = 0;
        ctx->l6lock_time[i].sec = 0.0;
    }
    for (i = 0; i < CLAS_L6_NPRN; i++) {
        ctx->l6health[i].nframe = -1;
    }
    return ctx;
}

/* route one frame; tag = 0 (no receiver time tag) disables the health check */
static int route(clas_ctx_t* ctx, int prn, int ptn, int sf_ind, double tag, int l6mrg, gtime_t now) {
    l6_header_t hdr = {0};
    hdr.prn = (uint8_t)prn;
    hdr.ptn = (uint8_t)ptn;
    hdr.sf_ind = (uint8_t)sf_ind;
    return clas_route_l6frame(ctx, &hdr, tag, l6mrg, now);
}

/* single-channel CLAS (l6mrg=0): one coherent PRN on ch0, handover re-lock */
static void test_single(void) {
    clas_ctx_t* ctx = fresh();

    CHECK(route(ctx, 194, 2, 0, 0.0, 0, at(0)) == 0, "single: first PRN locks ch0");
    CHECK(route(ctx, 194, 2, 0, 0.0, 0, at(1)) == 0, "single: active PRN stays on ch0");
    CHECK(route(ctx, 199, 1, 0, 0.0, 0, at(1)) == -1, "single: interleaved 2nd PRN dropped");
    CHECK(route(ctx, 194, 2, 0, 0.0, 0, at(5)) == 0, "single: active PRN refreshes lock");
    CHECK(route(ctx, 199, 1, 0, 0.0, 0, at(8)) == -1, "single: 2nd PRN still dropped within timeout");
    CHECK(route(ctx, 199, 1, 0, 0.0, 0, at(16)) == 0, "single: re-lock to new PRN after timeout (handover)");
    CHECK(route(ctx, 199, 1, 0, 0.0, 0, at(17)) == 0, "single: new active PRN stays on ch0");
    CHECK(route(ctx, 194, 2, 0, 0.0, 0, at(17)) == -1, "single: old PRN now dropped");

    free(ctx);
}

/* dual-channel CLAS (l6mrg!=0): one channel per transmit pattern */
static void test_dual(void) {
    clas_ctx_t* ctx = fresh();

    CHECK(route(ctx, 194, 2, 0, 0.0, 1, at(0)) == 0, "dual: pattern 2 → ch0");
    CHECK(route(ctx, 199, 1, 0, 0.0, 1, at(0)) == 1, "dual: pattern 1 → ch1");
    CHECK(route(ctx, 194, 2, 0, 0.0, 1, at(0)) == 0, "dual: pattern 2 PRN stays ch0");
    CHECK(route(ctx, 195, 1, 0, 0.0, 1, at(1)) == -1, "dual: 2nd same-pattern PRN dropped");
    CHECK(route(ctx, 195, 1, 0, 0.0, 1, at(12)) == 1, "dual: same-pattern handover re-locks ch1");
    CHECK(route(ctx, 195, 1, 0, 0.0, 1, at(13)) == 1, "dual: new active PRN stays ch1");
    CHECK(route(ctx, 199, 1, 0, 0.0, 1, at(13)) == -1, "dual: replaced PRN now dropped");
    CHECK(route(ctx, 194, 2, 0, 0.0, 1, at(13)) == 0, "dual: active PRN short-circuits regardless of pattern arg");

    free(ctx);
}

/* frame-by-frame 1 Hz feed of two PRNs for t = t0..t1-1 (frame t starts a
 * subframe when t % 5 == 0). mask_a(t) = 1 if PRN a's frame t arrives (RS ok);
 * PRN b is always clean. Returns the channel of the last frame of each PRN. */
static void feed(clas_ctx_t* ctx, int a, int b, int t0, int t1, int (*mask_a)(int), int l6mrg, int* cha, int* chb) {
    int t;
    for (t = t0; t < t1; t++) {
        if (mask_a(t)) {
            *cha = route(ctx, a, 1, t % 5 == 0, 1000.0 + t, l6mrg, at(t));
        }
        *chb = route(ctx, b, 0, t % 5 == 0, 1000.0 + t, l6mrg, at(t));
    }
}
static int all(int t) {
    (void)t;
    return 1;
}
/* every third frame lost: frames keep arriving (no silence timeout) but no
 * subframe is ever complete */
static int every3(int t) { return t % 3 != 0; }
static int none(int t) {
    (void)t;
    return 0;
}

/* degraded active source: frames keep coming but no complete subframe (#414) */
static void test_degraded(void) {
    clas_ctx_t* ctx = fresh();
    int cha = -9, chb = -9, t;

    feed(ctx, 194, 195, 0, 30, all, 0, &cha, &chb);
    CHECK(cha == 0 && chb == -1, "degraded: both clean, first PRN keeps ch0");

    /* 194 degrades from t=30: the lock holds for the timeout, then moves to 195
     * on 195's first frame of a subframe */
    for (t = 30; t < 60; t++) {
        if (every3(t)) {
            cha = route(ctx, 194, 1, t % 5 == 0, 1000.0 + t, 0, at(t));
        }
        chb = route(ctx, 195, 0, t % 5 == 0, 1000.0 + t, 0, at(t));
        if (chb == 0) {
            break;
        }
    }
    CHECK(chb == 0, "degraded: clean PRN takes over ch0");
    /* 194's last complete subframe ends at t=29; the first subframe start of
     * 195 more than CLAS_L6_DEGRADE_TIMEOUT later is t=40 */
    CHECK(t == 40, "degraded: re-lock on the first subframe start after the degrade timeout");
    CHECK(route(ctx, 194, 1, 0, 1000.0 + t + 1, 0, at(t + 1)) == -1, "degraded: old PRN now dropped");

    /* no ping-pong: 194 recovers but 195 stays clean → 195 keeps ch0 */
    feed(ctx, 194, 195, t + 1, t + 60, all, 0, &cha, &chb);
    CHECK(chb == 0 && cha == -1, "degraded: recovered old PRN does not take the lock back from a clean one");
    free(ctx);
}

/* both PRNs degraded: keep the lock (never re-lock onto another degraded PRN) */
static void test_both_degraded(void) {
    clas_ctx_t* ctx = fresh();
    int t, cha = -9, chb = -9, moved = 0;

    feed(ctx, 194, 195, 0, 10, all, 0, &cha, &chb);
    for (t = 10; t < 120; t++) {
        if (every3(t)) {
            cha = route(ctx, 194, 1, t % 5 == 0, 1000.0 + t, 0, at(t));
        }
        if ((t + 1) % 3 != 0) {
            chb = route(ctx, 195, 0, t % 5 == 0, 1000.0 + t, 0, at(t));
            moved |= chb == 0;
        }
    }
    CHECK(!moved && cha == 0, "both degraded: lock kept on the original PRN");
    free(ctx);
}

/* silence timeout unchanged; no time tag → health check off */
static void test_degraded_edges(void) {
    clas_ctx_t* ctx = fresh();
    int cha = -9, chb = -9, t, moved = 0;

    feed(ctx, 194, 195, 0, 10, all, 0, &cha, &chb);
    feed(ctx, 194, 195, 10, 40, none, 0, &cha, &chb);
    CHECK(chb == 0, "silence: handover re-lock still applies");
    free(ctx);

    ctx = fresh();
    for (t = 0; t < 120; t++) {
        if (t < 10 || every3(t)) {
            route(ctx, 194, 1, t % 5 == 0, 0.0, 0, at(t));
        }
        moved |= route(ctx, 195, 0, t % 5 == 0, 0.0, 0, at(t)) == 0;
    }
    CHECK(!moved, "no time tag: degraded source is kept (silence timeout only)");
    free(ctx);

    /* dual channel: a degraded source is replaced by a clean PRN of the same
     * pattern on its own channel; the other pattern's channel is untouched */
    ctx = fresh();
    for (t = 0; t < 10; t++) {
        route(ctx, 194, 1, t % 5 == 0, 1000.0 + t, 1, at(t));
        route(ctx, 199, 0, t % 5 == 0, 1000.0 + t, 1, at(t));
    }
    moved = -1;
    cha = 1;
    for (t = 10; t < 60 && moved < 0; t++) {
        if (every3(t)) {
            route(ctx, 194, 1, t % 5 == 0, 1000.0 + t, 1, at(t));
        }
        cha &= route(ctx, 199, 0, t % 5 == 0, 1000.0 + t, 1, at(t)) == 1;
        if (route(ctx, 195, 1, t % 5 == 0, 1000.0 + t, 1, at(t)) >= 0) {
            moved = t;
        }
    }
    CHECK(moved == 20 && ctx->l6delivery[0] == 195, "dual degraded: same-pattern clean PRN takes over its channel");
    CHECK(cha && ctx->l6delivery[1] == 199, "dual degraded: other pattern's channel unaffected");
    free(ctx);
}

int main(void) {
    test_single();
    test_dual();
    test_degraded();
    test_both_degraded();
    test_degraded_edges();

    printf("\n%s: %d check(s) failed\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
