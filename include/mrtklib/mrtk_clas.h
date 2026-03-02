/*------------------------------------------------------------------------------
 * mrtk_clas.h : QZSS CLAS (L6D) CSSR decoder and grid correction types
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
 *----------------------------------------------------------------------------*/
#ifndef MRTK_CLAS_H
#define MRTK_CLAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"

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
#define CLAS_MAX_NETWORK     6          /* max networks in bank storage */
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

    /* Current channel index for decoding */
    int                 chidx;

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
 * @brief Decode CSSR compact SSR message (subtypes 1-12).
 * @param[in,out] ctx   CLAS context
 * @param[in,out] nav   Navigation data (receives decoded corrections)
 * @param[in]     buff  Message buffer
 * @param[in]     len   Message length (bits)
 * @param[in]     ch    Channel index (0 or 1)
 * @return Decoded subtype number (1-12), 0 on no message, -1 on error
 */
int clas_decode_cssr(clas_ctx_t *ctx, nav_t *nav,
                     const uint8_t *buff, int len, int ch);

/**
 * @brief Input CSSR byte stream (one byte at a time, real-time).
 * @param[in,out] ctx   CLAS context
 * @param[in,out] nav   Navigation data
 * @param[in]     data  Input byte
 * @param[in]     ch    Channel index
 * @return Status (-1:error, 0:no message, 10:SSR message decoded)
 */
int clas_input_cssr(clas_ctx_t *ctx, nav_t *nav, uint8_t data, int ch);

/**
 * @brief Input CSSR from file (one message at a time, post-processing).
 * @param[in,out] ctx   CLAS context
 * @param[in,out] nav   Navigation data
 * @param[in]     fp    File pointer
 * @param[in]     ch    Channel index
 * @return Status (-2:EOF, -1:error, 0:no message, 10:SSR message decoded)
 */
int clas_input_cssrf(clas_ctx_t *ctx, nav_t *nav, FILE *fp, int ch);

/*============================================================================
 * Bank Management Functions
 *===========================================================================*/

/**
 * @brief Initialize bank control for a channel.
 * @param[out] ctrl     Bank control to initialize
 * @param[in]  facility L6 facility id
 */
void clas_bank_init(clas_bank_ctrl_t *ctrl, int facility);

/**
 * @brief Store orbit correction into orbit bank.
 * @param[in,out] ctrl  Bank control
 * @param[in]     cssr  Decoded CSSR state
 */
void clas_bank_set_orbit(clas_bank_ctrl_t *ctrl, const cssr_t *cssr);

/**
 * @brief Store clock correction into clock bank.
 * @param[in,out] ctrl  Bank control
 * @param[in]     cssr  Decoded CSSR state
 */
void clas_bank_set_clock(clas_bank_ctrl_t *ctrl, const cssr_t *cssr);

/**
 * @brief Store code bias into bias bank.
 * @param[in,out] ctrl  Bank control
 * @param[in]     cssr  Decoded CSSR state
 */
void clas_bank_set_cbias(clas_bank_ctrl_t *ctrl, const cssr_t *cssr);

/**
 * @brief Store phase bias into bias bank.
 * @param[in,out] ctrl  Bank control
 * @param[in]     cssr  Decoded CSSR state
 */
void clas_bank_set_pbias(clas_bank_ctrl_t *ctrl, const cssr_t *cssr);

/**
 * @brief Store troposphere/STEC into trop bank.
 * @param[in,out] ctrl  Bank control
 * @param[in]     cssr  Decoded CSSR state
 */
void clas_bank_set_trop(clas_bank_ctrl_t *ctrl, const cssr_t *cssr);

/**
 * @brief Merge closest corrections from all banks into a clas_corr_t snapshot.
 * @param[in]  ctrl     Bank control
 * @param[in]  time     Target time for matching
 * @param[in]  network  Network id
 * @param[out] corr     Output merged correction snapshot
 * @return 0 on success, -1 if no valid corrections found
 */
int clas_bank_get_close(const clas_bank_ctrl_t *ctrl, gtime_t time,
                        int network, clas_corr_t *corr);

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
 * Grid Interpolation Functions
 *===========================================================================*/

/**
 * @brief Find surrounding grid points for rover position.
 * @param[in]  pos      Rover position {lat,lon,hgt} (rad,m)
 * @param[in]  gridpos  Grid point coordinates [network][gp][3]
 * @param[in]  ngp      Number of grid points per network
 * @param[in]  nnet     Number of networks
 * @param[out] grid     Grid interpolation result
 * @return 0 on success, -1 if no valid grid found
 */
int clas_get_grid_index(const double *pos,
                        const double gridpos[][CLAS_MAX_GP][3],
                        const int *ngp, int nnet, clas_grid_t *grid);

/**
 * @brief Interpolate troposphere correction at rover position.
 * @param[in]  corr     Merged correction snapshot
 * @param[in]  grid     Grid interpolation result
 * @param[out] ztd      Interpolated zenith total delay (m)
 * @param[out] zwd      Interpolated zenith wet delay (m)
 * @return 0 on success, -1 on error
 */
int clas_trop_grid_data(const clas_corr_t *corr, const clas_grid_t *grid,
                        double *ztd, double *zwd);

/**
 * @brief Interpolate STEC correction for a satellite at rover position.
 * @param[in]  corr     Merged correction snapshot
 * @param[in]  grid     Grid interpolation result
 * @param[in]  sat      Satellite number
 * @param[out] stec     Interpolated STEC value (m)
 * @return 0 on success, -1 on error
 */
int clas_stec_grid_data(const clas_corr_t *corr, const clas_grid_t *grid,
                        int sat, double *stec);

/*============================================================================
 * Grid Definition I/O
 *===========================================================================*/

/**
 * @brief Read CLAS grid definition file.
 * @param[in]  file     Grid definition file path
 * @param[out] gridpos  Grid point coordinates [network][gp][3]
 * @param[out] ngp      Number of grid points per network
 * @param[out] nnet     Number of networks
 * @return 0 on success, -1 on error
 */
int clas_read_grid_def(const char *file,
                       double gridpos[][CLAS_MAX_GP][3],
                       int *ngp, int *nnet);

/**
 * @brief Get ocean loading parameters for a network.
 * @param[in] ctx      CLAS context
 * @param[in] network  Network id
 * @return Pointer to ocean loading data, NULL if not available
 */
const clas_oload_t *clas_get_oload(const clas_ctx_t *ctx, int network);

/*============================================================================
 * CSSR Helper Functions
 *===========================================================================*/

/**
 * @brief Convert CSSR system ID to RTKLIB system ID.
 * @param[in] cssr_sys  CSSR system (CSSR_SYS_GPS, etc.)
 * @return RTKLIB system (SYS_GPS, etc.), 0 on error
 */
int cssr_sys2sys(int cssr_sys);

/**
 * @brief Convert RTKLIB system ID to CSSR system ID.
 * @param[in] sys  RTKLIB system (SYS_GPS, etc.)
 * @return CSSR system (CSSR_SYS_GPS, etc.), CSSR_SYS_NONE on error
 */
int cssr_gnss2sys(int sys);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_CLAS_H */
