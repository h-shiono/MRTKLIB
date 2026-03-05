/*------------------------------------------------------------------------------
 * mrtk_obs.h : observation data type definitions and functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2015- Mitsubishi Electric Corp.
 * Copyright (C) 2014 Geospatial Information Authority of Japan
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_obs.h
 * @brief MRTKLIB Observation Module — Observation data types and utility
 *        functions.
 *
 * This header provides the fundamental GNSS observation data types
 * (obsd_t, obs_t, snrmask_t) and observation utility functions extracted
 * from rtklib.h/rtkcmn.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_obs.c.
 */
#ifndef MRTK_OBS_H
#define MRTK_OBS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"

/*============================================================================
 * Observation Data Types
 *===========================================================================*/

/**
 * @brief Observation data record.
 */
typedef struct {
    gtime_t time;       /* receiver sampling time (GPST) */
    uint8_t sat,rcv;    /* satellite/receiver number */
    uint16_t SNR[NFREQ+NEXOBS]; /* signal strength (0.001 dBHz) */
    uint8_t  LLI[NFREQ+NEXOBS]; /* loss of lock indicator */
    uint8_t code[NFREQ+NEXOBS]; /* code indicator (CODE_???) */
    double L[NFREQ+NEXOBS]; /* observation data carrier-phase (cycle) */
    double P[NFREQ+NEXOBS]; /* observation data pseudorange (m) */
    float  D[NFREQ+NEXOBS]; /* observation data doppler frequency (Hz) */
    int    facility;    /* L6 facility ID (for dual-channel VRS) */
} obsd_t;

/**
 * @brief Observation data collection.
 */
typedef struct {
    int n,nmax;         /* number of obervation data/allocated */
    obsd_t *data;       /* observation data records */
} obs_t;

/**
 * @brief SNR mask type.
 */
typedef struct {
    int ena[2];         /* enable flag {rover,base} */
    double mask[NFREQ][9]; /* mask (dBHz) at 5,10,...85 deg */
} snrmask_t;

/*============================================================================
 * Observation Code Functions
 *===========================================================================*/

/**
 * @brief Convert obs code type string to obs code.
 * @param[in] obs  Obs code string ("1C","1P","1Y",...)
 * @return Obs code (CODE_???)
 */
uint8_t obs2code(const char *obs);

/**
 * @brief Convert obs code to obs code string.
 * @param[in] code  Obs code (CODE_???)
 * @return Obs code string ("1C","1P",...)
 */
char *code2obs(uint8_t code);

/**
 * @brief Convert system and obs code to frequency index.
 * @param[in] sys   Satellite system (SYS_???)
 * @param[in] code  Obs code (CODE_???)
 * @return Frequency index (-1: error)
 */
int code2idx(int sys, uint8_t code);

/**
 * @brief Extract frequency number from obs code.
 * @param[in] code  Obs code (CODE_???)
 * @return Frequency number (1,2,5,...) (0: error)
 */
int code2freq_num(uint8_t code);

/**
 * @brief Convert system and obs code to frequency index (obsdef-based).
 * @param[in] sys   Satellite system (SYS_???)
 * @param[in] code  Obs code (CODE_???)
 * @return Frequency index (-1: error)
 */
int code2freq_idx(int sys, uint8_t code);

/**
 * @brief Convert system and obs code to carrier frequency.
 * @param[in] sys   Satellite system (SYS_???)
 * @param[in] code  Obs code (CODE_???)
 * @param[in] fcn   Frequency channel number for GLONASS
 * @return Carrier frequency (Hz) (0.0: error)
 */
double code2freq(int sys, uint8_t code, int fcn);

/**
 * @brief Set code priority for multiple codes in a frequency.
 * @param[in] sys  System (or of SYS_???)
 * @param[in] idx  Frequency index (0- )
 * @param[in] pri  Priority of codes (series of code characters)
 */
void setcodepri(int sys, int idx, const char *pri);

/**
 * @brief Get code priority for multiple codes in a frequency.
 * @param[in] sys   System (SYS_???)
 * @param[in] code  Obs code (CODE_???)
 * @param[in] opt   Code options (NULL:no option)
 * @return Priority (15:highest-1:lowest, 0:error)
 */
int getcodepri(int sys, uint8_t code, const char *opt);

/*============================================================================
 * Observation Utility Functions
 *===========================================================================*/

/**
 * @brief Sort observation data by time, receiver, and satellite.
 * @param[in,out] obs  Observation data to sort
 * @return Number of epochs
 */
int sortobs(obs_t *obs);

/**
 * @brief Replace signal type in observation data.
 * @param[in,out] obs  Observation data record
 * @param[in]     idx  Frequency index
 * @param[in]     f    Frequency character
 * @param[in,out] c    Signal code string
 */
void signal_replace(obsd_t *obs, int idx, char f, char *c);

/**
 * @brief Screen data by time range and interval.
 * @param[in] time  Time to check (GPST)
 * @param[in] ts    Start time (GPST) (ts.time==0: no start limit)
 * @param[in] te    End time (GPST) (te.time==0: no end limit)
 * @param[in] tint  Time interval (s) (0.0: no interval limit)
 * @return 1:on, 0:off
 */
int screent(gtime_t time, gtime_t ts, gtime_t te, double tint);

/**
 * @brief Test SNR mask.
 * @param[in] base  Rover or base-station (0:rover, 1:base station)
 * @param[in] idx   Frequency index (0:L1, 1:L2, 2:L3,...)
 * @param[in] el    Elevation angle (rad)
 * @param[in] snr   C/N0 (dBHz)
 * @param[in] mask  SNR mask
 * @return Status (1:masked, 0:unmasked)
 */
int testsnr(int base, int idx, double el, double snr,
            const snrmask_t *mask);

/**
 * @brief Test elevation mask.
 * @param[in] azel    Azimuth/elevation angle {az,el} (rad)
 * @param[in] elmask  Elevation mask vector (360 x 1) (0.1 deg)
 * @return Status (1:masked, 0:unmasked)
 */
int testelmask(const double *azel, const int16_t *elmask);

/**
 * @brief Free observation data memory.
 * @param[in,out] obs  Observation data
 */
void freeobs(obs_t *obs);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_OBS_H */
