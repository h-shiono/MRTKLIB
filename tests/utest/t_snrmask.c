/*------------------------------------------------------------------------------
 * t_snrmask.c : SNR mask with unknown C/N0 (#477)
 *
 * SNR=0 means the input carries no C/N0 (RINEX without S, RTCM MSM1-3). It
 * used to be compared with the mask as 0 dBHz, so a rover RINEX without S
 * lost every satellite and mrtk post wrote no solution. Unknown C/N0 must
 * pass the mask; known values must still be tested.
 *
 * Explicit checks, not assert(), so the test also runs under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <stdio.h>
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

int main(void) {
    snrmask_t mask;
    double el = 30.0 * D2R;
    int f, k;

    memset(&mask, 0, sizeof(mask));
    for (f = 0; f < NFREQ; f++) {
        for (k = 0; k < 9; k++) {
            mask.mask[f][k] = 35.0;
        }
    }

    /* disabled: nothing is masked */
    CHECK(!testsnr(0, 0, el, 20.0, &mask), "disabled mask must not mask");

    mask.ena[0] = 1;
    CHECK(testsnr(0, 0, el, 20.0, &mask), "20 dBHz below a 35 dBHz mask is masked");
    CHECK(!testsnr(0, 0, el, 45.0, &mask), "45 dBHz above a 35 dBHz mask is not masked");
    CHECK(!testsnr(0, 0, el, 0.0, &mask), "unknown C/N0 (0) is not masked");
    CHECK(!testsnr(0, 1, el, 0.0, &mask), "unknown C/N0 (0) on L2 is not masked");
    CHECK(!testsnr(0, 0, 3.0 * D2R, 0.0, &mask), "unknown C/N0 at low elevation is not masked");

    /* base flag is independent of the rover flag */
    CHECK(!testsnr(1, 0, el, 20.0, &mask), "base mask disabled");
    mask.ena[1] = 1;
    CHECK(testsnr(1, 0, el, 20.0, &mask), "base 20 dBHz is masked");
    CHECK(!testsnr(1, 0, el, 0.0, &mask), "base unknown C/N0 is not masked");

    if (fails) {
        printf("t_snrmask: %d failure(s)\n", fails);
        return 1;
    }
    printf("t_snrmask: OK\n");
    return 0;
}
