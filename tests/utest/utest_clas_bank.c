/*------------------------------------------------------------------------------
 * unit test : CLAS correction-set assembly after a lost subframe 1 (#406, #242)
 *
 * CLAS sends the global code bias (ST4) only in subframe 1 of each 30 s frame;
 * a network whose ST6 is phase-only (e.g. network 7) has no other code bias.
 * When subframe 1 of frame T is lost, the bank holds cbias T-30 and, one frame
 * later, cbias T+30, while the network phase bias is T. clas_bank_get_close()
 * must pair pbias T with cbias T+30 (newer by 30 s, within IS validity) rather
 * than fail on cbias T-30 (61 s old at T+31).
 *
 *   (1) lost cbias T: the set assembles with cbias T+30 and pbias T.
 *   (2) cbias T present: cbias T is used, as before.
 *   (3) no cbias newer than T-30: no set (no substitution beyond the window).
 *
 * The bank entries are written directly; the times follow the frame layout
 * seen in live mosaic-G5 captures (orbit/clock/cbias at the frame epoch,
 * network pbias and trop at the frame epoch, observation at T+33).
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

#define NET 7 /* network with phase-only ST6 */
#define SAT 5 /* G05 */

static gtime_t T0; /* frame epoch T whose subframe 1 is lost */

static gtime_t at(double dt) { return timeadd(T0, dt); }

static void add_orbit(clas_bank_ctrl_t* b, int k, double dt) {
    clas_orbit_bank_t* o = &b->OrbitBank[k];
    o->use = 1;
    o->time = at(dt);
    o->network = 0;
    o->prn[SAT - 1] = SAT;
    o->iode[SAT - 1] = 10;
}

static void add_clock(clas_bank_ctrl_t* b, int k, double dt) {
    clas_clock_bank_t* c = &b->ClockBank[k];
    c->use = 1;
    c->time = at(dt);
    c->network = 0;
    c->prn[SAT - 1] = SAT;
}

/* bflag 0x01: code bias, 0x02: phase bias */
static void add_bias(clas_bank_ctrl_t* b, int k, double dt, int network, int bflag) {
    clas_bias_bank_t* s = &b->BiasBank[k];
    s->use = 1;
    s->time = at(dt);
    s->network = network;
    s->bflag = bflag;
    s->prn[SAT - 1] = SAT;
    s->smode[SAT - 1][0] = CODE_L1C;
    s->sflag[SAT - 1][0] = bflag;
    s->cbias[SAT - 1][0] = (bflag & 0x01) ? 0.1 : 0.0;
    s->pbias[SAT - 1][0] = (bflag & 0x02) ? 0.2 : 0.0;
}

static void add_trop(clas_bank_ctrl_t* b, int k, double dt) {
    clas_trop_bank_t* t = &b->TropBank[k];
    t->use = 1;
    t->time = at(dt);
    t->gridnum[NET - 1] = 1;
}

/* frame T lost: orbit/clock/cbias T missing; network pbias T and trop T arrived
 * in subframes 2-6; frame T+30 subframe 1 arrived */
static void fill_lost_frame(clas_bank_ctrl_t* b) {
    memset(b->OrbitBank, 0, sizeof(b->OrbitBank));
    memset(b->ClockBank, 0, sizeof(b->ClockBank));
    memset(b->BiasBank, 0, sizeof(b->BiasBank));
    memset(b->TropBank, 0, sizeof(b->TropBank));
    add_orbit(b, 0, -30.0);
    add_orbit(b, 1, 30.0);
    add_clock(b, 0, -5.0);
    add_clock(b, 1, 30.0);
    add_bias(b, 0, -30.0, 0, 0x01);   /* global ST4 cbias T-30 */
    add_bias(b, 1, -30.0, NET, 0x02); /* network ST6 pbias T-30 */
    add_bias(b, 2, 0.0, NET, 0x02);   /* network ST6 pbias T */
    add_bias(b, 3, 30.0, 0, 0x01);    /* global ST4 cbias T+30 */
    add_trop(b, 0, -30.0);
    add_trop(b, 1, 0.0);
}

int main(void) {
    clas_ctx_t* ctx = (clas_ctx_t*)calloc(1, sizeof(clas_ctx_t));
    clas_corr_t* corr = (clas_corr_t*)calloc(1, sizeof(clas_corr_t));
    clas_bank_ctrl_t* b;
    int rc;

    if (!ctx || !corr || clas_ctx_init(ctx) != 0) {
        printf("FAIL: allocation\n");
        return 1;
    }
    b = ctx->bank[0];
    T0 = gpst2time(2390, 470490.0);

    printf("--- (1) cbias T lost\n");
    fill_lost_frame(b);
    rc = clas_bank_get_close(ctx, at(33.0), NET, 0, corr);
    CHECK(rc == 0, "set assembles at T+33");
    CHECK(rc == 0 && timediff(corr->time[SAT - 1][4], at(30.0)) == 0.0, "cbias is T+30");
    CHECK(rc == 0 && timediff(corr->time[SAT - 1][5], at(0.0)) == 0.0, "pbias is T");
    CHECK(rc == 0 && timediff(corr->orbit_time, at(30.0)) == 0.0, "orbit is T+30");

    printf("--- (2) cbias T present\n");
    fill_lost_frame(b);
    add_bias(b, 4, 0.0, 0, 0x01);
    rc = clas_bank_get_close(ctx, at(33.0), NET, 0, corr);
    CHECK(rc == 0, "set assembles at T+33");
    CHECK(rc == 0 && timediff(corr->time[SAT - 1][4], at(0.0)) == 0.0, "cbias is T (unchanged pairing)");

    printf("--- (3) no cbias newer than T-30\n");
    fill_lost_frame(b);
    memset(&b->BiasBank[3], 0, sizeof(b->BiasBank[3]));
    rc = clas_bank_get_close(ctx, at(33.0), NET, 0, corr);
    CHECK(rc != 0, "no set with cbias T-30 only");

    clas_ctx_free(ctx);
    free(ctx);
    free(corr);
    printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
