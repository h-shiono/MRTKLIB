/*------------------------------------------------------------------------------
 * l6extract.c : extract QZSS L6 frames from SBF/UBX binary files
 *
 * Copyright (C) 2026, MRTKLIB Contributors, All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * description : Extracts L6D (CLAS) and/or L6E (MADOCA) raw 250-byte frames
 *               from Septentrio SBF or u-blox UBX binary files, writing
 *               per-PRN/per-type .l6 files compatible with clas_input_cssrf().
 *
 *               Parses SBF/UBX framing directly without calling the full
 *               receiver decoders, avoiding SSR decode side-effects.
 *
 *-----------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* byte extraction macros (little-endian, matching RTKLIB convention) */
#define U1(p) ((uint8_t)(p)[0])
#define U2(p) ((uint16_t)(p)[0] | ((uint16_t)(p)[1] << 8))
#define U4(p) ((uint32_t)(p)[0] | ((uint32_t)(p)[1] << 8) | \
               ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24))

/* constants */
#define L6_FRAME_LEN   250       /* L6 payload size (bytes) */
#define SBF_SYNC_1     0x24      /* '$' */
#define SBF_SYNC_2     0x40      /* '@' */
#define SBF_QZSRAWL6   4069      /* SBF block ID: QZSRawL6 (L6E) */
#define SBF_QZSRAWL6D  4270      /* SBF block ID: QZSRawL6D (L6D) */
#define UBX_SYNC_1     0xB5
#define UBX_SYNC_2     0x62
#define UBX_RXM_QZSSL6_CLS 0x02
#define UBX_RXM_QZSSL6_ID  0x73

#define MINPRNQZS      193
#define MAX_QZS_PRN    10        /* J193..J202 */

/* format type */
enum { FMT_UNKNOWN = 0, FMT_SBF, FMT_UBX };

/* L6 type */
enum { L6_D = 0, L6_E = 1 };

/* per-PRN output file */
typedef struct {
    FILE *fp;
    int count;
    char path[512];
} l6_output_t;

/* global state */
static l6_output_t out[2][MAX_QZS_PRN]; /* [L6_D/L6_E][prn-MINPRNQZS] */
static char prefix[256] = "l6";
static int filter_l6d = 0; /* -l6d flag */
static int filter_l6e = 0; /* -l6e flag */
static int total_frames = 0;
static int total_skipped = 0;

/* ---- output file management ---- */

static int prn_index(int prn) {
    int idx = prn - MINPRNQZS;
    if (idx < 0 || idx >= MAX_QZS_PRN) return -1;
    return idx;
}

static int write_frame(int prn, int type, const uint8_t *data) {
    l6_output_t *o;
    int idx = prn_index(prn);
    if (idx < 0) return -1;

    /* check filter */
    if (filter_l6d && type != L6_D) return 0;
    if (filter_l6e && type != L6_E) return 0;

    o = &out[type][idx];

    /* open file on first frame */
    if (!o->fp) {
        snprintf(o->path, sizeof(o->path), "%s_J%d_%s.l6",
                 prefix, prn, type == L6_D ? "l6d" : "l6e");
        o->fp = fopen(o->path, "wb");
        if (!o->fp) {
            fprintf(stderr, "Error: cannot open %s\n", o->path);
            return -1;
        }
    }

    if (fwrite(data, 1, L6_FRAME_LEN, o->fp) != L6_FRAME_LEN) {
        fprintf(stderr, "Error: write failed to %s\n", o->path);
        return -1;
    }
    o->count++;
    total_frames++;
    return 0;
}

static void close_all(void) {
    int t, i;
    for (t = 0; t < 2; t++) {
        for (i = 0; i < MAX_QZS_PRN; i++) {
            if (out[t][i].fp) fclose(out[t][i].fp);
        }
    }
}

static void print_stats(void) {
    int t, i;
    int any = 0;

    fprintf(stderr, "\nL6 Frame Extraction Summary:\n");
    fprintf(stderr, "  %-6s %-5s %8s %10s  %s\n",
            "PRN", "Type", "Frames", "Bytes", "File");
    fprintf(stderr, "  %-6s %-5s %8s %10s  %s\n",
            "------", "-----", "--------", "----------", "----");

    for (t = 0; t < 2; t++) {
        for (i = 0; i < MAX_QZS_PRN; i++) {
            if (out[t][i].count > 0) {
                fprintf(stderr, "  J%-5d %-5s %8d %10d  %s\n",
                        i + MINPRNQZS,
                        t == L6_D ? "L6D" : "L6E",
                        out[t][i].count,
                        out[t][i].count * L6_FRAME_LEN,
                        out[t][i].path);
                any = 1;
            }
        }
    }

    if (!any) {
        fprintf(stderr, "  (no L6 frames found)\n");
    }
    fprintf(stderr, "\n  Total: %d frames extracted, %d skipped (parity/status error)\n",
            total_frames, total_skipped);
}

/* ---- SBF L6 extraction ---- */

/**
 * @brief Extract 250-byte L6 payload from SBF block data.
 *
 * SBF L6 blocks store 63 x 4-byte words in big-endian order starting at
 * offset +20 from block start (+12 from TOW). First 250 bytes are the
 * L6 payload.
 */
static void sbf_extract_l6_payload(const uint8_t *block, uint8_t *payload) {
    const uint8_t *p = block + 20; /* offset to data words */
    int i, j;
    for (i = 0, j = 0; i < 63; i++, j += 4) {
        /* SBF stores L6 words as 32-bit big-endian values in a
         * little-endian block. U4() reads 4 bytes as little-endian uint32,
         * then we extract bytes MSB-first. */
        uint32_t w = U4(p + i * 4);
        payload[j]     = (w >> 24) & 0xFF;
        payload[j + 1] = (w >> 16) & 0xFF;
        payload[j + 2] = (w >>  8) & 0xFF;
        payload[j + 3] =  w        & 0xFF;
    }
}

static int process_sbf(FILE *fp) {
    uint8_t hdr[8];
    uint8_t *block = NULL;
    uint8_t payload[L6_FRAME_LEN];
    uint16_t id, len;
    uint8_t svid, parity;
    int type, prn;
    int c, synced = 0;

    while (1) {
        /* sync to "$@" */
        if (!synced) {
            int prev = 0;
            while ((c = fgetc(fp)) != EOF) {
                if (prev == SBF_SYNC_1 && c == SBF_SYNC_2) {
                    synced = 1;
                    break;
                }
                prev = c;
            }
            if (!synced) break; /* EOF */
        }

        /* read remaining 6 bytes of header (sync already consumed) */
        if (fread(hdr + 2, 1, 6, fp) != 6) break;
        hdr[0] = SBF_SYNC_1;
        hdr[1] = SBF_SYNC_2;

        id  = U2(hdr + 4) & 0x1FFF; /* block ID (13 bits) */
        len = U2(hdr + 6);           /* block length */

        if (len < 8 || len > 65535) {
            synced = 0;
            continue;
        }

        /* read rest of block */
        block = realloc(block, len);
        if (!block) { fprintf(stderr, "Error: out of memory\n"); return -1; }
        memcpy(block, hdr, 8);
        if (fread(block + 8, 1, len - 8, fp) != (size_t)(len - 8)) break;

        synced = 0; /* resync after each block */

        /* check for L6 blocks */
        if (id != SBF_QZSRAWL6 && id != SBF_QZSRAWL6D) continue;
        if (len < 272) continue;

        type = (id == SBF_QZSRAWL6D) ? L6_D : L6_E;
        svid   = U1(block + 14);
        parity = U1(block + 15);
        prn = svid - 180 + MINPRNQZS - 1;

        if (prn < MINPRNQZS || prn >= MINPRNQZS + MAX_QZS_PRN) continue;

        if (parity == 0) {
            total_skipped++;
            continue;
        }

        sbf_extract_l6_payload(block, payload);
        write_frame(prn, type, payload);
    }

    free(block);
    return 0;
}

/* ---- UBX L6 extraction ---- */

static int process_ubx(FILE *fp) {
    uint8_t hdr[6];
    uint8_t *msg = NULL;
    uint8_t cls;
    uint16_t len;
    int prn, mtyp, stat, type;
    int c, synced = 0;

    while (1) {
        /* sync to 0xB5 0x62 */
        if (!synced) {
            int prev = 0;
            while ((c = fgetc(fp)) != EOF) {
                if (prev == UBX_SYNC_1 && c == UBX_SYNC_2) {
                    synced = 1;
                    break;
                }
                prev = c;
            }
            if (!synced) break;
        }

        /* read class, ID, length (4 bytes) */
        if (fread(hdr + 2, 1, 4, fp) != 4) break;
        hdr[0] = UBX_SYNC_1;
        hdr[1] = UBX_SYNC_2;

        cls = hdr[2];
        len = U2(hdr + 4);  /* payload length */

        if (len > 8192) {
            synced = 0;
            continue;
        }

        /* read payload + 2-byte checksum */
        msg = realloc(msg, len + 2);
        if (!msg) { fprintf(stderr, "Error: out of memory\n"); return -1; }
        if (fread(msg, 1, len + 2, fp) != (size_t)(len + 2)) break;

        synced = 0;

        /* check for RXM-QZSSL6 */
        if (cls != UBX_RXM_QZSSL6_CLS || hdr[3] != UBX_RXM_QZSSL6_ID) continue;
        if (len < 264) continue; /* minimum: 14 header + 250 payload */

        prn  = U1(msg + 1) + 192;
        mtyp = (U2(msg + 10) >> 10) & 1; /* 0=L6D, 1=L6E */
        stat = (U2(msg + 10) >> 12) & 3; /* 1=error-free */

        if (prn < MINPRNQZS || prn >= MINPRNQZS + MAX_QZS_PRN) continue;

        if (stat != 1) {
            total_skipped++;
            continue;
        }

        type = (mtyp == 0) ? L6_D : L6_E;
        write_frame(prn, type, msg + 14);
    }

    free(msg);
    return 0;
}

/* ---- format detection ---- */

static int detect_format(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return FMT_UNKNOWN;
    if (!strcasecmp(ext, ".sbf")) return FMT_SBF;
    if (!strcasecmp(ext, ".ubx")) return FMT_UBX;
    return FMT_UNKNOWN;
}

/* ---- CLI ---- */

static void print_usage(void) {
    fprintf(stderr,
            "l6extract: Extract QZSS L6 frames from SBF/UBX files\n"
            "\n"
            "Usage: mrtk l6extract [OPTIONS]\n"
            "\n"
            "Options:\n"
            "  -in FILE     Input SBF or UBX binary file (required)\n"
            "  -r FORMAT    Receiver format: sbf, ubx (auto-detect from extension)\n"
            "  -o PREFIX    Output file prefix (default: \"l6\")\n"
            "  -l6d         Extract L6D frames only (CLAS)\n"
            "  -l6e         Extract L6E frames only (MADOCA)\n"
            "  -h           Show this help\n"
            "\n"
            "Output files: {prefix}_J{PRN}_{l6d|l6e}.l6\n"
            "  Each file contains concatenated 250-byte L6 frames.\n");
}

int mrtk_l6extract(int argc, char **argv) {
    const char *infile = NULL;
    int fmt = FMT_UNKNOWN;
    int i;
    FILE *fp;

    memset(out, 0, sizeof(out));

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-in") && i + 1 < argc) {
            infile = argv[++i];
        } else if (!strcmp(argv[i], "-r") && i + 1 < argc) {
            i++;
            if (!strcasecmp(argv[i], "sbf")) fmt = FMT_SBF;
            else if (!strcasecmp(argv[i], "ubx")) fmt = FMT_UBX;
            else {
                fprintf(stderr, "Error: unknown format '%s'\n", argv[i]);
                return 1;
            }
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            snprintf(prefix, sizeof(prefix), "%s", argv[++i]);
        } else if (!strcmp(argv[i], "-l6d")) {
            filter_l6d = 1;
        } else if (!strcmp(argv[i], "-l6e")) {
            filter_l6e = 1;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage();
            return 0;
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    if (!infile) {
        fprintf(stderr, "Error: -in FILE is required\n\n");
        print_usage();
        return 1;
    }

    /* auto-detect format if not specified */
    if (fmt == FMT_UNKNOWN) {
        fmt = detect_format(infile);
        if (fmt == FMT_UNKNOWN) {
            fprintf(stderr, "Error: cannot determine format from '%s'. Use -r sbf|ubx\n",
                    infile);
            return 1;
        }
    }

    fp = fopen(infile, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open '%s'\n", infile);
        return 1;
    }

    fprintf(stderr, "l6extract: %s (%s)\n", infile, fmt == FMT_SBF ? "SBF" : "UBX");

    if (fmt == FMT_SBF) {
        process_sbf(fp);
    } else {
        process_ubx(fp);
    }

    fclose(fp);
    close_all();
    print_stats();

    return 0;
}
