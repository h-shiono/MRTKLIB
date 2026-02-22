/*------------------------------------------------------------------------------
* rtklib.h : RTKLIB constants, types and function prototypes
*
* Copyright (C) 2024-2025 Japan Aerospace Exploration Agency. All Rights Reserved.
* Copyright (C) 2007-2021 by T.TAKASU, All rights reserved.
*
* options : -DENAGLO   enable GLONASS
*           -DENAGAL   enable Galileo
*           -DENAQZS   enable QZSS
*           -DENACMP   enable BeiDou
*           -DENAIRN   enable IRNSS
*           -DNFREQ=n  set number of obs codes/frequencies
*           -DNEXOBS=n set number of extended obs codes
*           -DMAXOBS=n set max number of obs data in an epoch
*           -DWIN32    use WIN32 API
*           -DWIN_DLL  generate library as Windows DLL
*
* history : 2007/01/13 1.0  rtklib ver.1.0.0
*           2007/03/20 1.1  rtklib ver.1.1.0
*           2008/07/15 1.2  rtklib ver.2.1.0
*           2008/10/19 1.3  rtklib ver.2.1.1
*           2009/01/31 1.4  rtklib ver.2.2.0
*           2009/04/30 1.5  rtklib ver.2.2.1
*           2009/07/30 1.6  rtklib ver.2.2.2
*           2009/12/25 1.7  rtklib ver.2.3.0
*           2010/07/29 1.8  rtklib ver.2.4.0
*           2011/05/27 1.9  rtklib ver.2.4.1
*           2013/03/28 1.10 rtklib ver.2.4.2
*           2021/01/04 1.11 rtklib ver.2.4.3 b35
*           2024/02/01 1.12 branch from ver.2.4.3b35 for MALIB
*           2024/08/02 1.13 MALIB ver.1.1.0
*                           add stat format
*           2024/09/26 1.14 MALIB ver.1.1.1
*           2024/12/20 1.15 MALIB ver.1.1.2
*           2025/02/06 1.16 MALIB ver.1.1.3
*-----------------------------------------------------------------------------*/
#ifndef RTKLIB_H
#define RTKLIB_H
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <pthread.h>
#include <sys/select.h>
#endif

/* MRTKLIB modular headers — canonical definitions live here now */
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_atmos.h"
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_peph.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_sys.h"
#include "mrtklib/mrtk_astro.h"
#include "mrtklib/mrtk_antenna.h"
#include "mrtklib/mrtk_station.h"
#include "mrtklib/mrtk_rinex.h"
#include "mrtklib/mrtk_tides.h"
#include "mrtklib/mrtk_geoid.h"
#include "mrtklib/mrtk_sbas.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_ionex.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_sol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* constants -----------------------------------------------------------------*/

#define SOFTNAME    "MALIB"      /* software name */
#define VER_MALIB   "1.1.0"             /* MALIB version */
#define PATCH_LEVEL_MALIB "feature/1.2.0" /* patch level */

#define COPYRIGHT_MALIB \
            "Copyright (C) 2007-2023 T.Takasu\nAll rights reserved." \
            "Copyright (C) 2023-2025, TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION. All Rights Reserved." \
            "Copyright (C) 2023-2025, Japan Aerospace Exploration Agency. All Rights Reserved."

#define PI          3.1415926535897932  /* pi */
#define D2R         (PI/180.0)          /* deg to rad */
#define R2D         (180.0/PI)          /* rad to deg */
#define CLIGHT      299792458.0         /* speed of light (m/s) */
#define SC2RAD      3.1415926535898     /* semi-circle to radian (IS-GPS) */
#define AU          149597870691.0      /* 1 AU (m) */
#define AS2R        (D2R/3600.0)        /* arc sec to radian */
#define M2NS        (1E9/CLIGHT)        /* m to ns */
#define NS2M        (CLIGHT/1E9)        /* ns to m */

#define OMGE        7.2921151467E-5     /* earth angular velocity (IS-GPS) (rad/s) */

#define RE_WGS84    6378137.0           /* earth semimajor axis (WGS84) (m) */
#define FE_WGS84    (1.0/298.257223563) /* earth flattening (WGS84) */

#define HION        350000.0            /* ionosphere height (m) */

#define MAXFREQ     7                   /* max NFREQ */

#define FREQ1       1.57542E9           /* L1/E1/B1C  frequency (Hz) */
#define FREQ2       1.22760E9           /* L2         frequency (Hz) */
#define FREQ5       1.17645E9           /* L5/E5a/B2a frequency (Hz) */
#define FREQ6       1.27875E9           /* E6/L6  frequency (Hz) */
#define FREQ7       1.20714E9           /* E5b    frequency (Hz) */
#define FREQ8       1.191795E9          /* E5a+b  frequency (Hz) */
#define FREQ9       2.492028E9          /* S      frequency (Hz) */
#define FREQ1_GLO   1.60200E9           /* GLONASS G1 base frequency (Hz) */
#define DFRQ1_GLO   0.56250E6           /* GLONASS G1 bias frequency (Hz/n) */
#define FREQ2_GLO   1.24600E9           /* GLONASS G2 base frequency (Hz) */
#define DFRQ2_GLO   0.43750E6           /* GLONASS G2 bias frequency (Hz/n) */
#define FREQ3_GLO   1.202025E9          /* GLONASS G3 frequency (Hz) */
#define FREQ1a_GLO  1.600995E9          /* GLONASS G1a frequency (Hz) */
#define FREQ2a_GLO  1.248060E9          /* GLONASS G2a frequency (Hz) */
#define FREQ1_CMP   1.561098E9          /* BDS B1I     frequency (Hz) */
#define FREQ2_CMP   1.20714E9           /* BDS B2I/B2b frequency (Hz) */
#define FREQ3_CMP   1.26852E9           /* BDS B3      frequency (Hz) */

#define EFACT_GPS   1.0                 /* error factor: GPS */
#define EFACT_GLO   1.5                 /* error factor: GLONASS */
#define EFACT_GAL   1.0                 /* error factor: Galileo */
#define EFACT_QZS   1.0                 /* error factor: QZSS */
#define EFACT_CMP   1.0                 /* error factor: BeiDou */
#define EFACT_IRN   1.5                 /* error factor: NavIC */
#define EFACT_SBS   3.0                 /* error factor: SBAS */

#define SYS_NONE    0x00                /* navigation system: none */
#define SYS_GPS     0x01                /* navigation system: GPS */
#define SYS_SBS     0x02                /* navigation system: SBAS */
#define SYS_GLO     0x04                /* navigation system: GLONASS */
#define SYS_GAL     0x08                /* navigation system: Galileo */
#define SYS_QZS     0x10                /* navigation system: QZSS */
#define SYS_CMP     0x20                /* navigation system: BeiDou */
#define SYS_IRN     0x40                /* navigation system: NavIC */
#define SYS_LEO     0x80                /* navigation system: LEO */
#define SYS_ALL     0xFF                /* navigation system: all */

#define TSYS_GPS    0                   /* time system: GPS time */
#define TSYS_UTC    1                   /* time system: UTC */
#define TSYS_GLO    2                   /* time system: GLONASS time */
#define TSYS_GAL    3                   /* time system: Galileo time */
#define TSYS_QZS    4                   /* time system: QZSS time */
#define TSYS_CMP    5                   /* time system: BeiDou time */
#define TSYS_IRN    6                   /* time system: IRNSS time */

/* NFREQ, NFREQGLO, NEXOBS, SNR_UNIT moved to mrtklib/mrtk_foundation.h */

/* Constellation PRN defines, MAXSAT, MAXSTA moved to mrtklib/mrtk_foundation.h */

/* MAXOBS, MAXRCV, MAXOBSTYPE, DTTOL moved to mrtklib/mrtk_foundation.h */
#define MAXDTOE     7200.0              /* max time difference to GPS Toe (s) */
#define MAXDTOE_QZS 7200.0              /* max time difference to QZSS Toe (s) */
#define MAXDTOE_GAL 14400.0             /* max time difference to Galileo Toe (s) */
#define MAXDTOE_CMP 21600.0             /* max time difference to BeiDou Toe (s) */
#define MAXDTOE_GLO 1800.0              /* max time difference to GLONASS Toe (s) */
#define MAXDTOE_IRN 7200.0              /* max time difference to IRNSS Toe (s) */
#define MAXDTOE_SBS 360.0               /* max time difference to SBAS Toe (s) */
#define MAXDTOE_S   86400.0             /* max time difference to ephem toe (s) for other */
#define MAXGDOP     300.0               /* max GDOP */

#define INT_SWAP_TRAC 86400.0           /* swap interval of trace file (s) */
#define INT_SWAP_STAT 86400.0           /* swap interval of solution status file (s) */

/* MAXEXFILE moved to mrtklib/mrtk_foundation.h */
#define MAXSBSAGEF  30.0                /* max age of SBAS fast correction (s) */
#define MAXSBSAGEL  1800.0              /* max age of SBAS long term corr (s) */
#define MAXSBSURA   8                   /* max URA of SBAS satellite */
/* MAXBAND..MAXRCVCMD moved to mrtklib/mrtk_foundation.h */

#define RNX2VER     2.10                /* RINEX ver.2 default output version */
#define RNX3VER     3.00                /* RINEX ver.3 default output version */

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

/* CODE_NONE..MAXCODE moved to mrtklib/mrtk_foundation.h */
/* PMODE_*, SOLF_*, SOLQ_*, TIMES_*, IONOOPT_*, TROPOPT_*, EPHOPT_*,
 * ARMODE_*, SBSOPT_*, POSOPT_* moved to mrtklib/mrtk_opt.h */

#define STR_NONE     0                  /* stream type: none */
#define STR_SERIAL   1                  /* stream type: serial */
#define STR_FILE     2                  /* stream type: file */
#define STR_TCPSVR   3                  /* stream type: TCP server */
#define STR_TCPCLI   4                  /* stream type: TCP client */
#define STR_NTRIPSVR 5                  /* stream type: NTRIP server */
#define STR_NTRIPCLI 6                  /* stream type: NTRIP client */
#define STR_FTP      7                  /* stream type: ftp */
#define STR_HTTP     8                  /* stream type: http */
#define STR_NTRIPCAS 9                  /* stream type: NTRIP caster */
#define STR_UDPSVR   10                 /* stream type: UDP server */
#define STR_UDPCLI   11                 /* stream type: UDP server */
#define STR_MEMBUF   12                 /* stream type: memory buffer */

#define STRFMT_RTCM2 0                  /* stream format: RTCM 2 */
#define STRFMT_RTCM3 1                  /* stream format: RTCM 3 */
#define STRFMT_OEM4  2                  /* stream format: NovAtel OEMV/4 */
#define STRFMT_OEM3  3                  /* stream format: NovAtel OEM3 */
#define STRFMT_UBX   4                  /* stream format: u-blox LEA-*T */
#define STRFMT_SS2   5                  /* stream format: NovAtel Superstar II */
#define STRFMT_CRES  6                  /* stream format: Hemisphere */
#define STRFMT_STQ   7                  /* stream format: SkyTraq S1315F */
#define STRFMT_JAVAD 8                  /* stream format: JAVAD GRIL/GREIS */
#define STRFMT_NVS   9                  /* stream format: NVS NVC08C */
#define STRFMT_BINEX 10                 /* stream format: BINEX */
#define STRFMT_RT17  11                 /* stream format: Trimble RT17 */
#define STRFMT_SEPT  12                 /* stream format: Septentrio */
#define STRFMT_RINEX 13                 /* stream format: RINEX */
#define STRFMT_SP3   14                 /* stream format: SP3 */
#define STRFMT_RNXCLK 15                /* stream format: RINEX CLK */
#define STRFMT_SBAS  16                 /* stream format: SBAS messages */
#define STRFMT_NMEA  17                 /* stream format: NMEA 0183 */
#define STRFMT_STAT  20                 /* stream format: stat */
#define STRFMT_L6E   21                 /* stream format: L6E CSSR */
#define MAXRCVFMT    12                 /* max number of receiver format */

#define STR_MODE_R  0x1                 /* stream mode: read */
#define STR_MODE_W  0x2                 /* stream mode: write */
#define STR_MODE_RW 0x3                 /* stream mode: read/write */

/* GEOID_* constants moved to mrtklib/mrtk_geoid.h */

#define COMMENTH    "%"                 /* comment line indicator for solution */
#define MSG_DISCONN "$_DISCONNECT\r\n"  /* disconnect message */

#define DLOPT_FORCE   0x01              /* download option: force download existing */
#define DLOPT_KEEPCMP 0x02              /* download option: keep compressed file */
#define DLOPT_HOLDERR 0x04              /* download option: hold on error file */
#define DLOPT_HOLDLST 0x08              /* download option: hold on listing file */

#define LLI_SLIP    0x01                /* LLI: cycle-slip */
#define LLI_HALFC   0x02                /* LLI: half-cycle not resovled */
#define LLI_BOCTRK  0x04                /* LLI: boc tracking of mboc signal */
#define LLI_HALFA   0x40                /* LLI: half-cycle added */
#define LLI_HALFS   0x80                /* LLI: half-cycle subtracted */

#define P2_5        0.03125             /* 2^-5 */
#define P2_6        0.015625            /* 2^-6 */
#define P2_11       4.882812500000000E-04 /* 2^-11 */
#define P2_15       3.051757812500000E-05 /* 2^-15 */
#define P2_17       7.629394531250000E-06 /* 2^-17 */
#define P2_19       1.907348632812500E-06 /* 2^-19 */
#define P2_20       9.536743164062500E-07 /* 2^-20 */
#define P2_21       4.768371582031250E-07 /* 2^-21 */
#define P2_23       1.192092895507810E-07 /* 2^-23 */
#define P2_24       5.960464477539063E-08 /* 2^-24 */
#define P2_27       7.450580596923828E-09 /* 2^-27 */
#define P2_29       1.862645149230957E-09 /* 2^-29 */
#define P2_30       9.313225746154785E-10 /* 2^-30 */
#define P2_31       4.656612873077393E-10 /* 2^-31 */
#define P2_32       2.328306436538696E-10 /* 2^-32 */
#define P2_33       1.164153218269348E-10 /* 2^-33 */
#define P2_35       2.910383045673370E-11 /* 2^-35 */
#define P2_38       3.637978807091710E-12 /* 2^-38 */
#define P2_39       1.818989403545856E-12 /* 2^-39 */
#define P2_40       9.094947017729280E-13 /* 2^-40 */
#define P2_43       1.136868377216160E-13 /* 2^-43 */
#define P2_48       3.552713678800501E-15 /* 2^-48 */
#define P2_50       8.881784197001252E-16 /* 2^-50 */
#define P2_55       2.775557561562891E-17 /* 2^-55 */

/* MAXBSNXSYS moved to mrtklib/mrtk_foundation.h */
#define SSR_VENDOR_L6   0               /* vendor type L6(MADOCA-PPP) */
#define SSR_VENDOR_RTCM 1               /* vendor type RTCM3(JAXA-MADOCA) */
#define MAXAGESSRL6     60.0            /* max age of ssr satellite code and phase bias (s) L6E MADOCA-PPP*/

#define SATT_NORMAL     0               /* nominal attitude  */


#define GM_EGM          3.986004415E14  /* earth grav. constant   (EGM96/2008) */
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
#define STYPE_BDS_I_YS  21              /* satellite type: BeiDou IGSO(does not enter ON mode) */
#define STYPE_BDS_M_YS  22              /* satellite type: BeiDou MEO(does not enter ON mode) */
#define STYPE_BDS3_G    23              /* satellite type: BeiDou-3 GEO */
#define STYPE_BDS3_I_C  24              /* satellite type: BeiDou-3 IGSO by CAST */
#define STYPE_BDS3_I_S  25              /* satellite type: BeiDou-3 IGSO by SECM */
#define STYPE_BDS3_M_C  26              /* satellite type: BeiDou-3 MEO  by CAST */
#define STYPE_BDS3_M_S  27              /* satellite type: BeiDou-3 MEO  by SECM */

#define STYPE_QZS_I_YS  30              /* satellite type: QZSS Block IQ(enable beta angle 15 and 17.5[deg]) */
#define STYPE_LEO       31              /* satellite type: LEO */
#define STYPE_MAX       32              /* max satellite type */

#define MAXNP_EVENT     6               /* max number of event parameters */

#define EVENT_NONE      0               /* orbit event: no event */
#define EVENT_ATT_EC    1               /* orbit event: ATT_EC */
#define EVENT_ATT_YS    2               /* orbit event: ATT_YS */
#define EVENT_ATT_EC_YS 3               /* orbit event: ATT_EC_YS */
#define EVENT_ATT_YS_EC 4               /* orbit event: ATT_YS_EC */

#define TOW_LSB         30.0            /* epoch time 30s */
#define MAXTRP          2880            /* all time data, (1hour/1day) */
#define TYPE_TRP        0               /* data type of troposphere */
#define TYPE_ION        1               /* data type of ionosphere */
#define MAX_DIST_TRP    100.0           /* max distance for tropospheric correction (km) */
#define MAX_DIST_ION    50.0            /* max distance for ionospheric correction (km) */
#define MODE_PLANE      0               /* interpolation mode planer fitting */
#define MODE_DISTWGT    1               /* interpolation mode weighted average of distance */

#define SVR_CYCLE       10              /* server cycle (ms) */
/* MAXSITES moved to mrtklib/mrtk_foundation.h */

#define LOGIN_PROMPT    "*** LCLCMBRT MONITOR CONSOLE (" SOFTNAME " ver." VER_MALIB ") ***"
#define LOGIN_USER      "admin"         /* console login user */
#define LOGIN_PASSWD    "root"          /* console login password */
#define INACT_TIMEOUT   300             /* console inactive timeout (s) */

#define ESC_CLEAR       "\033[H\033[2J" /* ansi/vt100 escape: erase screen */
#define ESC_RESET       "\033[0m"       /* ansi/vt100 escape: reset attribute */
#define ESC_BOLD        "\033[1m"       /* ansi/vt100 escape: bold */

#define RTCM3PREAMB     0xD3            /* rtcm ver.3 frame preamble */
/* MAXBLK, MAXTRPSTA, MAXIONSTA, MAX_REJ_SITES moved to mrtklib/mrtk_foundation.h */
#define BTYPE_GRID      0               /* block type grid */
#define BTYPE_STA       1               /* block type station */

#define ION_BLKSIZE     2.0             /* ionosphere block size */
#define TRP_BLKSIZE     4.0             /* troposphere block size */

#define MAX_STANAME     16              /* max length of station name */

#ifdef WIN32
#define rtk_thread_t    HANDLE
#define rtk_lock_t      CRITICAL_SECTION
#define rtk_initlock(f) InitializeCriticalSection(f)
#define rtk_lock(f)     EnterCriticalSection(f)
#define rtk_unlock(f)   LeaveCriticalSection(f)
#define FILEPATHSEP '\\'
#else
#define rtk_thread_t    pthread_t
#define rtk_lock_t      pthread_mutex_t
#define rtk_initlock(f) pthread_mutex_init(f,NULL)
#define rtk_lock(f)     pthread_mutex_lock(f)
#define rtk_unlock(f)   pthread_mutex_unlock(f)
#define FILEPATHSEP '/'
#endif

/* type definitions ----------------------------------------------------------*/

/* gtime_t is now defined in mrtklib/mrtk_time.h */
/* obsd_t, obs_t moved to mrtklib/mrtk_obs.h */

/* erpd_t and erp_t moved to mrtklib/mrtk_peph.h */
/* pcv_t, pcvs_t moved to mrtklib/mrtk_nav.h */

/* alm_t, galm_t moved to mrtklib/mrtk_eph.h */

/* eph_t moved to mrtklib/mrtk_eph.h */

/* geph_t moved to mrtklib/mrtk_eph.h */
/* peph_t, pclk_t moved to mrtklib/mrtk_nav.h */
/* seph_t moved to mrtklib/mrtk_eph.h */

typedef struct {        /* NORAL TLE data type */
    char name [32];     /* common name */
    char alias[32];     /* alias name */
    char satno[16];     /* satellilte catalog number */
    char satclass;      /* classification */
    char desig[16];     /* international designator */
    gtime_t epoch;      /* element set epoch (UTC) */
    double ndot;        /* 1st derivative of mean motion */
    double nddot;       /* 2st derivative of mean motion */
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

/* tec_t..nav_t, sta_t and all dependent types moved to mrtklib/mrtk_nav.h */
/* sol_t, solbuf_t, solstat_t, solstatbuf_t, ssat_t moved to mrtklib/mrtk_sol.h */
/* rtcm_t moved to mrtklib/mrtk_rtcm.h */
/* rnxctr_t moved to mrtklib/mrtk_rinex.h */

typedef struct {        /* download URL type */
    char type[32];      /* data type */
    char path[1024];    /* URL path */
    char dir [1024];    /* local directory */
    double tint;        /* time interval (s) */
} url_t;

typedef struct {        /* option type */
    const char *name;   /* option name */
    int format;         /* option format (0:int,1:double,2:string,3:enum) */
    void *var;          /* pointer to option variable */
    const char *comment; /* option comment/enum labels/unit */
} opt_t;

/* snrmask_t moved to mrtklib/mrtk_obs.h */
/* prcopt_t moved to mrtklib/mrtk_opt.h */

/* solopt_t, filopt_t moved to mrtklib/mrtk_opt.h */
/* rnxopt_t moved to mrtklib/mrtk_rinex.h */
/* ssat_t moved to mrtklib/mrtk_sol.h */

typedef struct {        /* ambiguity control type */
    gtime_t epoch[4];   /* last epoch */
    int n[4];           /* number of epochs */
    double LC [4];      /* linear combination average */
    double LCv[4];      /* linear combination variance */
    int fixcnt;         /* fix count */
    char flags[MAXSAT]; /* fix flags */
} ambc_t;

typedef struct {        /* RTK control/result type */
    sol_t  sol;         /* RTK solution */
    double rb[6];       /* base position/velocity (ecef) (m|m/s) */
    int nx,na;          /* number of float states/fixed states */
    double tt;          /* time difference between current and previous (s) */
    double *x, *P;      /* float states and their covariance */
    double *xa,*Pa;     /* fixed states and their covariance */
    int nfix;           /* number of continuous fixes of ambiguity */
    ambc_t ambc[MAXSAT]; /* ambibuity control */
    ssat_t ssat[MAXSAT]; /* satellite status */
    int neb;            /* bytes in error message buffer */
    char errbuf[MAXERRMSG]; /* error message buffer */
    prcopt_t opt;       /* processing options */
} rtk_t;

typedef struct {        /* receiver raw data control type */
    gtime_t time;       /* message time */
    gtime_t tobs[MAXSAT][NFREQ+NEXOBS]; /* observation data time */
    obs_t obs;          /* observation data */
    obs_t obuf;         /* observation data buffer */
    nav_t nav;          /* satellite ephemerides */
    sta_t sta;          /* station parameters */
    int ephsat;         /* update satelle of ephemeris (0:no satellite) */
    int ephset;         /* update set of ephemeris (0-1) */
    sbsmsg_t sbsmsg;    /* SBAS message */
    char msgtype[256];  /* last message type */
    uint8_t subfrm[MAXSAT][380]; /* subframe buffer */
    double lockt[MAXSAT][NFREQ+NEXOBS]; /* lock time (s) */
    double icpp[MAXSAT],off[MAXSAT],icpc; /* carrier params for ss2 */
    double prCA[MAXSAT],dpCA[MAXSAT]; /* L1/CA pseudrange/doppler for javad */
    uint8_t halfc[MAXSAT][NFREQ+NEXOBS]; /* half-cycle add flag */
    char freqn[MAXOBS]; /* frequency number for javad */
    int nbyte;          /* number of bytes in message buffer */ 
    int len;            /* message length (bytes) */
    int iod;            /* issue of data */
    int tod;            /* time of day (ms) */
    int tbase;          /* time base (0:gpst,1:utc(usno),2:glonass,3:utc(su) */
    int flag;           /* general purpose flag */
    int outtype;        /* output message type */
    uint8_t buff[MAXRAWLEN]; /* message buffer */
    char opt[256];      /* receiver dependent options */
    int format;         /* receiver stream format */
    void *rcv_data;     /* receiver dependent data */
} raw_t;

typedef struct {        /* stream type */
    int type;           /* type (STR_???) */
    int mode;           /* mode (STR_MODE_?) */
    int state;          /* state (-1:error,0:close,1:open) */
    uint32_t inb,inr;   /* input bytes/rate */
    uint32_t outb,outr; /* output bytes/rate */
    uint32_t tick_i;    /* input tick tick */
    uint32_t tick_o;    /* output tick */
    uint32_t tact;      /* active tick */
    uint32_t inbt,outbt; /* input/output bytes at tick */
    rtk_lock_t lock;    /* lock flag */
    void *port;         /* type dependent port control struct */
    char path[MAXSTRPATH]; /* stream path */
    char msg [MAXSTRMSG];  /* stream message */
} stream_t;

typedef struct {        /* stream converter type */
    int itype,otype;    /* input and output stream type */
    int nmsg;           /* number of output messages */
    int msgs[32];       /* output message types */
    double tint[32];    /* output message intervals (s) */
    uint32_t tick[32];  /* cycle tick of output message */
    int ephsat[32];     /* satellites of output ephemeris */
    int stasel;         /* station info selection (0:remote,1:local) */
    rtcm_t rtcm;        /* rtcm input data buffer */
    raw_t raw;          /* raw  input data buffer */
    rtcm_t out;         /* rtcm output data buffer */
} strconv_t;

typedef struct {        /* stream server type */
    int state;          /* server state (0:stop,1:running) */
    int cycle;          /* server cycle (ms) */
    int buffsize;       /* input/monitor buffer size (bytes) */
    int nmeacycle;      /* NMEA request cycle (ms) (0:no) */
    int relayback;      /* relay back of output streams (0:no) */
    int nstr;           /* number of streams (1 input + (nstr-1) outputs */
    int npb;            /* data length in peek buffer (bytes) */
    char cmds_periodic[16][MAXRCVCMD]; /* periodic commands */
    double nmeapos[3];  /* NMEA request position (ecef) (m) */
    uint8_t *buff;      /* input buffers */
    uint8_t *pbuf;      /* peek buffer */
    uint32_t tick;      /* start tick */
    stream_t stream[16]; /* input/output streams */
    stream_t strlog[16]; /* return log streams */
    strconv_t *conv[16]; /* stream converter */
    rtk_thread_t thread; /* server thread */
    rtk_lock_t lock;    /* lock flag */
} strsvr_t;

typedef struct {        /* RTK server type */
    int state;          /* server state (0:stop,1:running) */
    int cycle;          /* processing cycle (ms) */
    int nmeacycle;      /* NMEA request cycle (ms) (0:no req) */
    int nmeareq;        /* NMEA request (0:no,1:nmeapos,2:single sol) */
    double nmeapos[3];  /* NMEA request position (ecef) (m) */
    int buffsize;       /* input buffer size (bytes) */
    int format[3];      /* input format {rov,base,corr} */
    solopt_t solopt[2]; /* output solution options {sol1,sol2} */
    int navsel;         /* ephemeris select (0:all,1:rover,2:base,3:corr) */
    int nsbs;           /* number of sbas message */
    int nsol;           /* number of solution buffer */
    rtk_t rtk;          /* RTK control/result struct */
    int nb [3];         /* bytes in input buffers {rov,base} */
    int nsb[2];         /* bytes in soulution buffers */
    int npb[3];         /* bytes in input peek buffers */
    uint8_t *buff[3];   /* input buffers {rov,base,corr} */
    uint8_t *sbuf[2];   /* output buffers {sol1,sol2} */
    uint8_t *pbuf[3];   /* peek buffers {rov,base,corr} */
    sol_t solbuf[MAXSOLBUF]; /* solution buffer */
    uint32_t nmsg[3][12]; /* input message counts */
    raw_t  raw [3];     /* receiver raw control {rov,base,corr} */
    rtcm_t rtcm[3];     /* RTCM control {rov,base,corr} */
    sstat_t sstat;      /* single stat control */
    gtime_t ftime[3];   /* download time {rov,base,corr} */
    char files[3][MAXSTRPATH]; /* download paths {rov,base,corr} */
    obs_t obs[3][MAXOBSBUF]; /* observation data {rov,base,corr} */
    nav_t nav;          /* navigation data */
    sbsmsg_t sbsmsg[MAXSBSMSG]; /* SBAS message buffer */
    stream_t stream[8]; /* streams {rov,base,corr,sol1,sol2,logr,logb,logc} */
    stream_t *moni;     /* monitor stream */
    uint32_t tick;      /* start tick */
    rtk_thread_t thread; /* server thread */
    int cputime;        /* CPU time (ms) for a processing cycle */
    int prcout;         /* missing observation data count */
    int nave;           /* number of averaging base pos */
    double rb_ave[3];   /* averaging base pos */
    char cmds_periodic[3][MAXRCVCMD]; /* periodic commands */
    char cmd_reset[MAXRCVCMD]; /* reset command */
    double bl_reset;    /* baseline length to reset (km) */
    rtk_lock_t lock;    /* lock flag */
} rtksvr_t;

typedef struct {        /* input monitor stream type */
    char sta[32];       /* station */
    int  fmt;           /* format */
    int  stat;          /* input stream status */
    int  dis;           /* disable flag */
    int  type_str;      /* input stream type */
    int  type_log;      /* log stream type */
    char path_str[MAXSTRPATH]; /* input stream path */
    char path_log[MAXSTRPATH]; /* log stream path */
    stream_t str;       /* input stream */
    stream_t log;       /* log stream */
    gtime_t tcon;       /* connet time */
    gtime_t time;       /* time tag of last stat data */
    unsigned int cnt;   /* input data count {stat} */
    sstat_t sstat;      /* stat correction */
    uint8_t buff[4096]; /* stat message buffer */
    int nbyte;          /* number of stat message */
    double rr[3];       /* position(ecef) */
} instat_t;

typedef struct {        /* stream local combination server */
    gtime_t ts;         /* update time */
    int btype;          /* block type */
    int state;          /* state (0:stop,1:run) */
    int navsys;         /* system nav data */
    int ninp;           /* number of input monitor streams */
    unsigned int cnt;   /* count {rtcm} */
    instat_t ins[MAXSITES]; /* input stat streams */
    stat_t stat;        /* input of iono/trop corrections */
    rtcm_t rtcm;        /* output of iono/trop corrections */
    stream_t ostr;      /* output stream for combined data */
    stream_t olog;      /* log stream for combined data */
    rtk_thread_t thread;/* server thread */
    rtk_lock_t lock;    /* mutex for lock */
} lclcmbsvr_t;

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

/* fatalfunc_t is now defined in mrtklib/mrtk_mat.h */

/* global variables ----------------------------------------------------------*/
extern const double chisqr[];        /* Chi-sqr(n) table (alpha=0.001) */
/* prcopt_default, solopt_default moved to mrtklib/mrtk_opt.h */
extern const sbsigpband_t igpband1[9][8]; /* SBAS IGP band 0-8 */
extern const sbsigpband_t igpband2[2][5]; /* SBAS IGP band 9-10 */
extern const char *formatstrs[];     /* stream format strings */
extern opt_t sysopts[];              /* system options table */

/* satno, satsys, satid2no, satno2id moved to mrtklib/mrtk_nav.h */
/* obs2code, code2obs, code2freq, sat2freq, code2idx moved to mrtklib/mrtk_obs.h */
/* testsnr, testelmask, setcodepri, getcodepri moved to mrtklib/mrtk_obs.h */
/* satexclude moved to mrtklib/mrtk_nav.h */

/* matrix and vector functions are now declared in mrtklib/mrtk_mat.h */

/* str2num, reppath, reppaths moved to mrtklib/mrtk_sys.h */

/* time functions are now declared in mrtklib/mrtk_time.h */

/* coordinates transformation functions are now declared in mrtklib/mrtk_coords.h */

/* input and output functions ------------------------------------------------*/
/* readpos, readblq, readelmask moved to mrtklib/mrtk_station.h */
/* sortobs, signal_replace, screent, freeobs moved to mrtklib/mrtk_obs.h */
/* uniqnav, readnav, savenav, freenav moved to mrtklib/mrtk_nav.h */
/* readerp, geterp moved to mrtklib/mrtk_peph.h */

/* debug trace functions -----------------------------------------------------*/
void traceopen(const char *file);
void traceclose(void);
void tracelevel(int level);
void trace(int level, const char *format, ...);
void tracet(int level, const char *format, ...);
void tracemat(int level, const double *A, int n, int m, int p, int q);
void traceobs(int level, const obsd_t *obs, int n);
void tracenav(int level, const nav_t *nav);
void tracegnav(int level, const nav_t *nav);
void tracehnav(int level, const nav_t *nav);
void tracepeph(int level, const nav_t *nav);
void tracepclk(int level, const nav_t *nav);
void traceb(int level, const uint8_t *p, int n);

/* execcmd, expath, createdir moved to mrtklib/mrtk_sys.h */

/* positioning geometry functions are now declared in mrtklib/mrtk_coords.h */

/* atmosphere model functions (ionmodel, ionmapf, ionppp, tropmodel, tropmapf)
 * are now declared in mrtklib/mrtk_atmos.h */
/* readtec, iontec moved to mrtklib/mrtk_ionex.h */
int ionocorr(gtime_t time, const nav_t *nav, int sat, const double *pos,
             const double *azel, int ionoopt, double *ion, double *var);
int tropcorr(gtime_t time, const nav_t *nav, const double *pos,
             const double *azel, int tropopt, double *trp, double *var);

/* readpcv, searchpcv, antmodel, antmodel_s moved to mrtklib/mrtk_antenna.h */
/* sunmoonpos, sunmoonpos_eci moved to mrtklib/mrtk_astro.h */

/* tidedisp moved to mrtklib/mrtk_tides.h */
/* opengeoid, closegeoid, geoidh moved to mrtklib/mrtk_geoid.h */

/* datum transformation ------------------------------------------------------*/
int loaddatump(const char *file);
int tokyo2jgd(double *pos);
int jgd2tokyo(double *pos);

/* RINEX functions -----------------------------------------------------------*/
/* readrnx, readrnxt, readrnxc, outrnxobsh, outrnxobsb, outrnxnavh, outrnxnavb,
   outrnxgnavh, outrnxgnavb, outrnxhnavh, outrnxhnavb, outrnxlnavh,
   outrnxqnavh, outrnxcnavh, outrnxinavh, init_rnxctr, free_rnxctr,
   open_rnxctr, input_rnxctr, rnxopt_t, rnxctr_t
   moved to mrtklib/mrtk_rinex.h */
/* rtk_uncompress moved to mrtklib/mrtk_sys.h */
int convrnx(int format, rnxopt_t *opt, const char *file, char **ofile);

/* ephemeris and clock functions ---------------------------------------------*/
/* eph2clk, geph2clk, seph2clk, eph2pos, geph2pos, seph2pos, alm2pos,
   setseleph, getseleph moved to mrtklib/mrtk_eph.h */
/* satpos, satposs, peph2pos, satantoff, readsp3, readsap, readdcb
   moved to mrtklib/mrtk_nav.h */

/* NORAD TLE (two line element) functions ------------------------------------*/
int tle_read(const char *file, tle_t *tle);
int tle_name_read(const char *file, tle_t *tle);
int tle_pos(gtime_t time, const char *name, const char *satno,
            const char *desig, const tle_t *tle, const erp_t *erp, double *rs);

/* getbitu, getbits, setbitu, setbits, rtk_crc32, rtk_crc24q, rtk_crc16, decode_word
 * moved to mrtklib/mrtk_bits.h */
/* receiver raw data functions -----------------------------------------------*/
int decode_frame(const uint8_t *buff, eph_t *eph, alm_t *alm, double *ion,
                 double *utc);
int test_glostr(const uint8_t *buff);
int decode_glostr(const uint8_t *buff, geph_t *geph, double *utc);
int decode_bds_d1(const uint8_t *buff, eph_t *eph, double *ion, double *utc);
int decode_bds_d2(const uint8_t *buff, eph_t *eph, double *utc);
int decode_gal_inav(const uint8_t *buff, eph_t *eph, double *ion, double *utc);
int decode_gal_fnav(const uint8_t *buff, eph_t *eph, double *ion, double *utc);
int decode_irn_nav(const uint8_t *buff, eph_t *eph, double *ion, double *utc);

int init_raw(raw_t *raw, int format);
void free_raw(raw_t *raw);
int input_raw(raw_t *raw, rtcm_t *rtcm, int format, uint8_t data);
int input_rawf(raw_t *raw, rtcm_t *rtcm, int format, FILE *fp);

/* receiver dependent functions ----------------------------------------------*/
int init_rt17(raw_t *raw);
void free_rt17(raw_t *raw);

int input_oem4(raw_t *raw, uint8_t data);
int input_oem3(raw_t *raw, uint8_t data);
int input_ubx(raw_t *raw, rtcm_t *rtcm, uint8_t data);
int input_ss2(raw_t *raw, uint8_t data);
int input_cres(raw_t *raw, uint8_t data);
int input_stq(raw_t *raw, uint8_t data);
int input_javad(raw_t *raw, uint8_t data);
int input_nvs(raw_t *raw, uint8_t data);
int input_bnx(raw_t *raw, uint8_t data);
int input_rt17(raw_t *raw, uint8_t data);
int input_sbf(raw_t *raw, rtcm_t *rtcm, uint8_t data);
int input_oem4f(raw_t *raw, FILE *fp);
int input_oem3f(raw_t *raw, FILE *fp);
int input_ubxf(raw_t *raw, rtcm_t *rtcm, FILE *fp);
int input_ss2f(raw_t *raw, FILE *fp);
int input_cresf(raw_t *raw, FILE *fp);
int input_stqf(raw_t *raw, FILE *fp);
int input_javadf(raw_t *raw, FILE *fp);
int input_nvsf(raw_t *raw, FILE *fp);
int input_bnxf(raw_t *raw, FILE *fp);
int input_rt17f(raw_t *raw, FILE *fp);
int input_sbff(raw_t *raw, rtcm_t *rtcm, FILE *fp);

int gen_ubx(const char *msg, uint8_t *buff);
int gen_stq(const char *msg, uint8_t *buff);
int gen_nvs(const char *msg, uint8_t *buff);

/* RTCM functions moved to mrtklib/mrtk_rtcm.h */

/* solution functions moved to mrtklib/mrtk_sol.h */

/* Google Earth KML converter ------------------------------------------------*/
int convkml(const char *infile, const char *outfile, gtime_t ts, gtime_t te,
            double tint, int qflg, double *offset, int tcolor, int pcolor,
            int outalt, int outtime);

/* GPX converter -------------------------------------------------------------*/
int convgpx(const char *infile, const char *outfile, gtime_t ts, gtime_t te,
            double tint, int qflg, double *offset, int outtrk, int outpnt,
            int outalt, int outtime);

/* SBAS functions moved to mrtklib/mrtk_sbas.h */

/* options functions ---------------------------------------------------------*/
opt_t *searchopt(const char *name, const opt_t *opts);
int str2opt(opt_t *opt, const char *str);
int opt2str(const opt_t *opt, char *str);
int opt2buf(const opt_t *opt, char *buff);
int loadopts(const char *file, opt_t *opts);
int saveopts(const char *file, const char *mode, const char *comment,
             const opt_t *opts);
void resetsysopts(void);
void getsysopts(prcopt_t *popt, solopt_t *sopt, filopt_t *fopt);
void setsysopts(const prcopt_t *popt, const solopt_t *sopt,
                const filopt_t *fopt);

/* stream data input and output functions ------------------------------------*/
void strinitcom(void);
void strinit(stream_t *stream);
void strlock(stream_t *stream);
void strunlock(stream_t *stream);
int stropen(stream_t *stream, int type, int mode, const char *path);
void strclose(stream_t *stream);
int strread(stream_t *stream, uint8_t *buff, int n);
int strwrite(stream_t *stream, uint8_t *buff, int n);
void strsync(stream_t *stream1, stream_t *stream2);
int strstat(stream_t *stream, char *msg);
int strstatx(stream_t *stream, char *msg);
void strsum(stream_t *stream, int *inb, int *inr, int *outb, int *outr);
void strsetopt(const int *opt);
gtime_t strgettime(stream_t *stream);
void strsendnmea(stream_t *stream, const sol_t *sol);
void strsendcmd(stream_t *stream, const char *cmd);
void strsettimeout(stream_t *stream, int toinact, int tirecon);
void strsetdir(const char *dir);
void strsetproxy(const char *addr);

/* integer ambiguity resolution ----------------------------------------------*/
int lambda(int n, int m, const double *a, const double *Q, double *F,
           double *s);
int lambda_reduction(int n, const double *Q, double *Z);
int lambda_search(int n, int m, const double *a, const double *Q, double *F,
                  double *s);

/* standard positioning ------------------------------------------------------*/
int pntpos(const obsd_t *obs, int n, const nav_t *nav, const prcopt_t *opt,
           sol_t *sol, double *azel, ssat_t *ssat, char *msg);

/* precise positioning -------------------------------------------------------*/
void rtkinit(rtk_t *rtk, const prcopt_t *opt);
void rtkfree(rtk_t *rtk);
int rtkpos(rtk_t *rtk, const obsd_t *obs, int nobs, nav_t *nav);
int rtkopenstat(const char *file, int level);
void rtkclosestat(void);
int rtkoutstat(rtk_t *rtk, char *buff);

/* precise point positioning -------------------------------------------------*/
void pppos(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav);
int pppnx(const prcopt_t *opt);
int pppoutstat(rtk_t *rtk, char *buff);

int ppp_ar(rtk_t *rtk, const obsd_t *obs, int n, int *exc, const nav_t *nav,
           const double *azel, double *x, double *P);

/* post-processing positioning -----------------------------------------------*/
int postpos(gtime_t ts, gtime_t te, double ti, double tu, const prcopt_t *popt,
            const solopt_t *sopt, const filopt_t *fopt, char **infile, int n,
            char *outfile, const char *rov, const char *base);

/* stream server functions ---------------------------------------------------*/
void strsvrinit (strsvr_t *svr, int nout);
int strsvrstart(strsvr_t *svr, int *opts, int *strs, char **paths, char **logs,
                strconv_t **conv, char **cmds, char **cmds_priodic,
                const double *nmeapos);
void strsvrstop(strsvr_t *svr, char **cmds);
void strsvrstat(strsvr_t *svr, int *stat, int *log_stat, int *byte, int *bps,
                char *msg);
strconv_t *strconvnew(int itype, int otype, const char *msgs, int staid,
                      int stasel, const char *opt);
void strconvfree(strconv_t *conv);

/* RTK server functions ------------------------------------------------------*/
int rtksvrinit(rtksvr_t *svr);
void rtksvrfree(rtksvr_t *svr);
int rtksvrstart(rtksvr_t *svr, int cycle, int buffsize, int *strs, char **paths,
                int *formats, int navsel, char **cmds, char **cmds_periodic,
                char **rcvopts, int nmeacycle, int nmeareq,
                const double *nmeapos, prcopt_t *prcopt, solopt_t *solopt,
                stream_t *moni, gtime_t rst, char *errmsg);
void rtksvrstop(rtksvr_t *svr, char **cmds);
int rtksvropenstr(rtksvr_t *svr, int index, int str, const char *path,
                  const solopt_t *solopt);
void rtksvrclosestr(rtksvr_t *svr, int index);
void rtksvrlock(rtksvr_t *svr);
void rtksvrunlock(rtksvr_t *svr);
int rtksvrostat(rtksvr_t *svr, int type, gtime_t *time, int *sat, double *az,
                double *el, int **snr, int *vsat);
void rtksvrsstat(rtksvr_t *svr, int *sstat, char *msg);
int rtksvrmark(rtksvr_t *svr, const char *name, const char *comment);

/* downloader functions ------------------------------------------------------*/
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

/* GIS data functions --------------------------------------------------------*/
int gis_read(const char *file, gis_t *gis, int layer);
void gis_free(gis_t *gis);

/* BIAS-SINEX functions ------------------------------------------------------*/
int getsysno(int sat);
biass_t *getbiass(void);
void addbia(const bia_t *bia, bia_t **ary, int *n, int *nmax);
int readbsnx(const char *file);
void outbsnxh(FILE *fp,gtime_t ts,gtime_t te,const char* agency);
void outbsnxrefh(FILE *fp);
void outbsnxrefb(FILE *fp, int type, char* reftext);
void outbsnxreff(FILE *fp);
void outbsnxcomh(FILE *fp);
void outbsnxcomb(FILE *fp, char* comtext);
void outbsnxcomf(FILE *fp);
void outbsnxsol(FILE *fp);
int udosb_sat(osb_t *osb, gtime_t gt, int mode);
int udosb_station(osb_t *osb, gtime_t gt, int mode, char *name);

/* read fcb functions ------------------------------------------------------*/
int readfcb(const char *file);
int udfcb_sat(osb_t *osb, gtime_t gt, int mode);

/* code/phase bias functions -------------------------------------------------*/
void updatbiass(gtime_t t, prcopt_t *popt, nav_t *nav);

/* application defined functions ---------------------------------------------*/
extern int showmsg(const char *format,...);
extern void settspan(gtime_t ts, gtime_t te);
extern void settime(gtime_t time);

/* MADOCA-PPP functions ------------------------------------------------------*/
extern void init_mcssr(const gtime_t gt);
extern int decode_qzss_l6emsg(rtcm_t *rtcm);
extern int input_qzssl6e(rtcm_t *rtcm, const uint8_t data);
extern int input_qzssl6ef(rtcm_t *rtcm, FILE *fp);
extern int mcssr_sel_biascode(const int sys, const int code);







/* local correction data combination common functions ------------------------*/
extern int initgridsta(const char *setfile, lclblock_t *lclblk, int btype);
extern void grid_intp_trop(const gtime_t gt, const stat_t *stat,
                           lclblock_t *lclblk);
extern void sta_sel_trop(const gtime_t time, const stat_t *stat,
                         lclblock_t *lslblk);
extern void grid_intp_iono(const gtime_t gt, const stat_t *stat,
                           lclblock_t *lclblk, double maxres, int *nrej, 
                           int *ncnt);
extern void sta_sel_iono(const gtime_t gt, const stat_t *stat,
                         lclblock_t *lslblk);
extern void output_lclcmb(rtcm_t *rtcm, int btype, stream_t *ostr);

/* local correction common functions -----------------------------------------*/
extern void initblkinf(blkinf_t *b);
extern int getstano(stat_t *stat, const char* sta);
extern int input_stat(sstat_t *sstat, const char data, char *buff, int *nbyte);
extern int input_statf(sstat_t *sstat, FILE *fp, char *buff, int *nbyte);
extern double posdist(const double *ecef1, const double *ecef2);
extern int get_trop_sta(gtime_t time, const trp_t *trpd, double *trp,
                        double *std);
extern int get_iono_sta(gtime_t time, const ion_t *iond, double *ion,
                        double *std, double *el);
extern int corr_trop_distwgt(const stat_t *stat, gtime_t time,
                             const double *llh,double maxdist, double *trp,
                             double *std);
extern int corr_iono_distwgt(const stat_t *stat, gtime_t time,
                             const double *llh,const double *el, double maxdist,
                             double *ion, double *std, double maxres, int *nrej,
                             int *ncnt);
extern int block2stat(rtcm_t *rtcm, stat_t *stat);

/* rtcm3 local correction message encoder/decoder functions ------------------*/
extern int encode_lcltrop(rtcm_t *rtcm, int type);
extern int encode_lcliono(rtcm_t *rtcm, int type);
extern int decode_lcltrop(rtcm_t *rtcm, int type);
extern int decode_lcliono(rtcm_t *rtcm, int type);

#ifdef __cplusplus
}
#endif
#endif /* RTKLIB_H */
