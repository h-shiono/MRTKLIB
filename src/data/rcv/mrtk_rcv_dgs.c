/*------------------------------------------------------------------------------
 * mrtk_rcv_dgs.c : DGS QZSS L6 frame container (DATAGNSS QZS-6C / MA-10P)
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 *
 * References:
 *   [1] DATAGNSS, DGS Format Notes,
 *       https://gist.github.com/datagnss/af7013746c3776a2f5e1cebb9cabdb6c
 *   [2] DATAGNSS, QZS-6C datasheet T-5-2410 V0.9, 5.2 (message RAWX-QZSSL6)
 *   [3] ALLYSTAR GNSS Receiver Binary Protocol Specification V2.3.6, 3.1
 *       (frame structure, checksum)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * notes  : "DGS" is the vendor's name for the Allystar binary message group
 *          0x02 / id 0x10, one QZSS L6 frame per message:
 *
 *            0    2  sync 0xF1 0xD9
 *            2    2  group 0x02, id 0x10
 *            4    2  payload length (little-endian) = 264
 *            6    2  SVN (little-endian) = 700 + L6 PRN
 *            8    1  frequency id
 *            9    1  data length in words + 2 = 65
 *           10    2  GPS week (big-endian)
 *           12    4  time of week (ms, big-endian)
 *           16    1  SNR
 *           17    1  status flags (DGS_FLAG_*)
 *           18  252  L6 frame in transmission order (250 bytes) + 2 pad bytes
 *          270    2  8-bit Fletcher checksum over bytes 2..269
 *
 *          The week and time of week are big-endian although [3] defines the
 *          protocol as little-endian; neither [1] nor [2] says so. Confirmed
 *          on captures of an MA-10P (2026) and an HD9310 option C (2022).
 *----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_l6.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_rcvraw.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_trace.h"

#define DGS_PAYLOAD_LEN 264 /* payload length (bytes) */
#define DGS_DATA_WORDS 65   /* data length field: 63 L6 words + 2 */

static const uint8_t dgs_sync[4] = {DGS_SYNC1, DGS_SYNC2, DGS_GROUP_RAW, DGS_ID_QZSSL6};

/* decode one DGS L6 frame ---------------------------------------------------*/
extern int dgs_decode_l6(const uint8_t* frame, dgs_l6_t* dgs) {
    uint8_t c1 = 0, c2 = 0;
    int i;

    if (frame[0] != DGS_SYNC1 || frame[1] != DGS_SYNC2 || frame[2] != DGS_GROUP_RAW || frame[3] != DGS_ID_QZSSL6) {
        return 0;
    }
    if ((frame[4] | (frame[5] << 8)) != DGS_PAYLOAD_LEN || frame[9] != DGS_DATA_WORDS) {
        return 0;
    }
    for (i = 2; i < DGS_FRAME_LEN - 2; i++) {
        c1 = (uint8_t)(c1 + frame[i]);
        c2 = (uint8_t)(c2 + c1);
    }
    if (c1 != frame[DGS_FRAME_LEN - 2] || c2 != frame[DGS_FRAME_LEN - 1]) {
        return 0;
    }
    dgs->prn = (frame[6] | (frame[7] << 8)) - 700;
    dgs->week = (frame[10] << 8) | frame[11];
    dgs->tow =
        (((uint32_t)frame[12] << 24) | ((uint32_t)frame[13] << 16) | ((uint32_t)frame[14] << 8) | frame[15]) * 1e-3;
    dgs->snr = frame[16];
    dgs->flag = frame[17];
    dgs->l6 = frame + 18;
    return 1;
}
/* route the L6 frame of a DGS frame ------------------------------------------*/
static int decode_dgs_l6(raw_t* raw, rtcm_t* rtcm, const dgs_l6_t* dgs) {
    l6_header_t hdr;
    int ret;

    if (raw->outtype) {
        sprintf(raw->msgtype, "DGS L6 (%4d): prn=%3d week=%d tow=%.0f snr=%d flag=%02X", DGS_FRAME_LEN, dgs->prn,
                dgs->week, dgs->tow, dgs->snr, dgs->flag);
    }
    if (dgs->flag & DGS_FLAG_RS) {
        trace(NULL, 2, "dgs l6 RS decode error: prn=%d tow=%.0f\n", dgs->prn, dgs->tow);
        return -1;
    }
    if (dgs->flag & DGS_FLAG_UNUSABLE) {
        /* the frame time drives the frame-gap detection of the L6 decoders */
        trace(NULL, 2, "dgs l6 time not confirmed: prn=%d flag=%02X\n", dgs->prn, dgs->flag);
        return 0;
    }
    memcpy(rtcm->buff, dgs->l6, L6_RAW_LEN);
    raw->time = gpst2time(dgs->week, dgs->tow);
    raw->l6tag = dgs->week * 604800.0 + dgs->tow;

    l6_parse_header(dgs->l6, &hdr);
    ret = decode_qzss_l6frame(rtcm, hdr.prn <= MAXPRNQZS, raw->l6tag);
    if (ret == 10 || ret == 16) {
        raw->ephsat = satno(SYS_QZS, hdr.prn);
    }
    return ret;
}
/* discard bytes until the buffer starts with a (partial) sync sequence ------*/
static void dgs_align(raw_t* raw, int from) {
    int i, n;

    for (i = from; i < raw->nbyte; i++) {
        n = raw->nbyte - i < 4 ? raw->nbyte - i : 4;
        if (!memcmp(raw->buff + i, dgs_sync, n)) break;
    }
    memmove(raw->buff, raw->buff + i, raw->nbyte - i);
    raw->nbyte -= i;
}
/* input DGS data from stream -------------------------------------------------
 * The stream of a QZS-6C carries NMEA and RTCM3 between the DGS frames, and
 * they can contain the sync sequence: a candidate that fails is rescanned from
 * its second byte, so a frame starting inside it is not lost.
 *----------------------------------------------------------------------------*/
extern int input_dgs(raw_t* raw, rtcm_t* rtcm, uint8_t data) {
    dgs_l6_t dgs;

    raw->buff[raw->nbyte++] = data;
    dgs_align(raw, 0);
    if (raw->nbyte < DGS_FRAME_LEN) {
        return 0;
    }
    if (!dgs_decode_l6(raw->buff, &dgs)) {
        dgs_align(raw, 1);
        return 0;
    }
    raw->len = raw->nbyte;
    raw->nbyte = 0;
    return decode_dgs_l6(raw, rtcm, &dgs);
}
