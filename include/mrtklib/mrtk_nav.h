/*------------------------------------------------------------------------------
 * mrtk_nav.h : navigation data type definitions and functions
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
 * @file mrtk_nav.h
 * @brief MRTKLIB Navigation Module — Navigation data types and satellite/nav
 *        utility functions.
 *
 * This header provides the core navigation data structures extracted from
 * rtklib.h with zero algorithmic changes.  It includes pcv_t, peph_t, pclk_t,
 * TEC/SBAS/SSR types, the central nav_t aggregation struct, and satellite
 * number/system conversion functions.
 *
 * @note Functions declared here are implemented in mrtk_nav.c.
 */
#ifndef MRTK_NAV_H
#define MRTK_NAV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_peph.h"
#include "mrtklib/mrtk_obs.h"

/*============================================================================
 * Antenna Parameter Types
 *===========================================================================*/

typedef struct {        /* antenna parameter type */
    int sat;            /* satellite number (0:receiver) */
    char type[MAXANT];  /* antenna type */
    char code[MAXANT];  /* serial number or satellite code */
    gtime_t ts,te;      /* valid time start and end */
    double off[NFREQPCV][ 3]; /* phase center offset e/n/u or x/y/z (m) */
    double var[NFREQPCV][19]; /* phase center variation (m) */
                        /* el=90,85,...,0 or nadir=0,1,2,3,... (deg) */
} pcv_t;

typedef struct {        /* antenna parameters type */
    int n,nmax;         /* number of data/allocated */
    pcv_t *pcv;         /* antenna parameters data */
} pcvs_t;

/*============================================================================
 * Precise Ephemeris / Clock Types
 *===========================================================================*/

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

/*============================================================================
 * TEC Grid Type
 *===========================================================================*/

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

/*============================================================================
 * SBAS Types
 *===========================================================================*/

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

/*============================================================================
 * DGPS / SSR / OSB Types
 *===========================================================================*/

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
    int vcbias[MAXCODE]; /* code biases valid flag (0:invalid, 1:valid) */
    int vpbias[MAXCODE]; /* phase biases valid flag (0:invalid, 1:valid) */
    int discnt[MAXCODE]; /* phase biases discontinuity counter */
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

/*============================================================================
 * MADOCA-PPP L6D Ionospheric Correction Types
 *===========================================================================*/

typedef struct {        /* MADOCA-PPP L6D STEC polynomial type */
    gtime_t t0;         /* correction time */
    int sqi;            /* SSR STEC quality indicator */
    double coef[6];     /* STEC poly.coef. {C00,C01,C10,C11,C02,C20} */
} miono_sat_t;

typedef struct {        /* MADOCA-PPP L6D area type */
    int avalid;         /* 0:invalid, 1:valid */
    int sid;            /* shape ID {rectangle, circle} */
    int type;           /* STEC correction type */
    double ref[2];      /* reference point {Lat., Lon.} */
    double span[2];     /* rect.{Lat.,Lon.}, span{Effective range, N/A} */
    miono_sat_t sat[MAXSAT]; /* satellite STEC polynomial coefficients */
} miono_area_t;

typedef struct {        /* MADOCA-PPP L6D region type */
    int rvalid;         /* 0:invalid, 1:valid */
    int ralert;         /* region alert flag */
    int narea;          /* number of areas */
    miono_area_t area[MIONO_MAX_ANUM]; /* areas */
} miono_region_t;

typedef struct {        /* PPP ionospheric correction data type */
    gtime_t time;       /* update time (GPST) */
    gtime_t t0[MAXSAT]; /* correction time */
    double  dly[MAXSAT]; /* L1 slant delay (m) */
    double  std[MAXSAT]; /* L1 slant delay std (m) */
} pppiono_corr_t;

typedef struct {        /* PPP ionospheric correction type */
    pppiono_corr_t corr; /* ionospheric correction data */
    miono_region_t re[MIONO_MAX_RID]; /* MADOCA-PPP L6D regions */
    int rid;            /* MADOCA-PPP L6D region id */
    int anum;           /* MADOCA-PPP L6D area number */
    int valid;          /* PPP iono correction flag (0:invalid, 1:valid) */
} pppiono_t;

/*============================================================================
 * Station Correction Types
 *===========================================================================*/

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

/*============================================================================
 * Local Combination Types
 *===========================================================================*/

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

/*============================================================================
 * ISB (Inter-System Bias) Types
 *===========================================================================*/

#define ISBOPT_OFF     0                /* ISB option: correction off */
#define ISBOPT_TABLE   1                /* ISB option: table file */
#define MAXRECTYP      100              /* max number of receiver types */
#define PHASE_CYCLE    (-0.25)          /* default L2C 1/4 cycle phase shift */

typedef struct {        /* ISB data type */
    char sta_name[20];                  /* station/receiver name */
    char sta_name_base[20];             /* base station/receiver name */
    double gsb[NSYS][NFREQ][2];         /* ISB [sys][freq][0:phase,1:code] (m) */
} isb_t;

typedef struct {        /* L2C phase shift table type */
    int n;                              /* number of receiver types */
    char rectyp[MAXRECTYP][MAXANT];     /* receiver type names */
    double bias[MAXRECTYP];             /* phase shift bias (cycle) */
} sft_t;

/*============================================================================
 * Station Parameter Type
 *===========================================================================*/

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
    double isb[NSYS][NFREQ][2]; /* ISB corrections [sys][freq][0:phase,1:code] (m) */
} sta_t;

/*============================================================================
 * Navigation Data Type (central aggregation)
 *===========================================================================*/

typedef struct nav_s {  /* navigation data type */
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
    ssr_t ssr_ch[SSR_CH_NUM][MAXSAT]; /* SSR corrections (per channel) */
    int facility[SSR_CH_NUM]; /* L6 facility ID per channel */
    int filreset;       /* filter reset flag (facility change) */
    int invtrop;        /* inverse troposphere flag (0:estimate, 1:SSR trop valid) */
    pppiono_t *pppiono; /* PPP ionospheric corrections (MADOCA-PPP L6D) (heap) */
    void *clas_ctx;     /* CLAS CSSR correction context (NULL if unused) */
    stat_t stat;        /* stat corrections */
    osb_t osb;          /* Observable-specific Signal Bias data */
    char biapath [MAXSTRPATH]; /* bias sinex file path */
    char fcbpath [MAXSTRPATH]; /* fcb file path */
    char pr_biapath [MAXSTRPATH]; /* previous bias sinex file path */
    char pr_fcbpath [MAXSTRPATH]; /* previous fcb file path */

    /* ISB (Inter-System Bias) data */
    isb_t *isb;             /* ISB data array */
    int ni, nimax;          /* number of ISB data / allocated */
    sft_t sfts;             /* L2C phase shift table */
    sta_t stas[MAXRCV];     /* station info with ISB corrections */
} nav_t;

/*============================================================================
 * Satellite Number / System Functions
 *===========================================================================*/

/**
 * @brief Convert satellite system+prn/slot number to satellite number.
 * @param[in] sys  Satellite system (SYS_GPS, SYS_GLO, ...)
 * @param[in] prn  Satellite prn/slot number
 * @return Satellite number (0: error)
 */
int satno(int sys, int prn);

/**
 * @brief Convert satellite number to satellite system.
 * @param[in]     sat  Satellite number (1-MAXSAT)
 * @param[in,out] prn  Satellite prn/slot number (NULL: no output)
 * @return Satellite system (SYS_GPS, SYS_GLO, ...)
 */
int satsys(int sat, int *prn);

/**
 * @brief Convert satellite id to satellite number.
 * @param[in] id  Satellite id (nn, Gnn, Rnn, Enn, Jnn, Cnn, Inn or Snn)
 * @return Satellite number (0: error)
 */
int satid2no(const char *id);

/**
 * @brief Convert satellite number to satellite id.
 * @param[in]  sat  Satellite number
 * @param[out] id   Satellite id (Gnn, Rnn, Enn, Jnn, Cnn, Inn or nnn)
 */
void satno2id(int sat, char *id);

/**
 * @brief Convert satellite and obs code to carrier frequency.
 * @param[in] sat   Satellite number
 * @param[in] code  Obs code (CODE_???)
 * @param[in] nav   Navigation data for GLONASS (NULL: not used)
 * @return Carrier frequency (Hz) (0.0: error)
 */
double sat2freq(int sat, uint8_t code, const nav_t *nav);

/**
 * @brief Distinguish BDS-2 from BDS-3.
 * @param[in]  sat  Satellite number
 * @param[out] prn  PRN number (NULL: not output)
 * @return Satellite system (SYS_CMP for BDS-3, SYS_BD2 for BDS-2)
 */
int satsys_bd2(int sat, int *prn);

/**
 * @brief Get code priority string for system and frequency index.
 * @param[in] sys       Satellite system (SYS_???)
 * @param[in] freq_idx  Frequency index (0,1,2,...)
 * @return Code priority string from obsdef table ("" on error)
 */
const char *get_codepris(int sys, int freq_idx);

/**
 * @brief Convert frequency index to frequency number using obsdef table.
 * @param[in] sys       Satellite system (SYS_???)
 * @param[in] freq_idx  Frequency index (0,1,2,...)
 * @return Frequency number (1,2,5,...) (0: error)
 */
int freq_idx2freq_num(int sys, int freq_idx);

/**
 * @brief Convert frequency number to frequency index using obsdef table.
 * @param[in] sys       Satellite system (SYS_???)
 * @param[in] freq_num  Frequency number (1,2,5,...)
 * @return Frequency index (0,1,2,...) (-1: error)
 */
int freq_num2freq_idx(int sys, int freq_num);

/**
 * @brief Convert frequency number to frequency in Hz.
 * @param[in] sys       Satellite system (SYS_???)
 * @param[in] freq_num  Frequency number (1,2,5,...)
 * @param[in] fcn       GLONASS frequency channel number (-7..6)
 * @return Frequency (Hz) (0.0: error)
 */
double freq_num2freq(int sys, int freq_num, int fcn);

/**
 * @brief Convert frequency number to antenna PCV index.
 * @param[in] sys       Satellite system (SYS_???)
 * @param[in] freq_num  Frequency number (1,2,5,...)
 * @return Antenna PCV index (0..NFREQPCV-1) (NFREQPCV: error)
 */
int freq_num2ant_idx(int sys, int freq_num);

/**
 * @brief Convert frequency index to antenna PCV index.
 * @param[in] sys       Satellite system (SYS_???)
 * @param[in] freq_idx  Frequency index (0,1,2,...)
 * @return Antenna PCV index (0..NFREQPCV-1) (NFREQPCV: error)
 */
int freq_idx2ant_idx(int sys, int freq_idx);

/**
 * @brief Rearrange obsdef table for a satellite system by frequency numbers.
 * @param[in] sys        Satellite system (SYS_???)
 * @param[in] freq_nums  Array of frequency numbers [MAXFREQ] (0=unused)
 */
void set_obsdef(int sys, const int *freq_nums);

/**
 * @brief Apply PPP signal selection options to obsdef tables.
 * @param[in] pppsig  Signal selection array [5]: GPS,QZS,GAL,BDS2,BDS3
 */
void apply_pppsig(const int *pppsig);

/*============================================================================
 * Navigation Data I/O Functions
 *===========================================================================*/

/**
 * @brief Unique ephemerides in navigation data.
 * @param[in,out] nav  Navigation data
 */
void uniqnav(nav_t *nav);

/**
 * @brief Read navigation data from file.
 * @param[in]  file  File path
 * @param[out] nav   Navigation data
 * @return Status (1:ok, 0:no file)
 */
int readnav(const char *file, nav_t *nav);

/**
 * @brief Save navigation data to file.
 * @param[in] file  File path
 * @param[in] nav   Navigation data
 * @return Status (1:ok, 0:file open error)
 */
int savenav(const char *file, const nav_t *nav);

/**
 * @brief Free navigation data memory.
 * @param[in,out] nav  Navigation data
 * @param[in]     opt  Option (0x01:eph, 0x02:geph, 0x04:seph,
 *                     0x08:peph, 0x10:pclk, 0x20:alm, 0x40:tec)
 */
void freenav(nav_t *nav, int opt);

/*============================================================================
 * Ephemeris and Clock Functions (nav_t-dependent)
 *===========================================================================*/

/**
 * @brief Compute satellite position, velocity and clock.
 * @param[in]  time    Time (GPST)
 * @param[in]  teph    Time to select ephemeris (GPST)
 * @param[in]  sat     Satellite number
 * @param[in]  ephopt  Ephemeris option (EPHOPT_???)
 * @param[in]  nav     Navigation data
 * @param[out] rs      Sat position and velocity (ecef) {x,y,z,vx,vy,vz} (m|m/s)
 * @param[out] dts     Sat clock {bias,drift} (s|s/s)
 * @param[out] var     Sat position and clock error variance (m^2)
 * @param[out] svh     Sat health flag (-1:correction not available)
 * @return Status (1:ok, 0:error)
 */
int satpos(gtime_t time, gtime_t teph, int sat, int ephopt,
           const nav_t *nav, double *rs, double *dts, double *var,
           int *svh);

/**
 * @brief Compute satellite positions, velocities and clocks.
 * @param[in]  teph    Time to select ephemeris (GPST)
 * @param[in]  obs     Observation data
 * @param[in]  n       Number of observation data
 * @param[in]  nav     Navigation data
 * @param[in]  ephopt  Ephemeris option (EPHOPT_???)
 * @param[out] rs      Satellite positions and velocities (ecef)
 * @param[out] dts     Satellite clocks
 * @param[out] var     Sat position and clock error variances (m^2)
 * @param[out] svh     Sat health flag (-1:correction not available)
 */
void satposs(gtime_t teph, const obsd_t *obs, int n, const nav_t *nav,
             int ephopt, double *rs, double *dts, double *var, int *svh);

/**
 * @brief Compute satellite position/clock with precise ephemeris/clock.
 * @param[in]  time  Time (GPST)
 * @param[in]  sat   Satellite number
 * @param[in]  nav   Navigation data
 * @param[in]  opt   Sat position option (0:center of mass, 1:antenna phase center)
 * @param[out] rs    Sat position and velocity (ecef) {x,y,z,vx,vy,vz} (m|m/s)
 * @param[out] dts   Sat clock {bias,drift} (s|s/s)
 * @param[out] var   Sat position and clock error variance (m^2) (NULL: no output)
 * @return Status (1:ok, 0:error or data outage)
 */
int peph2pos(gtime_t time, int sat, const nav_t *nav, int opt,
             double *rs, double *dts, double *var);

/**
 * @brief Compute satellite antenna phase center offset in ecef.
 * @param[in]  time  Time (GPST)
 * @param[in]  rs    Satellite position and velocity (ecef) {x,y,z,vx,vy,vz} (m|m/s)
 * @param[in]  sat   Satellite number
 * @param[in]  nav   Navigation data
 * @param[out] dant  Satellite antenna phase center offset (ecef) {dx,dy,dz} (m)
 */
void satantoff(gtime_t time, const double *rs, int sat, const nav_t *nav,
               double *dant);

/**
 * @brief Read sp3 precise ephemeris file.
 * @param[in]  file  SP3 precise ephemeris file (wild-card * expanded)
 * @param[in,out] nav  Navigation data
 * @param[in]  opt   Options (1:only observed + 2:only predicted + 4:not combined)
 */
void readsp3(const char *file, nav_t *nav, int opt);

/**
 * @brief Read satellite antenna parameters.
 * @param[in]  file  Antenna parameter file
 * @param[in]  time  Time (GPST)
 * @param[in,out] nav  Navigation data
 * @return Status (1:ok, 0:error)
 */
int readsap(const char *file, gtime_t time, nav_t *nav);

/**
 * @brief Read differential code bias (DCB) parameters.
 * @param[in]  file  DCB parameters file (wild-card * expanded)
 * @param[in,out] nav  Navigation data
 * @param[in]  sta   Station info data to import receiver DCB (NULL: no use)
 * @return Status (1:ok, 0:error)
 */
int readdcb(const char *file, nav_t *nav, const sta_t *sta);

/*============================================================================
 * Satellite Exclusion
 *===========================================================================*/

/* Forward declaration to avoid circular dependency with mrtk_opt.h */
struct prcopt_t;

/**
 * @brief Test excluded satellite.
 * @param[in] sat  Satellite number
 * @param[in] var  Variance of ephemeris (m^2)
 * @param[in] svh  SV health flag (-1: correction not available)
 * @param[in] opt  Processing options (NULL: not used)
 * @return Status (1:excluded, 0:not excluded)
 */
int satexclude(int sat, double var, int svh, const struct prcopt_t *opt);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_NAV_H */
