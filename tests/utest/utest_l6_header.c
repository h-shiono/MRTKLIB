/*------------------------------------------------------------------------------
 * unit test : QZSS L6 frame header parsing and service classification (#351)
 *
 * Coverage:
 *   (1) field extraction is bit-identical to the getbitu() sequence the CLAS
 *       and MADOCA decoders used before #351, for all 256 message type IDs
 *       and both alert values (the refactor must not change any decoded
 *       header field);
 *   (2) service classification per IS-QZSS-SAS-001 Table 6-8 and
 *       IS-QZSS-MDC-004 Table 4.2.1-3: vendor 101b -> CLAS, 010b/CSID0 ->
 *       MADOCA, 010b/CSID1 -> MADOCA iono, 011b -> QZNMA, others -> UNKNOWN.
 *       The same rule is also checked for every message type ID x alert x
 *       L6 PRN (193..211, incl. 197/200/201 and L6E PRNs), so the
 *       classification is shown to be PRN-independent;
 *   (3) null message (IS-QZSS-L6-008 4.1.2.3) and its near misses;
 *   (4) bad preamble;
 *   (5) one real subframe-head frame per service from mosaic-G5 captures
 *       (SBF blocks 4270/4271): l6_frames.bin holds 4 x 218 bytes in the
 *       order CLAS, MADOCA, MADOCA iono, QZNMA. CLAS/MADOCA/QZNMA come from
 *       G5P3287a (2026-10-13), MADOCA iono (L6D PRN 200) from G5P3186k
 *       (2026-07-05). Regenerate with scripts/tests/extract_l6_frames.py.
 *   (6) l6_file_service(): service of an L6 frame file from the first
 *       non-null frame (used by mrtk post to route .l6d files), incl. a
 *       leading null message, junk before the first preamble, empty and
 *       missing files (in a private mkdtemp() directory);
 *   (7) l6_file_type(): .l6/.l6d/.l6e extension, case-insensitive incl.
 *       mixed case.
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#define _POSIX_C_SOURCE 200809L /* mkdtemp(), rmdir() under -std=c11 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mrtklib/rtklib.h"

static int fails = 0;

#define CHECK(cond, msg)                 \
    do {                                 \
        if (!(cond)) {                   \
            printf("FAIL: %s\n", (msg)); \
            fails++;                     \
        }                                \
    } while (0)

static void make_frame(uint8_t* f, uint8_t prn, uint8_t msgid, int alert, uint8_t fill) {
    memset(f, fill, L6_FRAME_LEN);
    f[0] = 0x1A;
    f[1] = 0xCF;
    f[2] = 0xFC;
    f[3] = 0x1D;
    f[4] = prn;
    f[5] = msgid;
    f[6] = (uint8_t)((alert ? 0x80 : 0x00) | (fill & 0x7F));
}

/* (1) bit-identical to the pre-#351 getbitu() header sequence */
static void test_fields_exhaustive(void) {
    uint8_t f[L6_FRAME_LEN];
    l6_header_t h;
    int msgid, alert, bad = 0;

    for (msgid = 0; msgid < 256; msgid++) {
        for (alert = 0; alert < 2; alert++) {
            int i = 32;
            make_frame(f, 199, (uint8_t)msgid, alert, 0x5C);
            l6_parse_header(f, &h);
            if (h.preamble != getbitu(f, 0, 32)) bad++;
            if (h.prn != getbitu(f, i, 8)) bad++;
            i += 8;
            if (h.msgid != getbitu(f, i, 8)) bad++;
            if (h.vid != getbitu(f, i, 3)) bad++;
            if (h.fac != getbitu(f, i + 3, 2)) bad++;
            if (h.csid != getbitu(f, i + 5, 1)) bad++;
            if (h.anme != getbitu(f, i + 6, 1)) bad++;
            if (h.sf_ind != getbitu(f, i + 7, 1)) bad++;
            if (h.alert != getbitu(f, i + 8, 1)) bad++;
            /* CLAS pattern ID as read by the rtksvr demux before #351 */
            if (h.ptn != ((f[5] & 0x06) >> 1)) bad++;
        }
    }
    CHECK(bad == 0, "fields match getbitu() for all 256 msgid x 2 alert");
    printf("ok  : field extraction exhaustive (%d mismatches)\n", bad);
}

/* (2) service classification */
static void test_classify(void) {
    static const struct {
        uint8_t msgid;
        l6_service_t svc;
        const char* name;
    } c[] = {
        {0xA1, L6_SVC_CLAS, "CLAS 101b fac0 ptn0 sf1"},
        {0xBE, L6_SVC_CLAS, "CLAS 101b fac3 ptn3 sf0"},
        {0x48, L6_SVC_MADOCA, "MADOCA 010b fac1 csid0"},
        {0x5A, L6_SVC_MADOCA, "MADOCA 010b fac3 csid0 anme1"},
        {0x4D, L6_SVC_MADOCA_IONO, "MADOCA 010b csid1 sf1"},
        {0x69, L6_SVC_QZNMA, "QZNMA 011b"},
        {0x20, L6_SVC_UNKNOWN, "reserved 001b"},
        {0xE0, L6_SVC_UNKNOWN, "reserved 111b"},
        {0x00, L6_SVC_UNKNOWN, "msgid 0 without null payload"},
    };
    uint8_t f[L6_FRAME_LEN];
    l6_header_t h;
    size_t k;

    for (k = 0; k < sizeof(c) / sizeof(c[0]); k++) {
        make_frame(f, 195, c[k].msgid, 0, 0x5C);
        CHECK(l6_parse_header(f, &h) == 1, c[k].name);
        CHECK(h.svc == c[k].svc, c[k].name);
    }
    /* alert does not change the service (consumers decide how to use it) */
    make_frame(f, 195, 0xA1, 1, 0x5C);
    l6_parse_header(f, &h);
    CHECK(h.svc == L6_SVC_CLAS && h.alert == 1, "alert CLAS frame still CLAS");
    CHECK(strcmp(l6_service_name(L6_SVC_QZNMA), "QZNMA") == 0, "service name");
    printf("ok  : classification (%d cases)\n", (int)(sizeof(c) / sizeof(c[0])));
}

/* (2b) classification for every message type ID, alert value and L6 PRN:
 * expected service from IS-QZSS-SAS-001 Table 6-8 (vendor ID) and
 * IS-QZSS-MDC-004 Table 4.2.1-3 (CSID), independent of the PRN byte */
static void test_classify_exhaustive(void) {
    static const uint8_t prns[] = {193, 194, 195, 196, 197, 199, 200, 201, 204, 205, 209, 210, 211};
    uint8_t f[L6_FRAME_LEN];
    l6_header_t h;
    int msgid, alert, vid, bad = 0, nnull = 0, seen[6] = {0};
    size_t k;

    for (k = 0; k < sizeof(prns); k++) {
        for (msgid = 0; msgid < 256; msgid++) {
            for (alert = 0; alert < 2; alert++) {
                l6_service_t exp;
                vid = msgid >> 5;
                if (vid == 5) {
                    exp = L6_SVC_CLAS;
                } else if (vid == 2) {
                    exp = (msgid & 0x04) ? L6_SVC_MADOCA_IONO : L6_SVC_MADOCA;
                } else if (vid == 3) {
                    exp = L6_SVC_QZNMA;
                } else {
                    exp = L6_SVC_UNKNOWN; /* reserved 000b/001b/100b/110b/111b */
                }
                make_frame(f, prns[k], (uint8_t)msgid, alert, 0x5C); /* non-null payload */
                if (l6_parse_header(f, &h) != 1 || h.svc != exp) bad++;
                seen[h.svc]++;
            }
        }
        /* the null message is NULL whatever the PRN */
        make_frame(f, prns[k], 0x00, 1, 0xAA);
        if (l6_parse_header(f, &h) == 1 && h.svc == L6_SVC_NULL) nnull++;
    }
    CHECK(bad == 0, "service matches vendor ID + CSID for all msgid x alert x PRN");
    CHECK(nnull == (int)sizeof(prns), "null message classified NULL for every PRN");
    CHECK(seen[L6_SVC_UNKNOWN] && seen[L6_SVC_CLAS] && seen[L6_SVC_MADOCA] && seen[L6_SVC_MADOCA_IONO] &&
              seen[L6_SVC_QZNMA],
          "every non-null service reached");
    printf("ok  : classification exhaustive (%d mismatches)\n", bad);
}

/* (3) null message and near misses */
static void test_null(void) {
    uint8_t f[L6_FRAME_LEN];
    l6_header_t h;

    make_frame(f, 194, 0x00, 1, 0xAA);
    CHECK(l6_is_null(f) == 1, "null message detected");
    CHECK(l6_parse_header(f, &h) == 1 && h.svc == L6_SVC_NULL, "null message classified NULL");

    make_frame(f, 194, 0x00, 0, 0xAA);
    CHECK(l6_is_null(f) == 0, "alert=0 is not null");
    make_frame(f, 194, 0xA0, 1, 0xAA);
    CHECK(l6_is_null(f) == 0, "msgid!=0 is not null");
    make_frame(f, 194, 0x00, 1, 0xAA);
    f[L6_FRAME_LEN - 1] = 0xAB;
    CHECK(l6_is_null(f) == 0, "last byte differs is not null");
    make_frame(f, 194, 0x00, 1, 0xAA);
    f[6] = 0x80; /* alert set, first data bits not 0101010b */
    CHECK(l6_is_null(f) == 0, "byte 6 differs is not null");
    printf("ok  : null message\n");
}

/* (4) bad preamble */
static void test_bad_preamble(void) {
    uint8_t f[L6_FRAME_LEN];
    l6_header_t h;

    make_frame(f, 195, 0xA1, 0, 0x5C);
    f[3] = 0x1E;
    CHECK(l6_parse_header(f, &h) == 0, "bad preamble rejected");
    CHECK(h.svc == L6_SVC_UNKNOWN, "bad preamble -> UNKNOWN");
    CHECK(h.prn == 195 && h.msgid == 0xA1, "fields still filled on bad preamble");
    printf("ok  : bad preamble\n");
}

/* (5) real frames from mosaic-G5 captures */
static void test_real_frames(void) {
    static const struct {
        uint8_t prn;
        l6_service_t svc;
        uint8_t sf_ind;
        const char* name;
    } expected[] = {
        {195, L6_SVC_CLAS, 1, "real CLAS (L6D PRN 195, subframe head)"},
        {205, L6_SVC_MADOCA, 1, "real MADOCA (L6E PRN 205, subframe head)"},
        {200, L6_SVC_MADOCA_IONO, 1, "real MADOCA iono (L6D PRN 200, subframe head)"},
        {205, L6_SVC_QZNMA, 1, "real QZNMA (L6E PRN 205, subframe head)"},
    };
    uint8_t buf[4 * L6_FRAME_LEN];
    l6_header_t h;
    FILE* fp;
    int k;

    if (!(fp = fopen("l6_frames.bin", "rb"))) {
        CHECK(0, "open l6_frames.bin");
        return;
    }
    if (fread(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
        CHECK(0, "read 4 real frames");
        fclose(fp);
        return;
    }
    fclose(fp);

    for (k = 0; k < 4; k++) {
        const uint8_t* f = buf + k * L6_FRAME_LEN;
        CHECK(l6_parse_header(f, &h) == 1, expected[k].name);
        CHECK(h.prn == expected[k].prn, expected[k].name);
        CHECK(h.svc == expected[k].svc, expected[k].name);
        CHECK(h.sf_ind == expected[k].sf_ind, expected[k].name);
        CHECK(h.alert == 0 && !l6_is_null(f), expected[k].name);
    }
    printf("ok  : real frames\n");
}

/* (6) l6_file_service(): service of an L6 frame file from its content */
static int write_file(const char* path, const uint8_t* const* frames, int n, int junk) {
    static const uint8_t pad[L6_RAW_LEN - L6_FRAME_LEN] = {0};
    FILE* fp = fopen(path, "wb");
    int k;

    if (!fp) return 0;
    for (k = 0; k < junk; k++) fputc(0x5C, fp); /* bytes before the first preamble */
    for (k = 0; k < n; k++) {
        fwrite(frames[k], 1, L6_FRAME_LEN, fp);
        fwrite(pad, 1, sizeof(pad), fp); /* archive files carry the 32-byte RS parity */
    }
    fclose(fp);
    return 1;
}

static void test_file_service(void) {
    uint8_t buf[4 * L6_FRAME_LEN], null[L6_FRAME_LEN];
    const uint8_t* fr[2];
    char dir[1024], path[1100];
    const char* tmp = getenv("TMPDIR");
    FILE* fp;

    if (!(fp = fopen("l6_frames.bin", "rb")) || fread(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
        CHECK(0, "file service: read l6_frames.bin");
        if (fp) fclose(fp);
        return;
    }
    fclose(fp);
    make_frame(null, 194, 0x00, 1, 0xAA);
    /* private directory: concurrent runs cannot collide and no pre-existing
     * (possibly symlinked) path in a shared temp folder is written to */
    snprintf(dir, sizeof(dir), "%s/utest_l6_header_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(dir)) {
        CHECK(0, "file service: mkdtemp");
        return;
    }
    snprintf(path, sizeof(path), "%s/frames.l6d", dir);

    CHECK(l6_file_service("l6_frames.bin") == L6_SVC_CLAS, "file: first frame CLAS -> CLAS");

    fr[0] = buf + 2 * L6_FRAME_LEN; /* MADOCA iono, PRN 200 */
    CHECK(write_file(path, fr, 1, 0) && l6_file_service(path) == L6_SVC_MADOCA_IONO, "file: iono frame -> MADOCA iono");

    fr[0] = null;
    fr[1] = buf; /* null message, then CLAS */
    CHECK(write_file(path, fr, 2, 0) && l6_file_service(path) == L6_SVC_CLAS, "file: leading null message skipped");

    fr[0] = buf + L6_FRAME_LEN; /* MADOCA after junk bytes */
    CHECK(write_file(path, fr, 1, 1000) && l6_file_service(path) == L6_SVC_MADOCA, "file: preamble found after junk");

    CHECK(write_file(path, fr, 0, 0) && l6_file_service(path) == L6_SVC_UNKNOWN, "file: empty -> UNKNOWN");
    remove(path);
    CHECK(l6_file_service(path) == L6_SVC_UNKNOWN, "file: missing -> UNKNOWN");
    rmdir(dir);
    printf("ok  : l6_file_service\n");
}

/* (7) l6_file_type(): L6 file extension, case-insensitive */
static void test_file_type(void) {
    static const struct {
        const char* name;
        l6_file_type_t type;
    } c[] = {
        {"2019239Q.l6", L6_FILE_L6},      {"2019239Q.L6", L6_FILE_L6},    {"2025091A.200.l6", L6_FILE_L6},
        {"2023044J.l6d", L6_FILE_L6D},    {"2023044J.L6D", L6_FILE_L6D},  {"x.L6d", L6_FILE_L6D},
        {"x.l6D", L6_FILE_L6D},           {"2023044J.l6e", L6_FILE_L6E},  {"x.L6e", L6_FILE_L6E},
        {"/a.b/c.204.l6E", L6_FILE_L6E},  {"0627044J.bnx", L6_FILE_NONE}, {"x.l6x", L6_FILE_NONE},
        {"x.l6dd", L6_FILE_NONE},         {"x.l", L6_FILE_NONE},          {"noext", L6_FILE_NONE},
        {"dir.l6/obs.obs", L6_FILE_NONE},
    };
    size_t k;

    for (k = 0; k < sizeof(c) / sizeof(c[0]); k++) {
        CHECK(l6_file_type(c[k].name) == c[k].type, c[k].name);
    }
    printf("ok  : l6_file_type (%d cases)\n", (int)(sizeof(c) / sizeof(c[0])));
}

int main(void) {
    test_fields_exhaustive();
    test_classify();
    test_classify_exhaustive();
    test_null();
    test_bad_preamble();
    test_real_frames();
    test_file_service();
    test_file_type();
    if (fails) {
        printf("%d check(s) FAILED\n", fails);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
