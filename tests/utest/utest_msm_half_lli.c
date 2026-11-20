/* RTCM3 MSM half-cycle ambiguity indicator to LLI (#531). No captured receiver
 * data used.
 *
 * One GPS satellite with two signals is encoded as MSM4 to MSM7 epoch by epoch
 * with gen_rtcm3() and decoded with input_rtcm3(). The encoder sets the
 * indicator from LLI_HALFC and restarts the lock time on LLI_SLIP, so the
 * decoded LLI has to be the input LLI. The first epoch is the exception: lock
 * time 0 with no history is a loss of lock.
 *
 * The decoder used to add 3 for the indicator: LLI_HALFC alone came back as 3
 * (a slip on every epoch) and LLI_SLIP | LLI_HALFC as 4 (both flags lost).
 *
 * Only the LLI is compared: on a slip the encoder re-anchors the phase range,
 * which is not under test here. */
#include <stdio.h>
#include <stdlib.h>

#include "mrtklib/rtklib.h"

static int failed = 0;

#define NEPOCH 9

/* {input, expected} per epoch */
static const uint8_t l1_seq[NEPOCH][2] = {{2, 3}, {0, 0}, {2, 2}, {2, 2}, {3, 3}, {2, 2}, {0, 0}, {1, 1}, {0, 0}};
/* second signal with the flags at other epochs, so a cell mix-up fails */
static const uint8_t l2_seq[NEPOCH][2] = {{0, 1}, {2, 2}, {0, 0}, {3, 3}, {0, 0}, {0, 0}, {1, 1}, {2, 2}, {2, 2}};

static void run(int type) {
    rtcm_t* enc = calloc(1, sizeof(rtcm_t)); /* rtcm_t is ~7.5 MB: heap only */
    rtcm_t* dec = calloc(1, sizeof(rtcm_t));
    double ep[] = {2025, 6, 3, 12, 0, 0.0};
    gtime_t t0 = epoch2time(ep);
    int sat = satno(SYS_GPS, 5), i, k, n, ret, slot;
    const uint8_t codes[2] = {CODE_L1C, CODE_L2W};
    const uint8_t (*seq[2])[2] = {l1_seq, l2_seq};

    if (!enc || !dec || !init_rtcm(enc) || !init_rtcm(dec)) {
        printf("FAIL %d: init\n", type);
        failed++;
        goto cleanup;
    }
    enc->staid = 1;
    dec->time = t0; /* approximate time to resolve the week */

    for (n = 0; n < NEPOCH; n++) {
        gtime_t t = timeadd(t0, (double)n);
        obsd_t* d = &enc->obs.data[0];

        enc->time = t;
        enc->obs.n = 1;
        d->time = t;
        d->sat = (uint8_t)sat;
        for (k = 0; k < 2; k++) {
            double freq = code2freq(SYS_GPS, codes[k], 0);
            d->code[k] = codes[k];
            d->P[k] = 22000000.0 + 3.0 * k;
            d->L[k] = d->P[k] * freq / CLIGHT - 2.0;
            d->SNR[k] = (uint16_t)(42.0 / SNR_UNIT + 0.5);
            d->LLI[k] = seq[k][n][0];
        }
        if (!gen_rtcm3(enc, type, 0, 0)) {
            printf("FAIL %d: epoch %d gen_rtcm3\n", type, n);
            failed++;
            goto cleanup;
        }
        for (i = 0, ret = 0; i < enc->nbyte; i++) {
            ret = input_rtcm3(dec, enc->buff[i]);
        }
        if (ret != 1 || dec->obs.n != 1 || dec->obs.data[0].sat != sat) {
            printf("FAIL %d: epoch %d decode ret=%d n=%d\n", type, n, ret, dec->obs.n);
            failed++;
            goto cleanup;
        }
        for (k = 0; k < 2; k++) {
            for (slot = 0; slot < NFREQ + NEXOBS; slot++) {
                if (dec->obs.data[0].code[slot] == codes[k]) {
                    break;
                }
            }
            if (slot >= NFREQ + NEXOBS) {
                printf("FAIL %d: epoch %d signal %d missing\n", type, n, k);
                failed++;
            } else if (dec->obs.data[0].LLI[slot] != seq[k][n][1]) {
                printf("FAIL %d: epoch %d signal %d LLI in=%d out=%d expected=%d\n", type, n, k, seq[k][n][0],
                       dec->obs.data[0].LLI[slot], seq[k][n][1]);
                failed++;
            }
        }
    }
cleanup:
    if (enc) {
        free_rtcm(enc);
    }
    if (dec) {
        free_rtcm(dec);
    }
    free(enc);
    free(dec);
}

int main(void) {
    int type;

    for (type = 1074; type <= 1077; type++) {
        run(type);
    }
    if (failed) {
        printf("%d check(s) failed\n", failed);
        return 1;
    }
    printf("utest_msm_half_lli: all checks passed\n");
    return 0;
}
