/*------------------------------------------------------------------------------
 * t_ntrip.c : unit tests for NTRIP v2 chunked transfer encoding and helpers
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* include header-only implementation directly for testing */
#include "../../src/stream/ntrip_chunk.h"

static int n_pass = 0;
static int n_fail = 0;
static int test_failed = 0; /* per-test failure flag */

#define TEST(name)                                      \
    static void name(void);                             \
    static void name##_run(void) {                      \
        printf("  %-50s ", #name);                      \
        test_failed = 0;                                \
        name();                                         \
        if (test_failed) {                              \
            n_fail++;                                   \
        } else {                                        \
            printf("PASS\n");                           \
            n_pass++;                                   \
        }                                               \
    }                                                   \
    static void name(void)

#define ASSERT(cond)                                    \
    do {                                                \
        if (!(cond)) {                                  \
            printf("FAIL\n    %s:%d: %s\n",             \
                   __FILE__, __LINE__, #cond);           \
            test_failed = 1;                            \
            return;                                     \
        }                                               \
    } while (0)

/* ========================================================================== */
/* chunk_decode tests                                                         */
/* ========================================================================== */

/* single complete chunk */
TEST(test_chunk_decode_single) {
    chunk_dec_t dec;
    chunk_dec_init(&dec);

    const uint8_t input[] = "5\r\nHello\r\n0\r\n\r\n";
    const uint8_t *pin = input;
    int nin = (int)sizeof(input) - 1;
    uint8_t out[64];

    int nd = chunk_decode(&dec, &pin, &nin, out, sizeof(out));
    ASSERT(nd == 5);
    ASSERT(memcmp(out, "Hello", 5) == 0);
    ASSERT(dec.state == 3);
}

/* multiple chunks in sequence */
TEST(test_chunk_decode_multi) {
    chunk_dec_t dec;
    chunk_dec_init(&dec);

    const uint8_t input[] = "5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n";
    const uint8_t *pin = input;
    int nin = (int)sizeof(input) - 1;
    uint8_t out[64];

    int nd = chunk_decode(&dec, &pin, &nin, out, sizeof(out));
    ASSERT(nd == 11);
    ASSERT(memcmp(out, "Hello World", 11) == 0);
    ASSERT(dec.state == 3);
}

/* partial input: feed one byte at a time */
TEST(test_chunk_decode_partial) {
    chunk_dec_t dec;
    chunk_dec_init(&dec);

    const uint8_t input[] = "a\r\n0123456789\r\n0\r\n\r\n";
    int total_len = (int)sizeof(input) - 1;
    uint8_t out[64];
    int total_out = 0;

    for (int i = 0; i < total_len; i++) {
        const uint8_t *pin = &input[i];
        int nin = 1;
        int nd = chunk_decode(&dec, &pin, &nin, out + total_out,
                              (int)sizeof(out) - total_out);
        ASSERT(nd >= 0);
        total_out += nd;
    }
    ASSERT(total_out == 10);
    ASSERT(memcmp(out, "0123456789", 10) == 0);
    ASSERT(dec.state == 3);
}

/* zero-length chunk (immediate terminator) */
TEST(test_chunk_decode_zero) {
    chunk_dec_t dec;
    chunk_dec_init(&dec);

    const uint8_t input[] = "0\r\n\r\n";
    const uint8_t *pin = input;
    int nin = (int)sizeof(input) - 1;
    uint8_t out[64];

    int nd = chunk_decode(&dec, &pin, &nin, out, sizeof(out));
    ASSERT(nd == 0);
    ASSERT(dec.state == 3);
}

/* hex size with uppercase letters */
TEST(test_chunk_decode_hex_upper) {
    chunk_dec_t dec;
    chunk_dec_init(&dec);

    const uint8_t input[] = "1A\r\nabcdefghijklmnopqrstuvwxyz\r\n0\r\n\r\n";
    const uint8_t *pin = input;
    int nin = (int)sizeof(input) - 1;
    uint8_t out[64];

    int nd = chunk_decode(&dec, &pin, &nin, out, sizeof(out));
    ASSERT(nd == 26);
    ASSERT(memcmp(out, "abcdefghijklmnopqrstuvwxyz", 26) == 0);
    ASSERT(dec.state == 3);
}

/* chunk with extension (semicolon after hex size) */
TEST(test_chunk_decode_extension) {
    chunk_dec_t dec;
    chunk_dec_init(&dec);

    const uint8_t input[] = "5;ext=val\r\nHello\r\n0\r\n\r\n";
    const uint8_t *pin = input;
    int nin = (int)sizeof(input) - 1;
    uint8_t out[64];

    int nd = chunk_decode(&dec, &pin, &nin, out, sizeof(out));
    ASSERT(nd == 5);
    ASSERT(memcmp(out, "Hello", 5) == 0);
}

/* output buffer smaller than chunk data */
TEST(test_chunk_decode_output_limited) {
    chunk_dec_t dec;
    chunk_dec_init(&dec);

    const uint8_t input[] = "a\r\n0123456789\r\n0\r\n\r\n";
    const uint8_t *pin = input;
    int nin = (int)sizeof(input) - 1;
    uint8_t out[5];

    /* first call: only 5 bytes fit */
    int nd = chunk_decode(&dec, &pin, &nin, out, 5);
    ASSERT(nd == 5);
    ASSERT(memcmp(out, "01234", 5) == 0);

    /* second call: remaining 5 bytes */
    nd = chunk_decode(&dec, &pin, &nin, out, 5);
    ASSERT(nd == 5);
    ASSERT(memcmp(out, "56789", 5) == 0);
}

/* ========================================================================== */
/* chunk_encode tests                                                         */
/* ========================================================================== */

TEST(test_chunk_encode_basic) {
    uint8_t out[64];
    const uint8_t data[] = "Hello";

    int n = chunk_encode(out, sizeof(out), data, 5);
    ASSERT(n > 0);
    ASSERT(memcmp(out, "5\r\nHello\r\n", 10) == 0);
    ASSERT(n == 10);
}

TEST(test_chunk_encode_empty) {
    uint8_t out[64];

    int n = chunk_encode(out, sizeof(out), NULL, 0);
    ASSERT(n > 0);
    ASSERT(memcmp(out, "0\r\n\r\n", 5) == 0);
    ASSERT(n == 5);
}

TEST(test_chunk_encode_buffer_too_small) {
    uint8_t out[5];
    const uint8_t data[] = "Hello World";

    int n = chunk_encode(out, sizeof(out), data, 11);
    ASSERT(n == -1);
}

/* ========================================================================== */
/* round-trip: encode then decode                                             */
/* ========================================================================== */

TEST(test_chunk_roundtrip) {
    const uint8_t original[] = "RTCM3 correction data payload for GNSS";
    int orig_len = (int)sizeof(original) - 1;

    /* encode */
    uint8_t encoded[128];
    int elen = chunk_encode(encoded, sizeof(encoded), original, orig_len);
    ASSERT(elen > 0);

    /* append terminator chunk */
    int tlen = chunk_encode(encoded + elen, (int)sizeof(encoded) - elen, NULL, 0);
    ASSERT(tlen > 0);
    elen += tlen;

    /* decode */
    chunk_dec_t dec;
    chunk_dec_init(&dec);
    const uint8_t *pin = encoded;
    int nin = elen;
    uint8_t decoded[128];

    int dlen = chunk_decode(&dec, &pin, &nin, decoded, sizeof(decoded));
    ASSERT(dlen == orig_len);
    ASSERT(memcmp(decoded, original, orig_len) == 0);
    ASSERT(dec.state == 3);
}

/* final trailer split across calls: exercise FINAL_TRAIL state incrementally */
TEST(test_chunk_decode_final_trailer_split) {
    chunk_dec_t dec;
    chunk_dec_init(&dec);

    const uint8_t input1[] = "0\r\n\r";
    const uint8_t input2[] = "\n";
    const uint8_t *pin;
    int nin;
    uint8_t out[64];

    pin = input1;
    nin = (int)sizeof(input1) - 1;
    ASSERT(chunk_decode(&dec, &pin, &nin, out, sizeof(out)) == 0);
    ASSERT(dec.state == 4); /* in final_trail, waiting for \n */

    pin = input2;
    nin = (int)sizeof(input2) - 1;
    ASSERT(chunk_decode(&dec, &pin, &nin, out, sizeof(out)) == 0);
    ASSERT(dec.state == 3); /* done */
}

/* oversize chunk-size hex digits must be rejected */
TEST(test_chunk_decode_header_too_long) {
    chunk_dec_t dec;
    chunk_dec_init(&dec);

    uint8_t input[CHUNK_HDR_MAX + 3];
    const uint8_t *pin = input;
    int nin = (int)sizeof(input);
    uint8_t out[64];

    memset(input, 'F', CHUNK_HDR_MAX + 1);
    input[CHUNK_HDR_MAX + 1] = '\r';
    input[CHUNK_HDR_MAX + 2] = '\n';
    ASSERT(chunk_decode(&dec, &pin, &nin, out, sizeof(out)) < 0);
}

/* ========================================================================== */
/* HTTP header helper tests                                                   */
/* ========================================================================== */

TEST(test_http_header_end) {
    const uint8_t hdr[] = "HTTP/1.1 200 OK\r\nContent-Type: gnss/data\r\n\r\nBODY";
    int off = http_header_end(hdr, (int)sizeof(hdr) - 1);
    ASSERT(off == 44);
    ASSERT(memcmp(hdr + off, "BODY", 4) == 0);
}

TEST(test_http_header_end_incomplete) {
    const uint8_t hdr[] = "HTTP/1.1 200 OK\r\nContent-Type: gnss/data\r\n";
    int off = http_header_end(hdr, (int)sizeof(hdr) - 1);
    ASSERT(off == 0);
}

TEST(test_http_status_code) {
    const uint8_t hdr1[] = "HTTP/1.1 200 OK\r\n";
    const uint8_t hdr2[] = "HTTP/1.1 401 Unauthorized\r\n";
    const uint8_t hdr3[] = "ICY 200 OK\r\n";

    ASSERT(http_status_code(hdr1, (int)sizeof(hdr1) - 1) == 200);
    ASSERT(http_status_code(hdr2, (int)sizeof(hdr2) - 1) == 401);
    ASSERT(http_status_code(hdr3, (int)sizeof(hdr3) - 1) == 0);
}

TEST(test_http_find_header) {
    const uint8_t hdr[] =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: gnss/data\r\n"
        "Ntrip-Version: Ntrip/2.0\r\n"
        "\r\n";
    int nb = (int)sizeof(hdr) - 1;
    char val[256];

    ASSERT(http_find_header(hdr, nb, "Transfer-Encoding", val, sizeof(val)));
    ASSERT(strcmp(val, "chunked") == 0);

    ASSERT(http_find_header(hdr, nb, "Content-Type", val, sizeof(val)));
    ASSERT(strcmp(val, "gnss/data") == 0);

    ASSERT(http_find_header(hdr, nb, "Ntrip-Version", val, sizeof(val)));
    ASSERT(strcmp(val, "Ntrip/2.0") == 0);

    /* case-insensitive header name */
    ASSERT(http_find_header(hdr, nb, "transfer-encoding", val, sizeof(val)));
    ASSERT(strcmp(val, "chunked") == 0);

    /* not found */
    ASSERT(!http_find_header(hdr, nb, "X-Custom", val, sizeof(val)));
}

/* ========================================================================== */
/* main                                                                       */
/* ========================================================================== */

int main(void) {
    printf("NTRIP v2 unit tests\n");
    printf("====================\n");

    /* chunk decoder */
    test_chunk_decode_single_run();
    test_chunk_decode_multi_run();
    test_chunk_decode_partial_run();
    test_chunk_decode_zero_run();
    test_chunk_decode_hex_upper_run();
    test_chunk_decode_extension_run();
    test_chunk_decode_output_limited_run();
    test_chunk_decode_final_trailer_split_run();
    test_chunk_decode_header_too_long_run();

    /* chunk encoder */
    test_chunk_encode_basic_run();
    test_chunk_encode_empty_run();
    test_chunk_encode_buffer_too_small_run();

    /* round-trip */
    test_chunk_roundtrip_run();

    /* HTTP helpers */
    test_http_header_end_run();
    test_http_header_end_incomplete_run();
    test_http_status_code_run();
    test_http_find_header_run();

    printf("====================\n");
    printf("Results: %d passed, %d failed\n", n_pass, n_fail);

    return n_fail > 0 ? 1 : 0;
}
