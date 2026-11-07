/*------------------------------------------------------------------------------
 * unit test : SouthPAN PVS (DFMC L5 MT32) decoder and orbit/clock application
 *
 * Drives sbsupdatepvs() with synthetic L5 messages (226 bits without CRC:
 * 4-bit preamble, 6-bit type, MT32 fields as cssrlib sbas.py) and checks
 * satpos(EPHOPT_SSRAPC) against the plain broadcast position of a synthetic
 * GPS ephemeris.
 *
 * Coverage:
 *   1. GPS MT32 with odd tD/16: scales and signs (negative fields too), IODN
 *      -> IODE (8 LSBs), reception time as t0, vendor, iod[0] == iod[1].
 *   2. even tD/16 (SBAS only), slot 0, GLONASS / BDS slots: not used.
 *   3. Galileo slot: full 10-bit IODnav.
 *   4. PRN other than 122 (other DFMC GEOs send MT32 too) and L1 frames: not
 *      used; sbsupdatecorr() ignores L5 frames.
 *   5. .sbs round trip: sbsoutmsg() writes the L5 type, the reader sets the band
 *      from the type column, an L1 frame stays L1; an L5 MT0 whose two type
 *      fields both read 0 is L5 (no L1 preamble), an L1 MT0 stays L1.
 *   6. satpos: rs = broadcast + ECEF correction, dts = broadcast + dB/c - TGD;
 *      rejected 100 s after reception and on an IODE mismatch.
 *   7. resolve_correction(): PVS accepts only GPS L1/L5 and Galileo E1/E5a,
 *      from [signals] (pppsig) or the explicit signal list (sigcfg).
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_sbas.h"
#include "mrtklib/mrtk_time.h"

extern int satno(int sys, int prn);

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

#define WEEK 2380
#define TOW 370845

/* MT32 field values in LSBs */
typedef struct {
    int slot, iodn, dx, dy, dz, db, dxd, dyd, dzd, dbd, td16;
} mt32_t;

static sbsmsg_t make_mt32(const mt32_t* f, int prn, int band) {
    sbsmsg_t m;
    int i = 0;

    memset(&m, 0, sizeof(m));
    m.week = WEEK;
    m.tow = TOW;
    m.prn = (uint8_t)prn;
    m.band = (uint8_t)band;
    setbitu(m.msg, i, 4, 0xA);
    i += 4;
    setbitu(m.msg, i, 6, 32);
    i += 6;
    setbitu(m.msg, i, 8, f->slot);
    i += 8;
    setbitu(m.msg, i, 10, f->iodn);
    i += 10;
    setbits(m.msg, i, 11, f->dx);
    setbits(m.msg, i + 11, 11, f->dy);
    setbits(m.msg, i + 22, 11, f->dz);
    setbits(m.msg, i + 33, 12, f->db);
    i += 45;
    setbits(m.msg, i, 8, f->dxd);
    setbits(m.msg, i + 8, 8, f->dyd);
    setbits(m.msg, i + 16, 8, f->dzd);
    setbits(m.msg, i + 24, 9, f->dbd);
    setbitu(m.msg, i + 33, 13, f->td16);
    return m;
}

static int near(double a, double b, double tol) { return fabs(a - b) <= tol; }

int main(void) {
    nav_t* nav = (nav_t*)calloc(1, sizeof(nav_t));
    gtime_t trcv = gpst2time(WEEK, TOW);
    int g18 = satno(SYS_GPS, 18), e12 = satno(SYS_GAL, 12);
    const ssr_t* ssr;
    sbsmsg_t m;

    if (!nav) {
        return 1;
    }
    CHECK(sizeof(sbsmsg_t) == 40, "sbsmsg_t size unchanged by the band field");

    /* ===================================================================== *
     * 1. GPS MT32, odd tD/16                                                 *
     * ===================================================================== */
    {
        mt32_t f = {18, 0x31D, 19, -21, 27, 65, -3, 1, 2, -5, 1577};
        m = make_mt32(&f, 122, SBS_BAND_L5);
        CHECK(sbsmsgtype(&m) == 32, "1: L5 type at bit 4");
        CHECK(sbsupdatepvs(&m, nav) == g18, "1: G18 updated");
        ssr = nav->ssr_ch[0] + g18 - 1;
        CHECK(near(ssr->deph[0], 19 * 0.0625, 1e-12) && near(ssr->deph[1], -21 * 0.0625, 1e-12) &&
                  near(ssr->deph[2], 27 * 0.0625, 1e-12),
              "1: dx/dy/dz 0.0625 m (ECEF, as broadcast)");
        CHECK(near(ssr->dclk[0], 65 * 0.03125, 1e-12), "1: dB 0.03125 m");
        CHECK(near(ssr->ddeph[0], -3 / 2048.0, 1e-15) && near(ssr->ddeph[1], 1 / 2048.0, 1e-15) &&
                  near(ssr->ddeph[2], 2 / 2048.0, 1e-15),
              "1: rates 2^-11 m/s");
        CHECK(near(ssr->dclk[1], -5 / 4096.0, 1e-15), "1: dB rate 2^-12 m/s");
        CHECK(ssr->iode == (0x31D & 0xFF), "1: GPS IODN (IODC) -> IODE");
        CHECK(timediff(ssr->t0[0], trcv) == 0.0 && timediff(ssr->t0[1], trcv) == 0.0, "1: t0 = reception");
        CHECK(ssr->udi[0] == 0.0 && ssr->udi[1] == 0.0, "1: udi 0");
        CHECK(ssr->iod[0] == ssr->iod[1], "1: iod[0] == iod[1]");
        CHECK(ssr->vendor == SSR_VENDOR_PVS && ssr->update == 1, "1: vendor PVS, update");
    }

    /* ===================================================================== *
     * 2. messages not for PVS                                                *
     * ===================================================================== */
    {
        mt32_t f = {18, 29, 1, 1, 1, 1, 0, 0, 0, 0, 1578};
        double dx = nav->ssr_ch[0][g18 - 1].deph[0];
        m = make_mt32(&f, 122, SBS_BAND_L5);
        CHECK(sbsupdatepvs(&m, nav) == 0 && nav->ssr_ch[0][g18 - 1].deph[0] == dx, "2: even tD/16 not used");
        f.td16 = 1579;
        f.slot = 0;
        m = make_mt32(&f, 122, SBS_BAND_L5);
        CHECK(sbsupdatepvs(&m, nav) == 0, "2: slot 0 not used");
        f.slot = 40; /* GLONASS */
        m = make_mt32(&f, 122, SBS_BAND_L5);
        CHECK(sbsupdatepvs(&m, nav) == 0, "2: GLONASS slot not used");
        f.slot = 170; /* BDS */
        m = make_mt32(&f, 122, SBS_BAND_L5);
        CHECK(sbsupdatepvs(&m, nav) == 0, "2: BDS slot not used");
    }

    /* ===================================================================== *
     * 3. Galileo slot                                                        *
     * ===================================================================== */
    {
        mt32_t f = {86, 0x2A5, -1, 0, 0, -2, 0, 0, 0, 0, 1};
        m = make_mt32(&f, 122, SBS_BAND_L5);
        CHECK(sbsupdatepvs(&m, nav) == e12, "3: slot 86 -> E12");
        ssr = nav->ssr_ch[0] + e12 - 1;
        CHECK(ssr->iode == 0x2A5, "3: Galileo IODnav 10 bits");
        CHECK(near(ssr->deph[0], -0.0625, 1e-12) && near(ssr->dclk[0], -0.0625, 1e-12), "3: negative values");
    }

    /* ===================================================================== *
     * 4. other GEOs and L1 frames                                            *
     * ===================================================================== */
    {
        mt32_t f = {18, 29, 1, 1, 1, 1, 0, 0, 0, 0, 1579};
        double dx = nav->ssr_ch[0][g18 - 1].deph[0];
        m = make_mt32(&f, 130, SBS_BAND_L5);
        CHECK(sbsupdatepvs(&m, nav) == 0 && nav->ssr_ch[0][g18 - 1].deph[0] == dx, "4: PRN 130 MT32 not used");
        m = make_mt32(&f, 122, SBS_BAND_L1);
        CHECK(sbsupdatepvs(&m, nav) == 0, "4: L1 frame not used");
        m = make_mt32(&f, 122, SBS_BAND_L5);
        CHECK(sbsupdatecorr(&m, nav) == -1, "4: sbsupdatecorr ignores L5");
    }

    /* ===================================================================== *
     * 5. .sbs round trip                                                     *
     * ===================================================================== */
    {
        const char* file = "utest_pvs_tmp.sbs";
        mt32_t f = {18, 29, 1, 2, 3, 4, 0, 0, 0, 0, 1579};
        sbsmsg_t l1, l5mt0, l1mt0;
        sbs_t sbs = {0};
        FILE* fp = fopen(file, "w");

        memset(&l1, 0, sizeof(l1));
        l1.week = WEEK;
        l1.tow = TOW + 1;
        l1.prn = 122;
        setbitu(l1.msg, 0, 8, 0x53);
        setbitu(l1.msg, 8, 6, 25);
        /* L5 MT0 with bits 4-13 zero: both type fields read 0 */
        memset(&l5mt0, 0, sizeof(l5mt0));
        l5mt0.week = WEEK;
        l5mt0.tow = TOW + 2;
        l5mt0.prn = 122;
        l5mt0.band = SBS_BAND_L5;
        setbitu(l5mt0.msg, 0, 4, 0x3);
        setbitu(l5mt0.msg, 16, 8, 0xFF);
        l1mt0 = l1;
        l1mt0.tow = TOW + 3;
        setbitu(l1mt0.msg, 0, 8, 0x9A);
        setbitu(l1mt0.msg, 8, 6, 0);
        m = make_mt32(&f, 122, SBS_BAND_L5);
        CHECK(fp != NULL, "5: open temp file");
        if (fp) {
            sbsoutmsg(fp, &m);
            sbsoutmsg(fp, &l1);
            sbsoutmsg(fp, &l5mt0);
            sbsoutmsg(fp, &l1mt0);
            fclose(fp);
            fp = fopen(file, "r");
            char line[256] = "";
            CHECK(fp && fgets(line, sizeof(line), fp) && strstr(line, " 122 32 : A8"), "5: L5 type column 32");
            if (fp) {
                fclose(fp);
            }
            sbsreadmsg(file, 0, &sbs);
            CHECK(sbs.n == 4, "5: four messages read");
            if (sbs.n == 4) {
                CHECK(sbs.msgs[0].band == SBS_BAND_L5 && !memcmp(sbs.msgs[0].msg, m.msg, 29), "5: L5 frame + band");
                CHECK(sbs.msgs[1].band == SBS_BAND_L1 && sbsmsgtype(sbs.msgs + 1) == 25, "5: L1 frame stays L1");
                CHECK(sbs.msgs[2].band == SBS_BAND_L5 && sbsmsgtype(sbs.msgs + 2) == 0,
                      "5: L5 MT0 (bits 4-13 zero) is L5");
                CHECK(sbs.msgs[3].band == SBS_BAND_L1 && sbsmsgtype(sbs.msgs + 3) == 0, "5: L1 MT0 stays L1");
            }
            free(sbs.msgs);
            remove(file);
        }
    }

    /* ===================================================================== *
     * 6. satpos with the PVS correction                                      *
     * ===================================================================== */
    {
        eph_t eph;
        double rs0[6], dts0[2], rs1[6], dts1[2], var;
        int svh, ok;
        mt32_t f = {18, 0x11D, 16, -32, 8, 64, 0, 0, 0, 0, 1579};

        memset(&eph, 0, sizeof(eph));
        eph.sat = g18;
        eph.iode = 0x1D;
        eph.iodc = 0x11D;
        eph.A = 26560e3;
        eph.e = 0.01;
        eph.i0 = 0.96;
        eph.toes = TOW - 600.0;
        eph.week = WEEK;
        eph.toe = eph.toc = gpst2time(WEEK, eph.toes);
        eph.f0 = 1e-5;
        eph.tgd[0] = 5e-9;
        nav->eph = &eph;
        nav->n = nav->nmax = 1;

        m = make_mt32(&f, 122, SBS_BAND_L5);
        CHECK(sbsupdatepvs(&m, nav) == g18, "6: G18 updated");
        ok = satpos(trcv, trcv, g18, EPHOPT_BRDC, nav, rs0, dts0, &var, &svh);
        ok &= satpos(trcv, trcv, g18, EPHOPT_SSRAPC, nav, rs1, dts1, &var, &svh);
        CHECK(ok, "6: satpos brdc and ssrapc");
        CHECK(near(rs1[0] - rs0[0], 1.0, 1e-6) && near(rs1[1] - rs0[1], -2.0, 1e-6) && near(rs1[2] - rs0[2], 0.5, 1e-6),
              "6: rs = broadcast + ECEF correction");
        /* 1 mm: the two paths use different relativity formulas (~0.03 mm) */
        CHECK(near((dts1[0] - dts0[0]) * CLIGHT, 2.0 - 5e-9 * CLIGHT, 1e-3), "6: dts = broadcast + dB/c - TGD");
        CHECK(satpos(timeadd(trcv, 99.0), trcv, g18, EPHOPT_SSRAPC, nav, rs1, dts1, &var, &svh),
              "6: used 99 s after reception");
        CHECK(!satpos(timeadd(trcv, 101.0), trcv, g18, EPHOPT_SSRAPC, nav, rs1, dts1, &var, &svh),
              "6: rejected 101 s after reception");
        eph.iode = 0x1E;
        CHECK(!satpos(trcv, trcv, g18, EPHOPT_SSRAPC, nav, rs1, dts1, &var, &svh), "6: rejected on IODE mismatch");
        nav->eph = NULL;
        nav->n = nav->nmax = 0;
    }

    /* ===================================================================== *
     * 7. resolve_correction: PVS signal pairs                                *
     * ===================================================================== */
    {
        prcopt_t opt = prcopt_default;
        char msg[256];

        opt.mode = PMODE_PPP_KINEMA;
        opt.correction = CORR_SOUTHPAN_PVS;
        opt.sateph = EPHOPT_SSRAPC;
        opt.navsys = SYS_GPS | SYS_GAL;
        opt.modear = ARMODE_OFF;
        opt.pppsig[0] = 1; /* GPS L1/L5 */
        opt.pppsig[2] = 0; /* Galileo E1/E5a */
        CHECK(resolve_correction(&opt, msg, sizeof(msg)), "7: L1/L5 + E1/E5a accepted");
        opt.pppsig[0] = 0;
        CHECK(!resolve_correction(&opt, msg, sizeof(msg)), "7: GPS L1/L2 rejected");
        opt.navsys = SYS_GAL;
        CHECK(resolve_correction(&opt, msg, sizeof(msg)), "7: GPS pair ignored without GPS");
        opt.navsys = SYS_GPS | SYS_GAL;
        opt.pppsig[0] = 1;
        opt.pppsig[2] = 1;
        CHECK(!resolve_correction(&opt, msg, sizeof(msg)), "7: Galileo E1/E5b rejected");
        opt.pppsig[2] = 0;
        opt.sigcfg_set = 1;
        opt.sigcfg[0].nsig = 2;
        opt.sigcfg[0].sig[0].band = MRTK_BAND_GPS_L1;
        opt.sigcfg[0].sig[1].band = MRTK_BAND_GPS_L2;
        CHECK(!resolve_correction(&opt, msg, sizeof(msg)), "7: signals GPS L1+L2 rejected");
        opt.sigcfg[0].sig[1].band = MRTK_BAND_GPS_L5;
        CHECK(resolve_correction(&opt, msg, sizeof(msg)), "7: signals GPS L1+L5 accepted");
        /* the broadcast orbit refers to the antenna phase centre */
        opt.sateph = EPHOPT_SSRCOM;
        CHECK(!resolve_correction(&opt, msg, sizeof(msg)), "7: brdc+ssrcom rejected");
        opt.sateph = EPHOPT_SSRAPC;
        opt.nf = 1;
        CHECK(!resolve_correction(&opt, msg, sizeof(msg)), "7: single frequency rejected");
        opt.nf = 2;
        CHECK(resolve_correction(&opt, msg, sizeof(msg)), "7: two frequencies accepted");
    }

    free(nav);
    printf("%s: %d failure(s)\n", fails ? "FAILED" : "PASSED", fails);
    return fails ? 1 : 0;
}
