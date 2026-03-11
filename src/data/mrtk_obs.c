/*------------------------------------------------------------------------------
 * mrtk_obs.c : observation data functions
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
 * @file mrtk_obs.c
 * @brief MRTKLIB Observation Module — Implementation.
 *
 * All functions and static data in this file are cut-and-paste extractions
 * from the legacy rtkcmn.c with zero algorithmic changes.
 */

#include "mrtklib/mrtk_obs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_trace.h"

/*============================================================================
 * Forward Declarations (resolved at link time)
 *===========================================================================*/

/* satsys remains in mrtk_nav.c */
extern int satsys(int sat, int* prn);

/* timediff, time2gpst remain in mrtk_time.c */
extern double timediff(gtime_t t1, gtime_t t2);
extern double time2gpst(gtime_t t, int* week);

/*============================================================================
 * Private Constants
 *===========================================================================*/

#define PI 3.1415926535897932 /* pi */
#define R2D (180.0 / PI)      /* rad to deg */

#define SYS_NONE 0x00 /* navigation system: none */
#define SYS_GPS 0x01  /* navigation system: GPS */
#define SYS_SBS 0x02  /* navigation system: SBAS */
#define SYS_GLO 0x04  /* navigation system: GLONASS */
#define SYS_GAL 0x08  /* navigation system: Galileo */
#define SYS_QZS 0x10  /* navigation system: QZSS */
#define SYS_CMP 0x20  /* navigation system: BeiDou */
#define SYS_IRN 0x40  /* navigation system: NavIC */
#define SYS_LEO 0x80  /* navigation system: LEO */
#define SYS_BD2 0x100 /* navigation system: BeiDou-2 */
#define SYS_ALL 0xFF  /* navigation system: all */

#define MAXFREQ 7 /* max NFREQ */

#define FREQ1 1.57542E9       /* L1/E1/B1C  frequency (Hz) */
#define FREQ2 1.22760E9       /* L2         frequency (Hz) */
#define FREQ5 1.17645E9       /* L5/E5a/B2a frequency (Hz) */
#define FREQ6 1.27875E9       /* E6/L6  frequency (Hz) */
#define FREQ7 1.20714E9       /* E5b    frequency (Hz) */
#define FREQ8 1.191795E9      /* E5a+b  frequency (Hz) */
#define FREQ9 2.492028E9      /* S      frequency (Hz) */
#define FREQ1_GLO 1.60200E9   /* GLONASS G1 base frequency (Hz) */
#define DFRQ1_GLO 0.56250E6   /* GLONASS G1 bias frequency (Hz/n) */
#define FREQ2_GLO 1.24600E9   /* GLONASS G2 base frequency (Hz) */
#define DFRQ2_GLO 0.43750E6   /* GLONASS G2 bias frequency (Hz/n) */
#define FREQ3_GLO 1.202025E9  /* GLONASS G3 frequency (Hz) */
#define FREQ1a_GLO 1.600995E9 /* GLONASS G1a frequency (Hz) */
#define FREQ2a_GLO 1.248060E9 /* GLONASS G2a frequency (Hz) */
#define FREQ1_CMP 1.561098E9  /* BDS B1I     frequency (Hz) */
#define FREQ2_CMP 1.20714E9   /* BDS B2I/B2b frequency (Hz) */
#define FREQ3_CMP 1.26852E9   /* BDS B3      frequency (Hz) */

/*============================================================================
 * Static Data Arrays
 *===========================================================================*/

static char* obscodes[] = {
    /* observation code strings */

    "",   "1C", "1P", "1W", "1Y", "1M", "1N", "1S", "1L", "1E", /*  0- 9 */
    "1A", "1B", "1X", "1Z", "2C", "2D", "2S", "2L", "2X", "2P", /* 10-19 */
    "2W", "2Y", "2M", "2N", "5I", "5Q", "5X", "7I", "7Q", "7X", /* 20-29 */
    "6A", "6B", "6C", "6X", "6Z", "6S", "6L", "8L", "8Q", "8X", /* 30-39 */
    "2I", "2Q", "6I", "6Q", "3I", "3Q", "3X", "1I", "1Q", "5A", /* 40-49 */
    "5B", "5C", "9A", "9B", "9C", "9X", "1D", "5D", "5P", "5Z", /* 50-59 */
    "6E", "7D", "7P", "7Z", "8D", "8P", "4A", "4B", "4X", "6D", /* 60-69 */
    "6P", ""                                                    /* 70-   */
};

/*============================================================================
 * Private Functions
 *===========================================================================*/

static int band2idx_fixed(mrtk_band_t band); /* forward declaration */

/* compare observation data -------------------------------------------------*/
static int cmpobs(const void* p1, const void* p2) {
    obsd_t *q1 = (obsd_t*)p1, *q2 = (obsd_t*)p2;
    double tt = timediff(q1->time, q2->time);
    if (fabs(tt) > DTTOL) {
        return tt < 0 ? -1 : 1;
    }
    if (q1->rcv != q2->rcv) {
        return (int)q1->rcv - (int)q2->rcv;
    }
    return (int)q1->sat - (int)q2->sat;
}

/*============================================================================
 * Public Functions
 *===========================================================================*/

/* test SNR mask ---------------------------------------------------------------
 * test SNR mask
 * args   : int    base      I   rover or base-station (0:rover,1:base station)
 *          int    idx       I   frequency index (0:L1,1:L2,2:L3,...)
 *          double el        I   elevation angle (rad)
 *          double snr       I   C/N0 (dBHz)
 *          snrmask_t *mask  I   SNR mask
 * return : status (1:masked,0:unmasked)
 *-----------------------------------------------------------------------------*/
extern int testsnr(int base, int idx, double el, double snr, const snrmask_t* mask) {
    double minsnr, a;
    int i;

    if (!mask->ena[base] || idx < 0 || idx >= NFREQ) {
        return 0;
    }

    a = (el * R2D + 5.0) / 10.0;
    i = (int)floor(a);
    a -= i;
    if (i < 1) {
        minsnr = mask->mask[idx][0];
    } else if (i > 8) {
        minsnr = mask->mask[idx][8];
    } else {
        minsnr = (1.0 - a) * mask->mask[idx][i - 1] + a * mask->mask[idx][i];
    }

    return snr < minsnr;
}
/* test elevation mask ---------------------------------------------------------
 * test elevation mask
 * args   : double *azel     I   azimuth/elevation angle {az,el} (rad)
 *          int16_t *elmask  I   elevation mask vector (360 x 1) (0.1 deg)
 *                                 elmask[i]: elevation mask at azimuth i (deg)
 * return : status (1:masked,0:unmasked)
 *-----------------------------------------------------------------------------*/
extern int testelmask(const double* azel, const int16_t* elmask) {
    double az = azel[0] * R2D;

    az -= floor(az / 360.0) * 360.0; /* 0 <= az < 360.0 */

    return azel[1] * R2D < elmask[(int)floor(az)] * 0.1;
}
/* obs type string to obs code -------------------------------------------------
 * convert obs code type string to obs code
 * args   : char   *str      I   obs code string ("1C","1P","1Y",...)
 * return : obs code (CODE_???)
 * notes  : obs codes are based on RINEX 3.04
 *-----------------------------------------------------------------------------*/
extern uint8_t obs2code(const char* obs) {
    int i;

    for (i = 1; *obscodes[i]; i++) {
        if (strcmp(obscodes[i], obs)) {
            continue;
        }
        return (uint8_t)i;
    }
    return CODE_NONE;
}
/* obs code to obs code string -------------------------------------------------
 * convert obs code to obs code string
 * args   : uint8_t code     I   obs code (CODE_???)
 * return : obs code string ("1C","1P","1P",...)
 * notes  : obs codes are based on RINEX 3.05
 *-----------------------------------------------------------------------------*/
extern char* code2obs(uint8_t code) {
    if (code <= CODE_NONE || MAXCODE < code) {
        return "";
    }
    return obscodes[code];
}
/* system and obs code to frequency index --------------------------------------
 * convert system and obs code to frequency index
 * args   : int    sys       I   satellite system (SYS_???)
 *          uint8_t code     I   obs code (CODE_???)
 * return : frequency index (-1: error)
 *            freq-index    0        1        2        3        4
 *            Signal ID    L1       L2       L3       L4       L5
 *            -----------------------------------------------------
 *            GPS          L1       L2       L5        -        -
 *            GLONASS    G1/G1a   G2/G2a     G3        -        -
 *            Galileo      E1       E5b     E5a       E6      E5a+b
 *            QZSS         L1       L2       L5       L6        -
 *            SBAS         L1       L5       -         -        -
 *            BDS      B1/B1C/B1A B2/B2b    B2a     B3/B3A    B2a+b
 *            NavIC        L5        S       -         -        -
 *-----------------------------------------------------------------------------*/
extern int code2idx(int sys, uint8_t code) {
    int freq_num = code2freq_num(code);
    mrtk_band_t band;
    if (sys == SYS_BD2) {
        sys = SYS_CMP;
    }
    band = mrtk_rinex_freq_to_band(sys, freq_num);
    return band2idx_fixed(band);
}
/* obs code to frequency number ------------------------------------------------
 * extract frequency number from obs code
 * args   : uint8_t code     I   obs code (CODE_???)
 * return : frequency number (1,2,5,...) (0: error)
 *-----------------------------------------------------------------------------*/
extern int code2freq_num(uint8_t code) {
    char str[2] = {0};
    char* obs = code2obs(code);
    if (strlen(obs) < 2) {
        return 0;
    }
    str[0] = obs[0];
    return atoi(str);
}
/* system and obs code to frequency index (obsdef-based) -----------------------
 * convert system and obs code to frequency index using obsdef tables
 * args   : int    sys       I   satellite system (SYS_???)
 *          uint8_t code     I   obs code (CODE_???)
 * return : frequency index (-1: error)
 *            freq-index    0        1        2        3        4
 *            -----------------------------------------------------
 *            GPS          L1       L2       L5        -        -
 *            GLONASS      G1       G2        -        -        -
 *            Galileo      E1      E5a      E5b       E6      E5a+b
 *            QZSS         L1       L5       L2       L6        -
 *            SBAS         L1       L5       -         -        -
 *            BDS         B1I      B3I    B2I/B2b     B1C      B2a
 *            NavIC        L5        S       -         -        -
 *-----------------------------------------------------------------------------*/
extern int code2freq_idx(int sys, uint8_t code) {
    int fn = code2freq_num(code);
    return freq_num2freq_idx(sys, fn);
}
/* system and obs code to frequency --------------------------------------------
 * convert system and obs code to carrier frequency
 * args   : int    sys       I   satellite system (SYS_???)
 *          uint8_t code     I   obs code (CODE_???)
 *          int    fcn       I   frequency channel number for GLONASS
 * return : carrier frequency (Hz) (0.0: error)
 *-----------------------------------------------------------------------------*/
extern double code2freq(int sys, uint8_t code, int fcn) { return freq_num2freq(sys, code2freq_num(code), fcn); }
/* satellite and obs code to frequency -----------------------------------------
 * convert satellite and obs code to carrier frequency
 * args   : int    sat       I   satellite number
 *          uint8_t code     I   obs code (CODE_???)
 *          nav_t  *nav_t    I   navigation data for GLONASS (NULL: not used)
 * return : carrier frequency (Hz) (0.0: error)
 *-----------------------------------------------------------------------------*/
extern double sat2freq(int sat, uint8_t code, const nav_t* nav) {
    int i, fcn = 0, sys, prn;

    sys = satsys_bd2(sat, &prn);

    if (sys == SYS_GLO) {
        if (!nav) {
            return 0.0;
        }
        for (i = 0; i < nav->ng; i++) {
            if (nav->geph[i].sat == sat) {
                break;
            }
        }
        if (i < nav->ng) {
            fcn = nav->geph[i].frq;
        } else if (nav->glo_fcn[prn - 1] > 0) {
            fcn = nav->glo_fcn[prn - 1] - 8;
        } else {
            return 0.0;
        }
    }
    return code2freq(sys, code, fcn);
}
/* get code priority -----------------------------------------------------------
 * get code priority for multiple codes in a frequency
 * uses structured SIG_PRIORITY_TABLE via mrtk_get_signal_priority()
 * args   : int    sys       I   system (SYS_???)
 *          uint8_t code     I   obs code (CODE_???)
 *          char   *opt      I   code options (NULL:no option)
 * return : priority (15:highest-1:lowest,0:error)
 *-----------------------------------------------------------------------------*/
extern int getcodepri(int sys, uint8_t code, const char* opt) {
    const char *p, *optstr;
    const uint8_t* plist;
    mrtk_band_t band;
    char *obs, str[8] = "";
    int freq_num, i;

    if (sys == SYS_BD2) {
        sys = SYS_CMP;
    }

    switch (sys) {
        case SYS_GPS:
            optstr = "-GL%2s";
            break;
        case SYS_GLO:
            optstr = "-RL%2s";
            break;
        case SYS_GAL:
            optstr = "-EL%2s";
            break;
        case SYS_QZS:
            optstr = "-JL%2s";
            break;
        case SYS_SBS:
            optstr = "-SL%2s";
            break;
        case SYS_CMP:
            optstr = "-CL%2s";
            break;
        case SYS_IRN:
            optstr = "-IL%2s";
            break;
        default:
            return 0;
    }
    obs = code2obs(code);
    if (!obs[0]) {
        return 0;
    }

    /* parse code options (opt override: exact match = 15, mismatch = 0) */
    for (p = opt; p && (p = strchr(p, '-')); p++) {
        if (sscanf(p, optstr, str) < 1 || str[0] != obs[0]) {
            continue;
        }
        return str[1] == obs[1] ? 15 : 0;
    }

    /* look up priority from structured table */
    freq_num = code2freq_num(code);
    band = mrtk_rinex_freq_to_band(sys, freq_num);
    if (band == MRTK_BAND_UNKNOWN) {
        return 0;
    }
    plist = mrtk_get_signal_priority(sys, band);
    if (!plist) {
        return 0;
    }
    for (i = 0; i < MRTK_MAX_SIG_PRIORITY; i++) {
        if (plist[i] == 0) {
            break;
        }
        if (plist[i] == code) {
            return 14 - i; /* 14=highest .. 1=lowest, matching legacy range */
        }
    }
    return 0;
}
/* sort and unique observation data --------------------------------------------
 * sort and unique observation data by time, rcv, sat
 * args   : obs_t *obs    IO     observation data
 * return : number of epochs
 *-----------------------------------------------------------------------------*/
extern int sortobs(obs_t* obs) {
    int i, j, n;

    trace(NULL, 3, "sortobs: nobs=%d\n", obs->n);

    if (obs->n <= 0) {
        return 0;
    }

    qsort(obs->data, obs->n, sizeof(obsd_t), cmpobs);

    /* delete duplicated data */
    for (i = j = 0; i < obs->n; i++) {
        if (obs->data[i].sat != obs->data[j].sat || obs->data[i].rcv != obs->data[j].rcv ||
            timediff(obs->data[i].time, obs->data[j].time) != 0.0) {
            obs->data[++j] = obs->data[i];
        }
    }
    obs->n = j + 1;

    for (i = n = 0; i < obs->n; i = j, n++) {
        for (j = i + 1; j < obs->n; j++) {
            if (timediff(obs->data[j].time, obs->data[i].time) > DTTOL) {
                break;
            }
        }
    }
    return n;
}

/* signal replace --------------------------------------------------------------
 * replaces the observation data in the obs structure with the data specified
 * by the given frequency and signal code at the specified index
 * args   : obs_t *obs    IO     observation data
 *          int idx       I      observation index
 *          char f        I      freqency number
 *          char *c       I      code type
 *-----------------------------------------------------------------------------*/
extern void signal_replace(obsd_t* obs, int idx, char f, char* c) {
    int i, j;
    char* code;

    for (i = 0; i < NFREQ + NEXOBS; i++) {
        code = code2obs(obs->code[i]);
        for (j = 0; c[j] != '\0'; j++) {
            if (code[0] == f && code[1] == c[j]) {
                break;
            }
        }
        if (c[j] != '\0') {
            break;
        }
    }
    if (i < NFREQ + NEXOBS) {
        obs->SNR[idx] = obs->SNR[i];
        obs->LLI[idx] = obs->LLI[i];
        obs->code[idx] = obs->code[i];
        obs->L[idx] = obs->L[i];
        obs->P[idx] = obs->P[i];
        obs->D[idx] = obs->D[i];
    } else {
        obs->SNR[idx] = obs->LLI[idx] = obs->code[idx] = 0;
        obs->P[idx] = obs->L[idx] = obs->D[idx] = 0.0;
    }
}

/* screen by time --------------------------------------------------------------
 * screening by time start, time end, and time interval
 * args   : gtime_t time  I      time
 *          gtime_t ts    I      time start (ts.time==0:no screening by ts)
 *          gtime_t te    I      time end   (te.time==0:no screening by te)
 *          double  tint  I      time interval (s) (0.0:no screen by tint)
 * return : 1:on condition, 0:not on condition
 *-----------------------------------------------------------------------------*/
extern int screent(gtime_t time, gtime_t ts, gtime_t te, double tint) {
    return (tint <= 0.0 || fmod(time2gpst(time, NULL) + DTTOL, tint) <= DTTOL * 2.0) &&
           (ts.time == 0 || timediff(time, ts) >= -DTTOL) && (te.time == 0 || timediff(time, te) < DTTOL);
}
/* free observation data -------------------------------------------------------
 * free memory for observation data
 * args   : obs_t *obs    IO     observation data
 * return : none
 *-----------------------------------------------------------------------------*/
extern void freeobs(obs_t* obs) {
    free(obs->data);
    obs->data = NULL;
    obs->n = obs->nmax = 0;
}

/*============================================================================
 * Structured Signal Priority Table
 *
 * Derived from RTKLIB 2.4.3 b34 codepris[] (rinex.c), matched to MRTKLIB's
 * obsdef tables (mrtk_nav.c). Each codes[] array lists observation codes in
 * descending priority order; remaining slots are zero-initialized.
 *===========================================================================*/

typedef struct {
    int sys;
    mrtk_sig_priority_t priority;
} sig_priority_entry_t;

/* clang-format off */
static const sig_priority_entry_t SIG_PRIORITY_TABLE[] = {
    /* GPS — obsdef: "CPYWMNSL", "PYWCMNDLSX", "QXI" */
    {
        .sys = SYS_GPS,
        .priority = {
            .band = MRTK_BAND_GPS_L1,
            .codes = {
                CODE_L1C,
                CODE_L1P,
                CODE_L1Y,
                CODE_L1W,
                CODE_L1M,
                CODE_L1N,
                CODE_L1S,
                CODE_L1L,
            },
        },
    },
    {
        .sys = SYS_GPS,
        .priority = {
            .band = MRTK_BAND_GPS_L2,
            .codes = {
                CODE_L2P,
                CODE_L2Y,
                CODE_L2W,
                CODE_L2C,
                CODE_L2M,
                CODE_L2N,
                CODE_L2D,
                CODE_L2L,
                CODE_L2S,
                CODE_L2X,
            },
        },
    },
    {
        .sys = SYS_GPS,
        .priority = {
            .band = MRTK_BAND_GPS_L5,
            .codes = {
                CODE_L5Q,
                CODE_L5X,
                CODE_L5I,
            },
        },
    },

    /* GLONASS — obsdef: "CP", "CP", (G3 not in obsdef but kept for completeness) */
    {
        .sys = SYS_GLO,
        .priority = {
            .band = MRTK_BAND_GLO_G1,
            .codes = {
                CODE_L1C,
                CODE_L1P,
            },
        },
    },
    {
        .sys = SYS_GLO,
        .priority = {
            .band = MRTK_BAND_GLO_G2,
            .codes = {
                CODE_L2C,
                CODE_L2P,
            },
        },
    },
    {
        .sys = SYS_GLO,
        .priority = {
            .band = MRTK_BAND_GLO_G3,
            .codes = {
                CODE_L3I,
                CODE_L3Q,
                CODE_L3X,
            },
        },
    },

    /* Galileo — obsdef: "CABXZ", "QXI", "QXI", "CXE", "QXI" */
    {
        .sys = SYS_GAL,
        .priority = {
            .band = MRTK_BAND_GAL_E1,
            .codes = {
                CODE_L1C,
                CODE_L1A,
                CODE_L1B,
                CODE_L1X,
                CODE_L1Z,
            },
        },
    },
    {
        .sys = SYS_GAL,
        .priority = {
            .band = MRTK_BAND_GAL_E5a,
            .codes = {
                CODE_L5Q,
                CODE_L5X,
                CODE_L5I,
            },
        },
    },
    {
        .sys = SYS_GAL,
        .priority = {
            .band = MRTK_BAND_GAL_E5b,
            .codes = {
                CODE_L7Q,
                CODE_L7X,
                CODE_L7I,
            },
        },
    },
    {
        .sys = SYS_GAL,
        .priority = {
            .band = MRTK_BAND_GAL_E6,
            .codes = {
                CODE_L6C,
                CODE_L6X,
                CODE_L6E,
            },
        },
    },
    {
        .sys = SYS_GAL,
        .priority = {
            .band = MRTK_BAND_GAL_E5ab,
            .codes = {
                CODE_L8Q,
                CODE_L8X,
                CODE_L8I,
            },
        },
    },

    /* QZSS — obsdef: "LXSCE", "QXI", "LXS", "SEZ" */
    {
        .sys = SYS_QZS,
        .priority = {
            .band = MRTK_BAND_QZS_L1,
            .codes = {
                CODE_L1L,
                CODE_L1X,
                CODE_L1S,
                CODE_L1C,
                CODE_L1E,
            },
        },
    },
    {
        .sys = SYS_QZS,
        .priority = {
            .band = MRTK_BAND_QZS_L5,
            .codes = {
                CODE_L5Q,
                CODE_L5X,
                CODE_L5I,
            },
        },
    },
    {
        .sys = SYS_QZS,
        .priority = {
            .band = MRTK_BAND_QZS_L2,
            .codes = {
                CODE_L2L,
                CODE_L2X,
                CODE_L2S,
            },
        },
    },
    {
        .sys = SYS_QZS,
        .priority = {
            .band = MRTK_BAND_QZS_L6,
            .codes = {
                CODE_L6S,
                CODE_L6E,
                CODE_L6Z,
            },
        },
    },

    /* SBAS — obsdef: "C", "IQX" */
    {
        .sys = SYS_SBS,
        .priority = {
            .band = MRTK_BAND_SBS_L1,
            .codes = {
                CODE_L1C,
            },
        },
    },
    {
        .sys = SYS_SBS,
        .priority = {
            .band = MRTK_BAND_SBS_L5,
            .codes = {
                CODE_L5I,
                CODE_L5Q,
                CODE_L5X,
            },
        },
    },

    /* BDS — obsdef: "IQX"(B1I), "IQX"(B3), "DIQX"(B2b), "PXD"(B1C), "PXD"(B2a), "PXD"(B2ab) */
    {
        .sys = SYS_CMP,
        .priority = {
            .band = MRTK_BAND_BDS_B1I,
            .codes = {
                CODE_L2I,
                CODE_L2Q,
                CODE_L2X,
            },
        },
    },
    {
        .sys = SYS_CMP,
        .priority = {
            .band = MRTK_BAND_BDS_B3,
            .codes = {
                CODE_L6I,
                CODE_L6Q,
                CODE_L6X,
            },
        },
    },
    {
        .sys = SYS_CMP,
        .priority = {
            .band = MRTK_BAND_BDS_B2b,
            .codes = {
                CODE_L7D,
                CODE_L7I,
                CODE_L7Q,
                CODE_L7X,
            },
        },
    },
    {
        .sys = SYS_CMP,
        .priority = {
            .band = MRTK_BAND_BDS_B1C,
            .codes = {
                CODE_L1P,
                CODE_L1X,
                CODE_L1D,
            },
        },
    },
    {
        .sys = SYS_CMP,
        .priority = {
            .band = MRTK_BAND_BDS_B2a,
            .codes = {
                CODE_L5P,
                CODE_L5X,
                CODE_L5D,
            },
        },
    },
    {
        .sys = SYS_CMP,
        .priority = {
            .band = MRTK_BAND_BDS_B2ab,
            .codes = {
                CODE_L8P,
                CODE_L8X,
                CODE_L8D,
            },
        },
    },

    /* NavIC/IRNSS — obsdef: "ABCX", "ABCX" */
    {
        .sys = SYS_IRN,
        .priority = {
            .band = MRTK_BAND_IRN_L5,
            .codes = {
                CODE_L5A,
                CODE_L5B,
                CODE_L5C,
                CODE_L5X,
            },
        },
    },
    {
        .sys = SYS_IRN,
        .priority = {
            .band = MRTK_BAND_IRN_S,
            .codes = {
                CODE_L9A,
                CODE_L9B,
                CODE_L9C,
                CODE_L9X,
            },
        },
    },
};
/* clang-format on */

/* mrtk_band2freq_hz: physical band -> base carrier frequency -----------------
 * convert mrtk_band_t to base carrier frequency in Hz.
 * for GLONASS G1/G2, returns the base frequency without FDMA offset.
 * args   : mrtk_band_t band  I  physical frequency band
 * return : base carrier frequency (Hz), 0.0 on error
 *----------------------------------------------------------------------------*/
extern double mrtk_band2freq_hz(mrtk_band_t band) {
    /* clang-format off */
    switch (band) {
        case MRTK_BAND_GPS_L1:  return FREQ1;
        case MRTK_BAND_GPS_L2:  return FREQ2;
        case MRTK_BAND_GPS_L5:  return FREQ5;
        case MRTK_BAND_GLO_G1:  return FREQ1_GLO;
        case MRTK_BAND_GLO_G2:  return FREQ2_GLO;
        case MRTK_BAND_GLO_G3:  return FREQ3_GLO;
        case MRTK_BAND_GAL_E1:  return FREQ1;
        case MRTK_BAND_GAL_E5a: return FREQ5;
        case MRTK_BAND_GAL_E5b: return FREQ7;
        case MRTK_BAND_GAL_E6:  return FREQ6;
        case MRTK_BAND_GAL_E5ab:return FREQ8;
        case MRTK_BAND_QZS_L1:  return FREQ1;
        case MRTK_BAND_QZS_L2:  return FREQ2;
        case MRTK_BAND_QZS_L5:  return FREQ5;
        case MRTK_BAND_QZS_L6:  return FREQ6;
        case MRTK_BAND_SBS_L1:  return FREQ1;
        case MRTK_BAND_SBS_L5:  return FREQ5;
        case MRTK_BAND_BDS_B1I: return FREQ1_CMP;
        case MRTK_BAND_BDS_B1C: return FREQ1;
        case MRTK_BAND_BDS_B2a: return FREQ5;
        case MRTK_BAND_BDS_B2b: return FREQ2_CMP;
        case MRTK_BAND_BDS_B2ab:return FREQ8;
        case MRTK_BAND_BDS_B3:  return FREQ3_CMP;
        case MRTK_BAND_IRN_L5:  return FREQ5;
        case MRTK_BAND_IRN_S:   return FREQ9;
        default:                return 0.0;
    }
    /* clang-format on */
}

/* band2idx_fixed: physical band -> fixed frequency index ---------------------
 * convert mrtk_band_t to the fixed per-system frequency index used by code2idx().
 * this is the "Layer A" mapping that does NOT depend on obsdef table ordering.
 * args   : mrtk_band_t band  I  physical frequency band
 * return : fixed frequency index (0,1,2,...), -1 on error
 *----------------------------------------------------------------------------*/
static int band2idx_fixed(mrtk_band_t band) {
    /* clang-format off */
    switch (band) {
        /* GPS: L1=0, L2=1, L5=2 */
        case MRTK_BAND_GPS_L1:  return 0;
        case MRTK_BAND_GPS_L2:  return 1;
        case MRTK_BAND_GPS_L5:  return 2;
        /* GLO: G1=0, G2=1, G3=2 */
        case MRTK_BAND_GLO_G1:  return 0;
        case MRTK_BAND_GLO_G2:  return 1;
        case MRTK_BAND_GLO_G3:  return 2;
        /* GAL: E1=0, E5b=1, E5a=2, E6=3, E5ab=4 */
        case MRTK_BAND_GAL_E1:  return 0;
        case MRTK_BAND_GAL_E5b: return 1;
        case MRTK_BAND_GAL_E5a: return 2;
        case MRTK_BAND_GAL_E6:  return 3;
        case MRTK_BAND_GAL_E5ab:return 4;
        /* QZS: L1=0, L2=1, L5=2, L6=3 */
        case MRTK_BAND_QZS_L1:  return 0;
        case MRTK_BAND_QZS_L2:  return 1;
        case MRTK_BAND_QZS_L5:  return 2;
        case MRTK_BAND_QZS_L6:  return 3;
        /* SBS: L1=0, L5=1 */
        case MRTK_BAND_SBS_L1:  return 0;
        case MRTK_BAND_SBS_L5:  return 1;
        /* BDS: B1C/B1I=0, B2b=1, B2a=2, B3=3, B2ab=4 */
        case MRTK_BAND_BDS_B1C: return 0;
        case MRTK_BAND_BDS_B1I: return 0;
        case MRTK_BAND_BDS_B2b: return 1;
        case MRTK_BAND_BDS_B2a: return 2;
        case MRTK_BAND_BDS_B3:  return 3;
        case MRTK_BAND_BDS_B2ab:return 4;
        /* IRN: L5=0, S=1 */
        case MRTK_BAND_IRN_L5:  return 0;
        case MRTK_BAND_IRN_S:   return 1;
        default:                return -1;
    }
    /* clang-format on */
}

/* mrtk_rinex_freq_to_band: RINEX frequency digit -> physical band -----------
 * convert RINEX frequency digit (1,2,3,...,9) to mrtk_band_t for a given
 * satellite system.
 * args   : int sys            I  satellite system (SYS_???)
 *          int rinex_freq_id  I  RINEX frequency digit
 * return : mrtk_band_t (MRTK_BAND_UNKNOWN on error)
 *----------------------------------------------------------------------------*/
extern mrtk_band_t mrtk_rinex_freq_to_band(int sys, int rinex_freq_id) {
    switch (sys) {
        case SYS_GPS:
            switch (rinex_freq_id) {
                case 1:
                    return MRTK_BAND_GPS_L1;
                case 2:
                    return MRTK_BAND_GPS_L2;
                case 5:
                    return MRTK_BAND_GPS_L5;
            }
            break;
        case SYS_GLO:
            switch (rinex_freq_id) {
                case 1:
                case 4:
                    return MRTK_BAND_GLO_G1; /* G1 FDMA + G1a CDMA */
                case 2:
                case 6:
                    return MRTK_BAND_GLO_G2; /* G2 FDMA + G2a CDMA */
                case 3:
                    return MRTK_BAND_GLO_G3;
            }
            break;
        case SYS_GAL:
            switch (rinex_freq_id) {
                case 1:
                    return MRTK_BAND_GAL_E1;
                case 5:
                    return MRTK_BAND_GAL_E5a;
                case 7:
                    return MRTK_BAND_GAL_E5b;
                case 8:
                    return MRTK_BAND_GAL_E5ab;
                case 6:
                    return MRTK_BAND_GAL_E6;
            }
            break;
        case SYS_QZS:
            switch (rinex_freq_id) {
                case 1:
                    return MRTK_BAND_QZS_L1;
                case 2:
                    return MRTK_BAND_QZS_L2;
                case 5:
                    return MRTK_BAND_QZS_L5;
                case 6:
                    return MRTK_BAND_QZS_L6;
            }
            break;
        case SYS_SBS:
            switch (rinex_freq_id) {
                case 1:
                    return MRTK_BAND_SBS_L1;
                case 5:
                    return MRTK_BAND_SBS_L5;
            }
            break;
        case SYS_CMP:
            switch (rinex_freq_id) {
                case 1:
                    return MRTK_BAND_BDS_B1C;
                case 2:
                    return MRTK_BAND_BDS_B1I;
                case 5:
                    return MRTK_BAND_BDS_B2a;
                case 7:
                    return MRTK_BAND_BDS_B2b;
                case 8:
                    return MRTK_BAND_BDS_B2ab;
                case 6:
                    return MRTK_BAND_BDS_B3;
            }
            break;
        case SYS_IRN:
            switch (rinex_freq_id) {
                case 5:
                    return MRTK_BAND_IRN_L5;
                case 9:
                    return MRTK_BAND_IRN_S;
            }
            break;
        default:
            break;
    }
    return MRTK_BAND_UNKNOWN;
}

/* mrtk_get_signal_priority: get priority-ordered code list -------------------
 * look up the priority-ordered observation code list for a (system, band) pair
 * args   : int         sys   I  satellite system (SYS_???)
 *          mrtk_band_t band  I  physical frequency band
 * return : pointer to code array (MRTK_MAX_SIG_PRIORITY elements), or NULL
 *----------------------------------------------------------------------------*/
extern const uint8_t* mrtk_get_signal_priority(int sys, mrtk_band_t band) {
    int i;

    if (band == MRTK_BAND_UNKNOWN || band >= MRTK_BAND_MAX) {
        return NULL;
    }
    for (i = 0; i < (int)(sizeof(SIG_PRIORITY_TABLE) / sizeof(SIG_PRIORITY_TABLE[0])); i++) {
        if (SIG_PRIORITY_TABLE[i].sys == sys && SIG_PRIORITY_TABLE[i].priority.band == band) {
            return SIG_PRIORITY_TABLE[i].priority.codes;
        }
    }
    return NULL;
}

/* mrtk_band_to_freq_num: physical band -> RINEX frequency number ------------
 * reverse of mrtk_rinex_freq_to_band(). Converts mrtk_band_t back to the
 * RINEX frequency digit for a given satellite system.
 * args   : int         sys   I  satellite system (SYS_???)
 *          mrtk_band_t band  I  physical frequency band
 * return : RINEX frequency number (1,2,5,...) or 0 on error
 *----------------------------------------------------------------------------*/
extern int mrtk_band_to_freq_num(int sys, mrtk_band_t band) {
    switch (sys) {
        case SYS_GPS:
            switch (band) {
                case MRTK_BAND_GPS_L1:
                    return 1;
                case MRTK_BAND_GPS_L2:
                    return 2;
                case MRTK_BAND_GPS_L5:
                    return 5;
                default:
                    return 0;
            }
        case SYS_GLO:
            switch (band) {
                case MRTK_BAND_GLO_G1:
                    return 1;
                case MRTK_BAND_GLO_G2:
                    return 2;
                case MRTK_BAND_GLO_G3:
                    return 3;
                default:
                    return 0;
            }
        case SYS_GAL:
            switch (band) {
                case MRTK_BAND_GAL_E1:
                    return 1;
                case MRTK_BAND_GAL_E5a:
                    return 5;
                case MRTK_BAND_GAL_E5b:
                    return 7;
                case MRTK_BAND_GAL_E6:
                    return 6;
                case MRTK_BAND_GAL_E5ab:
                    return 8;
                default:
                    return 0;
            }
        case SYS_QZS:
            switch (band) {
                case MRTK_BAND_QZS_L1:
                    return 1;
                case MRTK_BAND_QZS_L2:
                    return 2;
                case MRTK_BAND_QZS_L5:
                    return 5;
                case MRTK_BAND_QZS_L6:
                    return 6;
                default:
                    return 0;
            }
        case SYS_SBS:
            switch (band) {
                case MRTK_BAND_SBS_L1:
                    return 1;
                case MRTK_BAND_SBS_L5:
                    return 5;
                default:
                    return 0;
            }
        case SYS_CMP:
            switch (band) {
                case MRTK_BAND_BDS_B1C:
                    return 1;
                case MRTK_BAND_BDS_B1I:
                    return 2;
                case MRTK_BAND_BDS_B2a:
                    return 5;
                case MRTK_BAND_BDS_B3:
                    return 6;
                case MRTK_BAND_BDS_B2b:
                    return 7;
                case MRTK_BAND_BDS_B2ab:
                    return 8;
                default:
                    return 0;
            }
        case SYS_IRN:
            switch (band) {
                case MRTK_BAND_IRN_L5:
                    return 5;
                case MRTK_BAND_IRN_S:
                    return 9;
                default:
                    return 0;
            }
        default:
            return 0;
    }
}

/* mrtk_parse_signal_str: parse RINEX3-style signal string -------------------
 * parse a signal string like "G1C" into (system, band, code) components.
 * format: {sys_char}{freq_digit}{attr_char} (3 chars)
 * args   : const char *str   I  signal string (e.g., "G1C", "E5Q", "J2X")
 *          int        *sys   O  satellite system (SYS_???)
 *          mrtk_band_t *band O  physical frequency band
 *          uint8_t    *code  O  observation code (CODE_???)
 * return : 0: success, -1: error
 *----------------------------------------------------------------------------*/
extern int mrtk_parse_signal_str(const char* str, int* sys, mrtk_band_t* band, uint8_t* code) {
    char obs_str[3];
    int freq_digit;

    if (!str || strlen(str) < 3) {
        return -1;
    }

    /* parse system character */
    switch (str[0]) {
        case 'G':
            *sys = SYS_GPS;
            break;
        case 'R':
            *sys = SYS_GLO;
            break;
        case 'E':
            *sys = SYS_GAL;
            break;
        case 'J':
            *sys = SYS_QZS;
            break;
        case 'S':
            *sys = SYS_SBS;
            break;
        case 'C':
            *sys = SYS_CMP;
            break;
        case 'I':
            *sys = SYS_IRN;
            break;
        default:
            return -1;
    }

    /* parse frequency digit */
    freq_digit = str[1] - '0';
    if (freq_digit < 1 || freq_digit > 9) {
        return -1;
    }
    *band = mrtk_rinex_freq_to_band(*sys, freq_digit);
    if (*band == MRTK_BAND_UNKNOWN) {
        return -1;
    }

    /* parse observation code: freq_digit + attribute char → obs code string */
    obs_str[0] = str[1]; /* freq digit */
    obs_str[1] = str[2]; /* attribute char */
    obs_str[2] = '\0';
    *code = obs2code(obs_str);
    if (*code == CODE_NONE) {
        return -1;
    }

    return 0;
}

/* sys index for sigcfg array ------------------------------------------------*/
static int sys2sigcfg_idx(int sys) {
    switch (sys) {
        case SYS_GPS:
            return 0;
        case SYS_GLO:
            return 1;
        case SYS_GAL:
            return 2;
        case SYS_QZS:
            return 3;
        case SYS_SBS:
            return 4;
        case SYS_CMP:
            return 5;
        case SYS_IRN:
            return 6;
        default:
            return -1;
    }
}

/* mrtk_sigcfg_from_signals: build sigcfg from signal string array -----------
 * parse an array of RINEX3-style signal strings (e.g., "G1C", "E5Q") and
 * populate per-constellation sigcfg arrays. Also derives nf as the maximum
 * number of signals configured for any single constellation.
 * args   : const char **sigs  I  array of signal strings
 *          int         nsig   I  number of signal strings
 *          mrtk_sigcfg_t *cfg O  per-constellation signal config [MRTK_NSYS]
 *          int         *nf    O  derived number of frequencies
 * return : 0: success, -1: error
 *----------------------------------------------------------------------------*/
extern int mrtk_sigcfg_from_signals(const char** sigs, int nsig, mrtk_sigcfg_t* cfg, int* nf) {
    int i, sys_idx, max_nf = 0;
    int sys;
    mrtk_band_t band;
    uint8_t code;

    if (!sigs || !cfg || !nf || nsig <= 0) {
        return -1;
    }

    /* zero-initialize all configs */
    memset(cfg, 0, sizeof(mrtk_sigcfg_t) * MRTK_NSYS);

    for (i = 0; i < nsig; i++) {
        if (mrtk_parse_signal_str(sigs[i], &sys, &band, &code) != 0) {
            trace(NULL, 2, "mrtk_sigcfg_from_signals: invalid signal '%s'\n", sigs[i]);
            return -1;
        }

        sys_idx = sys2sigcfg_idx(sys);
        if (sys_idx < 0) {
            return -1;
        }

        if (cfg[sys_idx].nsig >= MRTK_MAXSIG_PER_SYS) {
            trace(NULL, 2, "mrtk_sigcfg_from_signals: too many signals for sys=%d\n", sys);
            return -1;
        }

        cfg[sys_idx].sig[cfg[sys_idx].nsig].band = band;
        cfg[sys_idx].sig[cfg[sys_idx].nsig].preferred_code = code;
        cfg[sys_idx].nsig++;
    }

    /* derive nf as max signals across all constellations */
    for (i = 0; i < MRTK_NSYS; i++) {
        if (cfg[i].nsig > max_nf) {
            max_nf = cfg[i].nsig;
        }
    }
    *nf = max_nf;

    return 0;
}

/* mrtk_sigcfg_to_obsdef: bridge sigcfg to existing obsdef tables ------------
 * for each constellation with nsig > 0, convert sigcfg bands to RINEX
 * frequency numbers and call set_obsdef() to rearrange the obsdef table.
 * args   : const mrtk_sigcfg_t *cfg  I  per-constellation signal config [MRTK_NSYS]
 * return : 0: success, -1: error
 *----------------------------------------------------------------------------*/
extern int mrtk_sigcfg_to_obsdef(const mrtk_sigcfg_t* cfg) {
    static const int sys_list[MRTK_NSYS] = {SYS_GPS, SYS_GLO, SYS_GAL, SYS_QZS, SYS_SBS, SYS_CMP, SYS_IRN};
    int freq_nums[MAXFREQ];
    int i, j;

    if (!cfg) {
        return -1;
    }

    for (i = 0; i < MRTK_NSYS; i++) {
        if (cfg[i].nsig <= 0) {
            continue;
        }

        /* build freq_num array from sigcfg bands */
        memset(freq_nums, 0, sizeof(freq_nums));
        for (j = 0; j < cfg[i].nsig && j < MAXFREQ; j++) {
            freq_nums[j] = mrtk_band_to_freq_num(sys_list[i], cfg[i].sig[j].band);
            if (freq_nums[j] == 0) {
                trace(NULL, 2, "mrtk_sigcfg_to_obsdef: invalid band %d for sys=%02x\n", cfg[i].sig[j].band,
                      sys_list[i]);
                return -1;
            }
        }

        set_obsdef(sys_list[i], freq_nums);
    }
    return 0;
}
