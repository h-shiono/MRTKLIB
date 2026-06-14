/*------------------------------------------------------------------------------
 * mrtk_b2b.c : BeiDou PPP-B2b processing functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Reference:
 *   [1] BeiDou Navigation Satellite System Signal In Space Interface Control
 *       Document, Precise Point Positioning Service Signal PPP-B2b
 *       (Version 1.0), China Satellite Navigation Office, July 2020.
 *----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_b2b.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_trace.h"

/* forward declarations (resolved at link time) ------------------------------*/
extern int satno(int sys, int prn);
extern void satno2id(int sat, char* id);
extern char* time_str(gtime_t t, int n);

/* constants -----------------------------------------------------------------*/

#define B2B_CRC_BITS 462 /* CRC-24 coverage: MesType(6) + data(456) [1] 6.1.2 */

#define B2B_MT_MASK 1  /* message type 1: satellite mask */
#define B2B_MT_ORBIT 2 /* message type 2: orbit correction + URAI */
#define B2B_MT_CBIAS 3 /* message type 3: differential code bias */
#define B2B_MT_CLOCK 4 /* message type 4: clock correction */
#define B2B_MT_URAI 5  /* message type 5: URAI */
#define B2B_MT_COMB1 6 /* message type 6: clock + orbit combination 1 */
#define B2B_MT_COMB2 7 /* message type 7: clock + orbit combination 2 */
#define B2B_MT_NULL 63 /* message type 63: null message */

#define B2B_UDI_ORBIT 96.0    /* nominal orbit validity (s) [1] Table 6-12 */
#define B2B_UDI_CLOCK 12.0    /* nominal clock validity (s) [1] Table 6-12 */
#define B2B_UDI_CBIAS 86400.0 /* nominal DCB validity (s) [1] Table 6-12 */

/* BDS signal-and-tracking mode -> obs code ([1] Table 6-5) ------------------*/
static const uint8_t b2b_sig_bds[16] = {CODE_L2I,  CODE_L1D,  CODE_L1P,  CODE_NONE, CODE_L5D,  CODE_L5P,
                                        CODE_NONE, CODE_L7D,  CODE_L7P,  CODE_NONE, CODE_NONE, CODE_NONE,
                                        CODE_L6I,  CODE_NONE, CODE_NONE, CODE_NONE};

/* GPS signal-and-tracking mode -> obs code ([1] Table 6-5) ------------------*/
static const uint8_t b2b_sig_gps[16] = {CODE_L1C,  CODE_L1P, CODE_NONE, CODE_NONE, CODE_L1L,  CODE_L1X,
                                        CODE_NONE, CODE_L2L, CODE_L2X,  CODE_NONE, CODE_NONE, CODE_L5I,
                                        CODE_L5Q,  CODE_L5X, CODE_NONE, CODE_NONE};

/* Galileo signal-and-tracking mode -> obs code ([1] Table 6-5) --------------*/
static const uint8_t b2b_sig_gal[16] = {CODE_NONE, CODE_L1B,  CODE_L1C,  CODE_NONE, CODE_L5Q,  CODE_L5I,
                                        CODE_NONE, CODE_L7I,  CODE_L7Q,  CODE_NONE, CODE_NONE, CODE_L6C,
                                        CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE};

/* GLONASS signal-and-tracking mode -> obs code ([1] Table 6-5) --------------*/
static const uint8_t b2b_sig_glo[16] = {CODE_L1C,  CODE_L1P,  CODE_L2C,  CODE_NONE, CODE_NONE, CODE_NONE,
                                        CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE,
                                        CODE_NONE, CODE_NONE, CODE_NONE, CODE_NONE};

/* CRC-24Q over the leading nbits of a MSB-first bit string ------------------
 * The PPP-B2b CRC generator ([1] Eq. 6-1) is the RTCM CRC-24Q polynomial
 * 0x1864CFB; the coverage (462 bits) is not byte-aligned, so compute bitwise. */
static uint32_t b2b_crc24q(const uint8_t* buff, int nbits) {
    uint32_t crc = 0;
    int i;

    for (i = 0; i < nbits; i++) {
        crc ^= getbitu(buff, i, 1) << 23;
        crc = ((crc & 0x800000) ? (crc << 1) ^ 0x1864CFB : crc << 1) & 0xFFFFFF;
    }
    return crc;
}

/* bounds check: are bits [i, i+n) within the message data region? -----------*/
static int b2b_bits_ok(int i, int n) { return i >= 0 && n >= 0 && i + n <= B2B_CRC_BITS; }

/* signed field invalid encodings ([1] notes to Tables 6-3/-4/-6) ------------
 * Both the most-negative two's-complement value and its successor are treated
 * as "no correction available" (cssrlib convention; the ICD marks the value
 * "invalid if it exceeds the effective range"). */
static int b2b_invalid(int v, int nbits) {
    int lim = 1 << (nbits - 1);
    return v == -lim || v == -(lim - 1);
}

/* mask slot (1-255) -> satno ([1] Table 6-2) --------------------------------*/
static int b2b_slot2sat(int slot) {
    if (slot >= 1 && slot <= 63) {
        return satno(SYS_CMP, slot);
    }
    if (slot >= 64 && slot <= 100) {
        return satno(SYS_GPS, slot - 63);
    }
    if (slot >= 101 && slot <= 137) {
        return satno(SYS_GAL, slot - 100);
    }
    if (slot >= 138 && slot <= 174) {
        return satno(SYS_GLO, slot - 137);
    }
    return 0; /* reserved slots 175-255 */
}

/* IODN -> broadcast-ephemeris IODE for seleph() matching ([1] 6.2.3.2) ------
 * GPS: IODN is the LNAV IODC; its low 8 bits equal the IODE. BDS: IODN is the
 * B-CNAV1 IODC; match its low 8 bits against the CNAV1 IODE (cssrlib does the
 * same masking). Galileo IODnav and GLONASS Tb are used as-is. */
static int b2b_iodn2iode(int sys, int iodn) {
    if (sys == SYS_GPS || sys == SYS_CMP) {
        return iodn & 0xFF;
    }
    return iodn;
}

/* time of applicability from the 17-bit BDT seconds-of-day field ------------
 * The BDT day is taken from the reception time, with a half-day window to
 * absorb a day rollover between generation and reception. */
static gtime_t b2b_epoch_time(gtime_t time, int tod) {
    double tow, sod, dt;
    int week;

    tow = time2bdt(gpst2bdt(time), &week);
    sod = fmod(tow, 86400.0);
    dt = (double)tod - sod;
    if (dt < -43200.0) {
        dt += 86400.0;
    } else if (dt > 43200.0) {
        dt -= 86400.0;
    }
    return bdt2gpst(bdt2time(week, tow + dt));
}

/* decode the common per-type header (Epoch/Reserved/IOD SSR) ----------------
 * returns updated bit position; *t0 = time of applicability, *iodssr out */
static int decode_b2b_head(const uint8_t* msg, int i, gtime_t time, gtime_t* t0, int* iodssr) {
    int tod;

    if (!b2b_bits_ok(i, 23)) {
        return -1;
    }
    tod = (int)getbitu(msg, i, 17);
    i += 17;
    i += 4; /* Reserved */
    *iodssr = (int)getbitu(msg, i, 2);
    i += 2;

    if (tod > 86399) {
        return -1; /* BDT seconds-of-day out of range */
    }
    *t0 = b2b_epoch_time(time, tod);
    return i;
}

/* decode MT1 satellite mask ([1] 6.2.2) -------------------------------------*/
static int decode_b2b_mask(b2b_t* b2b, const uint8_t* msg, int i, gtime_t time) {
    b2b_mask_t* m = &b2b->mask;
    gtime_t t0;
    int k, iodssr, iodp, nsat = 0;

    if ((i = decode_b2b_head(msg, i, time, &t0, &iodssr)) < 0) {
        return -1;
    }
    if (!b2b_bits_ok(i, 4 + B2B_MASK_BITS)) {
        return -1;
    }
    iodp = (int)getbitu(msg, i, 4);
    i += 4;

    /* rebuilding the compressed satellite list is idempotent, so decode the
     * mask unconditionally; the IODP transition rule ([1] 6.2.2.2: keep using
     * the old mask until the other types match) is enforced on the consumer
     * side by the IODP gates in MT4/MT5/MT6. */
    m->valid = 0;
    for (k = 0; k < B2B_MASK_BITS; k++) {
        if (!getbitu(msg, i + k, 1)) {
            continue;
        }
        if (nsat >= B2B_MASK_BITS) {
            break;
        }
        m->sat[nsat++] = b2b_slot2sat(k + 1); /* mask bit k <-> slot k+1 */
    }
    i += B2B_MASK_BITS;

    m->iodp = iodp;
    m->nsat = nsat;
    m->valid = 1;
    b2b->iodssr = iodssr;

    trace(NULL, 4, "decode_b2b_mask: %s, iodssr=%d, iodp=%d, nsat=%d\n", time_str(t0, 0), iodssr, iodp, nsat);
    return i;
}

/* decode one 69-bit orbit-correction block ([1] 6.2.3, Fig. 6-4) ------------
 * shared by MT2, MT6 and MT7; returns updated bit position, -1 on overrun */
static int decode_b2b_orb_sat(b2b_t* b2b, const uint8_t* msg, int i, gtime_t t0) {
    ssr_t* ssr;
    int slot, iodn, iodcorr, dr, da, dc, ucls, uval, sat, sys, prn;
    char satid[8];

    if (!b2b_bits_ok(i, 69)) {
        return -1;
    }
    slot = (int)getbitu(msg, i, 9);
    i += 9;
    iodn = (int)getbitu(msg, i, 10);
    i += 10;
    iodcorr = (int)getbitu(msg, i, 3);
    i += 3;
    dr = getbits(msg, i, 15);
    i += 15;
    da = getbits(msg, i, 13);
    i += 13;
    dc = getbits(msg, i, 13);
    i += 13;
    ucls = (int)getbitu(msg, i, 3);
    i += 3;
    uval = (int)getbitu(msg, i, 3);
    i += 3;

    sat = b2b_slot2sat(slot);
    if (sat == 0) {
        return i; /* slot 0 = unused filler entry, or unsupported slot */
    }
    sys = satsys(sat, &prn);
    ssr = b2b->ssr + sat - 1;

    if (b2b_invalid(dr, 15) || b2b_invalid(da, 13) || b2b_invalid(dc, 13)) {
        /* orbit correction unavailable -> clear so satpos_ssr rejects the sat */
        ssr->t0[0].time = 0;
        ssr->t0[0].sec = 0.0;
        ssr->update = 1;
        return i;
    }

    /* sign convention: [1] Eq. 7-5 X_orbit = X_broadcast - [e_r e_a e_c]*dO,
     * which matches satpos_ssr's rs += -R*deph -> store dO as broadcast
     * (no negation, unlike Galileo HAS). */
    ssr->deph[0] = dr * 0.0016;
    ssr->deph[1] = da * 0.0064;
    ssr->deph[2] = dc * 0.0064;
    ssr->ddeph[0] = ssr->ddeph[1] = ssr->ddeph[2] = 0.0;
    ssr->iode = b2b_iodn2iode(sys, iodn);
    /* The orbit block owns iod[0]/t0[0] only; the clock messages own
     * iod[1]/t0[1]. satpos_ssr's iod[0]==iod[1] gate then implements the
     * ICD rule that orbit and clock combine only with equal IOD Corr
     * ([1] 6.2.1.2). */
    ssr->iod[0] = iodcorr;
    ssr->t0[0] = t0;
    ssr->udi[0] = B2B_UDI_ORBIT;
    ssr->ura = (ucls << 3) | uval;
    ssr->update = 1;

    if (satno2id(sat, satid), 1) {
        trace(NULL, 4, "decode_b2b_orb_sat: %s, sat=%s, iodn=%d, iodcorr=%d, dr=%.4f, da=%.4f, dc=%.4f\n",
              time_str(t0, 0), satid, iodn, iodcorr, ssr->deph[0], ssr->deph[1], ssr->deph[2]);
    }
    return i;
}

/* store one clock correction ([1] 6.2.5: IOD Corr + C0) ---------------------*/
static void store_b2b_clk_sat(b2b_t* b2b, int sat, int iodcorr, int c0, gtime_t t0) {
    ssr_t* ssr;

    if (sat == 0) {
        return;
    }
    ssr = b2b->ssr + sat - 1;

    if (b2b_invalid(c0, 15)) {
        /* clock correction unavailable -> clear so satpos_ssr rejects the sat */
        ssr->t0[1].time = 0;
        ssr->t0[1].sec = 0.0;
        ssr->update = 1;
        return;
    }
    /* sign convention: [1] Eq. 7-10 t_sat = t_broadcast - C0/c; satpos_ssr adds
     * dclk to the broadcast clock -> store the negated value (cssrlib ditto). */
    ssr->dclk[0] = -c0 * 0.0016;
    ssr->dclk[1] = ssr->dclk[2] = 0.0;
    ssr->hrclk = 0.0;
    ssr->iod[1] = iodcorr;
    ssr->t0[1] = t0;
    ssr->udi[1] = B2B_UDI_CLOCK;
    ssr->update = 1;
}

/* decode MT2 orbit correction + URAI ([1] 6.2.3) ----------------------------*/
static int decode_b2b_orbit(b2b_t* b2b, const uint8_t* msg, int i, gtime_t time) {
    gtime_t t0;
    int k, iodssr;

    if ((i = decode_b2b_head(msg, i, time, &t0, &iodssr)) < 0) {
        return -1;
    }
    if (b2b->iodssr < 0 || iodssr != b2b->iodssr) {
        return 0; /* configuration mismatch: unusable until a matching MT1 */
    }
    for (k = 0; k < 6; k++) {
        if ((i = decode_b2b_orb_sat(b2b, msg, i, t0)) < 0) {
            return -1;
        }
    }
    return 10;
}

/* decode MT3 differential code bias ([1] 6.2.4) -----------------------------*/
static int decode_b2b_cbias(b2b_t* b2b, const uint8_t* msg, int i, gtime_t time) {
    gtime_t t0;
    ssr_t* ssr;
    uint8_t code;
    int j, k, iodssr, nsat, slot, ncb, mode, dcb, sat, sys, prn, updated = 0;

    if ((i = decode_b2b_head(msg, i, time, &t0, &iodssr)) < 0) {
        return -1;
    }
    if (b2b->iodssr < 0 || iodssr != b2b->iodssr) {
        return 0;
    }
    if (!b2b_bits_ok(i, 5)) {
        return -1;
    }
    nsat = (int)getbitu(msg, i, 5);
    i += 5;

    for (j = 0; j < nsat; j++) {
        if (!b2b_bits_ok(i, 13)) {
            return -1;
        }
        slot = (int)getbitu(msg, i, 9);
        i += 9;
        ncb = (int)getbitu(msg, i, 4);
        i += 4;

        sat = b2b_slot2sat(slot);
        sys = sat ? satsys(sat, &prn) : SYS_NONE;

        for (k = 0; k < ncb; k++) {
            if (!b2b_bits_ok(i, 16)) {
                return -1;
            }
            mode = (int)getbitu(msg, i, 4);
            i += 4;
            dcb = getbits(msg, i, 12);
            i += 12;

            if (sat == 0 || b2b_invalid(dcb, 12)) {
                continue;
            }
            switch (sys) {
                case SYS_CMP:
                    code = b2b_sig_bds[mode];
                    break;
                case SYS_GPS:
                    code = b2b_sig_gps[mode];
                    break;
                case SYS_GAL:
                    code = b2b_sig_gal[mode];
                    break;
                case SYS_GLO:
                    code = b2b_sig_glo[mode];
                    break;
                default:
                    code = CODE_NONE;
                    break;
            }
            if (code == CODE_NONE) {
                continue;
            }
            ssr = b2b->ssr + sat - 1;
            /* sign convention: [1] Eq. 7-1 corrected = observed - DCB;
             * corr_meas applies P += cbias -> store the negated value. */
            ssr->cbias[code - 1] = (float)(-dcb * 0.017);
            ssr->vcbias[code - 1] = 1;
            ssr->t0[4] = t0;
            ssr->udi[4] = B2B_UDI_CBIAS;
            ssr->update = 1;
            updated = 1;
        }
    }
    return updated ? 10 : 0;
}

/* decode MT4 clock correction ([1] 6.2.5) -----------------------------------*/
static int decode_b2b_clock(b2b_t* b2b, const uint8_t* msg, int i, gtime_t time) {
    gtime_t t0;
    int k, iodssr, iodp, st1, iodcorr, c0, idx, sat;

    if ((i = decode_b2b_head(msg, i, time, &t0, &iodssr)) < 0) {
        return -1;
    }
    if (!b2b_bits_ok(i, 9 + 23 * 18)) {
        return -1;
    }
    iodp = (int)getbitu(msg, i, 4);
    i += 4;
    st1 = (int)getbitu(msg, i, 5);
    i += 5;

    if (b2b->iodssr < 0 || iodssr != b2b->iodssr) {
        return 0;
    }
    if (!b2b->mask.valid || iodp != b2b->mask.iodp) {
        return 0; /* mask mismatch: satellite indexing undefined ([1] 6.2.2.2) */
    }
    for (k = 0; k < 23; k++) {
        iodcorr = (int)getbitu(msg, i, 3);
        i += 3;
        c0 = getbits(msg, i, 15);
        i += 15;

        idx = st1 * 23 + k;
        if (idx >= b2b->mask.nsat) {
            continue;
        }
        sat = b2b->mask.sat[idx];
        store_b2b_clk_sat(b2b, sat, iodcorr, c0, t0);
    }
    trace(NULL, 4, "decode_b2b_clock: %s, iodp=%d, subtype1=%d\n", time_str(t0, 0), iodp, st1);
    return 10;
}

/* decode MT5 URAI ([1] 6.2.6) ------------------------------------------------*/
static int decode_b2b_urai(b2b_t* b2b, const uint8_t* msg, int i, gtime_t time) {
    gtime_t t0;
    int k, iodssr, iodp, st2, ucls, uval, idx, sat;

    if ((i = decode_b2b_head(msg, i, time, &t0, &iodssr)) < 0) {
        return -1;
    }
    if (!b2b_bits_ok(i, 7 + 70 * 6)) {
        return -1;
    }
    iodp = (int)getbitu(msg, i, 4);
    i += 4;
    st2 = (int)getbitu(msg, i, 3);
    i += 3;

    if (b2b->iodssr < 0 || iodssr != b2b->iodssr) {
        return 0;
    }
    if (!b2b->mask.valid || iodp != b2b->mask.iodp) {
        return 0;
    }
    for (k = 0; k < 70; k++) {
        ucls = (int)getbitu(msg, i, 3);
        i += 3;
        uval = (int)getbitu(msg, i, 3);
        i += 3;

        idx = st2 * 70 + k;
        if (idx >= b2b->mask.nsat) {
            continue;
        }
        sat = b2b->mask.sat[idx];
        if (sat == 0) {
            continue;
        }
        b2b->ssr[sat - 1].ura = (ucls << 3) | uval;
        /* URAI alone does not flag update: it rides along with orbit/clock */
    }
    return 0;
}

/* decode MT6 clock + orbit combination 1 ([1] 6.2.7) ------------------------*/
static int decode_b2b_comb1(b2b_t* b2b, const uint8_t* msg, int i, gtime_t time) {
    gtime_t t0;
    int k, numc, numo, iodssr, iodp, slot_s, iodcorr, c0, idx, ret = 0;

    if (!b2b_bits_ok(i, 8)) {
        return -1;
    }
    numc = (int)getbitu(msg, i, 5);
    i += 5;
    numo = (int)getbitu(msg, i, 3);
    i += 3;

    if (numc > 0) {
        if ((i = decode_b2b_head(msg, i, time, &t0, &iodssr)) < 0) {
            return -1;
        }
        if (!b2b_bits_ok(i, 13 + numc * 18)) {
            return -1;
        }
        iodp = (int)getbitu(msg, i, 4);
        i += 4;
        slot_s = (int)getbitu(msg, i, 9);
        i += 9;

        if (b2b->iodssr >= 0 && iodssr == b2b->iodssr && b2b->mask.valid && iodp == b2b->mask.iodp) {
            for (k = 0; k < numc; k++) {
                iodcorr = (int)getbitu(msg, i, 3);
                i += 3;
                c0 = getbits(msg, i, 15);
                i += 15;

                /* Slot_S is the 1-based position of the first satellite in the
                 * mask-compressed sequence ([1] 6.2.7.2 example: 48 masked sats,
                 * MT4 carries the 1st-46th, MT6 the 47th/48th with Slot_S=47).
                 * NOTE: cssrlib indexes sat_n[slot_s + k] (0-based, i.e. one
                 * satellite later); arbitrate against live data if MT6 appears. */
                idx = slot_s - 1 + k;
                if (idx < 0 || idx >= b2b->mask.nsat) {
                    continue;
                }
                store_b2b_clk_sat(b2b, b2b->mask.sat[idx], iodcorr, c0, t0);
            }
            ret = 10;
        } else {
            i += numc * 18; /* skip clock run with stale IODP/IOD SSR */
        }
    }
    if (numo > 0) {
        if ((i = decode_b2b_head(msg, i, time, &t0, &iodssr)) < 0) {
            return -1;
        }
        if (b2b->iodssr >= 0 && iodssr == b2b->iodssr) {
            for (k = 0; k < numo; k++) {
                if ((i = decode_b2b_orb_sat(b2b, msg, i, t0)) < 0) {
                    return -1;
                }
            }
            ret = 10;
        }
    }
    return ret;
}

/* decode MT7 clock + orbit combination 2 ([1] 6.2.8) ------------------------*/
static int decode_b2b_comb2(b2b_t* b2b, const uint8_t* msg, int i, gtime_t time) {
    gtime_t t0;
    int k, numc, numo, iodssr, slot, iodcorr, c0, ret = 0;

    if (!b2b_bits_ok(i, 8)) {
        return -1;
    }
    numc = (int)getbitu(msg, i, 5);
    i += 5;
    numo = (int)getbitu(msg, i, 3);
    i += 3;

    if (numc > 0) {
        if ((i = decode_b2b_head(msg, i, time, &t0, &iodssr)) < 0) {
            return -1;
        }
        if (!b2b_bits_ok(i, numc * 27)) {
            return -1;
        }
        if (b2b->iodssr >= 0 && iodssr == b2b->iodssr) {
            for (k = 0; k < numc; k++) {
                slot = (int)getbitu(msg, i, 9);
                i += 9;
                iodcorr = (int)getbitu(msg, i, 3);
                i += 3;
                c0 = getbits(msg, i, 15);
                i += 15;
                store_b2b_clk_sat(b2b, b2b_slot2sat(slot), iodcorr, c0, t0);
            }
            ret = 10;
        } else {
            i += numc * 27;
        }
    }
    if (numo > 0) {
        if ((i = decode_b2b_head(msg, i, time, &t0, &iodssr)) < 0) {
            return -1;
        }
        if (b2b->iodssr >= 0 && iodssr == b2b->iodssr) {
            for (k = 0; k < numo; k++) {
                if ((i = decode_b2b_orb_sat(b2b, msg, i, t0)) < 0) {
                    return -1;
                }
            }
            ret = 10;
        }
    }
    return ret;
}

/* ---------------------------------------------------------------------------*/
extern b2b_t* b2b_new(void) {
    b2b_t* b2b;

    b2b = (b2b_t*)calloc(1, sizeof(b2b_t));
    if (b2b) {
        b2b->iodssr = -1;
    }
    return b2b;
}

/* ---------------------------------------------------------------------------*/
extern void b2b_free(b2b_t* b2b) { free(b2b); }

/* ---------------------------------------------------------------------------*/
extern int b2b_input_msg(b2b_t* b2b, int prn, const uint8_t* msg61, gtime_t time) {
    int mt;

    /* PPP-B2b SSR corrections are broadcast only by the BDS-3 GEO satellites
     * (C59-C63; [1] 4.1), and all GEOs carry the same correction stream. The
     * B2b-I signal from the IGSO/MEO satellites instead carries B-CNAV3
     * navigation, whose message-type field collides numerically with the
     * PPP-B2b MTs and would corrupt the mask/orbit/clock state. Drop any
     * non-GEO PRN so a mixed real-time B2b stream is filtered to corrections
     * only (the post-processing .b2b path already carries GEO frames only). */
    if (prn < 59 || prn > 63) {
        return 0;
    }

    mt = (int)getbitu(msg61, 0, 6);
    if (mt == B2B_MT_NULL) {
        return 0;
    }
    if (b2b_crc24q(msg61, B2B_CRC_BITS) != getbitu(msg61, B2B_CRC_BITS, 24)) {
        trace(NULL, 2, "b2b_input_msg: CRC error mt=%d\n", mt);
        return -1;
    }

    switch (mt) {
        case B2B_MT_MASK:
            return decode_b2b_mask(b2b, msg61, 6, time) < 0 ? -1 : 0;
        case B2B_MT_ORBIT:
            return decode_b2b_orbit(b2b, msg61, 6, time);
        case B2B_MT_CBIAS:
            return decode_b2b_cbias(b2b, msg61, 6, time);
        case B2B_MT_CLOCK:
            return decode_b2b_clock(b2b, msg61, 6, time);
        case B2B_MT_URAI:
            return decode_b2b_urai(b2b, msg61, 6, time);
        case B2B_MT_COMB1:
            return decode_b2b_comb1(b2b, msg61, 6, time);
        case B2B_MT_COMB2:
            return decode_b2b_comb2(b2b, msg61, 6, time);
    }
    return 0; /* reserved message types 8-62 */
}

/* ---------------------------------------------------------------------------*/
extern int b2b_input_file(b2b_t* b2b, FILE* fp, gtime_t tmax) {
    uint8_t rec[B2B_REC_BYTES];
    gtime_t time;
    uint32_t tow_ms;
    int wnc, prn, ret, completed = 0;

    while (fread(rec, 1, B2B_REC_BYTES, fp) == B2B_REC_BYTES) {
        /* little-endian record fields (see b2b_input_file docstring) */
        tow_ms = (uint32_t)rec[0] | ((uint32_t)rec[1] << 8) | ((uint32_t)rec[2] << 16) | ((uint32_t)rec[3] << 24);
        wnc = (int)((uint16_t)rec[4] | ((uint16_t)rec[5] << 8));
        prn = rec[6];

        time = gpst2time(wnc, tow_ms * 1.0e-3);
        if (tmax.time != 0 && timediff(time, tmax) > 0.0) {
            /* rewind so the next call re-reads this record */
            fseek(fp, -B2B_REC_BYTES, SEEK_CUR);
            break;
        }

        ret = b2b_input_msg(b2b, prn, rec + 8, time);
        if (ret == 10) {
            completed = 10;
        } else if (ret < 0) {
            trace(NULL, 2, "b2b_input_file: message decode error\n");
        }
    }
    return completed;
}

/* ---------------------------------------------------------------------------*/
extern int b2b_sel_biascode(int sys, uint8_t code) {
    if (sys == SYS_CMP) {
        switch (code) {
            case CODE_L2I: /* B1I */
            case CODE_L2Q:
            case CODE_L2X:
                return CODE_L2I;
            case CODE_L1D: /* B1C(D) */
                return CODE_L1D;
            case CODE_L1P: /* B1C(P) */
            case CODE_L1X:
                return CODE_L1P;
            case CODE_L5D: /* B2a(D) */
                return CODE_L5D;
            case CODE_L5P: /* B2a(P) */
            case CODE_L5X:
                return CODE_L5P;
            case CODE_L7D: /* B2b-I */
                return CODE_L7D;
            case CODE_L7P: /* B2b-Q */
            case CODE_L7Z:
                return CODE_L7P;
            case CODE_L6I: /* B3I */
            case CODE_L6Q:
            case CODE_L6X:
                return CODE_L6I;
        }
    } else if (sys == SYS_GPS) {
        switch (code) {
            case CODE_L1C: /* L1 C/A */
                return CODE_L1C;
            case CODE_L1P: /* L1 P */
            case CODE_L1W:
                return CODE_L1P;
            case CODE_L1L: /* L1C(P) */
                return CODE_L1L;
            case CODE_L1S: /* L1C(D) -> L1C(D+P) */
            case CODE_L1X:
                return CODE_L1X;
            case CODE_L2L: /* L2C(L) */
                return CODE_L2L;
            case CODE_L2S: /* L2C(M) -> L2C(M+L) */
            case CODE_L2X:
                return CODE_L2X;
            case CODE_L5I: /* L5 I */
                return CODE_L5I;
            case CODE_L5Q: /* L5 Q */
                return CODE_L5Q;
            case CODE_L5X: /* L5 I+Q */
                return CODE_L5X;
        }
    } else if (sys == SYS_GAL) {
        switch (code) {
            case CODE_L1B: /* E1-B */
                return CODE_L1B;
            case CODE_L1C: /* E1-C */
            case CODE_L1X:
                return CODE_L1C;
            case CODE_L5I: /* E5a-I */
                return CODE_L5I;
            case CODE_L5Q: /* E5a-Q */
            case CODE_L5X:
                return CODE_L5Q;
            case CODE_L7I: /* E5b-I */
                return CODE_L7I;
            case CODE_L7Q: /* E5b-Q */
            case CODE_L7X:
                return CODE_L7Q;
            case CODE_L6C: /* E6-C */
            case CODE_L6X:
                return CODE_L6C;
        }
    } else if (sys == SYS_GLO) {
        switch (code) {
            case CODE_L1C: /* G1 C/A */
                return CODE_L1C;
            case CODE_L1P: /* G1 P */
                return CODE_L1P;
            case CODE_L2C: /* G2 C/A */
                return CODE_L2C;
        }
    }
    return CODE_NONE;
}
