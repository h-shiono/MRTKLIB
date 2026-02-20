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

#ifndef NFREQ
#define NFREQ       3                   /* number of carrier frequencies */
#endif
#define NFREQGLO    2                   /* number of carrier frequencies of GLONASS */

#ifndef NEXOBS
#define NEXOBS      0                   /* number of extended obs codes */
#endif

#define SNR_UNIT    0.001               /* SNR unit (dBHz) */

#define MINPRNGPS   1                   /* min satellite PRN number of GPS */
#define MAXPRNGPS   32                  /* max satellite PRN number of GPS */
#define NSATGPS     (MAXPRNGPS-MINPRNGPS+1) /* number of GPS satellites */
#define NSYSGPS     1

#ifdef ENAGLO
#define MINPRNGLO   1                   /* min satellite slot number of GLONASS */
#define MAXPRNGLO   27                  /* max satellite slot number of GLONASS */
#define NSATGLO     (MAXPRNGLO-MINPRNGLO+1) /* number of GLONASS satellites */
#define NSYSGLO     1
#else
#define MINPRNGLO   0
#define MAXPRNGLO   0
#define NSATGLO     0
#define NSYSGLO     0
#endif
#ifdef ENAGAL
#define MINPRNGAL   1                   /* min satellite PRN number of Galileo */
#define MAXPRNGAL   36                  /* max satellite PRN number of Galileo */
#define NSATGAL    (MAXPRNGAL-MINPRNGAL+1) /* number of Galileo satellites */
#define NSYSGAL     1
#else
#define MINPRNGAL   0
#define MAXPRNGAL   0
#define NSATGAL     0
#define NSYSGAL     0
#endif
#ifdef ENAQZS
#define MINPRNQZS   193                 /* min satellite PRN number of QZSS */
#define MAXPRNQZS   202                 /* max satellite PRN number of QZSS */
#define MINPRNQZS_S 183                 /* min satellite PRN number of QZSS L1S */
#define MAXPRNQZS_S 191                 /* max satellite PRN number of QZSS L1S */
#define NSATQZS     (MAXPRNQZS-MINPRNQZS+1) /* number of QZSS satellites */
#define NSYSQZS     1
#else
#define MINPRNQZS   0
#define MAXPRNQZS   0
#define MINPRNQZS_S 0
#define MAXPRNQZS_S 0
#define NSATQZS     0
#define NSYSQZS     0
#endif
#ifdef ENACMP
#define MINPRNCMP   1                   /* min satellite sat number of BeiDou */
#define MAXPRNCMP   63                  /* max satellite sat number of BeiDou */
#define NSATCMP     (MAXPRNCMP-MINPRNCMP+1) /* number of BeiDou satellites */
#define NSYSCMP     1
#else
#define MINPRNCMP   0
#define MAXPRNCMP   0
#define NSATCMP     0
#define NSYSCMP     0
#endif
#ifdef ENAIRN
#define MINPRNIRN   1                   /* min satellite sat number of IRNSS */
#define MAXPRNIRN   14                  /* max satellite sat number of IRNSS */
#define NSATIRN     (MAXPRNIRN-MINPRNIRN+1) /* number of IRNSS satellites */
#define NSYSIRN     1
#else
#define MINPRNIRN   0
#define MAXPRNIRN   0
#define NSATIRN     0
#define NSYSIRN     0
#endif
#ifdef ENALEO
#define MINPRNLEO   1                   /* min satellite sat number of LEO */
#define MAXPRNLEO   10                  /* max satellite sat number of LEO */
#define NSATLEO     (MAXPRNLEO-MINPRNLEO+1) /* number of LEO satellites */
#define NSYSLEO     1
#else
#define MINPRNLEO   0
#define MAXPRNLEO   0
#define NSATLEO     0
#define NSYSLEO     0
#endif
#define NSYS        (NSYSGPS+NSYSGLO+NSYSGAL+NSYSQZS+NSYSCMP+NSYSIRN+NSYSLEO) /* number of systems */

#define MINPRNSBS   120                 /* min satellite PRN number of SBAS */
#define MAXPRNSBS   158                 /* max satellite PRN number of SBAS */
#define NSATSBS     (MAXPRNSBS-MINPRNSBS+1) /* number of SBAS satellites */

#define MAXSAT      (NSATGPS+NSATGLO+NSATGAL+NSATQZS+NSATCMP+NSATIRN+NSATSBS+NSATLEO)
                                        /* max satellite number (1 to MAXSAT) */
#define MAXSTA      255

#ifndef MAXOBS
#define MAXOBS      96                  /* max number of obs in an epoch */
#endif
#define MAXRCV      64                  /* max receiver number (1 to MAXRCV) */
#define MAXOBSTYPE  64                  /* max number of obs type in RINEX */
#ifdef OBS_100HZ
#define DTTOL       0.005               /* tolerance of time difference (s) */
#else
#define DTTOL       0.025               /* tolerance of time difference (s) */
#endif
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

#define MAXEXFILE   1024                /* max number of expanded files */
#define MAXSBSAGEF  30.0                /* max age of SBAS fast correction (s) */
#define MAXSBSAGEL  1800.0              /* max age of SBAS long term corr (s) */
#define MAXSBSURA   8                   /* max URA of SBAS satellite */
#define MAXBAND     10                  /* max SBAS band of IGP */
#define MAXNIGP     201                 /* max number of IGP in SBAS band */
#define MAXNGEO     4                   /* max number of GEO satellites */
#define MAXCOMMENT  100                 /* max number of RINEX comments */
#define MAXSTRPATH  1024                /* max length of stream path */
#define MAXSTRMSG   1024                /* max length of stream message */
#define MAXSTRRTK   8                   /* max number of stream in RTK server */
#define MAXSBSMSG   32                  /* max number of SBAS msg in RTK server */
#define MAXSOLMSG   8191                /* max length of solution message */
#define MAXRAWLEN   16384               /* max length of receiver raw message */
#define MAXERRMSG   4096                /* max length of error/warning message */
#define MAXANT      64                  /* max length of station name/antenna type */
#define MAXSOLBUF   256                 /* max number of solution buffer */
#define MAXOBSBUF   128                 /* max number of observation data buffer */
#define MAXNRPOS    16                  /* max number of reference positions */
#define MAXLEAPS    64                  /* max number of leap seconds table */
#define MAXGISLAYER 32                  /* max number of GIS data layers */
#define MAXRCVCMD   4096                /* max length of receiver commands */

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

#define CODE_NONE   0                   /* obs code: none or unknown */
#define CODE_L1C    1                   /* obs code: L1C/A,G1C/A,E1C (GPS,GLO,GAL,QZS,SBS) */
#define CODE_L1P    2                   /* obs code: L1P,G1P,B1C(P) (GPS,GLO,BDS) */
#define CODE_L1W    3                   /* obs code: L1 Z-track (GPS) */
#define CODE_L1Y    4                   /* obs code: L1Y        (GPS) */
#define CODE_L1M    5                   /* obs code: L1M        (GPS) */
#define CODE_L1N    6                   /* obs code: L1codeless,B1codeless (GPS,BDS) */
#define CODE_L1S    7                   /* obs code: L1C(D),B1A(D) (GPS,QZS,BDS) */
#define CODE_L1L    8                   /* obs code: L1C(P),B1A(P) (GPS,QZS,BDS) */
#define CODE_L1E    9                   /* (not used) */
#define CODE_L1A    10                  /* obs code: E1A        (GAL) */
#define CODE_L1B    11                  /* obs code: E1B,L1Sb   (GAL,QZS) */
#define CODE_L1X    12                  /* obs code: E1B+C,L1C(D+P),B1C(D+P) (GAL,QZS,BDS) */
#define CODE_L1Z    13                  /* obs code: E1A+B+C,L1S,B1A(D+P) (GAL,QZS,BDS) */
#define CODE_L2C    14                  /* obs code: L2C/A,G1C/A (GPS,GLO) */
#define CODE_L2D    15                  /* obs code: L2 L1C/A-(P2-P1) (GPS) */
#define CODE_L2S    16                  /* obs code: L2C(M)     (GPS,QZS) */
#define CODE_L2L    17                  /* obs code: L2C(L)     (GPS,QZS) */
#define CODE_L2X    18                  /* obs code: L2C(M+L),B1_2I+Q (GPS,QZS,BDS) */
#define CODE_L2P    19                  /* obs code: L2P,G2P    (GPS,GLO) */
#define CODE_L2W    20                  /* obs code: L2 Z-track (GPS) */
#define CODE_L2Y    21                  /* obs code: L2Y        (GPS) */
#define CODE_L2M    22                  /* obs code: L2M        (GPS) */
#define CODE_L2N    23                  /* obs code: L2codeless (GPS) */
#define CODE_L5I    24                  /* obs code: L5I,E5aI   (GPS,GAL,QZS,SBS) */
#define CODE_L5Q    25                  /* obs code: L5Q,E5aQ   (GPS,GAL,QZS,SBS) */
#define CODE_L5X    26                  /* obs code: L5I+Q,E5aI+Q,L5B+C,B2aD+P (GPS,GAL,QZS,IRN,SBS,BDS) */
#define CODE_L7I    27                  /* obs code: E5bI,B2bI  (GAL,BDS) */
#define CODE_L7Q    28                  /* obs code: E5bQ,B2bQ  (GAL,BDS) */
#define CODE_L7X    29                  /* obs code: E5bI+Q,B2bI+Q (GAL,BDS) */
#define CODE_L6A    30                  /* obs code: E6A,B3A    (GAL,BDS) */
#define CODE_L6B    31                  /* obs code: E6B        (GAL) */
#define CODE_L6C    32                  /* obs code: E6C        (GAL) */
#define CODE_L6X    33                  /* obs code: E6B+C,L6(D+P),B3I+Q (GAL,QZS,BDS) */
#define CODE_L6Z    34                  /* obs code: E6A+B+C,L6(D+E),B3A(D+P) (GAL,QZS,BDS) */
#define CODE_L6S    35                  /* obs code: L6D        (QZS) */
#define CODE_L6L    36                  /* obs code: L6P        (QZS) */
#define CODE_L8I    37                  /* obs code: E5abI      (GAL) */
#define CODE_L8Q    38                  /* obs code: E5abQ      (GAL) */
#define CODE_L8X    39                  /* obs code: E5abI+Q,B2abD+P (GAL,BDS) */
#define CODE_L2I    40                  /* obs code: B1_2I      (BDS) */
#define CODE_L2Q    41                  /* obs code: B1_2Q      (BDS) */
#define CODE_L6I    42                  /* obs code: B3I        (BDS) */
#define CODE_L6Q    43                  /* obs code: B3Q        (BDS) */
#define CODE_L3I    44                  /* obs code: G3I        (GLO) */
#define CODE_L3Q    45                  /* obs code: G3Q        (GLO) */
#define CODE_L3X    46                  /* obs code: G3I+Q      (GLO) */
#define CODE_L1I    47                  /* obs code: B1I        (BDS) (obsolute) */
#define CODE_L1Q    48                  /* obs code: B1Q        (BDS) (obsolute) */
#define CODE_L5A    49                  /* obs code: L5A SPS    (IRN) */
#define CODE_L5B    50                  /* obs code: L5B RS(D)  (IRN) */
#define CODE_L5C    51                  /* obs code: L5C RS(P)  (IRN) */
#define CODE_L9A    52                  /* obs code: SA SPS     (IRN) */
#define CODE_L9B    53                  /* obs code: SB RS(D)   (IRN) */
#define CODE_L9C    54                  /* obs code: SC RS(P)   (IRN) */
#define CODE_L9X    55                  /* obs code: SB+C       (IRN) */
#define CODE_L1D    56                  /* obs code: B1D        (BDS) */
#define CODE_L5D    57                  /* obs code: L5S(I),B2aD (QZS,BDS) */
#define CODE_L5P    58                  /* obs code: L5S(Q),B2aP (QZS,BDS) */
#define CODE_L5Z    59                  /* obs code: L5S(I+Q)   (QZS) */
#define CODE_L6E    60                  /* obs code: L6E        (QZS) */
#define CODE_L7D    61                  /* obs code: B2bD       (BDS) */
#define CODE_L7P    62                  /* obs code: B2bP       (BDS) */
#define CODE_L7Z    63                  /* obs code: B2bD+P     (BDS) */
#define CODE_L8D    64                  /* obs code: B2abD      (BDS) */
#define CODE_L8P    65                  /* obs code: B2abP      (BDS) */
#define CODE_L4A    66                  /* obs code: G1aL1OCd   (GLO) */
#define CODE_L4B    67                  /* obs code: G1aL1OCd   (GLO) */
#define CODE_L4X    68                  /* obs code: G1al1OCd+p (GLO) */
#define CODE_L6D    69                  /* obs code: B3A(D)     (BDS) */
#define CODE_L6P    70                  /* obs code: B3A(P)     (BDS) */
#define MAXCODE     70                  /* max number of obs code */

#define PMODE_SINGLE 0                  /* positioning mode: single */
#define PMODE_DGPS   1                  /* positioning mode: DGPS/DGNSS */
#define PMODE_KINEMA 2                  /* positioning mode: kinematic */
#define PMODE_STATIC 3                  /* positioning mode: static */
#define PMODE_MOVEB  4                  /* positioning mode: moving-base */
#define PMODE_FIXED  5                  /* positioning mode: fixed */
#define PMODE_PPP_KINEMA 6              /* positioning mode: PPP-kinemaric */
#define PMODE_PPP_STATIC 7              /* positioning mode: PPP-static */
#define PMODE_PPP_FIXED 8               /* positioning mode: PPP-fixed */

#define SOLF_LLH    0                   /* solution format: lat/lon/height */
#define SOLF_XYZ    1                   /* solution format: x/y/z-ecef */
#define SOLF_ENU    2                   /* solution format: e/n/u-baseline */
#define SOLF_NMEA   3                   /* solution format: NMEA-183 */
#define SOLF_STAT   4                   /* solution format: solution status */
#define SOLF_GSIF   5                   /* solution format: GSI F1/F2 */

#define SOLQ_NONE   0                   /* solution status: no solution */
#define SOLQ_FIX    1                   /* solution status: fix */
#define SOLQ_FLOAT  2                   /* solution status: float */
#define SOLQ_SBAS   3                   /* solution status: SBAS */
#define SOLQ_DGPS   4                   /* solution status: DGPS/DGNSS */
#define SOLQ_SINGLE 5                   /* solution status: single */
#define SOLQ_PPP    6                   /* solution status: PPP */
#define SOLQ_DR     7                   /* solution status: dead reconing */
#define MAXSOLQ     7                   /* max number of solution status */

#define TIMES_GPST  0                   /* time system: gps time */
#define TIMES_UTC   1                   /* time system: utc */
#define TIMES_JST   2                   /* time system: jst */

#define IONOOPT_OFF 0                   /* ionosphere option: correction off */
#define IONOOPT_BRDC 1                  /* ionosphere option: broadcast model */
#define IONOOPT_SBAS 2                  /* ionosphere option: SBAS model */
#define IONOOPT_IFLC 3                  /* ionosphere option: L1/L2 iono-free LC */
#define IONOOPT_EST 4                   /* ionosphere option: estimation */
#define IONOOPT_TEC 5                   /* ionosphere option: IONEX TEC model */
#define IONOOPT_QZS 6                   /* ionosphere option: QZSS broadcast model */
#define IONOOPT_STEC 8                  /* ionosphere option: SLANT TEC model */

#define TROPOPT_OFF 0                   /* troposphere option: correction off */
#define TROPOPT_SAAS 1                  /* troposphere option: Saastamoinen model */
#define TROPOPT_SBAS 2                  /* troposphere option: SBAS model */
#define TROPOPT_EST 3                   /* troposphere option: ZTD estimation */
#define TROPOPT_ESTG 4                  /* troposphere option: ZTD+grad estimation */
#define TROPOPT_ZTD 5                   /* troposphere option: ZTD correction */

#define EPHOPT_BRDC 0                   /* ephemeris option: broadcast ephemeris */
#define EPHOPT_PREC 1                   /* ephemeris option: precise ephemeris */
#define EPHOPT_SBAS 2                   /* ephemeris option: broadcast + SBAS */
#define EPHOPT_SSRAPC 3                 /* ephemeris option: broadcast + SSR_APC */
#define EPHOPT_SSRCOM 4                 /* ephemeris option: broadcast + SSR_COM */

#define ARMODE_OFF  0                   /* AR mode: off */
#define ARMODE_CONT 1                   /* AR mode: continuous */
#define ARMODE_INST 2                   /* AR mode: instantaneous */
#define ARMODE_FIXHOLD 3                /* AR mode: fix and hold */
#define ARMODE_WLNL 4                   /* AR mode: wide lane/narrow lane */
#define ARMODE_TCAR 5                   /* AR mode: triple carrier ar */

#define SBSOPT_LCORR 1                  /* SBAS option: long term correction */
#define SBSOPT_FCORR 2                  /* SBAS option: fast correction */
#define SBSOPT_ICORR 4                  /* SBAS option: ionosphere correction */
#define SBSOPT_RANGE 8                  /* SBAS option: ranging */

#define POSOPT_POS   0                  /* pos option: LLH/XYZ */
#define POSOPT_SINGLE 1                 /* pos option: average of single pos */
#define POSOPT_FILE  2                  /* pos option: read from pos file */
#define POSOPT_RINEX 3                  /* pos option: rinex header pos */
#define POSOPT_RTCM  4                  /* pos option: rtcm/raw station pos */

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

#define GEOID_EMBEDDED    0             /* geoid model: embedded geoid */
#define GEOID_EGM96_M150  1             /* geoid model: EGM96 15x15" */
#define GEOID_EGM2008_M25 2             /* geoid model: EGM2008 2.5x2.5" */
#define GEOID_EGM2008_M10 3             /* geoid model: EGM2008 1.0x1.0" */
#define GEOID_GSI2000_M15 4             /* geoid model: GSI geoid 2000 1.0x1.5" */
#define GEOID_RAF09       5             /* geoid model: IGN RAF09 for France 1.5"x2" */

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

#define MAXBSNXSYS      7               /* BIAS-SINEX Supported GNSS(G,R,E,J,C,I,S) */
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
#define MAXSITES        256             /* max number of stat input streams */

#define LOGIN_PROMPT    "*** LCLCMBRT MONITOR CONSOLE (" SOFTNAME " ver." VER_MALIB ") ***"
#define LOGIN_USER      "admin"         /* console login user */
#define LOGIN_PASSWD    "root"          /* console login password */
#define INACT_TIMEOUT   300             /* console inactive timeout (s) */

#define ESC_CLEAR       "\033[H\033[2J" /* ansi/vt100 escape: erase screen */
#define ESC_RESET       "\033[0m"       /* ansi/vt100 escape: reset attribute */
#define ESC_BOLD        "\033[1m"       /* ansi/vt100 escape: bold */

#define RTCM3PREAMB     0xD3            /* rtcm ver.3 frame preamble */
#define MAXBLK          32              /* max number of block */
#define MAXTRPSTA       512             /* max number of tropospheric station blocks */
#define MAXIONSTA       256             /* max number of ionospheric station blocks */
#define MAX_REJ_SITES   5               /* max number of reject sites */
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

typedef struct {        /* time struct */
    time_t time;        /* time (s) expressed by standard time_t */
    double sec;         /* fraction of second under 1 s */
} gtime_t;

typedef struct {        /* observation data record */
    gtime_t time;       /* receiver sampling time (GPST) */
    uint8_t sat,rcv;    /* satellite/receiver number */
    uint16_t SNR[NFREQ+NEXOBS]; /* signal strength (0.001 dBHz) */
    uint8_t  LLI[NFREQ+NEXOBS]; /* loss of lock indicator */
    uint8_t code[NFREQ+NEXOBS]; /* code indicator (CODE_???) */
    double L[NFREQ+NEXOBS]; /* observation data carrier-phase (cycle) */
    double P[NFREQ+NEXOBS]; /* observation data pseudorange (m) */
    float  D[NFREQ+NEXOBS]; /* observation data doppler frequency (Hz) */
} obsd_t;

typedef struct {        /* observation data */
    int n,nmax;         /* number of obervation data/allocated */
    obsd_t *data;       /* observation data records */
} obs_t;

typedef struct {        /* ERP (earth rotation parameter) data type */
    double mjd;         /* mjd (days) */
    double xp,yp;       /* pole offset (rad) */
    double xpr,ypr;     /* pole offset rate (rad/day) */
    double ut1_utc;     /* ut1-utc (s) */
    double lod;         /* length of day (s/day) */
} erpd_t;

typedef struct {        /* ERP (earth rotation parameter) type */
    int n,nmax;         /* number and max number of data */
    erpd_t *data;       /* earth rotation parameter data */
} erp_t;

typedef struct {        /* antenna parameter type */
    int sat;            /* satellite number (0:receiver) */
    char type[MAXANT];  /* antenna type */
    char code[MAXANT];  /* serial number or satellite code */
    gtime_t ts,te;      /* valid time start and end */
    double off[NFREQ][ 3]; /* phase center offset e/n/u or x/y/z (m) */
    double var[NFREQ][19]; /* phase center variation (m) */
                        /* el=90,85,...,0 or nadir=0,1,2,3,... (deg) */
} pcv_t;

typedef struct {        /* antenna parameters type */
    int n,nmax;         /* number of data/allocated */
    pcv_t *pcv;         /* antenna parameters data */
} pcvs_t;

typedef struct {        /* GPS/GAL/QZS/BDS/IRN almanac type */
    int sat;            /* satellite number */
    int svh;            /* SV health (0:ok) */
    int svconf;         /* AS and SV config */
    int week;           /* GPS/QZS: GPS week, GAL: Galileo week */
    gtime_t toa;        /* Toa */
    double A,e,i0,OMG0,omg,M0,OMGd; /* SV orbit parameters */
    double toas;        /* Toa (s) in week */
    double f0,f1;       /* SV clock parameters (af0,af1) */
} alm_t;

typedef struct {        /* GLONASS almanac type */
    int sat;            /* satellite number */
    int Cn;             /* unhealthy flag (1:unhealthy) */
    int Mn;             /* satellite type (0:GLONASS,1:GLONASS-M) */
    int Hn;             /* carrier-frequency number */
    gtime_t toa;        /* Toa */
    double tn;          /* time of first acsending node passage (s) */
    double lamn,din,eccn,omgn,dTn,ddTn; /* SV orbit parameters */
    double taun;        /* SV clock parameters */
} galm_t;

typedef struct {        /* GPS/GAL/QZS/BDS/IRN broadcast ephemeris type */
    int sat;            /* satellite number */
    int iode,iodc;      /* IODE,IODC */
    int sva;            /* SV accuracy (URA index) */
    int svh;            /* SV health (0:ok) */
    int week;           /* GPS/QZS: gps week, GAL: galileo week */
    int code;           /* GPS/QZS: code on L2 */
                        /* GAL: data source defined as rinex 3.03 */
                        /* BDS: data source (0:unknown,1:B1I,2:B1Q,3:B2I,4:B2Q,5:B3I,6:B3Q) */
    int flag;           /* GPS/QZS: L2 P data flag */
                        /* BDS: nav type (0:unknown,1:IGSO/MEO,2:GEO) */
    gtime_t toe,toc,ttr; /* Toe,Toc,T_trans */
                        /* SV orbit parameters */
    double A,e,i0,OMG0,omg,M0,deln,OMGd,idot;
    double crc,crs,cuc,cus,cic,cis;
    double toes;        /* Toe (s) in week */
    double fit;         /* fit interval (h) */
    double f0,f1,f2;    /* SV clock parameters (af0,af1,af2) */
    double tgd[6];      /* group delay parameters */
                        /* GPS/QZS:tgd[0]=TGD */
                        /* GAL:tgd[0]=BGD_E1E5a,tgd[1]=BGD_E1E5b */
                        /* BDS:tgd[0]=TGD_B1I ,tgd[1]=TGD_B2I/B2b,tgd[2]=TGD_B1Cp */
                        /*     tgd[3]=TGD_B2ap,tgd[4]=ISC_B1Cd   ,tgd[5]=ISC_B2ad */
    int type;           /* ephemeris type */
                        /* GPS/QZS: 0=LNAV,1=CNAV,2=CNAV-2 */
                        /* GAL    : 0=INAV,1=FNAV */
                        /* BDS    : 0=D1,1=D2,2=CNAV-1,3=CNAV-2,4=CNAV-3 */
                        /* IRN    : 0=LNAV */
    double Adot,ndot;   /* Adot,ndot for CNAV */
} eph_t;

typedef struct {        /* GLONASS broadcast ephemeris type */
    int sat;            /* satellite number */
    int iode;           /* IODE (0-6 bit of tb field) */
    int frq;            /* FCN (frequency channel number) */
    int svh;            /* extended SVH (b3:ln,b2:Cn_a,b1:Cn,b0:Bn) */
    int flags;          /* status flags (b78:M,b6:P4,b5:P3,b4:P2,b23:P1,b01:P) */
    int sva,age;        /* URA index (FT), age of operation (En) */
    gtime_t toe;        /* epoch of ephemerides (gpst) */
    gtime_t tof;        /* message frame time (gpst) */
    double pos[3];      /* satellite position (ecef) (m) */
    double vel[3];      /* satellite velocity (ecef) (m/s) */
    double acc[3];      /* satellite acceleration (ecef) (m/s^2) */
    double taun,gamn;   /* SV clock bias (s)/relative freq bias */
    double dtaun;       /* delay between L1 and L2 (s) */
} geph_t;

typedef struct {        /* precise ephemeris type */
    gtime_t time;       /* time (GPST) */
    int index;          /* ephemeris index for multiple files */
    double pos[MAXSAT][4]; /* satellite position/clock (ecef) (m|s) */
    float  std[MAXSAT][4]; /* satellite position/clock std (m|s) */
    double vel[MAXSAT][4]; /* satellite velocity/clk-rate (m/s|s/s) */
    float  vst[MAXSAT][4]; /* satellite velocity/clk-rate std (m/s|s/s) */
    float  cov[MAXSAT][3]; /* satellite position covariance (m^2) */
    float  vco[MAXSAT][3]; /* satellite velocity covariance (m^2) */
} peph_t;

typedef struct {        /* precise clock type */
    gtime_t time;       /* time (GPST) */
    int index;          /* clock index for multiple files */
    double clk[MAXSAT][1]; /* satellite clock (s) */
    float  std[MAXSAT][1]; /* satellite clock std (s) */
} pclk_t;

typedef struct {        /* SBAS ephemeris type */
    int sat;            /* satellite number */
    gtime_t t0;         /* reference epoch time (GPST) */
    gtime_t tof;        /* time of message frame (GPST) */
    int sva;            /* SV accuracy (URA index) */
    int svh;            /* SV health (0:ok) */
    double pos[3];      /* satellite position (m) (ecef) */
    double vel[3];      /* satellite velocity (m/s) (ecef) */
    double acc[3];      /* satellite acceleration (m/s^2) (ecef) */
    double af0,af1;     /* satellite clock-offset/drift (s,s/s) */
} seph_t;

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

typedef struct {        /* TEC grid type */
    gtime_t time;       /* epoch time (GPST) */
    int ndata[3];       /* TEC grid data size {nlat,nlon,nhgt} */
    double rb;          /* earth radius (km) */
    double lats[3];     /* latitude start/interval (deg) */
    double lons[3];     /* longitude start/interval (deg) */
    double hgts[3];     /* heights start/interval (km) */
    double *data;       /* TEC grid data (tecu) */
    float *rms;         /* RMS values (tecu) */
} tec_t;

typedef struct {        /* SBAS message type */
    int week,tow;       /* receiption time */
    uint8_t prn,rcv;    /* SBAS satellite PRN,receiver number */
    uint8_t msg[29];    /* SBAS message (226bit) padded by 0 */
} sbsmsg_t;

typedef struct {        /* SBAS messages type */
    int n,nmax;         /* number of SBAS messages/allocated */
    sbsmsg_t *msgs;     /* SBAS messages */
} sbs_t;

typedef struct {        /* SBAS fast correction type */
    gtime_t t0;         /* time of applicability (TOF) */
    double prc;         /* pseudorange correction (PRC) (m) */
    double rrc;         /* range-rate correction (RRC) (m/s) */
    double dt;          /* range-rate correction delta-time (s) */
    int iodf;           /* IODF (issue of date fast corr) */
    int16_t udre;       /* UDRE+1 */
    int16_t ai;         /* degradation factor indicator */
} sbsfcorr_t;

typedef struct {        /* SBAS long term satellite error correction type */
    gtime_t t0;         /* correction time */
    int iode;           /* IODE (issue of date ephemeris) */
    double dpos[3];     /* delta position (m) (ecef) */
    double dvel[3];     /* delta velocity (m/s) (ecef) */
    double daf0,daf1;   /* delta clock-offset/drift (s,s/s) */
} sbslcorr_t;

typedef struct {        /* SBAS satellite correction type */
    int sat;            /* satellite number */
    sbsfcorr_t fcorr;   /* fast correction */
    sbslcorr_t lcorr;   /* long term correction */
} sbssatp_t;

typedef struct {        /* SBAS satellite corrections type */
    int iodp;           /* IODP (issue of date mask) */
    int nsat;           /* number of satellites */
    int tlat;           /* system latency (s) */
    sbssatp_t sat[MAXSAT]; /* satellite correction */
} sbssat_t;

typedef struct {        /* SBAS ionospheric correction type */
    gtime_t t0;         /* correction time */
    int16_t lat,lon;    /* latitude/longitude (deg) */
    int16_t give;       /* GIVI+1 */
    float delay;        /* vertical delay estimate (m) */
} sbsigp_t;

typedef struct {        /* IGP band type */
    int16_t x;          /* longitude/latitude (deg) */
    const int16_t *y;   /* latitudes/longitudes (deg) */
    uint8_t bits;       /* IGP mask start bit */
    uint8_t bite;       /* IGP mask end bit */
} sbsigpband_t;

typedef struct {        /* SBAS ionospheric corrections type */
    int iodi;           /* IODI (issue of date ionos corr) */
    int nigp;           /* number of igps */
    sbsigp_t igp[MAXNIGP]; /* ionospheric correction */
} sbsion_t;

typedef struct {        /* DGPS/GNSS correction type */
    gtime_t t0;         /* correction time */
    double prc;         /* pseudorange correction (PRC) (m) */
    double rrc;         /* range rate correction (RRC) (m/s) */
    int iod;            /* issue of data (IOD) */
    double udre;        /* UDRE */
} dgps_t;

typedef struct {        /* SSR correction type */
    gtime_t t0[6];      /* epoch time (GPST) {eph,clk,hrclk,ura,bias,pbias} */
    double udi[6];      /* SSR update interval (s) */
    int iod[6];         /* iod ssr {eph,clk,hrclk,ura,bias,pbias} */
    int iode;           /* issue of data */
    int iodcrc;         /* issue of data crc for beidou/sbas */
    int ura;            /* URA indicator */
    int refd;           /* sat ref datum (0:ITRF,1:regional) */
    double deph [3];    /* delta orbit {radial,along,cross} (m) */
    double ddeph[3];    /* dot delta orbit {radial,along,cross} (m/s) */
    double dclk [3];    /* delta clock {c0,c1,c2} (m,m/s,m/s^2) */
    double hrclk;       /* high-rate clock corection (m) */
    float  cbias[MAXCODE]; /* code biases (m) */
    double pbias[MAXCODE]; /* phase biases (m) */
    float  stdpb[MAXCODE]; /* std-dev of phase biases (m) */
    double yaw_ang,yaw_rate; /* yaw angle and yaw rate (deg,deg/s) */
    uint8_t update;     /* update flag (0:no update,1:update) */
    int vendor;         /* vendor type(0:L6(MADOCA-PPP),1:RTCM3(JAXA-MADOCA)) */
} ssr_t;

typedef struct {        /* stec data type */
    gtime_t gt[2];                     /* update time {satellite,station} */
    char staname[10];                  /* selected station */
    double scb[MAXSAT][MAXCODE];       /* satellite code biases */
    double spb[MAXSAT][MAXCODE];       /* satellite phase biases  */
    double rsyscb[MAXBSNXSYS][MAXCODE];/* system code biases of each station */
    double rsatcb[MAXSAT][MAXCODE];    /* satellite code biases of each station */
    int vscb[MAXSAT][MAXCODE];         /* valid flag(0:invlalid,1:valid) */
    int vspb[MAXSAT][MAXCODE];         /* valid flag(0:invlalid,1:valid) */
    int vrsyscb[MAXBSNXSYS][MAXCODE];  /* valid flag(0:invlalid,1:valid) */
    int vrsatcb[MAXSAT][MAXCODE];      /* valid flag(0:invlalid,1:valid) */
} osb_t;

typedef struct {        /* site type */
    char   name[8];     /* site name */
    double ecef[3];     /* site position(ecef-x,y,z) [m] */
} spos_t;

typedef struct {        /* ionospheric data type */
    gtime_t time;       /* time (GPST) */
    unsigned char sat;  /* satellite number */
    double ion;         /* slant ionos delay (m) */
    double std;         /* std-dev (m) */
    double azel[2];     /* estimate azimuth/elevation (rad) */
    unsigned char qflag; /* Q flag */
    double ipp[3];      /* ionospheric pierce point(x,y,z) [m] */
} ion_t;

typedef struct {        /* trop data type */
    gtime_t time;       /* time (GPST) */
    double trp[3];      /* zenith tropos delay/gradient (m) */
    double std[3];      /* std-dev (m) */
    unsigned char qflag; /* Q flag */
} trp_t;

typedef struct {        /* ppp corrections type */
    spos_t site;        /* site data */
    ion_t iond[MAXSAT]; /* ionospheric data */
}siteion_t;

typedef struct {        /* station stat corrections */
    spos_t site;        /* site data */
    trp_t trpd;         /* tropospheric data */
}sitetrp_t;

typedef struct {        /* station stat corrections */
    gtime_t time[MAXSITES]; /* time (GPST) */
    int  nsi,nst;       /* number of stat corrections stations */
    siteion_t sion[MAXSITES]; /* ionospheric corrections sites */
    sitetrp_t strp[MAXSITES]; /* tropospheric corrections sites */
}stat_t;

typedef struct {        /* single station stat corrections */
    gtime_t time;       /* time (GPST) */
    siteion_t sion;     /* ionospheric corrections site */
    sitetrp_t strp;     /* tropospheric corrections site */
}sstat_t;

typedef struct {        /* local combination options */
    gtime_t ts;         /* start time */
    gtime_t te;         /* stop time */
    int ni;             /* number of iteration */
    double maxres[MAX_REJ_SITES]; /* max residual */
    double eratio;      /* residual error ratio */
    char *gridfile;     /* input grid setting file name */ 
    char *stafile;      /* input station setting file name */ 
    char *istfile;      /* input stream setting file name (only lclcmbrt) */ 
    char *infile;       /* input file name */
    char *outpath;      /* output file path */ 
    char *outlog;       /* output log file name */
    char *clogfile;     /* output command log file name */
} lclopt_t;

typedef struct {        /* block info type */
    int btype;          /* block type (0:grid,1:station) */
    int bn;             /* block number */
    double bs;          /* block size */
    int gpitch;         /* grid pitch (0:16grid,1:64grid) */
    unsigned int mask[2]; /* grid mask */
    int n;              /* number of valid grid/station */
    double bpos[3];     /* block base position {lat,lon,hgt} (rad,m) */
    double grid[MAXTRPSTA][3]; /* grid/station position {lat,lon,hgt} (rad,m) */
    int gp[MAXTRPSTA];  /* valid grid point index */
} blkinf_t;

typedef struct {        /* block type */
    double idist,tdist; /* distance (km) for interpolation threshold */
    int extp;           /* flag enable extrapolation check on planer fitting mode */
    int interp;         /* interpolation mode (0:planer fitting,1:weighted average of distance)*/
    int inum,tnum;      /* number of ionospheric/tropospheric correction data */
    blkinf_t tblkinf[MAXBLK]; /* tropospheric block info */
    blkinf_t iblkinf[MAXBLK]; /* ionospheric block info */
    sitetrp_t tstat[MAXBLK][MAXTRPSTA]; /* tropospheric block correction data */
    siteion_t istat[MAXBLK][MAXTRPSTA]; /* ionospheric block correction data */
    int outin,outtn;    /* ionospheric/trop output control */
} lclblock_t;

typedef struct {        /* navigation data type */
    int n,nmax;         /* number of broadcast ephemeris */
    int ng,ngmax;       /* number of glonass ephemeris */
    int ns,nsmax;       /* number of sbas ephemeris */
    int ne,nemax;       /* number of precise ephemeris */
    int nc,ncmax;       /* number of precise clock */
    int na,namax;       /* number of almanac data */
    int nt,ntmax;       /* number of tec grid data */
    eph_t *eph;         /* GPS/QZS/GAL/BDS/IRN ephemeris */
    geph_t *geph;       /* GLONASS ephemeris */
    seph_t *seph;       /* SBAS ephemeris */
    peph_t *peph;       /* precise ephemeris */
    pclk_t *pclk;       /* precise clock */
    alm_t *alm;         /* almanac data */
    tec_t *tec;         /* tec grid data */
    erp_t  erp;         /* earth rotation parameters */
    double utc_gps[8];  /* GPS delta-UTC parameters {A0,A1,Tot,WNt,dt_LS,WN_LSF,DN,dt_LSF} */
    double utc_glo[8];  /* GLONASS UTC time parameters {tau_C,tau_GPS} */
    double utc_gal[8];  /* Galileo UTC parameters */
    double utc_qzs[8];  /* QZS UTC parameters */
    double utc_cmp[8];  /* BeiDou UTC parameters */
    double utc_irn[9];  /* IRNSS UTC parameters {A0,A1,Tot,...,dt_LSF,A2} */
    double utc_sbs[4];  /* SBAS UTC parameters */
    double ion_gps[8];  /* GPS iono model parameters {a0,a1,a2,a3,b0,b1,b2,b3} */
    double ion_gal[4];  /* Galileo iono model parameters {ai0,ai1,ai2,0} */
    double ion_qzs[8];  /* QZSS iono model parameters {a0,a1,a2,a3,b0,b1,b2,b3} */
    double ion_cmp[8];  /* BeiDou iono model parameters {a0,a1,a2,a3,b0,b1,b2,b3} */
    double ion_irn[8];  /* IRNSS iono model parameters {a0,a1,a2,a3,b0,b1,b2,b3} */
    int glo_fcn[32];    /* GLONASS FCN + 8 */
    double cbias[MAXSAT][3]; /* satellite DCB (0:P1-P2,1:P1-C1,2:P2-C2) (m) */
    double rbias[MAXRCV][2][3]; /* receiver DCB (0:P1-P2,1:P1-C1,2:P2-C2) (m) */
    double wlbias[MAXSAT];   /* wide-lane bias (cycle) */
    pcv_t pcvs[MAXSAT]; /* satellite antenna pcv */
    sbssat_t sbssat;    /* SBAS satellite corrections */
    sbsion_t sbsion[MAXBAND+1]; /* SBAS ionosphere corrections */
    dgps_t dgps[MAXSAT]; /* DGPS corrections */
    ssr_t ssr[MAXSAT];  /* SSR corrections */
    stat_t stat;        /* stat corrections */
    osb_t osb;          /* Observable-specific Signal Bias data */
    char biapath [MAXSTRPATH]; /* bias sinex file path */
    char fcbpath [MAXSTRPATH]; /* fcb file path */
    char pr_biapath [MAXSTRPATH]; /* previous bias sinex file path */
    char pr_fcbpath [MAXSTRPATH]; /* previous fcb file path */
} nav_t;

typedef struct {        /* station parameter type */
    char name   [MAXANT]; /* marker name */
    char marker [MAXANT]; /* marker number */
    char antdes [MAXANT]; /* antenna descriptor */
    char antsno [MAXANT]; /* antenna serial number */
    char rectype[MAXANT]; /* receiver type descriptor */
    char recver [MAXANT]; /* receiver firmware version */
    char recsno [MAXANT]; /* receiver serial number */
    int antsetup;       /* antenna setup id */
    int itrf;           /* ITRF realization year */
    int deltype;        /* antenna delta type (0:enu,1:xyz) */
    double pos[3];      /* station position (ecef) (m) */
    double del[3];      /* antenna position delta (e/n/u or x/y/z) (m) */
    double hgt;         /* antenna height (m) */
    int glo_cp_align;   /* GLONASS code-phase alignment (0:no,1:yes) */
    double glo_cp_bias[4]; /* GLONASS code-phase biases {1C,1P,2C,2P} (m) */
} sta_t;

typedef struct {        /* solution type */
    gtime_t time;       /* time (GPST) */
    double rr[6];       /* position/velocity (m|m/s) */
                        /* {x,y,z,vx,vy,vz} or {e,n,u,ve,vn,vu} */
    float  qr[6];       /* position variance/covariance (m^2) */
                        /* {c_xx,c_yy,c_zz,c_xy,c_yz,c_zx} or */
                        /* {c_ee,c_nn,c_uu,c_en,c_nu,c_ue} */
    float  qv[6];       /* velocity variance/covariance (m^2/s^2) */
    double dtr[6];      /* receiver clock bias to time systems (s) */
    uint8_t type;       /* type (0:xyz-ecef,1:enu-baseline) */
    uint8_t stat;       /* solution status (SOLQ_???) */
    uint8_t ns;         /* number of valid satellites */
    float age;          /* age of differential (s) */
    float ratio;        /* AR ratio factor for valiation */
    float thres;        /* AR ratio threshold for valiation */
} sol_t;

typedef struct {        /* solution buffer type */
    int n,nmax;         /* number of solution/max number of buffer */
    int cyclic;         /* cyclic buffer flag */
    int start,end;      /* start/end index */
    gtime_t time;       /* current solution time */
    sol_t *data;        /* solution data */
    double rb[3];       /* reference position {x,y,z} (ecef) (m) */
    uint8_t buff[MAXSOLMSG+1]; /* message buffer */
    int nb;             /* number of byte in message buffer */
} solbuf_t;

typedef struct {        /* solution status type */
    gtime_t time;       /* time (GPST) */
    uint8_t sat;        /* satellite number */
    uint8_t frq;        /* frequency (1:L1,2:L2,...) */
    float az,el;        /* azimuth/elevation angle (rad) */
    float resp;         /* pseudorange residual (m) */
    float resc;         /* carrier-phase residual (m) */
    uint8_t flag;       /* flags: (vsat<<5)+(slip<<3)+fix */
    uint16_t snr;       /* signal strength (*SNR_UNIT dBHz) */
    uint16_t lock;      /* lock counter */
    uint16_t outc;      /* outage counter */
    uint16_t slipc;     /* slip counter */
    uint16_t rejc;      /* reject counter */
} solstat_t;

typedef struct {        /* solution status buffer type */
    int n,nmax;         /* number of solution/max number of buffer */
    solstat_t *data;    /* solution status data */
} solstatbuf_t;

typedef struct {        /* RTCM control struct type */
    int staid;          /* station id */
    int stah;           /* station health */
    int seqno;          /* sequence number for rtcm 2 or iods msm */
    int outtype;        /* output message type */
    gtime_t time;       /* message time */
    gtime_t time_s;     /* message start time */
    obs_t obs;          /* observation data (uncorrected) */
    nav_t nav;          /* satellite ephemerides */
    sta_t sta;          /* station parameters */
    dgps_t *dgps;       /* output of dgps corrections */
    ssr_t ssr[MAXSAT];  /* output of ssr corrections */
    char msg[128];      /* special message */
    char msgtype[256];  /* last message type */
    char msmtype[7][128]; /* msm signal types */
    int obsflag;        /* obs data complete flag (1:ok,0:not complete) */
    int ephsat;         /* input ephemeris satellite number */
    int ephset;         /* input ephemeris set (0-1) */
    double cp[MAXSAT][NFREQ+NEXOBS]; /* carrier-phase measurement */
    uint16_t lock[MAXSAT][NFREQ+NEXOBS]; /* lock time */
    uint16_t loss[MAXSAT][NFREQ+NEXOBS]; /* loss of lock count */
    gtime_t lltime[MAXSAT][NFREQ+NEXOBS]; /* last lock time */
    int nbyte;          /* number of bytes in message buffer */ 
    int nbit;           /* number of bits in word buffer */ 
    int len;            /* message length (bytes) */
    uint8_t buff[1200]; /* message buffer */
    uint32_t word;      /* word buffer for rtcm 2 */
    uint32_t nmsg2[100]; /* message count of RTCM 2 (1-99:1-99,0:other) */
    uint32_t nmsg3[400]; /* message count of RTCM 3 (1-299:1001-1299,300-329:4070-4099,0:ohter) */
    char opt[256];      /* RTCM dependent options */
    lclblock_t lclblk;  /* output of iono/trop corrections */
} rtcm_t;

typedef struct {        /* RINEX control struct type */
    gtime_t time;       /* message time */
    int    ver;         /* RINEX version * 100 */
    char   type;        /* RINEX file type ('O','N',...) */
    int    sys;         /* navigation system */
    int    tsys;        /* time system */
    char   tobs[8][MAXOBSTYPE][4]; /* rinex obs types */
    obs_t  obs;         /* observation data */
    nav_t  nav;         /* navigation data */
    sta_t  sta;         /* station info */
    int    ephsat;      /* input ephemeris satellite number */
    int    ephset;      /* input ephemeris set (0-1) */
    char   opt[256];    /* rinex dependent options */
} rnxctr_t;

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

typedef struct {        /* SNR mask type */
    int ena[2];         /* enable flag {rover,base} */
    double mask[NFREQ][9]; /* mask (dBHz) at 5,10,...85 deg */
} snrmask_t;

typedef struct {        /* processing options type */
    int mode;           /* positioning mode (PMODE_???) */
    int soltype;        /* solution type (0:forward,1:backward,2:combined) */
    int nf;             /* number of frequencies (1:L1,2:L1+L2,3:L1+L2+L5) */
    int navsys;         /* navigation system */
    double elmin;       /* elevation mask angle (rad) */
    snrmask_t snrmask;  /* SNR mask */
    int sateph;         /* satellite ephemeris/clock (EPHOPT_???) */
    int modear;         /* AR mode (0:off,1:continuous,2:instantaneous,3:fix and hold,4:ppp-ar) */
    int glomodear;      /* GLONASS AR mode (0:off,1:on,2:auto cal,3:ext cal) */
    int bdsmodear;      /* BeiDou AR mode (0:off,1:on) */
    int arsys;          /* navigation system for PPP-AR */
    int maxout;         /* obs outage count to reset bias */
    int minlock;        /* min lock count to fix ambiguity */
    int minfix;         /* min fix count to hold ambiguity */
    int armaxiter;      /* max iteration to resolve ambiguity */
    int ionoopt;        /* ionosphere option (IONOOPT_???) */
    int tropopt;        /* troposphere option (TROPOPT_???) */
    int dynamics;       /* dynamics model (0:none,1:velociy,2:accel) */
    int tidecorr;       /* earth tide correction (0:off,1:solid,2:solid+otl+pole) */
    int niter;          /* number of filter iteration */
    int codesmooth;     /* code smoothing window size (0:none) */
    int intpref;        /* interpolate reference obs (for post mission) */
    int sbascorr;       /* SBAS correction options */
    int sbassatsel;     /* SBAS satellite selection (0:all) */
    int rovpos;         /* rover position for fixed mode */
    int refpos;         /* base position for relative mode */
                        /* (0:pos in prcopt,  1:average of single pos, */
                        /*  2:read from file, 3:rinex header, 4:rtcm pos) */
    double eratio[NFREQ]; /* code/phase error ratio */
    double err[5];      /* measurement error factor */
                        /* [0]:reserved */
                        /* [1-3]:error factor a/b/c of phase (m) */
                        /* [4]:doppler frequency (hz) */
    double std[3];      /* initial-state std [0]bias,[1]iono [2]trop */
    double prn[6];      /* process-noise std [0]bias,[1]iono [2]trop [3]acch [4]accv [5] pos */
    double sclkstab;    /* satellite clock stability (sec/sec) */
    double thresar[8];  /* AR validation threshold */
    double elmaskar;    /* elevation mask of AR for rising satellite (deg) */
    double elmaskhold;  /* elevation mask to hold ambiguity (deg) */
    double thresslip;   /* slip threshold of geometry-free phase (m) */
    double maxtdiff;    /* max difference of time (sec) */
    double maxinno;     /* reject threshold of innovation (m) */
    double maxgdop;     /* reject threshold of gdop */
    double baseline[2]; /* baseline length constraint {const,sigma} (m) */
    double ru[3];       /* rover position for fixed mode {x,y,z} (ecef) (m) */
    double rb[3];       /* base position for relative mode {x,y,z} (ecef) (m) */
    char anttype[2][MAXANT]; /* antenna types {rover,base} */
    double antdel[2][3]; /* antenna delta {{rov_e,rov_n,rov_u},{ref_e,ref_n,ref_u}} */
    pcv_t pcvr[2];      /* receiver antenna parameters {rov,base} */
    uint8_t exsats[MAXSAT]; /* excluded satellites (1:excluded,2:included) */
    int  ign_chierr;    /* ignore chi-square error mode (0:off,1:on) */
    int  bds2bias;      /* estimate of bias for each BeiDou2 satellite type (0:off,1:on) */
    int  sattmode;      /* 0:nominal */
    int  pppsatcb;      /* satellite code bias selection (0:auto,1:ssr,2:bia,3:dcb) */
    int  pppsatpb;      /* satellite phase bias selection (0:auto,1:ssr,3:fcb) */
    int  unbias   ;     /* (0:use uncorrected code bias,1:do not use uncorrected code bias) */
    double maxbiasdt;   /* bias sinex  and fcb  code/phase bias timeout(s) */
    int  maxaveep;      /* max averaging epoches */
    int  initrst;       /* initialize by restart */
    int  outsingle;     /* output single by dgps/float/fix/ppp outage */
    char rnxopt[2][256]; /* rinex options {rover,base} */
    int  posopt[6];     /* positioning options */
    int  syncsol;       /* solution sync mode (0:off,1:on) */
    double odisp[2][6*11]; /* ocean tide loading parameters {rov,base} */
    int  freqopt;       /* disable L2-AR */
    int16_t elmaskopt[360]; /* elevation mask pattern */
    char pppopt[256];   /* ppp option */
    char rtcmopt[256];  /* rtcm options */
    int  pppsig[6];     /* signal selection [0]GPS IIR-M,[1]GPS IIF,[2]GPS IIIA,[3]QZS-1/2,[4]BDS-3,[5]GAL */
    char staname[32];   /* station name */
} prcopt_t;

typedef struct {        /* solution options type */
    int posf;           /* solution format (SOLF_???) */
    int times;          /* time system (TIMES_???) */
    int timef;          /* time format (0:sssss.s,1:yyyy/mm/dd hh:mm:ss.s) */
    int timeu;          /* time digits under decimal point */
    int degf;           /* latitude/longitude format (0:ddd.ddd,1:ddd mm ss) */
    int outhead;        /* output header (0:no,1:yes) */
    int outopt;         /* output processing options (0:no,1:yes) */
    int outvel;         /* output velocity options (0:no,1:yes) */
    int datum;          /* datum (0:WGS84,1:Tokyo) */
    int height;         /* height (0:ellipsoidal,1:geodetic) */
    int geoid;          /* geoid model (0:EGM96,1:JGD2000) */
    int solstatic;      /* solution of static mode (0:all,1:single) */
    int sstat;          /* solution statistics level (0:off,1:states,2:residuals) */
    int trace;          /* debug trace level (0:off,1-5:debug) */
    double nmeaintv[2]; /* nmea output interval (s) (<0:no,0:all) */
                        /* nmeaintv[0]:gprmc,gpgga,nmeaintv[1]:gpgsv */
    char sep[64];       /* field separator */
    char prog[64];      /* program name */
    double maxsolstd;   /* max std-dev for solution output (m) (0:all) */
} solopt_t;

typedef struct {        /* file options type */
    char satantp[MAXSTRPATH]; /* satellite antenna parameters file */
    char rcvantp[MAXSTRPATH]; /* receiver antenna parameters file */
    char stapos [MAXSTRPATH]; /* station positions file */
    char geoid  [MAXSTRPATH]; /* external geoid data file */
    char iono   [MAXSTRPATH]; /* ionosphere data file */
    char dcb    [MAXSTRPATH]; /* dcb data file */
    char eop    [MAXSTRPATH]; /* eop data file */
    char blq    [MAXSTRPATH]; /* ocean tide loading blq file */
    char tempdir[MAXSTRPATH]; /* ftp/http temporaly directory */
    char geexe  [MAXSTRPATH]; /* google earth exec file */
    char elmask [MAXSTRPATH]; /* elevation mask file */
    char solstat[MAXSTRPATH]; /* solution statistics file */
    char trace  [MAXSTRPATH]; /* debug trace file */
    char bia    [MAXSTRPATH]; /* bias sinex data file */
    char fcb    [MAXSTRPATH]; /* fcb data file */
} filopt_t;

typedef struct {        /* RINEX options type */
    gtime_t ts,te;      /* time start/end */
    double tint;        /* time interval (s) */
    double ttol;        /* time tolerance (s) */
    double tunit;       /* time unit for multiple-session (s) */
    int rnxver;         /* RINEX version (x100) */
    int navsys;         /* navigation system */
    int obstype;        /* observation type */
    int freqtype;       /* frequency type */
    char mask[7][MAXCODE]; /* code mask {GPS,GLO,GAL,QZS,SBS,CMP,IRN} */
    char staid [32];    /* station id for RINEX file name */
    char prog  [32];    /* program */
    char runby [32];    /* run-by */
    char marker[64];    /* marker name */
    char markerno[32];  /* marker number */
    char markertype[32]; /* marker type (ver.3) */
    char name[2][32];   /* observer/agency */
    char rec [3][32];   /* receiver #/type/vers */
    char ant [3][32];   /* antenna #/type */
    double apppos[3];   /* approx position x/y/z */
    double antdel[3];   /* antenna delta h/e/n */
    double glo_cp_bias[4]; /* GLONASS code-phase biases (m) */
    char comment[MAXCOMMENT][64]; /* comments */
    char rcvopt[256];   /* receiver dependent options */
    uint8_t exsats[MAXSAT]; /* excluded satellites */
    int glofcn[32];     /* glonass fcn+8 */
    int outiono;        /* output iono correction */
    int outtime;        /* output time system correction */
    int outleaps;       /* output leap seconds */
    int autopos;        /* auto approx position */
    int phshift;        /* phase shift correction */
    int halfcyc;        /* half cycle correction */
    int sep_nav;        /* separated nav files */
    gtime_t tstart;     /* first obs time */
    gtime_t tend;       /* last obs time */
    gtime_t trtcm;      /* approx log start time for rtcm */
    char tobs[7][MAXOBSTYPE][4]; /* obs types {GPS,GLO,GAL,QZS,SBS,CMP,IRN} */
    double shift[7][MAXOBSTYPE]; /* phase shift (cyc) {GPS,GLO,GAL,QZS,SBS,CMP,IRN} */
    int nobs[7];        /* number of obs types {GPS,GLO,GAL,QZS,SBS,CMP,IRN} */
} rnxopt_t;

typedef struct {        /* satellite status type */
    uint8_t sys;        /* navigation system */
    uint8_t vs;         /* valid satellite flag single */
    double azel[2];     /* azimuth/elevation angles {az,el} (rad) */
    double resp[NFREQ]; /* residuals of pseudorange (m) */
    double resc[NFREQ]; /* residuals of carrier-phase (m) */
    uint8_t vsat[NFREQ]; /* valid satellite flag */
    uint16_t snr[NFREQ]; /* signal strength (*SNR_UNIT dBHz) */
    uint8_t fix [NFREQ]; /* ambiguity fix flag (1:fix,2:float,3:hold) */
    uint8_t slip[NFREQ]; /* cycle-slip flag */
    uint8_t half[NFREQ]; /* half-cycle valid flag */
    int lock [NFREQ];   /* lock counter of phase */
    uint32_t outc [NFREQ]; /* obs outage counter of phase */
    uint32_t slipc[NFREQ]; /* cycle-slip counter */
    uint32_t rejc [NFREQ]; /* reject counter */
    double gf[NFREQ-1]; /* geometry-free phase (m) */
    double mw[NFREQ-1]; /* MW-LC (m) */
    double phw;         /* phase windup (cycle) */
    gtime_t pt[2][NFREQ]; /* previous carrier-phase time */
    double ph[2][NFREQ]; /* previous carrier-phase observable (cycle) */
} ssat_t;

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

typedef void fatalfunc_t(const char *); /* fatal callback function type */

/* global variables ----------------------------------------------------------*/
extern const double chisqr[];        /* Chi-sqr(n) table (alpha=0.001) */
extern const prcopt_t prcopt_default; /* default positioning options */
extern const solopt_t solopt_default; /* default solution output options */
extern const sbsigpband_t igpband1[9][8]; /* SBAS IGP band 0-8 */
extern const sbsigpband_t igpband2[2][5]; /* SBAS IGP band 9-10 */
extern const char *formatstrs[];     /* stream format strings */
extern opt_t sysopts[];              /* system options table */

/* satellites, systems, codes functions --------------------------------------*/
int satno(int sys, int prn);
int satsys(int sat, int *prn);
int satid2no(const char *id);
void satno2id(int sat, char *id);
uint8_t obs2code(const char *obs);
char *code2obs(uint8_t code);
double code2freq(int sys, uint8_t code, int fcn);
double sat2freq(int sat, uint8_t code, const nav_t *nav);
int code2idx(int sys, uint8_t code);
int satexclude(int sat, double var, int svh, const prcopt_t *opt);
int testsnr(int base, int freq, double el, double snr, const snrmask_t *mask);
int testelmask(const double *azel, const int16_t *elmask);
void setcodepri(int sys, int idx, const char *pri);
int getcodepri(int sys, uint8_t code, const char *opt);

/* matrix and vector functions -----------------------------------------------*/
double *mat(int n, int m);
int *imat(int n, int m);
double *zeros(int n, int m);
double *eye(int n);
double dot(const double *a, const double *b, int n);
double norm(const double *a, int n);
void cross3(const double *a, const double *b, double *c);
int normv3(const double *a, double *b);
void matcpy(double *A, const double *B, int n, int m);
void matmul(const char *tr, int n, int k, int m, double alpha, const double *A,
            const double *B, double beta, double *C);
int matinv(double *A, int n);
int solve(const char *tr, const double *A, const double *Y, int n, int m,
          double *X);
int lsq(const double *A, const double *y, int n, int m, double *x, double *Q);
int filter(double *x, double *P, const double *H, const double *v,
           const double *R, int n, int m);
int smoother(const double *xf, const double *Qf, const double *xb,
             const double *Qb, int n, double *xs, double *Qs);
void matprint (const double *A, int n, int m, int p, int q);
void matfprint(const double *A, int n, int m, int p, int q, FILE *fp);

void add_fatal(fatalfunc_t *func);

/* time and string functions -------------------------------------------------*/
double  str2num(const char *s, int i, int n);
int str2time(const char *s, int i, int n, gtime_t *t);
void time2str(gtime_t t, char *str, int n);
gtime_t epoch2time(const double *ep);
void time2epoch(gtime_t t, double *ep);
gtime_t gpst2time(int week, double sec);
double time2gpst(gtime_t t, int *week);
gtime_t gst2time(int week, double sec);
double time2gst(gtime_t t, int *week);
gtime_t bdt2time(int week, double sec);
double time2bdt(gtime_t t, int *week);
char *time_str(gtime_t t, int n);

gtime_t timeadd(gtime_t t, double sec);
double timediff(gtime_t t1, gtime_t t2);
gtime_t gpst2utc(gtime_t t);
gtime_t utc2gpst(gtime_t t);
gtime_t gpst2bdt(gtime_t t);
gtime_t bdt2gpst(gtime_t t);
gtime_t timeget(void);
void timeset(gtime_t t);
void timereset(void);
double time2doy(gtime_t t);
double utc2gmst(gtime_t t, double ut1_utc);
int read_leaps(const char *file);

int adjgpsweek(int week);
uint32_t tickget(void);
void sleepms(int ms);

int reppath(const char *path, char *rpath, gtime_t time, const char *rov,
            const char *base);
int reppaths(const char *path, char *rpaths[], int nmax, gtime_t ts,
             gtime_t te, const char *rov, const char *base);

/* coordinates transformation ------------------------------------------------*/
void ecef2pos(const double *r, double *pos);
void pos2ecef(const double *pos, double *r);
void ecef2enu(const double *pos, const double *r, double *e);
void enu2ecef(const double *pos, const double *e, double *r);
void covenu(const double *pos, const double *P, double *Q);
void covecef(const double *pos, const double *Q, double *P);
void xyz2enu(const double *pos, double *E);
void eci2ecef(gtime_t tutc, const double *erpv, double *U, double *gmst);
void deg2dms(double deg, double *dms, int ndec);
double dms2deg(const double *dms);

/* input and output functions ------------------------------------------------*/
void readpos(const char *file, const char *rcv, double *pos);
int sortobs(obs_t *obs);
void signal_replace(obsd_t *obs, int idx, char f, char *c);
void uniqnav(nav_t *nav);
int screent(gtime_t time, gtime_t ts, gtime_t te, double tint);
int readnav(const char *file, nav_t *nav);
int savenav(const char *file, const nav_t *nav);
void freeobs(obs_t *obs);
void freenav(nav_t *nav, int opt);
int readblq(const char *file, const char *sta, double *odisp);
int readerp(const char *file, erp_t *erp);
int geterp(const erp_t *erp, gtime_t time, double *val);
int readelmask(const char *file, int16_t *elmask);

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

/* platform dependent functions ----------------------------------------------*/
int execcmd(const char *cmd);
int expath(const char *path, char *paths[], int nmax);
void createdir(const char *path);

/* positioning models --------------------------------------------------------*/
double satazel(const double *pos, const double *e, double *azel);
double geodist(const double *rs, const double *rr, double *e);
void dops(int ns, const double *azel, double elmin, double *dop);

/* atmosphere models ---------------------------------------------------------*/
double ionmodel(gtime_t t, const double *ion, const double *pos,
                const double *azel);
double ionmapf(const double *pos, const double *azel);
double ionppp(const double *pos, const double *azel, double re, double hion,
              double *pppos);
double tropmodel(gtime_t time, const double *pos, const double *azel,
                 double humi);
double tropmapf(gtime_t time, const double *pos, const double *azel,
                double *mapfw);
int iontec(gtime_t time, const nav_t *nav, const double *pos,
           const double *azel, int opt, double *delay, double *var);
void readtec(const char *file, nav_t *nav, int opt);
int ionocorr(gtime_t time, const nav_t *nav, int sat, const double *pos,
             const double *azel, int ionoopt, double *ion, double *var);
int tropcorr(gtime_t time, const nav_t *nav, const double *pos,
             const double *azel, int tropopt, double *trp, double *var);

/* antenna models ------------------------------------------------------------*/
int readpcv(const char *file, pcvs_t *pcvs);
pcv_t *searchpcv(int sat, const char *type, gtime_t time, const pcvs_t *pcvs);
void antmodel(const pcv_t *pcv, const double *del, const double *azel,
              int opt, double *dant);
void antmodel_s(const pcv_t *pcv, double nadir, double *dant);

/* earth tide models ---------------------------------------------------------*/
void sunmoonpos(gtime_t tutc, const double *erpv, double *rsun, double *rmoon,
                double *gmst);
void tidedisp(gtime_t tutc, const double *rr, int opt, const erp_t *erp,
              const double *odisp, double *dr);

/* geiod models --------------------------------------------------------------*/
int opengeoid(int model, const char *file);
void closegeoid(void);
double geoidh(const double *pos);

/* datum transformation ------------------------------------------------------*/
int loaddatump(const char *file);
int tokyo2jgd(double *pos);
int jgd2tokyo(double *pos);

/* RINEX functions -----------------------------------------------------------*/
int readrnx(const char *file, int rcv, const char *opt, obs_t *obs, nav_t *nav,
            sta_t *sta);
int readrnxt(const char *file, int rcv, gtime_t ts, gtime_t te, double tint,
             const char *opt, obs_t *obs, nav_t *nav, sta_t *sta);
int readrnxc(const char *file, nav_t *nav);
int outrnxobsh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);
int outrnxobsb(FILE *fp, const rnxopt_t *opt, const obsd_t *obs, int n,
               int epflag);
int outrnxnavh (FILE *fp, const rnxopt_t *opt, const nav_t *nav);
int outrnxgnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);
int outrnxhnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);
int outrnxlnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);
int outrnxqnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);
int outrnxcnavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);
int outrnxinavh(FILE *fp, const rnxopt_t *opt, const nav_t *nav);
int outrnxnavb(FILE *fp, const rnxopt_t *opt, const eph_t *eph);
int outrnxgnavb(FILE *fp, const rnxopt_t *opt, const geph_t *geph);
int outrnxhnavb(FILE *fp, const rnxopt_t *opt, const seph_t *seph);
int rtk_uncompress(const char *file, char *uncfile);
int convrnx(int format, rnxopt_t *opt, const char *file, char **ofile);
int init_rnxctr(rnxctr_t *rnx);
void free_rnxctr(rnxctr_t *rnx);
int open_rnxctr(rnxctr_t *rnx, FILE *fp);
int input_rnxctr(rnxctr_t *rnx, FILE *fp);

/* ephemeris and clock functions ---------------------------------------------*/
double eph2clk (gtime_t time, const eph_t  *eph);
double geph2clk(gtime_t time, const geph_t *geph);
double seph2clk(gtime_t time, const seph_t *seph);
void eph2pos(gtime_t time, const eph_t  *eph,  double *rs, double *dts,
             double *var);
void geph2pos(gtime_t time, const geph_t *geph, double *rs, double *dts,
              double *var);
void seph2pos(gtime_t time, const seph_t *seph, double *rs, double *dts,
              double *var);
int peph2pos(gtime_t time, int sat, const nav_t *nav, int opt, double *rs,
             double *dts, double *var);
void satantoff(gtime_t time, const double *rs, int sat, const nav_t *nav,
               double *dant);
int  satpos(gtime_t time, gtime_t teph, int sat, int ephopt, const nav_t *nav,
            double *rs, double *dts, double *var, int *svh);
void satposs(gtime_t time, const obsd_t *obs, int n, const nav_t *nav,
             int sateph, double *rs, double *dts, double *var, int *svh);
void setseleph(int sys, int sel);
int getseleph(int sys);
void readsp3(const char *file, nav_t *nav, int opt);
int readsap(const char *file, gtime_t time, nav_t *nav);
int readdcb(const char *file, nav_t *nav, const sta_t *sta);
void alm2pos(gtime_t time, const alm_t *alm, double *rs, double *dts);

/* NORAD TLE (two line element) functions ------------------------------------*/
int tle_read(const char *file, tle_t *tle);
int tle_name_read(const char *file, tle_t *tle);
int tle_pos(gtime_t time, const char *name, const char *satno,
            const char *desig, const tle_t *tle, const erp_t *erp, double *rs);

/* receiver raw data functions -----------------------------------------------*/
uint32_t getbitu(const uint8_t *buff, int pos, int len);
int32_t  getbits(const uint8_t *buff, int pos, int len);
void setbitu(uint8_t *buff, int pos, int len, uint32_t data);
void setbits(uint8_t *buff, int pos, int len, int32_t  data);
uint32_t rtk_crc32(const uint8_t *buff, int len);
uint32_t rtk_crc24q(const uint8_t *buff, int len);
uint16_t rtk_crc16(const uint8_t *buff, int len);
int decode_word (uint32_t word, uint8_t *data);
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

/* RTCM functions ------------------------------------------------------------*/
int init_rtcm(rtcm_t *rtcm);
void free_rtcm(rtcm_t *rtcm);
int input_rtcm2(rtcm_t *rtcm, uint8_t data);
int input_rtcm3(rtcm_t *rtcm, uint8_t data);
int input_rtcm2f(rtcm_t *rtcm, FILE *fp);
int input_rtcm3f(rtcm_t *rtcm, FILE *fp);
int gen_rtcm2(rtcm_t *rtcm, int type, int sync);
int gen_rtcm3(rtcm_t *rtcm, int type, int subtype, int sync);

/* solution functions --------------------------------------------------------*/
void initsolbuf(solbuf_t *solbuf, int cyclic, int nmax);
void freesolbuf(solbuf_t *solbuf);
void freesolstatbuf(solstatbuf_t *solstatbuf);
sol_t *getsol(solbuf_t *solbuf, int index);
int addsol(solbuf_t *solbuf, const sol_t *sol);
int readsol (char *files[], int nfile, solbuf_t *sol);
int readsolt(char *files[], int nfile, gtime_t ts, gtime_t te, double tint,
             int qflag, solbuf_t *sol);
int readsolstat(char *files[], int nfile, solstatbuf_t *statbuf);
int readsolstatt(char *files[], int nfile, gtime_t ts, gtime_t te, double tint,
                 solstatbuf_t *statbuf);
int inputsol(uint8_t data, gtime_t ts, gtime_t te, double tint, int qflag,
             const solopt_t *opt, solbuf_t *solbuf);

int outprcopts(uint8_t *buff, const prcopt_t *opt);
int outsolheads(uint8_t *buff, const solopt_t *opt);
int outsols(uint8_t *buff, const sol_t *sol, const double *rb,
            const solopt_t *opt);
int outsolexs(uint8_t *buff, const sol_t *sol, const ssat_t *ssat,
              const solopt_t *opt);
void outprcopt(FILE *fp, const prcopt_t *opt);
void outsolhead(FILE *fp, const solopt_t *opt);
void outsol(FILE *fp, const sol_t *sol, const double *rb, const solopt_t *opt);
void outsolex(FILE *fp, const sol_t *sol, const ssat_t *ssat,
              const solopt_t *opt);
int outnmea_rmc(uint8_t *buff, const sol_t *sol);
int outnmea_gga(uint8_t *buff, const sol_t *sol);
int outnmea_gsa(uint8_t *buff, const sol_t *sol, const ssat_t *ssat);
int outnmea_gsv(uint8_t *buff, const sol_t *sol, const ssat_t *ssat);

/* Google Earth KML converter ------------------------------------------------*/
int convkml(const char *infile, const char *outfile, gtime_t ts, gtime_t te,
            double tint, int qflg, double *offset, int tcolor, int pcolor,
            int outalt, int outtime);

/* GPX converter -------------------------------------------------------------*/
int convgpx(const char *infile, const char *outfile, gtime_t ts, gtime_t te,
            double tint, int qflg, double *offset, int outtrk, int outpnt,
            int outalt, int outtime);

/* SBAS functions ------------------------------------------------------------*/
int sbsreadmsg (const char *file, int sel, sbs_t *sbs);
int sbsreadmsgt(const char *file, int sel, gtime_t ts, gtime_t te, sbs_t *sbs);
void sbsoutmsg(FILE *fp, sbsmsg_t *sbsmsg);
int sbsdecodemsg(gtime_t time, int prn, const uint32_t *words,
                  sbsmsg_t *sbsmsg);
int sbsupdatecorr(const sbsmsg_t *msg, nav_t *nav);
int sbssatcorr(gtime_t time, int sat, const nav_t *nav, double *rs, double *dts,
               double *var);
int sbsioncorr(gtime_t time, const nav_t *nav, const double *pos,
               const double *azel, double *delay, double *var);
double sbstropcorr(gtime_t time, const double *pos, const double *azel,
                   double *var);

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
