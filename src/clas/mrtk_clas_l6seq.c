/*------------------------------------------------------------------------------
 * mrtk_clas_l6seq.c : CLAS L6 channels from several L6 frame files
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_clas_l6seq.c
 * @brief CLAS L6 channels from several L6 frame files (post-processing, #373).
 *
 * References:
 *   [1] IS-QZSS-L6-007, QZSS L6 Signal CLAS IF Specification (ST1 mask message)
 *
 * Notes:
 *   - Post-processing counterpart of the real-time pattern-keyed L6 demux
 *     (clas_route_l6frame()): files of one transmit pattern form one channel.
 *   - Files carry no time tags; the order comes from the ST1 epochs in the
 *     CSSR data, read without the CSSR decoder (fixed header bit positions).
 */
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_clas.h"
#include "mrtklib/mrtk_trace.h"

#define ST1_BIT 49       /* first data bit of an L6 frame (after the 49-bit header) */
#define WEEKSEC 604800.0 /* seconds per week */
#define CSSR_MSGNUM 4073 /* CSSR message number */

/* add a block to a scanned file ---------------------------------------------*/
static int add_block(clas_l6file_t* f, long off, double t) {
    if (f->nblk >= f->nmax) {
        int nmax = f->nmax ? 2 * f->nmax : 256;
        clas_l6blk_t* p = (clas_l6blk_t*)realloc(f->blk, sizeof(clas_l6blk_t) * nmax);
        if (!p) {
            return 0;
        }
        f->blk = p;
        f->nmax = nmax;
    }
    f->blk[f->nblk].off = off;
    f->blk[f->nblk].t = t;
    f->blk[f->nblk].nfrm = 0;
    f->nblk++;
    return 1;
}

extern int clas_l6_scan_file(const char* path, clas_l6file_t* f) {
    uint8_t fr[L6_FRAME_LEN];
    uint32_t preamble = 0;
    l6_header_t hdr;
    long pos = 0, fstart = 0;
    double wrap = 0.0, t;
    int c, nb = 0, tow;
    FILE* fp;

    memset(f, 0, sizeof(*f));
    snprintf(f->path, sizeof(f->path), "%s", path);
    f->prn = f->ptn = -1;

    if (!(fp = fopen(path, "rb"))) {
        return 0;
    }
    /* same frame synchronization as clas_input_cssr(), so the block offsets
     * fall on the frames the decoder sees */
    while ((c = fgetc(fp)) != EOF) {
        pos++;
        if (nb == 0) {
            preamble = (preamble << 8) | (uint32_t)c;
            if (preamble != L6_PREAMBLE) {
                continue;
            }
            preamble = 0;
            fstart = pos - 4;
            fr[0] = (L6_PREAMBLE >> 24) & 0xff;
            fr[1] = (L6_PREAMBLE >> 16) & 0xff;
            fr[2] = (L6_PREAMBLE >> 8) & 0xff;
            fr[3] = (uint8_t)c;
            nb = 4;
            continue;
        }
        fr[nb++] = (uint8_t)c;
        if (nb < L6_FRAME_LEN) {
            continue;
        }
        nb = 0;
        if (f->nblk > 0) {
            f->blk[f->nblk - 1].nfrm++;
        } else {
            f->nlead++;
        }

        l6_parse_header(fr, &hdr);
        if (hdr.svc != L6_SVC_CLAS) {
            continue;
        }
        if (f->ptn < 0) {
            f->ptn = hdr.ptn;
            f->prn = hdr.prn;
        }
        if (!hdr.sf_ind || getbitu(fr, ST1_BIT, 12) != CSSR_MSGNUM || getbitu(fr, ST1_BIT + 12, 4) != CSSR_TYPE_MASK) {
            continue;
        }
        if ((tow = (int)getbitu(fr, ST1_BIT + 16, 20)) >= (int)WEEKSEC) {
            continue;
        }
        t = tow + wrap;
        if (f->nblk > 0 && t < f->blk[f->nblk - 1].t - WEEKSEC / 2.0) { /* week rollover */
            wrap += WEEKSEC;
            t += WEEKSEC;
        }
        if (f->nblk > 0 && t <= f->blk[f->nblk - 1].t) {
            continue;
        }
        if (f->nblk > 0) {
            f->blk[f->nblk - 1].nfrm--; /* the ST1 frame, counted above, starts the new block */
        } else {
            f->nlead--;
            f->st1off = fstart;
        }
        if (!add_block(f, f->nblk == 0 ? 0 : fstart, t)) {
            trace(NULL, 1, "clas_l6_scan_file: memory allocation error, %s scanned up to byte %ld\n", path, pos);
            break;
        }
        f->blk[f->nblk - 1].nfrm = 1; /* the ST1 frame itself */
    }
    f->size = pos;
    fclose(fp);

    trace(NULL, 3, "clas_l6_scan_file: %s prn=%d ptn=%d size=%ld nblk=%d\n", path, f->prn, f->ptn, f->size, f->nblk);
    return 1;
}

extern void clas_l6_free_file(clas_l6file_t* f) {
    free(f->blk);
    f->blk = NULL;
    f->nblk = f->nmax = 0;
}

/* append a byte range to a channel, extending the last segment if contiguous */
static int add_segment(clas_l6seq_t* seq, int file, long start, long end) {
    clas_l6seg_t* s;

    if (start >= end) {
        return 1;
    }
    if (seq->nseg > 0) {
        s = seq->seg + seq->nseg - 1;
        if (s->file == file && s->end == start) {
            s->end = end;
            return 1;
        }
    }
    if (seq->nseg >= seq->nmax) {
        int nmax = seq->nmax ? 2 * seq->nmax : 64;
        clas_l6seg_t* p = (clas_l6seg_t*)realloc(seq->seg, sizeof(clas_l6seg_t) * nmax);
        if (!p) {
            return 0;
        }
        seq->seg = p;
        seq->nmax = nmax;
    }
    s = seq->seg + seq->nseg++;
    s->file = file;
    s->start = start;
    s->end = end;
    return 1;
}

/* build one channel from the files in grp[] (indexes into files[]) ----------*/
static void build_channel(const clas_l6file_t* files, const double* shift, const int* grp, int ng, clas_l6seq_t* seq) {
    int idx[CLAS_L6SEQ_MAXFILE] = {0}, k, j, cur = -1, best;
    double cov = 0.0, tb, tk; /* end of the last block read */
    int started = 0;

    for (k = 0; k < ng; k++) {
        snprintf(seq->path[k], sizeof(seq->path[k]), "%s", files[grp[k]].path);
    }
    seq->npath = ng;

    if (ng == 1) {
        add_segment(seq, 0, 0, files[grp[0]].size);
        return;
    }
    for (;;) {
        for (k = 0; k < ng; k++) { /* skip blocks of other files already covered */
            const clas_l6file_t* f = files + grp[k];
            while (started && k != cur && idx[k] < f->nblk && f->blk[idx[k]].t + shift[grp[k]] < cov - 0.5) {
                idx[k]++;
            }
        }
        best = (cur >= 0 && idx[cur] < files[grp[cur]].nblk) ? cur : -1;
        for (k = 0; k < ng; k++) {
            const clas_l6file_t* f = files + grp[k];
            if (k == cur || idx[k] >= f->nblk) {
                continue;
            }
            tk = f->blk[idx[k]].t + shift[grp[k]];
            tb = best >= 0 ? files[grp[best]].blk[idx[best]].t + shift[grp[best]] : 0.0;
            if (best < 0 || tk < tb) {
                best = k;
            }
        }
        if (best < 0) {
            break;
        }
        {
            const clas_l6file_t* f = files + grp[best];
            long start;
            j = idx[best];
            if (best != cur) {
                trace(NULL, 2, "clas l6 channel: %s from block %.0f\n", f->path, f->blk[j].t + shift[grp[best]]);
            }
            start = f->blk[j].off;
            if (j == 0 && started && f->blk[0].t + shift[grp[best]] - f->nlead < cov - 0.5) {
                /* frames before the first ST1 overlap what was read: drop them */
                trace(NULL, 2, "clas l6 channel: %s, %d leading frames dropped\n", f->path, f->nlead);
                start = f->st1off;
            }
            if (!add_segment(seq, best, start, j + 1 < f->nblk ? f->blk[j + 1].off : f->size)) {
                trace(NULL, 1, "clas l6 channel: memory allocation error, channel truncated\n");
                return;
            }
            cov = f->blk[j].t + shift[grp[best]] + f->blk[j].nfrm;
            started = 1;
            idx[best]++;
            cur = best;
        }
    }
}

extern int clas_l6seq_build(const clas_l6file_t* files, int n, int l6mrg, clas_l6seq_t* seq, int* chof) {
    double shift[CLAS_L6SEQ_MAXFILE] = {0}, t0 = 0.0, d;
    int grp[CLAS_L6SEQ_MAXFILE], use[CLAS_L6SEQ_MAXFILE];
    int i, ref = -1, nch = 0, ng;

    for (i = 0; i < CLAS_CH_NUM; i++) {
        memset(seq + i, 0, sizeof(clas_l6seq_t));
        seq[i].fpfile = -1;
    }
    if (n > CLAS_L6SEQ_MAXFILE) {
        n = CLAS_L6SEQ_MAXFILE;
    }
    for (i = 0; i < n; i++) {
        use[i] = files[i].ptn >= 0 && files[i].nblk > 0;
        if (chof) {
            chof[i] = -1;
        }
        if (!use[i]) {
            trace(NULL, 2, "clas l6: %s has no %s, not used\n", files[i].path,
                  files[i].ptn < 0 ? "CLAS frame" : "ST1 mask");
            continue;
        }
        if (ref < 0) {
            ref = i;
            t0 = files[i].blk[0].t;
        }
        /* align to the reference file within half a week */
        d = files[i].blk[0].t - t0;
        while (d > WEEKSEC / 2.0) {
            d -= WEEKSEC;
            shift[i] -= WEEKSEC;
        }
        while (d < -WEEKSEC / 2.0) {
            d += WEEKSEC;
            shift[i] += WEEKSEC;
        }
    }
    /* channels in order of first appearance (earliest first block, ties to
     * the earlier file); with l6mrg off a single channel takes every file */
    while (nch < (l6mrg ? CLAS_CH_NUM : 1)) {
        int first = -1;
        for (i = 0; i < n; i++) {
            if (use[i] && (first < 0 || files[i].blk[0].t + shift[i] < files[first].blk[0].t + shift[first])) {
                first = i;
            }
        }
        if (first < 0) {
            break;
        }
        for (ng = 0, i = 0; i < n; i++) {
            if (use[i] && (!l6mrg || files[i].ptn == files[first].ptn)) {
                grp[ng++] = i;
                use[i] = 0;
                if (chof) {
                    chof[i] = nch;
                }
                trace(NULL, 2, "clas l6: ch%d <- %s (prn=%d ptn=%d blocks=%d)\n", nch, files[i].path, files[i].prn,
                      files[i].ptn, files[i].nblk);
            }
        }
        build_channel(files, shift, grp, ng, seq + nch);
        nch++;
    }
    for (i = 0; i < n; i++) {
        if (use[i]) {
            trace(NULL, 2, "clas l6: more than %d transmit patterns, %s (ptn=%d) not used\n", CLAS_CH_NUM,
                  files[i].path, files[i].ptn);
        }
    }
    return nch;
}

/* next byte of an assembled channel, EOF at its end -------------------------*/
static int seq_getc(clas_l6seq_t* seq) {
    const clas_l6seg_t* s;
    int c;

    while (seq->iseg < seq->nseg) {
        s = seq->seg + seq->iseg;
        if (seq->fpfile != s->file) {
            if (seq->fp) {
                fclose(seq->fp);
            }
            seq->fp = fopen(seq->path[s->file], "rb");
            seq->fpfile = s->file;
            seq->pos = 0;
            if (seq->fp) {
                trace(NULL, 2, "clas l6 file open: %s\n", seq->path[s->file]);
            }
        }
        if (!seq->fp) {
            seq->iseg++;
            seq->inseg = 0;
            continue;
        }
        if (!seq->inseg) {
            if (seq->pos != s->start && fseek(seq->fp, s->start, SEEK_SET) != 0) {
                seq->iseg++;
                continue;
            }
            seq->pos = s->start;
            seq->inseg = 1;
        }
        if (seq->pos >= s->end || (c = fgetc(seq->fp)) == EOF) {
            seq->iseg++;
            seq->inseg = 0;
            continue;
        }
        seq->pos++;
        return c;
    }
    return EOF;
}

extern int clas_l6seq_input(clas_ctx_t* ctx, clas_l6seq_t* seq, int ch) {
    int i, data, ret;

    for (i = 0; i < 4096; i++) {
        if ((ret = clas_decode_msg(ctx, ch))) {
            return ret;
        }
        if ((data = seq_getc(seq)) == EOF) {
            return -2;
        }
        if ((ret = clas_input_cssr(ctx, (uint8_t)data, ch))) {
            return ret;
        }
    }
    return 0;
}

extern void clas_l6seq_free(clas_l6seq_t* seq) {
    if (seq->fp) {
        fclose(seq->fp);
    }
    free(seq->seg);
    memset(seq, 0, sizeof(*seq));
    seq->fpfile = -1;
}
