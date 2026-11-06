/*------------------------------------------------------------------------------
 * mrtk_b2b.h : BeiDou PPP-B2b decode functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Reference:
 *   BeiDou Navigation Satellite System Signal In Space Interface Control
 *   Document, Precise Point Positioning Service Signal PPP-B2b (Version 1.0),
 *   China Satellite Navigation Office, July 2020 (BDS-SIS-ICD-PPP-B2b-1.0).
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_b2b.h
 * @brief MRTKLIB BeiDou PPP-B2b Module — B2b-I message decode (MT1-7) into
 *        ssr_t corrections.
 *
 * This header provides the public API for decoding BeiDou PPP-B2b messages
 * (486-bit frames carried on B2b-I from BDS-3 GEO satellites) — satellite
 * mask (MT1), orbit corrections (MT2), code biases (MT3), clock corrections
 * (MT4), URAI (MT5) and combined messages (MT6/MT7) — and filling an internal
 * ssr_t array that callers copy into nav->ssr_ch[0].
 *
 * The LDPC(162,81) channel decoding is performed by the receiver; this module
 * consumes the 486-bit message (MesType + data + CRC-24) and verifies the
 * CRC-24Q itself.
 *
 * @note Functions declared here are implemented in src/b2b/mrtk_b2b.c.
 */
#ifndef MRTK_B2B_H
#define MRTK_B2B_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_time.h"

/*============================================================================
 * BeiDou PPP-B2b Constants
 *===========================================================================*/

#define B2B_MSG_BITS 486  /* message length: MesType(6)+data(456)+CRC(24) */
#define B2B_MSG_BYTES 61  /* message buffer length (486 bits, MSB-first) */
#define B2B_REC_BYTES 72  /* .b2b file record length (see b2b_input_file) */
#define B2B_MASK_BITS 255 /* satellite mask length (slots 1-255) */

/*============================================================================
 * BeiDou PPP-B2b Decode Context
 *===========================================================================*/

typedef struct {            /* PPP-B2b satellite mask (MT1) */
    int valid;              /* 0:empty, 1:populated */
    int iodp;               /* IOD of PRN mask (0-15) */
    int nsat;               /* number of masked satellites */
    int sat[B2B_MASK_BITS]; /* satno in mask-compressed order (0:unsupported) */
} b2b_mask_t;

typedef struct {       /* BeiDou PPP-B2b decode context */
    int iodssr;        /* current IOD SSR (-1:none yet) */
    b2b_mask_t mask;   /* current satellite mask */
    ssr_t ssr[MAXSAT]; /* decoded SSR corrections */
} b2b_t;

/*============================================================================
 * BeiDou PPP-B2b Functions
 *===========================================================================*/

/**
 * @brief Allocate and initialize a PPP-B2b decode context.
 * @return Pointer to a heap-allocated b2b_t (NULL on allocation failure).
 *         Free with b2b_free().
 */
b2b_t* b2b_new(void);

/**
 * @brief Free a PPP-B2b decode context.
 * @param[in,out] b2b  Context allocated by b2b_new() (NULL: no-op)
 */
void b2b_free(b2b_t* b2b);

/**
 * @brief Input one received PPP-B2b message.
 * @param[in,out] b2b    PPP-B2b decode context
 * @param[in]     prn    BDS PRN of the transmitting satellite; frames from PRNs
 *                       not listed by set_bdsgeo() are ignored
 * @param[in]     msg61  61-byte message buffer (486 bits MSB-first:
 *                       MesType(6) + data(456) + CRC-24; trailing 2 bits 0)
 * @param[in]     time   Message reception time (GPST)
 * @return 10 when corrections were updated in ssr[], 0 when no correction
 *         content (mask / URAI / null message), <0 on CRC or decode error.
 */
int b2b_input_msg(b2b_t* b2b, int prn, const uint8_t* msg61, gtime_t time);

/**
 * @brief Read .b2b records from a file and feed them to b2b_input_msg().
 * @param[in,out] b2b   PPP-B2b decode context
 * @param[in]     fp    Open .b2b file pointer (72-byte little-endian records:
 *                      [tow_ms u4][wnc u2][prn u1][flags u1][msg u8[61]][pad
 *                      u8[3]]; tow_ms/wnc = GPS time of reception)
 * @param[in]     tmax  Stop after reading records past this time (GPST)
 * @return 10 if any correction was updated during the call, 0 otherwise
 *         (including at end of file).
 */
int b2b_input_file(b2b_t* b2b, FILE* fp, gtime_t tmax);

/**
 * @brief Select the PPP-B2b bias slot obs code for an observation code.
 * @param[in] sys   Satellite system (SYS_GPS, SYS_GLO, SYS_GAL or SYS_CMP)
 * @param[in] code  Observation code (CODE_???)
 * @return Obs code under which the PPP-B2b code bias is stored, with
 *         same-band tracking-mode fallback (ICD Table 6-5); 0 (CODE_NONE) if
 *         no mapping exists.
 */
int b2b_sel_biascode(int sys, uint8_t code);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_B2B_H */
