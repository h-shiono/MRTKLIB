/*------------------------------------------------------------------------------
 * mrtk_bias_sinex.h : BIAS-SINEX type definitions and functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_bias_sinex.h
 * @brief MRTKLIB Bias SINEX Module — BIAS-SINEX data types and I/O functions.
 *
 * This header provides the BIAS-SINEX data structures (bia_t, biasat_t,
 * biasta_t, biass_t) and all related I/O and update functions, extracted
 * from rtklib.h and biassnx.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_bias_sinex.c.
 */
#ifndef MRTK_BIAS_SINEX_H
#define MRTK_BIAS_SINEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"

/*============================================================================
 * Bias Data Types
 *===========================================================================*/

typedef struct {        /* BIAS data record type */
    int type;           /* bias type(0:DSB,1:ISB,2:OSB)*/
    gtime_t t0,t1;      /* start, end time */
    int code[2];        /* signal CODE_xxx */
    double val, valstd; /* estimated value, std */
    double slp, slpstd; /* estimated slope, std */
} bia_t;

typedef struct {        /* satellite BIAS data record type */
    int ncb[MAXSAT],ncbmax[MAXSAT]; /* number of code biases */
    int rcb[MAXSAT];    /* bookmark of code biases */
    int npb[MAXSAT],npbmax[MAXSAT]; /* number of phase biases */
    int rpb[MAXSAT];    /* bookmark of phase biases */
    bia_t *cb[MAXSAT];  /* code biases data record */
    bia_t *pb[MAXSAT];  /* phase biases data record */
} biasat_t;

typedef struct {        /* station BIAS data record type */
    char name[10];      /* station name */
    int nsyscb[MAXBSNXSYS],nsyscbmax[MAXBSNXSYS]; /* number of system code biases */
    int rsyscb[MAXBSNXSYS]; /* bookmark of system code biases */
    int nsatcb[MAXSAT],nsatcbmax[MAXSAT]; /* number of satellite code biases */
    int rsatcb[MAXSAT]; /* bookmark of satellite code biases */
    bia_t *syscb[MAXBSNXSYS]; /* system code biases data record */
    bia_t *satcb[MAXSAT]; /* satellite code biases data record */
} biasta_t;

typedef struct {        /* BIAS-SINEX data type */
    int nsta;           /* number of stations */
    biasat_t sat;       /* satellite biases data record */
    biasta_t sta[MAXSTA]; /* station biases data record */
    int refcode[MAXBSNXSYS][2]; /* reference observables */
} biass_t;

/*============================================================================
 * Bias SINEX Functions
 *===========================================================================*/

/**
 * @brief Get system code number based on satellite number.
 * @param[in] sat  Satellite number
 * @return System code number (0 or positive: valid, -1: error)
 */
int getsysno(int sat);

/**
 * @brief Get pointer to the bias structure.
 * @return Pointer to bias structure
 */
biass_t *getbiass(void);

/**
 * @brief Add a bias entry to the bias array.
 * @param[in]  bia   Bias entry to add
 * @param[out] ary   Pointer to bias array
 * @param[out] n     Current number of entries in array
 * @param[out] nmax  Current maximum number of entries in array
 */
void addbia(const bia_t *bia, bia_t **ary, int *n, int *nmax);

/**
 * @brief Read BIAS-SINEX file.
 * @param[in] file  SINEX file path
 * @return Number of biases read (0: error)
 */
int readbsnx(const char *file);

/**
 * @brief Output BIAS-SINEX file header.
 * @param[in] fp      Output file pointer
 * @param[in] ts      Start time
 * @param[in] te      End time
 * @param[in] agency  Agency name
 */
void outbsnxh(FILE *fp, gtime_t ts, gtime_t te, const char *agency);

/**
 * @brief Output FILE/REFERENCE header.
 * @param[in] fp  Output file pointer
 */
void outbsnxrefh(FILE *fp);

/**
 * @brief Output FILE/REFERENCE body line.
 * @param[in] fp       Output file pointer
 * @param[in] type     Type of information
 * @param[in] reftext  Reference text to output
 */
void outbsnxrefb(FILE *fp, int type, char *reftext);

/**
 * @brief Output FILE/REFERENCE footer.
 * @param[in] fp  Output file pointer
 */
void outbsnxreff(FILE *fp);

/**
 * @brief Output FILE/COMMENT header.
 * @param[in] fp  Output file pointer
 */
void outbsnxcomh(FILE *fp);

/**
 * @brief Output FILE/COMMENT body line.
 * @param[in] fp       Output file pointer
 * @param[in] comtext  Comment text to output
 */
void outbsnxcomb(FILE *fp, char *comtext);

/**
 * @brief Output FILE/COMMENT footer.
 * @param[in] fp  Output file pointer
 */
void outbsnxcomf(FILE *fp);

/**
 * @brief Output BIAS/SOLUTION section.
 * @param[in] fp  Output file pointer
 */
void outbsnxsol(FILE *fp);

/**
 * @brief Update satellite OSB structure with current biases.
 * @param[out] osb   OSB structure to update
 * @param[in]  gt    Current time
 * @param[in]  mode  Mode (0: current, 1: all)
 * @return Number of updated biases
 */
int udosb_sat(osb_t *osb, gtime_t gt, int mode);

/**
 * @brief Update station OSB structure with current biases.
 * @param[out] osb   OSB structure to update
 * @param[in]  gt    Current time
 * @param[in]  mode  Mode (0: current, 1: all)
 * @param[in]  name  Station name
 * @return Number of updated biases
 */
int udosb_station(osb_t *osb, gtime_t gt, int mode, char *name);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_BIAS_SINEX_H */
