/*------------------------------------------------------------------------------
 * unit test : BeiDou PPP-B2b decoder (mrtk_b2b.c)
 *
 * Drives the public b2b_input_msg() API with synthetic 61-byte messages
 * (486 bits MSB-first: MesType(6) + data(456) + CRC-24Q over the first 462
 * bits). No real broadcast data is needed: each test builds the bit layout
 * with setbitu/setbits and finishes it with a local CRC-24Q identical to the
 * module's internal b2b_crc24q (poly 0x1864CFB, bitwise over 462 bits).
 *
 * Coverage (one block per numbered case in the task spec):
 *   1. happy path MT1->MT2->MT4->MT3: orbit/clock/cbias fields, signs, scaling,
 *      iode masking, udi values, update flag, iod[0]==iod[1] gate.
 *   2. CRC corruption: one flipped bit -> b2b_input_msg returns -1, no state.
 *   3. MT63 null: returns 0, no state change.
 *   4. IOD SSR mismatch: MT2 iodssr != MT1 -> return 0, no orbit stored.
 *   5. IODP mismatch: MT4 iodp != MT1 -> return 0, no clock stored.
 *   6. invalid sentinels: radial=-16384 clears t0[0]; c0=-16383 clears t0[1].
 *   7. clock before mask: MT4 with no prior MT1 -> return 0, nothing stored.
 *   8. negative values decode to correct (sign-reversed) doubles.
 *   9. b2b_sel_biascode exact + fallback + unmapped (all four systems).
 *  10. out-of-range orbit slot (200) is skipped, cursor stays aligned, the
 *      following valid block in the same message still decodes.
 *  11. GEO gate (#417): frames from PRNs outside set_bdsgeo() (default
 *      C01-C04) are ignored; C59-C62 only when listed (pre-2026-04 data).
 *  12. SBF BDSCNav1 (4251) B-CNAV1 ephemeris (#417): one real block (C28,
 *      kubolab mosaic-G5, 2026-10-23 20:00:32 GPST) decodes to the values of
 *      the same record in BRD400DLR_S_20262960000_01D_MN.rnx (CNV1), into
 *      ephemeris set 1, and only while the BeiDou selection is B-CNAV.
 *  13. IOD SSR change in MT1 clears every staged orbit/clock/code bias, so a
 *      new orbit cannot pair with an old clock of the same IOD Corr, and
 *      returns 10 so the caller publishes the cleared entries.
 *  14. a withdrawn code bias (-2048 / -2047) clears cbias and vcbias.
 *  15. one GEO stream at a time: interleaved frames of another GEO (C04 with
 *      a different IOD SSR, as on 2026-10-23) are ignored until the decoded
 *      source has been silent for 10 s; the next source is taken on its mask.
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_b2b.h"
#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_rcvraw.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_time.h"

extern void setbitu(uint8_t* buff, int pos, int len, uint32_t data);
extern void setbits(uint8_t* buff, int pos, int len, int32_t data);
extern int satno(int sys, int prn);
extern gtime_t epoch2time(const double* ep);
extern gtime_t gpst2bdt(gtime_t t);
extern gtime_t bdt2gpst(gtime_t t);
extern gtime_t bdt2time(int week, double sec);
extern double time2bdt(gtime_t t, int* week);
extern double timediff(gtime_t t1, gtime_t t2);
extern double time2gpst(gtime_t t, int* week);
extern gtime_t timeadd(gtime_t t, double sec);

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

#define B2B_CRC_BITS 462
#define GEO_PRN 1 /* BDS-3 GEO in the default set_bdsgeo() list (C01-C04) */

/* MT1 mask with only C09 (slot 9) */
static void make_mask_c09(uint8_t* m, int tod, int iodssr, int iodp);
/* MT2 with one C09 orbit block (IOD Corr 3) */
static void make_orbit_c09(uint8_t* m, int tod, int iodssr);
/* MT4 subtype 0, C09 = mask index 0, IOD Corr 3 */
static void make_clock_c09(uint8_t* m, int tod, int iodssr, int iodp);
/* MT3 with one C09 bias, mode 0 (B1I) */
static void make_cbias_c09(uint8_t* m, int tod, int iodssr, int dcb);

/* SBF BDSCNav1 (4251) block of C28 as logged (header, CRC and padding included) */
static const uint8_t sbf_bdscnav1_c28[160] = {
    0x24, 0x40, 0x2c, 0x5f, 0x9b, 0x10, 0xa0, 0x00, 0x00, 0xeb, 0x0a, 0x1e, 0x89, 0x09, 0x1c, 0x03, 0xc0, 0xb0,
    0x07, 0x00, 0x00, 0x00, 0xb0, 0x20, 0x04, 0x9d, 0x7a, 0x41, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9b, 0x71, 0x3f,
    0x00, 0xce, 0xc3, 0x30, 0x00, 0x80, 0x22, 0xa8, 0x00, 0x00, 0x00, 0x59, 0xc1, 0xe5, 0xe7, 0xbf, 0x00, 0x00,
    0x00, 0xc0, 0xa0, 0x5c, 0x30, 0x3f, 0x00, 0x00, 0x00, 0x65, 0xc6, 0xf7, 0xde, 0xbf, 0x00, 0x00, 0x80, 0xa4,
    0x0b, 0x88, 0xdf, 0xbf, 0x00, 0xe5, 0x21, 0xb1, 0x00, 0x00, 0xc0, 0xfe, 0x91, 0x43, 0xd3, 0x3f, 0x00, 0x60,
    0x1d, 0xaf, 0x00, 0x00, 0x40, 0x32, 0x00, 0x00, 0x80, 0xb1, 0x00, 0x48, 0x82, 0xc2, 0x00, 0xcf, 0x94, 0x43,
    0x00, 0xa0, 0x3b, 0x36, 0x00, 0x80, 0x5b, 0xb6, 0xc0, 0xb0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90,
    0x82, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x7d, 0xbd, 0x42, 0x3f, 0xc0, 0xb0, 0x07, 0x00, 0x1e, 0x3f, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0xb0, 0x00, 0x00, 0x30, 0xb1, 0x00, 0x00, 0x2e, 0xb2,
};

/* feed the C28 block to a fresh SBF decoder; return the last input_sbf status
 * and copy ephemeris set 0/1 of C28 */
static int decode_cnav1_block(eph_t* set0, eph_t* set1, int* ephset) {
    raw_t* raw = (raw_t*)calloc(1, sizeof(raw_t));
    rtcm_t* rtcm = (rtcm_t*)calloc(1, sizeof(rtcm_t)); /* ~7.5 MB: heap only */
    int i, ret = -9, sat = satno(SYS_CMP, 28);

    if (!raw || !rtcm || !init_raw(raw, STRFMT_SEPT) || !init_rtcm(rtcm)) {
        free(raw);
        free(rtcm);
        return -9;
    }
    for (i = 0; i < (int)sizeof(sbf_bdscnav1_c28); i++) {
        ret = input_sbf(raw, rtcm, sbf_bdscnav1_c28[i]);
    }
    *set0 = raw->nav.eph[sat - 1];
    *set1 = raw->nav.eph[sat - 1 + MAXSAT];
    *ephset = raw->ephset;
    free_rtcm(rtcm);
    free_raw(raw);
    free(raw);
    free(rtcm);
    return ret;
}

/* MesType values mirrored from the module (kept local to avoid leaking the
 * decoder's private #defines into the test). */
#define MT_MASK 1
#define MT_ORBIT 2
#define MT_CBIAS 3
#define MT_CLOCK 4
#define MT_NULL 63

/* local re-implementation of the module-private b2b_crc24q (poly 0x1864CFB,
 * bitwise over the leading nbits) so the test can finish a message. */
static uint32_t local_crc24q(const uint8_t* buff, int nbits) {
    uint32_t crc = 0;
    int i;
    for (i = 0; i < nbits; i++) {
        crc ^= getbitu(buff, i, 1) << 23;
        crc = ((crc & 0x800000) ? (crc << 1) ^ 0x1864CFB : crc << 1) & 0xFFFFFF;
    }
    return crc;
}

/* zero a 61-byte buffer, write the 6-bit MesType, return the bit cursor just
 * past it (=6, where the common header begins). */
static int begin_msg(uint8_t* msg, int mt) {
    memset(msg, 0, B2B_MSG_BYTES);
    setbitu(msg, 0, 6, (uint32_t)mt);
    return 6;
}

/* write the 23-bit common header: tod(17) | rev(4) | iodssr(2). */
static int put_head(uint8_t* msg, int i, int tod, int iodssr) {
    setbitu(msg, i, 17, (uint32_t)tod);
    i += 17;
    i += 4; /* Reserved */
    setbitu(msg, i, 2, (uint32_t)iodssr);
    i += 2;
    return i;
}

/* finalize: append the CRC-24Q over the first 462 bits at bit 462. */
static void finish_msg(uint8_t* msg) { setbitu(msg, B2B_CRC_BITS, 24, local_crc24q(msg, B2B_CRC_BITS)); }

/* one 69-bit orbit block: slot(9) iodn(10) iodcorr(3) dr(s15) da(s13) dc(s13)
 * ura_cls(3) ura_val(3). */
static int put_orb(uint8_t* msg, int i, int slot, int iodn, int iodcorr, int dr, int da, int dc, int ucls, int uval) {
    setbitu(msg, i, 9, (uint32_t)slot);
    i += 9;
    setbitu(msg, i, 10, (uint32_t)iodn);
    i += 10;
    setbitu(msg, i, 3, (uint32_t)iodcorr);
    i += 3;
    setbits(msg, i, 15, dr);
    i += 15;
    setbits(msg, i, 13, da);
    i += 13;
    setbits(msg, i, 13, dc);
    i += 13;
    setbitu(msg, i, 3, (uint32_t)ucls);
    i += 3;
    setbitu(msg, i, 3, (uint32_t)uval);
    i += 3;
    return i;
}

/* expected time of applicability: same derivation as the module's
 * b2b_epoch_time, used to assert ssr->t0 latency. */
static gtime_t expect_t0(gtime_t time, int tod) {
    double tow, sod, dt;
    int week;
    tow = time2bdt(gpst2bdt(time), &week);
    sod = tow - (double)((long)(tow / 86400.0)) * 86400.0;
    dt = (double)tod - sod;
    if (dt < -43200.0) {
        dt += 86400.0;
    } else if (dt > 43200.0) {
        dt -= 86400.0;
    }
    return bdt2gpst(bdt2time(week, tow + dt));
}

int main(void) {
    /* reception time 2025/08/21 07:00:00 GPST. */
    double ep[6] = {2025, 8, 21, 7, 0, 0};
    gtime_t time = epoch2time(ep);

    /* BDT seconds-of-day at reception, then tod = SOD - 5 (5 s latency). */
    int week;
    double tow = time2bdt(gpst2bdt(time), &week);
    int sod = (int)(tow - (double)((long)(tow / 86400.0)) * 86400.0);
    int tod = sod - 5;

    int bds5 = satno(SYS_CMP, 5);
    int bds9 = satno(SYS_CMP, 9);
    int gps3 = satno(SYS_GPS, 3);
    CHECK(bds5 > 0 && bds9 > 0 && gps3 > 0, "satno BDS5/BDS9/GPS3");

    /* ===================================================================== *
     * 1. happy path: MT1 -> MT2 -> MT4 -> MT3                                *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        CHECK(b2b != NULL, "1: b2b_new");

        /* --- MT1: mask BDS C05, C09 (slots 5,9) and GPS G03 (slot 66) ----- */
        uint8_t m[B2B_MSG_BYTES];
        int i = begin_msg(m, MT_MASK);
        i = put_head(m, i, tod, /*iodssr*/ 1);
        setbitu(m, i, 4, /*iodp*/ 2);
        i += 4;
        /* mask bit k <-> slot k+1; set slots 5, 9, 66 (mask bits 4, 8, 65). */
        setbitu(m, i + 4, 1, 1);  /* slot 5 (BDS C05) */
        setbitu(m, i + 8, 1, 1);  /* slot 9 (BDS C09) */
        setbitu(m, i + 65, 1, 1); /* slot 66 (GPS G03) */
        finish_msg(m);
        int ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == 0, "1: MT1 returns 0 (mask only)");

        /* --- MT2: 2 valid BDS orbit blocks (C05, C09) + 4 slot-0 fillers -- */
        i = begin_msg(m, MT_ORBIT);
        i = put_head(m, i, tod, /*iodssr*/ 1);
        /* C05: iodn=0x123 -> iode = 0x23 ; iodcorr=3 ; dr=1000 da=-500 dc=200 */
        i = put_orb(m, i, /*slot*/ 5, /*iodn*/ 0x123, /*iodcorr*/ 3, 1000, -500, 200, /*cls*/ 2, /*val*/ 5);
        i = put_orb(m, i, /*slot*/ 9, /*iodn*/ 0x0AB, /*iodcorr*/ 3, -300, 400, -100, 1, 2);
        i = put_orb(m, i, 0, 0, 0, 0, 0, 0, 0, 0); /* filler */
        i = put_orb(m, i, 0, 0, 0, 0, 0, 0, 0, 0);
        i = put_orb(m, i, 0, 0, 0, 0, 0, 0, 0, 0);
        i = put_orb(m, i, 0, 0, 0, 0, 0, 0, 0, 0);
        finish_msg(m);
        ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == 10, "1: MT2 returns 10 (orbit updated)");

        ssr_t* s5 = &b2b->ssr[bds5 - 1];
        ssr_t* s9 = &b2b->ssr[bds9 - 1];
        gtime_t t0 = expect_t0(time, tod);
        CHECK(fabs(s5->deph[0] - 1000 * 0.0016) < 1e-9, "1: C05 deph[0]=+dr*0.0016");
        CHECK(fabs(s5->deph[1] - (-500) * 0.0064) < 1e-9, "1: C05 deph[1]=+da*0.0064");
        CHECK(fabs(s5->deph[2] - 200 * 0.0064) < 1e-9, "1: C05 deph[2]=+dc*0.0064");
        CHECK(s5->iode == (0x123 & 0xff), "1: C05 iode = iodn&0xff (BDS)");
        CHECK(s5->iod[0] == 3, "1: C05 iod[0]=iodcorr");
        CHECK(fabs(s5->udi[0] - 96.0) < 1e-9, "1: C05 udi[0]=96.0");
        CHECK(s5->ura == ((2 << 3) | 5), "1: C05 ura=(cls<<3)|val");
        CHECK(s5->update == 1, "1: C05 update flag set");
        CHECK(s5->t0[0].time != 0, "1: C05 orbit t0[0] set");
        CHECK(fabs(timediff(time, s5->t0[0]) - 5.0) < 1e-6, "1: C05 t0[0] latency 5 s");
        CHECK(s9->iode == 0xAB, "1: C09 iode = iodn&0xff (BDS)");
        CHECK(s9->t0[1].time == 0, "1: C09 clock t0[1] absent before MT4");

        /* --- MT4: clock for masked sats, same iodssr/iodp, iodcorr=3 ------ */
        i = begin_msg(m, MT_CLOCK);
        i = put_head(m, i, tod, /*iodssr*/ 1);
        setbitu(m, i, 4, /*iodp*/ 2);
        i += 4;
        setbitu(m, i, 5, /*subtype1*/ 0); /* sats idx 0..22 -> mask order */
        i += 5;
        /* mask-compressed order: idx0=C05, idx1=C09, idx2=G03. */
        /* idx0 C05: iodcorr=3 c0=2000 ; idx1 C09: iodcorr=3 c0=-1500 ;
         * idx2 G03: iodcorr=3 c0=750 ; remaining 20 slots: c0=0 (valid). */
        {
            int c0v[23] = {2000, -1500, 750};
            int k;
            for (k = 0; k < 23; k++) {
                setbitu(m, i, 3, 3); /* iodcorr=3 */
                i += 3;
                setbits(m, i, 15, c0v[k]);
                i += 15;
            }
        }
        finish_msg(m);
        ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == 10, "1: MT4 returns 10 (clock updated)");
        CHECK(fabs(s5->dclk[0] - (-2000 * 0.0016)) < 1e-9, "1: C05 dclk[0]=-c0*0.0016");
        CHECK(s5->iod[1] == 3, "1: C05 iod[1]=iodcorr");
        CHECK(fabs(s5->udi[1] - 12.0) < 1e-9, "1: C05 udi[1]=12.0");
        CHECK(s5->t0[1].time != 0, "1: C05 clock t0[1] now set");
        CHECK(s5->iod[0] == s5->iod[1], "1: C05 iod[0]==iod[1] (gate passes)");
        CHECK(fabs(s9->dclk[0] - (-(-1500) * 0.0016)) < 1e-9, "1: C09 dclk[0]=-c0*0.0016 (neg c0)");
        CHECK(b2b->ssr[gps3 - 1].iod[1] == 3, "1: G03 clock iod[1] stored");

        /* --- MT3: BDS code biases for C05 (B1I=mode0, B3I=mode12) --------- */
        i = begin_msg(m, MT_CBIAS);
        i = put_head(m, i, tod, /*iodssr*/ 1);
        setbitu(m, i, 5, /*nsat*/ 1);
        i += 5;
        setbitu(m, i, 9, /*slot*/ 5); /* C05 */
        i += 9;
        setbitu(m, i, 4, /*ncb*/ 2); /* two code biases */
        i += 4;
        setbitu(m, i, 4, 0); /* mode 0 -> CODE_L2I (B1I) */
        i += 4;
        setbits(m, i, 12, 100); /* dcb=100 -> -100*0.017 */
        i += 12;
        setbitu(m, i, 4, 12); /* mode 12 -> CODE_L6I (B3I) */
        i += 4;
        setbits(m, i, 12, -50);
        i += 12;
        finish_msg(m);
        ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == 10, "1: MT3 returns 10 (cbias updated)");
        CHECK(fabs(s5->cbias[CODE_L2I - 1] - (float)(-100 * 0.017)) < 1e-5, "1: C05 cbias L2I=-dcb*0.017");
        CHECK(s5->vcbias[CODE_L2I - 1] == 1, "1: C05 vcbias L2I set");
        CHECK(fabs(s5->cbias[CODE_L6I - 1] - (float)(-(-50) * 0.017)) < 1e-5, "1: C05 cbias L6I (neg dcb)");
        CHECK(fabs(s5->udi[4] - 86400.0) < 1e-9, "1: C05 udi[4]=86400.0");
        (void)t0;
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 2. CRC corruption: one flipped bit -> -1, no state change             *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];
        int i = begin_msg(m, MT_MASK);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 4, 2);
        i += 4;
        setbitu(m, i + 4, 1, 1); /* slot 5 */
        finish_msg(m);
        /* flip one data bit AFTER the CRC was computed -> CRC mismatch. */
        m[10] ^= 0x01;
        int ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == -1, "2: CRC error returns -1");
        CHECK(b2b->mask.valid == 0, "2: mask not populated on CRC error");
        CHECK(b2b->iodssr == -1, "2: iodssr untouched on CRC error");
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 3. MT63 null message -> 0, no state change                            *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];
        begin_msg(m, MT_NULL);
        /* no CRC needed: null is rejected before the CRC check. */
        int ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == 0, "3: MT63 null returns 0");
        CHECK(b2b->mask.valid == 0, "3: no mask state after null");
        CHECK(b2b->iodssr == -1, "3: iodssr untouched after null");
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 4. IOD SSR mismatch: MT2 iodssr != MT1 -> 0, no orbit stored          *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];
        int i = begin_msg(m, MT_MASK);
        i = put_head(m, i, tod, /*iodssr*/ 1);
        setbitu(m, i, 4, 2);
        i += 4;
        setbitu(m, i + 4, 1, 1); /* slot 5 */
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);

        i = begin_msg(m, MT_ORBIT);
        i = put_head(m, i, tod, /*iodssr*/ 2); /* mismatched */
        i = put_orb(m, i, 5, 0x100, 3, 1000, 0, 0, 0, 0);
        finish_msg(m);
        int ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == 0, "4: MT2 with wrong iodssr returns 0");
        CHECK(b2b->ssr[bds5 - 1].t0[0].time == 0, "4: no orbit stored on iodssr mismatch");
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 5. IODP mismatch: MT4 iodp != MT1 -> 0, no clock stored               *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];
        int i = begin_msg(m, MT_MASK);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 4, /*iodp*/ 2);
        i += 4;
        setbitu(m, i + 4, 1, 1);
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);

        i = begin_msg(m, MT_CLOCK);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 4, /*iodp*/ 7); /* mismatched */
        i += 4;
        setbitu(m, i, 5, 0);
        i += 5;
        {
            int k;
            for (k = 0; k < 23; k++) {
                setbitu(m, i, 3, 3);
                i += 3;
                setbits(m, i, 15, 1000);
                i += 15;
            }
        }
        finish_msg(m);
        int ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == 0, "5: MT4 with wrong iodp returns 0");
        CHECK(b2b->ssr[bds5 - 1].t0[1].time == 0, "5: no clock stored on iodp mismatch");
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 6. invalid sentinels clear previously-set state                       *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];

        /* MT1 mask C05. */
        int i = begin_msg(m, MT_MASK);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 4, 2);
        i += 4;
        setbitu(m, i + 4, 1, 1);
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);

        /* MT2 valid orbit, then MT4 valid clock, to set t0[0] and t0[1]. */
        i = begin_msg(m, MT_ORBIT);
        i = put_head(m, i, tod, 1);
        i = put_orb(m, i, 5, 0x100, 3, 1000, 100, 100, 0, 0);
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        i = begin_msg(m, MT_CLOCK);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 4, 2);
        i += 4;
        setbitu(m, i, 5, 0);
        i += 5;
        {
            int k;
            for (k = 0; k < 23; k++) {
                setbitu(m, i, 3, 3);
                i += 3;
                setbits(m, i, 15, k == 0 ? 500 : 0);
                i += 15;
            }
        }
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(b2b->ssr[bds5 - 1].t0[0].time != 0, "6: orbit t0[0] set before clear");
        CHECK(b2b->ssr[bds5 - 1].t0[1].time != 0, "6: clock t0[1] set before clear");

        /* MT2 with radial = -16384 (= -2^14) invalid sentinel -> clears t0[0]. */
        i = begin_msg(m, MT_ORBIT);
        i = put_head(m, i, tod, 1);
        i = put_orb(m, i, 5, 0x100, 3, /*dr*/ -16384, 0, 0, 0, 0);
        finish_msg(m);
        int ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == 10, "6: MT2 sentinel still returns 10");
        CHECK(b2b->ssr[bds5 - 1].t0[0].time == 0, "6: radial=-16384 clears orbit t0[0]");

        /* MT4 with c0 = -16383 (= -(2^14 - 1)) invalid sentinel -> clears t0[1]. */
        i = begin_msg(m, MT_CLOCK);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 4, 2);
        i += 4;
        setbitu(m, i, 5, 0);
        i += 5;
        {
            int k;
            for (k = 0; k < 23; k++) {
                setbitu(m, i, 3, 3);
                i += 3;
                setbits(m, i, 15, k == 0 ? -16383 : 0);
                i += 15;
            }
        }
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(b2b->ssr[bds5 - 1].t0[1].time == 0, "6: c0=-16383 clears clock t0[1]");
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 7. clock before mask: MT4 with no MT1 -> 0, nothing stored            *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];
        int i = begin_msg(m, MT_CLOCK);
        i = put_head(m, i, tod, /*iodssr*/ 0);
        setbitu(m, i, 4, 0);
        i += 4;
        setbitu(m, i, 5, 0);
        i += 5;
        {
            int k;
            for (k = 0; k < 23; k++) {
                setbitu(m, i, 3, 3);
                i += 3;
                setbits(m, i, 15, 1000);
                i += 15;
            }
        }
        finish_msg(m);
        int ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == 0, "7: MT4 before any MT1 returns 0");
        CHECK(b2b->ssr[bds5 - 1].t0[1].time == 0, "7: no clock stored without mask");
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 8. negative values decode to correct (sign-reversed) doubles          *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];
        int i = begin_msg(m, MT_MASK);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 4, 2);
        i += 4;
        setbitu(m, i + 4, 1, 1); /* slot 5 */
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);

        i = begin_msg(m, MT_ORBIT);
        i = put_head(m, i, tod, 1);
        i = put_orb(m, i, 5, 0x100, 3, /*dr*/ -1234, /*da*/ -777, /*dc*/ -321, 0, 0);
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        ssr_t* s = &b2b->ssr[bds5 - 1];
        CHECK(fabs(s->deph[0] - (-1234 * 0.0016)) < 1e-9, "8: deph[0] negative radial");
        CHECK(fabs(s->deph[1] - (-777 * 0.0064)) < 1e-9, "8: deph[1] negative along");
        CHECK(fabs(s->deph[2] - (-321 * 0.0064)) < 1e-9, "8: deph[2] negative cross");
        CHECK(s->deph[0] < 0 && s->deph[1] < 0 && s->deph[2] < 0, "8: all deph negative");

        i = begin_msg(m, MT_CLOCK);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 4, 2);
        i += 4;
        setbitu(m, i, 5, 0);
        i += 5;
        {
            int k;
            for (k = 0; k < 23; k++) {
                setbitu(m, i, 3, 3);
                i += 3;
                setbits(m, i, 15, k == 0 ? -2222 : 0);
                i += 15;
            }
        }
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        /* dclk = -c0*0.0016; c0 negative -> dclk positive (sign reversal). */
        CHECK(fabs(s->dclk[0] - (-(-2222) * 0.0016)) < 1e-9, "8: dclk negative c0 -> positive");
        CHECK(s->dclk[0] > 0, "8: dclk sign reversed");

        /* negative DCB via MT3. */
        i = begin_msg(m, MT_CBIAS);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 5, 1);
        i += 5;
        setbitu(m, i, 9, 5);
        i += 9;
        setbitu(m, i, 4, 1); /* ncb=1 */
        i += 4;
        setbitu(m, i, 4, 0); /* mode 0 -> CODE_L2I */
        i += 4;
        setbits(m, i, 12, -333);
        i += 12;
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(fabs(s->cbias[CODE_L2I - 1] - (float)(-(-333) * 0.017)) < 1e-5, "8: cbias negative dcb -> positive");
        CHECK(s->cbias[CODE_L2I - 1] > 0, "8: cbias sign reversed");
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 9. b2b_sel_biascode exact + fallback + unmapped, all four systems      *
     * ===================================================================== */
    {
        /* BDS: B1I family folds to L2I; B1C(P) family folds to L1P; B3I folds. */
        CHECK(b2b_sel_biascode(SYS_CMP, CODE_L2I) == CODE_L2I, "9: BDS L2I exact");
        CHECK(b2b_sel_biascode(SYS_CMP, CODE_L2Q) == CODE_L2I, "9: BDS L2Q->L2I fallback");
        CHECK(b2b_sel_biascode(SYS_CMP, CODE_L1X) == CODE_L1P, "9: BDS L1X->L1P fallback");
        CHECK(b2b_sel_biascode(SYS_CMP, CODE_L6X) == CODE_L6I, "9: BDS L6X->L6I fallback");
        CHECK(b2b_sel_biascode(SYS_CMP, CODE_L7Z) == CODE_L7P, "9: BDS L7Z->L7P fallback");

        /* GPS: L1C exact; L1W folds to L1P; L2X exact; L2W has no B2b DCB. */
        CHECK(b2b_sel_biascode(SYS_GPS, CODE_L1C) == CODE_L1C, "9: GPS L1C exact");
        CHECK(b2b_sel_biascode(SYS_GPS, CODE_L1W) == CODE_L1P, "9: GPS L1W->L1P fallback");
        CHECK(b2b_sel_biascode(SYS_GPS, CODE_L2X) == CODE_L2X, "9: GPS L2X exact");
        CHECK(b2b_sel_biascode(SYS_GPS, CODE_L2W) == CODE_NONE, "9: GPS L2W unmapped -> CODE_NONE");

        /* GAL: E1-B exact; E1-C family; E5a-Q family; E6 folds to L6C. */
        CHECK(b2b_sel_biascode(SYS_GAL, CODE_L1B) == CODE_L1B, "9: GAL L1B exact");
        CHECK(b2b_sel_biascode(SYS_GAL, CODE_L1X) == CODE_L1C, "9: GAL L1X->L1C fallback");
        CHECK(b2b_sel_biascode(SYS_GAL, CODE_L5X) == CODE_L5Q, "9: GAL L5X->L5Q fallback");
        CHECK(b2b_sel_biascode(SYS_GAL, CODE_L6X) == CODE_L6C, "9: GAL L6X->L6C fallback");
        CHECK(b2b_sel_biascode(SYS_GAL, CODE_L8X) == CODE_NONE, "9: GAL L8X unmapped -> CODE_NONE");

        /* GLO: G1 C/A, G1 P, G2 C/A only. */
        CHECK(b2b_sel_biascode(SYS_GLO, CODE_L1C) == CODE_L1C, "9: GLO L1C exact");
        CHECK(b2b_sel_biascode(SYS_GLO, CODE_L2C) == CODE_L2C, "9: GLO L2C exact");
        CHECK(b2b_sel_biascode(SYS_GLO, CODE_L2P) == CODE_NONE, "9: GLO L2P unmapped -> CODE_NONE");
    }

    /* ===================================================================== *
     * 10. out-of-range orbit slot (200) is skipped, cursor stays aligned     *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];
        int i = begin_msg(m, MT_MASK);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 4, 2);
        i += 4;
        setbitu(m, i + 8, 1, 1); /* slot 9 (BDS C09) */
        finish_msg(m);
        b2b_input_msg(b2b, GEO_PRN, m, time);

        i = begin_msg(m, MT_ORBIT);
        i = put_head(m, i, tod, 1);
        /* first block: out-of-range slot 200 (b2b_slot2sat -> 0): skipped. */
        i = put_orb(m, i, /*slot*/ 200, 0x100, 3, 4242, 0, 0, 0, 0);
        /* second block: valid C09; must still decode after the skipped one. */
        i = put_orb(m, i, /*slot*/ 9, 0x055, 3, /*dr*/ 1600, 0, 0, 0, 0);
        finish_msg(m);
        int ret = b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(ret == 10, "10: MT2 with out-of-range slot still returns 10");
        CHECK(b2b->ssr[bds9 - 1].t0[0].time != 0, "10: valid block after skipped slot decoded");
        CHECK(fabs(b2b->ssr[bds9 - 1].deph[0] - 1600 * 0.0016) < 1e-9, "10: cursor aligned -> C09 deph correct");
        CHECK(b2b->ssr[bds9 - 1].iode == 0x55, "10: C09 iode correct (cursor aligned)");
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 11. GEO gate: only set_bdsgeo() PRNs feed the decoder                  *
     * ===================================================================== */
    {
        uint8_t m[B2B_MSG_BYTES];
        int i = begin_msg(m, MT_MASK);
        i = put_head(m, i, tod, 1);
        setbitu(m, i, 4, 2);
        i += 4;
        setbitu(m, i + 8, 1, 1); /* slot 9 (BDS C09) */
        finish_msg(m);
        uint8_t o[B2B_MSG_BYTES];
        i = begin_msg(o, MT_ORBIT);
        i = put_head(o, i, tod, 1);
        i = put_orb(o, i, /*slot*/ 9, 0x055, 3, 1600, 0, 0, 0, 0);
        finish_msg(o);

        CHECK(set_bdsgeo(NULL) == 4 && is_bdsgeo(1) && is_bdsgeo(4) && !is_bdsgeo(5) && !is_bdsgeo(59),
              "11: default GEO list is C01-C04");
        b2b_t* b2b = b2b_new();
        b2b_input_msg(b2b, 33, m, time); /* MEO C33: B-CNAV3, must be ignored */
        CHECK(b2b_input_msg(b2b, 33, o, time) == 0 && b2b->ssr[bds9 - 1].t0[0].time == 0,
              "11: non-GEO C33 frames ignored");
        CHECK(b2b_input_msg(b2b, 60, o, time) == 0 && b2b->ssr[bds9 - 1].t0[0].time == 0,
              "11: C60 ignored with the default list");
        b2b_input_msg(b2b, 4, m, time);
        CHECK(b2b_input_msg(b2b, 4, o, time) == 10, "11: GEO C04 decodes");
        b2b_free(b2b);

        CHECK(set_bdsgeo("C59-C62") == 4 && is_bdsgeo(59) && is_bdsgeo(62) && !is_bdsgeo(1),
              "11: C59-C62 list (pre-reassignment)");
        b2b = b2b_new();
        b2b_input_msg(b2b, 60, m, time);
        CHECK(b2b_input_msg(b2b, 60, o, time) == 10, "11: C60 decodes when listed");
        b2b_free(b2b);
        CHECK(set_bdsgeo("C59-C99") == -1 && is_bdsgeo(1) && !is_bdsgeo(59), "11: invalid list falls back to C01-C04");
        set_bdsgeo(NULL);
    }

    /* ===================================================================== *
     * 12. SBF BDSCNav1 (4251) B-CNAV1 ephemeris                              *
     * ===================================================================== */
    {
        eph_t e0, e1;
        int ephset = -1, week, sat28 = satno(SYS_CMP, 28);
        double tow, sqrta = 5.282623783970e+03;

        setseleph(SYS_CMP, 0);
        CHECK(decode_cnav1_block(&e0, &e1, &ephset) == 0 && e1.sat == 0,
              "12: BDSCNav1 ignored with the D1/D2 selection");

        setseleph(SYS_CMP, 1);
        int ret = decode_cnav1_block(&e0, &e1, &ephset);
        CHECK(ret == 2 && ephset == 1 && e1.sat == sat28 && e0.sat == 0,
              "12: BDSCNav1 -> ephemeris set 1 of C28 (set 0 untouched)");
        CHECK(e1.type == 2 && e1.iode == 0 && e1.iodc == 0, "12: type CNAV-1, IODE 0 / IODC 0");
        /* BRD400DLR CNV1 C28 2026 10 23 20 00 00 (RINEX prints 13 digits) */
        CHECK(fabs(e1.f0 - 5.719051696360e-04) < 1e-15 && fabs(e1.f1 - 3.710809437507e-12) < 1e-23, "12: a0 / a1");
        CHECK(fabs(e1.Adot - 4.298686981201e-03) < 1e-14 && fabs(e1.crs - (-6.514062500000e+01)) < 1e-9,
              "12: A_DOT / C_rs");
        CHECK(fabs(e1.deln - 4.475722146078e-09) < 1e-20 && fabs(e1.M0 - (-2.346129760099)) < 1e-11,
              "12: DELTA_n0 / M_0 (semi-circles -> rad)");
        CHECK(fabs(e1.e - 2.496616798453e-04) < 1e-15 && fabs(e1.A - sqrta * sqrta) < 1e-3, "12: e / A");
        CHECK(fabs(e1.i0 - 9.456167605280e-01) < 1e-12 && fabs(e1.OMG0 - (-1.547795335431)) < 1e-11 &&
                  fabs(e1.omg - (-1.520131813489)) < 1e-11,
              "12: i_0 / OMEGA_0 / omega");
        CHECK(fabs(e1.ndot - (-2.833893154632e-14)) < 1e-25 && fabs(e1.idot - (-4.496615873529e-10)) < 1e-21,
              "12: DELTA_n0_DOT / IDOT");
        CHECK(fabs(e1.tgd[2] - (-2.561137080193e-09)) < 1e-20 && fabs(e1.tgd[3] - (-1.012813299894e-08)) < 1e-19 &&
                  fabs(e1.tgd[4] - (-9.895302355289e-10)) < 1e-20,
              "12: T_GDB1Cp / T_GDB2ap / ISC_B1Cd");
        tow = time2gpst(e1.toe, &week);
        CHECK(e1.toes == 504000.0 && week == 2441 && fabs(tow - 504014.0) < 1e-6, "12: t_oe 504000 BDT = GPST 504014");
        CHECK(fabs(timediff(e1.toc, e1.toe)) < 1e-6, "12: t_oc = t_oe");
        setseleph(SYS_CMP, 0);
    }

    /* ===================================================================== *
     * 13. IOD SSR change clears the staged corrections                       *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];
        ssr_t* s9 = &b2b->ssr[bds9 - 1];

        make_mask_c09(m, tod, 1, 2);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        make_orbit_c09(m, tod, 1);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        make_clock_c09(m, tod, 1, 2);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        make_cbias_c09(m, tod, 1, 100);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(s9->t0[0].time && s9->t0[1].time && s9->vcbias[CODE_L2I - 1] && s9->vendor == SSR_VENDOR_B2B,
              "13: C09 orbit/clock/bias staged under IOD SSR 1, vendor PPP-B2b");

        s9->update = 0;
        make_mask_c09(m, tod, 1, 2); /* same IOD SSR: nothing cleared */
        CHECK(b2b_input_msg(b2b, GEO_PRN, m, time) == 0, "13: repeated MT1 returns 0");
        CHECK(s9->t0[0].time && s9->t0[1].time && !s9->update, "13: repeated MT1 keeps the corrections");

        make_mask_c09(m, tod, 2, 2); /* IOD SSR 1 -> 2 */
        CHECK(b2b_input_msg(b2b, GEO_PRN, m, time) == 10, "13: MT1 with a new IOD SSR returns 10 (publish the clear)");
        CHECK(!s9->t0[0].time && !s9->t0[1].time && !s9->t0[4].time && !s9->vcbias[CODE_L2I - 1] &&
                  s9->cbias[CODE_L2I - 1] == 0.0f,
              "13: IOD SSR change clears orbit, clock and code bias");
        CHECK(s9->update && s9->iod[0] == s9->iod[1], "13: cleared entry is flagged for the copy (iod[0] == iod[1])");

        make_orbit_c09(m, tod, 2); /* new orbit, IOD Corr 3 as the old clock */
        b2b_input_msg(b2b, GEO_PRN, m, time);
        CHECK(s9->t0[0].time && !s9->t0[1].time, "13: new orbit does not pair with the old clock");
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 14. withdrawn code bias                                               *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];
        ssr_t* s9 = &b2b->ssr[bds9 - 1];
        int dnu[2] = {-2048, -2047}, k;

        make_mask_c09(m, tod, 1, 2);
        b2b_input_msg(b2b, GEO_PRN, m, time);
        for (k = 0; k < 2; k++) {
            make_cbias_c09(m, tod, 1, 100);
            b2b_input_msg(b2b, GEO_PRN, m, time);
            s9->update = 0;
            make_cbias_c09(m, tod, 1, dnu[k]);
            int ret = b2b_input_msg(b2b, GEO_PRN, m, time);
            CHECK(ret == 10 && s9->update && !s9->vcbias[CODE_L2I - 1] && s9->cbias[CODE_L2I - 1] == 0.0f,
                  k ? "14: DCB -2047 withdraws the C09 B1I bias" : "14: DCB -2048 withdraws the C09 B1I bias");
        }
        b2b_free(b2b);
    }

    /* ===================================================================== *
     * 15. GEO source lock                                                   *
     * ===================================================================== */
    {
        b2b_t* b2b = b2b_new();
        uint8_t m[B2B_MSG_BYTES];
        ssr_t* s9 = &b2b->ssr[bds9 - 1];
        gtime_t t5 = timeadd(time, 5.0), t20 = timeadd(time, 20.0);

        make_orbit_c09(m, tod, 1);
        b2b_input_msg(b2b, 1, m, time);
        CHECK(b2b->src_prn == 0, "15: no source before a mask");
        make_mask_c09(m, tod, 1, 2);
        b2b_input_msg(b2b, 1, m, time);
        CHECK(b2b->src_prn == 1 && b2b->iodssr == 1, "15: C01 taken on its mask");

        make_mask_c09(m, tod, 2, 2); /* C04 stream, IOD SSR 2 */
        b2b_input_msg(b2b, 4, m, t5);
        make_clock_c09(m, tod, 2, 2);
        b2b_input_msg(b2b, 4, m, t5);
        CHECK(b2b->src_prn == 1 && b2b->iodssr == 1 && !s9->t0[1].time, "15: interleaved C04 frames ignored");

        make_orbit_c09(m, tod, 1);
        CHECK(b2b_input_msg(b2b, 1, m, t5) == 10 && s9->t0[0].time, "15: C01 orbit still decoded");

        make_clock_c09(m, tod, 2, 2);
        CHECK(b2b_input_msg(b2b, 4, m, t20) == 0 && b2b->src_prn == 1, "15: C01 silent 15 s: no switch on a clock");
        make_mask_c09(m, tod, 2, 2);
        b2b_input_msg(b2b, 4, m, t20);
        CHECK(b2b->src_prn == 4 && b2b->iodssr == 2 && !s9->t0[0].time,
              "15: C04 taken on its mask; IOD SSR 1 -> 2 clears the C01 orbit");
        b2b_free(b2b);
    }

    printf("\n%s: %d failure(s)\n", fails ? "FAILED" : "PASSED", fails);
    return fails ? 1 : 0;
}

static void make_mask_c09(uint8_t* m, int tod, int iodssr, int iodp) {
    int i = begin_msg(m, MT_MASK);
    i = put_head(m, i, tod, iodssr);
    setbitu(m, i, 4, (uint32_t)iodp);
    i += 4;
    setbitu(m, i + 8, 1, 1); /* slot 9 (BDS C09) */
    finish_msg(m);
}

static void make_orbit_c09(uint8_t* m, int tod, int iodssr) {
    int i = begin_msg(m, MT_ORBIT);
    i = put_head(m, i, tod, iodssr);
    put_orb(m, i, /*slot*/ 9, 0x055, /*iodcorr*/ 3, 1600, 0, 0, 0, 0);
    finish_msg(m);
}

static void make_clock_c09(uint8_t* m, int tod, int iodssr, int iodp) {
    int i = begin_msg(m, MT_CLOCK), k;
    i = put_head(m, i, tod, iodssr);
    setbitu(m, i, 4, (uint32_t)iodp);
    i += 4;
    setbitu(m, i, 5, 0); /* subtype 0 */
    i += 5;
    for (k = 0; k < 23; k++) {
        setbitu(m, i, 3, 3); /* iodcorr 3 */
        i += 3;
        setbits(m, i, 15, k == 0 ? -1500 : 0);
        i += 15;
    }
    finish_msg(m);
}

static void make_cbias_c09(uint8_t* m, int tod, int iodssr, int dcb) {
    int i = begin_msg(m, MT_CBIAS);
    i = put_head(m, i, tod, iodssr);
    setbitu(m, i, 5, 1); /* nsat */
    i += 5;
    setbitu(m, i, 9, 9); /* slot 9 (C09) */
    i += 9;
    setbitu(m, i, 4, 1); /* ncb */
    i += 4;
    setbitu(m, i, 4, 0); /* mode 0 -> CODE_L2I (B1I) */
    i += 4;
    setbits(m, i, 12, dcb);
    finish_msg(m);
}
