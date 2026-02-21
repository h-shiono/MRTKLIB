/**
 * @file mrtk_nav.h
 * @brief MRTKLIB Navigation Module — Navigation data types including nav_t
 *        and all its dependent struct types.
 *
 * This header provides the core navigation data structures extracted from
 * rtklib.h with zero algorithmic changes.  It includes pcv_t, peph_t, pclk_t,
 * TEC/SBAS/SSR types, and the central nav_t aggregation struct.
 *
 * @note No functions are moved in this header — only type definitions.
 *       All functions that operate on these types remain in their original
 *       .c files (preceph.c, sbas.c, rtkcmn.c, etc.).
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
    double off[NFREQ][ 3]; /* phase center offset e/n/u or x/y/z (m) */
    double var[NFREQ][19]; /* phase center variation (m) */
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
 * Navigation Data Type (central aggregation)
 *===========================================================================*/

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

#ifdef __cplusplus
}
#endif

#endif /* MRTK_NAV_H */
