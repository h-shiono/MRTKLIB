/*------------------------------------------------------------------------------
 * unit test : CLAS L6 channels from several L6 frame files (#373)
 *
 * Synthetic L6 files (250-byte frames, CLAS vendor ID): every block is an ST1
 * frame (subframe indicator set, CSSR 4073 subtype 1 with the GNSS epoch time)
 * and four continuation frames. Checked on the segment lists that
 * clas_l6seq_build() returns:
 *   (1) scan: blocks at the ST1 frames, the first block from byte 0 (leading
 *       frames), an ST1 epoch that does not advance starts no block
 *   (2) two files of one pattern without overlap, given in reverse time
 *       order: read back to back in time order
 *   (3) a PRN that comes back (A, B, A over a day): A, B, A
 *   (4) overlap: stay on the current file; on its gap take the other file
 *       and stay there when the first one resumes
 *   (5) l6mrg on: one channel per pattern in order of first appearance, a tie
 *       goes to the earlier file
 *   (6) l6mrg off: every file in channel 0, read across patterns in time
 *   (7) files without a CLAS frame or without an ST1 are not used
 *   (8) GPS week rollover inside a file and between files
 *   (9) a channel of one file reads the whole file
 *  (10) clas_l6seq_input() feeds the decoder the segment bytes in order
 *  (11) lost ST1 in the current file (its block runs on over the next 30 s):
 *       the other file's block inside that span is skipped, no step back
 *       in time; a real gap (fewer frames) is still filled
 *  (12) frames before a file's first ST1 are dropped on a switch when they
 *       overlap what was read, kept when they do not
 *----------------------------------------------------------------------------*/
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

#define FRM 250 /* frame with R-S parity */
#define NFB 5   /* frames per block */

static void put_frame(FILE* fp, int prn, int vid, int ptn, int sf, int st1, int tow) {
    uint8_t f[FRM] = {0};
    f[0] = 0x1A;
    f[1] = 0xCF;
    f[2] = 0xFC;
    f[3] = 0x1D;
    f[4] = (uint8_t)prn;
    f[5] = (uint8_t)((vid << 5) | ((ptn + 2) << 3) | (ptn << 1) | sf);
    if (st1) {
        setbitu(f, 49, 12, 4073);
        setbitu(f, 61, 4, 1);
        setbitu(f, 65, 20, (uint32_t)tow);
    } else if (sf) {
        setbitu(f, 49, 12, 4073);
        setbitu(f, 61, 4, 2);
    }
    fwrite(f, 1, FRM, fp);
}

/* L6 file: lead continuation frames, then one block per ST1 epoch of nfb[i]
 * frames (NFB if nfb is NULL) */
static void write_file_n(const char* path, int prn, int ptn, int lead, const int* tow, int n, const int* nfb) {
    FILE* fp = fopen(path, "wb");
    int i, k;
    if (!fp) {
        printf("cannot write %s\n", path);
        exit(2);
    }
    for (i = 0; i < lead; i++) {
        put_frame(fp, prn, 5, ptn, i == 0, 0, 0);
    }
    for (i = 0; i < n; i++) {
        put_frame(fp, prn, 5, ptn, 1, 1, tow[i]);
        for (k = 1; k < (nfb ? nfb[i] : NFB); k++) {
            put_frame(fp, prn, 5, ptn, k % 5 == 0, 0, 0);
        }
    }
    fclose(fp);
}

static void write_file(const char* path, int prn, int ptn, int lead, const int* tow, int n) {
    write_file_n(path, prn, ptn, lead, tow, n, NULL);
}

/* byte offset of block k of a file with lead leading frames */
static long boff(int lead, int k) { return k == 0 ? 0 : (long)(lead + NFB * k) * FRM; }

typedef struct {
    const char* path;
    long start, end;
} seg_t;

static int same_segs(const clas_l6seq_t* s, const seg_t* e, int n) {
    int i;
    if (s->nseg != n) {
        printf("      nseg=%d expected %d\n", s->nseg, n);
        for (i = 0; i < s->nseg; i++) {
            printf("      seg %s %ld..%ld\n", s->path[s->seg[i].file], s->seg[i].start, s->seg[i].end);
        }
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(s->path[s->seg[i].file], e[i].path) || s->seg[i].start != e[i].start || s->seg[i].end != e[i].end) {
            printf("      seg %d: %s %ld..%ld expected %s %ld..%ld\n", i, s->path[s->seg[i].file], s->seg[i].start,
                   s->seg[i].end, e[i].path, e[i].start, e[i].end);
            return 0;
        }
    }
    return 1;
}

static int build(const char** paths, int n, int l6mrg, clas_l6seq_t* seq, int* chof) {
    clas_l6file_t f[CLAS_L6SEQ_MAXFILE];
    int i, nch;
    for (i = 0; i < n; i++) {
        clas_l6_scan_file(paths[i], f + i);
    }
    nch = clas_l6seq_build(f, n, l6mrg, seq, chof);
    for (i = 0; i < n; i++) {
        clas_l6_free_file(f + i);
    }
    return nch;
}

static void free_seq(clas_l6seq_t* seq) {
    int ch;
    for (ch = 0; ch < CLAS_CH_NUM; ch++) {
        clas_l6seq_free(seq + ch);
    }
}

int main(void) {
    clas_l6seq_t seq[CLAS_CH_NUM];
    clas_l6file_t f;
    int chof[CLAS_L6SEQ_MAXFILE], nch;

    /* (1) scan */
    {
        static const int tow[] = {100, 130, 130, 120, 160};
        write_file("l6seq_scan.l6d", 199, 0, 3, tow, 5);
        CHECK(clas_l6_scan_file("l6seq_scan.l6d", &f), "(1) scan reads the file");
        CHECK(f.prn == 199 && f.ptn == 0, "(1) PRN and pattern of the first CLAS frame");
        CHECK(f.size == (3 + 5 * NFB) * FRM, "(1) file size");
        CHECK(f.nblk == 3 && f.blk[0].t == 100 && f.blk[1].t == 130 && f.blk[2].t == 160,
              "(1) blocks at advancing ST1 epochs only");
        CHECK(f.nblk == 3 && f.blk[0].off == 0 && f.blk[1].off == boff(3, 1) && f.blk[2].off == boff(3, 4),
              "(1) first block from byte 0, others at their ST1 frame");
        clas_l6_free_file(&f);
        CHECK(!clas_l6_scan_file("l6seq_missing.l6d", &f), "(1) missing file");
    }
    /* (2) handover within one pattern, reverse order on input */
    {
        static const int ta[] = {0, 30, 60}, tb[] = {90, 120};
        const char* in[] = {"l6seq_b.l6d", "l6seq_a.l6d"};
        const seg_t e[] = {{"l6seq_a.l6d", 0, 3 * NFB * FRM}, {"l6seq_b.l6d", 0, (1 + 2 * NFB) * FRM}};
        write_file("l6seq_a.l6d", 199, 0, 0, ta, 3);
        write_file("l6seq_b.l6d", 195, 0, 1, tb, 2);
        nch = build(in, 2, 0, seq, chof);
        CHECK(nch == 1 && chof[0] == 0 && chof[1] == 0, "(2) one channel");
        CHECK(same_segs(seq, e, 2), "(2) time order, whole files back to back");
        free_seq(seq);
        nch = build(in, 2, 1, seq, chof);
        CHECK(nch == 1 && same_segs(seq, e, 2), "(2) the same with l6mrg on (one pattern)");
        free_seq(seq);
    }
    /* (3) PRN coming back */
    {
        static const int ta[] = {0, 30, 60, 180, 210}, tb[] = {90, 120, 150};
        const char* in[] = {"l6seq_a.l6d", "l6seq_b.l6d"};
        const seg_t e[] = {{"l6seq_a.l6d", 0, boff(0, 3)},
                           {"l6seq_b.l6d", 0, 3 * NFB * FRM},
                           {"l6seq_a.l6d", boff(0, 3), 5 * NFB * FRM}};
        write_file("l6seq_a.l6d", 199, 0, 0, ta, 5);
        write_file("l6seq_b.l6d", 195, 0, 0, tb, 3);
        build(in, 2, 0, seq, chof);
        CHECK(same_segs(seq, e, 3), "(3) A, B, A");
        free_seq(seq);
    }
    /* (4) overlap */
    {
        static const int ta[] = {0, 30, 60, 90, 180, 210}, tb[] = {0, 30, 60, 90, 120, 150, 180, 210, 240};
        const char* in[] = {"l6seq_a.l6d", "l6seq_b.l6d"};
        const seg_t e[] = {{"l6seq_a.l6d", 0, boff(0, 4)}, {"l6seq_b.l6d", boff(0, 4), 9 * NFB * FRM}};
        write_file("l6seq_a.l6d", 199, 0, 0, ta, 6);
        write_file("l6seq_b.l6d", 195, 0, 0, tb, 9);
        build(in, 2, 0, seq, chof);
        CHECK(same_segs(seq, e, 2), "(4) stay on A, B from A's gap on, stay on B");
        free_seq(seq);
    }
    /* (5) l6mrg on: pattern channels */
    {
        static const int t194[] = {30, 60}, t196[] = {90}, t199[] = {0, 30, 60, 90};
        const char* in[] = {"l6seq_196.l6d", "l6seq_194.l6d", "l6seq_199.l6d"};
        const seg_t e0[] = {{"l6seq_199.l6d", 0, 4 * NFB * FRM}};
        const seg_t e1[] = {{"l6seq_194.l6d", 0, 2 * NFB * FRM}, {"l6seq_196.l6d", 0, NFB * FRM}};
        write_file("l6seq_194.l6d", 194, 1, 0, t194, 2);
        write_file("l6seq_196.l6d", 196, 1, 0, t196, 1);
        write_file("l6seq_199.l6d", 199, 0, 0, t199, 4);
        nch = build(in, 3, 1, seq, chof);
        CHECK(nch == 2 && chof[0] == 1 && chof[1] == 1 && chof[2] == 0, "(5) pattern of the earliest ST1 on ch0");
        CHECK(same_segs(seq, e0, 1) && same_segs(seq + 1, e1, 2), "(5) channel contents");
        free_seq(seq);

        write_file("l6seq_194.l6d", 194, 1, 0, t199, 4);
        nch = build(in, 3, 1, seq, chof);
        CHECK(nch == 2 && chof[1] == 0 && chof[2] == 1, "(5) tie on the first ST1: the earlier file's pattern on ch0");
        free_seq(seq);
    }
    /* (6) l6mrg off: one channel across patterns */
    {
        static const int t194[] = {90, 120}, t199[] = {0, 30, 60};
        const char* in[] = {"l6seq_194.l6d", "l6seq_199.l6d"};
        const seg_t e[] = {{"l6seq_199.l6d", 0, 3 * NFB * FRM}, {"l6seq_194.l6d", 0, 2 * NFB * FRM}};
        write_file("l6seq_194.l6d", 194, 1, 0, t194, 2);
        write_file("l6seq_199.l6d", 199, 0, 0, t199, 3);
        nch = build(in, 2, 0, seq, chof);
        CHECK(nch == 1 && chof[0] == 0 && chof[1] == 0 && seq[1].nseg == 0, "(6) all files on ch0");
        CHECK(same_segs(seq, e, 2), "(6) time order across patterns");
        free_seq(seq);
    }
    /* (7) unusable files */
    {
        static const int ta[] = {0, 30};
        const char* in[] = {"l6seq_mdc.l6d", "l6seq_nost1.l6d", "l6seq_a.l6d"};
        FILE* fp = fopen("l6seq_mdc.l6d", "wb");
        int i;
        for (i = 0; i < 5; i++) {
            put_frame(fp, 200, 2, 0, 1, 1, 0); /* MADOCA vendor ID */
        }
        fclose(fp);
        write_file("l6seq_nost1.l6d", 195, 0, 7, ta, 0);
        write_file("l6seq_a.l6d", 199, 0, 0, ta, 2);
        nch = build(in, 3, 0, seq, chof);
        CHECK(nch == 1 && chof[0] == -1 && chof[1] == -1 && chof[2] == 0, "(7) no CLAS frame / no ST1: not used");
        free_seq(seq);
    }
    /* (8) week rollover */
    {
        static const int ta[] = {604740, 604770, 0}, tb[] = {30, 60};
        const char* in[] = {"l6seq_b.l6d", "l6seq_a.l6d"};
        const seg_t e[] = {{"l6seq_a.l6d", 0, 3 * NFB * FRM}, {"l6seq_b.l6d", 0, 2 * NFB * FRM}};
        write_file("l6seq_a.l6d", 199, 0, 0, ta, 3);
        write_file("l6seq_b.l6d", 195, 0, 0, tb, 2);
        clas_l6_scan_file("l6seq_a.l6d", &f);
        CHECK(f.nblk == 3 && f.blk[2].t == 604800, "(8) rollover inside a file");
        clas_l6_free_file(&f);
        build(in, 2, 0, seq, chof);
        CHECK(same_segs(seq, e, 2), "(8) the next week's file after the old week's");
        free_seq(seq);
    }
    /* (9) one-file channel */
    {
        static const int ta[] = {0, 30, 20};
        const char* in[] = {"l6seq_194.l6d", "l6seq_a.l6d"};
        const seg_t e[] = {{"l6seq_a.l6d", 0, (2 + 3 * NFB) * FRM}};
        write_file("l6seq_a.l6d", 199, 0, 2, ta, 3);
        nch = build(in, 2, 1, seq, chof);
        CHECK(nch == 2 && chof[1] == 0 && same_segs(seq, e, 1), "(9) whole file");
        free_seq(seq);
    }
    /* (10) decoder input: A, B, A reach clas_input_cssr() in that order. A
     * call reads at most 4096 bytes, B is longer, so B is seen between calls */
    {
        static const int ta[] = {0, 30, 60, 270, 300}, tb[] = {90, 120, 150, 180, 210, 240};
        const char* in[] = {"l6seq_a.l6d", "l6seq_b.l6d"};
        clas_ctx_t* ctx = (clas_ctx_t*)calloc(1, sizeof(clas_ctx_t));
        int nrun = 0, prn[16], ret, ncall = 0;
        write_file("l6seq_a.l6d", 199, 0, 0, ta, 5);
        write_file("l6seq_b.l6d", 195, 0, 0, tb, 6);
        build(in, 2, 0, seq, chof);
        clas_ctx_init(ctx);
        do {
            ret = clas_l6seq_input(ctx, seq, 0);
            if (nrun < 16 && ctx->l6delivery[0] > 0 && (nrun == 0 || prn[nrun - 1] != ctx->l6delivery[0])) {
                prn[nrun++] = ctx->l6delivery[0];
            }
        } while (ret != -2 && ++ncall < 100000);
        CHECK(ret == -2, "(10) reads to the end of the channel");
        CHECK(nrun == 3 && prn[0] == 199 && prn[1] == 195 && prn[2] == 199, "(10) decoder sees A, B, A");
        CHECK(clas_l6seq_input(ctx, seq, 0) == -2, "(10) end stays end");
        free_seq(seq);
        clas_ctx_free(ctx);
        free(ctx);
    }
    /* (11) lost ST1: A's block 30 has 60 frames (the ST1 at 60 lost) */
    {
        static const int ta[] = {0, 30, 90}, na[] = {30, 60, 30}, tb[] = {0, 30, 60, 90, 120};
        static const int tc[] = {0, 30, 90}, nc[] = {30, 28, 30};
        static const int nb[] = {30, 30, 30, 30, 30};
        const char* in[] = {"l6seq_a.l6d", "l6seq_b.l6d"};
        const seg_t e[] = {{"l6seq_a.l6d", 0, 120 * FRM}, {"l6seq_b.l6d", 120 * FRM, 150 * FRM}};
        const seg_t e2[] = {{"l6seq_a.l6d", 0, 58 * FRM}, {"l6seq_b.l6d", 60 * FRM, 150 * FRM}};
        write_file_n("l6seq_a.l6d", 199, 0, 0, ta, 3, na);
        write_file_n("l6seq_b.l6d", 195, 0, 0, tb, 5, nb);
        clas_l6_scan_file("l6seq_a.l6d", &f);
        CHECK(f.nblk == 3 && f.blk[0].nfrm == 30 && f.blk[1].nfrm == 60 && f.blk[2].nfrm == 30,
              "(11) frames per block");
        clas_l6_free_file(&f);
        build(in, 2, 0, seq, chof);
        CHECK(same_segs(seq, e, 2), "(11) B's block 60 skipped, A kept at 90 (tie), B after A ends");
        free_seq(seq);
        /* the same span with 2 frames lost and no ST1 lost: B fills from 60 */
        write_file_n("l6seq_a.l6d", 199, 0, 0, tc, 3, nc);
        build(in, 2, 0, seq, chof);
        CHECK(same_segs(seq, e2, 2), "(11) a 2-frame shortfall does not hide B's block 60");
        free_seq(seq);
    }
    /* (12) A: 0, 30 (60 frames, end 60); B: lead frames, first ST1 at 60 / 70 */
    {
        static const int ta[] = {0, 30}, na[] = {30, 30}, tb[] = {60, 90}, tb2[] = {70, 100}, nb[] = {30, 30};
        const char* in[] = {"l6seq_a.l6d", "l6seq_b.l6d"};
        const seg_t e[] = {{"l6seq_a.l6d", 0, 60 * FRM}, {"l6seq_b.l6d", 15 * FRM, 75 * FRM}};
        const seg_t e2[] = {{"l6seq_a.l6d", 0, 60 * FRM}, {"l6seq_b.l6d", 0, 65 * FRM}};
        write_file_n("l6seq_a.l6d", 199, 0, 0, ta, 2, na);
        write_file_n("l6seq_b.l6d", 195, 0, 15, tb, 2, nb); /* lead dated 45..59 */
        clas_l6_scan_file("l6seq_b.l6d", &f);
        CHECK(f.nlead == 15 && f.st1off == 15 * FRM && f.blk[0].off == 0 && f.blk[0].nfrm == 30,
              "(12) leading frames and first ST1 offset");
        clas_l6_free_file(&f);
        build(in, 2, 0, seq, chof);
        CHECK(same_segs(seq, e, 2), "(12) overlapping leading frames dropped");
        free_seq(seq);
        write_file_n("l6seq_b.l6d", 195, 0, 5, tb2, 2, nb); /* lead dated 65..69 */
        build(in, 2, 0, seq, chof);
        CHECK(same_segs(seq, e2, 2), "(12) leading frames after the end kept");
        free_seq(seq);
    }
    remove("l6seq_scan.l6d");
    remove("l6seq_a.l6d");
    remove("l6seq_b.l6d");
    remove("l6seq_194.l6d");
    remove("l6seq_196.l6d");
    remove("l6seq_199.l6d");
    remove("l6seq_mdc.l6d");
    remove("l6seq_nost1.l6d");

    printf("%s (%d failure%s)\n", fails ? "FAILED" : "PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
