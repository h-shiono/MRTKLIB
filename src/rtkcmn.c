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
/* prcopt_default, solopt_default moved to mrtk_opt.c */
/* formatstrs[] moved to mrtk_stream.c */
/* obscodes[], codepris[][] moved to mrtk_obs.c */
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

/* satno, satsys, satid2no, satno2id moved to mrtk_nav.c */
/* satexclude moved to mrtk_nav.c */
/* testsnr, testelmask, obs2code, code2obs, code2freq_*, code2idx, code2freq,
 * sat2freq, setcodepri, getcodepri moved to mrtk_obs.c */
/* getbitu, getbits, setbitu, setbits, rtk_crc32, rtk_crc24q, rtk_crc16, decode_word
 * moved to mrtk_bits.c */

/* str2num moved to mrtk_sys.c */

/* coordinate functions moved to mrtk_coords.c */

/* decodef, addpcv, readngspcv, readantex, readpcv, searchpcv moved to mrtk_antenna.c */
/* readelmask, readpos, readblqrecord, readblq moved to mrtk_station.c */
/* readerp, geterp moved to mrtk_peph.c */

/* cmpeph, uniqeph, cmpgeph, uniqgeph, cmpseph, uniqseph, uniqnav moved to mrtk_nav.c */
/* cmpobs, sortobs, signal_replace, screent moved to mrtk_obs.c */
/* readnav, savenav moved to mrtk_nav.c */
/* freeobs moved to mrtk_obs.c */
/* freenav moved to mrtk_nav.c */
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

