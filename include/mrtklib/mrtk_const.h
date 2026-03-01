/*------------------------------------------------------------------------------
 * mrtk_const.h : physical, navigation, and system constants
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
 * @file mrtk_const.h
 * @brief MRTKLIB Constants — Physical, navigation, and system constants.
 *
 * This header provides fundamental physical constants (speed of light, earth
 * parameters), GNSS frequency constants, navigation system identifiers, and
 * miscellaneous constants used across the library and applications.
 *
 * All constants use #ifndef guards to avoid conflicts with local definitions
 * in legacy .c files that duplicate these values.
 *
 * @note This header also contains legacy type definitions and function
 *       declarations for modules not yet implemented (TLE, GIS, download).
 *       These are preserved for API compatibility.
 */
#ifndef MRTK_CONST_H
#define MRTK_CONST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_version.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_peph.h"
#include "mrtklib/mrtk_rinex.h"

/*============================================================================
 * Copyright Information
 *===========================================================================*/

#define COPYRIGHT_MRTKLIB \
            "Copyright (C) 2007-2023 T.Takasu\nAll rights reserved." \
            "Copyright (C) 2023-2025, TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION. All Rights Reserved." \
            "Copyright (C) 2023-2025, Japan Aerospace Exploration Agency. All Rights Reserved."

/*============================================================================
 * Physical Constants
 *===========================================================================*/

#ifndef PI
#define PI          3.1415926535897932  /* pi */
#endif
#ifndef D2R
#define D2R         (PI/180.0)          /* deg to rad */
#endif
#ifndef R2D
#define R2D         (180.0/PI)          /* rad to deg */
#endif
#ifndef CLIGHT
#define CLIGHT      299792458.0         /* speed of light (m/s) */
#endif
#ifndef SC2RAD
#define SC2RAD      3.1415926535898     /* semi-circle to radian (IS-GPS) */
#endif
#ifndef AU
#define AU          149597870691.0      /* 1 AU (m) */
#endif
#ifndef AS2R
#define AS2R        (D2R/3600.0)        /* arc sec to radian */
#endif
#ifndef M2NS
#define M2NS        (1E9/CLIGHT)        /* m to ns */
#endif
#ifndef NS2M
#define NS2M        (CLIGHT/1E9)        /* ns to m */
#endif
#ifndef OMGE
#define OMGE        7.2921151467E-5     /* earth angular velocity (IS-GPS) (rad/s) */
#endif
#ifndef RE_WGS84
#define RE_WGS84    6378137.0           /* earth semimajor axis (WGS84) (m) */
#endif
#ifndef FE_WGS84
#define FE_WGS84    (1.0/298.257223563) /* earth flattening (WGS84) */
#endif
#ifndef HION
#define HION        350000.0            /* ionosphere height (m) */
#endif

/*============================================================================
 * Frequency Constants
 *===========================================================================*/

#ifndef MAXFREQ
#define MAXFREQ     7                   /* max NFREQ */
#endif
#ifndef FREQ1
#define FREQ1       1.57542E9           /* L1/E1/B1C  frequency (Hz) */
#endif
#ifndef FREQ2
#define FREQ2       1.22760E9           /* L2         frequency (Hz) */
#endif
#ifndef FREQ5
#define FREQ5       1.17645E9           /* L5/E5a/B2a frequency (Hz) */
#endif
#ifndef FREQ6
#define FREQ6       1.27875E9           /* E6/L6  frequency (Hz) */
#endif
#ifndef FREQ7
#define FREQ7       1.20714E9           /* E5b    frequency (Hz) */
#endif
#ifndef FREQ8
#define FREQ8       1.191795E9          /* E5a+b  frequency (Hz) */
#endif
#ifndef FREQ9
#define FREQ9       2.492028E9          /* S      frequency (Hz) */
#endif
#ifndef FREQ1_GLO
#define FREQ1_GLO   1.60200E9           /* GLONASS G1 base frequency (Hz) */
#endif
#ifndef DFRQ1_GLO
#define DFRQ1_GLO   0.56250E6           /* GLONASS G1 bias frequency (Hz/n) */
#endif
#ifndef FREQ2_GLO
#define FREQ2_GLO   1.24600E9           /* GLONASS G2 base frequency (Hz) */
#endif
#ifndef DFRQ2_GLO
#define DFRQ2_GLO   0.43750E6           /* GLONASS G2 bias frequency (Hz/n) */
#endif
#ifndef FREQ3_GLO
#define FREQ3_GLO   1.202025E9          /* GLONASS G3 frequency (Hz) */
#endif
#ifndef FREQ1a_GLO
#define FREQ1a_GLO  1.600995E9          /* GLONASS G1a frequency (Hz) */
#endif
#ifndef FREQ2a_GLO
#define FREQ2a_GLO  1.248060E9          /* GLONASS G2a frequency (Hz) */
#endif
#ifndef FREQ1_CMP
#define FREQ1_CMP   1.561098E9          /* BDS B1I     frequency (Hz) */
#endif
#ifndef FREQ2_CMP
#define FREQ2_CMP   1.20714E9           /* BDS B2I/B2b frequency (Hz) */
#endif
#ifndef FREQ3_CMP
#define FREQ3_CMP   1.26852E9           /* BDS B3      frequency (Hz) */
#endif

/*============================================================================
 * Error Factors
 *===========================================================================*/

#ifndef EFACT_GPS
#define EFACT_GPS   1.0                 /* error factor: GPS */
#endif
#ifndef EFACT_GLO
#define EFACT_GLO   1.5                 /* error factor: GLONASS */
#endif
#ifndef EFACT_GAL
#define EFACT_GAL   1.0                 /* error factor: Galileo */
#endif
#ifndef EFACT_QZS
#define EFACT_QZS   1.0                 /* error factor: QZSS */
#endif
#ifndef EFACT_CMP
#define EFACT_CMP   1.0                 /* error factor: BeiDou */
#endif
#ifndef EFACT_IRN
#define EFACT_IRN   1.5                 /* error factor: NavIC */
#endif
#ifndef EFACT_SBS
#define EFACT_SBS   3.0                 /* error factor: SBAS */
#endif

/*============================================================================
 * Navigation System Identifiers
 *===========================================================================*/

#ifndef SYS_NONE
#define SYS_NONE    0x00                /* navigation system: none */
#endif
#ifndef SYS_GPS
#define SYS_GPS     0x01                /* navigation system: GPS */
#endif
#ifndef SYS_SBS
#define SYS_SBS     0x02                /* navigation system: SBAS */
#endif
#ifndef SYS_GLO
#define SYS_GLO     0x04                /* navigation system: GLONASS */
#endif
#ifndef SYS_GAL
#define SYS_GAL     0x08                /* navigation system: Galileo */
#endif
#ifndef SYS_QZS
#define SYS_QZS     0x10                /* navigation system: QZSS */
#endif
#ifndef SYS_CMP
#define SYS_CMP     0x20                /* navigation system: BeiDou */
#endif
#ifndef SYS_IRN
#define SYS_IRN     0x40                /* navigation system: NavIC */
#endif
#ifndef SYS_LEO
#define SYS_LEO     0x80                /* navigation system: LEO */
#endif
#ifndef SYS_BD2
#define SYS_BD2     0x100               /* navigation system: BeiDou-2 */
#endif
#ifndef SYS_ALL
#define SYS_ALL     0xFFF               /* navigation system: all */
#endif

/*============================================================================
 * Time System Constants
 *===========================================================================*/

#ifndef TSYS_GPS
#define TSYS_GPS    0                   /* time system: GPS time */
#endif
#ifndef TSYS_UTC
#define TSYS_UTC    1                   /* time system: UTC */
#endif
#ifndef TSYS_GLO
#define TSYS_GLO    2                   /* time system: GLONASS time */
#endif
#ifndef TSYS_GAL
#define TSYS_GAL    3                   /* time system: Galileo time */
#endif
#ifndef TSYS_QZS
#define TSYS_QZS    4                   /* time system: QZSS time */
#endif
#ifndef TSYS_CMP
#define TSYS_CMP    5                   /* time system: BeiDou time */
#endif
#ifndef TSYS_IRN
#define TSYS_IRN    6                   /* time system: IRNSS time */
#endif

/*============================================================================
 * Ephemeris / Navigation Constants
 *===========================================================================*/

#ifndef MAXDTOE
#define MAXDTOE     7200.0              /* max time difference to GPS Toe (s) */
#endif
#ifndef MAXDTOE_QZS
#define MAXDTOE_QZS 7200.0              /* max time difference to QZSS Toe (s) */
#endif
#ifndef MAXDTOE_GAL
#define MAXDTOE_GAL 14400.0             /* max time difference to Galileo Toe (s) */
#endif
#ifndef MAXDTOE_CMP
#define MAXDTOE_CMP 21600.0             /* max time difference to BeiDou Toe (s) */
#endif
#ifndef MAXDTOE_GLO
#define MAXDTOE_GLO 1800.0              /* max time difference to GLONASS Toe (s) */
#endif
#ifndef MAXDTOE_IRN
#define MAXDTOE_IRN 7200.0              /* max time difference to IRNSS Toe (s) */
#endif
#ifndef MAXDTOE_SBS
#define MAXDTOE_SBS 360.0               /* max time difference to SBAS Toe (s) */
#endif
#ifndef MAXDTOE_S
#define MAXDTOE_S   86400.0             /* max time difference to ephem toe (s) for other */
#endif
#ifndef MAXGDOP
#define MAXGDOP     300.0               /* max GDOP */
#endif

/*============================================================================
 * Swap / Timing Constants
 *===========================================================================*/

#define INT_SWAP_TRAC 86400.0           /* swap interval of trace file (s) */
#define INT_SWAP_STAT 86400.0           /* swap interval of solution status file (s) */

/*============================================================================
 * SBAS Constants
 *===========================================================================*/

#define MAXSBSAGEF  30.0                /* max age of SBAS fast correction (s) */
#define MAXSBSAGEL  1800.0              /* max age of SBAS long term corr (s) */
#define MAXSBSURA   8                   /* max URA of SBAS satellite */

/*============================================================================
 * RINEX Constants
 *===========================================================================*/

#define RNX2VER     2.10                /* RINEX ver.2 default output version */
#define RNX3VER     3.00                /* RINEX ver.3 default output version */

/*============================================================================
 * Observation / Frequency Type Flags
 *===========================================================================*/

#define OBSTYPE_PR  0x01                /* observation type: pseudorange */
#define OBSTYPE_CP  0x02                /* observation type: carrier-phase */
#define OBSTYPE_DOP 0x04                /* observation type: doppler-freq */
#define OBSTYPE_SNR 0x08                /* observation type: SNR */
#define OBSTYPE_ALL 0xFF                /* observation type: all */

#define FREQTYPE_L1 0x01                /* frequency type: L1/E1/B1 */
#define FREQTYPE_L2 0x02                /* frequency type: L2/E5b/B2 */
#define FREQTYPE_L3 0x04                /* frequency type: L5/E5a/L3 */
#define FREQTYPE_L4 0x08                /* frequency type: L6/E6/B3 */
#define FREQTYPE_L5 0x10                /* frequency type: E5ab */
#define FREQTYPE_ALL 0xFF               /* frequency type: all */

/*============================================================================
 * Misc Constants
 *===========================================================================*/

#ifndef COMMENTH
#define COMMENTH    "%"                 /* comment line indicator for solution */
#endif

#ifndef LLI_SLIP
#define LLI_SLIP    0x01                /* LLI: cycle-slip */
#endif
#ifndef LLI_HALFC
#define LLI_HALFC   0x02                /* LLI: half-cycle not resovled */
#endif
#ifndef LLI_BOCTRK
#define LLI_BOCTRK  0x04                /* LLI: boc tracking of mboc signal */
#endif
#ifndef LLI_HALFA
#define LLI_HALFA   0x40                /* LLI: half-cycle added */
#endif
#ifndef LLI_HALFS
#define LLI_HALFS   0x80                /* LLI: half-cycle subtracted */
#endif

#ifndef P2_5
#define P2_5        0.03125             /* 2^-5 */
#endif
#ifndef P2_6
#define P2_6        0.015625            /* 2^-6 */
#endif
#ifndef P2_11
#define P2_11       4.882812500000000E-04 /* 2^-11 */
#endif
#ifndef P2_15
#define P2_15       3.051757812500000E-05 /* 2^-15 */
#endif
#ifndef P2_17
#define P2_17       7.629394531250000E-06 /* 2^-17 */
#endif
#ifndef P2_19
#define P2_19       1.907348632812500E-06 /* 2^-19 */
#endif
#ifndef P2_20
#define P2_20       9.536743164062500E-07 /* 2^-20 */
#endif
#ifndef P2_21
#define P2_21       4.768371582031250E-07 /* 2^-21 */
#endif
#ifndef P2_23
#define P2_23       1.192092895507810E-07 /* 2^-23 */
#endif
#ifndef P2_24
#define P2_24       5.960464477539063E-08 /* 2^-24 */
#endif
#ifndef P2_27
#define P2_27       7.450580596923828E-09 /* 2^-27 */
#endif
#ifndef P2_29
#define P2_29       1.862645149230957E-09 /* 2^-29 */
#endif
#ifndef P2_30
#define P2_30       9.313225746154785E-10 /* 2^-30 */
#endif
#ifndef P2_31
#define P2_31       4.656612873077393E-10 /* 2^-31 */
#endif
#ifndef P2_32
#define P2_32       2.328306436538696E-10 /* 2^-32 */
#endif
#ifndef P2_33
#define P2_33       1.164153218269348E-10 /* 2^-33 */
#endif
#ifndef P2_35
#define P2_35       2.910383045673370E-11 /* 2^-35 */
#endif
#ifndef P2_38
#define P2_38       3.637978807091710E-12 /* 2^-38 */
#endif
#ifndef P2_39
#define P2_39       1.818989403545856E-12 /* 2^-39 */
#endif
#ifndef P2_40
#define P2_40       9.094947017729280E-13 /* 2^-40 */
#endif
#ifndef P2_43
#define P2_43       1.136868377216160E-13 /* 2^-43 */
#endif
#ifndef P2_48
#define P2_48       3.552713678800501E-15 /* 2^-48 */
#endif
#ifndef P2_50
#define P2_50       8.881784197001252E-16 /* 2^-50 */
#endif
#ifndef P2_55
#define P2_55       2.775557561562891E-17 /* 2^-55 */
#endif

/*============================================================================
 * SSR Constants
 *===========================================================================*/

#define SSR_VENDOR_L6   0               /* vendor type L6(MADOCA-PPP) */
#define SSR_VENDOR_RTCM 1               /* vendor type RTCM3(JAXA-MADOCA) */
#define MAXAGESSRL6     60.0            /* max age of ssr satellite code and phase bias (s) */
#define MIONO_MAX_AGE   300.0           /* max age of STEC correction (s) (Table 6.3.2-3) */
#define SSR_INVALID_CBIAS  -81.92       /* invalid value of ssr code bias (m) */
#define SSR_INVALID_PBIAS  -52.4288     /* invalid value of ssr phase bias (m) */

/*============================================================================
 * Satellite Attitude / Type Constants
 *===========================================================================*/

#define SATT_NORMAL     0               /* nominal attitude */
#define GM_EGM          3.986004415E14  /* earth grav. constant (EGM96/2008) */
#define DT_BETA_R       10.0            /* delta time for beta angle rate (s) */

#define STYPE_NONE      0               /* satellite type: unknown */
#define STYPE_GPSII     1               /* satellite type: GPS Block II */
#define STYPE_GPSIIA    2               /* satellite type: GPS Block IIA */
#define STYPE_GPSIIR    3               /* satellite type: GPS Block IIR */
#define STYPE_GPSIIRM   4               /* satellite type: GPS Block IIR-M */
#define STYPE_GPSIIF    5               /* satellite type: GPS Block IIF */
#define STYPE_GPSIIIA   6               /* satellite type: GPS Block III-A */
#define STYPE_GPSIIIB   7               /* satellite type: GPS Block III-B */
#define STYPE_GLO       8               /* satellite type: GLONASS */
#define STYPE_GLOM      9               /* satellite type: GLONASS-M */
#define STYPE_GLOK      10              /* satellite type: GLONASS-K */
#define STYPE_GAL       11              /* satellite type: Galileo IOV */
#define STYPE_QZS       12              /* satellite type: QZSS Block IQ */
#define STYPE_QZSIIQ    13              /* satellite type: QZSS Block IIQ */
#define STYPE_QZSIIG    14              /* satellite type: QZSS Block IIG */
#define STYPE_QZSIIIQ   15              /* satellite type: QZSS Block IIIQ */
#define STYPE_QZSIIIG   16              /* satellite type: QZSS Block IIIG */
#define STYPE_GAL_FOC   17              /* satellite type: Galileo FOC */
#define STYPE_BDS_G     18              /* satellite type: BeiDou GEO */
#define STYPE_BDS_I     19              /* satellite type: BeiDou IGSO */
#define STYPE_BDS_M     20              /* satellite type: BeiDou MEO */
#define STYPE_BDS_I_YS  21              /* satellite type: BeiDou IGSO(no ON mode) */
#define STYPE_BDS_M_YS  22              /* satellite type: BeiDou MEO(no ON mode) */
#define STYPE_BDS3_G    23              /* satellite type: BeiDou-3 GEO */
#define STYPE_BDS3_I_C  24              /* satellite type: BeiDou-3 IGSO by CAST */
#define STYPE_BDS3_I_S  25              /* satellite type: BeiDou-3 IGSO by SECM */
#define STYPE_BDS3_M_C  26              /* satellite type: BeiDou-3 MEO by CAST */
#define STYPE_BDS3_M_S  27              /* satellite type: BeiDou-3 MEO by SECM */
#define STYPE_QZS_I_YS  30              /* satellite type: QZSS Block IQ(beta 15/17.5 deg) */
#define STYPE_LEO       31              /* satellite type: LEO */
#define STYPE_MAX       32              /* max satellite type */

/*============================================================================
 * Orbit Event Constants
 *===========================================================================*/

#define MAXNP_EVENT     6               /* max number of event parameters */

#define EVENT_NONE      0               /* orbit event: no event */
#define EVENT_ATT_EC    1               /* orbit event: ATT_EC */
#define EVENT_ATT_YS    2               /* orbit event: ATT_YS */
#define EVENT_ATT_EC_YS 3               /* orbit event: ATT_EC_YS */
#define EVENT_ATT_YS_EC 4               /* orbit event: ATT_YS_EC */

/*============================================================================
 * Orbit Event Type
 *===========================================================================*/

typedef struct event_tag {  /* orbit event list type */
    gtime_t time;           /* event time (gpst) */
    int type;               /* event type */
    double p [MAXNP_EVENT]; /* event parameters */
    double s [MAXNP_EVENT]; /* standard deviation of event parameters */
    double p0[MAXNP_EVENT]; /* event parameters checkpoint */
    double s0[MAXNP_EVENT]; /* standard deviation checkpoint */
    int ix,nx;              /* index/number of estimated parameters */
    struct event_tag *next; /* pointer to next */
} event_t;

/*============================================================================
 * MADOCA-PPP / Local Correction Constants
 *===========================================================================*/

#define TOW_LSB         30.0            /* epoch time 30s */
#define MAXTRP          2880            /* all time data, (1hour/1day) */

#define SVR_CYCLE       10              /* server cycle (ms) */

/*============================================================================
 * Console / Application Constants
 *===========================================================================*/

#define LOGIN_PROMPT    "*** LCLCMBRT MONITOR CONSOLE (" MRTKLIB_SOFTNAME " ver." MRTKLIB_VERSION_STRING ") ***"
#define LOGIN_USER      "admin"         /* console login user */
#define LOGIN_PASSWD    "root"          /* console login password */
#define INACT_TIMEOUT   300             /* console inactive timeout (s) */

#define ESC_CLEAR       "\033[H\033[2J" /* ansi/vt100 escape: erase screen */
#define ESC_RESET       "\033[0m"       /* ansi/vt100 escape: reset attribute */
#define ESC_BOLD        "\033[1m"       /* ansi/vt100 escape: bold */

/*============================================================================
 * RTCM Constants
 *===========================================================================*/

#ifndef RTCM3PREAMB
#define RTCM3PREAMB     0xD3            /* rtcm ver.3 frame preamble */
#endif

/*============================================================================
 * Download Option Constants
 *===========================================================================*/

#define DLOPT_FORCE   0x01              /* download option: force download existing */
#define DLOPT_KEEPCMP 0x02              /* download option: keep compressed file */
#define DLOPT_HOLDERR 0x04              /* download option: hold on error file */
#define DLOPT_HOLDLST 0x08              /* download option: hold on listing file */

/*============================================================================
 * Legacy Types (not yet implemented — preserved for API compatibility)
 *===========================================================================*/

typedef struct {        /* NORAD TLE data type */
    char name [32];     /* common name */
    char alias[32];     /* alias name */
    char satno[16];     /* satellite catalog number */
    char satclass;      /* classification */
    char desig[16];     /* international designator */
    gtime_t epoch;      /* element set epoch (UTC) */
    double ndot;        /* 1st derivative of mean motion */
    double nddot;       /* 2nd derivative of mean motion */
    double bstar;       /* B* drag term */
    int etype;          /* element set type */
    int eleno;          /* element number */
    double inc;         /* orbit inclination (deg) */
    double OMG;         /* right ascension of ascending node (deg) */
    double ecc;         /* eccentricity */
    double omg;         /* argument of perigee (deg) */
    double M;           /* mean anomaly (deg) */
    double n;           /* mean motion (rev/day) */
    int rev;            /* revolution number at epoch */
} tled_t;

typedef struct {        /* NORAD TLE (two line element) type */
    int n,nmax;         /* number/max number of two line element data */
    tled_t *data;       /* NORAD TLE data */
} tle_t;

typedef struct {        /* download URL type */
    char type[32];      /* data type */
    char path[1024];    /* URL path */
    char dir [1024];    /* local directory */
    double tint;        /* time interval (s) */
} url_t;

typedef struct {        /* GIS data point type */
    double pos[3];      /* point data {lat,lon,height} (rad,m) */
} gis_pnt_t;

typedef struct {        /* GIS data polyline type */
    int npnt;           /* number of points */
    double bound[4];    /* boundary {lat0,lat1,lon0,lon1} */
    double *pos;        /* position data (3 x npnt) */
} gis_poly_t;

typedef struct {        /* GIS data polygon type */
    int npnt;           /* number of points */
    double bound[4];    /* boundary {lat0,lat1,lon0,lon1} */
    double *pos;        /* position data (3 x npnt) */
} gis_polygon_t;

typedef struct gisd_tag { /* GIS data list type */
    int type;           /* data type (1:point,2:polyline,3:polygon) */
    void *data;         /* data body */
    struct gisd_tag *next; /* pointer to next */
} gisd_t;

typedef struct {        /* GIS type */
    char name[MAXGISLAYER][256]; /* name */
    int flag[MAXGISLAYER];     /* flag */
    gisd_t *data[MAXGISLAYER]; /* gis data list */
    double bound[4];    /* boundary {lat0,lat1,lon0,lon1} */
} gis_t;

/*============================================================================
 * Legacy Function Declarations (not yet implemented)
 *===========================================================================*/

/* datum transformation */
int loaddatump(const char *file);
int tokyo2jgd(double *pos);
int jgd2tokyo(double *pos);

/* RINEX conversion */
int convrnx(int format, rnxopt_t *opt, const char *file, char **ofile);

/* NORAD TLE functions */
int tle_read(const char *file, tle_t *tle);
int tle_name_read(const char *file, tle_t *tle);
int tle_pos(gtime_t time, const char *name, const char *satno,
            const char *desig, const tle_t *tle, const erp_t *erp, double *rs);

/* Google Earth KML converter */
int convkml(const char *infile, const char *outfile, gtime_t ts, gtime_t te,
            double tint, int qflg, double *offset, int tcolor, int pcolor,
            int outalt, int outtime);

/* GPX converter */
int convgpx(const char *infile, const char *outfile, gtime_t ts, gtime_t te,
            double tint, int qflg, double *offset, int outtrk, int outpnt,
            int outalt, int outtime);

/* downloader functions */
int dl_readurls(const char *file, char **types, int ntype, url_t *urls,
                int nmax);
int dl_readstas(const char *file, char **stas, int nmax);
int dl_exec(gtime_t ts, gtime_t te, double ti, int seqnos, int seqnoe,
            const url_t *urls, int nurl, char **stas, int nsta, const char *dir,
            const char *usr, const char *pwd, const char *proxy, int opts,
            char *msg, FILE *fp);
void dl_test(gtime_t ts, gtime_t te, double ti, const url_t *urls, int nurl,
             char **stas, int nsta, const char *dir, int ncol, int datefmt,
             FILE *fp);

/* GIS data functions */
int gis_read(const char *file, gis_t *gis, int layer);
void gis_free(gis_t *gis);

/*============================================================================
 * Application-Defined Callbacks
 *===========================================================================*/

/**
 * @brief Show status message (application-defined).
 * @param[in] format  printf-style format string
 * @return Status (0:ok)
 */
extern int showmsg(const char *format,...);

/**
 * @brief Set time span for progress display (application-defined).
 * @param[in] ts  Start time
 * @param[in] te  End time
 */
extern void settspan(gtime_t ts, gtime_t te);

/**
 * @brief Set current time for progress display (application-defined).
 * @param[in] time  Current processing time
 */
extern void settime(gtime_t time);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_CONST_H */
