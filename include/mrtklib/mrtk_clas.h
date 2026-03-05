/*------------------------------------------------------------------------------
 * mrtk_clas.h : CLAS (QZSS L6D) type definitions and functions
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
 * @file mrtk_clas.h
 * @brief CLAS (QZSS L6D) CSSR decoder and grid correction type definitions.
 *
 * References:
 *   [1] claslib (https://github.com/mf-arai/claslib) — upstream implementation
 *   [2] IS-QZSS-L6-001 — CLAS Interface Specification
 *   [3] IS-QZSS-L6-003 — CLAS Compact SSR Specification
 *
 * Notes:
 *   - Bank struct dimensions are reduced from upstream claslib defaults for
 *     memory efficiency.  See "CLAS Storage Dimension" section below.
 *   - Global singletons in upstream (_cssr, cssrObject[], CurrentCSSR[],
 *     BackupCSSR[]) are replaced with clas_ctx_t context struct passed via
 *     function parameters for thread safety and multi-channel support.
 */
#ifndef MRTK_CLAS_H
#define MRTK_CLAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_rtkpos.h"  /* rtk_t, ssat_t, sol_t, prcopt_t (for OSR functions) */

/*============================================================================
 * CLAS Version
 *===========================================================================*/

#define CLAS_VER  "0.1"                 /* CLAS module version */

/*============================================================================
 * CSSR Protocol Constants (from IS-QZSS-L6-003, claslib/cssr.h)
 *
 * These constants define the CSSR compact SSR message format limits.
 * They match the upstream claslib values and should NOT be reduced.
 *===========================================================================*/

#define CSSR_MAX_GNSS       16          /* max GNSS systems in CSSR */
#define CSSR_MAX_SV_GNSS    40          /* max satellites per GNSS system */
#define CSSR_MAX_SV         64          /* max satellites in CSSR message */
#define CSSR_MAX_SIG        16          /* max signals per satellite */
#define CSSR_MAX_CELLMASK   64          /* max cellmask entries */
#define CSSR_MAX_NET        32          /* max networks in CSSR message */
#define CSSR_MAX_LOCAL_SV   32          /* max local satellites per network */
#define CSSR_MAX_GP        128          /* max grid points in CSSR message */

/*============================================================================
 * CSSR System Identifiers
 *===========================================================================*/

#define CSSR_SYS_GPS         0
#define CSSR_SYS_GLO         1
#define CSSR_SYS_GAL         2
#define CSSR_SYS_BDS         3
#define CSSR_SYS_QZS         4
#define CSSR_SYS_SBS         5
#define CSSR_SYS_NONE       -1

/*============================================================================
 * CSSR Message Subtype Constants
 *===========================================================================*/

#define CSSR_TYPE_NUM       14          /* total subtype count */
#define CSSR_TYPE_MASK       1          /* ST1:  Satellite/signal mask */
#define CSSR_TYPE_OC         2          /* ST2:  Orbit correction */
#define CSSR_TYPE_CC         3          /* ST3:  Clock correction */
#define CSSR_TYPE_CB         4          /* ST4:  Code bias */
#define CSSR_TYPE_PB         5          /* ST5:  Phase bias */
#define CSSR_TYPE_BIAS       6          /* ST6:  Combined bias */
#define CSSR_TYPE_URA        7          /* ST7:  URA */
#define CSSR_TYPE_STEC       8          /* ST8:  STEC correction (CLAS-only) */
#define CSSR_TYPE_GRID       9          /* ST9:  Grid definition (CLAS-only) */
#define CSSR_TYPE_SI        10          /* ST10: Service information (CLAS-only) */
#define CSSR_TYPE_COMBO     11          /* ST11: Combined correction (CLAS-only) */
#define CSSR_TYPE_ATMOS     12          /* ST12: Atmospheric corr. (CLAS-only) */
#define CSSR_TYPE_INIT     254          /* initialization marker */
#define CSSR_TYPE_NULL     255          /* null/invalid marker */

/*============================================================================
 * CSSR Signed-Value Maximum Limits
 *===========================================================================*/

#define P2_S16_MAX      32767
#define P2_S15_MAX      16383
#define P2_S14_MAX       8191
#define P2_S13_MAX       4095
#define P2_S12_MAX       2047
#define P2_S11_MAX       1023
#define P2_S10_MAX        511
#define P2_S9_MAX         255
#define P2_S8_MAX         127
#define P2_S7_MAX          63
#define P2_S6_MAX          31

/*============================================================================
 * CSSR Troposphere Reference Values
 *===========================================================================*/

#define CSSR_TROP_HS_REF    2.3         /* hydrostatic reference (m) */
#define CSSR_TROP_WET_REF   0.252       /* wet delay reference (m) */

/*============================================================================
 * CSSR Update Type Indices (for ssrn_t.update[])
 *===========================================================================*/

#define CSSR_UPDATE_TROP     0
#define CSSR_UPDATE_STEC     1
#define CSSR_UPDATE_PBIAS    2

/*============================================================================
 * CSSR Validity Ages (seconds)
 *===========================================================================*/

#define CSSR_TROPVALIDAGE 3600          /* max troposphere correction age */
#define CSSR_STECVALIDAGE 3600          /* max STEC correction age */

/*============================================================================
 * Special Values
 *===========================================================================*/

#define CSSR_INVALID_VALUE  -10000      /* invalid correction marker */

/*============================================================================
 * L6 Frame Constants
 *===========================================================================*/

#define L6FRMPREAMB     0x1ACFFC1DU     /* L6 frame preamble (4 bytes) */
#define BLEN_MSG        218             /* L6 subframe length (bytes) */

/*============================================================================
 * CSSR Week Reference Indices
 *===========================================================================*/

enum {
    CSSR_REF_MASK = 0, CSSR_REF_ORBIT, CSSR_REF_CLOCK,
    CSSR_REF_CBIAS, CSSR_REF_PBIAS, CSSR_REF_BIAS,
    CSSR_REF_URA, CSSR_REF_STEC, CSSR_REF_GRID,
    CSSR_REF_COMBINED, CSSR_REF_ATMOSPHERIC, CSSR_REF_MAX
};

/*============================================================================
 * CLAS Storage Dimensions (reduced from upstream for memory efficiency)
 *
 * Upstream claslib uses CSSR_MAX_NETWORK=32, *BANKNUM=128, RTCM_SSR_MAX_GP=128
 * which results in ~6.6GB per cssr_bank_control instance.
 *
 * Design decisions (agreed in planning):
 *   - Bank depth:    128 -> 32  (30s UDI × 32 = 960s history, ample)
 *   - Network count:  32 ->  6  (Japan CLAS uses 1-2, margin for boundaries)
 *   - Grid points:   128 -> 80  (practical CLAS grid sizes are 20-70)
 *   - Channels:        2        (L6 CH1 + CH2, same as upstream)
 *===========================================================================*/

#define CLAS_CH_NUM          2          /* L6 channels */
#define CLAS_MAX_NETWORK    13          /* max networks (upstream uses 32, 13 covers networks 1-12) */
#define CLAS_BANK_NUM       32          /* bank depth (ring buffer entries) */
#define CLAS_MAX_GP         80          /* max grid points in bank storage */

/*============================================================================
 * Grid Interpolation Constants
 *===========================================================================*/

#define CLAS_MAX_NGRID       4          /* grid points for interpolation */
#define CLAS_MAXPBCORSSR    20.0        /* max phase bias correction (m) */

/*============================================================================
 * STEC Data Types (from claslib/rtklib.h)
 *===========================================================================*/

typedef struct {                /* STEC data element type */
    gtime_t time;               /* time (GPST) */
    unsigned char sat;          /* satellite number */
    unsigned char slip;         /* slip flag */
    float iono;                 /* L1 ionosphere delay (m) */
    float rate;                 /* L1 ionosphere rate (m/s) */
    float quality;              /* ionosphere quality indicator */
    float rms;                  /* rms value (m) */
    int flag;                   /* status flag */
} stecd_t;

typedef struct {                /* STEC grid type */
    double pos[3];              /* latitude/longitude (deg) */
    int n,nmax;                 /* number of data/allocated */
    stecd_t *data;              /* STEC data (dynamically allocated) */
} stec_t;

/*============================================================================
 * ZWD (Zenith Wet Delay) Data Types (from claslib/rtklib.h)
 *===========================================================================*/

typedef struct {                /* ZWD data element type */
    gtime_t time;               /* time (GPST) */
    unsigned char valid;        /* valid flag (0:invalid, 1:valid) */
    float zwd;                  /* zenith wet delay (m) */
    float ztd;                  /* zenith total delay (m) */
    float rms;                  /* rms value (m) */
    float quality;              /* troposphere quality indicator */
} zwdd_t;

typedef struct {                /* ZWD grid type */
    float pos[3];               /* latitude/longitude (rad) */
    int n,nmax;                 /* number of data/allocated */
    zwdd_t *data;               /* ZWD data (dynamically allocated) */
} zwd_t;

/*============================================================================
 * Ocean Loading Type (from claslib/rtklib.h)
 *===========================================================================*/

typedef struct {                /* ocean loading per network */
    int gridnum;                /* number of grid points */
    double pos[CLAS_MAX_GP][2]; /* grid positions [deg] {lat,lon} */
    double odisp[CLAS_MAX_GP][6*11]; /* ocean tide loading parameters */
} clas_oload_t;

/*============================================================================
 * SSR Ground Point Type (from claslib/rtklib.h)
 *===========================================================================*/

typedef struct {                /* SSR ground point data */
    int type;                   /* grid type */
    double pos[3];              /* position {lat,lon,hgt} */
    int network;                /* network id */
    int update;                 /* update flag */
} clas_gp_t;

/*============================================================================
 * CSSR Decoder Types (from claslib/cssr.h)
 *
 * These represent the decoded CSSR message state.
 * Dimensions use protocol constants (CSSR_MAX_*) since the decoder must
 * handle full protocol capacity.
 *===========================================================================*/

typedef struct {                /* CSSR decoder options */
    int stec_type;              /* STEC correction type */
} cssropt_t;

typedef struct {                /* CSSR per-network correction data */
    gtime_t t0[2];              /* epoch time {trop,stec} (GPST) */
    double udi[2];              /* update interval {trop,stec} (s) */
    int iod[2];                 /* IOD {trop,stec} */
    int ngp;                    /* number of grid points */
    float quality_f[CSSR_MAX_LOCAL_SV]; /* STEC quality per satellite */
    float trop_wet[CSSR_MAX_GP];  /* troposphere wet delay per GP (m) */
    float trop_total[CSSR_MAX_GP]; /* troposphere total delay per GP (m) */
    int nsat_f;                 /* number of STEC satellites */
    int sat_f[CSSR_MAX_LOCAL_SV]; /* STEC satellite list */
    float quality;              /* troposphere quality indicator */
    double a[CSSR_MAX_LOCAL_SV][4]; /* STEC polynomial coefficients */
    int nsat[CSSR_MAX_GP];      /* number of satellites per GP */
    int sat[CSSR_MAX_GP][CSSR_MAX_LOCAL_SV]; /* satellite list per GP */
    float stec[CSSR_MAX_GP][CSSR_MAX_LOCAL_SV]; /* STEC values per GP */
    float stec0[CSSR_MAX_GP][CSSR_MAX_LOCAL_SV]; /* STEC polynomial baseline */
    double grid[CSSR_MAX_GP][3]; /* grid point coordinates {lat,lon,hgt} */
    int update[3];              /* update flags {trop,stec,pbias} */
} ssrn_t;

typedef struct {                /* CSSR main decoder state */
    int ver;                    /* CSSR version */
    cssropt_t opt;              /* decoder options */
    int iod;                    /* IOD SSR */
    int iod_sv;                 /* IOD satellite mask */
    int inet;                   /* current network index */
    int week;                   /* GPS week number */
    uint8_t cmi[CSSR_MAX_GNSS]; /* cellmask existence flag per GNSS */
    uint64_t svmask[CSSR_MAX_GNSS]; /* satellite mask per GNSS */
    uint16_t sigmask[CSSR_MAX_GNSS]; /* signal mask per GNSS */
    uint16_t cellmask[CSSR_MAX_SV]; /* cellmask per satellite */
    uint64_t net_svmask[CSSR_MAX_NET]; /* satellite mask per network */
    int ngnss;                  /* number of GNSS systems */
    int nsat;                   /* number of satellites */
    int ncell;                  /* number of cellmask entries */
    int sat[CSSR_MAX_SV];       /* satellite number list */
    int nsat_n[CSSR_MAX_NET];   /* number of satellites per network */
    int sat_n[CSSR_MAX_NET][CSSR_MAX_LOCAL_SV]; /* sat list per network */
    int nsig[CSSR_MAX_SV];      /* number of signals per satellite */
    int sigmask_s[CSSR_MAX_SV]; /* signal mask start index */
    int amb_bias[MAXSAT][MAXCODE]; /* ambiguity bias flags */
    uint8_t disc[MAXSAT][MAXCODE]; /* discontinuity indicators */
    float quality_i;            /* ionosphere quality */
    int l6delivery;             /* L6 delivery channel */
    int l6facility;             /* L6 facility */
    ssrn_t ssrn[CSSR_MAX_NET]; /* per-network correction data */
    int si_cnt;                 /* service information count */
    int si_sz;                  /* service information size */
    uint64_t si_data[4];        /* service information data */
} cssr_t;

/*============================================================================
 * CLAS Correction Bank Types (from claslib/cssr.c)
 *
 * Ring buffer system for temporal storage of CSSR corrections.
 * Dimensions use CLAS_MAX_* (reduced from upstream) for memory efficiency.
 * All bank structures are heap-allocated via clas_ctx_init().
 *===========================================================================*/

/** Merged correction snapshot — combines orbit/clock/bias/trop for one epoch */
typedef struct {
    /* STEC data per grid point per satellite */
    stecd_t stecdata[CLAS_MAX_GP][MAXSAT];
    stec_t  stec[CLAS_MAX_GP];

    /* ZWD data per grid point */
    zwdd_t  zwddata[CLAS_MAX_GP];
    zwd_t   zwd[CLAS_MAX_GP];

    /* signal biases per satellite */
    double  cbias[MAXSAT][MAXCODE];   /* code bias (m) */
    double  pbias[MAXSAT][MAXCODE];   /* phase bias (m) */
    int     smode[MAXSAT][MAXCODE];   /* signal tracking mode */

    /* orbit corrections per satellite */
    double  deph0[MAXSAT];            /* radial (m) */
    double  deph1[MAXSAT];            /* along-track (m) */
    double  deph2[MAXSAT];            /* cross-track (m) */
    int     iode[MAXSAT];             /* issue of data ephemeris */

    /* clock correction per satellite */
    double  c0[MAXSAT];              /* clock correction (m) */

    /* metadata per satellite per correction type */
    gtime_t time[MAXSAT][6];         /* epoch time */
    double  udi[MAXSAT][6];          /* update interval (s) */
    int     iod[MAXSAT][6];          /* IOD */
    int     prn[MAXSAT][6];          /* PRN flags */
    int     flag[MAXSAT];            /* satellite availability flag */

    /* snapshot timestamps */
    gtime_t update_time;             /* last update time */
    gtime_t orbit_time;              /* orbit correction time */
    gtime_t clock_time;              /* clock correction time */
    gtime_t bias_time;               /* bias correction time */
    gtime_t trop_time;               /* trop correction time */

    /* identification */
    int     facility;                /* L6 facility id */
    int     gridnum;                 /* number of grid points */
    int     network;                 /* network id */
    int     use;                     /* in-use flag */
} clas_corr_t;

/** Orbit correction ring buffer entry */
typedef struct {
    int     use;                     /* in-use flag */
    gtime_t time;                    /* epoch time */
    int     network;                 /* network id */
    int     prn[MAXSAT];             /* satellite availability */
    double  udi[MAXSAT];             /* update interval (s) */
    int     iod[MAXSAT];             /* IOD */
    int     iode[MAXSAT];            /* issue of data ephemeris */
    double  deph0[MAXSAT];           /* radial correction (m) */
    double  deph1[MAXSAT];           /* along-track correction (m) */
    double  deph2[MAXSAT];           /* cross-track correction (m) */
} clas_orbit_bank_t;

/** Clock correction ring buffer entry */
typedef struct {
    int     use;                     /* in-use flag */
    gtime_t time;                    /* epoch time */
    int     network;                 /* network id */
    int     prn[MAXSAT];             /* satellite availability */
    double  udi[MAXSAT];             /* update interval (s) */
    int     iod[MAXSAT];             /* IOD */
    double  c0[MAXSAT];             /* clock correction (m) */
} clas_clock_bank_t;

/** Code/phase bias ring buffer entry */
typedef struct {
    int     use;                     /* in-use flag */
    gtime_t time;                    /* epoch time */
    int     network;                 /* network id */
    int     bflag;                   /* bias type flag */
    int     prn[MAXSAT];             /* satellite availability */
    double  udi[MAXSAT];             /* update interval (s) */
    int     iod[MAXSAT];             /* IOD */
    int     smode[MAXSAT][MAXCODE];  /* signal tracking mode */
    int     sflag[MAXSAT][MAXCODE];  /* signal validity flag */
    double  cbias[MAXSAT][MAXCODE];  /* code bias (m) */
    double  pbias[MAXSAT][MAXCODE];  /* phase bias (m) */
} clas_bias_bank_t;

/** Troposphere/ionosphere grid ring buffer entry */
typedef struct {
    int     use;                     /* in-use flag */
    gtime_t time;                    /* epoch time */
    int     gridnum[CLAS_MAX_NETWORK]; /* grid point count per network */
    double  gridpos[CLAS_MAX_NETWORK][CLAS_MAX_GP][3]; /* grid positions */
    double  total[CLAS_MAX_NETWORK][CLAS_MAX_GP];   /* trop total delay (m) */
    double  wet[CLAS_MAX_NETWORK][CLAS_MAX_GP];     /* trop wet delay (m) */
    int     satnum[CLAS_MAX_NETWORK][CLAS_MAX_GP];  /* sat count per GP */
    int     prn[CLAS_MAX_NETWORK][CLAS_MAX_GP][MAXSAT]; /* sat availability */
    double  iono[CLAS_MAX_NETWORK][CLAS_MAX_GP][MAXSAT]; /* STEC per GP (m) */
} clas_trop_bank_t;

/** Latest troposphere/STEC state (non-ring-buffer, single instance) */
typedef struct {
    int     gridnum[CLAS_MAX_NETWORK]; /* grid point count per network */
    double  gridpos[CLAS_MAX_NETWORK][CLAS_MAX_GP][3]; /* grid positions */
    gtime_t troptime[CLAS_MAX_NETWORK][CLAS_MAX_GP]; /* trop update time */
    double  total[CLAS_MAX_NETWORK][CLAS_MAX_GP];    /* trop total delay */
    double  wet[CLAS_MAX_NETWORK][CLAS_MAX_GP];      /* trop wet delay */
    gtime_t stectime[CLAS_MAX_NETWORK][CLAS_MAX_GP][MAXSAT]; /* STEC time */
    int     prn[CLAS_MAX_NETWORK][CLAS_MAX_GP][MAXSAT]; /* sat availability */
    double  stec0[CLAS_MAX_NETWORK][CLAS_MAX_GP][MAXSAT]; /* STEC0 (m) */
    double  stec[CLAS_MAX_NETWORK][CLAS_MAX_GP][MAXSAT];  /* STEC (m) */
} clas_latest_trop_t;

/** Bank control — manages all ring buffers for one L6 channel */
typedef struct {
    clas_orbit_bank_t  OrbitBank[CLAS_BANK_NUM];
    clas_clock_bank_t  ClockBank[CLAS_BANK_NUM];
    clas_bias_bank_t   BiasBank[CLAS_BANK_NUM];
    clas_trop_bank_t   TropBank[CLAS_BANK_NUM];
    int     fastfix[CLAS_MAX_NETWORK]; /* fast fix status per network */
    clas_latest_trop_t LatestTrop;     /* latest trop/STEC snapshot */
    int     Facility;                  /* L6 facility id */
    int     separation;                /* correction separation flag */
    int     NextOrbit;                 /* next orbit bank write index */
    int     NextClock;                 /* next clock bank write index */
    int     NextBias;                  /* next bias bank write index */
    int     NextTrop;                  /* next trop bank write index */
    int     use;                       /* in-use flag */
} clas_bank_ctrl_t;

/*============================================================================
 * Grid Interpolation Types (from claslib/cssr2osr.h)
 *===========================================================================*/

/** Grid interpolation result (weights and indices for surrounding GPs) */
typedef struct {
    double  Gmat[CLAS_MAX_NGRID * CLAS_MAX_NGRID]; /* interpolation matrix */
    double  weight[CLAS_MAX_NGRID]; /* interpolation weights */
    double  Emat[CLAS_MAX_NGRID];   /* interpolation errors */
    int     index[CLAS_MAX_NGRID];  /* selected grid point indices */
    int     network;                /* network id */
    int     num;                    /* number of selected grid points */
} clas_grid_t;

/*============================================================================
 * OSR (Observation Space Representation) Data Type (from claslib/rtklib.h)
 *
 * Used by the PPP-RTK positioning engine (Phase 4).
 *===========================================================================*/

typedef struct {                /* observation space representation record */
    gtime_t time;               /* receiver sampling time (GPST) */
    unsigned char sat;          /* satellite number */
    unsigned char refsat;       /* reference satellite number */
    int     iode;               /* IODE */
    double  clk;                /* satellite clock correction by SSR (m) */
    double  orb;                /* satellite orbit correction by SSR (m) */
    double  trop;               /* troposphere delay correction (m) */
    double  iono;               /* ionosphere correction by SSR (m) */
    double  age;                /* SSR correction age (s) */
    double  cbias[NFREQ+NEXOBS]; /* code bias per signal (m) */
    double  pbias[NFREQ+NEXOBS]; /* phase bias per signal (m) */
    double  l0bias;             /* L0 phase bias (m) */
    double  discontinuity[NFREQ+NEXOBS]; /* discontinuity indicator */
    double  rho;                /* satellite-receiver distance (m) */
    double  dts;                /* satellite clock from broadcast (s) */
    double  relatv;             /* relativistic delay / Shapiro (m) */
    double  earthtide;          /* solid earth tide correction (m) */
    double  antr[NFREQ+NEXOBS]; /* receiver antenna PCV (m) */
    double  wupL[NFREQ+NEXOBS]; /* phase windup correction (m) */
    double  compL[NFREQ+NEXOBS]; /* time variation compensation (m) */
    double  CPC[NFREQ+NEXOBS];  /* carrier-phase correction (m) */
    double  PRC[NFREQ+NEXOBS];  /* pseudorange correction (m) */
    double  dCPC[NFREQ+NEXOBS]; /* SD carrier-phase correction (m) */
    double  dPRC[NFREQ+NEXOBS]; /* SD pseudorange correction (m) */
    double  c[NFREQ+NEXOBS];    /* carrier-phase from model (m) */
    double  p[NFREQ+NEXOBS];    /* pseudorange from model (m) */
    double  resc[NFREQ+NEXOBS]; /* carrier-phase residual (m) */
    double  resp[NFREQ+NEXOBS]; /* pseudorange residual (m) */
    double  dresc[NFREQ+NEXOBS]; /* SD carrier-phase residual (m) */
    double  dresp[NFREQ+NEXOBS]; /* SD pseudorange residual (m) */
    double  ddisp;              /* SD dispersive residual (m) */
    double  dL0;                /* SD non-dispersive residual (m) */
    double  sis;                /* signal-in-space error */
} clas_osrd_t;

typedef clas_osrd_t osrd_t;  /* upstream compat alias */

/*============================================================================
 * CLAS Decoder Per-Satellite Buffer (replaces upstream rtcm->nav.ssr[])
 *
 * The CLAS decoder writes per-satellite corrections here instead of
 * writing directly to nav_t.ssr[].  Bank management functions read
 * from this buffer.  Fields match upstream claslib ssr_t semantics.
 *===========================================================================*/

typedef struct {
    gtime_t t0[6];              /* epoch time per corr type (GPST) */
    double  udi[6];             /* update interval (s) */
    int     iod[6];             /* IOD per correction type */
    int     iode;               /* issue of data ephemeris */
    int     ura;                /* URA class value */
    double  deph[3];            /* orbit {radial,along,cross} (m) */
    double  ddeph[3];           /* orbit rate (m/s) */
    double  dclk[3];            /* clock {c0,c1,c2} */
    double  cbias[MAXCODE];     /* code bias (m) — double for decode precision */
    double  pbias[MAXCODE];     /* phase bias (m) */
    int     discnt[MAXCODE];    /* discontinuity indicator */
    int     smode[MAXCODE];     /* signal tracking mode */
    int     nsig;               /* number of signals */
    uint8_t update;             /* general update flag */
    uint8_t update_oc;          /* orbit correction updated */
    uint8_t update_cc;          /* clock correction updated */
    uint8_t update_cb;          /* code bias updated */
    uint8_t update_pb;          /* phase bias updated */
    uint8_t update_ura;         /* URA updated */
} clas_dec_ssr_t;

/*============================================================================
 * L6 Frame Buffer (one per channel)
 *
 * Manages L6 subframe synchronization, bit buffer accumulation, and
 * decode state.  Replaces the static variables in upstream input_cssr().
 *===========================================================================*/

typedef struct {
    uint8_t  buff[1200];        /* bit buffer for CSSR decoder */
    uint8_t  fbuff[BLEN_MSG];   /* subframe byte buffer */
    int      nbyte;             /* bytes received in current subframe */
    int      len;               /* expected subframe length */
    int      nbit;              /* current decode bit position */
    int      havebit;           /* total bits available in buff */
    int      nframe;            /* subframe counter (0-4) */
    int      decode_start;      /* decode started flag */
    int      tow;               /* epoch time-of-week */
    int      tow0;              /* base epoch (floor to hour of mask TOW) */
    uint32_t preamble;          /* preamble accumulator */
    uint8_t  frame;             /* subframe receipt bitmap */
    gtime_t  time;              /* current decode time */
    int      ctype;             /* CSSR message type (4073) */
    int      subtype;           /* CSSR subtype (1-12) */
} clas_l6buf_t;

/*============================================================================
 * CLAS Context Type (NEW — replaces upstream global singletons)
 *
 * Aggregates all CLAS state for thread-safe, multi-channel operation.
 * Must be allocated on heap via clas_ctx_init() and freed via clas_ctx_free().
 *===========================================================================*/

typedef struct {
    /* CSSR decoder state per channel */
    cssr_t              cssr[CLAS_CH_NUM];

    /* Bank management per channel (heap-allocated via clas_ctx_init) */
    clas_bank_ctrl_t   *bank[CLAS_CH_NUM];

    /* Current and backup correction snapshots per channel */
    clas_corr_t         current[CLAS_CH_NUM];
    clas_corr_t         backup[CLAS_CH_NUM];

    /* Grid interpolation state per channel */
    clas_grid_t         grid[CLAS_CH_NUM];
    clas_grid_t         backup_grid[CLAS_CH_NUM];

    /* Grid point coordinates [network][grid_point][lat,lon,hgt] */
    double              grid_pos[CLAS_MAX_NETWORK][CLAS_MAX_GP][3];

    /* Grid point status per channel [channel][network][grid_point] */
    int                 grid_stat[CLAS_CH_NUM][CLAS_MAX_NETWORK][CLAS_MAX_GP];

    /* Ocean loading data per network */
    clas_oload_t        oload[CLAS_MAX_NETWORK];

    /* Per-satellite decoder output (replaces upstream rtcm->nav.ssr[]) */
    clas_dec_ssr_t      dec_ssr[MAXSAT];

    /* L6 frame buffer per channel */
    clas_l6buf_t        l6buf[CLAS_CH_NUM];

    /* Week references per subtype (for week rollover handling) */
    int                 week_ref[CSSR_REF_MAX];
    gtime_t             obs_ref[CSSR_REF_MAX];   /* observation ref time per subtype */
    int                 tow_ref[CSSR_REF_MAX];   /* last TOW per subtype (-1=unset) */

    /* L6 delivery/facility per channel */
    int                 l6delivery[CLAS_CH_NUM];
    int                 l6facility[CLAS_CH_NUM];

    /* nav_t update flag (signals positioning engine to refresh) */
    int                 updateac;

    /* Current channel index for decoding */
    int                 chidx;

    /* Grid definition version */
    int                 gridsel;

    /* Initialization flag */
    int                 initialized;
} clas_ctx_t;

/*============================================================================
 * Context Management Functions
 *===========================================================================*/

/**
 * @brief Initialize CLAS context (allocate bank control on heap).
 * @param[out] ctx  CLAS context to initialize
 * @return 0 on success, -1 on allocation failure
 */
int clas_ctx_init(clas_ctx_t *ctx);

/**
 * @brief Free CLAS context (release heap-allocated bank control).
 * @param[in,out] ctx  CLAS context to free
 */
void clas_ctx_free(clas_ctx_t *ctx);

/*============================================================================
 * CSSR Decoder Functions
 *===========================================================================*/

/**
 * @brief Input CSSR byte stream (one byte at a time, real-time).
 * @param[in,out] ctx   CLAS context
 * @param[in]     data  Input byte
 * @param[in]     ch    Channel index (0 or 1)
 * @return Status (-1:error, 0:no message, 10:SSR message decoded)
 */
int clas_input_cssr(clas_ctx_t *ctx, uint8_t data, int ch);

/**
 * @brief Input CSSR from file (one message at a time, post-processing).
 * @param[in,out] ctx   CLAS context
 * @param[in]     fp    File pointer
 * @param[in]     ch    Channel index
 * @return Status (-2:EOF, -1:error, 0:no message, 10:SSR message decoded)
 */
int clas_input_cssrf(clas_ctx_t *ctx, FILE *fp, int ch);

/*============================================================================
 * Bank Management Functions
 *===========================================================================*/

/**
 * @brief Initialize bank control for a channel.
 * @param[in,out] ctx  CLAS context
 * @param[in]     ch   Channel index
 */
void clas_bank_init(clas_ctx_t *ctx, int ch);

/**
 * @brief Merge closest corrections from all banks into a clas_corr_t snapshot.
 * @param[in]  ctx      CLAS context
 * @param[in]  time     Target time for matching
 * @param[in]  network  Network id
 * @param[in]  ch       Channel index
 * @param[out] corr     Output merged correction snapshot
 * @return 0 on success, -1 if no valid corrections found
 */
int clas_bank_get_close(const clas_ctx_t *ctx, gtime_t time,
                        int network, int ch, clas_corr_t *corr);

/**
 * @brief Apply global corrections (orbit/clock/bias) to nav_t.ssr[].
 * @param[in,out] nav   Navigation data
 * @param[in]     corr  Merged correction snapshot
 * @param[in]     ch    Channel index
 */
void clas_update_global(nav_t *nav, const clas_corr_t *corr, int ch);

/**
 * @brief Apply local corrections (trop/STEC) to nav_t.
 * @param[in,out] nav   Navigation data
 * @param[in]     corr  Merged correction snapshot
 * @param[in]     ch    Channel index
 */
void clas_update_local(nav_t *nav, const clas_corr_t *corr, int ch);

/*============================================================================
 * Grid Definition I/O
 *===========================================================================*/

/**
 * @brief Read CLAS grid definition file.
 * @param[in,out] ctx   CLAS context (stores grid positions)
 * @param[in]     file  Grid definition file path
 * @return 0 on success, -1 on error
 */
int clas_read_grid_def(clas_ctx_t *ctx, const char *file);

/**
 * @brief Check and update grid status flags based on correction availability.
 *
 * Validates that orbit, trop, and STEC corrections exist for each grid point
 * and sets grid_stat[ch][network][gp] accordingly. Must be called after
 * clas_bank_get_close() to enable grid-based positioning.
 *
 * @param[in,out] ctx   CLAS context (updates grid_stat)
 * @param[in]     time  Current observation time
 * @param[in]     ch    L6 channel index (0..CLAS_CH_NUM-1)
 */
void clas_check_grid_status(clas_ctx_t *ctx, const clas_corr_t *corr, int ch);

/**
 * @brief Get L6 facility ID from message ID byte.
 * @param[in] msgid  L6 message ID byte
 * @return Facility ID (0-3)
 */
int clas_get_correct_fac(int msgid);

/*============================================================================
 * Grid Interpolation (mrtk_clas_grid.c)
 *===========================================================================*/

/**
 * @brief Select surrounding grid points and compute interpolation weights.
 * @param[in,out] ctx      CLAS context (grid_pos, grid_stat)
 * @param[in]     pos      Receiver position {lat,lon,hgt} (rad,m)
 * @param[out]    grid     Interpolation result (weights, indices, matrix)
 * @param[in]     gridsel  Grid selection threshold (m), 0=always multi-grid
 * @param[in]     obstime  Observation time (reserved)
 * @return Number of selected grids (0=error, 1/3/4=ok)
 */
int clas_get_grid_index(clas_ctx_t *ctx, const double *pos,
                        clas_grid_t *grid, int gridsel, gtime_t obstime);

/**
 * @brief Interpolate STEC (ionosphere delay) from grid points.
 * @param[in]  corr    Merged correction snapshot
 * @param[in]  index   Selected grid point indices
 * @param[in]  time    Time (GPST)
 * @param[in]  sat     Satellite number
 * @param[in]  n       Number of grids (1-4)
 * @param[in]  weight  Interpolation weights
 * @param[in]  Gmat    Interpolation matrix (4x4, can be NULL)
 * @param[in]  Emat    Basis vector (4x1, can be NULL)
 * @param[out] iono    Interpolated L1 ionosphere delay (m)
 * @param[out] rate    Interpolated ionosphere rate (m/s)
 * @param[out] var     Interpolated variance (m^2)
 * @param[out] brk     Slip break flag
 * @return 1=ok, 0=error
 */
int clas_stec_grid_data(clas_corr_t *corr, const int *index,
                        gtime_t time, int sat, int n,
                        const double *weight, const double *Gmat,
                        const double *Emat, double *iono, double *rate,
                        double *var, int *brk);

/**
 * @brief Interpolate troposphere delay from grid points.
 * @param[in]  corr    Merged correction snapshot
 * @param[in]  index   Selected grid point indices
 * @param[in]  time    Time (GPST)
 * @param[in]  n       Number of grids (1-4)
 * @param[in]  weight  Interpolation weights
 * @param[in]  Gmat    Interpolation matrix (4x4, can be NULL)
 * @param[in]  Emat    Basis vector (4x1, can be NULL)
 * @param[out] zwd     Interpolated zenith wet delay
 * @param[out] ztd     Interpolated zenith total delay
 * @param[out] tbrk    Validity flag (0=all valid, 1=partial)
 * @return 1=ok, 0=error
 */
int clas_trop_grid_data(clas_corr_t *corr, const int *index,
                        gtime_t time, int n, const double *weight,
                        const double *Gmat, const double *Emat,
                        double *zwd, double *ztd, int *tbrk);

/**
 * @brief Initialize ocean loading data in context.
 * @param[in,out] ctx  CLAS context
 */
void clas_init_oload(clas_ctx_t *ctx);

/**
 * @brief Read BLQ ocean tide loading parameters for CLAS grid points.
 * @param[in]     file  BLQ grid file
 * @param[in,out] ctx   CLAS context (oload[] populated)
 * @return 1:ok, 0:file open error
 */
int readblqgrid(const char *file, clas_ctx_t *ctx);

/*============================================================================
 * OSR (SSR→OSR) Conversion Context (mrtk_clas_osr.c)
 *
 * Replaces static variables from upstream cssr2osr.c with a context struct
 * for thread safety and multi-instance support.
 *===========================================================================*/

/** Per-satellite SSR correction state for SIS/IODE adjustment */
typedef struct {
    double currtow;       /* orbit TOW of last computed SIS discontinuity (s) */
    double currsis;       /* current SIS discontinuity (m) */
    double prevtow;       /* clock TOW when prevsis was saved (s) */
    double prevsis;       /* SIS value saved at 25s boundary */
    int    previode;      /* IODE when prevsis was saved */
    double prev_orb_tow;  /* orbit epoch of previous call (for orbit-update detection) */
    double tow;           /* last computed TOW */
    double orb;           /* orbit correction along line-of-sight (m) */
    double clk;           /* clock correction (m) */
} clas_satcorr_t;

/** SSR→OSR conversion persistent state */
typedef struct {
    /* zdres() persistent state (replaces statics in cssr2osr.c:zdres) */
    double  pbias_ofst[MAXSAT * (NFREQ + NEXOBS)]; /* phase bias offset history */
    double  cpctmp[MAXSAT * (NFREQ + NEXOBS)];     /* previous epoch CPC */
    gtime_t pt0tmp[MAXSAT * (NFREQ + NEXOBS)];     /* previous epoch pbias time */

    /* compensatedisp() persistent state */
    gtime_t comp_t0[CLAS_CH_NUM][MAXSAT];          /* previous update time */
    gtime_t comp_tm[CLAS_CH_NUM][MAXSAT];           /* prev-previous update time */
    double  comp_b0[CLAS_CH_NUM][MAXSAT * (NFREQ + NEXOBS)]; /* previous pbias */
    double  comp_bm[CLAS_CH_NUM][MAXSAT * (NFREQ + NEXOBS)]; /* prev-prev pbias */
    double  comp_iono0[CLAS_CH_NUM][MAXSAT];        /* previous iono */
    double  comp_ionom[CLAS_CH_NUM][MAXSAT];        /* prev-prev iono */
    double  comp_coef[CLAS_CH_NUM][MAXSAT * (NFREQ + NEXOBS)]; /* linear coeff */
    int     comp_slip[CLAS_CH_NUM][MAXSAT * NFREQ]; /* slip detection state */

    /* SIS/IODE adjustment state (replaces static satcorr[] in upstream) */
    clas_satcorr_t satcorr[CLAS_CH_NUM][MAXSAT];
    double  saved_rr[3];   /* saved receiver position for SIS computation */

    int     initialized;
} clas_osr_ctx_t;

/*============================================================================
 * OSR Conversion Functions (mrtk_clas_osr.c)
 *===========================================================================*/

/** Invalid CSSR bias value sentinel */
#define CLAS_CSSRINVALID  (-10000)

/** Maximum age for SSR bias corrections (seconds) */
#define CLAS_MAXAGESSR_BIAS  120.0

/** Maximum phase bias correction for SSR (meters) */
#define CLAS_MAXPBCORSSR   20.0

/**
 * @brief Precise troposphere model for CLAS (MOPS standard + mapping function).
 * @param[in] time  Observation time
 * @param[in] pos   Receiver position {lat,lon,hgt} (rad,m)
 * @param[in] azel  Azimuth/elevation {az,el} (rad)
 * @param[in] zwd   Zenith wet delay from grid (m)
 * @param[in] ztd   Zenith total delay from grid (m)
 * @return Slant tropospheric delay (m), 0 on error
 */
double clas_osr_prectrop(gtime_t time, const double *pos, const double *azel,
                         double zwd, double ztd);

/**
 * @brief Select frequency pair for multi-frequency processing.
 * @param[in] sat  Satellite number
 * @param[in] opt  Processing options
 * @param[in] obs  Observation data
 * @return Frequency pair bitmask (0=L1 only, 1=L2, 2=L5, 3=L2+L5)
 */
int clas_osr_selfreqpair(int sat, const prcopt_t *opt, const obsd_t *obs);

/**
 * @brief Compute zero-differenced phase/code residuals with CLAS corrections.
 * @param[in]     obs    Observation data for epoch
 * @param[in]     n      Number of observations
 * @param[in]     rs     Satellite positions+velocities (6*n)
 * @param[in]     dts    Satellite clock corrections (2*n)
 * @param[in]     vare   Satellite position variances (n)
 * @param[in]     svh    Satellite health flags (n)
 * @param[in,out] nav    Navigation data
 * @param[in,out] x      State vector (position extracted from [0:2])
 * @param[out]    y      Residuals (n*nf*2)
 * @param[out]    e      Line-of-sight vectors (3*n)
 * @param[out]    azel   Azimuth/elevation (2*n)
 * @param[in]     rtk    RTK control struct
 * @param[in]     osrlog Enable OSR logging
 * @param[in,out] osr_ctx OSR context for persistent state
 * @param[in]     grid   Grid interpolation state
 * @param[in]     corr   Merged correction snapshot
 * @param[in,out] ssat   Satellite status array
 * @param[in]     opt    Processing options
 * @param[in,out] sol    Solution struct
 * @param[out]    osr    OSR output per satellite
 * @param[in]     ch     SSR channel (0 or 1)
 * @return Number of valid measurements
 */
/**
 * @brief Update per-satellite SIS correction state for IODE adjustment.
 * @param[in]     time  Observation time
 * @param[in]     teph  Time for ephemeris selection
 * @param[in]     sat   Satellite number
 * @param[in]     opt   Processing options
 * @param[in]     nav   Navigation data
 * @param[in]     ch    SSR channel index
 * @param[in,out] ctx   OSR context (satcorr state updated)
 * @return 1:ok, 0:error
 */
int clas_osr_satcorr_update(gtime_t time, gtime_t teph, int sat,
                             const prcopt_t *opt, const nav_t *nav, int ch,
                             clas_osr_ctx_t *ctx);

int clas_osr_zdres(const obsd_t *obs, int n, const double *rs, const double *dts,
                   const double *vare, const int *svh, nav_t *nav,
                   double *x, double *y, double *e, double *azel,
                   rtk_t *rtk, int osrlog,
                   clas_osr_ctx_t *osr_ctx, clas_grid_t *grid,
                   clas_corr_t *corr, ssat_t *ssat,
                   prcopt_t *opt, sol_t *sol, clas_osrd_t *osr, int ch);

/**
 * @brief Compensate time variation of signal bias and ionosphere delay.
 * @param[in]     corr    Merged correction snapshot
 * @param[in]     index   Grid point indices
 * @param[in]     obs     Observation for this satellite
 * @param[in]     sat     Satellite number
 * @param[in]     iono    Ionosphere delay (m)
 * @param[in]     pb      Phase biases per frequency
 * @param[out]    compL   Compensation output per frequency
 * @param[in,out] pbreset Phase bias reset flags
 * @param[in]     opt     Processing options
 * @param[in]     ssat    Satellite status
 * @param[in]     ch      SSR channel
 * @param[in,out] osr_ctx OSR context for persistent state
 * @param[in]     nav     Navigation data (for wavelengths)
 */
void clas_osr_compensatedisp(clas_corr_t *corr, const int *index,
                             const obsd_t *obs, int sat,
                             double iono, const double *pb,
                             double *compL, int *pbreset,
                             const prcopt_t *opt, ssat_t ssat, int ch,
                             clas_osr_ctx_t *osr_ctx, const nav_t *nav);

/**
 * @brief Assemble per-satellite ionosphere/bias/antenna corrections.
 * @param[in]     obs     Observation for this satellite
 * @param[in,out] nav     Navigation data
 * @param[in]     pos     Receiver position {lat,lon,hgt} (rad,m)
 * @param[in]     azel    Azimuth/elevation for this satellite
 * @param[in]     opt     Processing options
 * @param[in]     grid    Grid interpolation state
 * @param[in]     corr    Merged correction snapshot
 * @param[in]     ssat    Satellite status
 * @param[out]    brk     Slip break flag
 * @param[out]    osr     OSR output for this satellite
 * @param[in,out] pbreset Phase bias reset flags
 * @param[in]     ch      SSR channel
 * @param[in,out] osr_ctx OSR context
 * @return 1=ok, 0=error (stale bias, missing iono)
 */
int clas_osr_corrmeas(const obsd_t *obs, nav_t *nav, const double *pos,
                      const double *azel, const prcopt_t *opt,
                      clas_grid_t *grid, clas_corr_t *corr,
                      ssat_t ssat, int *brk, clas_osrd_t *osr,
                      int *pbreset, int ch, clas_osr_ctx_t *osr_ctx);

/**
 * @brief Initialize OSR conversion context.
 * @param[out] ctx  OSR context to initialize
 */
void clas_osr_ctx_init(clas_osr_ctx_t *ctx);

/**
 * @brief Convert compact SSR corrections to observation-space representations.
 *
 * Top-level wrapper that sets up grid/CSSR corrections and calls clas_osr_zdres().
 * Verbatim port of upstream claslib ssr2osr() from cssr2osr.c.
 *
 * @param[in,out] rtk   RTK control structure
 * @param[in,out] obs   Observation data array
 * @param[in]     n     Number of observations
 * @param[in,out] nav   Navigation data
 * @param[out]    osr   OSR output per satellite
 * @param[in]     mode  0=normal (upstream compat)
 * @param[in]     clas  CLAS decoder context
 * @return Number of valid measurements, 0 on failure
 */
int clas_ssr2osr(rtk_t *rtk, obsd_t *obs, int n, nav_t *nav,
                 clas_osrd_t *osr, int mode, clas_ctx_t *clas);

/*============================================================================
 * CSSR Helper Functions
 *===========================================================================*/

/**
 * @brief Convert CSSR GNSS ID to RTKLIB system bitmask.
 * @param[in]  gnss  CSSR GNSS ID (CSSR_SYS_GPS, etc.)
 * @param[out] prn0  Output base PRN offset (can be NULL)
 * @return RTKLIB system bitmask (SYS_GPS, etc.), 0 on error
 */
int cssr_gnss2sys(int gnss, int *prn0);

/**
 * @brief Convert RTKLIB system bitmask to CSSR GNSS ID.
 * @param[in]  sys   RTKLIB system (SYS_GPS, etc.)
 * @param[out] prn0  Output base PRN offset (can be NULL)
 * @return CSSR GNSS ID, CSSR_SYS_NONE on error
 */
int cssr_sys2gnss(int sys, int *prn0);

/*============================================================================
 * ISB Functions (types defined in mrtk_nav.h)
 *===========================================================================*/

/**
 * @brief Read ISB correction table from file.
 * @param[in]     file  ISB table file path (wild-card * expanded)
 * @param[in,out] nav   Navigation data (isb array populated)
 * @return Status (1:ok, 0:error)
 */
int readisb(const char *file, nav_t *nav);

/**
 * @brief Read L2C 1/4 cycle phase shift table from file.
 * @param[in]     file  L2C phase shift table file path
 * @param[in,out] nav   Navigation data (sfts populated)
 * @return Status (0:ok, -1:error)
 */
int readL2C(const char *file, nav_t *nav);

/**
 * @brief Set ISB corrections for rover and reference stations.
 * @param[in]  nav       Navigation data (contains isb table)
 * @param[in]  rectype0  Rover receiver type
 * @param[in]  rectype1  Reference receiver type
 * @param[out] sta0      Rover station (isb field populated)
 * @param[out] sta1      Reference station (isb field populated, can be NULL)
 */
void setisb(const nav_t *nav, const char *rectype0, const char *rectype1,
            sta_t *sta0, sta_t *sta1);

/**
 * @brief Get ISB correction values for a satellite system.
 * @param[in]  sysno  Satellite system code (SYS_GPS, etc.)
 * @param[in]  opt    Processing options
 * @param[in]  sta    Station data with ISB values
 * @param[out] y      ISB corrections [NFREQ][2] (0:phase, 1:code) (m)
 */
void chk_isb(int sysno, const prcopt_t *opt, const sta_t *sta,
             double y[NFREQ][2]);

/**
 * @brief Check if observation code is L2C.
 * @param[in] code  Observation code (CODE_???)
 * @return 1 if L2C, 0 otherwise
 */
int isL2C(int code);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_CLAS_H */
