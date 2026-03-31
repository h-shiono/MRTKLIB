/*------------------------------------------------------------------------------
 * ntrip_chunk.h : NTRIP v2 chunked transfer encoding and HTTP helpers
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This is a header-only implementation intended to be #included by both
 * mrtk_stream.c and test code (t_ntrip.c).  All functions are static.
 *----------------------------------------------------------------------------*/
#ifndef NTRIP_CHUNK_H
#define NTRIP_CHUNK_H

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#else
#include <strings.h> /* strncasecmp on POSIX (Linux, macOS, BSD, etc.) */
#endif

/*============================================================================
 * NTRIP Version Constants
 *===========================================================================*/

#define NTRIP_VER_AUTO 0 /* auto-detect (try v2, fallback v1) */
#define NTRIP_VER_1 1    /* NTRIP version 1.0 */
#define NTRIP_VER_2 2    /* NTRIP version 2.0 */

/*============================================================================
 * Chunked Transfer Encoding Types
 *===========================================================================*/

#define CHUNK_HDR_MAX 16       /* max chunk-size header length ("FFFFFFFF\r\n") */
#define CHUNK_SIZE_MAX 0xFFFFFF /* max chunk size (~16 MB, well within int range) */

typedef struct {               /* chunked transfer decoder state */
    int state;                 /* 0:size, 1:data, 2:trail, 3:done */
    int remain;                /* remaining bytes in current chunk */
    char hdr[CHUNK_HDR_MAX];   /* partial chunk-size line buffer */
    int nhdr;                  /* bytes accumulated in hdr[] */
} chunk_dec_t;

/*============================================================================
 * Chunked Transfer Decoder
 *===========================================================================*/

/* initialize chunked decoder state */
static void chunk_dec_init(chunk_dec_t *dec) {
    dec->state = 0;
    dec->remain = 0;
    dec->nhdr = 0;
}

/* decode chunked transfer encoding (incremental, non-blocking)
 *
 * Reads from *pin (length *pnin), writes decoded payload to out (max nout).
 * Advances *pin and decrements *pnin as input is consumed.
 * Returns number of decoded bytes written to out, or -1 on error.
 *
 * State machine:
 *   0 (SIZE)  : accumulate hex digits + optional extension until \r\n
 *   1 (DATA)  : copy `remain` bytes from input to output
 *   2 (TRAIL) : consume trailing \r\n after chunk data
 *   3 (DONE)  : terminal state (zero-length chunk received)
 */
static int chunk_decode(chunk_dec_t *dec, const uint8_t **pin, int *pnin,
                        uint8_t *out, int nout) {
    const uint8_t *in = *pin;
    int nin = *pnin;
    int nw = 0; /* bytes written to out */

    while (nin > 0 && dec->state != 3) { /* 3 = DONE (terminal) */
        switch (dec->state) {
        case 0: /* SIZE: read chunk-size line */
            while (nin > 0) {
                char c = (char)*in++;
                nin--;

                if (c == '\n' && dec->nhdr > 0 && dec->hdr[dec->nhdr - 1] == '\r') {
                    /* complete chunk-size line: parse hex */
                    dec->hdr[dec->nhdr - 1] = '\0'; /* strip \r */
                    unsigned long size = 0;
                    const char *h = dec->hdr;
                    int ndigits = 0;

                    /* parse hex digits (stop at extension ';' if present) */
                    while (*h) {
                        int d;
                        if (*h >= '0' && *h <= '9') {
                            d = *h - '0';
                        } else if (*h >= 'a' && *h <= 'f') {
                            d = *h - 'a' + 10;
                        } else if (*h >= 'A' && *h <= 'F') {
                            d = *h - 'A' + 10;
                        } else {
                            break; /* extension or invalid */
                        }
                        size = (size << 4) | d;
                        ndigits++;
                        h++;
                    }
                    dec->nhdr = 0;

                    /* reject empty size line or overflow */
                    if (ndigits == 0 || size > CHUNK_SIZE_MAX) {
                        *pin = in;
                        *pnin = nin;
                        return -1;
                    }
                    dec->remain = (int)size;

                    if (dec->remain == 0) {
                        dec->state = 4; /* consume final trailer CRLF */
                    } else {
                        dec->state = 1; /* data follows */
                    }
                    break;
                }
                if (dec->nhdr < CHUNK_HDR_MAX - 1) {
                    dec->hdr[dec->nhdr++] = c;
                } else {
                    /* chunk-size header too long */
                    *pin = in;
                    *pnin = nin;
                    return -1;
                }
            }
            break;

        case 1: { /* DATA: copy chunk payload */
            int avail = nin < dec->remain ? nin : dec->remain;
            int space = nout - nw;
            int copy = avail < space ? avail : space;

            if (copy > 0) {
                memcpy(out + nw, in, copy);
                nw += copy;
                in += copy;
                nin -= copy;
                dec->remain -= copy;
            }
            if (dec->remain == 0) {
                dec->state = 2; /* expect trailing \r\n */
            }
            if (nw >= nout) {
                /* output full, return what we have */
                goto done;
            }
            break;
        }
        case 2: /* TRAIL: consume \r\n after chunk data */
            while (nin > 0) {
                char c = (char)*in++;
                nin--;

                if (c == '\n') {
                    dec->state = 0; /* next chunk */
                    break;
                }
                /* skip \r, ignore other chars in trailer */
            }
            break;

        case 4: /* FINAL_TRAIL: consume trailing \r\n after zero-length chunk */
            while (nin > 0) {
                char c = (char)*in++;
                nin--;

                if (c == '\n') {
                    dec->state = 3; /* DONE */
                    break;
                }
            }
            break;
        }
    }
done:
    *pin = in;
    *pnin = nin;
    return nw;
}

/*============================================================================
 * Chunked Transfer Encoder
 *===========================================================================*/

/* encode data as a single HTTP chunk: "<hex-size>\r\n<data>\r\n"
 *
 * Writes to out (max nout bytes).  Returns total bytes written, or -1 if
 * output buffer is too small.  Caller must ensure nout >= ndata + 20.
 */
static int chunk_encode(uint8_t *out, int nout, const uint8_t *data,
                        int ndata) {
    char hdr[16];
    int hlen = snprintf(hdr, sizeof(hdr), "%x\r\n", ndata);

    if (hlen + ndata + 2 > nout) {
        return -1; /* buffer too small */
    }
    memcpy(out, hdr, hlen);
    if (ndata > 0) {
        memcpy(out + hlen, data, ndata);
    }
    out[hlen + ndata] = '\r';
    out[hlen + ndata + 1] = '\n';
    return hlen + ndata + 2;
}

/*============================================================================
 * HTTP Header Helpers
 *===========================================================================*/

/* find end of HTTP headers in buffer (bounded search for \r\n\r\n)
 * returns offset to body start, or 0 if headers are incomplete */
static int http_header_end(const uint8_t *buff, int nb) {
    int i;
    if (nb < 4) {
        return 0;
    }
    for (i = 0; i <= nb - 4; i++) {
        if (buff[i]     == '\r' && buff[i + 1] == '\n' &&
            buff[i + 2] == '\r' && buff[i + 3] == '\n') {
            return i + 4;
        }
    }
    return 0;
}

/* extract HTTP status code from first line (e.g. "HTTP/1.1 200 OK\r\n")
 * returns status code (e.g. 200), or 0 if not found */
static int http_status_code(const uint8_t *buff, int nb) {
    int i;
    if (nb < 12) {
        return 0;
    }
    if (strncmp((const char *)buff, "HTTP/", 5) != 0) {
        return 0;
    }
    /* find first space within nb, then parse exactly 3 digits within bounds */
    for (i = 5; i < nb; i++) {
        if (buff[i] == ' ') {
            if (i + 3 >= nb) {
                return 0;
            }
            if (buff[i + 1] >= '0' && buff[i + 1] <= '9' &&
                buff[i + 2] >= '0' && buff[i + 2] <= '9' &&
                buff[i + 3] >= '0' && buff[i + 3] <= '9') {
                return (buff[i + 1] - '0') * 100 +
                       (buff[i + 2] - '0') * 10 +
                       (buff[i + 3] - '0');
            }
            return 0;
        }
    }
    return 0;
}

/* search for a specific HTTP header value (case-insensitive header name)
 * copies value (up to vmax-1 chars) into val, returns 1 if found */
static int http_find_header(const uint8_t *buff, int nb, const char *name,
                            char *val, int vmax) {
    int nlen = (int)strlen(name);
    const char *p = (const char *)buff;
    const char *end = p + nb;

    while (p < end) {
        /* check if enough room for "name:" */
        if (p + nlen + 1 <= end &&
            strncasecmp(p, name, nlen) == 0 && p[nlen] == ':') {
            const char *v = p + nlen + 1;
            int i = 0;
            /* skip leading whitespace */
            while (v < end && (*v == ' ' || *v == '\t')) {
                v++;
            }
            /* copy until \r or \n or end */
            while (v < end && *v != '\r' && *v != '\n' && i < vmax - 1) {
                val[i++] = *v++;
            }
            val[i] = '\0';
            return 1;
        }
        /* advance to next line */
        while (p < end && *p != '\n') {
            p++;
        }
        if (p < end) {
            p++; /* skip \n */
        }
    }
    val[0] = '\0';
    return 0;
}

#endif /* NTRIP_CHUNK_H */
