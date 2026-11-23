/*------------------------------------------------------------------------------
 * unit test : RTCM3 MSM decode of a signal whose band is not in obsdef (#548)
 *
 * A GPS MSM7 epoch with L1C, L2W and L5Q is encoded with the default obsdef
 * and decoded after apply_pppsig() with the default selection (GPS L1/L2),
 * which removes L5 from obsdef. L1C and L2W must stay in slots 0 and 1, and
 * L5Q must be dropped, as mrtk_sigcfg_freq_idx() drops a band that is not
 * configured (include/mrtklib/mrtk_obs.h, #189). The same message decoded
 * after the GPS L1/L5 selection drops L2W, the middle signal: L5Q must then
 * be in slot 1 with its own pseudorange, so the cells after a dropped one
 * stay aligned.
 *
 * The decoder used to index sigindex()'s local arrays with -1 for L5Q: under
 * -fsanitize=address it aborts, and a GCC RelWithDebInfo build put L5Q in
 * slot 5.
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

#define TOL_P 2E-3 /* MSM7 fine pseudorange 2^-29 ms = 0.56 mm */
#define NSLOT (NFREQ + NEXOBS)
_Static_assert(NFREQ >= 3, "utest_msm_unselected_band encodes GPS L5 in slot 2");

static const obsd_t* find_sat(const obs_t* obs, int sat) {
    int i;
    for (i = 0; i < obs->n; i++) {
        if (obs->data[i].sat == sat) {
            return &obs->data[i];
        }
    }
    return NULL;
}

static int find_code(const obsd_t* d, uint8_t code) {
    int k;
    for (k = 0; d && k < NSLOT; k++) {
        if (d->code[k] == code) {
            return k;
        }
    }
    return -1;
}

static const uint8_t codes[3] = {CODE_L1C, CODE_L2W, CODE_L5Q};
static const double P[3] = {22000000.000, 22000004.250, 22000006.500};

/* decode enc's message after the selection pppsig; want[i] is the expected
 * slot of codes[i] (-1: dropped) */
static void decode_check(const rtcm_t* enc, const int* pppsig, const int* want, const char* label) {
    rtcm_t* dec = (rtcm_t*)calloc(1, sizeof(rtcm_t)); /* never stack-allocate rtcm_t (~7.5 MB) */
    const obsd_t* d;
    char msg[128];
    int i, ret = 0, slot;

    printf("--- %s\n", label);
    if (!dec || init_rtcm(dec) != 1) {
        printf("FAIL: rtcm_t setup\n");
        fails++;
        free(dec);
        return;
    }
    apply_pppsig(pppsig);
    for (i = 0; i < 3; i++) {
        snprintf(msg, sizeof(msg), "selection: %s -> %d", code2obs(codes[i]), want[i]);
        CHECK(code2freq_idx(SYS_GPS, codes[i]) == want[i], msg);
    }
    dec->time = enc->time;
    for (i = 0; i < enc->nbyte; i++) {
        ret = input_rtcm3(dec, enc->buff[i]);
    }
    CHECK(ret == 1, "final byte completes the obs message (ret==1)");
    d = find_sat(&dec->obs, enc->obs.data[0].sat);
    CHECK(d != NULL, "G05 present in decoded epoch");
    for (i = 0; d && i < 3; i++) {
        slot = find_code(d, codes[i]);
        if (want[i] < 0) {
            snprintf(msg, sizeof(msg), "%s dropped (got slot %d)", code2obs(codes[i]), slot);
            CHECK(slot < 0, msg);
            continue;
        }
        snprintf(msg, sizeof(msg), "%s decoded in slot %d (got %d)", code2obs(codes[i]), want[i], slot);
        CHECK(slot == want[i], msg);
        if (slot == want[i]) {
            snprintf(msg, sizeof(msg), "%s P round-trips (|dP|=%.2e m)", code2obs(codes[i]), fabs(d->P[slot] - P[i]));
            CHECK(fabs(d->P[slot] - P[i]) < TOL_P, msg);
        }
    }
    reset_obsdef();
    free_rtcm(dec);
    free(dec);
}

int main(void) {
    static const int sel_l1l2[5] = {0, 0, 0, 0, 0}; /* default: GPS L1/L2 */
    static const int sel_l1l5[5] = {1, 0, 0, 0, 0}; /* GPS L1/L5 */
    static const int want_l1l2[3] = {0, 1, -1};
    static const int want_l1l5[3] = {0, -1, 1};
    double ep[] = {2026, 11, 18, 12, 0, 0.0};
    gtime_t t0 = epoch2time(ep);
    rtcm_t* enc = (rtcm_t*)calloc(1, sizeof(rtcm_t));
    obsd_t* e;
    int i;

    if (!enc || init_rtcm(enc) != 1) {
        printf("FAIL: rtcm_t setup\n");
        return 1;
    }

    /* encode with the default obsdef: L1C/L2W/L5Q in slots 0/1/2 */
    reset_obsdef();
    enc->time = t0;
    enc->staid = 1;
    enc->obs.n = 1;
    e = &enc->obs.data[0];
    e->time = t0;
    e->sat = satno(SYS_GPS, 5);
    for (i = 0; i < 3; i++) {
        e->code[i] = codes[i];
        e->P[i] = P[i];
        e->L[i] = P[i] * code2freq(SYS_GPS, codes[i], 0) / CLIGHT - 3.0;
        e->SNR[i] = (uint16_t)(40.0 / SNR_UNIT + 0.5);
    }
    CHECK(gen_rtcm3(enc, 1077, 0, 0) == 1, "gen_rtcm3 type 1077 (GPS MSM7) succeeds");

    decode_check(enc, sel_l1l2, want_l1l2, "GPS L1/L2 selection: L5Q (last signal) dropped");
    decode_check(enc, sel_l1l5, want_l1l5, "GPS L1/L5 selection: L2W (middle signal) dropped");

    free_rtcm(enc);
    free(enc);

    if (fails) {
        printf("utest_msm_unselected_band: %d check(s) FAILED\n", fails);
        return 1;
    }
    printf("utest_msm_unselected_band: all checks passed\n");
    return 0;
}
