/*------------------------------------------------------------------------------
 * mrtk_rcv_nvs.c : NVS receiver raw data decoder
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
#include "mrtklib/mrtk_rcvraw.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_sbas.h"
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

#define NVSSYNC     0x10        /* nvs message sync code 1 */
#define NVSENDMSG   0x03        /* nvs message sync code 1 */
#define NVSCFG      0x06        /* nvs message cfg-??? */

#define ID_XF5RAW   0xf5        /* nvs msg id: raw measurement data */
#define ID_X4AIONO  0x4a        /* nvs msg id: gps ionospheric data */
#define ID_X4BTIME  0x4b        /* nvs msg id: GPS/GLONASS/UTC timescale data */
#define ID_XF7EPH   0xf7        /* nvs msg id: subframe buffer */
#define ID_XE5BIT   0xe5        /* nvs msg id: bit information */

#define ID_XD7ADVANCED 0xd7     /* */
#define ID_X02RATEPVT  0x02     /* */
#define ID_XF4RATERAW  0xf4     /* */
#define ID_XD7SMOOTH   0xd7     /* */
#define ID_XD5BIT      0xd5     /* */

/* get fields (little-endian) ------------------------------------------------*/
#define U1(p) (*((uint8_t *)(p)))
#define I1(p) (*((int8_t  *)(p)))
static uint16_t U2(uint8_t *p) {uint16_t u; memcpy(&u,p,2); return u;}
static uint32_t U4(uint8_t *p) {uint32_t u; memcpy(&u,p,4); return u;}
static int16_t  I2(uint8_t *p) {int16_t  i; memcpy(&i,p,2); return i;}
static int32_t  I4(uint8_t *p) {int32_t  i; memcpy(&i,p,4); return i;}
static float    R4(uint8_t *p) {float    r; memcpy(&r,p,4); return r;}
static double   R8(uint8_t *p) {double   r; memcpy(&r,p,8); return r;}

/* ura values (ref [3] 20.3.3.3.1.1) -----------------------------------------*/
static const double ura_eph[]={
    2.4,3.4,4.85,6.85,9.65,13.65,24.0,48.0,96.0,192.0,384.0,768.0,1536.0,
    3072.0,6144.0,0.0
};
/* ura value (m) to ura index ------------------------------------------------*/
static int uraindex(double value)
{
    int i;
    for (i=0;i<15;i++) if (ura_eph[i]>=value) break;
    return i;
}
/* decode NVS xf5-raw: raw measurement data ----------------------------------*/
static int decode_xf5raw(raw_t *raw)
{
    gtime_t time;
    double tadj=0.0,toff=0.0,tn;
    int dTowInt;
    double dTowUTC, dTowGPS, dTowFrac, L1, P1, D1;
    double gpsutcTimescale;
    uint8_t rcvTimeScaleCorr, sys, carrNo;
    int i,j,prn,sat,n=0,nsat,week;
    uint8_t *p=raw->buff+2;
    char *q,tstr[32],flag;
    
    trace(NULL,4,"decode_xf5raw: len=%d\n",raw->len);
    
    /* time tag adjustment option (-TADJ) */
    if ((q=strstr(raw->opt,"-tadj"))) {
        sscanf(q,"-TADJ=%lf",&tadj);
    }
    dTowUTC =R8(p);
    week = U2(p+8);
    gpsutcTimescale = R8(p+10);
    /* glonassutcTimescale = R8(p+18); */
    rcvTimeScaleCorr = I1(p+26);
    
    /* check gps week range */
    if (week>=4096) {
        trace(NULL,2,"nvs xf5raw obs week error: week=%d\n",week);
        return -1;
    }
    week=adjgpsweek(week);
    
    if ((raw->len - 31)%30) {
        
        /* Message length is not correct: there could be an error in the stream */
        trace(NULL,2,"nvs xf5raw len=%d seems not be correct\n",raw->len);
        return -1;
    }
    nsat = (raw->len - 31)/30;
    
    dTowGPS = dTowUTC + gpsutcTimescale;
    
    /* Tweak pseudoranges to allow Rinex to represent the NVS time of measure */
    dTowInt  = 10.0*floor((dTowGPS/10.0)+0.5);
    dTowFrac = dTowGPS - (double) dTowInt;
    time=gpst2time(week, dTowInt*0.001);
    
    /* time tag adjustment */
    if (tadj>0.0) {
        tn=time2gpst(time,&week)/tadj;
        toff=(tn-floor(tn+0.5))*tadj;
        time=timeadd(time,-toff);
    }
    /* check time tag jump and output warning */
    if (raw->time.time&&fabs(timediff(time,raw->time))>86400.0) {
        time2str(time,tstr,3);
        trace(NULL,2,"nvs xf5raw time tag jump warning: time=%s\n",tstr);
    }
    if (fabs(timediff(time,raw->time))<=1e-3) {
        time2str(time,tstr,3);
        trace(NULL,2,"nvs xf5raw time tag duplicated: time=%s\n",tstr);
        return 0;
    }
    for (i=0,p+=27;(i<nsat) && (n<MAXOBS); i++,p+=30) {
        raw->obs.data[n].time  = time;
        sys = (U1(p)==1)?SYS_GLO:((U1(p)==2)?SYS_GPS:((U1(p)==4)?SYS_SBS:SYS_NONE));
        prn = U1(p+1);
        if (sys == SYS_SBS) prn += 120; /* Correct this */
        if (!(sat=satno(sys,prn))) {
            trace(NULL,2,"nvs xf5raw satellite number error: sys=%d prn=%d\n",sys,prn);
            continue;
        }
        carrNo = I1(p+2);
        L1 = R8(p+ 4);
        P1 = R8(p+12);
        D1 = R8(p+20);
        
        /* check range error */
        if (L1<-1E10||L1>1E10||P1<-1E10||P1>1E10||D1<-1E5||D1>1E5) {
            trace(NULL,2,"nvs xf5raw obs range error: sat=%2d L1=%12.5e P1=%12.5e D1=%12.5e\n",
                  sat,L1,P1,D1);
            continue;
        }
        raw->obs.data[n].SNR[0]=(uint16_t)(I1(p+3)/SNR_UNIT+0.5);
        if (sys==SYS_GLO) {
            raw->obs.data[n].L[0]  =  L1 - toff*(FREQ1_GLO+DFRQ1_GLO*carrNo);
        } else {
            raw->obs.data[n].L[0]  =  L1 - toff*FREQ1;
        }
        raw->obs.data[n].P[0]    = (P1-dTowFrac)*CLIGHT*0.001 - toff*CLIGHT; /* in ms, needs to be converted */
        raw->obs.data[n].D[0]    =  (float)D1;
        
        /* set LLI if meas flag 4 (carrier phase present) off -> on */
        flag=U1(p+28);
        raw->obs.data[n].LLI[0]=(flag&0x08)&&!(raw->halfc[sat-1][0]&0x08)?1:0;
        raw->halfc[sat-1][0]=flag;
        
        raw->obs.data[n].code[0] = CODE_L1C;
        raw->obs.data[n].sat = sat;
        
        for (j=1;j<NFREQ+NEXOBS;j++) {
            raw->obs.data[n].L[j]=raw->obs.data[n].P[j]=0.0;
            raw->obs.data[n].D[j]=0.0;
            raw->obs.data[n].SNR[j]=raw->obs.data[n].LLI[j]=0;
            raw->obs.data[n].code[j]=CODE_NONE;
        }
        n++;
    }
    raw->time=time;
    raw->obs.n=n;
    return 1;
}
/* decode ephemeris ----------------------------------------------------------*/
static int decode_gpsephem(int sat, raw_t *raw)
{
    eph_t eph={0};
    uint8_t *puiTmp = (raw->buff)+2;
    uint16_t week;
    double toc;
    
    trace(NULL,4,"decode_ephem: sat=%2d\n",sat);
    
    eph.crs    = R4(&puiTmp[  2]);
    eph.deln   = R4(&puiTmp[  6]) * 1e+3;
    eph.M0     = R8(&puiTmp[ 10]);
    eph.cuc    = R4(&puiTmp[ 18]);
    eph.e      = R8(&puiTmp[ 22]);
    eph.cus    = R4(&puiTmp[ 30]);
    eph.A      = pow(R8(&puiTmp[ 34]), 2);
    eph.toes   = R8(&puiTmp[ 42]) * 1e-3;
    eph.cic    = R4(&puiTmp[ 50]);
    eph.OMG0   = R8(&puiTmp[ 54]);
    eph.cis    = R4(&puiTmp[ 62]);
    eph.i0     = R8(&puiTmp[ 66]);
    eph.crc    = R4(&puiTmp[ 74]);
    eph.omg    = R8(&puiTmp[ 78]);
    eph.OMGd   = R8(&puiTmp[ 86]) * 1e+3;
    eph.idot   = R8(&puiTmp[ 94]) * 1e+3;
    eph.tgd[0] = R4(&puiTmp[102]) * 1e-3;
    toc        = R8(&puiTmp[106]) * 1e-3;
    eph.f2     = R4(&puiTmp[114]) * 1e+3;
    eph.f1     = R4(&puiTmp[118]);
    eph.f0     = R4(&puiTmp[122]) * 1e-3;
    eph.sva    = uraindex(I2(&puiTmp[126]));
    eph.iode   = I2(&puiTmp[128]);
    eph.iodc   = I2(&puiTmp[130]);
    eph.code   = I2(&puiTmp[132]);
    eph.flag   = I2(&puiTmp[134]);
    week       = I2(&puiTmp[136]);
    eph.fit    = 0;
    
    if (week>=4096) {
        trace(NULL,2,"nvs gps ephemeris week error: sat=%2d week=%d\n",sat,week);
        return -1;
    }
    eph.week=adjgpsweek(week);
    eph.toe=gpst2time(eph.week,eph.toes);
    eph.toc=gpst2time(eph.week,toc);
    eph.ttr=raw->time;
    
    if (!strstr(raw->opt,"-EPHALL")) {
        if (eph.iode==raw->nav.eph[sat-1].iode) return 0; /* unchanged */
    }
    eph.sat=sat;
    eph.type=0; /* ephemeris type = LNAV */
    raw->nav.eph[sat-1]=eph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* adjust daily rollover of time ---------------------------------------------*/
static gtime_t adjday(gtime_t time, double tod)
{
    double ep[6],tod_p;
    time2epoch(time,ep);
    tod_p=ep[3]*3600.0+ep[4]*60.0+ep[5];
    if      (tod<tod_p-43200.0) tod+=86400.0;
    else if (tod>tod_p+43200.0) tod-=86400.0;
    ep[3]=ep[4]=ep[5]=0.0;
    return timeadd(epoch2time(ep),tod);
}
/* decode gloephem -----------------------------------------------------------*/
static int decode_gloephem(int sat, raw_t *raw)
{
    geph_t geph={0};
    uint8_t *p=(raw->buff)+2;
    int prn,tk,tb;
    
    if (raw->len>=93) {
        prn        =I1(p+ 1);
        geph.frq   =I1(p+ 2);
        geph.pos[0]=R8(p+ 3);
        geph.pos[1]=R8(p+11);
        geph.pos[2]=R8(p+19);
        geph.vel[0]=R8(p+27) * 1e+3;
        geph.vel[1]=R8(p+35) * 1e+3;
        geph.vel[2]=R8(p+43) * 1e+3;
        geph.acc[0]=R8(p+51) * 1e+6;
        geph.acc[1]=R8(p+59) * 1e+6;
        geph.acc[2]=R8(p+67) * 1e+6;
        tb = R8(p+75) * 1e-3;
        tk = tb;
        geph.gamn  =R4(p+83);
        geph.taun  =R4(p+87) * 1e-3;
        geph.age   =I2(p+91);
    }
    else {
        trace(NULL,2,"nvs NE length error: len=%d\n",raw->len);
        return -1;
    }
    if (!(geph.sat=satno(SYS_GLO,prn))) {
        trace(NULL,2,"nvs NE satellite error: prn=%d\n",prn);
        return -1;
    }
    if (raw->time.time==0) return 0;
    
    geph.iode=(tb/900)&0x7F;
    geph.toe=utc2gpst(adjday(raw->time,tb-10800.0));
    geph.tof=utc2gpst(adjday(raw->time,tk-10800.0));
#if 0
    /* check illegal ephemeris by toe */
    tt=timediff(raw->time,geph.toe);
    if (fabs(tt)>3600.0) {
        trace(NULL,3,"nvs NE illegal toe: prn=%2d tt=%6.0f\n",prn,tt);
        return 0;
    }
#endif
#if 0
    /* check illegal ephemeris by frequency number consistency */
    if (raw->nav.geph[prn-MINPRNGLO].toe.time&&
        geph.frq!=raw->nav.geph[prn-MINPRNGLO].frq) {
        trace(NULL,2,"nvs NE illegal freq change: prn=%2d frq=%2d->%2d\n",prn,
              raw->nav.geph[prn-MINPRNGLO].frq,geph.frq);
        return -1;
    }
    if (!strstr(raw->opt,"-EPHALL")) {
        if (fabs(timediff(geph.toe,raw->nav.geph[prn-MINPRNGLO].toe))<1.0&&
            geph.svh==raw->nav.geph[prn-MINPRNGLO].svh) return 0;
    }
#endif
    raw->nav.geph[prn-1]=geph;
    raw->ephsat=geph.sat;
    raw->ephset=0;
    
    return 2;
}
/* decode NVS ephemerides in clear -------------------------------------------*/
static int decode_xf7eph(raw_t *raw)
{
    int prn,sat,sys;
    uint8_t *p=raw->buff;
    
    trace(NULL,4,"decode_xf7eph: len=%d\n",raw->len);
    
    if ((raw->len)<93) {
        trace(NULL,2,"nvs xf7eph length error: len=%d\n",raw->len);
        return -1;
    }
    sys = (U1(p+2)==1)?SYS_GPS:((U1(p+2)==2)?SYS_GLO:SYS_NONE);
    prn = U1(p+3);
    if (!(sat=satno(sys==1?SYS_GPS:SYS_GLO,prn))) {
        trace(NULL,2,"nvs xf7eph satellite number error: prn=%d\n",prn);
        return -1;
    }
    if (sys==SYS_GPS) {
        return decode_gpsephem(sat,raw);
    }
    else if (sys==SYS_GLO) {
        return decode_gloephem(sat,raw);
    }
    return 0;
}
/* decode NVS rxm-sfrb: subframe buffer --------------------------------------*/
static int decode_xe5bit(raw_t *raw)
{
    int prn;
    int iBlkStartIdx, iExpLen, iIdx;
    uint32_t words[10];
    uint8_t uiDataBlocks, uiDataType;
    uint8_t *p=raw->buff;
    
    trace(NULL,4,"decode_xe5bit: len=%d\n",raw->len);
    
    p += 2;         /* Discard preamble and message identifier */
    uiDataBlocks = U1(p);
    
    if (uiDataBlocks>=16) {
        trace(NULL,2,"nvs xf5bit message error: data blocks %u\n", uiDataBlocks);
        return -1;
    }
    iBlkStartIdx = 1;
    for (iIdx = 0; iIdx < uiDataBlocks; iIdx++) {
        iExpLen = (iBlkStartIdx+10);
        if ((raw->len) < iExpLen) {
            trace(NULL,2,"nvs xf5bit message too short (expected at least %d)\n", iExpLen);
            return -1;
        }
        uiDataType = U1(p+iBlkStartIdx+1);
        
        switch (uiDataType) {
            case 1: /* Glonass */
                iBlkStartIdx += 19;
                break;
            case 2: /* GPS */
                iBlkStartIdx += 47;
                break;
            case 4: /* SBAS */
                prn = U1(p+(iBlkStartIdx+2)) + 120;
                
                /* sat = satno(SYS_SBS, prn); */
                /* sys = satsys(sat,&prn); */
                memset(words, 0, 10*sizeof(uint32_t));
                for (iIdx=0, iBlkStartIdx+=7; iIdx<10; iIdx++, iBlkStartIdx+=4) {
                    words[iIdx]=U4(p+iBlkStartIdx);
                }
                words[7] >>= 6;
                return sbsdecodemsg(raw->time,prn,words,&raw->sbsmsg) ? 3 : 0;
            default:
                trace(NULL,2,"nvs xf5bit SNS type unknown (got %d)\n", uiDataType);
                return -1;
        }
    }
    return 0;
}
/* decode NVS x4aiono --------------------------------------------------------*/
static int decode_x4aiono(raw_t *raw)
{
    uint8_t *p=raw->buff+2;
    
    trace(NULL,4,"decode_x4aiono: len=%d\n", raw->len);
    
    raw->nav.ion_gps[0] = R4(p   );
    raw->nav.ion_gps[1] = R4(p+ 4);
    raw->nav.ion_gps[2] = R4(p+ 8);
    raw->nav.ion_gps[3] = R4(p+12);
    raw->nav.ion_gps[4] = R4(p+16);
    raw->nav.ion_gps[5] = R4(p+20);
    raw->nav.ion_gps[6] = R4(p+24);
    raw->nav.ion_gps[7] = R4(p+28);
    
    return 9;
}
/* decode NVS x4btime --------------------------------------------------------*/
static int decode_x4btime(raw_t *raw)
{
    uint8_t *p=raw->buff+2;
    
    trace(NULL,4,"decode_x4btime: len=%d\n", raw->len);
    
    raw->nav.utc_gps[1] = R8(p   );
    raw->nav.utc_gps[0] = R8(p+ 8);
    raw->nav.utc_gps[2] = I4(p+16);
    raw->nav.utc_gps[3] = I2(p+20);
    raw->nav.utc_gps[4] = I1(p+22);
    
    return 9;
}
/* decode NVS raw message ----------------------------------------------------*/
static int decode_nvs(raw_t *raw)
{
    int type=U1(raw->buff+1);
    
    trace(NULL,3,"decode_nvs: type=%02x len=%d\n",type,raw->len);
    
    sprintf(raw->msgtype,"NVS: type=%2d len=%3d",type,raw->len);
    
    switch (type) {
        case ID_XF5RAW:  return decode_xf5raw (raw);
        case ID_XF7EPH:  return decode_xf7eph (raw);
        case ID_XE5BIT:  return decode_xe5bit (raw);
        case ID_X4AIONO: return decode_x4aiono(raw);
        case ID_X4BTIME: return decode_x4btime(raw);
        default: break;
    }
    return 0;
}
/* input NVS raw message from stream -------------------------------------------
* fetch next NVS raw data and input a message from stream
* args   : raw_t *raw   IO    receiver raw data control struct
*          uint8_t data I     stream data (1 byte)
* return : status (-1: error message, 0: no message, 1: input observation data,
*                  2: input ephemeris, 3: input sbas message,
*                  9: input ion/utc parameter)
*
* notes  : to specify input options, set raw->opt to the following option
*          strings separated by spaces.
*
*          -EPHALL    : input all ephemerides
*          -TADJ=tint : adjust time tags to multiples of tint (sec)
*
*-----------------------------------------------------------------------------*/
extern int input_nvs(raw_t *raw, uint8_t data)
{
    trace(NULL,5,"input_nvs: data=%02x\n",data);
    
    /* synchronize frame */
    if ((raw->nbyte==0) && (data==NVSSYNC)) {
        
        /* Search a 0x10 */
        raw->buff[0] = data;
        raw->nbyte=1;
        return 0;
    }
    if ((raw->nbyte==1) && (data != NVSSYNC) && (data != NVSENDMSG)) {
        
        /* Discard double 0x10 and 0x10 0x03 at beginning of frame */
        raw->buff[1]=data;
        raw->nbyte=2;
        raw->flag=0;
        return 0;
    }
    /* This is all done to discard a double 0x10 */
    if (data==NVSSYNC) raw->flag = (raw->flag +1) % 2;
    if ((data!=NVSSYNC) || (raw->flag)) {
        
        /* Store the new byte */
        raw->buff[(raw->nbyte++)] = data;
    }
    /* Detect ending sequence */
    if ((data==NVSENDMSG) && (raw->flag)) {
        raw->len   = raw->nbyte;
        raw->nbyte = 0;
        
        /* Decode NVS raw message */
        return decode_nvs(raw);
    }
    if (raw->nbyte == MAXRAWLEN) {
        trace(NULL,2,"nvs message size error: len=%d\n",raw->nbyte);
        raw->nbyte=0;
        return -1;
    }
    return 0;
}
/* input NVS raw message from file ---------------------------------------------
* fetch next NVS raw data and input a message from file
* args   : raw_t  *raw  IO    receiver raw data control struct
*          FILE   *fp   I     file pointer
* return : status(-2: end of file, -1...9: same as above)
*-----------------------------------------------------------------------------*/
extern int input_nvsf(raw_t *raw, FILE *fp)
{
    int i,data, odd=0;
    
    trace(NULL,4,"input_nvsf:\n");
    
    /* synchronize frame */
    for (i=0;;i++) {
        if ((data=fgetc(fp))==EOF) return -2;
        
        /* Search a 0x10 */
        if (data==NVSSYNC) {
            
            /* Store the frame begin */
            raw->buff[0] = data;
            if ((data=fgetc(fp))==EOF) return -2;
            
            /* Discard double 0x10 and 0x10 0x03 */
            if ((data != NVSSYNC) && (data != NVSENDMSG)) {
                raw->buff[1]=data;
                break;
            }
        }
        if (i>=4096) return 0;
    }
    raw->nbyte = 2;
    for (i=0;;i++) {
        if ((data=fgetc(fp))==EOF) return -2;
        if (data==NVSSYNC) odd=(odd+1)%2;
        if ((data!=NVSSYNC) || odd) {
            
            /* Store the new byte */
            raw->buff[(raw->nbyte++)] = data;
        }
        /* Detect ending sequence */
        if ((data==NVSENDMSG) && odd) break;
        if (i>=4096) return 0;
    }
    raw->len = raw->nbyte;
    if ((raw->len) > MAXRAWLEN) {
        trace(NULL,2,"nvs length error: len=%d\n",raw->len);
        return -1;
    }
    /* decode nvs raw message */
    return decode_nvs(raw);
}
/* generate NVS binary message -------------------------------------------------
* generate NVS binary message from message string
* args   : char  *msg   I      message string
*            "RESTART  [arg...]" system reset
*            "CFG-SERI [arg...]" configure serial port property
*            "CFG-FMT  [arg...]" configure output message format
*            "CFG-RATE [arg...]" configure binary measurement output rates
*          uint8_t *buff O binary message
* return : length of binary message (0: error)
* note   : see reference [1][2] for details.
*-----------------------------------------------------------------------------*/
extern int gen_nvs(const char *msg, uint8_t *buff)
{
    uint8_t *q=buff;
    char mbuff[1024],*args[32],*p;
    uint32_t byte;
    int iRate,n,narg=0;
    uint8_t ui100Ms;
    
    trace(NULL,4,"gen_nvs: msg=%s\n",msg);
    
    strcpy(mbuff,msg);
    for (p=strtok(mbuff," ");p&&narg<32;p=strtok(NULL," ")) {
        args[narg++]=p;
    }
    if (narg<1) {
        return 0;
    }
    *q++=NVSSYNC; /* DLE */
    
    if (!strcmp(args[0],"CFG-PVTRATE")) {
        *q++=ID_XD7ADVANCED;
        *q++=ID_X02RATEPVT;
        if (narg>1) {
            iRate = atoi(args[1]);
            *q++ = (uint8_t) iRate;
        }
    }
    else if (!strcmp(args[0],"CFG-RAWRATE")) {
        *q++=ID_XF4RATERAW;
        if (narg>1) {
            iRate = atoi(args[1]);
            switch(iRate) {
                case 2:  ui100Ms =  5; break;
                case 5:  ui100Ms =  2; break;
                case 10: ui100Ms =  1; break;
                default: ui100Ms = 10; break;
            }
            *q++ = ui100Ms;
        }
    }
    else if (!strcmp(args[0],"CFG-SMOOTH")) {
        *q++=ID_XD7SMOOTH;
        *q++ = 0x03;
        *q++ = 0x01;
        *q++ = 0x00;
    }
    else if (!strcmp(args[0],"CFG-BINR")) {
        for (n=1;(n<narg);n++) {
            if (sscanf(args[n], "%2x",&byte)) *q++=(uint8_t)byte;
        }
    }
    else return 0;
    
    n=(int)(q-buff);
    
    *q++=0x10; /* ETX */
    *q=0x03;   /* DLE */
    return n+2;
}
