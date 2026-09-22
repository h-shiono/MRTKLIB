/*------------------------------------------------------------------------------
 * unit test : RTCM3 MSM BeiDou B2b signal round-trip (issue #333)
 *
 * msm_sig_cmp[] in src/rtcm/mrtk_rtcm3.c is the BeiDou MSM signal-ID -> RINEX
 * code table shared by the decoder (save_msm_obs) and the encoder (to_sigid).
 * IDs 25/26/27 (B2b: 7D/7P/7Z, RTCM 10403.3 Amendment 2) were empty, so B2b
 * cells were silently dropped on both paths. This test is also the first MSM
 * encode->decode round-trip coverage in the suite (issue #296). Coverage:
 *
 *   (1) one BDS epoch with two satellites is encoded as MSM7 (type 1127) via
 *       gen_rtcm3() and fed byte-by-byte into a second rtcm_t via
 *       input_rtcm3(); exactly one obs result (ret==1) on the final byte and
 *       both satellites present.
 *   (2) control signals (BDS-3 C21: 2I, BDS-2 C06: 2I+7I) round-trip P/L/SNR
 *       within MSM7 quantization: this proves the harness independently of
 *       the table fix.
 *   (3) B2b signals (C21: 7D/7P/7Z) round-trip the same way. Decoded codes
 *       are searched in all NFREQ+NEXOBS slots of the record: sigindex()
 *       assigns primary vs extended slots by code priority, which is not
 *       part of the contract under test.
 *
 * Note: the phase comparison relies on the encoder's integer-cycle offset
 * (rtcm->cp) staying 0, which holds when LLI==0 and |L*lambda-rrng| < 1171 m;
 * all P values are within ~100 m of each other and L within a few cycles of
 * P*f/c so that condition is met.
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

#define TOL_P 2E-3  /* MSM7 fine pseudorange 2^-29 ms = 0.56 mm */
#define TOL_L 2E-3  /* MSM7 fine phase-range 2^-31 ms = 0.14 mm (compared in metres) */
#define TOL_SNR 0.1 /* MSM7 CNR resolution 1/16 dB-Hz */
#define NSLOT (NFREQ + NEXOBS)

typedef struct {
    int sat;
    uint8_t code;
    double P;   /* m */
    double L;   /* cycle */
    double snr; /* dB-Hz */
    const char* label;
} msig_t;

/* fill one encoder slot from a msig_t */
static void set_sig(obsd_t* d, int slot, const msig_t* s) {
    d->code[slot] = s->code;
    d->P[slot] = s->P;
    d->L[slot] = s->L;
    d->D[slot] = 0.0f;
    d->LLI[slot] = 0;
    d->SNR[slot] = (uint16_t)(s->snr / SNR_UNIT + 0.5);
}

static const obsd_t* find_sat(const obs_t* obs, int sat) {
    int i;
    for (i = 0; i < obs->n; i++) {
        if (obs->data[i].sat == sat) {
            return &obs->data[i];
        }
    }
    return NULL;
}

/* check one expected (sat,code) in the decoded epoch */
static void check_sig(const obs_t* obs, const msig_t* s) {
    const obsd_t* d = find_sat(obs, s->sat);
    double lambda = CLIGHT / code2freq(SYS_CMP, s->code, 0);
    char msg[128];
    int k, slot = -1;

    if (d) {
        for (k = 0; k < NSLOT; k++) {
            if (d->code[k] == s->code) {
                slot = k;
                break;
            }
        }
    }
    snprintf(msg, sizeof(msg), "%s: code present in decoded record (slot %d)", s->label, slot);
    CHECK(slot >= 0, msg);
    if (slot < 0) {
        return; /* remaining checks need the slot */
    }
    snprintf(msg, sizeof(msg), "%s: P round-trips (|dP|=%.2e m)", s->label, fabs(d->P[slot] - s->P));
    CHECK(fabs(d->P[slot] - s->P) < TOL_P, msg);
    snprintf(msg, sizeof(msg), "%s: L round-trips (|dL|=%.2e m)", s->label, fabs(d->L[slot] - s->L) * lambda);
    CHECK(fabs(d->L[slot] - s->L) * lambda < TOL_L, msg);
    snprintf(msg, sizeof(msg), "%s: SNR round-trips (|dSNR|=%.3f dB-Hz)", s->label,
             fabs(d->SNR[slot] * SNR_UNIT - s->snr));
    CHECK(fabs(d->SNR[slot] * SNR_UNIT - s->snr) < TOL_SNR, msg);
}

static void test_msm7_bds_roundtrip(void) {
    rtcm_t* enc = (rtcm_t*)calloc(1, sizeof(rtcm_t)); /* never stack-allocate rtcm_t (~7.5 MB) */
    rtcm_t* dec = (rtcm_t*)calloc(1, sizeof(rtcm_t));
    /* mid-week epoch: BDS MSM epoch is BDT (GPST-14 s), so a week-start epoch
     * would exercise the adjweek() wrap instead of the signal table */
    double ep[] = {2025, 6, 3, 12, 0, 0.0};
    gtime_t t0 = epoch2time(ep);
    double f2 = code2freq(SYS_CMP, CODE_L2I, 0), f7 = code2freq(SYS_CMP, CODE_L7I, 0);
    int c21 = satno(SYS_CMP, 21), c06 = satno(SYS_CMP, 6);
    /* P: ~2.2e7 m, a few metres apart; L: P*f/c minus a few cycles (iono-like) */
    const msig_t sigs[] = {
        {0, CODE_L2I, 22000000.000, 0.0, 40.0, "C21 2I (control)"},
        {0, CODE_L7D, 22000003.250, 0.0, 42.0, "C21 7D (B2b)"},
        {0, CODE_L7P, 22000006.500, 0.0, 44.0, "C21 7P (B2b)"},
        {0, CODE_L7Z, 22000009.750, 0.0, 46.0, "C21 7Z (B2b)"},
        {0, CODE_L2I, 23500000.125, 0.0, 38.0, "C06 2I (control)"},
        {0, CODE_L7I, 23500004.375, 0.0, 41.0, "C06 7I (control)"},
    };
    msig_t s[6];
    obsd_t* d;
    int i, ret = 0, mid_ok = 1, ok, ncomplete = 0;

    printf("--- test_msm7_bds_roundtrip (NFREQ=%d NEXOBS=%d)\n", NFREQ, NEXOBS);
    if (!enc || !dec) {
        printf("FAIL: calloc rtcm_t\n");
        fails++;
        goto cleanup;
    }
    ok = init_rtcm(enc) == 1 && init_rtcm(dec) == 1;
    CHECK(ok, "init_rtcm enc+dec");
    if (!ok) {
        goto cleanup;
    }
    CHECK(c21 > 0 && c06 > 0, "satno(SYS_CMP,21/6) valid");
    CHECK(fabs(code2freq(SYS_CMP, CODE_L7D, 0) - 1207.14E6) < 1.0, "code2freq(SYS_CMP,CODE_L7D) is B2b 1207.14 MHz");

    for (i = 0; i < 6; i++) {
        double f = (i == 0 || i == 4) ? f2 : f7;
        s[i] = sigs[i];
        s[i].sat = i < 4 ? c21 : c06;
        s[i].L = s[i].P * f / CLIGHT - 3.0 - 0.25 * i;
    }

    /* build the encoder epoch: C21 in slots 0-3, C06 in slots 0-1 */
    enc->time = t0;
    enc->staid = 1234;
    enc->obs.n = 2;
    d = &enc->obs.data[0];
    d->time = t0;
    d->sat = c21;
    for (i = 0; i < 4; i++) {
        set_sig(d, i, &s[i]);
    }
    d = &enc->obs.data[1];
    d->time = t0;
    d->sat = c06;
    set_sig(d, 0, &s[4]);
    set_sig(d, 1, &s[5]);

    CHECK(gen_rtcm3(enc, 1127, 0, 0) == 1, "gen_rtcm3 type 1127 (BDS MSM7) succeeds");
    CHECK(enc->nbyte > 0 && enc->nbyte == enc->len + 3, "framed length: nbyte == len + parity > 0");
    CHECK(enc->buff[0] == 0xD3, "frame starts with RTCM3 preamble");
    CHECK(getbitu(enc->buff, 24, 12) == 1127, "message number field is 1127");
    printf("info: encoded %d bytes\n", enc->nbyte);

    /* feed byte-by-byte into a fresh decoder */
    dec->time = t0; /* approximate time to resolve the GPS week */
    for (i = 0; i < enc->nbyte; i++) {
        ret = input_rtcm3(dec, enc->buff[i]);
        if (ret == 1) {
            ncomplete++;
        }
        if (i + 1 < enc->nbyte && ret != 0) {
            mid_ok = 0;
        }
    }
    CHECK(mid_ok, "no decode result before the final byte");
    CHECK(ret == 1, "final byte completes the obs message (ret==1)");
    CHECK(ncomplete == 1, "input_rtcm3 returned 1 exactly once");
    CHECK(dec->obs.n == 2, "decoded epoch has 2 satellites");
    CHECK(find_sat(&dec->obs, c21) != NULL, "C21 present in decoded epoch");
    CHECK(find_sat(&dec->obs, c06) != NULL, "C06 present in decoded epoch");
    if (dec->obs.n > 0) {
        CHECK(fabs(timediff(dec->obs.data[0].time, t0)) < 1E-6, "epoch time round-trips (BDT<->GPST)");
    }

    for (i = 0; i < 6; i++) {
        check_sig(&dec->obs, &s[i]);
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
    test_msm7_bds_roundtrip();

    if (fails) {
        printf("utest_msm_bds_b2b: %d check(s) FAILED\n", fails);
        return 1;
    }
    printf("utest_msm_bds_b2b: all checks passed\n");
    return 0;
}
