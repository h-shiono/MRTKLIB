/*------------------------------------------------------------------------------
 * mrtk_rcv_ss2.c : NovAtel Superstar II receiver raw data decoder
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

#define SS2SOH      0x01        /* ss2 start of header */

#define ID_SS2LLH   20          /* ss2 message ID#20 navigation data (user) */
#define ID_SS2ECEF  21          /* ss2 message ID#21 navigation data (ecef) */
#define ID_SS2EPH   22          /* ss2 message ID#22 ephemeris data */
#define ID_SS2RAW   23          /* ss2 message ID#23 measurement block */
#define ID_SS2SBAS  67          /* ss2 message ID#67 sbas data */

/* get/set fields (little-endian) --------------------------------------------*/
#define U1(p) (*((uint8_t *)(p)))
static uint16_t U2(uint8_t *p) {uint16_t u; memcpy(&u,p,2); return u;}
static uint32_t U4(uint8_t *p) {uint32_t u; memcpy(&u,p,4); return u;}
static double   R8(uint8_t *p) {double   r; memcpy(&r,p,8); return r;}

/* checksum ------------------------------------------------------------------*/
static int chksum(const uint8_t *buff, int len)
{
    int i;
    uint16_t sum=0;
    
    for (i=0;i<len-2;i++) sum+=buff[i];
    return (sum>>8)==buff[len-1]&&(sum&0xFF)==buff[len-2];
}
/* adjust week ---------------------------------------------------------------*/
static int adjweek(raw_t *raw, double sec)
{
    double tow;
    int week;
    
    if (raw->time.time==0) return 0;
    tow=time2gpst(raw->time,&week);
    if      (sec<tow-302400.0) sec+=604800.0;
    else if (sec>tow+302400.0) sec-=604800.0;
    raw->time=gpst2time(week,sec);
    return 1;
}
/* decode id#20 navigation data (user) ---------------------------------------*/
static int decode_ss2llh(raw_t *raw)
{
	double ep[6];
    uint8_t *p=raw->buff+4;
    
    trace(NULL,4,"decode_ss2llh: len=%d\n",raw->len);
    
    if (raw->len!=77) {
        trace(NULL,2,"ss2 id#20 length error: len=%d\n",raw->len);
        return -1;
    }
    ep[3]=U1(p   ); ep[4]=U1(p+ 1); ep[5]=R8(p+ 2);
    ep[2]=U1(p+10); ep[1]=U1(p+11); ep[0]=U2(p+12);
    raw->time=utc2gpst(epoch2time(ep));
    return 0;
}
/* decode id#21 navigation data (ecef) ---------------------------------------*/
static int decode_ss2ecef(raw_t *raw)
{
    uint8_t *p=raw->buff+4;
    
    trace(NULL,4,"decode_ss2ecef: len=%d\n",raw->len);
    
    if (raw->len!=85) {
        trace(NULL,2,"ss2 id#21 length error: len=%d\n",raw->len);
        return -1;
    }
    raw->time=gpst2time(U2(p+8),R8(p));
    return 0;
}
/* decode id#23 measurement block --------------------------------------------*/
static int decode_ss2meas(raw_t *raw)
{
    const double freqif=1.405396825E6,tslew=1.75E-7;
    double tow,slew,code,icp,d;
    int i,j,n,prn,sat,nobs;
    uint8_t *p=raw->buff+4;
    uint32_t sc;
    
    trace(NULL,4,"decode_ss2meas: len=%d\n",raw->len);
    
    nobs=U1(p+2);
    if (17+nobs*11!=raw->len) {
        trace(NULL,2,"ss2 id#23 message length error: len=%d\n",raw->len);
        return -1;
    }
    tow=floor(R8(p+3)*1000.0+0.5)/1000.0; /* rounded by 1ms */
    if (!adjweek(raw,tow)) {
        trace(NULL,2,"ss2 id#23 message time adjustment error\n");
        return -1;
    }
    /* time slew defined as uchar (ref [1]) but minus value appears in some f/w */
    slew=*(char *)(p)*tslew;
    
    raw->icpc+=4.5803-freqif*slew-FREQ1*(slew-1E-6); /* phase correction */
    
    for (i=n=0,p+=11;i<nobs&&n<MAXOBS;i++,p+=11) {
        prn=(p[0]&0x1F)+1;
        if (!(sat=satno(p[0]&0x20?SYS_SBS:SYS_GPS,prn))) {
            trace(NULL,2,"ss2 id#23 satellite number error: prn=%d\n",prn);
            continue;
        }
        raw->obs.data[n].time=raw->time;
        raw->obs.data[n].sat=sat;
        code=(tow-floor(tow))-(double)(U4(p+2))/2095104000.0;
        raw->obs.data[n].P[0]=CLIGHT*(code+(code<0.0?1.0:0.0));
        icp=(double)(U4(p+6)>>2)/1024.0+raw->off[sat-1]; /* unwrap */
        if (fabs(icp-raw->icpp[sat-1])>524288.0) {
            d=icp>raw->icpp[sat-1]?-1048576.0:1048576.0;
            raw->off[sat-1]+=d; icp+=d;
        }
        raw->icpp[sat-1]=icp;
        raw->obs.data[n].L[0]=icp+raw->icpc;
        raw->obs.data[n].D[0]=0.0;
        raw->obs.data[n].SNR[0]=(uint16_t)(U1(p+1)*0.25/SNR_UNIT+0.5);
        sc=U1(p+10);
        raw->obs.data[n].LLI[0]=(int)((uint8_t)sc-(uint8_t)raw->lockt[sat-1][0])>0;
        raw->obs.data[n].LLI[0]|=U1(p+6)&1?2:0;
        raw->obs.data[n].code[0]=CODE_L1C;
        raw->lockt[sat-1][0]=sc;
        
        for (j=1;j<NFREQ;j++) {
            raw->obs.data[n].L[j]=raw->obs.data[n].P[j]=0.0;
            raw->obs.data[n].D[j]=0.0;
            raw->obs.data[n].SNR[j]=raw->obs.data[n].LLI[j]=0;
            raw->obs.data[n].code[j]=CODE_NONE;
        }
        n++;
    }
    raw->obs.n=n;
    return 1;
}
/* decode id#22 ephemeris data ------------------------------------------------*/
static int decode_ss2eph(raw_t *raw)
{
    eph_t eph={0};
    uint32_t tow;
    uint8_t *p=raw->buff+4,buff[90]={0};
    int i,j,prn,sat;
    
    trace(NULL,4,"decode_ss2eph: len=%d\n",raw->len);
    
    if (raw->len!=79) {
        trace(NULL,2,"ss2 id#22 length error: len=%d\n",raw->len);
        return -1;
    }
    prn=(U4(p)&0x1F)+1;
    if (!(sat=satno(SYS_GPS,prn))) {
        trace(NULL,2,"ss2 id#22 satellite number error: prn=%d\n",prn);
        return -1;
    }
    if (raw->time.time==0) {
        trace(NULL,2,"ss2 id#22 week number unknown error\n");
        return -1;
    }
    tow=(uint32_t)(time2gpst(raw->time,NULL)/6.0);
    for (i=0;i<3;i++) {
        buff[30*i+3]=(uint8_t)(tow>>9); /* add tow + subframe id */
        buff[30*i+4]=(uint8_t)(tow>>1);
        buff[30*i+5]=(uint8_t)(((tow&1)<<7)+((i+1)<<2));
        for (j=0;j<24;j++) buff[30*i+6+j]=p[1+24*i+j];
    }
    if (!decode_frame(buff,&eph,NULL,NULL,NULL)) {
        trace(NULL,2,"ss2 id#22 subframe error: prn=%d\n",prn);
        return -1;
    }
    if (!strstr(raw->opt,"-EPHALL")) {
        if (eph.iode==raw->nav.eph[sat-1].iode) return 0; /* unchanged */
    }
    eph.sat=sat;
    eph.ttr=raw->time;
    raw->nav.eph[sat-1]=eph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* decode id#67 sbas data ----------------------------------------------------*/
static int decode_ss2sbas(raw_t *raw)
{
    gtime_t time;
    int i,prn;
    uint8_t *p=raw->buff+4;
    
    trace(NULL,4,"decode_ss2sbas: len=%d\n",raw->len);
    
    if (raw->len!=54) {
        trace(NULL,2,"ss2 id#67 length error: len=%d\n",raw->len);
        return -1;
    }
    prn=U4(p+12);
    if (prn<MINPRNSBS||MAXPRNSBS<prn) {
        trace(NULL,3,"ss2 id#67 prn error: prn=%d\n",prn);
        return 0;
    }
    raw->sbsmsg.week=U4(p);
    raw->sbsmsg.tow=(int)R8(p+4);
    time=gpst2time(raw->sbsmsg.week,raw->sbsmsg.tow);
    raw->sbsmsg.prn=prn;
    for (i=0;i<29;i++) raw->sbsmsg.msg[i]=p[16+i];
    return 3;
}
/* decode superstar 2 raw message --------------------------------------------*/
static int decode_ss2(raw_t *raw)
{
    uint8_t *p=raw->buff;
    int type=U1(p+1);
    
    trace(NULL,3,"decode_ss2: type=%2d\n",type);
    
    if (!chksum(raw->buff,raw->len)) {
        trace(NULL,2,"ss2 message checksum error: type=%d len=%d\n",type,raw->len);
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype,"SS2 %2d (%4d):",type,raw->len);
    }
    switch (type) {
        case ID_SS2LLH : return decode_ss2llh (raw);
        case ID_SS2ECEF: return decode_ss2ecef(raw);
        case ID_SS2RAW : return decode_ss2meas(raw);
        case ID_SS2EPH : return decode_ss2eph (raw);
        case ID_SS2SBAS: return decode_ss2sbas(raw);
    }
    return 0;
}
/* sync code -----------------------------------------------------------------*/
static int sync_ss2(uint8_t *buff, uint8_t data)
{
    buff[0]=buff[1]; buff[1]=buff[2]; buff[2]=data;
    return buff[0]==SS2SOH&&(buff[1]^buff[2])==0xFF;
}
/* input superstar 2 raw message from stream -----------------------------------
* input next superstar 2 raw message from stream
* args   : raw_t *raw   IO     receiver raw data control struct
*          uint8_t data I      stream data (1 byte)
* return : status (-1: error message, 0: no message, 1: input observation data,
*                  2: input ephemeris, 3: input sbas message,
*                  9: input ion/utc parameter)
* notes  : needs #20 or #21 message to get proper week number of #23 raw
*          observation data
*-----------------------------------------------------------------------------*/
extern int input_ss2(raw_t *raw, uint8_t data)
{
    trace(NULL,5,"input_ss2: data=%02x\n",data);
    
    /* synchronize frame */
    if (raw->nbyte==0) {
        if (!sync_ss2(raw->buff,data)) return 0;
        raw->nbyte=3;
        return 0;
    }
    raw->buff[raw->nbyte++]=data;
    
    if (raw->nbyte==4) {
        if ((raw->len=U1(raw->buff+3)+6)>MAXRAWLEN) {
            trace(NULL,2,"ss2 length error: len=%d\n",raw->len);
            raw->nbyte=0;
            return -1;
        }
    }
    if (raw->nbyte<4||raw->nbyte<raw->len) return 0;
    raw->nbyte=0;
    
    /* decode superstar 2 raw message */
    return decode_ss2(raw);
}
/* input superstar 2 raw message from file -------------------------------------
* input next superstar 2 raw message from file
* args   : raw_t  *raw   IO     receiver raw data control struct
*          FILE   *fp    I      file pointer
* return : status(-2: end of file, -1...9: same as above)
*-----------------------------------------------------------------------------*/
extern int input_ss2f(raw_t *raw, FILE *fp)
{
    int i,data;
    
    trace(NULL,4,"input_ss2f:\n");
    
    /* synchronize frame */
    if (raw->nbyte==0) {
        for (i=0;;i++) {
            if ((data=fgetc(fp))==EOF) return -2;
            if (sync_ss2(raw->buff,(uint8_t)data)) break;
            if (i>=4096) return 0;
        }
    }
    if (fread(raw->buff+3,1,1,fp)<1) return -2;
    raw->nbyte=4;
    
    if ((raw->len=U1(raw->buff+3)+6)>MAXRAWLEN) {
        trace(NULL,2,"ss2 length error: len=%d\n",raw->len);
        raw->nbyte=0;
        return -1;
    }
    if (fread(raw->buff+4,1,raw->len-4,fp)<(size_t)(raw->len-4)) return -2;
    raw->nbyte=0;
    
    /* decode superstar 2 raw message */
    return decode_ss2(raw);
}
