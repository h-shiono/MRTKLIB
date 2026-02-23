/*------------------------------------------------------------------------------
 * mrtk_rcv_skytraq.c : SkyTraq receiver raw data decoder
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_rcvraw.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_sys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "mrtklib/mrtk_trace.h"

/* Local constants (duplicated from rtklib.h to avoid dependency) */
#define PI          3.1415926535897932  /* pi */
#define D2R         (PI/180.0)         /* deg to rad */
#define R2D         (180.0/PI)         /* rad to deg */
#define CLIGHT      299792458.0        /* speed of light (m/s) */
#define SC2RAD      3.1415926535898    /* semi-circle to radian (IS-GPS) */

#define SYS_NONE    0x00
#define SYS_GPS     0x01
#define SYS_SBS     0x02
#define SYS_GLO     0x04
#define SYS_GAL     0x08
#define SYS_QZS     0x10
#define SYS_CMP     0x20
#define SYS_IRN     0x40

#define TSYS_GPS    0
#define TSYS_UTC    1
#define TSYS_GLO    2
#define TSYS_GAL    3
#define TSYS_QZS    4
#define TSYS_CMP    5
#define TSYS_IRN    6

#define FREQ1       1.57542E9          /* L1/E1/B1C  frequency (Hz) */
#define FREQ2       1.22760E9          /* L2         frequency (Hz) */
#define FREQ5       1.17645E9          /* L5/E5a/B2a frequency (Hz) */
#define FREQ6       1.27875E9          /* E6/L6      frequency (Hz) */
#define FREQ7       1.20714E9          /* E5b/B2I    frequency (Hz) */
#define FREQ8       1.191795E9         /* E5ab/B2ab  frequency (Hz) */
#define FREQ9       2.492028E9         /* S          frequency (Hz) */
#define FREQ1_GLO   1.60200E9          /* GLONASS G1 base frequency (Hz) */
#define DFRQ1_GLO   0.56250E6          /* GLONASS G1 bias frequency (Hz/n) */
#define FREQ2_GLO   1.24600E9          /* GLONASS G2 base frequency (Hz) */
#define DFRQ2_GLO   0.43750E6          /* GLONASS G2 bias frequency (Hz/n) */
#define FREQ3_GLO   1.202025E9         /* GLONASS G3 frequency (Hz) */
#define FREQ1a_GLO  1.600995E9         /* GLONASS G1a frequency (Hz) */
#define FREQ2a_GLO  1.248060E9         /* GLONASS G2a frequency (Hz) */
#define FREQ1_CMP   1.561098E9         /* BDS B1I    frequency (Hz) */

#define MINPRNGPS   1
#define MAXPRNGPS   32
#define MINPRNSBS   120
#define MAXPRNSBS   158

/* Power-of-2 constants */
#define P2_5        0.03125
#define P2_6        0.015625
#define P2_11       4.882812500000000E-04
#define P2_15       3.051757812500000E-05
#define P2_17       7.629394531250000E-06
#define P2_19       1.907348632812500E-06
#define P2_20       9.536743164062500E-07
#define P2_21       4.768371582031250E-07
#define P2_23       1.192092895507810E-07
#define P2_24       5.960464477539063E-08
#define P2_27       7.450580596923828E-09
#define P2_29       1.862645149230957E-09
#define P2_30       9.313225746154785E-10
#define P2_31       4.656612873077393E-10
#define P2_32       2.328306436538696E-10
#define P2_33       1.164153218269348E-10
#define P2_34       5.820766091346740E-11
#define P2_35       2.910383045673370E-11
#define P2_38       3.637978807091710E-12
#define P2_40       9.094947017729280E-13
#define P2_43       1.136868377216160E-13
#define P2_46       1.421085471520200E-14
#define P2_50       8.881784197001252E-16
#define P2_55       2.775557561562891E-17
#define P2_59       1.734723475976810E-18

/* Forward declarations for functions in rtklib (resolved at link time) */

#define STQSYNC1    0xA0        /* skytraq binary sync code 1 */
#define STQSYNC2    0xA1        /* skytraq binary sync code 2 */

#define ID_STQTIME  0xDC        /* skytraq message id: measurement epoch */
#define ID_STQRAW   0xDD        /* skytraq message id: raw measurement */
#define ID_STQSVCH  0xDE        /* skytraq message id: SV and channel status */
#define ID_STQSTAT  0xDF        /* skytraq message id: navigation status */
#define ID_STQGPS   0xE0        /* skytraq message id: gps/qzs subframe */
#define ID_STQGLO   0xE1        /* skytraq message id: glonass string */
#define ID_STQBDSD1 0xE2        /* skytraq message id: beidou d1 subframe */
#define ID_STQBDSD2 0xE3        /* skytraq message id: beidou d2 subframe */
#define ID_STQRAWX  0xE5        /* skytraq message id: extended raw meas v.1 */
#define ID_STQGLOE  0x5C        /* skytraq message id: glonass ephemeris */
#define ID_STQACK   0x83        /* skytraq message id: ack to request msg */
#define ID_STQNACK  0x84        /* skytraq message id: nack to request msg */

#define ID_RESTART  0x01        /* skytraq message id: system restart */
#define ID_CFGSERI  0x05        /* skytraq message id: configure serial port */
#define ID_CFGFMT   0x09        /* skytraq message id: configure message format */
#define ID_CFGRATE  0x12        /* skytraq message id: configure message rate */
#define ID_CFGBIN   0x1E        /* skytraq message id: configure binary message */
#define ID_GETGLOEPH 0x5B       /* skytraq message id: get glonass ephemeris */

/* extract field (big-endian) ------------------------------------------------*/
#define U1(p)       (*((uint8_t *)(p)))
#define I1(p)       (*((int8_t  *)(p)))

static uint16_t U2(uint8_t *p)
{
    uint16_t value;
    uint8_t *q=(uint8_t *)&value+1;
    int i;
    for (i=0;i<2;i++) *q--=*p++;
    return value;
}
static uint32_t U4(uint8_t *p)
{
    uint32_t value;
    uint8_t *q=(uint8_t *)&value+3;
    int i;
    for (i=0;i<4;i++) *q--=*p++;
    return value;
}
static float R4(uint8_t *p)
{
    float value;
    uint8_t *q=(uint8_t *)&value+3;
    int i;
    for (i=0;i<4;i++) *q--=*p++;
    return value;
}
static double R8(uint8_t *p)
{
    double value;
    uint8_t *q=(uint8_t *)&value+7;
    int i;
    for (i=0;i<8;i++) *q--=*p++;
    return value;
}
/* checksum ------------------------------------------------------------------*/
static uint8_t checksum(uint8_t *buff, int len)
{
    uint8_t cs=0;
    int i;
    
    for (i=4;i<len-3;i++) {
        cs^=buff[i];
    }
    return cs;
}
/* 8-bit week -> full week ---------------------------------------------------*/
static void adj_utcweek(gtime_t time, double *utc)
{
    int week;
    
    if (utc[3]>=256.0) return;
    time2gpst(time,&week);
    utc[3]+=week/256*256;
    if      (utc[3]<week-128) utc[3]+=256.0;
    else if (utc[3]>week+128) utc[3]-=256.0;
}
/* decode skytraq measurement epoch (0xDC) -----------------------------------*/
static int decode_stqtime(raw_t *raw)
{
    uint8_t *p=raw->buff+4;
    double tow;
    int week;
    
    trace(NULL,4,"decode_stqtime: len=%d\n",raw->len);
    
    raw->iod=U1(p+1);
    week    =U2(p+2);
    week    =adjgpsweek(week);
    tow     =U4(p+4)*0.001;
    raw->time=gpst2time(week,tow);
    
    if (raw->outtype) {
        sprintf(raw->msgtype,"SKYTRAQ EPOCH (%4d): iod=%d week=%d tow=%.3f",
                raw->len,raw->iod,week,tow);
    }
    return 0;
}
/* decode skytraq raw measurement (0xDD) -------------------------------------*/
static int decode_stqraw(raw_t *raw)
{
    uint8_t *p=raw->buff+4,ind;
    double pr1,cp1;
    int i,j,iod,prn,sys,sat,n=0,nsat;
    
    trace(NULL,4,"decode_stqraw: len=%d\n",raw->len);
    
    if (raw->outtype) {
        sprintf(raw->msgtype,"SKYTRAQ RAW   (%4d): nsat=%d",raw->len,U1(p+2));
    }
    iod=U1(p+1);
    if (iod!=raw->iod) { /* need preceding measurement epoch (0xDC) */
        trace(NULL,2,"stq raw iod error: iod=%d %d\n",iod,raw->iod);
        return -1;
    }
    nsat=U1(p+2);
    if (raw->len<8+23*nsat) {
        trace(NULL,2,"stq raw length error: len=%d nsat=%d\n",raw->len,nsat);
        return -1;
    }
    for (i=0,p+=3;i<nsat&&i<MAXOBS;i++,p+=23) {
        prn=U1(p);
        
        if (MINPRNGPS<=prn&&prn<=MAXPRNGPS) {
            sys=SYS_GPS;
        }
        else if (MINPRNGLO<=prn-64&&prn-64<=MAXPRNGLO) {
            sys=SYS_GLO;
            prn-=64;
        }
        else if (MINPRNQZS<=prn&&prn<=MAXPRNQZS) {
            sys=SYS_QZS;
        }
        else if (MINPRNCMP<=prn-200&&prn-200<=MAXPRNCMP) {
            sys=SYS_CMP;
            prn-=200;
        }
        else {
            trace(NULL,2,"stq raw satellite number error: prn=%d\n",prn);
            continue;
        }
        if (!(sat=satno(sys,prn))) {
            trace(NULL,2,"stq raw satellite number error: sys=%d prn=%d\n",sys,prn);
            continue;
        }
        ind=U1(p+22);
        pr1=!(ind&1)?0.0:R8(p+ 2);
        cp1=!(ind&4)?0.0:R8(p+10);
        cp1-=floor((cp1+1E9)/2E9)*2E9; /* -10^9 < cp1 < 10^9 */
        
        raw->obs.data[n].P[0]=pr1;
        raw->obs.data[n].L[0]=cp1;
        raw->obs.data[n].D[0]=!(ind&2)?0.0:R4(p+18);
        raw->obs.data[n].SNR[0]=(uint16_t)(U1(p+1)/SNR_UNIT+0.5);
        raw->obs.data[n].LLI[0]=0;
        raw->obs.data[n].code[0]=sys==SYS_CMP?CODE_L2I:CODE_L1C;
        
        raw->lockt[sat-1][0]=ind&8?1:0; /* cycle slip */
        
        if (raw->obs.data[n].L[0]!=0.0) {
            raw->obs.data[n].LLI[0]=(uint8_t)raw->lockt[sat-1][0];
            raw->lockt[sat-1][0]=0;
        }
        /* receiver dependent options */
        if (strstr(raw->opt,"-INVCP")) {
            raw->obs.data[n].L[0]*=-1.0;
        }
        raw->obs.data[n].time=raw->time;
        raw->obs.data[n].sat =sat;
        
        for (j=1;j<NFREQ+NEXOBS;j++) {
            raw->obs.data[n].L[j]=raw->obs.data[n].P[j]=0.0;
            raw->obs.data[n].D[j]=0.0;
            raw->obs.data[n].SNR[j]=raw->obs.data[n].LLI[j]=0;
            raw->obs.data[n].code[j]=CODE_NONE;
        }
        n++;
    }
    raw->obs.n=n;
    return n>0?1:0;
}
/* decode skytraq extended raw measurement data v.1 (0xE5) -------------------*/
static int decode_stqrawx(raw_t *raw)
{
    uint8_t *p=raw->buff+4,ind;
    double tow,peri,pr1,cp1;
    int i,j,ver,week,nsat,sys,sig,prn,sat,n=0;
    int gnss_type, signal_type;
    
    trace(NULL,4,"decode_stqraw: len=%d\n",raw->len);
    
    if (raw->outtype) {
        sprintf(raw->msgtype,"SKYTRAQ RAWX  (%4d): nsat=%2d",raw->len,U1(p+13));
    }
    ver=U1(p+1);
    raw->iod=U1(p+2);
    week=U2(p+3);
    week=adjgpsweek(week);
    tow =U4(p+5)*0.001;
    raw->time=gpst2time(week,tow);
    peri=U2(p+9)*0.001;
    nsat=U1(p+13);
    if (raw->len<19+31*nsat) {
        trace(NULL,2,"stq raw length error: len=%d nsat=%d\n",raw->len,nsat);
        return -1;
    }
    for (i=0,p+=14;i<nsat&&i<MAXOBS;i++,p+=31) {
        gnss_type=U1(p)&0xF;
        signal_type=(U1(p)>>4)&0xF;
        if (gnss_type==0) { /* GPS */
            sys=SYS_GPS;
            switch (signal_type) {
                case  1: sig=CODE_L1X; break;
                case  2: sig=CODE_L2X; break;
                case  4: sig=CODE_L5X; break;
                default: sig=CODE_L1C; break;
            }
            prn=U1(p+1);
        }
        else if (gnss_type==1) { /* SBAS */
            sys=SYS_SBS;
            sig=CODE_L1C;
            prn=U1(p+1);
        }
        else if (gnss_type==2) { /* GLONASS */
            sys=SYS_GLO;
            switch (signal_type) {
                case  2: sig=CODE_L2C; break;
                case  4: sig=CODE_L3X; break;
                default: sig=CODE_L1C; break;
            }
            prn=U1(p+1);
        }
        else if (gnss_type==3) { /* Galileo */
            sys=SYS_GAL;
            switch (signal_type) {
                case  4: sig=CODE_L5X; break;
                case  5: sig=CODE_L7X; break;
                case  6: sig=CODE_L6X; break;
                default: sig=CODE_L1C; break;
            }
            prn=U1(p+1);
        }
        else if (gnss_type==4) { /* QZSS */
            sys=SYS_QZS;
            switch (signal_type) {
                case  1: sig=CODE_L1X; break;
                case  2: sig=CODE_L2X; break;
                case  4: sig=CODE_L5X; break;
                case  6: sig=CODE_L6X; break;
                default: sig=CODE_L1C; break;
            }
            prn=U1(p+1);
        }
        else if (gnss_type==5) { /* BeiDou */
            sys=SYS_CMP;
            switch (signal_type) {
                case  4: sig=CODE_L7I; break;
                case  6: sig=CODE_L6I; break;
                default: sig=CODE_L2I; break;
            }
            prn=U1(p+1);
        }
        else {
            trace(NULL,2,"stq rawx gnss type error: type=%d\n",U1(p));
            continue;
        }
        if (!(sat=satno(sys,prn))) {
            trace(NULL,2,"stq raw satellite number error: sys=%d prn=%d\n",sys,prn);
            continue;
        }
        /* set glonass freq channel number */
        if (gnss_type==2) {
            raw->nav.geph[prn-1].frq=(int)(U1(p+2)&0xF)-7;
        }
        ind=U2(p+27);
        pr1=!(ind&1)?0.0:R8(p+ 4);
        cp1=!(ind&4)?0.0:R8(p+12);
        cp1-=floor((cp1+1E9)/2E9)*2E9; /* -10^9 < cp1 < 10^9 */
        
        raw->obs.data[n].P[0]=pr1;
        raw->obs.data[n].L[0]=cp1;
        raw->obs.data[n].D[0]=!(ind&2)?0.0:R4(p+20);
        raw->obs.data[n].SNR[0]=(uint16_t)(U1(p+3)/SNR_UNIT+0.5);
        raw->obs.data[n].LLI[0]=0;
        raw->obs.data[n].code[0]=sys==SYS_CMP?CODE_L2I:CODE_L1C;
        
        raw->lockt[sat-1][0]=ind&8?1:0; /* cycle slip */
        
        if (raw->obs.data[n].L[0]!=0.0) {
            raw->obs.data[n].LLI[0]=(uint8_t)raw->lockt[sat-1][0];
            raw->lockt[sat-1][0]=0;
        }
        /* receiver dependent options */
        if (strstr(raw->opt,"-INVCP")) {
            raw->obs.data[n].L[0]*=-1.0;
        }
        raw->obs.data[n].time=raw->time;
        raw->obs.data[n].sat =sat;
        
        for (j=1;j<NFREQ+NEXOBS;j++) {
            raw->obs.data[n].L[j]=raw->obs.data[n].P[j]=0.0;
            raw->obs.data[n].D[j]=0.0;
            raw->obs.data[n].SNR[j]=raw->obs.data[n].LLI[j]=0;
            raw->obs.data[n].code[j]=CODE_NONE;
        }
        n++;
    }
    raw->obs.n=n;
    return n>0?1:0;
}
/* save subframe -------------------------------------------------------------*/
static int save_subfrm(int sat, raw_t *raw)
{
    uint8_t *p=raw->buff+7,*q;
    int i,id;
    
    trace(NULL,4,"save_subfrm: sat=%2d\n",sat);
    
    /* check navigation subframe preamble */
    if (p[0]!=0x8B) {
        trace(NULL,2,"stq subframe preamble error: 0x%02X\n",p[0]);
        return 0;
    }
    id=(p[5]>>2)&0x7;
    
    /* check subframe id */
    if (id<1||5<id) {
        trace(NULL,2,"stq subframe id error: id=%d\n",id);
        return 0;
    }
    q=raw->subfrm[sat-1]+(id-1)*30;
    
    for (i=0;i<30;i++) q[i]=p[i];
    
    return id;
}
/* decode ephemeris ----------------------------------------------------------*/
static int decode_ephem(int sat, raw_t *raw)
{
    eph_t eph={0};
    
    trace(NULL,4,"decode_ephem: sat=%2d\n",sat);
    
    if (!decode_frame(raw->subfrm[sat-1],&eph,NULL,NULL,NULL)) return 0;
    
    if (!strstr(raw->opt,"-EPHALL")) {
        if (eph.iode==raw->nav.eph[sat-1].iode&&
            eph.iodc==raw->nav.eph[sat-1].iodc) return 0; /* unchanged */
    }
    eph.sat=sat;
    raw->nav.eph[sat-1]=eph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* decode almanac and ion/utc ------------------------------------------------*/
static int decode_alm1(int sat, raw_t *raw)
{
    int sys=satsys(sat,NULL);
    
    trace(NULL,4,"decode_alm1 : sat=%2d\n",sat);
    
    if (sys==SYS_GPS) {
        decode_frame(raw->subfrm[sat-1],NULL,raw->nav.alm,raw->nav.ion_gps,
                     raw->nav.utc_gps);
        adj_utcweek(raw->time,raw->nav.utc_gps);
    }
    else if (sys==SYS_QZS) {
        decode_frame(raw->subfrm[sat-1],NULL,raw->nav.alm,raw->nav.ion_qzs,
                     raw->nav.utc_qzs);
        adj_utcweek(raw->time,raw->nav.utc_qzs);
    }
    return 9;
}
/* decode almanac ------------------------------------------------------------*/
static int decode_alm2(int sat, raw_t *raw)
{
    int sys=satsys(sat,NULL);
    
    trace(NULL,4,"decode_alm2 : sat=%2d\n",sat);
    
    if (sys==SYS_GPS) {
        decode_frame(raw->subfrm[sat-1],NULL,raw->nav.alm,NULL,NULL);
    }
    else if (sys==SYS_QZS) {
        decode_frame(raw->subfrm[sat-1],NULL,raw->nav.alm,raw->nav.ion_qzs,
                     raw->nav.utc_qzs);
        adj_utcweek(raw->time,raw->nav.utc_qzs);
    }
    return  0;
}
/* decode gps/qzss subframe (0xE0) -------------------------------------------*/
static int decode_stqgps(raw_t *raw)
{
    int prn,sat,id;
    uint8_t *p=raw->buff+4;
    
    trace(NULL,4,"decode_stqgps: len=%d\n",raw->len);
    
    if (raw->len<40) {
        trace(NULL,2,"stq gps/qzss subframe length error: len=%d\n",raw->len);
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype,"SKYTRAQ GPSSF (%4d): prn=%2d id=%d",raw->len,
                U1(p+1),(p[8]>>2)&0x7);
    }
    prn=U1(p+1);
    if (!(sat=satno(MINPRNQZS<=prn&&prn<=MAXPRNQZS?SYS_QZS:SYS_GPS,prn))) {
        trace(NULL,2,"stq gps/qzss subframe satellite number error: prn=%d\n",prn);
        return -1;
    }
    id=save_subfrm(sat,raw);
    if (id==3) return decode_ephem(sat,raw);
    if (id==4) return decode_alm1 (sat,raw);
    if (id==5) return decode_alm2 (sat,raw);
    return 0;
}
/* decode glonass string (0xE1) ----------------------------------------------*/
static int decode_stqglo(raw_t *raw)
{
    geph_t geph={0};
    int i,prn,sat,m;
    uint8_t *p=raw->buff+4;
    
    trace(NULL,4,"decode_stqglo: len=%d\n",raw->len);
    
    if (raw->len<19) {
        trace(NULL,2,"stq glo string length error: len=%d\n",raw->len);
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype,"SKYTRAQ GLSTR (%4d): prn=%2d no=%d",raw->len,
                U1(p+1)-64,U1(p+2));
    }
    prn=U1(p+1)-64;
    if (!(sat=satno(SYS_GLO,prn))) {
        trace(NULL,2,"stq glo string satellite number error: prn=%d\n",prn);
        return -1;
    }
    m=U1(p+2); /* string number */
    if (m<1||4<m) {
        return 0; /* non-immediate info and almanac */
    }
    setbitu(raw->subfrm[sat-1]+(m-1)*10,1,4,m);
    for (i=0;i<9;i++) {
        setbitu(raw->subfrm[sat-1]+(m-1)*10,5+i*8,8,p[3+i]);
    }
    if (m!=4) return 0;
    
    /* decode glonass ephemeris strings */
    geph.tof=raw->time;
    if (!decode_glostr(raw->subfrm[sat-1],&geph,NULL)||geph.sat!=sat) return 0;
    
    if (!strstr(raw->opt,"-EPHALL")) {
        if (geph.iode==raw->nav.geph[prn-1].iode) return 0; /* unchanged */
    }
    /* keep freq channel number */
    geph.frq=raw->nav.geph[prn-1].frq;
    raw->nav.geph[prn-1]=geph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* decode glonass string (requested) (0x5C) ----------------------------------*/
static int decode_stqgloe(raw_t *raw)
{
    int prn,sat;
    uint8_t *p=raw->buff+4;
    
    trace(NULL,4,"decode_stqgloe: len=%d\n",raw->len);
    
    if (raw->len<50) {
        trace(NULL,2,"stq glo string length error: len=%d\n",raw->len);
        return -1;
    }
    prn=U1(p+1);
    if (!(sat=satno(SYS_GLO,prn))) {
        trace(NULL,2,"stq gloe string satellite number error: prn=%d\n",prn);
        return -1;
    }
    /* set frequency channel number */
    raw->nav.geph[prn-1].frq=I1(p+2);
    
    return 0;
}
/* decode beidou subframe (0xE2,0xE3) ----------------------------------------*/
static int decode_stqbds(raw_t *raw)
{
    eph_t eph={0};
    uint32_t word;
    int i,j=0,id,pgn,prn,sat;
    uint8_t *p=raw->buff+4;
    
    trace(NULL,4,"decode_stqbds: len=%d\n",raw->len);
    
    if (raw->len<38) {
        trace(NULL,2,"stq bds subframe length error: len=%d\n",raw->len);
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype,"SKYTRAQ BDSSF (%4d): prn=%2d id=%d",raw->len,
                U1(p+1)-200,U1(p+2));
    }
    prn=U1(p+1)-200;
    if (!(sat=satno(SYS_CMP,prn))) {
        trace(NULL,2,"stq bds subframe satellite number error: prn=%d\n",prn);
        return -1;
    }
    id=U1(p+2); /* subframe id */
    if (id<1||5<id) {
        trace(NULL,2,"stq bds subframe id error: prn=%2d\n",prn);
        return -1;
    }
    if (prn>5) { /* IGSO/MEO */
        word=getbitu(p+3,j,26)<<4; j+=26;
        setbitu(raw->subfrm[sat-1]+(id-1)*38,0,30,word);
        
        for (i=1;i<10;i++) {
            word=getbitu(p+3,j,22)<<8; j+=22;
            setbitu(raw->subfrm[sat-1]+(id-1)*38,i*30,30,word);
        }
        if (id!=3) return 0;
        if (!decode_bds_d1(raw->subfrm[sat-1],&eph,NULL,NULL)) return 0;
    }
    else { /* GEO */
        if (id!=1) return 0;
        
        pgn=getbitu(p+3,26+12,4); /* page number */
        if (pgn<1||10<pgn) {
            trace(NULL,2,"stq bds subframe page number error: prn=%2d pgn=%d\n",prn,pgn);
            return -1;
        }
        word=getbitu(p+3,j,26)<<4; j+=26;
        setbitu(raw->subfrm[sat-1]+(pgn-1)*38,0,30,word);
        
        for (i=1;i<10;i++) {
            word=getbitu(p+3,j,22)<<8; j+=22;
            setbitu(raw->subfrm[sat-1]+(pgn-1)*38,i*30,30,word);
        }
        if (pgn!=10) return 0;
        if (!decode_bds_d2(raw->subfrm[sat-1],&eph,NULL)) return 0;
    }
    if (!strstr(raw->opt,"-EPHALL")) {
        if (timediff(eph.toe,raw->nav.eph[sat-1].toe)==0.0) return 0; /* unchanged */
    }
    eph.sat=sat;
    raw->nav.eph[sat-1]=eph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* decode ack to request msg (0x83) ------------------------------------------*/
static int decode_stqack(raw_t *raw)
{
    uint8_t *p=raw->buff+4;
    
    trace(NULL,4,"decode_stqack: len=%d\n",raw->len);
    
    if (raw->len<9) {
        trace(NULL,2,"stq ack length error: len=%d\n",raw->len);
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype,"SKYTRAQ ACK   (%4d): msg=0x%02X",raw->len,
                U1(p+1));
    }
    return 0;
}
/* decode nack to request msg (0x84) -----------------------------------------*/
static int decode_stqnack(raw_t *raw)
{
    uint8_t *p=raw->buff+4;
    
    trace(NULL,4,"decode_stqnack: len=%d\n",raw->len);
    
    if (raw->len<9) {
        trace(NULL,2,"stq nack length error: len=%d\n",raw->len);
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype,"SKYTRAQ NACK  (%4d): msg=0x%02X",raw->len,
                U1(p+1));
    }
    return 0;
}
/* decode skytraq message ----------------------------------------------------*/
static int decode_stq(raw_t *raw)
{
    int type=U1(raw->buff+4);
    uint8_t cs,*p=raw->buff+raw->len-3;
    
    trace(NULL,3,"decode_stq: type=%02x len=%d\n",type,raw->len);
    
    /* checksum */
    cs=checksum(raw->buff,raw->len);
    
    if (cs!=*p||*(p+1)!=0x0D||*(p+2)!=0x0A) {
        trace(NULL,2,"stq checksum error: type=%02X cs=%02X tail=%02X%02X%02X\n",
              type,cs,*p,*(p+1),*(p+2));
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype,"SKYTRAQ 0x%02X  (%4d):",type,raw->len);
    }
    switch (type) {
        case ID_STQTIME : return decode_stqtime(raw);
        case ID_STQRAW  : return decode_stqraw (raw);
        case ID_STQRAWX : return decode_stqrawx(raw);
        case ID_STQGPS  : return decode_stqgps (raw);
        case ID_STQGLO  : return decode_stqglo (raw);
        case ID_STQGLOE : return decode_stqgloe(raw);
        case ID_STQBDSD1: return decode_stqbds (raw);
        case ID_STQBDSD2: return decode_stqbds (raw);
        case ID_STQACK  : return decode_stqack (raw);
        case ID_STQNACK : return decode_stqnack(raw);
    }
    return 0;
}
/* sync code -----------------------------------------------------------------*/
static int sync_stq(uint8_t *buff, uint8_t data)
{
    buff[0]=buff[1]; buff[1]=data;
    return buff[0]==STQSYNC1&&buff[1]==STQSYNC2;
}
/* input skytraq raw message from stream ---------------------------------------
* fetch next skytraq raw data and input a mesasge from stream
* args   : raw_t *raw       IO  receiver raw data control struct
*          uint8_t data     I   stream data (1 byte)
* return : status (-1: error message, 0: no message, 1: input observation data,
*                  2: input ephemeris, 3: input sbas message,
*                  9: input ion/utc parameter)
*
* notes  : to specify input options, set raw->opt to the following option
*          strings separated by spaces.
*
*          -INVCP     : inverse polarity of carrier-phase
*
*-----------------------------------------------------------------------------*/
extern int input_stq(raw_t *raw, uint8_t data)
{
    trace(NULL,5,"input_stq: data=%02x\n",data);
    
    /* synchronize frame */
    if (raw->nbyte==0) {
        if (!sync_stq(raw->buff,data)) return 0;
        raw->nbyte=2;
        return 0;
    }
    raw->buff[raw->nbyte++]=data;
    
    if (raw->nbyte==4) {
        if ((raw->len=U2(raw->buff+2)+7)>MAXRAWLEN) {
            trace(NULL,2,"stq message length error: len=%d\n",raw->len);
            raw->nbyte=0;
            return -1;
        }
    }
    if (raw->nbyte<4||raw->nbyte<raw->len) return 0;
    raw->nbyte=0;
    
    /* decode skytraq raw message */
    return decode_stq(raw);
}
/* input skytraq raw message from file -----------------------------------------
* fetch next skytraq raw data and input a message from file
* args   : raw_t  *raw      IO  receiver raw data control struct
*          FILE   *fp       I   file pointer
* return : status(-2: end of file, -1...9: same as above)
*-----------------------------------------------------------------------------*/
extern int input_stqf(raw_t *raw, FILE *fp)
{
    int i,data;
    
    trace(NULL,4,"input_stqf:\n");
    
    /* synchronize frame */
    if (raw->nbyte==0) {
        for (i=0;;i++) {
            if ((data=fgetc(fp))==EOF) return -2;
            if (sync_stq(raw->buff,(uint8_t)data)) break;
            if (i>=4096) return 0;
        }
    }
    if (fread(raw->buff+2,1,2,fp)<2) return -2;
    raw->nbyte=4;
    
    if ((raw->len=U2(raw->buff+2)+7)>MAXRAWLEN) {
        trace(NULL,2,"stq message length error: len=%d\n",raw->len);
        raw->nbyte=0;
        return -1;
    }
    if (fread(raw->buff+4,1,raw->len-4,fp)<(size_t)(raw->len-4)) return -2;
    raw->nbyte=0;
    
    /* decode skytraq raw message */
    return decode_stq(raw);
}
/* generate skytraq binary message ---------------------------------------------
* generate skytraq binary message from message string
* args   : char  *msg       I   message string 
*            "RESTART  [arg...]" system restart
*            "CFG-SERI [arg...]" configure serial port propperty
*            "CFG-FMT  [arg...]" configure output message format
*            "CFG-RATE [arg...]" configure binary measurement output rates
*            "CFG-BIN  [arg...]" configure general binary
*            "GET-GLOEPH [slot]" get glonass ephemeris for freq channel number
*          uint8_t *buff O binary message
* return : length of binary message (0: error)
* note   : see reference [1][2][3][4] for details.
*-----------------------------------------------------------------------------*/
extern int gen_stq(const char *msg, uint8_t *buff)
{
    const char *hz[]={"1Hz","2Hz","4Hz","5Hz","10Hz","20Hz",""};
    uint8_t *q=buff;
    char mbuff[1024],*args[32],*p;
    int i,n,narg=0;
    
    trace(NULL,4,"gen_stq: msg=%s\n",msg);
    
    strcpy(mbuff,msg);
    for (p=strtok(mbuff," ");p&&narg<32;p=strtok(NULL," ")) {
        args[narg++]=p;
    }
    if (narg<1) {
        return 0;
    }
    *q++=STQSYNC1;
    *q++=STQSYNC2;
    if (!strcmp(args[0],"RESTART")) {
        *q++=0;
        *q++=15;
        *q++=ID_RESTART;
        *q++=narg>2?(uint8_t)atoi(args[1]):0;
        for (i=1;i<15;i++) *q++=0; /* set all 0 */
    }
    else if (!strcmp(args[0],"CFG-SERI")) {
        *q++=0;
        *q++=4;
        *q++=ID_CFGSERI;
        for (i=1;i<4;i++) *q++=narg>i+1?(uint8_t)atoi(args[i]):0;
    }
    else if (!strcmp(args[0],"CFG-FMT")) {
        *q++=0;
        *q++=3;
        *q++=ID_CFGFMT;
        for (i=1;i<3;i++) *q++=narg>i+1?(uint8_t)atoi(args[i]):0;
    }
    else if (!strcmp(args[0],"CFG-RATE")) {
        *q++=0;
        *q++=8;
        *q++=ID_CFGRATE;
        if (narg>2) {
            for (i=0;*hz[i];i++) if (!strcmp(args[1],hz[i])) break;
            if (*hz[i]) *q++=i; else *q++=(uint8_t)atoi(args[1]);
        }
        else *q++=0;
        for (i=2;i<8;i++) *q++=narg>i+1?(uint8_t)atoi(args[i]):0;
    }
    else if (!strcmp(args[0],"CFG-BIN")) {
        *q++=0;
        *q++=9; /* F/W 1.4.32 */
        *q++=ID_CFGBIN;
        if (narg>2) {
            for (i=0;*hz[i];i++) if (!strcmp(args[1],hz[i])) break;
            if (*hz[i]) *q++=i; else *q++=(uint8_t)atoi(args[1]);
        }
        else *q++=0;
        for (i=2;i<9;i++) *q++=narg>i+1?(uint8_t)atoi(args[i]):0;
    }
    else if (!strcmp(args[0],"GET-GLOEPH")) {
        *q++=0;
        *q++=2;
        *q++=ID_GETGLOEPH;
        *q++=narg>=2?(uint8_t)atoi(args[1]):0;
    }
    else return 0;
    
    n=(int)(q-buff);
    *q++=checksum(buff,n+3);
    *q++=0x0D;
    *q=0x0A;
    
    trace(NULL,4,"gen_stq: buff=\n"); traceb(NULL,4,buff,n+3);
    return n+3;
}
