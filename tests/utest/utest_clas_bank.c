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
 *   (4) cbias T-30 lost too (two consecutive frames): no cbias older than
 *       pbias is left, and the set still assembles with cbias T+30.
 *   (5) cbias T+30 comes from a different mask than pbias T, so its slots
 *       hold the signals in another order: each code bias stays with its own
 *       signal (Greptile, PR #433).
 *   (6) the set carries the L6 PRN of the clock set it uses (NMEA GGA
 *       station id).
 *   (7) the phase discontinuity indicators of the network (ST6) and global
 *       (ST5) phase bias both reach the set and nav->ssr_ch (#393).
 *   (8) l6_merge BackupCSSR restore with one channel unusable (facility
 *       changed, or backup older than 180 s): clas_backup_valid() succeeds
 *       on the other channel, and the unusable channel's current set, still
 *       an earlier snapshot, is cleared instead of being applied.
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

/* set the slots of a bias entry: signal modes and their values in slot order */
static void set_slots(clas_bias_bank_t* s, int bflag, const int* mode, const double* val, int n) {
    int j;
    memset(s->smode[SAT - 1], 0, sizeof(s->smode[SAT - 1]));
    memset(s->sflag[SAT - 1], 0, sizeof(s->sflag[SAT - 1]));
    for (j = 0; j < n; j++) {
        s->smode[SAT - 1][j] = mode[j];
        s->sflag[SAT - 1][j] = bflag;
        if (bflag & 0x01) {
            s->cbias[SAT - 1][j] = val[j];
        } else {
            s->pbias[SAT - 1][j] = val[j];
        }
    }
}

/* slot of a signal in the assembled set (-1: absent) */
static int corr_slot(const clas_corr_t* corr, int mode) {
    int j;
    for (j = 0; j < MAXCODE; j++) {
        if (corr->smode[SAT - 1][j] == mode) {
            return j;
        }
    }
    return -1;
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

    printf("--- (4) cbias T-30 and T lost\n");
    fill_lost_frame(b);
    memset(&b->BiasBank[0], 0, sizeof(b->BiasBank[0]));
    rc = clas_bank_get_close(ctx, at(33.0), NET, 0, corr);
    CHECK(rc == 0, "set assembles at T+33");
    CHECK(rc == 0 && timediff(corr->time[SAT - 1][4], at(30.0)) == 0.0, "cbias is T+30");

    printf("--- (5) cbias T+30 from another mask\n");
    fill_lost_frame(b);
    memset(&b->BiasBank[0], 0, sizeof(b->BiasBank[0]));
    {
        const int pmode[] = {CODE_L1C, CODE_L2W}, cmode[] = {CODE_L2W, CODE_L5Q, CODE_L1C};
        const double pval[] = {0.2, 0.3}, cval[] = {1.2, 1.5, 1.1};
        int k1, k2, k5;
        set_slots(&b->BiasBank[2], 0x02, pmode, pval, 2);
        set_slots(&b->BiasBank[3], 0x01, cmode, cval, 3);
        rc = clas_bank_get_close(ctx, at(33.0), NET, 0, corr);
        k1 = corr_slot(corr, CODE_L1C);
        k2 = corr_slot(corr, CODE_L2W);
        k5 = corr_slot(corr, CODE_L5Q);
        CHECK(rc == 0 && k1 >= 0 && k2 >= 0 && k5 >= 0, "set holds L1C, L2W and L5Q once each");
        CHECK(k1 >= 0 && corr->cbias[SAT - 1][k1] == 1.1 && corr->pbias[SAT - 1][k1] == 0.2, "L1C code and phase bias");
        CHECK(k2 >= 0 && corr->cbias[SAT - 1][k2] == 1.2 && corr->pbias[SAT - 1][k2] == 0.3, "L2W code and phase bias");
        CHECK(k5 >= 0 && corr->cbias[SAT - 1][k5] == 1.5 && corr->pbias[SAT - 1][k5] == 0.0, "L5Q code bias only");
        CHECK(k1 == 0 && k2 == 1 && k5 == 2, "slots in phase-bias order, then code-bias-only signals");
    }

    printf("--- (6) L6 PRN of the clock set\n");
    fill_lost_frame(b);
    b->ClockBank[0].l6prn = 194;
    b->ClockBank[1].l6prn = 196;
    rc = clas_bank_get_close(ctx, at(33.0), NET, 0, corr);
    CHECK(rc == 0 && corr->l6prn == 196, "l6prn from the T+30 clock set");

    printf("--- (7) phase discontinuity indicators\n");
    fill_lost_frame(b);
    b->BiasBank[2].discnt[SAT - 1][0] = (1 | CLAS_DISC_VALID) << CLAS_DISC_NET;
    add_bias(b, 4, 0.0, 0, 0x02); /* global ST5 pbias T */
    b->BiasBank[4].discnt[SAT - 1][0] = (2 | CLAS_DISC_VALID) << CLAS_DISC_GLB;
    rc = clas_bank_get_close(ctx, at(33.0), NET, 0, corr);
    {
        const int disc = ((1 | CLAS_DISC_VALID) << CLAS_DISC_NET) | ((2 | CLAS_DISC_VALID) << CLAS_DISC_GLB);
        int k1 = corr_slot(corr, CODE_L1C);
        nav_t* nav = (nav_t*)calloc(1, sizeof(nav_t));
        CHECK(rc == 0 && k1 >= 0 && corr->discnt[SAT - 1][k1] == disc, "set carries network and global indicator");
        CHECK(rc == 0 && k1 >= 0 && corr->pbias[SAT - 1][k1] == 0.4, "phase bias is network + global");
        if (nav) {
            clas_update_global(nav, corr, 0);
            CHECK(nav->ssr_ch[0][SAT - 1].discnt[CODE_L1C - 1] == disc, "nav->ssr_ch carries the indicators");
            free(nav);
        }
    }

    printf("--- (8) BackupCSSR restore, one merge channel unusable\n");
    {
        static const char* why[] = {"both usable", "ch1 facility changed", "ch1 backup expired"};
        clas_grid_t* grid = (clas_grid_t*)calloc(1, sizeof(clas_grid_t));
        int ch, k;
        for (k = 0; grid && k < 3; k++) {
            char msg[128];
            for (ch = 0; ch < 2; ch++) {
                clas_corr_t* x = &ctx->current[ch];
                clas_bank_init(ctx, ch);
                ctx->bank[ch]->Facility = 0;
                memset(x, 0, sizeof(*x));
                x->use = 1;
                x->facility = 0;
                x->network = NET;
                x->clock_time = x->orbit_time = x->bias_time = at(k == 2 ? -200.0 : 0.0);
                if (ch == 0) {
                    x->clock_time = x->orbit_time = x->bias_time = at(0.0);
                }
            }
            clas_backup_current(ctx, grid, 1);
            if (k == 1) {
                ctx->bank[1]->Facility = 1;
            }
            CHECK(clas_backup_valid(ctx, at(1.0), 1), "backup valid on ch0");
            clas_restore_backup(ctx, at(1.0), grid, 1);
            snprintf(msg, sizeof(msg), "%s: ch0 restored", why[k]);
            CHECK(ctx->current[0].use && ctx->current[0].facility == 0, msg);
            snprintf(msg, sizeof(msg), "%s: ch1 %s", why[k], k ? "cleared" : "restored");
            CHECK(k ? !ctx->current[1].use : ctx->current[1].use, msg);
        }
        free(grid);
    }

    clas_ctx_free(ctx);
    free(ctx);
    free(corr);
    printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
