/*------------------------------------------------------------------------------
 * mrtk_obs.c : observation data functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_obs.c
 * @brief MRTKLIB Observation Module — Implementation.
 *
 * All functions and static data in this file are cut-and-paste extractions
 * from the legacy rtkcmn.c with zero algorithmic changes.
 */

#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mrtklib/mrtk_trace.h"

/*============================================================================
 * Forward Declarations (resolved at link time)
 *===========================================================================*/

/* satsys remains in mrtk_nav.c */
extern int satsys(int sat, int *prn);

/* timediff, time2gpst remain in mrtk_time.c */
extern double timediff(gtime_t t1, gtime_t t2);
extern double time2gpst(gtime_t t, int *week);

/*============================================================================
 * Private Constants
 *===========================================================================*/

#define PI          3.1415926535897932  /* pi */
#define R2D         (180.0/PI)         /* rad to deg */

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

/*============================================================================
 * Static Data Arrays
 *===========================================================================*/

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

/*============================================================================
 * Private Functions
 *===========================================================================*/

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
/* compare observation data -------------------------------------------------*/
static int cmpobs(const void *p1, const void *p2)
{
    obsd_t *q1=(obsd_t *)p1,*q2=(obsd_t *)p2;
    double tt=timediff(q1->time,q2->time);
    if (fabs(tt)>DTTOL) return tt<0?-1:1;
    if (q1->rcv!=q2->rcv) return (int)q1->rcv-(int)q2->rcv;
    return (int)q1->sat-(int)q2->sat;
}

/*============================================================================
 * Public Functions
 *===========================================================================*/

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
    trace(NULL,3,"setcodepri:sys=%d idx=%d pri=%s\n",sys,idx,pri);

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
/* sort and unique observation data --------------------------------------------
* sort and unique observation data by time, rcv, sat
* args   : obs_t *obs    IO     observation data
* return : number of epochs
*-----------------------------------------------------------------------------*/
extern int sortobs(obs_t *obs)
{
    int i,j,n;

    trace(NULL,3,"sortobs: nobs=%d\n",obs->n);

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
/* free observation data -------------------------------------------------------
* free memory for observation data
* args   : obs_t *obs    IO     observation data
* return : none
*-----------------------------------------------------------------------------*/
extern void freeobs(obs_t *obs)
{
    free(obs->data); obs->data=NULL; obs->n=obs->nmax=0;
}
