/*------------------------------------------------------------------------------
 * t_rinex_lli.c : loss-of-lock indicator on a blank RINEX phase (#504)
 *
 * Some receivers (Trimble Alloy 6.40 on QZSS L6L) flag a slip only on an epoch
 * whose phase is blank, then write the next valid phase with LLI 0. The reader
 * must carry that slip to the next valid phase of the same satellite/signal,
 * both in readrnx()/readrnxt() (post-processing) and in convrnx() (RINEX ->
 * RINEX via input_rnxctr()).
 *
 * Synthetic RINEX 3.04, J02 C6L/L6L/S6L + L1C, G01 C1C/L1C:
 *   10:18:00  J02 L6L blank, LLI 1     G01 L1C blank, LLI 0 (control)
 *   10:18:30  J02 L6L blank
 *   10:19:00  J02 L6L valid, LLI 0  -> must read as a slip
 *   10:19:30  J02 L6L valid, LLI 0  -> no slip (carried once only)
 * J02 L1C is valid with LLI 0 throughout (no leak into another slot).
 *
 * Explicit checks, not assert(), so the test also runs under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <math.h>
#include <stdarg.h>
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
        }                                \
    } while (0)

/* convrnx() reports through this application callback (as in mrtk_main.c) */
extern int showmsg(const char* format, ...) {
    (void)format;
    return 0;
}

static const char* FILE_IN = "t_rinex_lli.obs";
static const char* FILE_OUT = "t_rinex_lli_out.obs";

static void hdr(FILE* fp, const char* body, const char* label) { fprintf(fp, "%-60s%-20s\n", body, label); }

/* one 16-column observation field: value (0: blank) + LLI (-1: blank) */
static void fld(FILE* fp, double val, int lli) {
    if (val == 0.0) {
        fprintf(fp, "              ");
    } else {
        fprintf(fp, "%14.3f", val);
    }
    if (lli < 0) {
        fprintf(fp, "  ");
    } else {
        fprintf(fp, "%d ", lli);
    }
}

static int write_input(void) {
    static const double sec[4] = {0.0, 30.0, 60.0, 90.0};
    FILE* fp = fopen(FILE_IN, "w");
    int i;

    if (!fp) return 0;
    hdr(fp, "     3.04           OBSERVATION DATA    M", "RINEX VERSION / TYPE");
    hdr(fp, "GABO", "MARKER NAME");
    hdr(fp, "  -4420000.0000  2650000.0000 -3870000.0000", "APPROX POSITION XYZ");
    hdr(fp, "G    2 C1C L1C", "SYS / # / OBS TYPES");
    hdr(fp, "J    4 C1C L1C C6L L6L", "SYS / # / OBS TYPES");
    hdr(fp, "  2026    11     8    10    18    0.0000000     GPS", "TIME OF FIRST OBS");
    hdr(fp, "", "END OF HEADER");
    for (i = 0; i < 4; i++) {
        fprintf(fp, "> 2026 11 08 10 %02d %10.7f  0  2\n", 18 + (int)(sec[i] / 60.0),
                sec[i] - 60.0 * (int)(sec[i] / 60.0));
        fprintf(fp, "G01");
        fld(fp, 21000000.0 + i, -1);
        fld(fp, i == 0 ? 0.0 : 110000000.0 + i, -1);
        fprintf(fp, "\n");
        fprintf(fp, "J02");
        fld(fp, 38000000.0 + i, -1);
        fld(fp, 200000000.0 + i, 0);
        fld(fp, 44080420.0 + i, -1);
        fld(fp, i < 2 ? 0.0 : 180727180.0 + i, i == 0 ? 1 : 0);
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 1;
}

/* LLI of the slot holding the given code, -1 if absent / no phase */
static int lli_of(const obsd_t* o, uint8_t code) {
    int j;
    for (j = 0; j < NFREQ + NEXOBS; j++) {
        if (o->code[j] == code) return o->L[j] != 0.0 ? o->LLI[j] : -2;
    }
    return -1;
}

static const obsd_t* find(const obs_t* obs, int sat, double sec) {
    double ep[6] = {2026, 11, 8, 10, 18, 0};
    gtime_t t = timeadd(epoch2time(ep), sec);
    int i;
    for (i = 0; i < obs->n; i++) {
        if (obs->data[i].sat == sat && fabs(timediff(obs->data[i].time, t)) < 1e-3) return obs->data + i;
    }
    return NULL;
}

/* checks shared by every reader path; t0 = first epoch kept (s from 10:18).
 * slip0: the path flags every phase of its first epoch (convrnx does, as the
 * start of a new station), so L1C is not checked there. */
static void check_obs(const char* tag, const obs_t* obs, double t0, int slip0) {
    int j02 = satno(SYS_QZS, 194), g01 = satno(SYS_GPS, 1);
    const obsd_t* o;
    char msg[128];
    double t;

    for (t = t0; t <= 90.0; t += 30.0) {
        if (!(o = find(obs, j02, t))) {
            snprintf(msg, sizeof(msg), "%s: J02 epoch +%.0f s present", tag, t);
            CHECK(0, msg);
            continue;
        }
        printf("%s J02 +%2.0fs L6L LLI=%2d L1C LLI=%2d\n", tag, t, lli_of(o, CODE_L6L), lli_of(o, CODE_L1C));
        snprintf(msg, sizeof(msg), "%s: J02 L1C LLI unchanged at +%.0f s", tag, t);
        CHECK(lli_of(o, CODE_L1C) == ((slip0 && t == t0) ? LLI_SLIP : 0), msg);
        if (t == 60.0) {
            snprintf(msg, sizeof(msg), "%s: blank-phase slip carried to the first valid L6L", tag);
            CHECK(lli_of(o, CODE_L6L) >= 0 && (lli_of(o, CODE_L6L) & LLI_SLIP), msg);
        }
        if (t == 90.0) {
            snprintf(msg, sizeof(msg), "%s: slip not repeated on the second valid L6L", tag);
            CHECK(lli_of(o, CODE_L6L) == 0, msg);
        }
    }
    if ((o = find(obs, g01, 30.0))) {
        snprintf(msg, sizeof(msg), "%s: G01 L1C after a blank without LLI has no slip", tag);
        CHECK(lli_of(o, CODE_L1C) == 0, msg);
    } else {
        snprintf(msg, sizeof(msg), "%s: G01 epoch +30 s present", tag);
        CHECK(0, msg);
    }
}

static void test_readrnx(void) {
    obs_t obs = {0};
    gtime_t t_none = {0};
    double ep[6] = {2026, 11, 8, 10, 18, 30};

    readrnx(FILE_IN, 1, "-JL6L", &obs, NULL, NULL);
    CHECK(obs.n == 8, "readrnx: 8 observation records");
    check_obs("readrnx", &obs, 0.0, 0);
    freeobs(&obs);

    /* blank+LLI epoch outside the time window: carried by the slip cache */
    readrnxt(FILE_IN, 1, epoch2time(ep), t_none, 0.0, "-JL6L", &obs, NULL, NULL);
    CHECK(obs.n == 6, "readrnxt: 6 observation records");
    check_obs("readrnxt", &obs, 30.0, 0);
    freeobs(&obs);
}

/* LLI characters of J02 L6L in the written file, one per epoch: the reader
 * now repairs a missing slip itself, so readback alone cannot prove that
 * convrnx wrote it. Returns the number of epochs found. */
static int written_l6l_lli(const char* file, char* lli, int nmax) {
    char buff[1024], *q;
    int k = -1, n = 0, i;
    FILE* fp = fopen(file, "r");

    if (!fp) return 0;
    while (fgets(buff, sizeof(buff), fp)) {
        if (strstr(buff, "SYS / # / OBS TYPES") && buff[0] == 'J') {
            for (i = 0, q = buff + 7; i < 13; i++, q += 4) {
                if (!strncmp(q, "L6L", 3)) k = i;
            }
        }
        if (strncmp(buff, "J02", 3) || k < 0 || n >= nmax) continue;
        lli[n++] = (int)strlen(buff) > 3 + 16 * k + 14 ? buff[3 + 16 * k + 14] : ' ';
    }
    fclose(fp);
    return n;
}

static void test_convrnx(void) {
    rnxopt_t* opt = (rnxopt_t*)calloc(1, sizeof(rnxopt_t));
    char ofile_[9][1024] = {{0}}, *ofile[9], lli[8] = {0};
    obs_t obs = {0};
    int i, j;

    if (!opt) {
        CHECK(0, "convrnx: alloc");
        return;
    }
    opt->rnxver = 304;
    opt->obstype = OBSTYPE_PR | OBSTYPE_CP | OBSTYPE_DOP | OBSTYPE_SNR;
    opt->navsys = SYS_GPS | SYS_QZS;
    opt->freqtype = FREQTYPE_L1 | FREQTYPE_L2 | FREQTYPE_L3 | FREQTYPE_L4 | FREQTYPE_L5;
    for (i = 0; i < 7; i++)
        for (j = 0; j < MAXCODE; j++) opt->mask[i][j] = '1';
    strcpy(opt->rcvopt, "-JL6L");
    for (i = 0; i < 9; i++) ofile[i] = ofile_[i];
    strcpy(ofile[0], FILE_OUT);

    CHECK(convrnx(STRFMT_RINEX, opt, FILE_IN, ofile) == 1, "convrnx: conversion ok");
    free(opt);

    /* written J02 L6L LLI: blank+1 kept, blank, carried 1, none */
    CHECK(written_l6l_lli(FILE_OUT, lli, 8) == 4, "convrnx: 4 J02 records written");
    printf("convrnx written J02 L6L LLI: '%c' '%c' '%c' '%c'\n", lli[0], lli[1], lli[2], lli[3]);
    CHECK(lli[0] == '1', "convrnx: LLI 1 kept on the blank L6L field");
    CHECK(lli[1] == ' ', "convrnx: no LLI on the blank L6L without one");
    CHECK(lli[2] == '1', "convrnx: LLI 1 written on the first valid L6L");
    CHECK(lli[3] == ' ', "convrnx: no LLI on the second valid L6L");

    readrnx(FILE_OUT, 1, "-JL6L", &obs, NULL, NULL);
    CHECK(obs.n == 8, "convrnx: 8 observation records in the output");
    check_obs("convrnx", &obs, 0.0, 1);
    freeobs(&obs);
    if (fails == 0) remove(FILE_OUT);
}

int main(void) {
    if (!write_input()) {
        printf("FAIL: setup\n");
        return 1;
    }
    test_readrnx();
    test_convrnx();
    remove(FILE_IN);
    printf("%s\n", fails ? "FAILED" : "all checks passed");
    return fails ? 1 : 0;
}
