/*------------------------------------------------------------------------------
 * unit test : RTCM local correction block lazy allocation (issue #295)
 *
 * rtcm_t.lclblk is a lazily-allocated lclblock_t* (previously an embedded
 * array). No regression test feeds RTCM3 types 2001-2016, so this test is the
 * direct coverage of the active path. Coverage:
 *
 *   (1) fresh-struct invariants: init_rtcm() leaves lclblk NULL; the
 *       NULL-guarded consumers (block2stat, encode_lcltrop, encode_lcliono)
 *       return 0 without crashing; rtcm_lclblk() allocates on first use and
 *       is idempotent; free_rtcm() frees + NULLs lclblk and a second
 *       free_rtcm() call is safe.
 *   (2) encode->decode round-trip of a local tropospheric correction grid
 *       message (RTCM3 type 2001) through gen_rtcm3()/input_rtcm3(): the
 *       decoder side allocates lclblk lazily on the first message, and block
 *       number / trop value / std round-trip within the encoding
 *       quantization. block2stat() is then exercised on the decoded block
 *       (non-NULL path).
 *
 * Note: the single-grid-point setup (mask[0]=0x1 so n=1 and gp[0]==0) is
 * load-bearing: encode_lcltrop() reads blkinf->grid[gp[i]] (mask-bit index)
 * while initblkinf()/decode_lcltrop() fill/read grid[] compactly (0..n-1);
 * the two indexings only coincide when gp[i]==i.
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

/* encoding constants (mirror src/rtcm/mrtk_rtcm3_local_corr.c) */
#define TRP_BLKSIZE 4.0   /* troposphere block size (deg) */
#define TRP_DET_LSB 0.002 /* tropospheric delay detail value resolution (m) */

/* (1) fresh-struct invariants ---------------------------------------------- */
static void test_fresh_struct(void) {
    rtcm_t* r = (rtcm_t*)calloc(1, sizeof(rtcm_t));    /* never stack-allocate rtcm_t (~7.5 MB) */
    stat_t* stat = (stat_t*)calloc(1, sizeof(stat_t)); /* stat_t ~4.8 MB: heap too */
    lclblock_t* blk;
    int ok;

    printf("--- test_fresh_struct\n");
    if (!r || !stat) {
        printf("FAIL: calloc rtcm_t/stat_t\n");
        fails++;
        goto cleanup;
    }
    ok = init_rtcm(r);
    CHECK(ok == 1, "init_rtcm succeeds");
    if (ok != 1) {
        goto cleanup;
    }
    CHECK(r->lclblk == NULL, "init_rtcm leaves lclblk NULL");
    CHECK(block2stat(r, stat) == 0, "block2stat with NULL lclblk returns 0");
    CHECK(encode_lcltrop(r, 2001) == 0, "encode_lcltrop with NULL lclblk returns 0");
    CHECK(encode_lcliono(r, 2002) == 0, "encode_lcliono with NULL lclblk returns 0");

    blk = rtcm_lclblk(r);
    CHECK(blk != NULL, "rtcm_lclblk allocates on first use");
    CHECK(r->lclblk == blk, "rtcm_lclblk stores the allocation in rtcm->lclblk");
    CHECK(rtcm_lclblk(r) == blk, "second rtcm_lclblk call returns the same pointer");

    free_rtcm(r);
    CHECK(r->lclblk == NULL, "free_rtcm resets lclblk to NULL");
    free_rtcm(r); /* double-free safety: all pointers were NULLed above */
    CHECK(r->lclblk == NULL, "second free_rtcm is safe");

cleanup:
    free(stat);
    free(r);
}

/* (2) trop (2001) encode->decode round-trip -------------------------------- */
static void test_trop_roundtrip(void) {
    rtcm_t* enc = (rtcm_t*)calloc(1, sizeof(rtcm_t));
    rtcm_t* dec = (rtcm_t*)calloc(1, sizeof(rtcm_t));
    stat_t* stat = (stat_t*)calloc(1, sizeof(stat_t));
    /* Sunday 2026-08-16, tow=60 s: a 30 s multiple so the 15-bit epoch field
     * (LSB 30 s) round-trips the time exactly */
    double ep[] = {2026, 8, 16, 0, 1, 0.0};
    double zazel[] = {0.0, PI / 2.0};
    gtime_t t0 = epoch2time(ep);
    lclblock_t* lcl;
    blkinf_t* bi;
    sitetrp_t* st;
    trp_t* dtrp;
    double zhd, trp_in;
    int i, ret = 0, mid_ok = 1, ok;

    printf("--- test_trop_roundtrip\n");
    if (!enc || !dec || !stat) {
        printf("FAIL: calloc rtcm_t/stat_t\n");
        fails++;
        goto cleanup;
    }
    ok = init_rtcm(enc) == 1 && init_rtcm(dec) == 1;
    CHECK(ok, "init_rtcm enc+dec");
    if (!ok) {
        goto cleanup;
    }
    enc->time = t0;

    /* build a minimal valid trop block: grid type, bn=1204 (38N/136E),
     * 16-grid pitch, single grid point (mask bit 0) */
    lcl = rtcm_lclblk(enc);
    CHECK(lcl != NULL, "encoder rtcm_lclblk");
    if (!lcl) {
        goto cleanup;
    }
    bi = &lcl->tblkinf[0];
    bi->btype = 0; /* BTYPE_GRID */
    bi->bn = 1204;
    bi->bs = TRP_BLKSIZE;
    bi->gpitch = 0;
    bi->mask[0] = 0x1;
    initblkinf(bi);
    CHECK(bi->n == 1 && bi->gp[0] == 0, "initblkinf: single grid point, gp[0]==0");
    lcl->tnum = 1;
    lcl->outtn = 0;

    /* trop value: zhd + 15 cm zenith wet delay; std 0.010 quantizes to std
     * index 2 -> 0.008 (deliberately off the log2 integer boundary) */
    zhd = tropmodel(t0, bi->grid[0], zazel, 0.0);
    st = &lcl->tstat[0][0];
    st->trpd.time = t0;
    st->trpd.trp[0] = zhd + 0.15;
    st->trpd.std[0] = 0.010;
    trp_in = st->trpd.trp[0];

    CHECK(gen_rtcm3(enc, 2001, 0, 0) == 1, "gen_rtcm3 type 2001 succeeds");
    CHECK(enc->nbyte == enc->len + 3, "framed length: nbyte == len + parity");
    CHECK(enc->buff[0] == 0xD3, "frame starts with RTCM3 preamble");
    CHECK(getbitu(enc->buff, 24, 12) == 2001, "message number field is 2001");

    /* feed byte-by-byte into a fresh decoder */
    dec->time = t0; /* approximate time to resolve the epoch week */
    CHECK(dec->lclblk == NULL, "decoder lclblk NULL before first byte");
    for (i = 0; i < enc->nbyte; i++) {
        ret = input_rtcm3(dec, enc->buff[i]);
        if (i + 1 < enc->nbyte && ret != 0) {
            mid_ok = 0;
        }
    }
    CHECK(mid_ok, "no decode result before the final byte");
    CHECK(ret == 12, "final byte completes the message (ret==12)");
    CHECK(dec->lclblk != NULL, "decoder allocated lclblk lazily");
    if (!dec->lclblk) {
        goto cleanup; /* remaining checks dereference dec->lclblk */
    }
    CHECK(dec->lclblk->tnum == 1, "decoded tnum == 1");
    CHECK(dec->lclblk->tblkinf[0].bn == 1204, "block number round-trips");
    CHECK(dec->lclblk->tblkinf[0].n == 1, "decoded grid point count == 1");

    dtrp = &dec->lclblk->tstat[0][0].trpd;
    CHECK(fabs(timediff(dtrp->time, t0)) < 1e-9, "epoch time round-trips exactly");
    CHECK(fabs(dtrp->trp[0] - trp_in) < 2.0 * TRP_DET_LSB, "trop delay round-trips within quantization");
    CHECK(fabs(dtrp->std[0] - 0.008) < 1e-12, "trop std quantizes to index 2 (0.008)");

    /* block2stat non-NULL path on the decoded block */
    CHECK(block2stat(dec, stat) == 1, "block2stat on decoded block returns 1");
    CHECK(stat->nst == 1, "block2stat filled one trop station");
    CHECK(fabs(stat->strp[0].trpd.trp[0] - trp_in) < 2.0 * TRP_DET_LSB, "block2stat trop value matches");

cleanup:
    if (enc) {
        free_rtcm(enc);
    }
    if (dec) {
        free_rtcm(dec);
    }
    free(stat);
    free(enc);
    free(dec);
}

int main(void) {
    test_fresh_struct();
    test_trop_roundtrip();

    if (fails) {
        printf("utest_lclblk: %d check(s) FAILED\n", fails);
        return 1;
    }
    printf("utest_lclblk: all checks passed\n");
    return 0;
}
