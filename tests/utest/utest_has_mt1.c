/*------------------------------------------------------------------------------
 * unit test : Galileo HAS MT1 decoder hardening (mrtk_has.c)
 *
 * Negative / safety tests for the MT1 block parsers, driven entirely through the
 * public has_input_page() API with synthetic recovered messages. No real signal
 * data is needed: the RS(255,32,224) erasure decoder is systematic, so feeding
 * MS pages carrying PIDs 1..MS reproduces the message pages verbatim (rows 0..MS-1
 * of the generator matrix are the identity I32). Each "page" we hand to
 * has_input_page() is a 56-byte buffer: a 24-bit HAS page header followed by the
 * 53-byte (424-bit) message-page payload.
 *
 * Coverage:
 *   (A) a mask block that claims more bits than the recovered buffer holds is
 *       rejected (heap over-read guard, FINDING 1) without crashing or applying
 *       state.
 *   (B) a mask block with a duplicate GNSS ID is rejected (FINDING 2: one mask
 *       per GNSS), without applying state.
 *   (C) a well-formed mask+orbit-only message applies orbit state with iod[0]
 *       set but iod[1] left from a different value, so the satpos_ssr-style gate
 *       iod[0]==iod[1] correctly excludes the satellite until a clock block of
 *       the same IOD set arrives (FINDING 3); a following clock block of the
 *       same IOD set then makes the gate pass.
 *   (D) a TOH >= 3600 and an implausible-latency header are both rejected
 *       (FINDING 1 plausibility gate).
 *   (H) has_sel_bias() takes the observed code when HAS provides it, then
 *       another code of the same signal (component <-> combined), never a
 *       different signal of the same band (GPS L2 P vs L2C, L1 C/A vs L1C);
 *       code and phase biases are selected on their own valid flags (#473).
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_has.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_time.h"

extern void setbitu(uint8_t* buff, int pos, int len, uint32_t data);
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

#define HAS_GNSS_GPS 0
#define HAS_GNSS_GAL 2
#define PAGE_OCT 53

/* feed a recovered message (ms*53 bytes) through has_input_page() as ms
 * systematic pages (PIDs 1..ms). Returns the last has_input_page() code (10 on a
 * complete, applied MT1; otherwise <10). */
static int feed_message(has_t* has, const uint8_t* msgbits, int ms, gtime_t time) {
    uint8_t page56[56];
    int p, ret = 0;

    for (p = 1; p <= ms; p++) {
        memset(page56, 0, sizeof(page56));
        /* 24-bit HAS page header: HASS(2)|Rsv(2)|MT(2)|MID(5)|MS(5)|PID(8).
         * HASS=1 (operational), MT=1 (the only correction type), MID=0,
         * MS stored minus 1, PID=p. */
        setbitu(page56, 0, 2, 1);       /* HASS = 1 */
        setbitu(page56, 2, 2, 0);       /* Reserved */
        setbitu(page56, 4, 2, 1);       /* MT = 1 */
        setbitu(page56, 6, 5, 0);       /* MID = 0 */
        setbitu(page56, 11, 5, ms - 1); /* MS - 1 */
        setbitu(page56, 16, 8, p);      /* PID */
        memcpy(page56 + 3, msgbits + (p - 1) * PAGE_OCT, PAGE_OCT);
        ret = has_input_page(has, 1, page56, time);
    }
    return ret;
}

/* write the 32-bit MT1 header into a recovered-message buffer. flags is a 6-bit
 * field: mask|orbit|cfull|csub|cb|pb (MSB first). */
static void put_mt1_header(uint8_t* msg, int toh, int flags6, int mid, int iodset) {
    setbitu(msg, 0, 12, toh);
    setbitu(msg, 12, 6, flags6);
    setbitu(msg, 18, 4, 0); /* Reserved */
    setbitu(msg, 22, 5, mid);
    setbitu(msg, 27, 5, iodset);
}

/* count satellites in has->ssr[] that would pass the satpos_ssr gate
 * (orbit and clock both present for the same IOD set). */
static int count_gate_pass(const has_t* has) {
    int i, n = 0;
    for (i = 0; i < MAXSAT; i++) {
        const ssr_t* s = &has->ssr[i];
        if (s->t0[0].time && s->t0[1].time && s->iod[0] == s->iod[1]) {
            n++;
        }
    }
    return n;
}

int main(void) {
    /* reception time: GST sow ~= 1 s past the hour so toh=0 has latency 1 s. */
    gtime_t time = gpst2time(2300, 3601.0);

    /* ---- (A) mask claims more bits than the 1-page (424-bit) buffer ---- */
    {
        has_t* has = has_new();
        CHECK(has != NULL, "A: has_new");
        uint8_t msg[PAGE_OCT]; /* MS=1 -> 424 bits */
        memset(msg, 0, sizeof(msg));
        put_mt1_header(msg, /*toh*/ 0, /*flags mask only*/ 0x20, 0, 0);
        /* Nsys=1, GPS, full SatM (40 sats), full SigM (16 sigs), CMAF=1. The
         * cell mask alone is 40*16=640 bits, far past the 424-bit buffer. */
        int bp = 32;
        setbitu(msg, bp, 4, 1); /* Nsys = 1 */
        bp += 4;
        setbitu(msg, bp, 4, HAS_GNSS_GPS); /* GNSS ID = GPS */
        bp += 4;
        setbitu(msg, bp, 32, 0xFFFFFFFFu); /* SatM bits 0..31 (setbitu caps at 32) */
        setbitu(msg, bp + 32, 8, 0xFF);    /* SatM bits 32..39 */
        bp += 40;
        setbitu(msg, bp, 16, 0xFFFF); /* SigM all 16 set */
        bp += 16;
        setbitu(msg, bp, 1, 1); /* CMAF = 1 (cell mask present) */

        int ret = feed_message(has, msg, 1, time);
        CHECK(ret != 10, "A: over-long mask rejected (no complete MT1)");
        CHECK(count_gate_pass(has) == 0, "A: no satellite state applied");
        has_free(has);
    }

    /* ---- (B) duplicate GNSS ID in the mask block ---- */
    {
        has_t* has = has_new();
        uint8_t msg[2 * PAGE_OCT]; /* MS=2 -> 848 bits, plenty of room */
        memset(msg, 0, sizeof(msg));
        put_mt1_header(msg, 0, 0x20, 0, 0); /* mask flag only */
        int bp = 32;
        setbitu(msg, bp, 4, 2); /* Nsys = 2 */
        bp += 4;
        /* first GNSS: GAL with 1 satellite, 1 signal, CMAF=0 */
        setbitu(msg, bp, 4, HAS_GNSS_GAL);
        bp += 4;
        setbitu(msg, bp, 1, 1); /* SatM bit 0 -> PRN 1 */
        bp += 40;
        setbitu(msg, bp, 1, 1); /* SigM bit 0 */
        bp += 16;
        setbitu(msg, bp, 1, 0); /* CMAF = 0 */
        bp += 1;
        setbitu(msg, bp, 3, 0); /* NM */
        bp += 3;
        /* second GNSS: GAL AGAIN (duplicate) */
        setbitu(msg, bp, 4, HAS_GNSS_GAL);
        bp += 4;
        setbitu(msg, bp, 1, 1);
        bp += 40;
        setbitu(msg, bp, 1, 1);
        bp += 16;
        setbitu(msg, bp, 1, 0);
        bp += 1;
        setbitu(msg, bp, 3, 0);

        int ret = feed_message(has, msg, 2, time);
        CHECK(ret != 10, "B: duplicate GNSS mask rejected");
        CHECK(count_gate_pass(has) == 0, "B: no satellite state applied");
        has_free(has);
    }

    /* ---- (C) orbit-only message: iod[0] set, iod[1] absent -> gate excludes;
     *          a later clock of the same IOD set -> gate passes ---- */
    {
        has_t* has = has_new();
        int gal1 = satno(SYS_GAL, 1);
        CHECK(gal1 > 0, "C: GAL PRN1 satno");

        /* message 1: mask + orbit (flags 110000), IOD set = 5, GAL PRN1 only */
        uint8_t m1[2 * PAGE_OCT];
        memset(m1, 0, sizeof(m1));
        put_mt1_header(m1, 0, 0x30, 0, 5); /* mask + orbit */
        int bp = 32;
        setbitu(m1, bp, 4, 1); /* Nsys = 1 */
        bp += 4;
        setbitu(m1, bp, 4, HAS_GNSS_GAL);
        bp += 4;
        setbitu(m1, bp, 1, 1); /* SatM PRN1 */
        bp += 40;
        setbitu(m1, bp, 1, 1); /* SigM sig0 */
        bp += 16;
        setbitu(m1, bp, 1, 0); /* CMAF=0 */
        bp += 1;
        setbitu(m1, bp, 3, 0); /* NM */
        bp += 3;
        bp += 6; /* mask Reserved */
        /* orbit block: VI(4) | IODref(10 GAL) | DR(13) DIT(12) DCT(12) */
        setbitu(m1, bp, 4, 0); /* VI */
        bp += 4;
        setbitu(m1, bp, 10, 42); /* IODref */
        bp += 10;
        setbitu(m1, bp, 13, 100); /* DR (non-NA) */
        bp += 13;
        setbitu(m1, bp, 12, 50); /* DIT */
        bp += 12;
        setbitu(m1, bp, 12, 25); /* DCT */
        bp += 12;

        int ret = feed_message(has, m1, 2, time);
        CHECK(ret == 10, "C: orbit-only message decoded");
        CHECK(has->ssr[gal1 - 1].t0[0].time != 0, "C: orbit t0[0] set");
        CHECK(has->ssr[gal1 - 1].t0[1].time == 0, "C: clock t0[1] absent");
        CHECK(count_gate_pass(has) == 0, "C: gate excludes sat with no clock");

        /* message 2: clock full-set (flags 001000), same mask + IOD set 5 */
        uint8_t m2[PAGE_OCT];
        memset(m2, 0, sizeof(m2));
        put_mt1_header(m2, 0, 0x08, 0, 5); /* clock full-set, mask flag 0 */
        bp = 32;
        setbitu(m2, bp, 4, 0); /* VI */
        bp += 4;
        setbitu(m2, bp, 2, 0); /* DCM for the single GNSS */
        bp += 2;
        setbitu(m2, bp, 13, 80); /* DCC (non-NA) */
        bp += 13;

        ret = feed_message(has, m2, 1, time);
        CHECK(ret == 10, "C: clock message decoded");
        CHECK(has->ssr[gal1 - 1].t0[1].time != 0, "C: clock t0[1] now set");
        CHECK(has->ssr[gal1 - 1].iod[0] == has->ssr[gal1 - 1].iod[1], "C: iod[0]==iod[1] after both blocks");
        CHECK(count_gate_pass(has) == 1, "C: gate passes once both blocks arrive");
        /* hrclk slot must stay inert: t0[2] never set */
        CHECK(has->ssr[gal1 - 1].t0[2].time == 0, "C: hrclk t0[2] stays 0 (inert)");
        has_free(has);
    }

    /* ---- (D) plausibility gate: TOH>=3600 and implausible latency ---- */
    {
        has_t* has = has_new();
        uint8_t msg[PAGE_OCT];
        memset(msg, 0, sizeof(msg));
        put_mt1_header(msg, /*toh*/ 4000, 0x20, 0, 0); /* TOH out of range */
        int ret = feed_message(has, msg, 1, time);
        CHECK(ret != 10, "D: TOH>=3600 rejected");

        /* latency gate: reception 1200 s past the hour, toh=0 -> latency 1200 s
         * > 600 s -> reject. */
        gtime_t late = gpst2time(2300, 1200.0);
        memset(msg, 0, sizeof(msg));
        put_mt1_header(msg, 0, 0x20, 0, 0);
        ret = feed_message(has, msg, 1, late);
        CHECK(ret != 10, "D: implausible latency rejected");
        has_free(has);
    }

    /* ---- (E) phase-bias NA clears a previously valid bias (vpbias=0, pbias=0).
     *          Regression for the float-PPP "keep carrier when no phase bias"
     *          behavior: an NA must NOT leave SSR_INVALID_PBIAS behind. ---- */
    {
        has_t* has = has_new();
        int gal1 = satno(SYS_GAL, 1);

        /* message 1: mask + pbias (flags 100001), GAL PRN1, single signal sig0
         * (E1-B = CODE_L1B), CMAF=1 with the one cell provided. Valid pbias. */
        uint8_t m1[2 * PAGE_OCT];
        memset(m1, 0, sizeof(m1));
        put_mt1_header(m1, 0, 0x21, 0, 5); /* mask + pbias */
        int bp = 32;
        setbitu(m1, bp, 4, 1); /* Nsys = 1 */
        bp += 4;
        setbitu(m1, bp, 4, HAS_GNSS_GAL);
        bp += 4;
        setbitu(m1, bp, 1, 1); /* SatM PRN1 */
        bp += 40;
        setbitu(m1, bp, 1, 1); /* SigM sig0 (E1-B) */
        bp += 16;
        setbitu(m1, bp, 1, 1); /* CMAF = 1 */
        bp += 1;
        setbitu(m1, bp, 1, 1); /* cell mask: 1 signal provided */
        bp += 1;
        setbitu(m1, bp, 3, 0); /* NM */
        bp += 3;
        bp += 6;               /* mask Reserved */
        setbitu(m1, bp, 4, 0); /* pbias VI */
        bp += 4;
        setbitu(m1, bp, 11, 50); /* PB value (non-NA) */
        bp += 11;
        setbitu(m1, bp, 2, 1); /* PDI */
        bp += 2;

        int ret = feed_message(has, m1, 2, time);
        CHECK(ret == 10, "E: mask+pbias message decoded");
        CHECK(has->ssr[gal1 - 1].vpbias[CODE_L1B - 1] == 1, "E: vpbias set after valid pbias");
        CHECK(has->ssr[gal1 - 1].pbias[CODE_L1B - 1] != 0.0, "E: pbias non-zero after valid pbias");

        /* message 2: pbias-only (flags 000001), same mask, NA value. */
        uint8_t m2[PAGE_OCT];
        memset(m2, 0, sizeof(m2));
        put_mt1_header(m2, 0, 0x01, 0, 5); /* pbias only */
        bp = 32;
        setbitu(m2, bp, 4, 0); /* pbias VI */
        bp += 4;
        setbitu(m2, bp, 11, (uint32_t)(-1024) & 0x7FF); /* PB = NA (1000...0) */
        bp += 11;
        setbitu(m2, bp, 2, 0); /* PDI */
        bp += 2;

        ret = feed_message(has, m2, 1, time);
        CHECK(ret == 10, "E: pbias-NA message decoded");
        CHECK(has->ssr[gal1 - 1].vpbias[CODE_L1B - 1] == 0, "E: vpbias cleared on pbias NA");
        CHECK(has->ssr[gal1 - 1].pbias[CODE_L1B - 1] == 0.0, "E: pbias cleared to 0 on NA (not SSR_INVALID_PBIAS)");
        CHECK(has->ssr[gal1 - 1].update == 1, "E: update flag set so cleared state propagates");
        has_free(has);
    }

    /* ---- (F) code-bias NA clears a previously valid bias (vcbias=0, cbias=0).
     *          Prevents a stale valid bias persisting in nav->ssr_ch. ---- */
    {
        has_t* has = has_new();
        int gal1 = satno(SYS_GAL, 1);

        /* message 1: mask + cbias (flags 100010), GAL PRN1, sig0, valid cbias. */
        uint8_t m1[2 * PAGE_OCT];
        memset(m1, 0, sizeof(m1));
        put_mt1_header(m1, 0, 0x22, 0, 5); /* mask + cbias */
        int bp = 32;
        setbitu(m1, bp, 4, 1);
        bp += 4;
        setbitu(m1, bp, 4, HAS_GNSS_GAL);
        bp += 4;
        setbitu(m1, bp, 1, 1); /* SatM PRN1 */
        bp += 40;
        setbitu(m1, bp, 1, 1); /* SigM sig0 */
        bp += 16;
        setbitu(m1, bp, 1, 1); /* CMAF = 1 */
        bp += 1;
        setbitu(m1, bp, 1, 1); /* cell mask */
        bp += 1;
        setbitu(m1, bp, 3, 0); /* NM */
        bp += 3;
        bp += 6;               /* mask Reserved */
        setbitu(m1, bp, 4, 0); /* cbias VI */
        bp += 4;
        setbitu(m1, bp, 11, 40); /* CB value (non-NA) */
        bp += 11;

        int ret = feed_message(has, m1, 2, time);
        CHECK(ret == 10, "F: mask+cbias message decoded");
        CHECK(has->ssr[gal1 - 1].vcbias[CODE_L1B - 1] == 1, "F: vcbias set after valid cbias");
        CHECK(has->ssr[gal1 - 1].cbias[CODE_L1B - 1] != 0.0f, "F: cbias non-zero after valid cbias");

        /* message 2: cbias-only (flags 000010), NA value. */
        uint8_t m2[PAGE_OCT];
        memset(m2, 0, sizeof(m2));
        put_mt1_header(m2, 0, 0x02, 0, 5); /* cbias only */
        bp = 32;
        setbitu(m2, bp, 4, 0); /* cbias VI */
        bp += 4;
        setbitu(m2, bp, 11, (uint32_t)(-1024) & 0x7FF); /* CB = NA */
        bp += 11;

        ret = feed_message(has, m2, 1, time);
        CHECK(ret == 10, "F: cbias-NA message decoded");
        CHECK(has->ssr[gal1 - 1].vcbias[CODE_L1B - 1] == 0, "F: vcbias cleared on cbias NA");
        CHECK(has->ssr[gal1 - 1].cbias[CODE_L1B - 1] == 0.0f, "F: cbias cleared to 0 on NA (not SSR_INVALID_CBIAS)");
        CHECK(has->ssr[gal1 - 1].update == 1, "F: update flag set so cleared state propagates");
        has_free(has);
    }

    /* ---- (G) MS-change restart: an in-flight (MID,MT) collection that receives
     *          a page with a different MS must restart, not mix generations into
     *          one RS decode. Mixed pages must not complete a message; a clean,
     *          self-consistent set then completes normally. ---- */
    {
        has_t* has = has_new();
        int gal1 = satno(SYS_GAL, 1);

        /* Build a valid 2-page (MS=2) mask+orbit message for GAL PRN1. */
        uint8_t big[2 * PAGE_OCT];
        memset(big, 0, sizeof(big));
        put_mt1_header(big, 0, 0x30, 0, 5); /* mask + orbit */
        int bp = 32;
        setbitu(big, bp, 4, 1);
        bp += 4;
        setbitu(big, bp, 4, HAS_GNSS_GAL);
        bp += 4;
        setbitu(big, bp, 1, 1); /* SatM PRN1 */
        bp += 40;
        setbitu(big, bp, 1, 1); /* SigM sig0 */
        bp += 16;
        setbitu(big, bp, 1, 0); /* CMAF=0 */
        bp += 1;
        setbitu(big, bp, 3, 0); /* NM */
        bp += 3;
        bp += 6;                /* mask Reserved */
        setbitu(big, bp, 4, 0); /* orbit VI */
        bp += 4;
        setbitu(big, bp, 10, 42); /* IODref */
        bp += 10;
        setbitu(big, bp, 13, 100); /* DR */
        bp += 13;
        setbitu(big, bp, 12, 50); /* DIT */
        bp += 12;
        setbitu(big, bp, 12, 25); /* DCT */
        bp += 12;

        /* Feed page 1 of an MS=2 collection (MID=0), then a page that re-uses
         * MID=0 but advertises a different MS (MS=1). The mismatch must restart
         * the collector, so neither the stale page nor the lone new page can
         * complete a message. */
        uint8_t page56[56];
        memset(page56, 0, sizeof(page56));
        setbitu(page56, 0, 2, 1);  /* HASS=1 */
        setbitu(page56, 4, 2, 1);  /* MT=1 */
        setbitu(page56, 6, 5, 0);  /* MID=0 */
        setbitu(page56, 11, 5, 1); /* MS-1 = 1 -> MS=2 */
        setbitu(page56, 16, 8, 1); /* PID=1 */
        memcpy(page56 + 3, big, PAGE_OCT);
        int ret = has_input_page(has, 1, page56, time);
        CHECK(ret != 10, "G: first page of MS=2 collection does not complete");

        /* same MID, different MS (MS=1) -> restart; this single page cannot
         * RS-decode into the MS=2 message and must not complete. */
        memset(page56, 0, sizeof(page56));
        setbitu(page56, 0, 2, 1);
        setbitu(page56, 4, 2, 1);
        setbitu(page56, 6, 5, 0);                     /* MID=0 (reused) */
        setbitu(page56, 11, 5, 0);                    /* MS-1 = 0 -> MS=1 */
        setbitu(page56, 16, 8, 1);                    /* PID=1 */
        memcpy(page56 + 3, big + PAGE_OCT, PAGE_OCT); /* page 2 payload (mismatched) */
        ret = has_input_page(has, 1, page56, time);
        CHECK(ret != 10, "G: MS-change page restarts collection (no mixed-gen decode)");
        CHECK(count_gate_pass(has) == 0, "G: no satellite state applied from mixed pages");

        /* Now a clean self-consistent MS=2 message on the same MID completes. */
        ret = feed_message(has, big, 2, time);
        CHECK(ret == 10, "G: clean MS=2 message completes after restart");
        CHECK(has->ssr[gal1 - 1].t0[0].time != 0, "G: orbit state applied from clean set");
        has_free(has);
    }

    /* ---- (H) bias signal selection (#473) ---- */
    {
        static ssr_t ssr;

        memset(&ssr, 0, sizeof(ssr));
        ssr.vcbias[CODE_L1C - 1] = ssr.vcbias[CODE_L5Q - 1] = 1; /* HAS E1-C, E5a-Q */
        CHECK(has_sel_bias(&ssr, SYS_GAL, CODE_L1C, 0) == CODE_L1C, "H: GAL obs C1C -> HAS C1C (exact)");
        CHECK(has_sel_bias(&ssr, SYS_GAL, CODE_L5Q, 0) == CODE_L5Q, "H: GAL obs C5Q -> HAS C5Q (exact)");
        CHECK(has_sel_bias(&ssr, SYS_GAL, CODE_L1X, 0) == CODE_L1C,
              "H: GAL obs C1X -> HAS C1C (combined -> component)");
        CHECK(has_sel_bias(&ssr, SYS_GAL, CODE_L5I, 0) == CODE_L5Q, "H: GAL obs C5I -> HAS C5Q (same signal)");
        CHECK(has_sel_bias(&ssr, SYS_GAL, CODE_L7Q, 0) == CODE_L7X, "H: GAL obs C7Q, no E5b bias -> C7X (absent)");
        CHECK(has_sel_bias(&ssr, SYS_GAL, CODE_L1C, 1) == CODE_L1X, "H: phase uses vpbias (none) -> C1X (absent)");

        memset(&ssr, 0, sizeof(ssr));
        ssr.vcbias[CODE_L1X - 1] = 1;
        ssr.vpbias[CODE_L1C - 1] = 1;
        CHECK(has_sel_bias(&ssr, SYS_GAL, CODE_L1C, 0) == CODE_L1X,
              "H: GAL obs C1C -> HAS C1X (component -> combined)");
        CHECK(has_sel_bias(&ssr, SYS_GAL, CODE_L1C, 1) == CODE_L1C, "H: GAL phase obs L1C -> HAS L1C (vpbias)");

        memset(&ssr, 0, sizeof(ssr));
        ssr.vcbias[CODE_L1C - 1] = ssr.vcbias[CODE_L2W - 1] = ssr.vcbias[CODE_L2L - 1] = 1;
        CHECK(has_sel_bias(&ssr, SYS_GPS, CODE_L2L, 0) == CODE_L2L, "H: GPS obs C2L -> HAS C2L (exact)");
        CHECK(has_sel_bias(&ssr, SYS_GPS, CODE_L2W, 0) == CODE_L2W, "H: GPS obs C2W -> HAS C2W (exact)");
        CHECK(has_sel_bias(&ssr, SYS_GPS, CODE_L2S, 0) == CODE_L2L, "H: GPS obs C2S -> HAS C2L (same signal)");
        CHECK(has_sel_bias(&ssr, SYS_GPS, CODE_L1L, 0) == CODE_L1X, "H: GPS obs L1C(P), only C/A bias -> C1X (absent)");

        memset(&ssr, 0, sizeof(ssr));
        ssr.vcbias[CODE_L2W - 1] = 1;
        CHECK(has_sel_bias(&ssr, SYS_GPS, CODE_L2L, 0) == CODE_L2X, "H: GPS obs C2L, only L2 P bias -> C2X (absent)");
        CHECK(has_sel_bias(&ssr, SYS_GAL, CODE_NONE, 0) == CODE_NONE, "H: CODE_NONE -> CODE_NONE");
        CHECK(has_sel_bias(&ssr, SYS_GLO, CODE_L1C, 0) == CODE_NONE, "H: GLONASS -> CODE_NONE (no HAS signal)");
    }

    /* ---- (I) a signal that leaves the mask keeps no bias (#473 review): mask
     *          E1-C, then E1-B+C; obs C1C must select the fresh C1X slot. ---- */
    {
        has_t* has = has_new();
        int gal1 = satno(SYS_GAL, 1), sig, k;

        for (k = 0; k < 2; k++) {
            uint8_t m[2 * PAGE_OCT];
            int bp = 32;
            sig = k == 0 ? 1 : 2; /* Table 20: 1 = E1-C, 2 = E1-B+C */
            memset(m, 0, sizeof(m));
            put_mt1_header(m, 0, 0x23, 0, 5); /* mask + cbias + pbias */
            setbitu(m, bp, 4, 1);             /* Nsys */
            bp += 4;
            setbitu(m, bp, 4, HAS_GNSS_GAL);
            bp += 4;
            setbitu(m, bp, 1, 1); /* SatM PRN1 */
            bp += 40;
            setbitu(m, bp + sig, 1, 1); /* SigM */
            bp += 16;
            setbitu(m, bp, 1, 0); /* CMAF = 0: all cells */
            bp += 1;
            setbitu(m, bp, 3, 0); /* NM */
            bp += 3;
            bp += 6;              /* mask Reserved */
            setbitu(m, bp, 4, 0); /* cbias VI */
            bp += 4;
            setbitu(m, bp, 11, 40 + k); /* CB */
            bp += 11;
            setbitu(m, bp, 4, 0); /* pbias VI */
            bp += 4;
            setbitu(m, bp, 11, 30 + k); /* PB */
            bp += 11;
            setbitu(m, bp, 2, 1); /* PDI */
            bp += 2;
            CHECK(feed_message(has, m, 2, time) == 10, k == 0 ? "I: E1-C mask decoded" : "I: E1-B+C mask decoded");
        }
        const ssr_t* s = &has->ssr[gal1 - 1];
        CHECK(s->vcbias[CODE_L1C - 1] == 0 && s->cbias[CODE_L1C - 1] == 0.0f, "I: C1C code bias cleared");
        CHECK(s->vpbias[CODE_L1C - 1] == 0 && s->pbias[CODE_L1C - 1] == 0.0, "I: L1C phase bias cleared");
        CHECK(s->vcbias[CODE_L1X - 1] == 1 && s->vpbias[CODE_L1X - 1] == 1, "I: C1X code / phase bias valid");
        CHECK(has_sel_bias(s, SYS_GAL, CODE_L1C, 0) == CODE_L1X, "I: obs C1C -> fresh C1X code bias");
        CHECK(has_sel_bias(s, SYS_GAL, CODE_L1C, 1) == CODE_L1X, "I: obs L1C -> fresh L1X phase bias");
        CHECK(s->discnt[CODE_L1C - 1] == 1, "I: discnt of the removed signal kept");
        has_free(has);
    }

    printf("\n%s: %d failure(s)\n", fails ? "FAILED" : "PASSED", fails);
    return fails ? 1 : 0;
}
