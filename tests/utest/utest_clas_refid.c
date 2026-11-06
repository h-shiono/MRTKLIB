/*------------------------------------------------------------------------------
 * unit test : CLAS L6 source as the NMEA GGA station id
 *
 * clas_nmea_refid() encodes the corrections in use as P1*100 + P2*10 + F:
 * P1/P2 = L6 PRN - 192 of channel 1/2 (0: none), F = facility + 1, 9 when the
 * channels carry different facilities, 0 overall without CLAS corrections.
 *
 *   (1) encoding: single channel, two-channel merge, facility mismatch, no
 *       corrections, PRN outside 193-199, merge off with ch2 populated.
 *   (2) GGA field 14 carries sol->refid; outsols() writes it with
 *       out-clasrefid on and 0000 with it off.
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

static void set_ch(clas_ctx_t* ctx, int ch, int use, int l6prn, int facility) {
    ctx->current[ch].use = use;
    ctx->current[ch].l6prn = l6prn;
    ctx->current[ch].facility = facility;
}

/* field 14 (station id) of the first GGA sentence in buff */
static int gga_refid(const char* buff, char* id) {
    const char* p = strstr(buff, "GGA,");
    int i;

    if (!p) {
        return 0;
    }
    for (i = 0; i < 14 && (p = strchr(p, ',')); i++) {
        p++;
    }
    if (!p || sscanf(p, "%4[0-9]", id) != 1) {
        return 0;
    }
    return 1;
}

int main(void) {
    clas_ctx_t* ctx = (clas_ctx_t*)calloc(1, sizeof(clas_ctx_t));
    sol_t sol = {{0}};
    solopt_t opt = solopt_default;
    char buff[1024], id[8];
    double pos[3] = {35.0 * D2R, 139.0 * D2R, 50.0};

    if (!ctx) {
        printf("FAIL: allocation\n");
        return 1;
    }

    printf("--- (1) encoding\n");
    set_ch(ctx, 0, 1, 194, 0);
    CHECK(clas_nmea_refid(ctx, 1) == 201, "1ch PRN 194 facility 0 -> 0201");
    set_ch(ctx, 0, 1, 196, 1);
    set_ch(ctx, 1, 1, 197, 1);
    CHECK(clas_nmea_refid(ctx, 2) == 452, "2ch PRN 196/197 facility 1 -> 0452");
    CHECK(clas_nmea_refid(ctx, 1) == 402, "merge off ignores ch2 -> 0402");
    set_ch(ctx, 1, 1, 197, 3);
    CHECK(clas_nmea_refid(ctx, 2) == 459, "2ch facility mismatch -> 0459");
    set_ch(ctx, 1, 0, 197, 1);
    CHECK(clas_nmea_refid(ctx, 2) == 402, "ch2 without corrections -> 0402");
    set_ch(ctx, 0, 0, 196, 1);
    set_ch(ctx, 1, 1, 199, 2);
    CHECK(clas_nmea_refid(ctx, 2) == 73, "only ch2 PRN 199 facility 2 -> 0073");
    set_ch(ctx, 0, 0, 0, -1);
    set_ch(ctx, 1, 0, 0, -1);
    CHECK(clas_nmea_refid(ctx, 2) == 0, "no corrections -> 0000");
    set_ch(ctx, 0, 1, 0, 1);
    CHECK(clas_nmea_refid(ctx, 1) == 0, "unknown PRN -> 0000");

    printf("--- (2) GGA field 14\n");
    sol.time = gpst2time(2390, 470490.0);
    sol.stat = SOLQ_FIX;
    sol.ns = 12;
    sol.refid = 452;
    pos2ecef(pos, sol.rr);
    outnmea_gga((uint8_t*)buff, &sol);
    CHECK(gga_refid(buff, id) && !strcmp(id, "0452"), "outnmea_gga writes 0452");
    opt.posf = SOLF_NMEA;
    CHECK(opt.clasrefid == 1, "out-clasrefid on by default");
    buff[outsols((uint8_t*)buff, &sol, NULL, &opt)] = '\0';
    CHECK(gga_refid(buff, id) && !strcmp(id, "0452"), "outsols on: 0452");
    opt.clasrefid = 0;
    buff[outsols((uint8_t*)buff, &sol, NULL, &opt)] = '\0';
    CHECK(gga_refid(buff, id) && !strcmp(id, "0000"), "outsols off: 0000");

    free(ctx);
    printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
