/**
 * @file mrtk_obs.h
 * @brief MRTKLIB Observation Module — Observation data types and utility
 *        functions (sortobs, screent, signal_replace).
 *
 * This header provides the fundamental GNSS observation data types
 * (obsd_t, obs_t) extracted from rtklib.h with zero algorithmic changes.
 *
 * @note Functions declared here (sortobs, screent, signal_replace) are still
 *       implemented in rtkcmn.c; only the declarations are moved.
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
} obsd_t;

/**
 * @brief Observation data collection.
 */
typedef struct {
    int n,nmax;         /* number of obervation data/allocated */
    obsd_t *data;       /* observation data records */
} obs_t;

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

#ifdef __cplusplus
}
#endif

#endif /* MRTK_OBS_H */
