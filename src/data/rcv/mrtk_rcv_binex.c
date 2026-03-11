/*------------------------------------------------------------------------------
 * mrtk_rcv_binex.c : BINEX receiver raw data decoder
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
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_sys.h"
#include "mrtklib/mrtk_opt.h"

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

#define BNXSYNC1    0xC2    /* BINEX sync (little-endian,regular-crc) */
#define BNXSYNC2    0xE2    /* BINEX sync (big-endian   ,regular-crc) */
#define BNXSYNC3    0xC8    /* BINEX sync (little-endian,enhanced-crc) */
#define BNXSYNC4    0xE8    /* BINEX sync (big-endian   ,enhanced-crc) */

#define BNXSYNC1R   0xD2    /* BINEX sync (little-endian,regular-crc,rev) */
#define BNXSYNC2R   0xF2    /* BINEX sync (big-endian   ,regular-crc,rev) */
#define BNXSYNC3R   0xD8    /* BINEX sync (little-endian,enhanced-crc,rev) */
#define BNXSYNC4R   0xF8    /* BINEX sync (big-endian   ,enhanced-crc,rev) */

#define MIN(x,y)    ((x)<(y)?(x):(y))
#define SQR(x)      ((x)*(x))

/* URA table (URA index -> URA value) ----------------------------------------*/
static const double ura_eph[]={
    2.4,3.4,4.85,6.85,9.65,13.65,24.0,48.0,96.0,192.0,384.0,768.0,1536.0,
    3072.0,6144.0,0.0
};
/* get fields (big-endian) ---------------------------------------------------*/
#define U1(p) (*((uint8_t *)(p)))
#define I1(p) (*((int8_t  *)(p)))

static uint16_t U2(uint8_t *p)
{
    uint16_t value;
    uint8_t *q=(uint8_t *)&value+1;
    int i;
    for (i = 0; i < 2; i++) {
        *q-- = *p++;
    }
    return value;
}
static uint32_t U4(uint8_t *p)
{
    uint32_t value;
    uint8_t *q=(uint8_t *)&value+3;
    int i;
    for (i = 0; i < 4; i++) {
        *q-- = *p++;
    }
    return value;
}
static int32_t I4(uint8_t *p)
{
    return (int32_t)U4(p);
}
static float R4(uint8_t *p)
{
    float value;
    uint8_t *q=(uint8_t *)&value+3;
    int i;
    for (i = 0; i < 4; i++) {
        *q-- = *p++;
    }
    return value;
}
static double R8(uint8_t *p)
{
    double value;
    uint8_t *q=(uint8_t *)&value+7;
    int i;
    for (i = 0; i < 8; i++) {
        *q-- = *p++;
    }
    return value;
}
/* get BINEX 1-4 byte unsigned integer (big endian) --------------------------*/
static int getbnxi(uint8_t *p, uint32_t *val)
{
    int i;
    
    for (*val=0,i=0;i<3;i++) {
        *val=(*val<<7)+(p[i]&0x7F);
        if (!(p[i] & 0x80)) {
            return i + 1;
        }
    }
    *val=(*val<<8)+p[i];
    return 4;
}
/* checksum 8 parity ---------------------------------------------------------*/
static uint8_t csum8(const uint8_t *buff, int len)
{
    uint8_t cs=0;
    int i;
    
    for (i=0;i<len;i++) {
        cs^=buff[i];
    }
    return cs;
}
/* adjust weekly rollover of GPS time ----------------------------------------*/
static gtime_t adjweek(gtime_t time, double tow)
{
    double tow_p;
    int week;
    tow_p=time2gpst(time,&week);
    if (tow < tow_p - 302400.0) {
        tow += 604800.0;
    } else if (tow > tow_p + 302400.0) {
        tow -= 604800.0;
    }
    return gpst2time(week,tow);
}
/* adjust daily rollover of time ---------------------------------------------*/
static gtime_t adjday(gtime_t time, double tod)
{
    double ep[6],tod_p;
    time2epoch(time,ep);
    tod_p=ep[3]*3600.0+ep[4]*60.0+ep[5];
    if (tod < tod_p - 43200.0) {
        tod += 86400.0;
    } else if (tod > tod_p + 43200.0) {
        tod -= 86400.0;
    }
    ep[3]=ep[4]=ep[5]=0.0;
    return timeadd(epoch2time(ep),tod);
}
/* URA value (m) to URA index ------------------------------------------------*/
static int uraindex(double value)
{
    int i;
    for (i = 0; i < 15; i++) {
        if (ura_eph[i] >= value) {
            break;
        }
    }
    return i;
}
/* Galileo SISA value (m) to SISA index --------------------------------------*/
static int sisaindex(double value)
{
    if (value < 0.5) {
        return (int)((value) / 0.01);
    }
    if (value < 1.0) {
        return (int)((value - 0.5) / 0.02) + 50;
    }
    if (value < 2.0) {
        return (int)((value - 1.0) / 0.04) + 75;
    }
    if (value <= 6.0) {
        return (int)((value - 2.0) / 0.16) + 100;
    }
    return 255; /* NAPA */
}
/* decode BINEX mesaage 0x00: site metadata ----------------------------------*/
static int decode_bnx_00(raw_t *raw, uint8_t *buff, int len)
{
    const static double gpst0[]={1980,1,6,0,0,0};
    double x[3];
    char *msg,str[MAXANT];
    uint8_t *p=buff;
    uint32_t min,qsec,src,fid=0,flen=0;
    int i,ret=0;
    
    min =U4(p); p+=4;
    qsec=U1(p); p+=1;
    src =U1(p); p+=1;
    raw->time=timeadd(epoch2time(gpst0),min*60.0+qsec*0.25);
    
    msg=raw->msgtype+strlen(raw->msgtype);
    if (raw->outtype) {
        msg+=sprintf(msg," time=%s src=%d",time_str(raw->time,0),src);
    }
    while (p-buff<len) {
        p+=getbnxi(p,&fid);
        if (p - buff >= len) {
            break;
        }

        if (fid<=0x0c||(fid>=0x0f&&fid<=0x1c)||(fid>=0x20&&fid<=0x22)||
            fid==0x7f) {
            p+=getbnxi(p,&flen); /* field length*/
            sprintf(str,"%.*s",MIN(flen,MAXANT-1),(char *)p);
            p+=flen;
            if (raw->outtype) {
                msg+=sprintf(msg," [%02x]%s",fid,str);
            }
            if (fid == 0x08) {
                strcpy(raw->sta.name, str);
            } else if (fid == 0x09) {
                strcpy(raw->sta.marker, str);
            } else if (fid == 0x17) {
                strcpy(raw->sta.antdes, str);
            } else if (fid == 0x18) {
                strcpy(raw->sta.antsno, str);
            } else if (fid == 0x19) {
                strcpy(raw->sta.rectype, str);
            } else if (fid == 0x1a) {
                strcpy(raw->sta.recsno, str);
            } else if (fid == 0x1b) {
                strcpy(raw->sta.recver, str);
            }
            ret=5;
        }
        else if (fid==0x1d||fid==0x1e||fid==0x1f) {
            if (fid==0x1d||fid==0x1e) {
                p+=getbnxi(p,&flen); /* subfield length */
                p+=flen;
            }
            for (i=0;i<3;i++) {
                x[i]=R8(p); p+=8;
            }
            if (raw->outtype) {
                msg+=sprintf(msg," [%02x]%.3f/%.3f/%.3f",fid,x[0],x[1],x[2]);
            }
            if (fid==0x1d) { /* antenna ECEF X/Y/Z position */
                matcpy(raw->sta.pos,x,3,1);
            }
            else if (fid==0x1e) { /* antenna geographic position */
                x[0]*=D2R;
                x[1]*=D2R;
                pos2ecef(x,raw->sta.pos);
            }
            else if (fid==0x1f) { /* antenna offset (H/E/N) */
                raw->sta.deltype=0; /* (E/N/U) */
                raw->sta.del[0]=x[1];
                raw->sta.del[1]=x[2];
                raw->sta.del[2]=x[0];
            }
            ret=5;
        }
        else {
            trace(NULL,2,"BINEX 0x00: unsupported field fid=0x%02x\n",fid);
            break;
        }
    }
    return ret;
}
/* decode BINEX mesaage 0x01-00: coded (raw bytes) GNSS ephemeris ------------*/
static int decode_bnx_01_00(raw_t *raw, uint8_t *buff, int len)
{
    trace(NULL,2,"BINEX 0x01-00: unsupported message\n");
    return 0;
}
/* decode BINEX mesaage 0x01-01: decoded GPS ephmemeris ----------------------*/
static int decode_bnx_01_01(raw_t *raw, uint8_t *buff, int len)
{
    eph_t eph={0};
    uint8_t *p=buff;
    double tow,ura,sqrtA;
    int prn,sat,flag;
    
    trace(NULL,4,"BINEX 0x01-01: len=%d\n",len);
    
    if (len>=127) {
        prn       =U1(p)+1;      p+=1;
        eph.week  =U2(p);        p+=2;
        tow       =I4(p);        p+=4;
        eph.toes  =I4(p);        p+=4;
        eph.tgd[0]=R4(p);        p+=4;
        eph.iodc  =I4(p);        p+=4;
        eph.f2    =R4(p);        p+=4;
        eph.f1    =R4(p);        p+=4;
        eph.f0    =R4(p);        p+=4;
        eph.iode  =I4(p);        p+=4;
        eph.deln  =R4(p)*SC2RAD; p+=4;
        eph.M0    =R8(p);        p+=8;
        eph.e     =R8(p);        p+=8;
        sqrtA     =R8(p);        p+=8;
        eph.cic   =R4(p);        p+=4;
        eph.crc   =R4(p);        p+=4;
        eph.cis   =R4(p);        p+=4;
        eph.crs   =R4(p);        p+=4;
        eph.cuc   =R4(p);        p+=4;
        eph.cus   =R4(p);        p+=4;
        eph.OMG0  =R8(p);        p+=8;
        eph.omg   =R8(p);        p+=8;
        eph.i0    =R8(p);        p+=8;
        eph.OMGd  =R4(p)*SC2RAD; p+=4;
        eph.idot  =R4(p)*SC2RAD; p+=4;
        ura       =R4(p)*0.1;    p+=4;
        eph.svh   =U2(p);        p+=2;
        flag      =U2(p);
    }
    else {
        trace(NULL,2,"BINEX 0x01-01: length error len=%d\n",len);
        return -1;
    }
    if (!(sat=satno(SYS_GPS,prn))) {
        trace(NULL,2,"BINEX 0x01-01: satellite error prn=%d\n",prn);
        return -1;
    }
    eph.sat=sat;
    eph.type=0; /* ephemeris type = LNAV */
    eph.A=SQR(sqrtA);
    eph.toe=gpst2time(eph.week,eph.toes);
    eph.toc=gpst2time(eph.week,eph.toes);
    eph.ttr=adjweek(eph.toe,tow);
    eph.fit=flag&0xFF;
    eph.flag=(flag>>8)&0x01;
    eph.code=(flag>>9)&0x03;
    eph.sva=uraindex(ura);
    
    if (!strstr(raw->opt,"-EPHALL")) {
        if (raw->nav.eph[eph.sat - 1].iode == eph.iode && raw->nav.eph[eph.sat - 1].iodc == eph.iodc) {
            return 0; /* unchanged */
        }
    }
    raw->nav.eph[sat-1]=eph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* decode BINEX mesaage 0x01-02: decoded GLONASS ephmemeris ------------------*/
static int decode_bnx_01_02(raw_t *raw, uint8_t *buff, int len)
{
    geph_t geph={0};
    uint8_t *p=buff;
    double tod,tof,tau_gps;
    int prn,sat,day,leap;
    
    trace(NULL,4,"BINEX 0x01-02: len=%d\n",len);
    
    if (len>=119) {
        prn        =U1(p)+1;   p+=1;
        day        =U2(p);     p+=2;
        tod        =U4(p);     p+=4;
        geph.taun  =-R8(p);    p+=8;
        geph.gamn  =R8(p);     p+=8;
        tof        =U4(p);     p+=4;
        geph.pos[0]=R8(p)*1E3; p+=8;
        geph.vel[0]=R8(p)*1E3; p+=8;
        geph.acc[0]=R8(p)*1E3; p+=8;
        geph.pos[1]=R8(p)*1E3; p+=8;
        geph.vel[1]=R8(p)*1E3; p+=8;
        geph.acc[1]=R8(p)*1E3; p+=8;
        geph.pos[2]=R8(p)*1E3; p+=8;
        geph.vel[2]=R8(p)*1E3; p+=8;
        geph.acc[2]=R8(p)*1E3; p+=8;
        geph.svh   =U1(p)&7;   p+=1; /* extended SVH (Cn_a+Cn+Bn) */
        geph.frq   =I1(p);     p+=1;
        geph.age   =U1(p);     p+=1;
        leap       =U1(p);     p+=1;
        tau_gps    =R8(p);     p+=8;
        geph.dtaun =R8(p);
    }
    else {
        trace(NULL,2,"BINEX 0x01-02: length error len=%d\n",len);
        return -1;
    }
    if (!(sat=satno(SYS_GLO,prn))) {
        trace(NULL,2,"BINEX 0x01-02: satellite error prn=%d\n",prn);
        return -1;
    }
    if (raw->time.time == 0) {
        return 0;
    }
    geph.sat=sat;
    geph.toe=utc2gpst(adjday(raw->time,tod-10800.0));
    geph.tof=utc2gpst(adjday(raw->time,tof-10800.0));
    geph.iode=(int)(fmod(tod,86400.0)/900.0+0.5);
    
    if (!strstr(raw->opt,"-EPHALL")) {
        if (fabs(timediff(geph.toe, raw->nav.geph[prn - MINPRNGLO].toe)) < 1.0 &&
            geph.svh == raw->nav.geph[prn - MINPRNGLO].svh) {
            return 0;
        }
    }
    raw->nav.geph[prn-1]=geph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* decode BINEX mesaage 0x01-03: decoded SBAS ephmemeris ---------------------*/
static int decode_bnx_01_03(raw_t *raw, uint8_t *buff, int len)
{
    seph_t seph={0};
    uint8_t *p=buff;
    double tow,tod,tof;
    int prn,sat,week,iodn;
    
    trace(NULL,4,"BINEX 0x01-03: len=%d\n",len);
    
    if (len>=98) {
        prn        =U1(p);     p+=1;
        week       =U2(p);     p+=2;
        tow        =U4(p);     p+=4;
        seph.af0   =R8(p);     p+=8;
        tod        =R4(p);     p+=4;
        tof        =U4(p);     p+=4;
        seph.pos[0]=R8(p)*1E3; p+=8;
        seph.vel[0]=R8(p)*1E3; p+=8;
        seph.acc[0]=R8(p)*1E3; p+=8;
        seph.pos[1]=R8(p)*1E3; p+=8;
        seph.vel[1]=R8(p)*1E3; p+=8;
        seph.acc[1]=R8(p)*1E3; p+=8;
        seph.pos[2]=R8(p)*1E3; p+=8;
        seph.vel[2]=R8(p)*1E3; p+=8;
        seph.acc[2]=R8(p)*1E3; p+=8;
        seph.svh   =U1(p);     p+=1;
        seph.sva   =U1(p);     p+=1;
        iodn       =U1(p);
    }
    else {
        trace(NULL,2,"BINEX 0x01-03 length error: len=%d\n",len);
        return -1;
    }
    if (!(sat=satno(SYS_SBS,prn))) {
        trace(NULL,2,"BINEX 0x01-03 satellite error: prn=%d\n",prn);
        return -1;
    }
    seph.sat=sat;
    seph.t0=gpst2time(week,tow);
    seph.tof=adjweek(seph.t0,tof);
    
    if (!strstr(raw->opt,"-EPHALL")) {
        if (fabs(timediff(seph.t0, raw->nav.seph[prn - MINPRNSBS].t0)) < 1.0 &&
            seph.sva == raw->nav.seph[prn - MINPRNSBS].sva) {
            return 0;
        }
    }
    raw->nav.seph[prn-MINPRNSBS]=seph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* decode BINEX mesaage 0x01-04: decoded Galileo ephmemeris ------------------*/
static int decode_bnx_01_04(raw_t *raw, uint8_t *buff, int len)
{
    eph_t eph={0};
    uint8_t *p=buff;
    double tow,ura,sqrtA;
    int prn,sat,set,eph_sel=3; /* ephemeris selection (1:I/NAV+2:F/NAV) */
    
    trace(NULL,4,"BINEX 0x01-04: len=%d\n",len);

    if (strstr(raw->opt, "-GALFNAV")) {
        eph_sel = 1;
    }
    if (strstr(raw->opt, "-GALINAV")) {
        eph_sel = 2;
    }

    if (len>=127) {
        prn       =U1(p)+1;      p+=1;
        eph.week  =U2(p);        p+=2; /* gal-week = gps-week */
        tow       =I4(p);        p+=4;
        eph.toes  =I4(p);        p+=4;
        eph.tgd[0]=R4(p);        p+=4; /* BGD E5a/E1 */
        eph.tgd[1]=R4(p);        p+=4; /* BGD E5b/E1 */
        eph.iode  =I4(p);        p+=4; /* IODnav */
        eph.f2    =R4(p);        p+=4;
        eph.f1    =R4(p);        p+=4;
        eph.f0    =R4(p);        p+=4;
        eph.deln  =R4(p)*SC2RAD; p+=4;
        eph.M0    =R8(p);        p+=8;
        eph.e     =R8(p);        p+=8;
        sqrtA     =R8(p);        p+=8;
        eph.cic   =R4(p);        p+=4;
        eph.crc   =R4(p);        p+=4;
        eph.cis   =R4(p);        p+=4;
        eph.crs   =R4(p);        p+=4;
        eph.cuc   =R4(p);        p+=4;
        eph.cus   =R4(p);        p+=4;
        eph.OMG0  =R8(p);        p+=8;
        eph.omg   =R8(p);        p+=8;
        eph.i0    =R8(p);        p+=8;
        eph.OMGd  =R4(p)*SC2RAD; p+=4;
        eph.idot  =R4(p)*SC2RAD; p+=4;
        ura       =R4(p);        p+=4;
        eph.svh   =U2(p);        p+=2;
        eph.code  =U2(p); /* data source defined as RINEX 3.03 */
    }
    else {
        trace(NULL,2,"BINEX 0x01-04: length error len=%d\n",len);
        return -1;
    }
    if (!(sat=satno(SYS_GAL,prn))) {
        trace(NULL,2,"BINEX 0x01-04: satellite error prn=%d\n",prn);
        return -1;
    }
    set=(eph.code&(1<<8))?1:0; /* 0:I/NAV,1:F/NAV */
    if (!(eph_sel & 1) && set == 0) {
        return 0;
    }
    if (!(eph_sel & 2) && set == 1) {
        return 0;
    }

    eph.sat=sat;
    eph.type=set; /* ephemeris type = I/NAV or F/NAV */
    eph.A=SQR(sqrtA);
    eph.iodc=eph.iode;
    eph.toe=gpst2time(eph.week,eph.toes);
    eph.toc=gpst2time(eph.week,eph.toes);
    eph.ttr=adjweek(eph.toe,tow);
    eph.sva=ura<0.0?(int)(-ura)-1:sisaindex(ura); /* SISA index */
    if (!strstr(raw->opt,"-EPHALL")) {
        if (raw->nav.eph[sat-1+MAXSAT*set].iode==eph.iode&&
            fabs(timediff(raw->nav.eph[sat-1+MAXSAT*set].toe,eph.toe))<1.0&&
            fabs(timediff(raw->nav.eph[sat-1+MAXSAT*set].toc,eph.toc))<1.0) {
            return 0;
        }
    }
    raw->nav.eph[sat-1+MAXSAT*set]=eph;
    raw->ephsat=sat;
    raw->ephset=set;
    return 2;
}
/* BDS signed 10 bit Tgd -> sec ----------------------------------------------*/
static double bds_tgd(int tgd)
{
    tgd&=0x3FF;
    return (tgd&0x200)?-1E-10*((~tgd)&0x1FF):1E-10*(tgd&0x1FF);
}
/* decode BINEX mesaage 0x01-05: decoded Beidou-2/Compass ephmemeris ---------*/
static int decode_bnx_01_05(raw_t *raw, uint8_t *buff, int len)
{
    eph_t eph={0};
    uint8_t *p=buff;
    double tow,toc,sqrtA;
    int prn,sat,flag1,flag2;
    
    trace(NULL,4,"BINEX 0x01-05: len=%d\n",len);
    
    if (len>=117) {
        prn       =U1(p);        p+=1;
        eph.week  =U2(p);        p+=2;
        tow       =I4(p);        p+=4;
        toc       =I4(p);        p+=4;
        eph.toes  =I4(p);        p+=4;
        eph.f2    =R4(p);        p+=4;
        eph.f1    =R4(p);        p+=4;
        eph.f0    =R4(p);        p+=4;
        eph.deln  =R4(p)*SC2RAD; p+=4;
        eph.M0    =R8(p);        p+=8;
        eph.e     =R8(p);        p+=8;
        sqrtA     =R8(p);        p+=8;
        eph.cic   =R4(p);        p+=4;
        eph.crc   =R4(p);        p+=4;
        eph.cis   =R4(p);        p+=4;
        eph.crs   =R4(p);        p+=4;
        eph.cuc   =R4(p);        p+=4;
        eph.cus   =R4(p);        p+=4;
        eph.OMG0  =R8(p);        p+=8;
        eph.omg   =R8(p);        p+=8;
        eph.i0    =R8(p);        p+=8;
        eph.OMGd  =R4(p)*SC2RAD; p+=4;
        eph.idot  =R4(p)*SC2RAD; p+=4;
        flag1     =U2(p);        p+=2;
        flag2     =U4(p);
    }
    else {
        trace(NULL,2,"BINEX 0x01-05: length error len=%d\n",len);
        return -1;
    }
    if (!(sat=satno(SYS_CMP,prn))) {
        trace(NULL,2,"BINEX 0x01-05: satellite error prn=%d\n",prn);
        return 0;
    }
    eph.sat=sat;
    eph.type=(prn>=6&&prn<=58)?0:1; /* ephemeris type = D1 or D2 */
    eph.A=SQR(sqrtA);
    eph.toe=gpst2time(eph.week+1356,eph.toes+14.0); /* bdt -> gpst */
    eph.toc=gpst2time(eph.week+1356,eph.toes+14.0); /* bdt -> gpst */
    eph.ttr=adjweek(eph.toe,tow+14.0); /* bdt -> gpst */
    eph.iodc=(flag1>>1)&0x1F;
    eph.iode=(flag1>>6)&0x1F;
    eph.svh=flag1&0x01;
    eph.sva=flag2&0x0F; /* URA index */
    eph.tgd[0]=bds_tgd(flag2>> 4); /* TGD1 (s) */
    eph.tgd[1]=bds_tgd(flag2>>14); /* TGD2 (s) */
    eph.flag=(flag1>>11)&0x07; /* nav type (0:unknown,1:IGSO/MEO,2:GEO) */
    eph.code=(flag2>>25)&0x7F;
        /* message source (0:unknown,1:B1I,2:B1Q,3:B2I,4:B2Q,5:B3I,6:B3Q)*/
    
    if (!strstr(raw->opt,"-EPHALL")) {
        if (fabs(timediff(raw->nav.eph[sat - 1].toe, eph.toe)) < 1.0) {
            return 0;
        }
    }
    raw->nav.eph[sat-1]=eph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* decode BINEX mesaage 0x01-06: decoded QZSS ephmemeris ---------------------*/
static int decode_bnx_01_06(raw_t *raw, uint8_t *buff, int len)
{
    eph_t eph={0};
    uint8_t *p=buff;
    double tow,ura,sqrtA;
    int prn,sat,flag;
    
    trace(NULL,4,"BINEX 0x01-06: len=%d\n",len);
    
    if (len>=127) {
        prn       =U1(p);        p+=1;
        eph.week  =U2(p);        p+=2;
        tow       =I4(p);        p+=4;
        eph.toes  =I4(p);        p+=4;
        eph.tgd[0]=R4(p);        p+=4;
        eph.iodc  =I4(p);        p+=4;
        eph.f2    =R4(p);        p+=4;
        eph.f1    =R4(p);        p+=4;
        eph.f0    =R4(p);        p+=4;
        eph.iode  =I4(p);        p+=4;
        eph.deln  =R4(p)*SC2RAD; p+=4;
        eph.M0    =R8(p);        p+=8;
        eph.e     =R8(p);        p+=8;
        sqrtA     =R8(p);        p+=8;
        eph.cic   =R4(p);        p+=4;
        eph.crc   =R4(p);        p+=4;
        eph.cis   =R4(p);        p+=4;
        eph.crs   =R4(p);        p+=4;
        eph.cuc   =R4(p);        p+=4;
        eph.cus   =R4(p);        p+=4;
        eph.OMG0  =R8(p);        p+=8;
        eph.omg   =R8(p);        p+=8;
        eph.i0    =R8(p);        p+=8;
        eph.OMGd  =R4(p)*SC2RAD; p+=4;
        eph.idot  =R4(p)*SC2RAD; p+=4;
        ura       =R4(p)*0.1;    p+=4;
        eph.svh   =U2(p);        p+=2;
        flag      =U2(p);
    }
    else {
        trace(NULL,2,"BINEX 0x01-06: length error len=%d\n",len);
        return -1;
    }
    if (!(sat=satno(SYS_QZS,prn))) {
        trace(NULL,2,"BINEX 0x01-06: satellite error prn=%d\n",prn);
        return 0;
    }
    eph.sat=sat;
    eph.type=0; /* ephemeris type = LNAV */
    eph.A=SQR(sqrtA);
    eph.toe=gpst2time(eph.week,eph.toes);
    eph.toc=gpst2time(eph.week,eph.toes);
    eph.ttr=adjweek(eph.toe,tow);
    eph.fit=(flag&0x01)?0.0:2.0; /* 0:2hr,1:>2hr */
    eph.sva=uraindex(ura);
    eph.code=2; /* codes on L2 channel */
    
    if (!strstr(raw->opt,"-EPHALL")) {
        if (raw->nav.eph[sat - 1].iode == eph.iode && raw->nav.eph[sat - 1].iodc == eph.iodc) {
            return 0;
        }
    }
    raw->nav.eph[sat-1]=eph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* decode BINEX mesaage 0x01-07: decoded IRNSS ephmemeris --------------------*/
static int decode_bnx_01_07(raw_t *raw, uint8_t *buff, int len)
{
    eph_t eph={0};
    uint8_t *p=buff;
    double tow,toc,sqrtA;
    int prn,sat,flag,iodec;
    
    trace(NULL,4,"BINEX 0x01-07: len=%d\n",len);
    
    if (len>=114) {
        prn       =U1(p);        p+=1;
        eph.week  =U2(p)+1024;   p+=2; /* IRNSS week -> GPS week */
        tow       =I4(p);        p+=4;
        toc       =I4(p);        p+=4;
        eph.toes  =I4(p);        p+=4;
        eph.f2    =R4(p);        p+=4;
        eph.f1    =R4(p);        p+=4;
        eph.f0    =R4(p);        p+=4;
        eph.deln  =R4(p)*SC2RAD; p+=4;
        eph.M0    =R8(p);        p+=8;
        eph.e     =R8(p);        p+=8;
        sqrtA     =R8(p);        p+=8;
        eph.cic   =R4(p);        p+=4;
        eph.crc   =R4(p);        p+=4;
        eph.cis   =R4(p);        p+=4;
        eph.crs   =R4(p);        p+=4;
        eph.cuc   =R4(p);        p+=4;
        eph.cus   =R4(p);        p+=4;
        eph.OMG0  =R8(p);        p+=8;
        eph.omg   =R8(p);        p+=8;
        eph.i0    =R8(p);        p+=8;
        eph.OMGd  =R4(p)*SC2RAD; p+=4;
        eph.idot  =R4(p)*SC2RAD; p+=4;
        flag      =U1(p);        p+=1;
        iodec     =U2(p);
    }
    else {
        trace(NULL,2,"BINEX 0x01-07: length error len=%d\n",len);
        return -1;
    }
    if (!(sat=satno(SYS_IRN,prn))) {
        trace(NULL,2,"BINEX 0x01-07: satellite error prn=%d\n",prn);
        return 0;
    }
    eph.sat=sat;
    eph.type=0; /* ephemeris type = LNAV */
    eph.A=SQR(sqrtA);
    eph.toe=gpst2time(eph.week,eph.toes);
    eph.toc=adjweek(eph.toe,toc);
    eph.ttr=adjweek(eph.toe,tow);
    eph.sva=flag&0x0F; /* URA index (0-15) */
    eph.svh=((flag&0x10)?2:0)+((flag&0x20)?1:0);
    eph.iode=eph.iodc=iodec&0xFF;
    eph.tgd[0]=(int8_t)(iodec>>8)*P2_31;
    
    if (!strstr(raw->opt,"-EPHALL")) {
        if (raw->nav.eph[sat - 1].iode == eph.iode && fabs(timediff(raw->nav.eph[sat - 1].toe, eph.toe)) < 1.0 &&
            fabs(timediff(raw->nav.eph[sat - 1].toc, eph.toc)) < 1.0) {
            return 0;
        }
    }
    raw->nav.eph[sat-1]=eph;
    raw->ephsat=sat;
    raw->ephset=0;
    return 2;
}
/* decode BINEX mesaage 0x01-14: upgraded decoded Galileo ephmemeris ---------*/
static int decode_bnx_01_14(raw_t *raw, uint8_t *buff, int len)
{
    eph_t eph={0};
    uint8_t *p=buff;
    double tow,ura,sqrtA;
    int prn,sat,tocs,set,eph_sel=3;
    
    trace(NULL,4,"BINEX 0x01-14: len=%d\n",len);

    if (strstr(raw->opt, "-GALFNAV")) {
        eph_sel = 1;
    }
    if (strstr(raw->opt, "-GALINAV")) {
        eph_sel = 2;
    }

    if (len>=135) {
        prn       =U1(p)+1;      p+=1;
        eph.week  =U2(p);        p+=2; /* gal-week = gps-week */
        tow       =I4(p);        p+=4;
        tocs      =I4(p);        p+=4;
        eph.toes  =I4(p);        p+=4;
        eph.tgd[0]=R4(p);        p+=4; /* BGD E5a/E1 */
        eph.tgd[1]=R4(p);        p+=4; /* BGD E5b/E1 */
        eph.iode  =I4(p);        p+=4; /* IODnav */
        eph.f2    =R4(p);        p+=4;
        eph.f1    =R4(p);        p+=4;
        eph.f0    =R8(p);        p+=8;
        eph.deln  =R4(p)*SC2RAD; p+=4;
        eph.M0    =R8(p);        p+=8;
        eph.e     =R8(p);        p+=8;
        sqrtA     =R8(p);        p+=8;
        eph.cic   =R4(p);        p+=4;
        eph.crc   =R4(p);        p+=4;
        eph.cis   =R4(p);        p+=4;
        eph.crs   =R4(p);        p+=4;
        eph.cuc   =R4(p);        p+=4;
        eph.cus   =R4(p);        p+=4;
        eph.OMG0  =R8(p);        p+=8;
        eph.omg   =R8(p);        p+=8;
        eph.i0    =R8(p);        p+=8;
        eph.OMGd  =R4(p)*SC2RAD; p+=4;
        eph.idot  =R4(p)*SC2RAD; p+=4;
        ura       =R4(p);        p+=4;
        eph.svh   =U2(p);        p+=2;
        eph.code  =U2(p); /* data source defined as RINEX 3.03 */
    }
    else {
        trace(NULL,2,"BINEX 0x01-14: length error len=%d\n",len);
        return -1;
    }
    if (!(sat=satno(SYS_GAL,prn))) {
        trace(NULL,2,"BINEX 0x01-14: satellite error prn=%d\n",prn);
        return -1;
    }
    set=(eph.code&(1<<8))?1:0; /* 0:I/NAV,1:F/NAV */
    if (!(eph_sel & 1) && set == 0) {
        return 0;
    }
    if (!(eph_sel & 2) && set == 1) {
        return 0;
    }

    eph.sat=sat;
    eph.type=set; /* ephemeris type = I/NAV or F/NAV */
    eph.A=SQR(sqrtA);
    eph.iodc=eph.iode;
    eph.toe=gpst2time(eph.week,eph.toes);
    eph.toc=gpst2time(eph.week,tocs);
    eph.ttr=adjweek(eph.toe,tow);
    eph.sva=ura<0.0?(int)(-ura)-1:sisaindex(ura); /* SISA index */
    if (!strstr(raw->opt,"-EPHALL")) {
        if (raw->nav.eph[sat-1+MAXSAT*set].iode==eph.iode&&
            fabs(timediff(raw->nav.eph[sat-1+MAXSAT*set].toe,eph.toe))<1.0&&
            fabs(timediff(raw->nav.eph[sat-1+MAXSAT*set].toc,eph.toc))<1.0) {
            return 0;
        }
    }
    raw->nav.eph[sat-1+MAXSAT*set]=eph;
    raw->ephsat=sat;
    raw->ephset=set;
    return 2;
}
/* decode BINEX mesaage 0x01: GNSS navigation information --------------------*/
static int decode_bnx_01(raw_t *raw, uint8_t *buff, int len)
{
    int srec=U1(buff),prn=U1(buff+1);
    
    if (raw->outtype) {
        prn=srec==0x01||srec==0x02||srec==0x04?prn+1:(srec==0x00?0:prn);
        sprintf(raw->msgtype+strlen(raw->msgtype)," subrec=%02X prn=%d",srec,
                prn);
    }
    switch (srec) {
        case 0x00: return decode_bnx_01_00(raw,buff+1,len-1);
        case 0x01: return decode_bnx_01_01(raw,buff+1,len-1);
        case 0x02: return decode_bnx_01_02(raw,buff+1,len-1);
        case 0x03: return decode_bnx_01_03(raw,buff+1,len-1);
        case 0x04: return decode_bnx_01_04(raw,buff+1,len-1);
        case 0x05: return decode_bnx_01_05(raw,buff+1,len-1);
        case 0x06: return decode_bnx_01_06(raw,buff+1,len-1);
        case 0x07: return decode_bnx_01_07(raw,buff+1,len-1);
        case 0x14: return decode_bnx_01_14(raw,buff+1,len-1);
    }
    return 0;
}
/* decode BINEX mesaage 0x02: generalized GNSS data --------------------------*/
static int decode_bnx_02(raw_t *raw, uint8_t *buff, int len)
{
    trace(NULL,2,"BINEX 0x02: unsupported message\n");
    return 0;
}
/* decode BINEX mesaage 0x03: generalized ancillary site data ----------------*/
static int decode_bnx_03(raw_t *raw, uint8_t *buff, int len)
{
    trace(NULL,2,"BINEX 0x03: unsupported message\n");
    return 0;
}
/* decode BINEX mesaage 0x7d: receiver internal state prototyping ------------*/
static int decode_bnx_7d(raw_t *raw, uint8_t *buff, int len)
{
    trace(NULL,2,"BINEX 0x7d: unsupported message\n");
    return 0;
}
/* decode BINEX mesaage 0x7e: ancillary site data prototyping ----------------*/
static int decode_bnx_7e(raw_t *raw, uint8_t *buff, int len)
{
    trace(NULL,2,"BINEX 0x7e: unsupported message\n");
    return 0;
}
/* decode BINEX mesaage 0x7f-00: JPL fiducial site ---------------------------*/
static int decode_bnx_7f_00(raw_t *raw, uint8_t *buff, int len)
{
    trace(NULL,2,"BINEX 0x7f-00: unsupported message\n");
    return 0;
}
/* decode BINEX mesaage 0x7f-01: UCAR COSMIC ---------------------------------*/
static int decode_bnx_7f_01(raw_t *raw, uint8_t *buff, int len)
{
    trace(NULL,2,"BINEX 0x7f-01: unsupported message\n");
    return 0;
}
/* decode BINEX mesaage 0x7f-02: Trimble 4700 --------------------------------*/
static int decode_bnx_7f_02(raw_t *raw, uint8_t *buff, int len)
{
    trace(NULL,2,"BINEX 0x7f-02: unsupported message\n");
    return 0;
}
/* decode BINEX mesaage 0x7f-03: Trimble NetRS -------------------------------*/
static int decode_bnx_7f_03(raw_t *raw, uint8_t *buff, int len)
{
    trace(NULL,2,"BINEX 0x7f-03: unsupported message\n");
    return 0;
}
/* decode BINEX mesaage 0x7f-04: Trimble NetRS -------------------------------*/
static int decode_bnx_7f_04(raw_t *raw, uint8_t *buff, int len)
{
    trace(NULL,2,"BINEX 0x7f-04: unsupported message\n");
    return 0;
}
/* decode BINEX mesaage 0x7f-05: Trimble NetR8 obs data ----------------------*/
static uint8_t *decode_bnx_7f_05_obs(raw_t *raw, uint8_t *buff, int sat,
                                     int nobs, obsd_t *data)
{
    const uint8_t codes_gps[32]={
        CODE_L1C ,CODE_L1C ,CODE_L1P ,CODE_L1W ,CODE_L1Y ,CODE_L1M , /*  0- 5 */
        CODE_L1L ,CODE_L1N ,CODE_L1S ,CODE_L1X ,CODE_L2W ,CODE_L2C , /*  6-11 */
        CODE_L2D ,CODE_L2S ,CODE_L2L ,CODE_L2X ,CODE_L2P ,CODE_L2W , /* 12-17 */
        CODE_L2Y ,CODE_L2M ,CODE_L2N ,CODE_NONE,CODE_NONE,CODE_L5X , /* 18-23 */
        CODE_L5I ,CODE_L5Q ,CODE_L5X ,CODE_NONE,CODE_NONE,CODE_NONE, /* 24-29 */
        CODE_NONE,CODE_NONE                                          /* 30-31 */
    };
    const uint8_t codes_glo[32]={
        CODE_L1C ,CODE_L1C ,CODE_L1P ,CODE_NONE,CODE_NONE,CODE_L4A , /*  0- 5 */
        CODE_L4B ,CODE_L4X ,CODE_NONE,CODE_NONE,CODE_L2C ,CODE_L2C , /*  6-11 */
        CODE_L2P ,CODE_L3X ,CODE_L3I ,CODE_L3Q ,CODE_L3X ,CODE_NONE, /* 12-17 */
        CODE_NONE,CODE_L6X ,CODE_L6A ,CODE_L6B ,CODE_L6X ,CODE_NONE, /* 18-23 */
        CODE_NONE,CODE_NONE,CODE_NONE,CODE_NONE,CODE_NONE,CODE_NONE, /* 24-29 */
        CODE_NONE,CODE_NONE                                          /* 30-31 */
    };
    const uint8_t codes_gal[32]={
        CODE_L1C ,CODE_L1A ,CODE_L1B ,CODE_L1C ,CODE_L1X ,CODE_L1Z , /*  0- 5 */
        CODE_L5X ,CODE_L5I ,CODE_L5Q ,CODE_L5X ,CODE_L7X ,CODE_L7I , /*  6-11 */
        CODE_L7Q ,CODE_L7X ,CODE_L8X ,CODE_L8I ,CODE_L8Q ,CODE_L8X , /* 12-17 */
        CODE_L6X ,CODE_L6A ,CODE_L6B ,CODE_L6C ,CODE_L6X ,CODE_L6Z , /* 18-23 */
        CODE_NONE,CODE_NONE,CODE_NONE,CODE_NONE,CODE_NONE,CODE_NONE, /* 24-29 */
        CODE_NONE,CODE_NONE                                          /* 30-31 */
    };
    const uint8_t codes_sbs[32]={
        CODE_L1C ,CODE_L1C ,CODE_NONE,CODE_NONE,CODE_NONE,CODE_NONE, /*  0- 5 */
        CODE_L5X ,CODE_L5I ,CODE_L5Q ,CODE_L5X                       /*  6- 9 */
    };
    const uint8_t codes_cmp[32]={
        CODE_L2X ,CODE_L2I ,CODE_L2Q ,CODE_L2X ,CODE_L7X ,CODE_L7I , /*  0- 5 */
        CODE_L7Q ,CODE_L7X ,CODE_L6X ,CODE_L6I ,CODE_L6Q ,CODE_L6X , /*  6-11 */
        CODE_L1X ,CODE_L1D ,CODE_L1P ,CODE_L1X ,CODE_L5X ,CODE_L5D , /* 12-17 */
        CODE_L5P ,CODE_L5X ,CODE_L1S ,CODE_L1L ,CODE_L1Z ,CODE_L7D , /* 18-23 */
        CODE_L7P ,CODE_L7Z ,CODE_L8D ,CODE_L8P ,CODE_L8X ,CODE_L6D , /* 24-29 */
        CODE_L6P ,CODE_L6Z                                           /* 30-31 */
    };
    const uint8_t codes_qzs[32]={
        CODE_L1C ,CODE_L1C ,CODE_L1S ,CODE_L1L ,CODE_L1X ,CODE_NONE, /*  0- 5 */
        CODE_NONE,CODE_L2X ,CODE_L2S ,CODE_L2L ,CODE_L2X ,CODE_NONE, /*  6-11 */
        CODE_NONE,CODE_L5X ,CODE_L5I ,CODE_L5Q ,CODE_L5X ,CODE_NONE, /* 12-17 */
        CODE_NONE,CODE_L6X ,CODE_L6S ,CODE_L6L ,CODE_L6X ,CODE_L6E , /* 18-23 */
        CODE_L6Z ,CODE_L5D ,CODE_L5P ,CODE_L5Z ,CODE_NONE,CODE_NONE, /* 24-29 */
        CODE_L1Z ,CODE_L1B                                           /* 30-31 */
    };
    const uint8_t codes_irn[32]={
        CODE_L5X ,CODE_L5A ,CODE_L5B ,CODE_L5C ,CODE_L5X ,CODE_L9X , /*  0- 5 */
        CODE_L9A ,CODE_L9B ,CODE_L9C ,CODE_L9X                       /*  6- 9 */
    };
    const uint8_t *codes=NULL;
    double range[8],phase[8],cnr[8],dopp[8]={0},acc,freq;
    uint8_t *p=buff;
    uint8_t flag,flags[4];
    int i,j,k,sys,prn,fcn=-10,code[8],slip[8],pri[8],idx[8];
    int slipcnt[8]={0},mask[8]={0};
    
    trace(NULL,5,"decode_bnx_7f_05_obs: sat=%2d nobs=%2d\n",sat,nobs);
    
    sys=satsys(sat,&prn);
    
    switch (sys) {
        case SYS_GPS: codes=codes_gps; break;
        case SYS_GLO: codes=codes_glo; break;
        case SYS_GAL: codes=codes_gal; break;
        case SYS_QZS: codes=codes_qzs; break;
        case SYS_SBS: codes=codes_sbs; break;
        case SYS_CMP: codes=codes_cmp; break;
        case SYS_IRN: codes=codes_irn; break;
        default: codes=NULL; break; /* skip unsupported sat data */
    }
    for (i=0;i<nobs;i++) {
        flag   =getbitu(p,0,1);
        slip[i]=getbitu(p,2,1);
        code[i]=getbitu(p,3,5); p++;

        for (j = 0; j < 4; j++) {
            flags[j] = 0;
        }

        for (j=0;flag&&j<4;j++) {
            flag=U1(p++);
            flags[flag&0x03]=flag&0x7F;
            flag&=0x80;
        }
        if (flags[2]) {
            fcn=getbits(flags+2,2,4);
            if (sys==SYS_GLO&&!raw->nav.glo_fcn[prn-1]) {
                raw->nav.glo_fcn[prn-1]=fcn+8; /* fcn+8 */
            }
        }
        acc=(flags[0]&0x20)?0.0001:0.00002; /* phase accuracy */
        
        cnr[i]=U1(p++)*0.4;
        
        if (i==0) {
            cnr[i]+=getbits(p,0,2)*0.1;
            range[i]=getbitu(p,2,32)*0.064+getbitu(p,34,6)*0.001; p+=5;
        }
        else if (flags[0]&0x40) {
            cnr[i]+=getbits(p,0,2)*0.1;
            range[i]=range[0]+getbits(p,4,20)*0.001; p+=3;
        }
        else {
            range[i]=range[0]+getbits(p,0,16)*0.001; p+=2;
        }
        if (flags[0]&0x40) {
            phase[i]=range[i]+getbits(p,0,24)*acc; p+=3;
        }
        else {
            cnr[i]+=getbits(p,0,2)*0.1;
            phase[i]=range[i]+getbits(p,2,22)*acc; p+=3;
        }
        if (flags[0]&0x04) {
            dopp[i]=getbits(p,0,24)/256.0; p+=3;
        }
        if (flags[0]&0x08) {
            if (flags[0]&0x10) {
                slipcnt[i]=U2(p); p+=2;
            }
            else {
                slipcnt[i]=U1(p); p+=1;
            }
        }
        trace(NULL,5,"(%d) CODE=%2d S=%d F=%02X %02X %02X %02X\n",i+1,
              code[i],slip[i],flags[0],flags[1],flags[2],flags[3]);
        trace(NULL,5,"(%d) P=%13.3f L=%13.3f D=%7.1f SNR=%4.1f SCNT=%2d\n",
              i+1,range[i],phase[i],dopp[i],cnr[i],slipcnt[i]);
    }
    if (!codes) {
        data->sat=0;
        return p;
    }
    data->time=raw->time;
    data->sat=sat;
    
    /* get code priority */
    for (i=0;i<nobs;i++) {
        idx[i]=code2freq_idx(sys,codes[code[i]]);
        pri[i]=getcodepri(sys,codes[code[i]],raw->opt);
    }
    for (i=0;i<NFREQ;i++) {
        for (j=0,k=-1;j<nobs;j++) {
            if (idx[j] == i && (k < 0 || pri[j] > pri[k])) {
                k = j;
            }
        }
        if (k<0) {
            data->P[i]=data->L[i]=0.0;
            data->D[i]=0.0f;
            data->SNR[i]=data->LLI[i]=0;
            data->code[i]=CODE_NONE;
        }
        else {
            freq=code2freq(sys,codes[code[k]],fcn);
            data->P[i]=range[k];
            data->L[i]=phase[k]*freq/CLIGHT;
            data->D[i]=dopp[k];
            data->SNR[i]=(uint16_t)(cnr[k]/SNR_UNIT+0.5);
            data->code[i]=codes[code[k]];
            data->LLI[i]=slip[k]?1:0;
            mask[k]=1;
        }
    }
    for (;i<NFREQ+NEXOBS;i++) {
        for (k=0;k<nobs;k++) {
            if (!mask[k]) {
                break;
            }
        }
        if (k>=nobs) {
            data->P[i]=data->L[i]=0.0;
            data->D[i]=0.0f;
            data->SNR[i]=data->LLI[i]=0;
            data->code[i]=CODE_NONE;
        }
        else {
            freq=code2freq(sys,codes[code[k]],fcn);
            data->P[i]=range[k];
            data->L[i]=phase[k]*freq/CLIGHT;
            data->D[i]=dopp[k];
            data->SNR[i]=(uint16_t)(cnr[k]/SNR_UNIT+0.5);
            data->code[i]=codes[code[k]];
            data->LLI[i]=slip[k]?1:0;
            mask[k]=1;
        }
    }
    return p;
}
/* decode BINEX mesaage 0x7f-05: Trimble NetR8 -------------------------------*/
static int decode_bnx_7f_05(raw_t *raw, uint8_t *buff, int len)
{
    obsd_t data={{0}};
    double clkoff=0.0,toff[16]={0};
    char *msg;
    uint8_t *p=buff;
    uint32_t flag;
    int i,nsat,nobs,prn,sys,sat,clkrst=0,rsys=0,nsys=0,tsys[16]={0};
    
    trace(NULL,4,"decode_bnx_7f_05\n");
    
    raw->obs.n=0;
    flag=U1(p++);
    nsat=(int)(flag&0x3F)+1;
    
    if (flag&0x80) { /* rxclkoff */
        clkrst=getbitu(p,0, 2);
        clkoff=getbits(p,2,22)*1E-9; p+=3;
    }
    if (flag&0x40) { /* systime */
        nsys=getbitu(p,0,4);
        rsys=getbitu(p,4,4); p++;
        for (i=0;i<nsys;i++) {
            toff[i]=getbits(p,0,24)*1E-9;
            tsys[i]=getbitu(p,28,4); p+=4;
        }
    }
    for (i=0;i<nsat;i++) {
        prn =U1(p++);
        nobs=getbitu(p,1,3);
        sys =getbitu(p,4,4); p++;
        
        trace(NULL,5,"BINEX 0x7F-05 PRN=%3d SYS=%d NOBS=%d\n",prn,sys,nobs);
        
        switch (sys) {
            case 0: sat=satno(SYS_GPS,prn); break;
            case 1: sat=satno(SYS_GLO,prn); break;
            case 2: sat=satno(SYS_SBS,prn); break;
            case 3: sat=satno(SYS_GAL,prn); break;
            case 4: sat=satno(SYS_CMP,prn); break;
            case 5: sat=satno(SYS_QZS,prn); break;
            case 6: sat=satno(SYS_IRN,prn); break;
            default: sat=0; break;
        }
        /* decode BINEX mesaage 0x7F-05 obs data */
        if (!(p = decode_bnx_7f_05_obs(raw, p, sat, nobs, &data))) {
            return -1;
        }

        if ((int)(p-buff)>len) {
            trace(NULL,2,"BINEX 0x7F-05 length error: nsat=%2d len=%d\n",nsat,len);
            return -1;
        }
        /* save obs data to obs buffer */
        if (data.sat&&raw->obs.n<MAXOBS) {
            raw->obs.data[raw->obs.n++]=data;
        }
    }
    if (raw->outtype) {
        msg=raw->msgtype+strlen(raw->msgtype);
        sprintf(msg," nsat=%2d",nsat);
    }
    return raw->obs.n>0?1:0;
}
/* decode BINEX mesaage 0x7f: GNSS data prototyping --------------------------*/
static int decode_bnx_7f(raw_t *raw, uint8_t *buff, int len)
{
    const static double gpst0[]={1980,1,6,0,0,0};
    char *msg;
    uint8_t *p=buff;
    uint32_t srec,min,msec;
    
    srec=U1(p); p+=1; /* subrecord ID */
    min =U4(p); p+=4;
    msec=U2(p); p+=2;
    raw->time=timeadd(epoch2time(gpst0),min*60.0);
    raw->time=timeadd(raw->time,msec*0.001);
    
    if (raw->outtype) {
        msg=raw->msgtype+strlen(raw->msgtype);
        sprintf(msg," subrec=%02X time%s",srec,time_str(raw->time,3));
    }
    switch (srec) {
        case 0x00: return decode_bnx_7f_00(raw,buff+7,len-7);
        case 0x01: return decode_bnx_7f_01(raw,buff+7,len-7);
        case 0x02: return decode_bnx_7f_02(raw,buff+7,len-7);
        case 0x03: return decode_bnx_7f_03(raw,buff+7,len-7);
        case 0x04: return decode_bnx_7f_04(raw,buff+7,len-7);
        case 0x05: return decode_bnx_7f_05(raw,buff+7,len-7);
    }
    return 0;
}
/* decode BINEX mesaage ------------------------------------------------------*/
static int decode_bnx(raw_t *raw)
{
    uint32_t len,cs1,cs2;
    int rec,len_h;
    
    rec=raw->buff[1]; /* record ID */
    
    /* record and header length */
    len_h=getbnxi(raw->buff+2,&len);
    
    trace(NULL,5,"decode_bnx: rec=%02x len=%d\n",rec,len);
    
    /* check parity */
    if (raw->len-1<128) {
        cs1=U1(raw->buff+raw->len);
        cs2=csum8(raw->buff+1,raw->len-1);
    }
    else {
        cs1=U2(raw->buff+raw->len);
        cs2=rtk_crc16(raw->buff+1,raw->len-1);
    }
    if (cs1!=cs2) {
        trace(NULL,2,"BINEX 0x%02X parity error CS=%X %X\n",rec,cs1,cs2);
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype,"BINEX 0x%02X (%4d)",rec,raw->len);
    }
    /* decode BINEX message record */
    switch (rec) {
        case 0x00: return decode_bnx_00(raw,raw->buff+2+len_h,len);
        case 0x01: return decode_bnx_01(raw,raw->buff+2+len_h,len);
        case 0x02: return decode_bnx_02(raw,raw->buff+2+len_h,len);
        case 0x03: return decode_bnx_03(raw,raw->buff+2+len_h,len);
        case 0x7d: return decode_bnx_7d(raw,raw->buff+2+len_h,len);
        case 0x7e: return decode_bnx_7e(raw,raw->buff+2+len_h,len);
        case 0x7f: return decode_bnx_7f(raw,raw->buff+2+len_h,len);
    }
    return 0;
}
/* synchronize BINEX message -------------------------------------------------*/
static int sync_bnx(uint8_t *buff, uint8_t data)
{
    buff[0]=buff[1]; buff[1]=data;
    
    return buff[0]==BNXSYNC2&&
           (buff[1]==0x00||buff[1]==0x01||buff[1]==0x02||buff[1]==0x03||
            buff[1]==0x7D||buff[1]==0x7E||buff[1]==0x7F);
}
/* input BINEX message from stream ---------------------------------------------
* fetch next BINEX data and input a message from stream
* args   : raw_t *raw       IO  receiver raw data control struct
*          uint8_t data     I   stream data (1 byte)
* return : status (-1: error message, 0: no message, 1: input observation data,
*                  2: input ephemeris, 5: input station pos/ant parameters)
* notes  : support only the following message (ref [1])
*
*          - big-endian, regular CRC, forward record (sync=0xE2)
*          - record-subrecord:
*            0x00   : site metadata (monument,marker,ref point,setup)
*            0x01-01: decoded GPS ephemeris
*            0x01-02: decoded GLONASS ephemeris
*            0x01-03: decoded SBAS ephemeris
*            0x01-04: decoded Galileo ephemeris
*            0x01-05: decoded BDS-2/compass ephemeris
*            0x01-06: decoded QZSS ephemeris
*            0x01-07: decoded IRNSS ephemeris
*            0x01-14: decoded upgraded Galileo ephemeris
*            0x7f-05: GNSS data prototyping - Trimble NetR8
*
*          to specify input options, set rtcm->opt to the following option
*          strings separated by spaces.
*
*          -EPHALL  : input all ephemerides
*          -GLss    : select signal ss for GPS (ss=1C,1P,...)
*          -RLss    : select signal ss for GLO (ss=1C,1P,...)
*          -ELss    : select signal ss for GAL (ss=1C,1B,...)
*          -JLss    : select signal ss for QZS (ss=1C,2C,...)
*          -CLss    : select signal ss for BDS (ss=2I,2X,...)
*          -GALINAV : select I/NAV for Galileo ephemeris (default: all)
*          -GALFNAV : select F/NAV for Galileo ephemeris (default: all)
*-----------------------------------------------------------------------------*/
extern int input_bnx(raw_t *raw, uint8_t data)
{
    uint32_t len;
    int len_h,len_c;
    
    trace(NULL,5,"input_bnx: data=%02x\n",data);
    
    /* synchronize BINEX message */
    if (raw->nbyte==0) {
        if (!sync_bnx(raw->buff, data)) {
            return 0;
        }
        raw->nbyte=2;
        return 0;
    }
    raw->buff[raw->nbyte++]=data;
    if (raw->nbyte < 4) {
        return 0;
    }

    len_h=getbnxi(raw->buff+2,&len);
    
    raw->len=len+len_h+2; /* length without CRC */
    
    if (raw->len-1>4096) {
        trace(NULL,2,"BINEX length error: len=%d\n",raw->len-1);
        raw->nbyte=0;
        return -1;
    }
    len_c=(raw->len-1<128)?1:2;

    if (raw->nbyte < (int)(raw->len + len_c)) {
        return 0;
    }
    raw->nbyte=0;
    
    /* decode BINEX message */
    return decode_bnx(raw);
}
/* input BINEX message from file -----------------------------------------------
* fetch next BINEX data and input a message from file
* args   : raw_t  *raw      IO  receiver raw data control struct
*          FILE   *fp       I   file pointer
* return : status(-2: end of file, -1...9: same as above)
*-----------------------------------------------------------------------------*/
extern int input_bnxf(raw_t *raw, FILE *fp)
{
    uint32_t len;
    int i,data,len_h,len_c;

    trace(NULL,4,"input_bnxf\n");

    if (raw->nbyte==0) {
        for (i=0;;i++) {
            if ((data = fgetc(fp)) == EOF) {
                return -2;
            }
            if (sync_bnx(raw->buff, (uint8_t)data)) {
                break;
            }
            if (i >= 4096) {
                return 0;
            }
        }
    }
    if (fread(raw->buff + 2, 1, 4, fp) < 4) {
        return -2;
    }

    len_h=getbnxi(raw->buff+2,&len);

    raw->len=len+len_h+2;

    if (raw->len-1>4096) {
        trace(NULL,2,"BINEX length error: len=%d\n",raw->len-1);
        raw->nbyte=0;
        return -1;
    }
    len_c=(raw->len-1<128)?1:2;

    if (fread(raw->buff+6,1,raw->len+len_c-6,fp)<(size_t)(raw->len+len_c-6)) {
        return -2;
    }
    raw->nbyte=0;

    /* decode BINEX message */
    return decode_bnx(raw);
}

/*============================================================================
 * BINEX file reading functions (post-processing)
 *
 * Ported from upstream claslib/src/rcv/binex.c lines 1305-1601.
 *===========================================================================*/

#define NINCOBS 262144  /* incremental number of obs data */

/* set string without padding ------------------------------------------------*/
static void setstr_bnx(char *dst, const char *src, int n)
{
    char *p=dst;
    const char *q=src;
    while (*q && q < src + n) {
        *p++ = *q++;
    }
    *p--='\0';
    while (p >= dst && *p == ' ') {
        *p-- = '\0';
    }
}
/* save cycle-slip flags -----------------------------------------------------*/
static void saveslips(unsigned char slips[][NFREQ], obsd_t *data)
{
    int i;
    for (i=0;i<NFREQ;i++) {
        if (data->LLI[i] & 1) {
            slips[data->sat - 1][i] |= 1;
        }
    }
}
/* restore cycle-slip flags --------------------------------------------------*/
static void restslips(unsigned char slips[][NFREQ], obsd_t *data)
{
    int i;
    for (i=0;i<NFREQ;i++) {
        if (slips[data->sat - 1][i] & 1) {
            data->LLI[i] |= 1;
        }
        slips[data->sat-1][i]=0;
    }
}
/* add observation data ------------------------------------------------------*/
static int addobsdata(obs_t *obs, const obsd_t *data)
{
    obsd_t *obs_data;

    if (obs->nmax<=obs->n) {
        if (obs->nmax <= 0) {
            obs->nmax = NINCOBS;
        } else {
            obs->nmax *= 2;
        }
        if (!(obs_data=(obsd_t *)realloc(obs->data,sizeof(obsd_t)*obs->nmax))) {
            trace(NULL,1,"addobsdata: memalloc error n=%dx%d\n",
                  (int)sizeof(obsd_t),obs->nmax);
            free(obs->data); obs->data=NULL; obs->n=obs->nmax=0;
            return -1;
        }
        obs->data=obs_data;
    }
    obs->data[obs->n++]=*data;
    return 1;
}
/* add ephemeris data --------------------------------------------------------*/
static int add_eph(nav_t *nav, const eph_t *eph)
{
    eph_t *nav_eph;

    if (nav->nmax<=nav->n) {
        nav->nmax+=1024;
        if (!(nav_eph=(eph_t *)realloc(nav->eph,sizeof(eph_t)*nav->nmax))) {
            trace(NULL,1,"add_eph: memalloc error n=%d\n",nav->nmax);
            free(nav->eph); nav->eph=NULL; nav->n=nav->nmax=0;
            return 0;
        }
        nav->eph=nav_eph;
    }
    nav->eph[nav->n++]=*eph;
    return 1;
}
/* add GLONASS ephemeris data ------------------------------------------------*/
static int add_geph(nav_t *nav, const geph_t *geph)
{
    geph_t *nav_geph;

    if (nav->ngmax<=nav->ng) {
        nav->ngmax+=1024;
        if (!(nav_geph=(geph_t *)realloc(nav->geph,sizeof(geph_t)*nav->ngmax))) {
            trace(NULL,1,"add_geph: memalloc error n=%d\n",nav->ngmax);
            free(nav->geph); nav->geph=NULL; nav->ng=nav->ngmax=0;
            return 0;
        }
        nav->geph=nav_geph;
    }
    nav->geph[nav->ng++]=*geph;
    return 1;
}
/* add SBAS ephemeris data ---------------------------------------------------*/
static int add_seph(nav_t *nav, const seph_t *seph)
{
    seph_t *nav_seph;

    if (nav->nsmax<=nav->ns) {
        nav->nsmax+=1024;
        if (!(nav_seph=(seph_t *)realloc(nav->seph,sizeof(seph_t)*nav->nsmax))) {
            trace(NULL,1,"add_seph: memalloc error n=%d\n",nav->nsmax);
            free(nav->seph); nav->seph=NULL; nav->ns=nav->nsmax=0;
            return 0;
        }
        nav->seph=nav_seph;
    }
    nav->seph[nav->ns++]=*seph;
    return 1;
}
/* read BINEX file pointer ---------------------------------------------------*/
static int readbnxfp(FILE *fp, gtime_t ts, gtime_t te, double tint,
                     const char *opt, int flag, int index, char *type,
                     obs_t *obs, nav_t *nav, sta_t *sta, const prcopt_t *popt)
{
    unsigned char slips[MAXSAT][NFREQ]={{0}};
    int stat,data,dec_type,sys,sat,prn,mask,i;
    uint32_t len;
    int len_h,len_c;
    static raw_t raw;

    memset(&raw,'\0',sizeof(raw));
    init_raw(&raw,STRFMT_BINEX);
    strcpy(raw.opt,opt);
    mask=popt->navsys;

    while (1) {
        if ((data = fgetc(fp)) == EOF) {
            break;
        }
        if (!sync_bnx(raw.buff, (unsigned char)data)) {
            continue;
        }

        if (fread(raw.buff + 2, 1, 4, fp) < 4) {
            break;
        }

        len_h=getbnxi(raw.buff+2,&len);
        raw.len=len+len_h+2;

        if (raw.len-1>4096) {
            trace(NULL,2,"binex length error: len=%d\n",raw.len-1);
            raw.nbyte=0;
            continue;
        }
        len_c=(raw.len-1<128)?1:2;

        if (raw.len+len_c-6<1) {
            trace(NULL,2,"binex length error: len=%d\n",raw.len-1);
            raw.nbyte=0;
            continue;
        }
        if (fread(raw.buff+6,1,raw.len+len_c-6,fp)<(size_t)(raw.len+len_c-6)) {
            break;
        }
        raw.nbyte=0;

        if ((dec_type=decode_bnx(&raw))>=1) {
            switch (dec_type) {
                case 1: /* observation data */
                    for (i=0;i<raw.obs.n;i++) {
                        saveslips(slips,raw.obs.data+i);
                    }
                    if (raw.obs.n > 0 && !screent(raw.obs.data[0].time, ts, te, tint)) {
                        continue;
                    }

                    for (i=0;i<raw.obs.n;i++) {
                        restslips(slips,raw.obs.data+i);
                        raw.obs.data[i].rcv=(unsigned char)index;

                        if (!(satsys(raw.obs.data[i].sat, NULL) & mask)) {
                            continue;
                        }

                        if ((stat = addobsdata(obs, raw.obs.data + i)) < 0) {
                            break;
                        }
                    }
                    break;
                case 2: /* ephemeris data */
                    sat=raw.ephsat;
                    sys=satsys(sat,&prn);

                    if (!(sys & mask)) {
                        break;
                    }

                    switch (sys) {
                        case SYS_GLO:
                            stat=add_geph(nav,&raw.nav.geph[prn-1]);
                            break;
                        case SYS_SBS:
                            stat=add_seph(nav,&raw.nav.seph[prn-MINPRNSBS]);
                            break;
                        default:
                            stat=add_eph(nav,&raw.nav.eph[sat-1]);
                            break;
                    }
                    if (!stat) { free_raw(&raw); return 0; }
                    break;
            }
        }
    }
    free_raw(&raw);
    return 1;
}
/* read BINEX file -----------------------------------------------------------*/
static int readbnxfile(const char *file, gtime_t ts, gtime_t te, double tint,
                       const char *opt, int flag, int index, char *type,
                       obs_t *obs, nav_t *nav, sta_t *sta, const prcopt_t *popt)
{
    FILE *fp;
    int cstat,stat;
    char tmpfile[1024];

    trace(NULL,3,"readbnxfile: file=%s flag=%d index=%d\n",file,flag,index);

    if (sta) {
        *sta->name='\0'; *sta->marker='\0'; *sta->antdes='\0';
        *sta->antsno='\0'; *sta->rectype='\0'; *sta->recver='\0';
        *sta->recsno='\0';
        sta->antsetup=sta->itrf=sta->deltype=0;
        sta->hgt=0.0;
    }
    /* uncompress file */
    if ((cstat=rtk_uncompress(file,tmpfile))<0) {
        trace(NULL,2,"binex file uncompact error: %s\n",file);
        return 0;
    }
    if (!(fp=fopen(cstat?tmpfile:file,"rb"))) {
        trace(NULL,2,"binex file open error: %s\n",cstat?tmpfile:file);
        return 0;
    }
    stat=readbnxfp(fp,ts,te,tint,opt,flag,index,type,obs,nav,sta,popt);

    fclose(fp);
    if (cstat) {
        remove(tmpfile);
    }

    return stat;
}
/* read BINEX observation/navigation data --------------------------------------
* read BINEX obs/nav data from file
* args   : char   *file     I   BINEX file name
*          int     rcv      I   receiver number (1:rover, 2:base)
*          gtime_t ts       I   start time (ts.time==0: from beginning)
*          gtime_t te       I   end time   (te.time==0: to end)
*          double  tint     I   observation interval (0: all)
*          char   *opt      I   decode options
*          obs_t  *obs      IO  observation data
*          nav_t  *nav      IO  navigation data
*          sta_t  *sta      IO  station info (NULL: not output)
*          prcopt_t *popt   I   processing options (for navsys mask)
* return : status (1:ok, 0:no data, -1:error)
*-----------------------------------------------------------------------------*/
extern int readbnxt(const char *file, int rcv, gtime_t ts, gtime_t te,
                    double tint, const char *opt, obs_t *obs, nav_t *nav,
                    sta_t *sta, const prcopt_t *popt)
{
    int i,n,stat=0;
    const char *p;
    char type=' ',*files[MAXEXFILE]={0};

    trace(NULL,3,"readbnxt: file=%s rcv=%d\n",file,rcv);

    if (!*file) {
        return readbnxfp(stdin,ts,te,tint,opt,0,1,&type,obs,nav,sta,popt);
    }
    for (i=0;i<MAXEXFILE;i++) {
        if (!(files[i]=(char *)malloc(1024))) {
            for (i--; i >= 0; i--) {
                free(files[i]);
            }
            return -1;
        }
    }
    /* expand wild-card */
    if ((n=expath(file,files,MAXEXFILE))<=0) {
        for (i = 0; i < MAXEXFILE; i++) {
            free(files[i]);
        }
        return 0;
    }
    for (i=0;i<n&&stat>=0;i++) {
        stat=readbnxfile(files[i],ts,te,tint,opt,0,rcv,&type,obs,nav,sta,popt);
    }
    /* if station name empty, set 4-char name from file head */
    if (sta) {
        if (!(p = strrchr(file, FILEPATHSEP))) {
            p = file - 1;
        }
        if (!*sta->name) {
            setstr_bnx(sta->name, p + 1, 4);
        }
    }
    for (i = 0; i < MAXEXFILE; i++) {
        free(files[i]);
    }

    return stat;
}
