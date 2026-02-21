/*------------------------------------------------------------------------------
* rtkcmn.c : rtklib common functions
*
* Copyright (C) 2024 Japan Aerospace Exploration Agency. All Rights Reserved.
* Copyright (C) 2007-2021 by T.TAKASU, All rights reserved.
*
* options : -DLAPACK   use LAPACK/BLAS
*           -DMKL      use Intel MKL
*           -DTRACE    enable debug trace
*           -DWIN32    use WIN32 API
*           -DNOCALLOC no use calloc for zero matrix
*           -DIERS_MODEL use GMF instead of NMF
*           -DDLL      built for shared library
*           -DCPUTIME_IN_GPST cputime operated in gpst
*
* references :
*     [1] IS-GPS-200D, Navstar GPS Space Segment/Navigation User Interfaces,
*         7 March, 2006
*     [2] RTCA/DO-229C, Minimum operational performance standards for global
*         positioning system/wide area augmentation system airborne equipment,
*         November 28, 2001
*     [3] M.Rothacher, R.Schmid, ANTEX: The Antenna Exchange Format Version 1.4,
*         15 September, 2010
*     [4] A.Gelb ed., Applied Optimal Estimation, The M.I.T Press, 1974
*     [5] A.E.Niell, Global mapping functions for the atmosphere delay at radio
*         wavelengths, Journal of geophysical research, 1996
*     [6] W.Gurtner and L.Estey, RINEX The Receiver Independent Exchange Format
*         Version 3.00, November 28, 2007
*     [7] J.Kouba, A Guide to using International GNSS Service (IGS) products,
*         May 2009
*     [8] China Satellite Navigation Office, BeiDou navigation satellite system
*         signal in space interface control document, open service signal B1I
*         (version 1.0), Dec 2012
*     [9] J.Boehm, A.Niell, P.Tregoning and H.Shuh, Global Mapping Function
*         (GMF): A new empirical mapping function base on numerical weather
*         model data, Geophysical Research Letters, 33, L07304, 2006
*     [10] GLONASS/GPS/Galileo/Compass/SBAS NV08C receiver series BINR interface
*         protocol specification ver.1.3, August, 2012
*
* history : 2007/01/12 1.0 new
*           2007/03/06 1.1 input initial rover pos of pntpos()
*                          update only effective states of filter()
*                          fix bug of atan2() domain error
*           2007/04/11 1.2 add function antmodel()
*                          add gdop mask for pntpos()
*                          change constant MAXDTOE value
*           2007/05/25 1.3 add function execcmd(),expandpath()
*           2008/06/21 1.4 add funciton sortobs(),uniqeph(),screent()
*                          replace geodist() by sagnac correction way
*           2008/10/29 1.5 fix bug of ionospheric mapping function
*                          fix bug of seasonal variation term of tropmapf
*           2008/12/27 1.6 add function tickget(), sleepms(), tracenav(),
*                          xyz2enu(), satposv(), pntvel(), covecef()
*           2009/03/12 1.7 fix bug on error-stop when localtime() returns NULL
*           2009/03/13 1.8 fix bug on time adjustment for summer time
*           2009/04/10 1.9 add function adjgpsweek(),getbits(),getbitu()
*                          add function geph2pos()
*           2009/06/08 1.10 add function seph2pos()
*           2009/11/28 1.11 change function pntpos()
*                           add function tracegnav(),tracepeph()
*           2009/12/22 1.12 change default parameter of ionos std
*                           valid under second for timeget()
*           2010/07/28 1.13 fix bug in tropmapf()
*                           added api:
*                               obs2code(),code2obs(),cross3(),normv3(),
*                               gst2time(),time2gst(),time_str(),timeset(),
*                               deg2dms(),dms2deg(),searchpcv(),antmodel_s(),
*                               tracehnav(),tracepclk(),reppath(),reppaths(),
*                               createdir()
*                           changed api:
*                               readpcv(),
*                           deleted api:
*                               uniqeph()
*           2010/08/20 1.14 omit to include mkl header files
*                           fix bug on chi-sqr(n) table
*           2010/12/11 1.15 added api:
*                               freeobs(),freenav(),ionppp()
*           2011/05/28 1.16 fix bug on half-hour offset by time2epoch()
*                           added api:
*                               uniqnav()
*           2012/06/09 1.17 add a leap second after 2012-6-30
*           2012/07/15 1.18 add api setbits(),setbitu(),utc2gmst()
*                           fix bug on interpolation of antenna pcv
*                           fix bug on str2num() for string with over 256 char
*                           add api readblq(),satexclude(),setcodepri(),
*                           getcodepri()
*                           change api obs2code(),code2obs(),antmodel()
*           2012/12/25 1.19 fix bug on satwavelen(),code2obs(),obs2code()
*                           add api testsnr()
*           2013/01/04 1.20 add api gpst2bdt(),bdt2gpst(),bdt2time(),time2bdt()
*                           readblq(),readerp(),geterp(),crc16()
*                           change api eci2ecef(),sunmoonpos()
*           2013/03/26 1.21 tickget() uses clock_gettime() for linux
*           2013/05/08 1.22 fix bug on nutation coefficients for ast_args()
*           2013/06/02 1.23 add #ifdef for undefined CLOCK_MONOTONIC_RAW
*           2013/09/01 1.24 fix bug on interpolation of satellite antenna pcv
*           2013/09/06 1.25 fix bug on extrapolation of erp
*           2014/04/27 1.26 add SYS_LEO for satellite system
*                           add BDS L1 code for RINEX 3.02 and RTCM 3.2
*                           support BDS L1 in satwavelen()
*           2014/05/29 1.27 fix bug on obs2code() to search obs code table
*           2014/08/26 1.28 fix problem on output of uncompress() for tar file
*                           add function to swap trace file with keywords
*           2014/10/21 1.29 strtok() -> strtok_r() in expath() for thread-safe
*                           add bdsmodear in procopt_default
*           2015/03/19 1.30 fix bug on interpolation of erp values in geterp()
*                           add leap second insertion before 2015/07/01 00:00
*                           add api read_leaps()
*           2015/05/31 1.31 delete api windupcorr()
*           2015/08/08 1.32 add compile option CPUTIME_IN_GPST
*                           add api add_fatal()
*                           support usno leapsec.dat for api read_leaps()
*           2016/01/23 1.33 enable septentrio
*           2016/02/05 1.34 support GLONASS for savenav(), loadnav()
*           2016/06/11 1.35 delete trace() in reppath() to avoid deadlock
*           2016/07/01 1.36 support IRNSS
*                           add leap second before 2017/1/1 00:00:00
*           2016/07/29 1.37 rename api compress() -> rtk_uncompress()
*                           rename api crc16()    -> rtk_crc16()
*                           rename api crc24q()   -> rtk_crc24q()
*                           rename api crc32()    -> rtk_crc32()
*           2016/08/20 1.38 fix type incompatibility in win64 environment
*                           change constant _POSIX_C_SOURCE 199309 -> 199506
*           2016/08/21 1.39 fix bug on week overflow in time2gpst()/gpst2time()
*           2016/09/05 1.40 fix bug on invalid nav data read in readnav()
*           2016/09/17 1.41 suppress warnings
*           2016/09/19 1.42 modify api deg2dms() to consider numerical error
*           2017/04/11 1.43 delete EXPORT for global variables
*           2018/10/10 1.44 modify api satexclude()
*           2020/11/30 1.45 add API code2idx() to get freq-index
*                           add API code2freq() to get carrier frequency
*                           add API timereset() to reset current time
*                           modify API obs2code(), code2obs() and setcodepri()
*                           delete API satwavelen()
*                           delete API csmooth()
*                           delete global variable lam_carr[]
*                           compensate L3,L4,... PCVs by L2 PCV if no PCV data
*                            in input file by API readpcv()
*                           add support hatanaka-compressed RINEX files with
*                            extension ".crx" or ".CRX"
*                           update stream format strings table
*                           update obs code strings and priority table
*                           use integer types in stdint.h
*                           surppress warnings
*           2021/01/09 1.46 use GLONASS extended SVH in satexclude()
*                           update obscodes[] and codepris[][][]
*           2021/01/11 1.47 lock(),unlock(),initlock()->
*                             rtk_lock(),rtk_unlock(),rtk_initlock()
*           2021/05/21 1.48 add api testelmask(), readelmask()
*                           fix typos in comments
*           2024/02/01 1.49 branch from ver.2.4.3b35 for MALIB
*                           add pos2-arsys,pos2-ign_chierr
*           2024/08/02 1.50 change initial value of glomodear,bdsmodear
*           2024/12/20 1.51 change API sunmoonpos_eci()
*                           add signal_replace() from rtkpos.c
*-----------------------------------------------------------------------------*/
#define _POSIX_C_SOURCE 199506
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#ifndef WIN32
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include "rtklib.h"

/* constants -----------------------------------------------------------------*/

/* POLYCRC32 and POLYCRC24Q moved to mrtk_bits.c */

#define SQR(x)      ((x)*(x))
#define MAX_VAR_EPH SQR(300.0)  /* max variance eph to reject satellite (m^2) */

/* gpst0, gst0, bdt0, leaps[] moved to mrtk_time.c */

const double chisqr[100]={      /* chi-sqr(n) (alpha=0.001) */
    10.8,13.8,16.3,18.5,20.5,22.5,24.3,26.1,27.9,29.6,
    31.3,32.9,34.5,36.1,37.7,39.3,40.8,42.3,43.8,45.3,
    46.8,48.3,49.7,51.2,52.6,54.1,55.5,56.9,58.3,59.7,
    61.1,62.5,63.9,65.2,66.6,68.0,69.3,70.7,72.1,73.4,
    74.7,76.0,77.3,78.6,80.0,81.3,82.6,84.0,85.4,86.7,
    88.0,89.3,90.6,91.9,93.3,94.7,96.0,97.4,98.7,100 ,
    101 ,102 ,103 ,104 ,105 ,107 ,108 ,109 ,110 ,112 ,
    113 ,114 ,115 ,116 ,118 ,119 ,120 ,122 ,123 ,125 ,
    126 ,127 ,128 ,129 ,131 ,132 ,133 ,134 ,135 ,137 ,
    138 ,139 ,140 ,142 ,143 ,144 ,145 ,147 ,148 ,149
};
const prcopt_t prcopt_default={ /* defaults processing options */
    PMODE_SINGLE,0,2,SYS_GPS,   /* mode,soltype,nf,navsys */
    15.0*D2R,{{0,0}},           /* elmin,snrmask */
    0,1,0,0,                    /* sateph,modear,glomodear,bdsmodear */
    SYS_GPS,                    /* arsys */
    5,0,10,1,                   /* maxout,minlock,minfix,armaxiter */
    0,0,0,0,                    /* estion,esttrop,dynamics,tidecorr */
    1,0,0,0,0,                  /* niter,codesmooth,intpref,sbascorr,sbassatsel */
    0,0,                        /* rovpos,refpos */
    {100.0,100.0},              /* eratio[] */
    {100.0,0.003,0.003,0.0,1.0}, /* err[] */
    {30.0,0.03,0.3},            /* std[] */
    {1E-4,1E-3,1E-4,1E-1,1E-2,0.0}, /* prn[] */
    5E-12,                      /* sclkstab */
    {3.0,0.9999,0.25,0.1,0.05}, /* thresar */
    0.0,0.0,0.05,               /* elmaskar,almaskhold,thresslip */
    30.0,30.0,30.0,             /* maxtdif,maxinno,maxgdop */
    {0},{0},{0},                /* baseline,ru,rb */
    {"",""},                    /* anttype */
    {{0}},{{0}},{0}             /* antdel,pcv,exsats */
    ,0,0                        /* ign_chierr,bds2bias */
    ,0,0,0,0                    /* pppsatcb,pppsatpb,unbias,maxbiasdt */
};
const solopt_t solopt_default={ /* defaults solution output options */
    SOLF_LLH,TIMES_GPST,1,3,    /* posf,times,timef,timeu */
    0,1,0,0,0,0,0,              /* degf,outhead,outopt,outvel,datum,height,geoid */
    0,0,0,                      /* solstatic,sstat,trace */
    {0.0,0.0},                  /* nmeaintv */
    " ",""                      /* separator/program name */
};
const char *formatstrs[32]={    /* stream format strings */
    "RTCM 2",                   /*  0 */
    "RTCM 3",                   /*  1 */
    "NovAtel OEM7",             /*  2 */
    "NovAtel OEM3",             /*  3 */
    "u-blox UBX",               /*  4 */
    "Superstar II",             /*  5 */
    "Hemisphere",               /*  6 */
    "SkyTraq",                  /*  7 */
    "Javad GREIS",              /*  8 */
    "NVS BINR",                 /*  9 */
    "BINEX",                    /* 10 */
    "Trimble RT17",             /* 11 */
    "Septentrio SBF",           /* 12 */
    "RINEX",                    /* 13 */
    "SP3",                      /* 14 */
    "RINEX CLK",                /* 15 */
    "SBAS",                     /* 16 */
    "NMEA 0183",                /* 17 */
    NULL
};
static char *obscodes[]={       /* observation code strings */
    
    ""  ,"1C","1P","1W","1Y", "1M","1N","1S","1L","1E", /*  0- 9 */
    "1A","1B","1X","1Z","2C", "2D","2S","2L","2X","2P", /* 10-19 */
    "2W","2Y","2M","2N","5I", "5Q","5X","7I","7Q","7X", /* 20-29 */
    "6A","6B","6C","6X","6Z", "6S","6L","8L","8Q","8X", /* 30-39 */
    "2I","2Q","6I","6Q","3I", "3Q","3X","1I","1Q","5A", /* 40-49 */
    "5B","5C","9A","9B","9C", "9X","1D","5D","5P","5Z", /* 50-59 */
    "6E","7D","7P","7Z","8D", "8P","4A","4B","4X","6D", /* 60-69 */
    "6P",""                                             /* 70-   */
};
static char codepris[7][MAXFREQ][16]={  /* code priority for each freq-index */
   /*    0         1          2          3         4         5     */
    {"C"       ,"PYWCMNDLXS","QXI"     ,""       ,""       ,""      ,""}, /* GPS */
    {"CP"      ,"PC"        ,"QXI"     ,""       ,""       ,""      ,""}, /* GLO */
    {"CBX"     ,"QXI"       ,"QXI"     ,"CXB"    ,"QXI"    ,""      ,""}, /* GAL */
    {"CELXS"   ,"LXS"       ,"QXI"     ,"SEZ"    ,""       ,""      ,""}, /* QZS */
    {"C"       ,"IQX"       ,""        ,""       ,""       ,""      ,""}, /* SBS */
    {"IQDPXSLZ","IQXDPZ"    ,"DPX"     ,"IQXDPZ" ,"DPX"    ,""      ,""}, /* BDS */
    {"ABCX"    ,"ABCX"      ,""        ,""       ,""       ,""      ,""}  /* IRN */
};
/* fatalfunc, fatalerr, add_fatal moved to mrtk_mat.c */

/* tbl_CRC16 and tbl_CRC24Q moved to mrtk_bits.c */
#ifdef MKL
#define LAPACK
#define dgemm_      dgemm
#define dgetrf_     dgetrf
#define dgetri_     dgetri
#define dgetrs_     dgetrs
#endif
#ifdef LAPACK
extern void dgemm_(char *, char *, int *, int *, int *, double *, double *,
                   int *, double *, int *, double *, double *, int *);
extern void dgetrf_(int *, int *, double *, int *, int *, int *);
extern void dgetri_(int *, double *, int *, int *, double *, int *, int *);
extern void dgetrs_(char *, int *, int *, double *, int *, int *, double *,
                    int *, int *);
#endif

#ifdef IERS_MODEL
extern int gmf_(double *mjd, double *lat, double *lon, double *hgt, double *zd,
                double *gmfh, double *gmfw);
#endif

/* satellite system+prn/slot number to satellite number ------------------------
* convert satellite system+prn/slot number to satellite number
* args   : int    sys       I   satellite system (SYS_GPS,SYS_GLO,...)
*          int    prn       I   satellite prn/slot number
* return : satellite number (0:error)
*-----------------------------------------------------------------------------*/
extern int satno(int sys, int prn)
{
    if (prn<=0) return 0;
    switch (sys) {
        case SYS_GPS:
            if (prn<MINPRNGPS||MAXPRNGPS<prn) return 0;
            return prn-MINPRNGPS+1;
        case SYS_GLO:
            if (prn<MINPRNGLO||MAXPRNGLO<prn) return 0;
            return NSATGPS+prn-MINPRNGLO+1;
        case SYS_GAL:
            if (prn<MINPRNGAL||MAXPRNGAL<prn) return 0;
            return NSATGPS+NSATGLO+prn-MINPRNGAL+1;
        case SYS_QZS:
            if (prn<MINPRNQZS||MAXPRNQZS<prn) return 0;
            return NSATGPS+NSATGLO+NSATGAL+prn-MINPRNQZS+1;
        case SYS_CMP:
            if (prn<MINPRNCMP||MAXPRNCMP<prn) return 0;
            return NSATGPS+NSATGLO+NSATGAL+NSATQZS+prn-MINPRNCMP+1;
        case SYS_IRN:
            if (prn<MINPRNIRN||MAXPRNIRN<prn) return 0;
            return NSATGPS+NSATGLO+NSATGAL+NSATQZS+NSATCMP+prn-MINPRNIRN+1;
        case SYS_LEO:
            if (prn<MINPRNLEO||MAXPRNLEO<prn) return 0;
            return NSATGPS+NSATGLO+NSATGAL+NSATQZS+NSATCMP+NSATIRN+
                   prn-MINPRNLEO+1;
        case SYS_SBS:
            if (prn<MINPRNSBS||MAXPRNSBS<prn) return 0;
            return NSATGPS+NSATGLO+NSATGAL+NSATQZS+NSATCMP+NSATIRN+NSATLEO+
                   prn-MINPRNSBS+1;
    }
    return 0;
}
/* satellite number to satellite system ----------------------------------------
* convert satellite number to satellite system
* args   : int    sat       I   satellite number (1-MAXSAT)
*          int    *prn      IO  satellite prn/slot number (NULL: no output)
* return : satellite system (SYS_GPS,SYS_GLO,...)
*-----------------------------------------------------------------------------*/
extern int satsys(int sat, int *prn)
{
    int sys=SYS_NONE;
    if (sat<=0||MAXSAT<sat) sat=0;
    else if (sat<=NSATGPS) {
        sys=SYS_GPS; sat+=MINPRNGPS-1;
    }
    else if ((sat-=NSATGPS)<=NSATGLO) {
        sys=SYS_GLO; sat+=MINPRNGLO-1;
    }
    else if ((sat-=NSATGLO)<=NSATGAL) {
        sys=SYS_GAL; sat+=MINPRNGAL-1;
    }
    else if ((sat-=NSATGAL)<=NSATQZS) {
        sys=SYS_QZS; sat+=MINPRNQZS-1; 
    }
    else if ((sat-=NSATQZS)<=NSATCMP) {
        sys=SYS_CMP; sat+=MINPRNCMP-1; 
    }
    else if ((sat-=NSATCMP)<=NSATIRN) {
        sys=SYS_IRN; sat+=MINPRNIRN-1; 
    }
    else if ((sat-=NSATIRN)<=NSATLEO) {
        sys=SYS_LEO; sat+=MINPRNLEO-1; 
    }
    else if ((sat-=NSATLEO)<=NSATSBS) {
        sys=SYS_SBS; sat+=MINPRNSBS-1; 
    }
    else sat=0;
    if (prn) *prn=sat;
    return sys;
}
/* satellite id to satellite number --------------------------------------------
* convert satellite id to satellite number
* args   : char   *id       I   satellite id (nn,Gnn,Rnn,Enn,Jnn,Cnn,Inn or Snn)
* return : satellite number (0: error)
* notes  : 120-142 and 193-199 are also recognized as sbas and qzss
*-----------------------------------------------------------------------------*/
extern int satid2no(const char *id)
{
    int sys,prn;
    char code;
    
    if (sscanf(id,"%d",&prn)==1) {
        if      (MINPRNGPS<=prn&&prn<=MAXPRNGPS) sys=SYS_GPS;
        else if (MINPRNSBS<=prn&&prn<=MAXPRNSBS) sys=SYS_SBS;
        else if (MINPRNQZS<=prn&&prn<=MAXPRNQZS) sys=SYS_QZS;
        else return 0;
        return satno(sys,prn);
    }
    if (sscanf(id,"%c%d",&code,&prn)<2) return 0;
    
    switch (code) {
        case 'G': sys=SYS_GPS; prn+=MINPRNGPS-1; break;
        case 'R': sys=SYS_GLO; prn+=MINPRNGLO-1; break;
        case 'E': sys=SYS_GAL; prn+=MINPRNGAL-1; break;
        case 'J': sys=SYS_QZS; prn+=MINPRNQZS-1; break;
        case 'C': sys=SYS_CMP; prn+=MINPRNCMP-1; break;
        case 'I': sys=SYS_IRN; prn+=MINPRNIRN-1; break;
        case 'L': sys=SYS_LEO; prn+=MINPRNLEO-1; break;
        case 'S': sys=SYS_SBS; prn+=100; break;
        default: return 0;
    }
    return satno(sys,prn);
}
/* satellite number to satellite id --------------------------------------------
* convert satellite number to satellite id
* args   : int    sat       I   satellite number
*          char   *id       O   satellite id (Gnn,Rnn,Enn,Jnn,Cnn,Inn or nnn)
* return : none
*-----------------------------------------------------------------------------*/
extern void satno2id(int sat, char *id)
{
    int prn;
    switch (satsys(sat,&prn)) {
        case SYS_GPS: sprintf(id,"G%02d",prn-MINPRNGPS+1); return;
        case SYS_GLO: sprintf(id,"R%02d",prn-MINPRNGLO+1); return;
        case SYS_GAL: sprintf(id,"E%02d",prn-MINPRNGAL+1); return;
        case SYS_QZS: sprintf(id,"J%02d",prn-MINPRNQZS+1); return;
        case SYS_CMP: sprintf(id,"C%02d",prn-MINPRNCMP+1); return;
        case SYS_IRN: sprintf(id,"I%02d",prn-MINPRNIRN+1); return;
        case SYS_LEO: sprintf(id,"L%02d",prn-MINPRNLEO+1); return;
        case SYS_SBS: sprintf(id,"%03d" ,prn); return;
    }
    strcpy(id,"");
}
/* test excluded satellite -----------------------------------------------------
* test excluded satellite
* args   : int    sat       I   satellite number
*          double var       I   variance of ephemeris (m^2)
*          int    svh       I   sv health flag
*          prcopt_t *opt    I   processing options (NULL: not used)
* return : status (1:excluded,0:not excluded)
*-----------------------------------------------------------------------------*/
extern int satexclude(int sat, double var, int svh, const prcopt_t *opt)
{
    int sys=satsys(sat,NULL);
    
    if (svh<0) return 1; /* ephemeris unavailable */
    
    if (opt) {
        if (opt->exsats[sat-1]==1) return 1; /* excluded satellite */
        if (opt->exsats[sat-1]==2) return 0; /* included satellite */
        if (!(sys&opt->navsys)) return 1; /* unselected sat sys */
    }
    if (sys==SYS_QZS) svh&=0xEE; /* mask QZSS L1C/A,C/B health */
    
    if (sys==SYS_GLO) {
        if ((svh&9)||((svh>>1)&3)==2) { /* test Bn and extended SVH */
            trace(3,"unhealthy GLO satellite: sat=%3d svh=%02X\n",sat,svh);
            return 1;
        }
    }
    else if (svh) {
        trace(3,"unhealthy satellite: sat=%3d svh=%02X\n",sat,svh);
        return 1;
    }
    if (var>MAX_VAR_EPH) {
        trace(3,"invalid ura satellite: sat=%3d ura=%.2f\n",sat,sqrt(var));
        return 1;
    }
    return 0;
}
/* test SNR mask ---------------------------------------------------------------
* test SNR mask
* args   : int    base      I   rover or base-station (0:rover,1:base station)
*          int    idx       I   frequency index (0:L1,1:L2,2:L3,...)
*          double el        I   elevation angle (rad)
*          double snr       I   C/N0 (dBHz)
*          snrmask_t *mask  I   SNR mask
* return : status (1:masked,0:unmasked)
*-----------------------------------------------------------------------------*/
extern int testsnr(int base, int idx, double el, double snr,
                   const snrmask_t *mask)
{
    double minsnr,a;
    int i;
    
    if (!mask->ena[base]||idx<0||idx>=NFREQ) return 0;
    
    a=(el*R2D+5.0)/10.0;
    i=(int)floor(a); a-=i;
    if      (i<1) minsnr=mask->mask[idx][0];
    else if (i>8) minsnr=mask->mask[idx][8];
    else minsnr=(1.0-a)*mask->mask[idx][i-1]+a*mask->mask[idx][i];
    
    return snr<minsnr;
}
/* test elevation mask ---------------------------------------------------------
* test elevation mask
* args   : double *azel     I   azimuth/elevation angle {az,el} (rad)
*          int16_t *elmask  I   elevation mask vector (360 x 1) (0.1 deg)
*                                 elmask[i]: elevation mask at azimuth i (deg)
* return : status (1:masked,0:unmasked)
*-----------------------------------------------------------------------------*/
extern int testelmask(const double *azel, const int16_t *elmask)
{
    double az=azel[0]*R2D;
    
    az-=floor(az/360.0)*360.0; /* 0 <= az < 360.0 */
    
    return azel[1]*R2D<elmask[(int)floor(az)]*0.1;
}
/* obs type string to obs code -------------------------------------------------
* convert obs code type string to obs code
* args   : char   *str      I   obs code string ("1C","1P","1Y",...)
* return : obs code (CODE_???)
* notes  : obs codes are based on RINEX 3.04
*-----------------------------------------------------------------------------*/
extern uint8_t obs2code(const char *obs)
{
    int i;
    
    for (i=1;*obscodes[i];i++) {
        if (strcmp(obscodes[i],obs)) continue;
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
extern char *code2obs(uint8_t code)
{
    if (code<=CODE_NONE||MAXCODE<code) return "";
    return obscodes[code];
}
/* GPS obs code to frequency -------------------------------------------------*/
static int code2freq_GPS(uint8_t code, double *freq)
{
    char *obs=code2obs(code);
    
    switch (obs[0]) {
        case '1': *freq=FREQ1; return 0; /* L1 */
        case '2': *freq=FREQ2; return 1; /* L2 */
        case '5': *freq=FREQ5; return 2; /* L5 */
    }
    return -1;
}
/* GLONASS obs code to frequency ---------------------------------------------*/
static int code2freq_GLO(uint8_t code, int fcn, double *freq)
{
    char *obs=code2obs(code);
    
    if (fcn<-7||fcn>6) return -1;
    
    switch (obs[0]) {
        case '1': *freq=FREQ1_GLO+DFRQ1_GLO*fcn; return 0; /* G1 */
        case '2': *freq=FREQ2_GLO+DFRQ2_GLO*fcn; return 1; /* G2 */
        case '3': *freq=FREQ3_GLO;               return 2; /* G3 */
        case '4': *freq=FREQ1a_GLO;              return 0; /* G1a */
        case '6': *freq=FREQ2a_GLO;              return 1; /* G2a */
    }
    return -1;
}
/* Galileo obs code to frequency ---------------------------------------------*/
static int code2freq_GAL(uint8_t code, double *freq)
{
    char *obs=code2obs(code);
    
    switch (obs[0]) {
        case '1': *freq=FREQ1; return 0; /* E1 */
        case '7': *freq=FREQ7; return 1; /* E5b */
        case '5': *freq=FREQ5; return 2; /* E5a */
        case '6': *freq=FREQ6; return 3; /* E6 */
        case '8': *freq=FREQ8; return 4; /* E5a+b */
    }
    return -1;
}
/* QZSS obs code to frequency ------------------------------------------------*/
static int code2freq_QZS(uint8_t code, double *freq)
{
    char *obs=code2obs(code);
    
    switch (obs[0]) {
        case '1': *freq=FREQ1; return 0; /* L1 */
        case '2': *freq=FREQ2; return 1; /* L2 */
        case '5': *freq=FREQ5; return 2; /* L5 */
        case '6': *freq=FREQ6; return 3; /* L6 */
    }
    return -1;
}
/* SBAS obs code to frequency ------------------------------------------------*/
static int code2freq_SBS(uint8_t code, double *freq)
{
    char *obs=code2obs(code);
    
    switch (obs[0]) {
        case '1': *freq=FREQ1; return 0; /* L1 */
        case '5': *freq=FREQ5; return 1; /* L5 */
    }
    return -1;
}
/* BDS obs code to frequency -------------------------------------------------*/
static int code2freq_BDS(uint8_t code, double *freq)
{
    char *obs=code2obs(code);
    
    switch (obs[0]) {
        case '1': *freq=FREQ1;     return 0; /* B1C/B1A */
        case '2': *freq=FREQ1_CMP; return 0; /* B1 */
        case '7': *freq=FREQ2_CMP; return 1; /* B2/B2b */
        case '5': *freq=FREQ5;     return 2; /* B2a */
        case '6': *freq=FREQ3_CMP; return 3; /* B3/B3A */
        case '8': *freq=FREQ8;     return 4; /* B2a+b */
    }
    return -1;
}
/* NavIC obs code to frequency -----------------------------------------------*/
static int code2freq_IRN(uint8_t code, double *freq)
{
    char *obs=code2obs(code);
    
    switch (obs[0]) {
        case '5': *freq=FREQ5; return 0; /* L5 */
        case '9': *freq=FREQ9; return 1; /* S */
    }
    return -1;
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
extern int code2idx(int sys, uint8_t code)
{
    double freq;
    
    switch (sys) {
        case SYS_GPS: return code2freq_GPS(code,&freq);
        case SYS_GLO: return code2freq_GLO(code,0,&freq);
        case SYS_GAL: return code2freq_GAL(code,&freq);
        case SYS_QZS: return code2freq_QZS(code,&freq);
        case SYS_SBS: return code2freq_SBS(code,&freq);
        case SYS_CMP: return code2freq_BDS(code,&freq);
        case SYS_IRN: return code2freq_IRN(code,&freq);
    }
    return -1;
}
/* system and obs code to frequency --------------------------------------------
* convert system and obs code to carrier frequency
* args   : int    sys       I   satellite system (SYS_???)
*          uint8_t code     I   obs code (CODE_???)
*          int    fcn       I   frequency channel number for GLONASS
* return : carrier frequency (Hz) (0.0: error)
*-----------------------------------------------------------------------------*/
extern double code2freq(int sys, uint8_t code, int fcn)
{
    double freq=0.0;
    
    switch (sys) {
        case SYS_GPS: (void)code2freq_GPS(code,&freq); break;
        case SYS_GLO: (void)code2freq_GLO(code,fcn,&freq); break;
        case SYS_GAL: (void)code2freq_GAL(code,&freq); break;
        case SYS_QZS: (void)code2freq_QZS(code,&freq); break;
        case SYS_SBS: (void)code2freq_SBS(code,&freq); break;
        case SYS_CMP: (void)code2freq_BDS(code,&freq); break;
        case SYS_IRN: (void)code2freq_IRN(code,&freq); break;
    }
    return freq;
}
/* satellite and obs code to frequency -----------------------------------------
* convert satellite and obs code to carrier frequency
* args   : int    sat       I   satellite number
*          uint8_t code     I   obs code (CODE_???)
*          nav_t  *nav_t    I   navigation data for GLONASS (NULL: not used)
* return : carrier frequency (Hz) (0.0: error)
*-----------------------------------------------------------------------------*/
extern double sat2freq(int sat, uint8_t code, const nav_t *nav)
{
    int i,fcn=0,sys,prn;
    
    sys=satsys(sat,&prn);
    
    if (sys==SYS_GLO) {
        if (!nav) return 0.0;
        for (i=0;i<nav->ng;i++) {
            if (nav->geph[i].sat==sat) break;
        }
        if (i<nav->ng) {
            fcn=nav->geph[i].frq;
        }
        else if (nav->glo_fcn[prn-1]>0) {
            fcn=nav->glo_fcn[prn-1]-8;
        }
        else return 0.0;
    }
    return code2freq(sys,code,fcn);
}
/* set code priority -----------------------------------------------------------
* set code priority for multiple codes in a frequency
* args   : int    sys       I   system (or of SYS_???)
*          int    idx       I   frequency index (0- )
*          char   *pri      I   priority of codes (series of code characters)
*                               (higher priority precedes lower)
* return : none
*-----------------------------------------------------------------------------*/
extern void setcodepri(int sys, int idx, const char *pri)
{
    trace(3,"setcodepri:sys=%d idx=%d pri=%s\n",sys,idx,pri);
    
    if (idx<0||idx>=MAXFREQ) return;
    if (sys&SYS_GPS) strcpy(codepris[0][idx],pri);
    if (sys&SYS_GLO) strcpy(codepris[1][idx],pri);
    if (sys&SYS_GAL) strcpy(codepris[2][idx],pri);
    if (sys&SYS_QZS) strcpy(codepris[3][idx],pri);
    if (sys&SYS_SBS) strcpy(codepris[4][idx],pri);
    if (sys&SYS_CMP) strcpy(codepris[5][idx],pri);
    if (sys&SYS_IRN) strcpy(codepris[6][idx],pri);
}
/* get code priority -----------------------------------------------------------
* get code priority for multiple codes in a frequency
* args   : int    sys       I   system (SYS_???)
*          uint8_t code     I   obs code (CODE_???)
*          char   *opt      I   code options (NULL:no option)
* return : priority (15:highest-1:lowest,0:error)
*-----------------------------------------------------------------------------*/
extern int getcodepri(int sys, uint8_t code, const char *opt)
{
    const char *p,*optstr;
    char *obs,str[8]="";
    int i,j;
    
    switch (sys) {
        case SYS_GPS: i=0; optstr="-GL%2s"; break;
        case SYS_GLO: i=1; optstr="-RL%2s"; break;
        case SYS_GAL: i=2; optstr="-EL%2s"; break;
        case SYS_QZS: i=3; optstr="-JL%2s"; break;
        case SYS_SBS: i=4; optstr="-SL%2s"; break;
        case SYS_CMP: i=5; optstr="-CL%2s"; break;
        case SYS_IRN: i=6; optstr="-IL%2s"; break;
        default: return 0;
    }
    if ((j=code2idx(sys,code))<0) return 0;
    obs=code2obs(code);
    
    /* parse code options */
    for (p=opt;p&&(p=strchr(p,'-'));p++) {
        if (sscanf(p,optstr,str)<1||str[0]!=obs[0]) continue;
        return str[1]==obs[1]?15:0;
    }
    /* search code priority */
    return (p=strchr(codepris[i][j],obs[1]))?14-(int)(p-codepris[i][j]):0;
}
/* getbitu, getbits, setbitu, setbits, rtk_crc32, rtk_crc24q, rtk_crc16, decode_word
 * moved to mrtk_bits.c */

/* str2num moved to mrtk_sys.c */

/* coordinate functions moved to mrtk_coords.c */

/* decodef, addpcv, readngspcv, readantex, readpcv, searchpcv moved to mrtk_antenna.c */
/* readelmask, readpos, readblqrecord, readblq moved to mrtk_station.c */
/* readerp, geterp moved to mrtk_peph.c */

/* compare ephemeris ---------------------------------------------------------*/
static int cmpeph(const void *p1, const void *p2)
{
    eph_t *q1=(eph_t *)p1,*q2=(eph_t *)p2;
    return q1->ttr.time!=q2->ttr.time?(int)(q1->ttr.time-q2->ttr.time):
           (q1->toe.time!=q2->toe.time?(int)(q1->toe.time-q2->toe.time):
            q1->sat-q2->sat);
}
/* sort and unique ephemeris -------------------------------------------------*/
static void uniqeph(nav_t *nav)
{
    eph_t *nav_eph;
    int i,j;
    
    trace(3,"uniqeph: n=%d\n",nav->n);
    
    if (nav->n<=0) return;
    
    qsort(nav->eph,nav->n,sizeof(eph_t),cmpeph);
    
    for (i=1,j=0;i<nav->n;i++) {
        if (nav->eph[i].sat!=nav->eph[j].sat||
            nav->eph[i].iode!=nav->eph[j].iode||
            nav->eph[i].type!=nav->eph[j].type) {
            nav->eph[++j]=nav->eph[i];
        }
    }
    nav->n=j+1;
    
    if (!(nav_eph=(eph_t *)realloc(nav->eph,sizeof(eph_t)*nav->n))) {
        trace(1,"uniqeph malloc error n=%d\n",nav->n);
        free(nav->eph); nav->eph=NULL; nav->n=nav->nmax=0;
        return;
    }
    nav->eph=nav_eph;
    nav->nmax=nav->n;
    
    trace(4,"uniqeph: n=%d\n",nav->n);
}
/* compare glonass ephemeris -------------------------------------------------*/
static int cmpgeph(const void *p1, const void *p2)
{
    geph_t *q1=(geph_t *)p1,*q2=(geph_t *)p2;
    return q1->tof.time!=q2->tof.time?(int)(q1->tof.time-q2->tof.time):
           (q1->toe.time!=q2->toe.time?(int)(q1->toe.time-q2->toe.time):
            q1->sat-q2->sat);
}
/* sort and unique glonass ephemeris -----------------------------------------*/
static void uniqgeph(nav_t *nav)
{
    geph_t *nav_geph;
    int i,j;
    
    trace(3,"uniqgeph: ng=%d\n",nav->ng);
    
    if (nav->ng<=0) return;
    
    qsort(nav->geph,nav->ng,sizeof(geph_t),cmpgeph);
    
    for (i=j=0;i<nav->ng;i++) {
        if (nav->geph[i].sat!=nav->geph[j].sat||
            nav->geph[i].toe.time!=nav->geph[j].toe.time||
            nav->geph[i].svh!=nav->geph[j].svh) {
            nav->geph[++j]=nav->geph[i];
        }
    }
    nav->ng=j+1;
    
    if (!(nav_geph=(geph_t *)realloc(nav->geph,sizeof(geph_t)*nav->ng))) {
        trace(1,"uniqgeph malloc error ng=%d\n",nav->ng);
        free(nav->geph); nav->geph=NULL; nav->ng=nav->ngmax=0;
        return;
    }
    nav->geph=nav_geph;
    nav->ngmax=nav->ng;
    
    trace(4,"uniqgeph: ng=%d\n",nav->ng);
}
/* compare sbas ephemeris ----------------------------------------------------*/
static int cmpseph(const void *p1, const void *p2)
{
    seph_t *q1=(seph_t *)p1,*q2=(seph_t *)p2;
    return q1->tof.time!=q2->tof.time?(int)(q1->tof.time-q2->tof.time):
           (q1->t0.time!=q2->t0.time?(int)(q1->t0.time-q2->t0.time):
            q1->sat-q2->sat);
}
/* sort and unique sbas ephemeris --------------------------------------------*/
static void uniqseph(nav_t *nav)
{
    seph_t *nav_seph;
    int i,j;
    
    trace(3,"uniqseph: ns=%d\n",nav->ns);
    
    if (nav->ns<=0) return;
    
    qsort(nav->seph,nav->ns,sizeof(seph_t),cmpseph);
    
    for (i=j=0;i<nav->ns;i++) {
        if (nav->seph[i].sat!=nav->seph[j].sat||
            nav->seph[i].t0.time!=nav->seph[j].t0.time) {
            nav->seph[++j]=nav->seph[i];
        }
    }
    nav->ns=j+1;
    
    if (!(nav_seph=(seph_t *)realloc(nav->seph,sizeof(seph_t)*nav->ns))) {
        trace(1,"uniqseph malloc error ns=%d\n",nav->ns);
        free(nav->seph); nav->seph=NULL; nav->ns=nav->nsmax=0;
        return;
    }
    nav->seph=nav_seph;
    nav->nsmax=nav->ns;
    
    trace(4,"uniqseph: ns=%d\n",nav->ns);
}
/* unique ephemerides ----------------------------------------------------------
* unique ephemerides in navigation data
* args   : nav_t *nav    IO     navigation data
* return : number of epochs
*-----------------------------------------------------------------------------*/
extern void uniqnav(nav_t *nav)
{
    trace(3,"uniqnav: neph=%d ngeph=%d nseph=%d\n",nav->n,nav->ng,nav->ns);
    
    /* unique ephemeris */
    uniqeph (nav);
    uniqgeph(nav);
    uniqseph(nav);
}
/* compare observation data -------------------------------------------------*/
static int cmpobs(const void *p1, const void *p2)
{
    obsd_t *q1=(obsd_t *)p1,*q2=(obsd_t *)p2;
    double tt=timediff(q1->time,q2->time);
    if (fabs(tt)>DTTOL) return tt<0?-1:1;
    if (q1->rcv!=q2->rcv) return (int)q1->rcv-(int)q2->rcv;
    return (int)q1->sat-(int)q2->sat;
}
/* sort and unique observation data --------------------------------------------
* sort and unique observation data by time, rcv, sat
* args   : obs_t *obs    IO     observation data
* return : number of epochs
*-----------------------------------------------------------------------------*/
extern int sortobs(obs_t *obs)
{
    int i,j,n;
    
    trace(3,"sortobs: nobs=%d\n",obs->n);
    
    if (obs->n<=0) return 0;
    
    qsort(obs->data,obs->n,sizeof(obsd_t),cmpobs);
    
    /* delete duplicated data */
    for (i=j=0;i<obs->n;i++) {
        if (obs->data[i].sat!=obs->data[j].sat||
            obs->data[i].rcv!=obs->data[j].rcv||
            timediff(obs->data[i].time,obs->data[j].time)!=0.0) {
            obs->data[++j]=obs->data[i];
        }
    }
    obs->n=j+1;
    
    for (i=n=0;i<obs->n;i=j,n++) {
        for (j=i+1;j<obs->n;j++) {
            if (timediff(obs->data[j].time,obs->data[i].time)>DTTOL) break;
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
extern void signal_replace(obsd_t *obs, int idx, char f, char *c)
{
    int i,j;
    char *code;

    for(i=0;i<NFREQ+NEXOBS;i++){
        code=code2obs(obs->code[i]);
        for(j=0;c[j]!='\0';j++) if(code[0]==f && code[1]==c[j])break;
        if(c[j]!='\0')break;
    }
    if(i<NFREQ+NEXOBS) {
        obs->SNR[idx]=obs->SNR[i];obs->LLI[idx]=obs->LLI[i];obs->code[idx]=obs->code[i];
        obs->L[idx]  =obs->L[i];  obs->P[idx]  =obs->P[i];  obs->D[idx]   =obs->D[i];
    }
    else {
        obs->SNR[idx]=obs->LLI[idx]=obs->code[idx]=0;
        obs->P[idx]  =obs->L[idx]  =obs->D[idx]   =0.0;
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
extern int screent(gtime_t time, gtime_t ts, gtime_t te, double tint)
{
    return (tint<=0.0||fmod(time2gpst(time,NULL)+DTTOL,tint)<=DTTOL*2.0)&&
           (ts.time==0||timediff(time,ts)>=-DTTOL)&&
           (te.time==0||timediff(time,te)<  DTTOL);
}
/* read/save navigation data ---------------------------------------------------
* save or load navigation data
* args   : char    file  I      file path
*          nav_t   nav   O/I    navigation data
* return : status (1:ok,0:no file)
*-----------------------------------------------------------------------------*/
extern int readnav(const char *file, nav_t *nav)
{
    FILE *fp;
    eph_t eph0={0};
    geph_t geph0={0};
    char buff[4096],*p;
    long toe_time,tof_time,toc_time,ttr_time;
    int i,sat,prn;
    
    trace(3,"loadnav: file=%s\n",file);
    
    if (!(fp=fopen(file,"r"))) return 0;
    
    while (fgets(buff,sizeof(buff),fp)) {
        if (!strncmp(buff,"IONUTC",6)) {
            for (i=0;i<8;i++) nav->ion_gps[i]=0.0;
            for (i=0;i<8;i++) nav->utc_gps[i]=0.0;
            sscanf(buff,"IONUTC,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
                   &nav->ion_gps[0],&nav->ion_gps[1],&nav->ion_gps[2],&nav->ion_gps[3],
                   &nav->ion_gps[4],&nav->ion_gps[5],&nav->ion_gps[6],&nav->ion_gps[7],
                   &nav->utc_gps[0],&nav->utc_gps[1],&nav->utc_gps[2],&nav->utc_gps[3],
                   &nav->utc_gps[4]);
            continue;   
        }
        if ((p=strchr(buff,','))) *p='\0'; else continue;
        if (!(sat=satid2no(buff))) continue;
        if (satsys(sat,&prn)==SYS_GLO) {
            nav->geph[prn-1]=geph0;
            nav->geph[prn-1].sat=sat;
            toe_time=tof_time=0;
            sscanf(p+1,"%d,%d,%d,%d,%d,%ld,%ld,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,"
                        "%lf,%lf,%lf,%lf",
                   &nav->geph[prn-1].iode,&nav->geph[prn-1].frq,&nav->geph[prn-1].svh,
                   &nav->geph[prn-1].sva,&nav->geph[prn-1].age,
                   &toe_time,&tof_time,
                   &nav->geph[prn-1].pos[0],&nav->geph[prn-1].pos[1],&nav->geph[prn-1].pos[2],
                   &nav->geph[prn-1].vel[0],&nav->geph[prn-1].vel[1],&nav->geph[prn-1].vel[2],
                   &nav->geph[prn-1].acc[0],&nav->geph[prn-1].acc[1],&nav->geph[prn-1].acc[2],
                   &nav->geph[prn-1].taun  ,&nav->geph[prn-1].gamn  ,&nav->geph[prn-1].dtaun);
            nav->geph[prn-1].toe.time=toe_time;
            nav->geph[prn-1].tof.time=tof_time;
        }
        else {
            nav->eph[sat-1]=eph0;
            nav->eph[sat-1].sat=sat;
            toe_time=toc_time=ttr_time=0;
            sscanf(p+1,"%d,%d,%d,%d,%ld,%ld,%ld,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,"
                        "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%d,%d",
                   &nav->eph[sat-1].iode,&nav->eph[sat-1].iodc,&nav->eph[sat-1].sva ,
                   &nav->eph[sat-1].svh ,
                   &toe_time,&toc_time,&ttr_time,
                   &nav->eph[sat-1].A   ,&nav->eph[sat-1].e   ,&nav->eph[sat-1].i0  ,
                   &nav->eph[sat-1].OMG0,&nav->eph[sat-1].omg ,&nav->eph[sat-1].M0  ,
                   &nav->eph[sat-1].deln,&nav->eph[sat-1].OMGd,&nav->eph[sat-1].idot,
                   &nav->eph[sat-1].crc ,&nav->eph[sat-1].crs ,&nav->eph[sat-1].cuc ,
                   &nav->eph[sat-1].cus ,&nav->eph[sat-1].cic ,&nav->eph[sat-1].cis ,
                   &nav->eph[sat-1].toes,&nav->eph[sat-1].fit ,&nav->eph[sat-1].f0  ,
                   &nav->eph[sat-1].f1  ,&nav->eph[sat-1].f2  ,&nav->eph[sat-1].tgd[0],
                   &nav->eph[sat-1].code, &nav->eph[sat-1].flag);
            nav->eph[sat-1].toe.time=toe_time;
            nav->eph[sat-1].toc.time=toc_time;
            nav->eph[sat-1].ttr.time=ttr_time;
        }
    }
    fclose(fp);
    return 1;
}
extern int savenav(const char *file, const nav_t *nav)
{
    FILE *fp;
    int i;
    char id[32];
    
    trace(3,"savenav: file=%s\n",file);
    
    if (!(fp=fopen(file,"w"))) return 0;
    
    for (i=0;i<MAXSAT;i++) {
        if (nav->eph[i].ttr.time==0) continue;
        satno2id(nav->eph[i].sat,id);
        fprintf(fp,"%s,%d,%d,%d,%d,%d,%d,%d,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,"
                   "%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,"
                   "%.14E,%.14E,%.14E,%.14E,%.14E,%d,%d\n",
                id,nav->eph[i].iode,nav->eph[i].iodc,nav->eph[i].sva ,
                nav->eph[i].svh ,(int)nav->eph[i].toe.time,
                (int)nav->eph[i].toc.time,(int)nav->eph[i].ttr.time,
                nav->eph[i].A   ,nav->eph[i].e  ,nav->eph[i].i0  ,nav->eph[i].OMG0,
                nav->eph[i].omg ,nav->eph[i].M0 ,nav->eph[i].deln,nav->eph[i].OMGd,
                nav->eph[i].idot,nav->eph[i].crc,nav->eph[i].crs ,nav->eph[i].cuc ,
                nav->eph[i].cus ,nav->eph[i].cic,nav->eph[i].cis ,nav->eph[i].toes,
                nav->eph[i].fit ,nav->eph[i].f0 ,nav->eph[i].f1  ,nav->eph[i].f2  ,
                nav->eph[i].tgd[0],nav->eph[i].code,nav->eph[i].flag);
    }
    for (i=0;i<MAXPRNGLO;i++) {
        if (nav->geph[i].tof.time==0) continue;
        satno2id(nav->geph[i].sat,id);
        fprintf(fp,"%s,%d,%d,%d,%d,%d,%d,%d,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,"
                   "%.14E,%.14E,%.14E,%.14E,%.14E,%.14E\n",
                id,nav->geph[i].iode,nav->geph[i].frq,nav->geph[i].svh,
                nav->geph[i].sva,nav->geph[i].age,(int)nav->geph[i].toe.time,
                (int)nav->geph[i].tof.time,
                nav->geph[i].pos[0],nav->geph[i].pos[1],nav->geph[i].pos[2],
                nav->geph[i].vel[0],nav->geph[i].vel[1],nav->geph[i].vel[2],
                nav->geph[i].acc[0],nav->geph[i].acc[1],nav->geph[i].acc[2],
                nav->geph[i].taun,nav->geph[i].gamn,nav->geph[i].dtaun);
    }
    fprintf(fp,"IONUTC,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,"
               "%.14E,%.14E,%.14E,%.0f",
            nav->ion_gps[0],nav->ion_gps[1],nav->ion_gps[2],nav->ion_gps[3],
            nav->ion_gps[4],nav->ion_gps[5],nav->ion_gps[6],nav->ion_gps[7],
            nav->utc_gps[0],nav->utc_gps[1],nav->utc_gps[2],nav->utc_gps[3],
            nav->utc_gps[4]);
    
    fclose(fp);
    return 1;
}
/* free observation data -------------------------------------------------------
* free memory for observation data
* args   : obs_t *obs    IO     observation data
* return : none
*-----------------------------------------------------------------------------*/
extern void freeobs(obs_t *obs)
{
    free(obs->data); obs->data=NULL; obs->n=obs->nmax=0;
}
/* free navigation data ---------------------------------------------------------
* free memory for navigation data
* args   : nav_t *nav    IO     navigation data
*          int   opt     I      option (or of followings)
*                               (0x01: gps/qzs ephmeris, 0x02: glonass ephemeris,
*                                0x04: sbas ephemeris,   0x08: precise ephemeris,
*                                0x10: precise clock     0x20: almanac,
*                                0x40: tec data)
* return : none
*-----------------------------------------------------------------------------*/
extern void freenav(nav_t *nav, int opt)
{
    if (opt&0x01) {free(nav->eph ); nav->eph =NULL; nav->n =nav->nmax =0;}
    if (opt&0x02) {free(nav->geph); nav->geph=NULL; nav->ng=nav->ngmax=0;}
    if (opt&0x04) {free(nav->seph); nav->seph=NULL; nav->ns=nav->nsmax=0;}
    if (opt&0x08) {free(nav->peph); nav->peph=NULL; nav->ne=nav->nemax=0;}
    if (opt&0x10) {free(nav->pclk); nav->pclk=NULL; nav->nc=nav->ncmax=0;}
    if (opt&0x20) {free(nav->alm ); nav->alm =NULL; nav->na=nav->namax=0;}
    if (opt&0x40) {free(nav->tec ); nav->tec =NULL; nav->nt=nav->ntmax=0;}
}
/* debug trace functions -----------------------------------------------------*/

/*
 * Transitional bridge: legacy trace() → mrtk_log_v().
 *
 * The old file-I/O-based trace implementation (fp_trace, traceswap, etc.)
 * has been replaced by a thin wrapper that routes all output through the
 * new context-based logging system in mrtk_core.c.
 *
 * Call sites throughout src/ are *unchanged*; only the implementation here
 * has been replaced.
 */
#ifdef TRACE

#include "mrtklib/mrtklib.h"

/** @brief Tick time captured at traceopen, used by tracet() for elapsed time */
static uint32_t tick_trace=0;

/**
 * @brief Map RTKLIB trace level (1=most critical) to mrtk_log_level_t.
 *
 * RTKLIB levels:  1 (error) → 5 (verbose debug)
 * MRTK levels:    DEBUG(0) < INFO(1) < WARN(2) < ERROR(3) < NONE(4)
 */
static mrtk_log_level_t trace_level_to_mrtk(int level)
{
    switch (level) {
        case 1:  return MRTK_LOG_ERROR;
        case 2:  return MRTK_LOG_WARN;
        case 3:  return MRTK_LOG_INFO;
        default: return MRTK_LOG_DEBUG; /* levels 4, 5 */
    }
}
extern void traceopen(const char *file)
{
    (void)file;
    tick_trace=tickget();
}
extern void traceclose(void)
{
    /* no-op: file I/O removed */
}
extern void tracelevel(int level)
{
    mrtk_log_level_t mlevel;

    if (!g_mrtk_legacy_ctx) return;

    /* Map RTKLIB threshold to mrtk threshold.
     * RTKLIB level_trace=N means "show messages with level <= N".
     * RTKLIB level 1 = most critical, 5 = most verbose.
     * We map the threshold so that messages at the requested verbosity
     * and above are shown. */
    if      (level<=0) mlevel=MRTK_LOG_NONE;
    else if (level==1) mlevel=MRTK_LOG_ERROR;
    else if (level==2) mlevel=MRTK_LOG_WARN;
    else if (level==3) mlevel=MRTK_LOG_INFO;
    else               mlevel=MRTK_LOG_DEBUG;

    mrtk_context_set_log_level(g_mrtk_legacy_ctx, mlevel);
}
extern void trace(int level, const char *format, ...)
{
    va_list ap;

    if (!g_mrtk_legacy_ctx) return;

    va_start(ap, format);
    mrtk_log_v(g_mrtk_legacy_ctx, trace_level_to_mrtk(level), format, ap);
    va_end(ap);
}
extern void tracet(int level, const char *format, ...)
{
    va_list ap;
    char buf[1024];
    int  off;

    if (!g_mrtk_legacy_ctx) return;

    /* Prepend elapsed time (seconds since traceopen) */
    off=snprintf(buf, sizeof(buf), "%d %9.3f: ", level,
                 (tickget()-tick_trace)/1000.0);
    if (off<0) off=0;

    va_start(ap, format);
    vsnprintf(buf+off, sizeof(buf)-(size_t)off, format, ap);
    va_end(ap);

    mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level), "%s", buf);
}
extern void tracemat(int level, const double *A, int n, int m, int p, int q)
{
    char buf[1024];
    int i,j,off;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<n;i++) {
        off=0;
        for (j=0;j<m;j++) {
            off+=snprintf(buf+off, sizeof(buf)-(size_t)off, " %*.*f", p, q,
                          A[i+j*n]);
            if (off>=(int)sizeof(buf)) break;
        }
        mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level), "%s", buf);
    }
}
extern void traceobs(int level, const obsd_t *obs, int n)
{
    char str[64],id[16];
    int i;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<n;i++) {
        time2str(obs[i].time,str,3);
        satno2id(obs[i].sat,id);
        mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                 " (%2d) %s %-3s rcv%d %13.3f %13.3f %13.3f %13.3f"
                 " %d %d %d %d %3.1f %3.1f",
                 i+1,str,id,obs[i].rcv,obs[i].L[0],obs[i].L[1],obs[i].P[0],
                 obs[i].P[1],obs[i].LLI[0],obs[i].LLI[1],obs[i].code[0],
                 obs[i].code[1],obs[i].SNR[0]*SNR_UNIT,obs[i].SNR[1]*SNR_UNIT);
    }
}
extern void tracenav(int level, const nav_t *nav)
{
    char s1[64],s2[64],id[16];
    int i;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<nav->n;i++) {
        time2str(nav->eph[i].toe,s1,0);
        time2str(nav->eph[i].ttr,s2,0);
        satno2id(nav->eph[i].sat,id);
        mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                 "(%3d) %-3s : %s %s %3d %3d %02x",
                 i+1,id,s1,s2,nav->eph[i].iode,nav->eph[i].iodc,
                 nav->eph[i].svh);
    }
    mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
             "(ion) %9.4e %9.4e %9.4e %9.4e",
             nav->ion_gps[0],nav->ion_gps[1],nav->ion_gps[2],nav->ion_gps[3]);
    mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
             "(ion) %9.4e %9.4e %9.4e %9.4e",
             nav->ion_gps[4],nav->ion_gps[5],nav->ion_gps[6],nav->ion_gps[7]);
    mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
             "(ion) %9.4e %9.4e %9.4e %9.4e",
             nav->ion_gal[0],nav->ion_gal[1],nav->ion_gal[2],nav->ion_gal[3]);
}
extern void tracegnav(int level, const nav_t *nav)
{
    char s1[64],s2[64],id[16];
    int i;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<nav->ng;i++) {
        time2str(nav->geph[i].toe,s1,0);
        time2str(nav->geph[i].tof,s2,0);
        satno2id(nav->geph[i].sat,id);
        mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                 "(%3d) %-3s : %s %s %2d %2d %8.3f",
                 i+1,id,s1,s2,nav->geph[i].frq,nav->geph[i].svh,
                 nav->geph[i].taun*1E6);
    }
}
extern void tracehnav(int level, const nav_t *nav)
{
    char s1[64],s2[64],id[16];
    int i;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<nav->ns;i++) {
        time2str(nav->seph[i].t0,s1,0);
        time2str(nav->seph[i].tof,s2,0);
        satno2id(nav->seph[i].sat,id);
        mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                 "(%3d) %-3s : %s %s %2d %2d",
                 i+1,id,s1,s2,nav->seph[i].svh,nav->seph[i].sva);
    }
}
extern void tracepeph(int level, const nav_t *nav)
{
    char s[64],id[16];
    int i,j;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<nav->ne;i++) {
        time2str(nav->peph[i].time,s,0);
        for (j=0;j<MAXSAT;j++) {
            satno2id(j+1,id);
            mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                     "%-3s %d %-3s %13.3f %13.3f %13.3f %13.3f"
                     " %6.3f %6.3f %6.3f %6.3f",
                     s,nav->peph[i].index,id,
                     nav->peph[i].pos[j][0],nav->peph[i].pos[j][1],
                     nav->peph[i].pos[j][2],nav->peph[i].pos[j][3]*1E9,
                     nav->peph[i].std[j][0],nav->peph[i].std[j][1],
                     nav->peph[i].std[j][2],nav->peph[i].std[j][3]*1E9);
        }
    }
}
extern void tracepclk(int level, const nav_t *nav)
{
    char s[64],id[16];
    int i,j;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<nav->nc;i++) {
        time2str(nav->pclk[i].time,s,0);
        for (j=0;j<MAXSAT;j++) {
            satno2id(j+1,id);
            mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                     "%-3s %d %-3s %13.3f %6.3f",
                     s,nav->pclk[i].index,id,
                     nav->pclk[i].clk[j][0]*1E9,nav->pclk[i].std[j][0]*1E9);
        }
    }
}
extern void traceb(int level, const uint8_t *p, int n)
{
    char buf[1024];
    int i,off=0;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<n;i++) {
        off+=snprintf(buf+off, sizeof(buf)-(size_t)off, "%02X%s",
                      *p++, i%8==7?" ":"");
        if (off>=(int)sizeof(buf)) break;
    }
    mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level), "%s", buf);
}
#else
extern void traceopen(const char *file) {}
extern void traceclose(void) {}
extern void tracelevel(int level) {}
extern void trace   (int level, const char *format, ...) {}
extern void tracet  (int level, const char *format, ...) {}
extern void tracemat(int level, const double *A, int n, int m, int p, int q) {}
extern void traceobs(int level, const obsd_t *obs, int n) {}
extern void tracenav(int level, const nav_t *nav) {}
extern void tracegnav(int level, const nav_t *nav) {}
extern void tracehnav(int level, const nav_t *nav) {}
extern void tracepeph(int level, const nav_t *nav) {}
extern void tracepclk(int level, const nav_t *nav) {}
extern void traceb  (int level, const uint8_t *p, int n) {}

#endif /* TRACE */

/* execcmd, expath, mkdir_r, createdir, repstr, reppath, reppaths moved to mrtk_sys.c */

/* geodist, satazel, dops moved to mrtk_coords.c */

/* ionmodel, ionmapf, ionppp, tropmodel, tropmapf moved to mrtk_atmos.c */

/* interpvar, antmodel, antmodel_s moved to mrtk_antenna.c */
/* ast_args, sunmoonpos_eci, sunmoonpos moved to mrtk_astro.c */
/* rtk_uncompress moved to mrtk_sys.c */
/* dummy application functions for shared library ----------------------------*/
#ifdef WIN_DLL
extern int showmsg(char *format,...) {return 0;}
extern void settspan(gtime_t ts, gtime_t te) {}
extern void settime(gtime_t time) {}
#endif

