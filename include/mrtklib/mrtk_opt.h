/*------------------------------------------------------------------------------
 * mrtk_opt.h : processing and solution option type definitions
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
 * @file mrtk_opt.h
 * @brief MRTKLIB Options Module — Processing and solution option types.
 *
 * This header provides the processing option (prcopt_t), solution output
 * option (solopt_t), and file option (filopt_t) structures, along with
 * related mode/option constants, extracted from rtklib.h with zero
 * algorithmic changes.
 *
 * @note Default option instances (prcopt_default, solopt_default) are
 *       defined in mrtk_opt.c.
 */
#ifndef MRTK_OPT_H
#define MRTK_OPT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_nav.h" /* pcv_t, MAXSAT, MAXANT, MAXSTRPATH */
#include "mrtklib/mrtk_obs.h" /* snrmask_t */

/*============================================================================
 * Processing Mode Constants
 *===========================================================================*/

#define PMODE_SINGLE 0         /* positioning mode: single */
#define PMODE_DGPS 1           /* positioning mode: DGPS/DGNSS */
#define PMODE_KINEMA 2         /* positioning mode: kinematic */
#define PMODE_STATIC 3         /* positioning mode: static */
#define PMODE_MOVEB 4          /* positioning mode: moving-base */
#define PMODE_FIXED 5          /* positioning mode: fixed */
#define PMODE_PPP_KINEMA 6     /* positioning mode: PPP-kinemaric */
#define PMODE_PPP_STATIC 7     /* positioning mode: PPP-static */
#define PMODE_PPP_FIXED 8      /* positioning mode: PPP-fixed */
#define PMODE_PPP_RTK 9        /* positioning mode: PPP-RTK (CLAS) */
#define PMODE_SSR2OSR 10       /* conversion mode: SSR2OSR */
#define PMODE_SSR2OSR_FIXED 11 /* conversion mode: SSR2OSR-fixed */
#define PMODE_VRS_RTK 12       /* positioning mode: VRS-RTK (CLAS) */

/*============================================================================
 * Solution Format / Quality Constants
 *===========================================================================*/

#define SOLF_LLH 0  /* solution format: lat/lon/height */
#define SOLF_XYZ 1  /* solution format: x/y/z-ecef */
#define SOLF_ENU 2  /* solution format: e/n/u-baseline */
#define SOLF_NMEA 3 /* solution format: NMEA-183 */
#define SOLF_STAT 4 /* solution format: solution status */
#define SOLF_GSIF 5 /* solution format: GSI F1/F2 */

#define SOLQ_NONE 0   /* solution status: no solution */
#define SOLQ_FIX 1    /* solution status: fix */
#define SOLQ_FLOAT 2  /* solution status: float */
#define SOLQ_SBAS 3   /* solution status: SBAS */
#define SOLQ_DGPS 4   /* solution status: DGPS/DGNSS */
#define SOLQ_SINGLE 5 /* solution status: single */
#define SOLQ_PPP 6    /* solution status: PPP */
#define SOLQ_DR 7     /* solution status: dead reconing */
#define MAXSOLQ 7     /* max number of solution status */

/*============================================================================
 * Time System Constants
 *===========================================================================*/

#define TIMES_GPST 0 /* time system: gps time */
#define TIMES_UTC 1  /* time system: utc */
#define TIMES_JST 2  /* time system: jst */

/*============================================================================
 * Ionosphere / Troposphere / Ephemeris Option Constants
 *===========================================================================*/

#define IONOOPT_OFF 0      /* ionosphere option: correction off */
#define IONOOPT_BRDC 1     /* ionosphere option: broadcast model */
#define IONOOPT_SBAS 2     /* ionosphere option: SBAS model */
#define IONOOPT_IFLC 3     /* ionosphere option: L1/L2 iono-free LC */
#define IONOOPT_EST 4      /* ionosphere option: estimation */
#define IONOOPT_TEC 5      /* ionosphere option: IONEX TEC model */
#define IONOOPT_QZS 6      /* ionosphere option: QZSS broadcast model */
#define IONOOPT_STEC 8     /* ionosphere option: SLANT TEC model */
#define IONOOPT_EST_ADPT 9 /* ionosphere option: adaptive estimation */

#define TROPOPT_OFF 0  /* troposphere option: correction off */
#define TROPOPT_SAAS 1 /* troposphere option: Saastamoinen model */
#define TROPOPT_SBAS 2 /* troposphere option: SBAS model */
#define TROPOPT_EST 3  /* troposphere option: ZTD estimation */
#define TROPOPT_ESTG 4 /* troposphere option: ZTD+grad estimation */
#define TROPOPT_ZTD 5  /* troposphere option: ZTD correction */

#define EPHOPT_BRDC 0   /* ephemeris option: broadcast ephemeris */
#define EPHOPT_PREC 1   /* ephemeris option: precise ephemeris */
#define EPHOPT_SBAS 2   /* ephemeris option: broadcast + SBAS */
#define EPHOPT_SSRAPC 3 /* ephemeris option: broadcast + SSR_APC */
#define EPHOPT_SSRCOM 4 /* ephemeris option: broadcast + SSR_COM */

/*============================================================================
 * AR Mode Constants
 *===========================================================================*/

#define ARMODE_OFF 0     /* AR mode: off */
#define ARMODE_CONT 1    /* AR mode: continuous */
#define ARMODE_INST 2    /* AR mode: instantaneous */
#define ARMODE_FIXHOLD 3 /* AR mode: fix and hold */
#define ARMODE_WLNL 4    /* AR mode: wide lane/narrow lane */
#define ARMODE_TCAR 5    /* AR mode: triple carrier ar */

#define GLO_ARMODE_OFF 0     /* GLO AR mode: off */
#define GLO_ARMODE_ON 1      /* GLO AR mode: on */
#define GLO_ARMODE_AUTOCAL 2 /* GLO AR mode: auto calibrate */
#define GLO_ARMODE_FIXHOLD 3 /* GLO AR mode: fix and hold */

/*============================================================================
 * SBAS / Position Option Constants
 *===========================================================================*/

#define SBSOPT_LCORR 1 /* SBAS option: long term correction */
#define SBSOPT_FCORR 2 /* SBAS option: fast correction */
#define SBSOPT_ICORR 4 /* SBAS option: ionosphere correction */
#define SBSOPT_RANGE 8 /* SBAS option: ranging */

#define POSOPT_POS 0    /* pos option: LLH/XYZ */
#define POSOPT_SINGLE 1 /* pos option: average of single pos */
#define POSOPT_FILE 2   /* pos option: read from pos file */
#define POSOPT_RINEX 3  /* pos option: rinex header pos */
#define POSOPT_RTCM 4   /* pos option: rtcm/raw station pos */

/*============================================================================
 * Extended Receiver Error Model Type
 *===========================================================================*/

typedef struct {               /* extended receiver error model */
    int ena[4];                /* model enabled */
    double cerr[4][NFREQ * 2]; /* code errors (m) */
    double perr[4][NFREQ * 2]; /* carrier-phase errors (m) */
    double gpsglob[NFREQ];     /* gps-glonass h/w bias (m) */
    double gloicb[NFREQ];      /* glonass interchannel bias (m/fn) */
} exterr_t;

/*============================================================================
 * Processing Options Type
 *===========================================================================*/

typedef struct prcopt_t {         /* processing options type */
    int mode;                     /* positioning mode (PMODE_???) */
    int soltype;                  /* solution type (0:forward,1:backward,2:combined) */
    int nf;                       /* number of frequencies (1:L1,2:L1+L2,3:L1+L2+L5) */
    int navsys;                   /* navigation system */
    double elmin;                 /* elevation mask angle (rad) */
    snrmask_t snrmask;            /* SNR mask */
    int sateph;                   /* satellite ephemeris/clock (EPHOPT_???) */
    int modear;                   /* AR mode (0:off,1:continuous,2:instantaneous,3:fix and hold,4:ppp-ar) */
    int glomodear;                /* GLONASS AR mode (0:off,1:on,2:auto cal,3:ext cal) */
    int bdsmodear;                /* BeiDou AR mode (0:off,1:on) */
    int arsys;                    /* navigation system for PPP-AR */
    int maxout;                   /* obs outage count to reset bias */
    int minlock;                  /* min lock count to fix ambiguity */
    int minfix;                   /* min fix count to hold ambiguity */
    int armaxiter;                /* max iteration to resolve ambiguity */
    int ionoopt;                  /* ionosphere option (IONOOPT_???) */
    int tropopt;                  /* troposphere option (TROPOPT_???) */
    int dynamics;                 /* dynamics model (0:none,1:velociy,2:accel) */
    int tidecorr;                 /* earth tide correction (0:off,1:solid,2:solid+otl+pole) */
    int niter;                    /* number of filter iteration */
    int codesmooth;               /* code smoothing window size (0:none) */
    int intpref;                  /* interpolate reference obs (for post mission) */
    int sbascorr;                 /* SBAS correction options */
    int sbassatsel;               /* SBAS satellite selection (0:all) */
    int rovpos;                   /* rover position for fixed mode */
    int refpos;                   /* base position for relative mode */
                                  /* (0:pos in prcopt,  1:average of single pos, */
                                  /*  2:read from file, 3:rinex header, 4:rtcm pos) */
    double eratio[NFREQ];         /* code/phase error ratio */
    double err[8];                /* measurement error factor */
                                  /* [0]:reserved */
                                  /* [1-3]:error factor a/b/c of phase (m) */
                                  /* [4]:doppler frequency (hz) */
                                  /* [5]:max SNR (dBHz) for SNR-based variance (0=disabled) */
                                  /* [6]:SNR variance scale factor (0=disabled) */
                                  /* [7]:reserved */
    double std[3];                /* initial-state std [0]bias,[1]iono [2]trop */
    double prn[7];                /* process-noise std [0]bias,[1]iono [2]trop [3]acch [4]accv [5]pos [6]ifb */
    double sclkstab;              /* satellite clock stability (sec/sec) */
    double thresar[8];            /* AR validation threshold */
    double elmaskar;              /* elevation mask of AR for rising satellite (deg) */
    double elmaskhold;            /* elevation mask to hold ambiguity (deg) */
    double thresslip;             /* slip threshold of geometry-free phase (m) */
    double thresdop;              /* slip threshold of doppler-phase difference (cycle/s) (0:off) */
    double maxtdiff;              /* max difference of time (sec) */
    double maxinno;               /* reject threshold of innovation (m) */
    double maxgdop;               /* reject threshold of gdop */
    double baseline[2];           /* baseline length constraint {const,sigma} (m) */
    double ru[3];                 /* rover position for fixed mode {x,y,z} (ecef) (m) */
    double rb[3];                 /* base position for relative mode {x,y,z} (ecef) (m) */
    char anttype[2][MAXANT];      /* antenna types {rover,base} */
    double antdel[2][3];          /* antenna delta {{rov_e,rov_n,rov_u},{ref_e,ref_n,ref_u}} */
    pcv_t pcvr[2];                /* receiver antenna parameters {rov,base} */
    uint8_t exsats[MAXSAT];       /* excluded satellites (1:excluded,2:included) */
    int ign_chierr;               /* ignore chi-square error mode (0:off,1:on) */
    int bds2bias;                 /* estimate of bias for each BeiDou2 satellite type (0:off,1:on) */
    int sattmode;                 /* 0:nominal */
    int pppsatcb;                 /* satellite code bias selection (0:auto,1:ssr,2:bia,3:dcb) */
    int pppsatpb;                 /* satellite phase bias selection (0:auto,1:ssr,3:fcb) */
    int unbias;                   /* (0:use uncorrected code bias,1:do not use uncorrected code bias) */
    double maxbiasdt;             /* bias sinex  and fcb  code/phase bias timeout(s) */
    int maxaveep;                 /* max averaging epoches */
    int initrst;                  /* initialize by restart */
    int outsingle;                /* output single by dgps/float/fix/ppp outage */
    char rnxopt[2][256];          /* rinex options {rover,base} */
    int posopt[13];               /* positioning options */
    int syncsol;                  /* solution sync mode (0:off,1:on) */
    double odisp[2][6 * 11];      /* ocean tide loading parameters {rov,base} */
    exterr_t exterr;              /* extended receiver error model */
    int freqopt;                  /* disable L2-AR */
    int16_t elmaskopt[360];       /* elevation mask pattern */
    char pppopt[256];             /* ppp option */
    char rtcmopt[256];            /* rtcm options */
    int pppsig[5];                /* signal selection [0]GPS,[1]QZS,[2]GAL,[3]BDS2,[4]BDS3 */
    char staname[32];             /* station name */
    char* l6dpath[MIONO_MAX_PRN]; /* MADOCA-PPP L6D file paths */
    int ionocorr;                 /* MADOCA-PPP ionospheric correction (0:off,1:on) */
    double uraratio;              /* ratio for external URA in PPP variance */

    /* PPP-RTK specific (CLAS) options — appended for ABI stability */
    int gridsel;             /* grid select threshold (m), 0=auto */
    int alphaar;             /* AR significance index (0:0.1%..5:20%) */
    int qzsmodear;           /* QZSS AR mode (0:off,1:on) */
    int minamb;              /* min ambiguities for PAR */
    int armaxdelsat;         /* max excluded sats for PAR */
    int floatcnt;            /* float counter for filter reset */
    int poserrcnt;           /* pos error count to reset */
    int phasshft;            /* phase shift correction (0:off,1:table) */
    int isb;                 /* ISB correction mode (0:off, 1:table) */
    int prnadpt;             /* adaptive process noise (0:off,1:on) */
    double maxinno_ext[5];   /* reject thresholds [0]:L1/L2 [1]:disp [2]:nondisp [3]:F&H [4]:Fix */
    double varholdamb;       /* hold-ambiguity variance (cycle^2) */
    double maxdiffp;         /* max pseudorange diff for reset (m) */
    double maxobsloss_s;     /* max obs gap to reset states (s) */
    double forgetion;        /* forgetting factor iono (0-1) */
    double afgainion;        /* adaptive gain iono */
    double forgetpva;        /* forgetting factor PVA (0-1) */
    double afgainpva;        /* adaptive gain PVA */
    double prnionomax;       /* max process noise for adaptive iono (m) */
    double stats_prnposith;  /* process noise std pos h (m) */
    double stats_prnpositv;  /* process noise std pos v (m) */
    double stats_tconstiono; /* time constant of iono variation (s) */
    char rectype[2][MAXANT]; /* receiver types {rover,ref} */
    int l6mrg;               /* L6 2-channel mode (0:single,1:dual-priority,2:dual-balanced) */
    int regularly;           /* filter reset cycle (s) (0:off) */

    /* VRS-RTK specific options */
    double maxpdopar;   /* maximum PDOP for AR (0:no limit) */
    double maxpdophold; /* maximum PDOP to hold ambiguity (0:no limit) */
    int refdop;         /* reference DOP (0:conventional, 1:single-diff) */
    double beta;        /* ionosphere time constant for Gauss-Markov (s) */

    /* Partial AR and AR filter options (demo5) */
    int minfixsats;     /* min sats for valid AR fix; nb >= minfixsats-1 DD pairs required (0=no minimum) */
    int mindropsats;    /* min sats before excluding weakest for PAR; nb >= mindropsats required (0=off) */
    int minholdsats;    /* min DD pairs for fix-and-hold (0=no minimum) */
    int arfilter;       /* AR filter: exclude newly-locked sats degrading ratio (0:off,1:on) */
    int gpsmodear;      /* GPS AR mode for GLO fix-and-hold second pass (0:off,1:on) */
    double gainholdamb; /* gain for fractional GLO/SBAS inter-channel bias update */

    /* Signal configuration (explicit band/code specification) */
    mrtk_sigcfg_t sigcfg[MRTK_NSYS]; /* per-constellation signal config */
    int sigcfg_set;                   /* 1: sigcfg explicitly configured (overrides nf/pppsig) */
} prcopt_t;

/*============================================================================
 * Solution Options Type
 *===========================================================================*/

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

/*============================================================================
 * File Options Type
 *===========================================================================*/

typedef struct {              /* file options type */
    char satantp[MAXSTRPATH]; /* satellite antenna parameters file */
    char rcvantp[MAXSTRPATH]; /* receiver antenna parameters file */
    char stapos[MAXSTRPATH];  /* station positions file */
    char geoid[MAXSTRPATH];   /* external geoid data file */
    char iono[MAXSTRPATH];    /* ionosphere data file */
    char dcb[MAXSTRPATH];     /* dcb data file */
    char eop[MAXSTRPATH];     /* eop data file */
    char blq[MAXSTRPATH];     /* ocean tide loading blq file */
    char tempdir[MAXSTRPATH]; /* ftp/http temporaly directory */
    char geexe[MAXSTRPATH];   /* google earth exec file */
    char elmask[MAXSTRPATH];  /* elevation mask file */
    char solstat[MAXSTRPATH]; /* solution statistics file */
    char trace[MAXSTRPATH];   /* debug trace file */
    char bia[MAXSTRPATH];     /* bias sinex data file */
    char fcb[MAXSTRPATH];     /* fcb data file */
    char grid[MAXSTRPATH];    /* CLAS grid definition file */
    char isb[MAXSTRPATH];     /* ISB correction table file */
    char phacyc[MAXSTRPATH];  /* phase cycle shift table file */
} filopt_t;

/*============================================================================
 * Default Option Instances
 *===========================================================================*/

extern const prcopt_t prcopt_default;
extern const solopt_t solopt_default;

#ifdef __cplusplus
}
#endif

#endif /* MRTK_OPT_H */
