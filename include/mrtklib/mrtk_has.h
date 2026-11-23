/*------------------------------------------------------------------------------
 * mrtk_has.h : Galileo HAS (High Accuracy Service) decode functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Reference:
 *   Galileo High Accuracy Service Signal-in-Space Interface Control Document
 *   (HAS SIS ICD), Issue 1.0, May 2022, European Union.
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_has.h
 * @brief MRTKLIB Galileo HAS Module — E6-B C/NAV HAS page collection and
 *        Message Type 1 (MT1) decode into ssr_t corrections.
 *
 * This header provides the public API for collecting Galileo HAS pages
 * (keyed by Message ID / Message Type), recovering the encoded message via
 * the RS(255,32) erasure decoder, decoding MT1 (mask, orbit, clock, code
 * bias, phase bias blocks) and filling an internal ssr_t array that callers
 * copy into nav->ssr_ch[0].
 *
 * @note Functions declared here are implemented in src/has/mrtk_has.c.
 */
#ifndef MRTK_HAS_H
#define MRTK_HAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_time.h"

/*============================================================================
 * Galileo HAS Constants
 *===========================================================================*/

#define HAS_MAX_MID 32      /* number of distinct Mask IDs (5-bit field) */
#define HAS_MAX_IODSET 32   /* number of distinct IOD Set IDs (5-bit field) */
#define HAS_MAX_PAGES 32    /* max non-encoded pages per message (MS, 1..32) */
#define HAS_PAGE_BYTES 53   /* encoded-page payload length (424 bits) */
#define HAS_MAX_INFLIGHT 2  /* in-flight collection slots (current + previous) */
#define HAS_MAX_GNSS_SAT 40 /* satellites per GNSS satellite mask (SatM) */
#define HAS_MAX_SIG 16      /* signals per GNSS signal mask (SigM) */

/*============================================================================
 * Galileo HAS Decode Context
 *===========================================================================*/

typedef struct {                       /* cached HAS mask (per Mask ID) */
    int valid;                         /* 0:empty, 1:populated */
    int nsys;                          /* number of GNSS in the mask */
    int nsat;                          /* total masked satellites (all GNSS) */
    int sat[MAXSAT];                   /* satno per mask slot (0:unsupported) */
    int gnssid[MAXSAT];                /* HAS GNSS ID per mask slot */
    uint8_t code[MAXSAT][HAS_MAX_SIG]; /* obs code per (sat,signal slot) */
    int nsig[MAXSAT];                  /* number of signals per satellite */
    uint16_t cellmask[MAXSAT];         /* signal cell mask per satellite (MSB=sig0) */
    int navmsg[MAXSAT];                /* navigation message index per satellite */
} has_mask_t;

typedef struct {        /* cached IOD set (per Mask ID + IOD Set ID) */
    int valid;          /* 0:empty, 1:populated */
    int iodref[MAXSAT]; /* reference IOD per mask slot */
} has_iodset_t;

typedef struct {                                 /* in-flight page collector */
    int active;                                  /* 0:idle, 1:collecting */
    int mid;                                     /* Message ID being collected */
    int mt;                                      /* Message Type being collected */
    int ms;                                      /* message size (distinct pages needed) */
    int npid;                                    /* number of distinct PIDs collected */
    gtime_t t0;                                  /* reception time of first page */
    uint8_t pid[HAS_MAX_PAGES];                  /* collected PIDs */
    uint8_t page[HAS_MAX_PAGES][HAS_PAGE_BYTES]; /* collected encoded-page payloads */
} has_collect_t;

typedef struct {                                      /* Galileo HAS decode context */
    has_collect_t collect[HAS_MAX_INFLIGHT];          /* in-flight collectors */
    has_mask_t mask[HAS_MAX_MID];                     /* mask cache keyed by Mask ID */
    has_iodset_t iodset[HAS_MAX_MID][HAS_MAX_IODSET]; /* IOD-set cache */
    ssr_t ssr[MAXSAT];                                /* decoded SSR corrections */
} has_t;

/*============================================================================
 * Galileo HAS Functions
 *===========================================================================*/

/**
 * @brief Allocate and initialize a Galileo HAS decode context.
 * @return Pointer to a heap-allocated has_t (NULL on allocation failure or RS
 *         init failure). Free with has_free().
 * @note Calls has_rs_init() to build the RS(255,32) erasure-decoder tables.
 */
has_t* has_new(void);

/**
 * @brief Free a Galileo HAS decode context.
 * @param[in,out] has  Context allocated by has_new() (NULL: no-op)
 */
void has_free(has_t* has);

/**
 * @brief Input one received Galileo HAS page.
 * @param[in,out] has     HAS decode context
 * @param[in]     prn     Galileo PRN of the transmitting satellite (1-40)
 * @param[in]     page56  56-byte HAS page (24-bit header + 424-bit encoded page)
 * @param[in]     time    Page reception time (GPST)
 * @return 10 when a complete MT1 was decoded and ssr[] updated, 0 when no
 *         message completed, <0 on error.
 */
int has_input_page(has_t* has, int prn, const uint8_t* page56, gtime_t time);

/**
 * @brief Read .has records from a file and feed them to has_input_page().
 * @param[in,out] has   HAS decode context
 * @param[in]     fp    Open .has file pointer (64-byte little-endian records)
 * @param[in]     tmax  Stop after reading records past this time (GPST)
 * @return 10 if any message completed during the call, 0 otherwise (including
 *         at end of file).
 */
int has_input_file(has_t* has, FILE* fp, gtime_t tmax);

/**
 * @brief Select the HAS bias slot obs code for an observation code.
 * @param[in] sys   Satellite system (SYS_GPS or SYS_GAL)
 * @param[in] code  Observation code (CODE_???)
 * @return Obs code under which the HAS bias is stored, with same-frequency
 *         fallback (component <-> combined on the same carrier per HAS SIS ICD
 *         Table 20); 0 (CODE_NONE) if no mapping exists.
 */
int has_sel_biascode(int sys, uint8_t code);

/**
 * @brief Select the obs code under which HAS provides the bias of an observation.
 *
 * The observed code first, then the other codes of the same signal
 * (component <-> combined, has_sel_biascode()), e.g. observed C1C with a HAS
 * E1-C bias -> C1C, observed C1X with only an E1-C bias -> C1C.
 *
 * @param[in] ssr    SSR entry of the satellite (vcbias / vpbias are read)
 * @param[in] sys    Satellite system (SYS_GPS or SYS_GAL)
 * @param[in] code   Observation code (CODE_???)
 * @param[in] phase  0: code bias (vcbias), 1: phase bias (vpbias)
 * @return Obs code of a valid HAS bias; has_sel_biascode(sys, code) if none of
 *         the signal is valid; 0 (CODE_NONE) if the signal has no HAS mapping.
 */
int has_sel_bias(const ssr_t* ssr, int sys, uint8_t code, int phase);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_HAS_H */
