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
 *               For SBF input it also extracts Galileo HAS pages from
 *               GALRawCNAV blocks (block ID 4024) into a single .has file in
 *               the 64-byte record format of docs/design/gal-has.md §3.
 *
 *               Parses SBF/UBX framing directly without calling the full
 *               receiver decoders, avoiding SSR decode side-effects.
 *
 *-----------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp */

#include "mrtklib/mrtk_cli.h"

/* byte extraction macros (little-endian, matching RTKLIB convention) */
#define U1(p) ((uint8_t)(p)[0])
#define U2(p) ((uint16_t)(p)[0] | ((uint16_t)(p)[1] << 8))
#define U4(p) ((uint32_t)(p)[0] | ((uint32_t)(p)[1] << 8) | ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24))

/* constants */
#define L6_FRAME_LEN 250    /* L6 payload size (bytes) */
#define SBF_SYNC_1 0x24     /* '$' */
#define SBF_SYNC_2 0x40     /* '@' */
#define SBF_QZSRAWL6 4069   /* SBF block ID: QZSRawL6 (L6E) */
#define SBF_QZSRAWL6D 4270  /* SBF block ID: QZSRawL6D (L6D) */
#define SBF_QZSRAWL6E 4271  /* SBF block ID: QZSRawL6E (L6E, mosaic-G5) */
#define SBF_GALRAWCNAV 4024 /* SBF block ID: GALRawCNAV (E6-B C/NAV / HAS) */
#define UBX_SYNC_1 0xB5
#define UBX_SYNC_2 0x62
#define UBX_RXM_QZSSL6_CLS 0x02
#define UBX_RXM_QZSSL6_ID 0x73

#define MINPRNQZS 193
#define MAX_QZS_PRN 17    /* J193..J209 (L6D uses J193..J202, L6E J203..J209) */
#define L6E_PRN_OFFSET 10 /* mosaic-G5 numbers L6E PRNs 10 above the L6D PRN */

/* Galileo HAS constants */
#define GAL_SVID_OFFSET 70        /* GAL PRN = SVID - 70 */
#define MAX_GAL_PRN 40            /* E01..E40 */
#define HAS_PAGE_LEN 56           /* 448-bit HAS page (bytes) */
#define HAS_RECORD_LEN 64         /* .has fixed record size (bytes) */
#define HAS_DUMMY_HEADER 0xAF3BC3 /* 24-bit dummy-page header */
#define TOW_INVALID 0xFFFFFFFFu   /* SBF do-not-use TOW sentinel */

/* format type */
enum { FMT_UNKNOWN = 0, FMT_SBF, FMT_UBX };

/* L6 type */
enum { L6_D = 0, L6_E = 1 };

/* per-PRN output file */
typedef struct {
    FILE* fp;
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

/* Galileo HAS state (SBF GALRawCNAV → single .has file) */
static int has_enable = 1; /* HAS extraction on by default (disable: -no-has) */
static char has_path[512];
static FILE* has_fp = NULL;
static int has_records = 0;            /* records written */
static int has_blocks = 0;             /* GALRawCNAV blocks seen */
static int has_crc_fail = 0;           /* CRCPassed == 0 */
static int has_dummy = 0;              /* dummy pages skipped */
static int has_prn_count[MAX_GAL_PRN]; /* per-PRN valid record count */

/* ---- output file management ---- */

static int prn_index(int prn) {
    int idx = prn - MINPRNQZS;
    if (idx < 0 || idx >= MAX_QZS_PRN) return -1;
    return idx;
}

static int write_frame(int prn, int type, const uint8_t* data) {
    l6_output_t* o;
    int idx = prn_index(prn);
    if (idx < 0) return -1;

    /* check filter */
    if (filter_l6d && type != L6_D) return 0;
    if (filter_l6e && type != L6_E) return 0;

    o = &out[type][idx];

    /* open file on first frame */
    if (!o->fp) {
        snprintf(o->path, sizeof(o->path), "%s_J%d_%s.l6", prefix, prn, type == L6_D ? "l6d" : "l6e");
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
    if (has_fp) {
        fclose(has_fp);
        has_fp = NULL;
    }
}

static void print_stats(void) {
    int t, i;
    int any = 0;

    fprintf(stderr, "\nL6 Frame Extraction Summary:\n");
    fprintf(stderr, "  %-6s %-5s %8s %10s  %s\n", "PRN", "Type", "Frames", "Bytes", "File");
    fprintf(stderr, "  %-6s %-5s %8s %10s  %s\n", "------", "-----", "--------", "----------", "----");

    for (t = 0; t < 2; t++) {
        for (i = 0; i < MAX_QZS_PRN; i++) {
            if (out[t][i].count > 0) {
                fprintf(stderr, "  J%-5d %-5s %8d %10d  %s\n", i + MINPRNQZS, t == L6_D ? "L6D" : "L6E",
                        out[t][i].count, out[t][i].count * L6_FRAME_LEN, out[t][i].path);
                any = 1;
            }
        }
    }

    if (!any) {
        fprintf(stderr, "  (no L6 frames found)\n");
    }
    fprintf(stderr, "\n  Total: %d frames extracted, %d skipped (parity/status error)\n", total_frames, total_skipped);
}

static void print_has_stats(void) {
    int i;

    if (!has_enable || has_blocks == 0) return;

    fprintf(stderr, "\nGalileo HAS Extraction Summary:\n");
    if (has_records > 0) {
        fprintf(stderr, "  %-6s %8s\n", "PRN", "Records");
        fprintf(stderr, "  %-6s %8s\n", "------", "--------");
        for (i = 0; i < MAX_GAL_PRN; i++) {
            if (has_prn_count[i] > 0) {
                fprintf(stderr, "  E%-5d %8d\n", i + 1, has_prn_count[i]);
            }
        }
        fprintf(stderr, "\n  File: %s (%d records x %d bytes = %ld bytes)\n", has_path, has_records, HAS_RECORD_LEN,
                (long)has_records * HAS_RECORD_LEN);
    } else {
        fprintf(stderr, "  (no valid HAS pages found)\n");
    }
    fprintf(stderr, "  GALRawCNAV blocks: %d, %d CRC-failed, %d dummy pages, %d valid records\n", has_blocks,
            has_crc_fail, has_dummy, has_records);
}

/* ---- SBF L6 extraction ---- */

/**
 * @brief Extract 250-byte L6 payload from SBF block data.
 *
 * SBF L6 blocks store 63 x 4-byte words in big-endian order starting at
 * offset +20 from block start (+12 from TOW). First 250 bytes are the
 * L6 payload.
 */
static void sbf_extract_l6_payload(const uint8_t* block, uint8_t* payload) {
    const uint8_t* p = block + 20; /* offset to data words */
    int i, j;
    uint32_t w;
    /* SBF stores L6 words as 32-bit big-endian values in a little-endian
     * block. U4() reads 4 bytes as little-endian uint32, then we extract
     * bytes MSB-first. The data area is 63 x 4-byte words = 252 bytes, but
     * the L6 payload is only 250 bytes (the final 2 bytes are SBF 4-byte
     * alignment padding). Unpack the first 62 words fully, then only the top
     * 2 bytes of word 63 to avoid writing past payload[249]. */
    for (i = 0, j = 0; i < 62; i++, j += 4) {
        w = U4(p + i * 4);
        payload[j] = (w >> 24) & 0xFF;
        payload[j + 1] = (w >> 16) & 0xFF;
        payload[j + 2] = (w >> 8) & 0xFF;
        payload[j + 3] = w & 0xFF;
    }
    w = U4(p + 62 * 4);
    payload[248] = (w >> 24) & 0xFF;
    payload[249] = (w >> 16) & 0xFF;
}

/* ---- SBF Galileo HAS extraction ---- */

/**
 * @brief Extract one 64-byte .has record from an SBF GALRawCNAV block.
 *
 * Payload (after the 8-byte SBF header) is: TOW u4, WNc u2, SVID u1,
 * CRCPassed u1, ViterbiCnt u1, Source u1, FreqNr u1, RxChannel u1,
 * NAVBits u4[16]. The 16 little-endian words are concatenated MSB-first into
 * 512 bits; the 448-bit HAS page is bits 14..461 (56 bytes). Dummy pages
 * (24-bit header 0xAF3BC3), CRC failures, and TOW sentinels are skipped.
 */
static void sbf_extract_has_page(const uint8_t* block) {
    uint8_t bits[64]; /* 512 bits, MSB-first */
    uint8_t page[HAS_PAGE_LEN];
    uint8_t rec[HAS_RECORD_LEN];
    uint32_t tow, hdr24;
    uint16_t wnc;
    uint8_t svid, crc_ok;
    int prn, i, srcbit, b;

    has_blocks++;

    tow = U4(block + 8);
    wnc = U2(block + 12);
    svid = U1(block + 14);
    crc_ok = U1(block + 15);

    if (tow == TOW_INVALID) return;
    if (crc_ok == 0) {
        has_crc_fail++;
        return;
    }

    prn = (int)svid - GAL_SVID_OFFSET;
    if (prn < 1 || prn > MAX_GAL_PRN) return;

    /* concatenate 16 NAVBits words MSB-first into a 512-bit buffer */
    for (i = 0; i < 16; i++) {
        uint32_t w = U4(block + 20 + i * 4);
        bits[i * 4] = (w >> 24) & 0xFF;
        bits[i * 4 + 1] = (w >> 16) & 0xFF;
        bits[i * 4 + 2] = (w >> 8) & 0xFF;
        bits[i * 4 + 3] = w & 0xFF;
    }

    /* HAS page = bits 14..461 (448 bits = 56 bytes), MSB-first */
    for (i = 0; i < HAS_PAGE_LEN * 8; i++) {
        srcbit = 14 + i;
        b = (bits[srcbit >> 3] >> (7 - (srcbit & 7))) & 1;
        if (b)
            page[i >> 3] |= (uint8_t)(1 << (7 - (i & 7)));
        else
            page[i >> 3] &= (uint8_t)~(1 << (7 - (i & 7)));
    }

    /* skip dummy pages (24-bit HAS header == 0xAF3BC3) */
    hdr24 = ((uint32_t)page[0] << 16) | ((uint32_t)page[1] << 8) | page[2];
    if (hdr24 == HAS_DUMMY_HEADER) {
        has_dummy++;
        return;
    }

    /* open output on first valid page */
    if (!has_fp) {
        has_fp = fopen(has_path, "wb");
        if (!has_fp) {
            fprintf(stderr, "Error: cannot open %s\n", has_path);
            return;
        }
    }

    /* assemble 64-byte little-endian record: tow_ms u4, wnc u2, prn u1,
     * flags u1, page u8[56] */
    rec[0] = tow & 0xFF;
    rec[1] = (tow >> 8) & 0xFF;
    rec[2] = (tow >> 16) & 0xFF;
    rec[3] = (tow >> 24) & 0xFF;
    rec[4] = wnc & 0xFF;
    rec[5] = (wnc >> 8) & 0xFF;
    rec[6] = (uint8_t)prn;
    rec[7] = 0; /* flags reserved */
    memcpy(rec + 8, page, HAS_PAGE_LEN);

    if (fwrite(rec, 1, HAS_RECORD_LEN, has_fp) != HAS_RECORD_LEN) {
        fprintf(stderr, "Error: write failed to %s\n", has_path);
        return;
    }
    has_records++;
    has_prn_count[prn - 1]++;
}

static int process_sbf(FILE* fp) {
    uint8_t hdr[8];
    uint8_t* block = NULL;
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

        id = U2(hdr + 4) & 0x1FFF; /* block ID (13 bits) */
        len = U2(hdr + 6);         /* block length */

        if (len < 8 || len > 65535) {
            synced = 0;
            continue;
        }

        /* read rest of block */
        block = realloc(block, len);
        if (!block) {
            fprintf(stderr, "Error: out of memory\n");
            return -1;
        }
        memcpy(block, hdr, 8);
        if (fread(block + 8, 1, len - 8, fp) != (size_t)(len - 8)) break;

        synced = 0; /* resync after each block */

        /* Galileo HAS pages from GALRawCNAV (block 4024).
         * Payload needs 8 hdr + 12 fields + 64 NAVBits = 84 bytes. */
        if (id == SBF_GALRAWCNAV) {
            if (has_enable && len >= 84) sbf_extract_has_page(block);
            continue;
        }

        /* check for L6 blocks (4069/4271 = L6E, 4270 = L6D) */
        if (id != SBF_QZSRAWL6 && id != SBF_QZSRAWL6D && id != SBF_QZSRAWL6E) continue;
        if (len < 272) continue;

        type = (id == SBF_QZSRAWL6D) ? L6_D : L6_E;
        svid = U1(block + 14);
        parity = U1(block + 15);
        prn = svid - 180 + MINPRNQZS - 1;

        /* mosaic-G5 QZSRawL6E (4271) carries the same SVID as the L6D block;
         * its L6E stream is published under a PRN offset by +10 (J204/J205/J209
         * for J194/J195/J199) to match the MADOCA .l6 PRN convention. */
        if (id == SBF_QZSRAWL6E) prn += L6E_PRN_OFFSET;

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

static int process_ubx(FILE* fp) {
    uint8_t hdr[6];
    uint8_t* msg = NULL;
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
        len = U2(hdr + 4); /* payload length */

        if (len > 8192) {
            synced = 0;
            continue;
        }

        /* read payload + 2-byte checksum */
        msg = realloc(msg, len + 2);
        if (!msg) {
            fprintf(stderr, "Error: out of memory\n");
            return -1;
        }
        if (fread(msg, 1, len + 2, fp) != (size_t)(len + 2)) break;

        synced = 0;

        /* check for RXM-QZSSL6 */
        if (cls != UBX_RXM_QZSSL6_CLS || hdr[3] != UBX_RXM_QZSSL6_ID) continue;
        if (len < 264) continue; /* minimum: 14 header + 250 payload */

        prn = U1(msg + 1) + 192;
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

static int detect_format(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return FMT_UNKNOWN;
    if (!strcasecmp(ext, ".sbf")) return FMT_SBF;
    if (!strcasecmp(ext, ".ubx")) return FMT_UBX;
    return FMT_UNKNOWN;
}

/* ---- CLI ---- */

/* long-option aliases */
static const mrtk_optmap_t opt_aliases[] = {
    {"--input", "-in"},
    {"--output", "-o"},
    {NULL, NULL},
};

static void print_usage(void) {
    static const char* lines[] = {
        "mrtk l6extract: extract QZSS L6 frames and Galileo HAS pages from SBF/UBX files",
        "",
        "Usage: mrtk l6extract [OPTIONS] [FILE]",
        "",
        "Options:",
        "  -in, --input  FILE     Input SBF or UBX binary file (required)",
        "  -r   FORMAT            Receiver format: sbf, ubx (auto-detect)",
        "  -o,  --output PREFIX   Output file prefix                  [\"l6\"]",
        "  -l6d                   Extract L6D frames only (CLAS)",
        "  -l6e                   Extract L6E frames only (MADOCA)",
        "  -no-has                Disable Galileo HAS extraction",
        "  -h,  --help            Show this help",
        "",
        "Output files:",
        "  {prefix}_J{PRN}_{l6d|l6e}.l6  Concatenated 250-byte QZSS L6 frames.",
        "  {input-basename}.has          Galileo HAS pages from SBF GALRawCNAV",
        "                                (block 4024), 64-byte records per",
        "                                docs/design/gal-has.md §3, all PRNs in",
        "                                one file. SBF input only; on by default.",
        "",
        "Examples:",
        "  mrtk l6extract --input rover.sbf --output session1",
        "  mrtk l6extract rover.sbf            # L6 + rover.has",
        NULL,
    };
    int i;
    for (i = 0; lines[i]; i++) {
        fprintf(stderr, "%s\n", lines[i]);
    }
}

int mrtk_l6extract(int argc, char** argv) {
    const char* infile = NULL;
    int fmt = FMT_UNKNOWN;
    int i;
    FILE* fp;

    memset(out, 0, sizeof(out));

    /* translate --long flags to their -short aliases before parsing */
    mrtk_normalize_args(argc, argv, opt_aliases);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-in") && i + 1 < argc) {
            infile = argv[++i];
        } else if (!strcmp(argv[i], "-r") && i + 1 < argc) {
            i++;
            if (!strcasecmp(argv[i], "sbf"))
                fmt = FMT_SBF;
            else if (!strcasecmp(argv[i], "ubx"))
                fmt = FMT_UBX;
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
        } else if (!strcmp(argv[i], "-no-has")) {
            has_enable = 0;
        } else if (argv[i][0] != '-' && !infile) {
            infile = argv[i];
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

    if (filter_l6d && filter_l6e) {
        fprintf(stderr,
                "Error: -l6d and -l6e are mutually exclusive\n"
                "       (specifying both filters out every frame).\n"
                "       Omit both flags to extract L6D and L6E together.\n");
        return 1;
    }

    /* auto-detect format if not specified */
    if (fmt == FMT_UNKNOWN) {
        fmt = detect_format(infile);
        if (fmt == FMT_UNKNOWN) {
            fprintf(stderr, "Error: cannot determine format from '%s'. Use -r sbf|ubx\n", infile);
            return 1;
        }
    }

    fp = fopen(infile, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open '%s'\n", infile);
        return 1;
    }

    fprintf(stderr, "l6extract: %s (%s)\n", infile, fmt == FMT_SBF ? "SBF" : "UBX");

    /* HAS output path: <input-basename>.has (UBX carries no GALRawCNAV) */
    if (has_enable && fmt == FMT_SBF) {
        const char* base = strrchr(infile, '/');
        const char* dot;
        base = base ? base + 1 : infile;
        dot = strrchr(base, '.');
        if (dot && dot != base)
            snprintf(has_path, sizeof(has_path), "%.*s.has", (int)(dot - base), base);
        else
            snprintf(has_path, sizeof(has_path), "%s.has", base);
    } else {
        has_enable = 0;
    }

    if (fmt == FMT_SBF) {
        process_sbf(fp);
    } else {
        process_ubx(fp);
    }

    fclose(fp);
    close_all();
    print_stats();
    print_has_stats();

    return 0;
}
