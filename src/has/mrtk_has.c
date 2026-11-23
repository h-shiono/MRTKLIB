/*------------------------------------------------------------------------------
 * mrtk_has.c : Galileo HAS (High Accuracy Service) processing functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Reference:
 *   [1] Galileo High Accuracy Service Signal-in-Space Interface Control
 *       Document (HAS SIS ICD), Issue 1.0, May 2022, European Union.
 *----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_has.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mrtk_has_rs.h"
#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_trace.h"

/* forward declarations (resolved at link time) ------------------------------*/
extern int satno(int sys, int prn);
extern void satno2id(int sat, char* id);
extern double time2gpst(gtime_t t, int* week);
extern char* time_str(gtime_t t, int n);

/* constants -----------------------------------------------------------------*/

#define HAS_DUMMY_HEADER 0xAF3BC3 /* 24-bit HAS header of a dummy page */
#define HAS_TIMEOUT 150.0         /* message collection timeout (s) [1] 6.4.1 */

#define HAS_MT1 1 /* HAS Message Type 1 (satellite corrections) */

#define HAS_GNSS_GPS 0 /* HAS GNSS ID: GPS ([1] Table 18) */
#define HAS_GNSS_GAL 2 /* HAS GNSS ID: Galileo ([1] Table 18) */

#define HAS_HASS_DONTUSE 3 /* HAS Status: don't use ([1] Table 9) */

#define HAS_SATM_BITS 40 /* satellite mask length ([1] 5.2.1.2) */
#define HAS_SIGM_BITS 16 /* signal mask length ([1] 5.2.1.3) */

#define HAS_DR_NA (-4096)  /* DR "data not available" (1000...0, 13-bit) */
#define HAS_DIT_NA (-2048) /* DIT "data not available" (1000...0, 12-bit) */
#define HAS_DCT_NA (-2048) /* DCT "data not available" (1000...0, 12-bit) */
#define HAS_DCC_NA (-4096) /* DCC "data not available" (1000...0, 13-bit) */
#define HAS_DCC_NU (4095)  /* DCC "shall not be used" (0111...1, 13-bit) */
#define HAS_CB_NA (-1024)  /* CB "data not available" (1000...0, 11-bit) */
#define HAS_PB_NA (-1024)  /* PB "data not available" (1000...0, 11-bit) */

/* Galileo signal index -> obs code ([1] Table 20) ---------------------------*/
static const uint8_t has_sig_gal[HAS_MAX_SIG] = {CODE_L1B, CODE_L1C, CODE_L1X, CODE_L5I, CODE_L5Q, CODE_L5X,
                                                 CODE_L7I, CODE_L7Q, CODE_L7X, CODE_L8I, CODE_L8Q, CODE_L8X,
                                                 CODE_L6B, CODE_L6C, CODE_L6X, CODE_NONE};

/* GPS signal index -> obs code ([1] Table 20) -------------------------------*/
static const uint8_t has_sig_gps[HAS_MAX_SIG] = {CODE_L1C, CODE_NONE, CODE_NONE, CODE_L1S, CODE_L1L, CODE_L1X,
                                                 CODE_L2S, CODE_L2L,  CODE_L2X,  CODE_L2W, CODE_L5I, CODE_L5Q,
                                                 CODE_L5X, CODE_NONE, CODE_NONE, CODE_NONE};

/* map HAS GNSS ID to satellite system ---------------------------------------*/
static int has_gnssid2sys(int gnssid) {
    switch (gnssid) {
        case HAS_GNSS_GPS:
            return SYS_GPS;
        case HAS_GNSS_GAL:
            return SYS_GAL;
    }
    return SYS_NONE;
}

/* map HAS GNSS ID + signal index to obs code --------------------------------*/
static uint8_t has_sigcode(int gnssid, int sigidx) {
    if (sigidx < 0 || sigidx >= HAS_MAX_SIG) {
        return CODE_NONE;
    }
    switch (gnssid) {
        case HAS_GNSS_GPS:
            return has_sig_gps[sigidx];
        case HAS_GNSS_GAL:
            return has_sig_gal[sigidx];
    }
    return CODE_NONE;
}

/* IODref field width (bits) for a HAS GNSS ID ([1] Table 26) -----------------*/
static int has_iodref_bits(int gnssid) {
    switch (gnssid) {
        case HAS_GNSS_GPS:
            return 8;
        case HAS_GNSS_GAL:
            return 10;
    }
    return 0; /* unknown */
}

/* carrier wavelength for a Galileo/GPS obs code (m) --------------------------*/
static double has_code2lam(int sys, uint8_t code) {
    double freq = 0.0;

    if (sys == SYS_GAL) {
        switch (code) {
            case CODE_L1B:
            case CODE_L1C:
            case CODE_L1X:
                freq = FREQ1; /* E1 */
                break;
            case CODE_L5I:
            case CODE_L5Q:
            case CODE_L5X:
                freq = FREQ5; /* E5a */
                break;
            case CODE_L7I:
            case CODE_L7Q:
            case CODE_L7X:
                freq = FREQ7; /* E5b */
                break;
            case CODE_L8I:
            case CODE_L8Q:
            case CODE_L8X:
                freq = FREQ8; /* E5 (E5a+E5b) */
                break;
            case CODE_L6B:
            case CODE_L6C:
            case CODE_L6X:
                freq = FREQ6; /* E6 */
                break;
        }
    } else if (sys == SYS_GPS) {
        switch (code) {
            case CODE_L1C:
            case CODE_L1S:
            case CODE_L1L:
            case CODE_L1X:
                freq = FREQ1; /* L1 */
                break;
            case CODE_L2S:
            case CODE_L2L:
            case CODE_L2X:
            case CODE_L2W:
                freq = FREQ2; /* L2 */
                break;
            case CODE_L5I:
            case CODE_L5Q:
            case CODE_L5X:
                freq = FREQ5; /* L5 */
                break;
        }
    }
    return (freq > 0.0) ? CLIGHT / freq : 0.0;
}

/* bounds check: are bits [i, i+n) within the recovered message [0, nbit)? ----
 * Every block parser verifies this BEFORE each getbitu/getbits so a structured-
 * garbage message (e.g. RS decode over pages mixed from two content generations
 * of a reused MID within the 150 s window) cannot drive the cursor past the
 * calloc(ms*53) buffer and over-read the heap. */
static int has_bits_ok(int i, int n, int nbit) { return i >= 0 && n >= 0 && i + n <= nbit; }

/* maximum tolerated MT1 latency (rec_sow - toh, mod 3600); genuine messages
 * show 1-21 s on real data, so 600 s rejects MID-reuse structured garbage while
 * leaving a wide margin for the real signal ([1] 6.4.1 collection window). */
#define HAS_MAX_LATENCY 600.0

/* count set bits in a HAS satellite mask span -------------------------------*/
static int popcount_field(const uint8_t* buff, int pos, int len) {
    int i, cnt = 0;

    for (i = 0; i < len; i++) {
        if (getbitu(buff, pos + i, 1)) {
            cnt++;
        }
    }
    return cnt;
}

/* time of application of the HAS corrections ([1] 7.7, Eq. 28/29) -----------*/
static gtime_t has_t_mt1(gtime_t time, int toh) {
    double sow_r, hr3600;
    int week;

    sow_r = time2gpst(time, &week); /* GPST ~= GST; week-relative SoW */
    hr3600 = floor(sow_r / 3600.0) * 3600.0;
    if (hr3600 + (double)toh > sow_r) {
        hr3600 -= 3600.0; /* hour transition between generation and reception */
    }
    return gpst2time(week, hr3600 + (double)toh);
}

/* decode MT1 mask block ([1] 5.2.1) -----------------------------------------
 * fills has->mask[mid]; returns updated bit position, -1 on unknown GNSS,
 * slot overflow, duplicate GNSS, or a read past the recovered message */
static int decode_has_mask(has_t* has, const uint8_t* msg, int i, int nbit, int mid) {
    has_mask_t* m = &has->mask[mid];
    int j, k, s, nsys, gnssid, cmaf, nm, nsat, nsig, sys, prn;
    int seen_gnss[16] = {0}; /* GNSS ID is 4 bits; one mask per GNSS ([1] 5.2.1) */
    char satid[8];

    memset(m, 0, sizeof(has_mask_t));

    if (!has_bits_ok(i, 4, nbit)) {
        return -1;
    }
    nsys = (int)getbitu(msg, i, 4);
    i += 4;
    m->nsys = nsys;
    s = 0; /* running mask-slot index across all GNSS */

    for (j = 0; j < nsys; j++) {
        if (!has_bits_ok(i, 4, nbit)) {
            return -1;
        }
        gnssid = (int)getbitu(msg, i, 4);
        i += 4;
        sys = has_gnssid2sys(gnssid);
        if (sys == SYS_NONE) {
            /* IODref width for an unknown GNSS is undefined ([1] Table 26):
             * orbit/clock/bias block lengths cannot be computed -> abort. */
            trace(NULL, 2, "decode_has_mask: unknown gnssid=%d, abort\n", gnssid);
            return -1;
        }
        if (seen_gnss[gnssid]) {
            /* the ICD defines exactly one mask per GNSS ([1] 5.2.1); a repeat is
             * structured garbage (e.g. MID-reuse RS mixing) -> abort. */
            trace(NULL, 2, "decode_has_mask: duplicate gnssid=%d, abort\n", gnssid);
            return -1;
        }
        seen_gnss[gnssid] = 1;

        if (!has_bits_ok(i, HAS_SATM_BITS, nbit)) {
            return -1;
        }
        nsat = popcount_field(msg, i, HAS_SATM_BITS);
        if (s + nsat > MAXSAT) {
            /* accumulated mask slots exceed the m->sat[]/m->gnssid[]/... arrays
             * (sized MAXSAT); abort rather than write out of bounds. */
            trace(NULL, 2, "decode_has_mask: slot overflow s=%d+nsat=%d > %d, abort\n", s, nsat, MAXSAT);
            return -1;
        }
        for (k = 0; k < HAS_SATM_BITS; k++) {
            if (getbitu(msg, i + k, 1)) {
                prn = k + 1; /* satellite index 0 <-> PRN 1 ([1] Table 19) */
                m->sat[s] = satno(sys, prn);
                m->gnssid[s] = gnssid;
                s++;
            }
        }
        i += HAS_SATM_BITS;

        if (!has_bits_ok(i, HAS_SIGM_BITS + 1, nbit)) {
            return -1;
        }
        nsig = popcount_field(msg, i, HAS_SIGM_BITS);
        cmaf = (int)getbitu(msg, i + HAS_SIGM_BITS, 1);

        /* signal codes for this GNSS, in mask order */
        uint8_t codes[HAS_MAX_SIG];
        int nc = 0;
        for (k = 0; k < HAS_SIGM_BITS; k++) {
            if (getbitu(msg, i + k, 1)) {
                codes[nc++] = has_sigcode(gnssid, k);
            }
        }
        i += HAS_SIGM_BITS;
        i += 1; /* CMAF */

        for (k = s - nsat; k < s; k++) {
            int c;
            uint16_t cell;
            m->nsig[k] = nsig;
            for (c = 0; c < nsig; c++) {
                m->code[k][c] = codes[c];
            }
            if (cmaf) {
                if (!has_bits_ok(i, nsig, nbit)) {
                    return -1;
                }
                cell = (uint16_t)getbitu(msg, i, nsig);
                i += nsig;
            } else {
                cell = (uint16_t)((nsig >= 16) ? 0xFFFF : ((1u << nsig) - 1u));
            }
            m->cellmask[k] = cell;
        }

        if (!has_bits_ok(i, 3, nbit)) {
            return -1;
        }
        nm = (int)getbitu(msg, i, 3);
        i += 3;
        for (k = s - nsat; k < s; k++) {
            m->navmsg[k] = nm;
        }

        trace(NULL, 4, "decode_has_mask: gnssid=%d, nsat=%d, nsig=%d, cmaf=%d, nm=%d\n", gnssid, nsat, nsig, cmaf, nm);
    }
    if (!has_bits_ok(i, 6, nbit)) {
        return -1;
    }
    i += 6; /* Reserved */

    m->nsat = s;
    m->valid = 1;

    for (k = 0; k < s; k++) {
        if (m->sat[k]) {
            satno2id(m->sat[k], satid);
            trace(NULL, 4, "decode_has_mask: slot=%d, sat=%s, nsig=%d, cell=0x%04X\n", k, satid, m->nsig[k],
                  m->cellmask[k]);
        }
    }
    return i;
}

/* decode MT1 orbit block ([1] 5.2.2); fills deph + iode + iodset cache ------*/
static int decode_has_orbit(has_t* has, const uint8_t* msg, int i, int nbit, int mid, int iodset, gtime_t t0) {
    const has_mask_t* m = &has->mask[mid];
    has_iodset_t* io = &has->iodset[mid][iodset];
    int j, sat, nb, iodref, dr, dit, dct;

    if (!has_bits_ok(i, 4, nbit)) {
        return -1;
    }
    i += 4; /* VI (validity interval index; unused: t0 carries applicability) */

    memset(io, 0, sizeof(has_iodset_t));

    for (j = 0; j < m->nsat; j++) {
        nb = has_iodref_bits(m->gnssid[j]);
        if (!has_bits_ok(i, nb + 13 + 12 + 12, nbit)) {
            return -1;
        }
        iodref = (int)getbitu(msg, i, nb);
        i += nb;
        dr = getbits(msg, i, 13);
        i += 13;
        dit = getbits(msg, i, 12);
        i += 12;
        dct = getbits(msg, i, 12);
        i += 12;

        io->iodref[j] = iodref;

        sat = m->sat[j];
        if (sat == 0) {
            continue; /* unsupported satellite (still parsed structurally) */
        }
        if (dr == HAS_DR_NA || dit == HAS_DIT_NA || dct == HAS_DCT_NA) {
            continue; /* data not available */
        }

        /* sign convention: satpos_ssr applies rs += -R*deph, HAS x~=x+R*dR */
        has->ssr[sat - 1].deph[0] = -dr * 0.0025;
        has->ssr[sat - 1].deph[1] = -dit * 0.0080;
        has->ssr[sat - 1].deph[2] = -dct * 0.0080;
        has->ssr[sat - 1].ddeph[0] = 0.0;
        has->ssr[sat - 1].ddeph[1] = 0.0;
        has->ssr[sat - 1].ddeph[2] = 0.0;
        has->ssr[sat - 1].iode = iodref;
        /* Orbit block owns iod[0]/t0[0] only; the clock block owns iod[1]/t0[1].
         * Keeping them separate makes satpos_ssr's iod[0]==iod[1] gate reject a
         * stale clock from a previous IOD set against the new IODref ephemeris
         * at an IOD-set transition (clock-only messages lag the mask+orbit by up
         * to ~10 s in real broadcast). */
        has->ssr[sat - 1].iod[0] = iodset;
        has->ssr[sat - 1].t0[0] = t0;
        has->ssr[sat - 1].udi[0] = 0.0;
        has->ssr[sat - 1].update = 1;
    }
    io->valid = 1;
    return i;
}

/* decode MT1 clock full-set block ([1] 5.2.3) -------------------------------*/
static int decode_has_clock(has_t* has, const uint8_t* msg, int i, int nbit, int mid, int iodset, gtime_t t0) {
    const has_mask_t* m = &has->mask[mid];
    int j, sat, dcm[HAS_MAX_SIG], mult, dcc, prevgid = -1, sysidx = -1;

    if (!has_bits_ok(i, 4, nbit)) {
        return -1;
    }
    i += 4; /* VI */

    /* DCM(2 bits) per GNSS, in mask order. m->nsys <= 15 (4-bit Nsys) and
     * dcm[] is sized HAS_MAX_SIG (16), so the index is bounded. */
    for (j = 0; j < m->nsys; j++) {
        if (!has_bits_ok(i, 2, nbit)) {
            return -1;
        }
        dcm[j] = (int)getbitu(msg, i, 2);
        i += 2;
    }

    for (j = 0; j < m->nsat; j++) {
        if (m->gnssid[j] != prevgid) {
            prevgid = m->gnssid[j];
            sysidx++;
        }
        mult = dcm[sysidx] + 1; /* DCM value n -> multiplier n+1 ([1] Table 29) */

        if (!has_bits_ok(i, 13, nbit)) {
            return -1;
        }
        dcc = getbits(msg, i, 13);
        i += 13;

        sat = m->sat[j];
        if (sat == 0) {
            continue;
        }
        if (dcc == HAS_DCC_NA) {
            continue; /* data not available */
        }
        if (dcc == HAS_DCC_NU) {
            /* "shall not be used" -> clear clock so satpos_ssr rejects sat */
            has->ssr[sat - 1].t0[1].time = 0;
            has->ssr[sat - 1].t0[1].sec = 0.0;
            continue;
        }

        has->ssr[sat - 1].dclk[0] = dcc * 0.0025 * mult;
        has->ssr[sat - 1].dclk[1] = 0.0;
        has->ssr[sat - 1].dclk[2] = 0.0;
        has->ssr[sat - 1].hrclk = 0.0;
        has->ssr[sat - 1].iod[1] = iodset;
        has->ssr[sat - 1].t0[1] = t0;
        has->ssr[sat - 1].udi[1] = 0.0;
        has->ssr[sat - 1].update = 1;
    }
    return i;
}

/* decode MT1 clock subset block ([1] 5.2.4) ---------------------------------*/
static int decode_has_clock_subset(has_t* has, const uint8_t* msg, int i, int nbit, int mid, int iodset, gtime_t t0) {
    const has_mask_t* m = &has->mask[mid];
    int g, j, gnssid, dcm, mult, dcc, sat, nsat_gnss, base, submask_pos, nsys_sub;

    if (!has_bits_ok(i, 4 + 4, nbit)) {
        return -1;
    }
    i += 4; /* VI */
    nsys_sub = (int)getbitu(msg, i, 4);
    i += 4;

    for (g = 0; g < nsys_sub; g++) {
        if (!has_bits_ok(i, 4 + 2, nbit)) {
            return -1;
        }
        gnssid = (int)getbitu(msg, i, 4);
        i += 4;
        dcm = (int)getbitu(msg, i, 2);
        i += 2;
        mult = dcm + 1;

        /* locate the contiguous mask-slot range for this GNSS in SatM order */
        base = -1;
        nsat_gnss = 0;
        for (j = 0; j < m->nsat; j++) {
            if (m->gnssid[j] == gnssid) {
                if (base < 0) {
                    base = j;
                }
                nsat_gnss++;
            }
        }
        if (base < 0) {
            trace(NULL, 2, "decode_has_clock_subset: gnssid=%d not in mask\n", gnssid);
            return -1;
        }

        /* subset mask (Nsat bits relative to the GNSS satellites of SatM),
         * then one DCC(13) per selected satellite */
        if (!has_bits_ok(i, nsat_gnss, nbit)) {
            return -1;
        }
        submask_pos = i;
        i += nsat_gnss;
        for (j = 0; j < nsat_gnss; j++) {
            if (!getbitu(msg, submask_pos + j, 1)) {
                continue;
            }
            if (!has_bits_ok(i, 13, nbit)) {
                return -1;
            }
            dcc = getbits(msg, i, 13);
            i += 13;

            sat = m->sat[base + j];
            if (sat == 0) {
                continue;
            }
            if (dcc == HAS_DCC_NA) {
                continue;
            }
            if (dcc == HAS_DCC_NU) {
                has->ssr[sat - 1].t0[1].time = 0;
                has->ssr[sat - 1].t0[1].sec = 0.0;
                continue;
            }
            has->ssr[sat - 1].dclk[0] = dcc * 0.0025 * mult;
            has->ssr[sat - 1].dclk[1] = 0.0;
            has->ssr[sat - 1].dclk[2] = 0.0;
            has->ssr[sat - 1].hrclk = 0.0;
            has->ssr[sat - 1].iod[1] = iodset;
            has->ssr[sat - 1].t0[1] = t0;
            has->ssr[sat - 1].udi[1] = 0.0;
            has->ssr[sat - 1].update = 1;
        }
    }
    return i;
}

/* decode MT1 code bias block ([1] 5.2.5) ------------------------------------*/
static int decode_has_codebias(has_t* has, const uint8_t* msg, int i, int nbit, gtime_t t0, const has_mask_t* m) {
    int j, c, sat, cb;
    uint8_t code;

    if (!has_bits_ok(i, 4, nbit)) {
        return -1;
    }
    i += 4; /* VI */

    for (j = 0; j < m->nsat; j++) {
        sat = m->sat[j];
        if (sat) {
            has->ssr[sat - 1].t0[4] = t0;
            has->ssr[sat - 1].udi[4] = 0.0;
            /* the block carries every code bias of the satellite under the current
             * mask: a signal that left the mask keeps no bias, else has_sel_bias()
             * would prefer its stale slot over the fresh one of the same signal */
            memset(has->ssr[sat - 1].cbias, 0, sizeof(has->ssr[sat - 1].cbias));
            memset(has->ssr[sat - 1].vcbias, 0, sizeof(has->ssr[sat - 1].vcbias));
            has->ssr[sat - 1].update = 1;
        }
        for (c = 0; c < m->nsig[j]; c++) {
            if (!((m->cellmask[j] >> (m->nsig[j] - 1 - c)) & 0x1)) {
                continue; /* cell not provided */
            }
            if (!has_bits_ok(i, 11, nbit)) {
                return -1;
            }
            cb = getbits(msg, i, 11);
            i += 11;

            if (sat == 0) {
                continue;
            }
            code = m->code[j][c];
            if (code == CODE_NONE) {
                continue;
            }
            if (cb == HAS_CB_NA) {
                /* "data not available" -> clear the bias so corr_meas treats it as
                 * absent (the !vcbias path drops the pseudorange, which is correct:
                 * a HAS code bias replaces BGD and is required). Clearing both the
                 * value and the valid flag also prevents a previously valid bias
                 * from persisting in nav->ssr_ch via the update-gated copy loop. */
                has->ssr[sat - 1].cbias[code - 1] = 0.0f;
                has->ssr[sat - 1].vcbias[code - 1] = 0;
                has->ssr[sat - 1].update = 1;
                continue;
            }
            has->ssr[sat - 1].cbias[code - 1] = (float)(cb * 0.02); /* added to P (Eq. 25) */
            has->ssr[sat - 1].vcbias[code - 1] = 1;
            has->ssr[sat - 1].update = 1;
        }
    }
    return i;
}

/* decode MT1 phase bias block ([1] 5.2.6) -----------------------------------*/
static int decode_has_phasebias(has_t* has, const uint8_t* msg, int i, int nbit, gtime_t t0, const has_mask_t* m) {
    int j, c, sat, pb, pdi, sys;
    uint8_t code;
    double lam;

    if (!has_bits_ok(i, 4, nbit)) {
        return -1;
    }
    i += 4; /* VI */

    for (j = 0; j < m->nsat; j++) {
        sat = m->sat[j];
        sys = sat ? has_gnssid2sys(m->gnssid[j]) : SYS_NONE;
        if (sat) {
            has->ssr[sat - 1].t0[5] = t0;
            has->ssr[sat - 1].udi[5] = 0.0;
            /* as in decode_has_codebias(); discnt is kept */
            memset(has->ssr[sat - 1].pbias, 0, sizeof(has->ssr[sat - 1].pbias));
            memset(has->ssr[sat - 1].vpbias, 0, sizeof(has->ssr[sat - 1].vpbias));
            has->ssr[sat - 1].update = 1;
        }
        for (c = 0; c < m->nsig[j]; c++) {
            if (!((m->cellmask[j] >> (m->nsig[j] - 1 - c)) & 0x1)) {
                continue;
            }
            if (!has_bits_ok(i, 11 + 2, nbit)) {
                return -1;
            }
            pb = getbits(msg, i, 11);
            i += 11;
            pdi = (int)getbitu(msg, i, 2);
            i += 2;

            if (sat == 0) {
                continue;
            }
            code = m->code[j][c];
            if (code == CODE_NONE) {
                continue;
            }
            if (pb == HAS_PB_NA) {
                /* "data not available" -> treat as "no phase bias provided" rather
                 * than an invalid sentinel. With pbias cleared and vpbias=0,
                 * corr_meas keeps the carrier for CORR_GAL_HAS (float PPP absorbs
                 * the unknown satellite phase bias into the float ambiguity);
                 * writing SSR_INVALID_PBIAS would instead force L[i]=0 and drop the
                 * carrier. Clearing also evicts any stale valid bias from
                 * nav->ssr_ch through the update-gated copy. discnt is left as-is
                 * (no new discontinuity is signalled by an NA). */
                has->ssr[sat - 1].pbias[code - 1] = 0.0;
                has->ssr[sat - 1].vpbias[code - 1] = 0;
                has->ssr[sat - 1].update = 1;
                continue;
            }
            lam = has_code2lam(sys, code);
            has->ssr[sat - 1].pbias[code - 1] = pb * 0.01 * lam; /* cycles -> m (Eq. 26) */
            has->ssr[sat - 1].vpbias[code - 1] = 1;
            has->ssr[sat - 1].discnt[code - 1] = pdi;
            has->ssr[sat - 1].update = 1;
        }
    }
    return i;
}

/* decode a recovered MT1 message ([1] section 5) ----------------------------*/
static int decode_has_mt1(has_t* has, const uint8_t* msg, int nbit, gtime_t time) {
    int i = 0, toh, fmask, forbit, fcfull, fcsub, fcb, fpb, mid, iodset, week;
    double sow_r, latency;
    gtime_t t0;

    if (nbit < 32) {
        return -1;
    }

    toh = (int)getbitu(msg, i, 12);
    i += 12;
    fmask = (int)getbitu(msg, i, 1);
    i += 1;
    forbit = (int)getbitu(msg, i, 1);
    i += 1;
    fcfull = (int)getbitu(msg, i, 1);
    i += 1;
    fcsub = (int)getbitu(msg, i, 1);
    i += 1;
    fcb = (int)getbitu(msg, i, 1);
    i += 1;
    fpb = (int)getbitu(msg, i, 1);
    i += 1;
    i += 4; /* Reserved */
    mid = (int)getbitu(msg, i, 5);
    i += 5;
    iodset = (int)getbitu(msg, i, 5);
    i += 5;

    /* plausibility gate: TOH is 0..3599 ([1] 5.2, Table 8). A value >=3600 means
     * the recovered header is garbage (e.g. RS decode over pages mixed from two
     * content generations of a reused MID within the 150 s window) -> reject. */
    if (toh >= 3600) {
        trace(NULL, 2, "decode_has_mt1: invalid toh=%d (>=3600), reject\n", toh);
        return -1;
    }
    /* implied latency = (rec_sow - toh) mod 3600; genuine messages show 1-21 s
     * on real data, so >HAS_MAX_LATENCY (600 s) kills most MID-reuse garbage. */
    sow_r = time2gpst(time, &week);
    latency = fmod(sow_r - (double)toh, 3600.0);
    if (latency < 0.0) {
        latency += 3600.0;
    }
    if (latency > HAS_MAX_LATENCY) {
        trace(NULL, 2, "decode_has_mt1: latency=%.0f s > %.0f, reject (toh=%d)\n", latency, HAS_MAX_LATENCY, toh);
        return -1;
    }

    t0 = has_t_mt1(time, toh);

    trace(NULL, 4, "decode_has_mt1: %s, toh=%d, flags=%d%d%d%d%d%d, mid=%d, iodset=%d\n", time_str(t0, 3), toh, fmask,
          forbit, fcfull, fcsub, fcb, fpb, mid, iodset);

    if (fmask) {
        if ((i = decode_has_mask(has, msg, i, nbit, mid)) < 0) {
            return -1;
        }
    }
    if (!has->mask[mid].valid) {
        trace(NULL, 2, "decode_has_mt1: %s, mask id=%d not yet received\n", time_str(t0, 3), mid);
        return -1; /* cannot interpret body without a cached mask */
    }

    if (forbit) {
        if ((i = decode_has_orbit(has, msg, i, nbit, mid, iodset, t0)) < 0) {
            return -1;
        }
    } else if (!has->iodset[mid][iodset].valid) {
        trace(NULL, 2, "decode_has_mt1: %s, iodset (mid=%d,id=%d) not yet received\n", time_str(t0, 3), mid, iodset);
        /* IODref cache absent; corrections still applied with cached mask, but
         * iode reference is unknown -> warn (satpos_ssr will gate on iode). */
    }
    if (fcfull) {
        if ((i = decode_has_clock(has, msg, i, nbit, mid, iodset, t0)) < 0) {
            return -1;
        }
    }
    if (fcsub) {
        if ((i = decode_has_clock_subset(has, msg, i, nbit, mid, iodset, t0)) < 0) {
            return -1;
        }
    }
    if (fcb) {
        if ((i = decode_has_codebias(has, msg, i, nbit, t0, &has->mask[mid])) < 0) {
            return -1;
        }
    }
    if (fpb) {
        if ((i = decode_has_phasebias(has, msg, i, nbit, t0, &has->mask[mid])) < 0) {
            return -1;
        }
    }
    return 10;
}

/* run RS erasure decoder on a completed collector and decode MT1 ------------*/
static int has_complete(has_t* has, has_collect_t* col, gtime_t time) {
    uint8_t* out;
    int ret;

    out = (uint8_t*)calloc((size_t)col->ms * HAS_PAGE_BYTES, 1);
    if (!out) {
        return -1;
    }
    if (has_rs_decode(col->page, col->pid, col->ms, out) != 0) {
        trace(NULL, 2, "has_complete: RS decode failed mid=%d, k=%d\n", col->mid, col->ms);
        free(out);
        return -1;
    }

    ret = 0;
    if (col->mt == HAS_MT1) {
        ret = decode_has_mt1(has, out, col->ms * HAS_PAGE_BYTES * 8, time);
    }
    free(out);
    col->active = 0;
    return ret;
}

/* find or allocate an in-flight collector for (mid, mt) ---------------------*/
static has_collect_t* has_find_collect(has_t* has, int mid, int mt, gtime_t time) {
    int k, oldest = 0;
    double age, maxage = -1.0;

    for (k = 0; k < HAS_MAX_INFLIGHT; k++) {
        if (has->collect[k].active && has->collect[k].mid == mid && has->collect[k].mt == mt) {
            if (timediff(time, has->collect[k].t0) > HAS_TIMEOUT) {
                has->collect[k].active = 0; /* expired; restart below */
                break;
            }
            return &has->collect[k];
        }
    }
    /* reuse an idle slot, else the oldest active one */
    for (k = 0; k < HAS_MAX_INFLIGHT; k++) {
        if (!has->collect[k].active) {
            return &has->collect[k];
        }
        age = timediff(time, has->collect[k].t0);
        if (age > maxage) {
            maxage = age;
            oldest = k;
        }
    }
    return &has->collect[oldest];
}

/* clear all collected state ([1] HASS==3 handling) --------------------------*/
static void has_flush(has_t* has) {
    int k;

    for (k = 0; k < HAS_MAX_INFLIGHT; k++) {
        has->collect[k].active = 0;
    }
    for (k = 0; k < HAS_MAX_MID; k++) {
        has->mask[k].valid = 0;
    }
    trace(NULL, 2, "has_flush: HAS state discarded\n");
}

/* ---------------------------------------------------------------------------*/
extern has_t* has_new(void) {
    has_t* has;

    if (has_rs_init() != 0) {
        return NULL;
    }
    has = (has_t*)calloc(1, sizeof(has_t));
    return has;
}

/* ---------------------------------------------------------------------------*/
extern void has_free(has_t* has) { free(has); }

/* ---------------------------------------------------------------------------*/
extern int has_input_page(has_t* has, int prn, const uint8_t* page56, gtime_t time) {
    has_collect_t* col;
    uint32_t header;
    int hass, mt, mid, ms, pid, k;

    (void)prn; /* PRN is a routing hint only; collection keys on (MID, MT) */

    header = getbitu(page56, 0, 24);
    if (header == HAS_DUMMY_HEADER) {
        return 0; /* dummy page (defensive; normally filtered upstream) */
    }

    hass = (int)getbitu(page56, 0, 2);
    /* getbitu(page56,2,2) = Reserved */
    mt = (int)getbitu(page56, 4, 2);
    mid = (int)getbitu(page56, 6, 5);
    ms = (int)getbitu(page56, 11, 5) + 1; /* MS stored minus 1 */
    pid = (int)getbitu(page56, 16, 8);

    if (hass == HAS_HASS_DONTUSE) {
        has_flush(has);
        return 0;
    }
    if (pid == 0 || mt != HAS_MT1) {
        return 0; /* PID 0 reserved; only MT1 carries corrections */
    }

    col = has_find_collect(has, mid, mt, time);
    /* Restart the collection when the slot is fresh, or when an in-flight
     * collector for this (MID, MT) reports a different MS. A MID is reused
     * across content generations within the 150 s window, and a new generation
     * (or a corrupted header) can carry a different MS. Mixing pages from two
     * generations would feed the RS(255,32) decoder the wrong page count k and
     * recover garbage, so drop the previously collected pages and begin again
     * with this page. (PID duplicates that differ in payload across generations
     * are first-wins; the toh/latency gate in decode_has_mt1 catches any
     * residue that still RS-decodes.) */
    if (!col->active || col->mid != mid || col->mt != mt || col->ms != ms) {
        if (col->active && col->mid == mid && col->mt == mt && col->ms != ms) {
            trace(NULL, 4, "has_input_page: MS change mid=%d %d->%d, restart collection\n", mid, col->ms, ms);
        }
        memset(col, 0, sizeof(has_collect_t));
        col->active = 1;
        col->mid = mid;
        col->mt = mt;
        col->ms = ms;
        col->t0 = time;
    }

    for (k = 0; k < col->npid; k++) {
        if (col->pid[k] == pid) {
            return 0; /* duplicate PID already stored */
        }
    }
    if (col->npid >= HAS_MAX_PAGES) {
        return 0;
    }
    col->pid[col->npid] = (uint8_t)pid;
    memcpy(col->page[col->npid], page56 + 3, HAS_PAGE_BYTES); /* skip 24-bit header */
    col->npid++;

    if (col->npid >= col->ms) {
        return has_complete(has, col, time);
    }
    return 0;
}

/* ---------------------------------------------------------------------------*/
extern int has_input_file(has_t* has, FILE* fp, gtime_t tmax) {
    uint8_t rec[64];
    gtime_t time;
    uint32_t tow_ms;
    int wnc, prn, ret, completed = 0;

    while (fread(rec, 1, 64, fp) == 64) {
        /* little-endian record fields ([1] design doc 3) */
        tow_ms = (uint32_t)rec[0] | ((uint32_t)rec[1] << 8) | ((uint32_t)rec[2] << 16) | ((uint32_t)rec[3] << 24);
        wnc = (int)((uint16_t)rec[4] | ((uint16_t)rec[5] << 8));
        prn = rec[6];

        time = gpst2time(wnc, tow_ms * 1.0e-3);
        if (tmax.time != 0 && timediff(time, tmax) > 0.0) {
            /* rewind so the next call re-reads this record */
            fseek(fp, -64, SEEK_CUR);
            break;
        }

        ret = has_input_page(has, prn, rec + 8, time);
        if (ret == 10) {
            completed = 10;
        } else if (ret < 0) {
            trace(NULL, 2, "has_input_file: page decode error\n");
        }
    }
    return completed;
}

/* select the HAS bias signal for an observation code -------------------------
 * HAS stores each bias under the code of its mask signal (Table 20), e.g. E1-C
 * -> C1C, while has_sel_biascode() maps every code of a signal to its combined
 * code (C1X). Take the observed code when HAS provides it, then the other
 * codes of the same signal (combined first). The combined and component biases
 * are taken as equal here (#473); a receiver forms the combined signal with its
 * own weighting of the components.
 *-----------------------------------------------------------------------------*/
extern int has_sel_bias(const ssr_t* ssr, int sys, uint8_t code, int phase) {
    const int* valid = phase ? ssr->vpbias : ssr->vcbias;
    int sig = has_sel_biascode(sys, code), c;

    if (sig == CODE_NONE) {
        return CODE_NONE;
    }
    if (valid[code - 1]) {
        return code;
    }
    if (valid[sig - 1]) {
        return sig;
    }
    for (c = 1; c <= MAXCODE; c++) {
        if (valid[c - 1] && has_sel_biascode(sys, (uint8_t)c) == sig) {
            return c;
        }
    }
    return sig;
}

/* ---------------------------------------------------------------------------*/
extern int has_sel_biascode(int sys, uint8_t code) {
    if (sys == SYS_GAL) {
        switch (code) {
            case CODE_L1B: /* E1-B */
            case CODE_L1C: /* E1-C */
            case CODE_L1X:
                return CODE_L1X; /* E1-B+C */
            case CODE_L5I:       /* E5a-I */
            case CODE_L5Q:       /* E5a-Q */
            case CODE_L5X:
                return CODE_L5X; /* E5a-I+Q */
            case CODE_L7I:       /* E5b-I */
            case CODE_L7Q:       /* E5b-Q */
            case CODE_L7X:
                return CODE_L7X; /* E5b-I+Q */
            case CODE_L8I:       /* E5-I */
            case CODE_L8Q:       /* E5-Q */
            case CODE_L8X:
                return CODE_L8X; /* E5-I+Q */
            case CODE_L6B:       /* E6-B */
            case CODE_L6C:       /* E6-C */
            case CODE_L6X:
                return CODE_L6X; /* E6-B+C */
        }
    } else if (sys == SYS_GPS) {
        switch (code) {
            case CODE_L1C:
                return CODE_L1C; /* L1 C/A */
            case CODE_L1S:       /* L1C(D) */
            case CODE_L1L:       /* L1C(P) */
            case CODE_L1X:
                return CODE_L1X; /* L1C(D+P) */
            case CODE_L2S:       /* L2 CM */
            case CODE_L2L:       /* L2 CL */
            case CODE_L2X:
                return CODE_L2X; /* L2 CM+CL */
            case CODE_L2W:
                return CODE_L2W; /* L2 P */
            case CODE_L5I:       /* L5 I */
            case CODE_L5Q:       /* L5 Q */
            case CODE_L5X:
                return CODE_L5X; /* L5 I+Q */
        }
    }
    return CODE_NONE;
}
