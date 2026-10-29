/*------------------------------------------------------------------------------
 * mrtk_l6.h : QZSS L6 frame header parsing and service classification
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 *
 * References:
 *   [1] IS-QZSS-L6-008, 4.1.2 (L6 message structure, Table 4.1.2-2), 4.1.2.3
 *       (null message)
 *   [2] IS-QZSS-MDC-004, 4.2.1 (Tables 4.2.1-2, 4.2.1-3)
 *   [3] IS-QZSS-SAS-001, 6.10 (Table 6-8: L6 vendor ID and service name)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_l6.h
 * @brief MRTKLIB L6 Module — QZSS L6 frame header parsing.
 *
 * Every QZSS L6 frame (L6D and L6E) starts with a 49-bit header: 32-bit
 * preamble, 8-bit PRN, 8-bit L6 message type ID and 1-bit alert flag,
 * followed by 1695 data bits and a 256-bit Reed-Solomon parity. The service
 * carried by a frame (CLAS, MADOCA-PPP, QZNMA) is identified by the vendor
 * ID in the message type ID [3], never by the PRN: L6E PRNs time-multiplex
 * MADOCA-PPP and QZNMA frames, and PRN-to-service assignments change over
 * time.
 *
 * The functions here are stateless; byte synchronisation and subframe
 * assembly stay with each service decoder.
 */
#ifndef MRTK_L6_H
#define MRTK_L6_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*============================================================================
 * Frame Constants
 *===========================================================================*/

#define L6_PREAMBLE 0x1ACFFC1DU  /* L6 frame preamble */
#define L6_FRAME_LEN 218         /* frame length without R-S parity (bytes) */
#define L6_RAW_LEN 250           /* frame length with R-S parity (bytes) */
#define L6_HEADER_BITS 49        /* preamble + PRN + type ID + alert (bits) */
#define L6_DATA_BITS 1695        /* data part (bits) */
#define L6_FILE_SCAN_BYTES 65536 /* read chunk of l6_file_service() */

#define L6_VID_MADOCA 2 /* vendor ID 010b: MADOCA-PPP (L6D and L6E) [3] */
#define L6_VID_QZNMA 3  /* vendor ID 011b: QZNMA (L6E only) [3] */
#define L6_VID_CLAS 5   /* vendor ID 101b: CLAS (L6D only) [3] */

/*============================================================================
 * Types
 *===========================================================================*/

/** @brief Service carried by an L6 frame. */
typedef enum {
    L6_SVC_UNKNOWN = 0, /**< bad preamble or reserved vendor ID */
    L6_SVC_NULL,        /**< null message [1] 4.1.2.3 */
    L6_SVC_CLAS,        /**< CLAS (vendor 101b) */
    L6_SVC_MADOCA,      /**< MADOCA-PPP clock/ephemeris (vendor 010b, CSID 0) */
    L6_SVC_MADOCA_IONO, /**< MADOCA-PPP ionospheric corrections (vendor 010b, CSID 1) */
    L6_SVC_QZNMA        /**< QZNMA (vendor 011b) */
} l6_service_t;

/** @brief Decoded L6 frame header. Raw bit fields; no service-specific remapping. */
typedef struct {
    uint32_t preamble; /**< 32-bit preamble (L6_PREAMBLE when valid) */
    uint8_t prn;       /**< L6 PRN (header byte 4; L6E PRNs exceed MAXPRNQZS) */
    uint8_t msgid;     /**< L6 message type ID (header byte 5) */
    uint8_t vid;       /**< vendor ID, msgid bits 7-5 */
    uint8_t fac;       /**< msgid bits 4-3 (CLAS: facility + pattern ID; MADOCA: facility) */
    uint8_t ptn;       /**< msgid bits 2-1 (CLAS transmit pattern ID) */
    uint8_t csid;      /**< msgid bit 2 (MADOCA correction service ID: 0 clk/eph, 1 iono) */
    uint8_t anme;      /**< msgid bit 1 (MADOCA applicable navigation message extension) */
    uint8_t sf_ind;    /**< msgid bit 0 (subframe indicator: 1 = first data part) */
    uint8_t alert;     /**< alert flag */
    l6_service_t svc;  /**< service classification */
} l6_header_t;

/**
 * @brief Auxiliary per-PRN information for one L6 PRN.
 *
 * Nominal assignments from the PRN code assignment tables of [2] and [3]
 * and the CLAS transmit-pattern table of [1] (Table 4.1.1-1). Used for
 * logging, for consistency warnings and for the MADOCA ionospheric region
 * slot, never for service classification: assignments change over time
 * (PRN 197 moved from technology demonstration to CLAS in MDC revision 002)
 * and L6E PRNs carry both MADOCA-PPP and QZNMA.
 */
typedef struct {
    uint8_t prn;      /**< L6 PRN */
    const char* sat;  /**< satellite name, e.g. "QZS2" */
    uint8_t l6e;      /**< 0: L6D PRN, 1: L6E PRN */
    l6_service_t svc; /**< nominal service (L6E: MADOCA, QZNMA also expected) */
    int8_t iono_slot; /**< MADOCA iono decoder slot (L6D PRN 200/201), -1 otherwise */
    uint8_t pair_prn; /**< PRN of the other L6 signal of the same satellite, 0 if none */
} l6_prn_info_t;

/*============================================================================
 * Functions
 *===========================================================================*/

/**
 * @brief Parse the header of one L6 frame and classify its service.
 * @param[in]  frame  L6 frame starting at the preamble; at least L6_FRAME_LEN bytes.
 * @param[out] h      Decoded header. All fields are filled even when the preamble is invalid.
 * @return 1 if the preamble is valid, 0 otherwise (h->svc is L6_SVC_UNKNOWN).
 */
int l6_parse_header(const uint8_t* frame, l6_header_t* h);

/**
 * @brief Test whether an L6 frame is a null message.
 *
 * Null message [1] 4.1.2.3: message type ID 0, alert flag 1, data part
 * "0101010"b followed by repeated "10101010"b, i.e. bytes 6..217 all 0xAA.
 *
 * @param[in] frame  L6 frame starting at the preamble; at least L6_FRAME_LEN bytes.
 * @return 1 if the frame is a null message, 0 otherwise.
 */
int l6_is_null(const uint8_t* frame);

/**
 * @brief Look up the auxiliary information of an L6 PRN.
 * @param[in] prn  L6 PRN (header byte 4).
 * @return Table entry, or NULL if the PRN has no nominal L6 assignment.
 */
const l6_prn_info_t* l6_prn_info(int prn);

/**
 * @brief Test whether a classified frame matches the nominal assignment of its PRN.
 *
 * Null messages always match; on L6E PRNs both MADOCA and QZNMA match.
 *
 * @param[in] h  Parsed header.
 * @return 1 if consistent, 0 if the PRN is unassigned or carries an unexpected service.
 */
int l6_service_expected(const l6_header_t* h);

/** @brief QZSS L6 file type from the file name extension. */
typedef enum {
    L6_FILE_NONE = 0, /**< not an L6 file */
    L6_FILE_L6,       /**< ".l6" (legacy: CLAS L6D or MADOCA-PPP L6E) */
    L6_FILE_L6D,      /**< ".l6d" */
    L6_FILE_L6E       /**< ".l6e" */
} l6_file_type_t;

/**
 * @brief Classify a file name by its QZSS L6 extension (case-insensitive).
 * @param[in] file  File path or name.
 * @return L6 file type, L6_FILE_NONE if the extension is not .l6/.l6d/.l6e.
 */
l6_file_type_t l6_file_type(const char* file);

/**
 * @brief Service of an L6 frame file, from the first non-null frame header.
 *
 * Scans the start of the file (concatenated 218- or 250-byte frames, as
 * written by receivers and archives) for the preamble and classifies the
 * first frame that is not a null message.
 *
 * @param[in] file  File path (no keyword expansion).
 * @return Service, or L6_SVC_UNKNOWN if the file cannot be read or holds no
 *         classifiable frame. The file is read in L6_FILE_SCAN_BYTES chunks
 *         until the first classifiable frame, so a leading run of null
 *         messages of any length is skipped.
 */
l6_service_t l6_file_service(const char* file);

/**
 * @brief Short name of an L6 service, for logs.
 * @param[in] svc  Service.
 * @return Static string (e.g. "CLAS", "MADOCA", "QZNMA").
 */
const char* l6_service_name(l6_service_t svc);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_L6_H */
