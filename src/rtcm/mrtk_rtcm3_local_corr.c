/*------------------------------------------------------------------------------
 * mrtk_rtcm3_local_corr.c : RTCM3 local correction message encoder/decoder
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
 * @file mrtk_rtcm3_local_corr.c
 * @brief MRTKLIB RTCM Module — RTCM3 local correction message functions.
 */
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_sys.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_atmos.h"

#include <string.h>
#include <math.h>
#include "mrtklib/mrtk_trace.h"

/*--- local constants (duplicated to avoid rtklib.h dependency) -------------*/
#define SYS_GPS     0x01
#define SYS_GLO     0x04
#define SYS_GAL     0x08
#define SYS_QZS     0x10
#define SYS_CMP     0x20

#define TSYS_GAL    3

static const double PI       = 3.1415926535897932;
static const double D2R      = 3.1415926535897932 / 180.0;
static const double R2D      = 180.0 / 3.1415926535897932;
static const double RE_WGS84 = 6378137.0;

#define TOW_LSB         30.0            /* epoch time 30s */
#define BTYPE_GRID      0               /* block type grid */
#define BTYPE_STA       1               /* block type station */
#define ION_BLKSIZE     2.0             /* ionosphere block size */
#define TRP_BLKSIZE     4.0             /* troposphere block size */

/*--- forward declarations for legacy functions resolved at link time -------*/

#define GN(gp)      (gp==1?64:16)

/* initialize station setting --------------------------------------------------
* initialize station settings based on provided block information
* args   : blkinf_t *b   IO   block information
*-----------------------------------------------------------------------------*/
void initblkinf(blkinf_t *b)
{
    int i,br,gr;
    double gridsize;

    gr=(int)sqrt(GN(b->gpitch));
    br=(int)360.0/b->bs;
    b->bpos[0]=90.0-(int)(b->bn/br)*b->bs;
    b->bpos[1]=(b->bn%br)*b->bs;
    if (b->bpos[1] >= 180) {
        b->bpos[1] -= 360;
    }
    b->bpos[0]=b->bpos[0]*D2R;
    b->bpos[1]=b->bpos[1]*D2R;

    if(b->btype==BTYPE_GRID){
        gridsize = (double)b->bs / (double)gr;
        /* init grid position */
        for (i=0;i<gr*gr;i++) {
            if (i < 32) {
                if (!((b->mask[0] >> i) & 1)) {
                    continue;
                }
            } else {
                if (!((b->mask[1] >> (i - 32)) & 1)) {
                    continue;
                }
            }
            b->grid[b->n][0]=b->bpos[0]-((i/gr)*gridsize)*D2R;
            b->grid[b->n][1]=b->bpos[1]+((i%gr)*gridsize)*D2R;
            b->grid[b->n][2]=0;
            b->gp[b->n]=i;
            b->n++;
        }
    }
}

#define ION_BAS_LSB 0.35      /* ionospheric delay base value resolution */
#define ION_DET_LSB 0.002     /* ionospheric delay detail value resolution */
#define ION_STD_LSB 0.02      /* ionospheric delay std index resolution */
#define ION_STD_MAX 2.56      /* ionospheric delay std index=7 */
#define LAT_LSB     0.001     /* station pos latitude resolution */
#define LON_LSB     0.001     /* station pos longitude resolution */
#define TRP_BAS_LSB 0.1       /* tropspheric delay base value resolution */
#define TRP_DET_LSB 0.002     /* tropspheric delay detail value resolution */
#define TRP_STD_LSB 0.002     /* tropspheric delay std index resolution */
#define TRP_STD_MAX 0.256     /* tropspheric delay std index=7 */

#define ROUND(x)    (int)floor((x)+0.5)

/* decode local correction grid rtcm message header --------------------------*/
static int decode_lclhead(rtcm_t *rtcm, int type,blkinf_t *blkinf)
{
    int p=24,wn;
    double tow;
    gtime_t ntime;
    static gtime_t ptime={0};

    /* message no */
    /* 'type' is already partially decoded, thus skipped */
                                        p+=12;

    /* gps epoch time */
    tow=getbitu(rtcm->buff,p,15);       p+=15;
    tow=tow*TOW_LSB;

    /* tow -> gpstime with cpu time */
    time2gst(rtcm->time,&wn);
    ntime=gst2time(wn,tow);
    if (ptime.time!=0) {
        if (timediff(ntime,ptime)>(double)86400*6) {
            wn-=1;
            ntime=gst2time(wn,tow);
        }
    }
    rtcm->time=ntime;
    ptime=ntime;

    if (type==2001 || type==2011) {
        blkinf->bn=getbitu(rtcm->buff,p,12); p+=12;
        blkinf->bs=TRP_BLKSIZE;
    }
    else if((2002<=type && type<=2006) || (2012<=type && type<=2016)) {
        blkinf->bn=getbitu(rtcm->buff,p,14); p+=14;
        blkinf->bs=ION_BLKSIZE;
    }

    /* grid pitch and mask */
    if (2001<=type && type<=2006) {
        /* grid pitch */
        blkinf->gpitch=getbitu(rtcm->buff,p,1);       p+=1;
        /* grid mask */
        if (blkinf->gpitch) {
            blkinf->mask[0]=getbitu(rtcm->buff,p,32); p+=32;
            blkinf->mask[1]=getbitu(rtcm->buff,p,32); p+=32;
        }
        else {
            blkinf->mask[0]=getbitu(rtcm->buff,p,16); p+=16;
        }
        blkinf->btype=BTYPE_GRID;
        initblkinf(blkinf);
    }

    /* number of station */
    if (type==2011) {
        blkinf->btype=BTYPE_STA;
        initblkinf(blkinf);
        blkinf->n = getbitu(rtcm->buff, p, 9);  p+=9;
    } 
    else if(2012<=type && type<=2016){
        blkinf->btype=BTYPE_STA;
        initblkinf(blkinf);
        blkinf->n = getbitu(rtcm->buff, p, 8);  p+=8;
    }

    return p;
}
/* decode local tropospheric delay correction grid RTCM message ----------------
* decode local tropospheric correction grid from RTCM message
* args   : rtcm_t *rtcm     IO   RTCM control struct
*          int type         I    message type
* return : status (12: success, other: error)
*-----------------------------------------------------------------------------*/
int decode_lcltrop(rtcm_t *rtcm, int type)
{
    int i,p=0,ctnum,tp=0;
    unsigned int lat,lon;
    double base,trp,ind,ecef[3];
    double zhd,zazel[]={0.0,PI/2.0};
    blkinf_t *blkinf,tmpblkinf={0};
    sitetrp_t *strp;

    p=decode_lclhead(rtcm,type,&tmpblkinf);
    trace(NULL,3,"decode_lcltrop : %s,bn=%4d,btype=%d,gp=%d,gnum=%d\n",
        time_str(rtcm->time,0), tmpblkinf.bn, tmpblkinf.btype, tmpblkinf.gpitch,
        tmpblkinf.n);
    for(i=0;i<rtcm->lclblk.tnum;i++){
        if (rtcm->lclblk.tblkinf[i].bn == tmpblkinf.bn) {
            break;
        }
    }
    ctnum=i;
    if(i==rtcm->lclblk.tnum){
        rtcm->lclblk.tnum++;
    }
    memcpy(&rtcm->lclblk.tblkinf[i],&tmpblkinf,sizeof(blkinf_t));
    blkinf=&rtcm->lclblk.tblkinf[i];

    base=getbitu(rtcm->buff,p,6)*TRP_BAS_LSB; p+=6;
    for (i=0;i<blkinf->n;i++) {
        /* latitude and longitude */
        if(type==2011) {
            lat=getbitu(rtcm->buff,p,12);     p+=12;
            lon=getbitu(rtcm->buff,p,12);     p+=12;
            blkinf->grid[i][0]=blkinf->bpos[0]-lat*LAT_LSB*D2R;
            blkinf->grid[i][1]=blkinf->bpos[1]+lon*LON_LSB*D2R;
        }
        if(type==2001) {
            tp=blkinf->gp[i];
        }
        else if(type==2011) {
            tp=i;
        }

        strp=&rtcm->lclblk.tstat[ctnum][tp];
        trp=getbitu(rtcm->buff,p,9)*TRP_DET_LSB; p+=9;
        ind=getbitu(rtcm->buff,p,3);             p+=3;
        if (ind==7){
            continue;
        }

        /* zenith hydrostatic delay */
        zhd=tropmodel(rtcm->time,blkinf->grid[i],zazel,0.0);
        strp->trpd.time=rtcm->time;
        strp->trpd.trp[0]=trp+base+zhd;
        strp->trpd.std[0]=TRP_STD_LSB*pow(2,ind);
        pos2ecef(blkinf->grid[i],ecef);
        memcpy(strp->site.ecef,ecef,sizeof(strp->site.ecef));
        trace(NULL,4,"decode_lcltrop : %s,bn=%4d,lat=%.3f,lon=%.3f,base=%lf,detail=%lf,std=%lf\n",
            time_str(rtcm->time,0), blkinf->bn, 
            blkinf->grid[i][0]*R2D, blkinf->grid[i][1]*R2D,
            base,trp,strp->trpd.std[0]);
    }
           
    return 12;
}

/* decode local ionospheric delay correction grid RTCM message ----------------
* decode local ionospheric correction grid from RTCM message
* args   : rtcm_t *rtcm     IO   RTCM control struct
*          int type         I    message type
* return : status (12: success, other: error)
*-----------------------------------------------------------------------------*/
int decode_lcliono(rtcm_t *rtcm, int type)
{
    int i, j, p=0, ns, nsat, sat, msys, minprn, ssat, cinum, tp=0;
    int prn[MAXSAT]={0};
    unsigned int smask[2]={0},nbase,lat,lon;
    double base,ion,ind,ecef[3];
    blkinf_t *blkinf,tmpblkinf={0};
    ion_t *ionp;

    switch (type) {
        case 2002: msys=SYS_GPS; minprn=MINPRNGPS; nsat=NSATGPS; break;
        case 2003: msys=SYS_GLO; minprn=MINPRNGLO; nsat=NSATGLO; break;
        case 2004: msys=SYS_GAL; minprn=MINPRNGAL; nsat=NSATGAL; break;
        case 2005: msys=SYS_QZS; minprn=MINPRNQZS; nsat=NSATQZS; break;
        case 2006: msys=SYS_CMP; minprn=MINPRNCMP; nsat=NSATCMP; break;
        case 2012: msys=SYS_GPS; minprn=MINPRNGPS; nsat=NSATGPS; break;
        case 2013: msys=SYS_GLO; minprn=MINPRNGLO; nsat=NSATGLO; break;
        case 2014: msys=SYS_GAL; minprn=MINPRNGAL; nsat=NSATGAL; break;
        case 2015: msys=SYS_QZS; minprn=MINPRNQZS; nsat=NSATQZS; break;
        case 2016: msys=SYS_CMP; minprn=MINPRNCMP; nsat=NSATCMP; break;
        default :  return 0;
    }

    p=decode_lclhead(rtcm,type,&tmpblkinf);
    trace(NULL,3,"decode_lcliono : %s,bn=%4d,btype=%d,gp=%d\n",
        time_str(rtcm->time,0), tmpblkinf.bn, tmpblkinf.btype, tmpblkinf.gpitch);
    for(i=0;i<rtcm->lclblk.inum;i++){
        if (rtcm->lclblk.iblkinf[i].bn == tmpblkinf.bn) {
            break;
        }
    }
    if(i>=rtcm->lclblk.inum){
        rtcm->lclblk.inum++;
    }
    cinum=i;
    memcpy(&rtcm->lclblk.iblkinf[cinum],&tmpblkinf,sizeof(blkinf_t));
    blkinf=&rtcm->lclblk.iblkinf[cinum];

    if (nsat>32) {
        smask[0]=getbitu(rtcm->buff,p,32);      p+=32;
        smask[1]=getbitu(rtcm->buff,p,nsat-32); p+=nsat-32;
    }
    else {
        smask[0]=getbitu(rtcm->buff,p,nsat);    p+=nsat;
    }
    for (j=0,ns=0;j<nsat;j++) {
        if (j < 32) {
            if (((smask[0] >> j) & 1)) {
                prn[ns++] = j + minprn;
            }
        } else {
            if (((smask[1] >> (j - 32)) & 1)) {
                prn[ns++] = j + minprn;
            }
        }
    }

    /* latitude and longitude */
    if (2012<=type && type<=2016) {
        for (i=0;i<blkinf->n;i++) {
            lat=getbitu(rtcm->buff,p,11);     p+=11;
            lon=getbitu(rtcm->buff,p,11);     p+=11;
            blkinf->grid[i][0]=blkinf->bpos[0]-lat*LAT_LSB*D2R;
            blkinf->grid[i][1]=blkinf->bpos[1]+lon*LON_LSB*D2R;
        }
    }

    /* clear old data */
    ssat=satno(msys, minprn);
    for (i=0;i<blkinf->n;i++) {
        if (2002<=type && type<=2006){
            tp=blkinf->gp[i];
        }
        else if (2012<=type && type<=2016) {
            tp=i;
        }
        for (j=ssat-1;j<ssat+nsat-1;j++) {
            ionp=&rtcm->lclblk.istat[cinum][tp].iond[j];
            ionp->ion=0.0;
            ionp->std=0.0;
        }
        pos2ecef(blkinf->grid[i],ecef);
        memcpy(rtcm->lclblk.istat[cinum][tp].site.ecef,ecef,
            sizeof(rtcm->lclblk.istat[cinum][tp].site.ecef));
    }

    for (i=0;i<ns;i++) {
        nbase=getbitu(rtcm->buff,p,8); p+=8;
        base=nbase*ION_BAS_LSB;
        if (!(sat = satno(msys, prn[i]))) {
            continue;
        }
        for (j=0;j<blkinf->n;j++) {
            if (2002<=type && type<=2006){
                tp=blkinf->gp[j];
            }
            else if (2012<=type && type<=2016) {
                tp=j;
            }
            ionp=&rtcm->lclblk.istat[cinum][tp].iond[sat-1];
            ion=getbitu(rtcm->buff,p,9)*ION_DET_LSB; p+=9;
            ind=getbitu(rtcm->buff,p,3);             p+=3;
            /*if (ind==7) continue;*/
            
            ionp->time=rtcm->time;
            ionp->ion=ion+base;
            ionp->std=ION_STD_LSB*pow(2,ind);
            trace(NULL,4,"decode_lcliono : %s,sys=%d,prn=%d,bn=%5d,gn=%d,lat=%.3f,lon=%.3f,base=%lf,detail=%lf,std=%lf\n",
                time_str(rtcm->time,0),msys,prn[j],blkinf->bn,tp,
                blkinf->grid[j][0]*R2D, blkinf->grid[j][1]*R2D,
                base, ion, ionp->std);
        }
    }
    return 12;
}
/* encode local correction grid rtcm message header --------------------------*/
static int encode_lclhead(rtcm_t *rtcm, int type,blkinf_t *blkinf)
{
    int p=24,week,epoch;
    double tow;

    /* message no */
    setbitu(rtcm->buff,p,12,type);      p+=12; 

    /* gps epoch time */
    tow=time2gpst(rtcm->time,&week)/TOW_LSB;
    epoch=ROUND(tow);
    setbitu(rtcm->buff,p,15,epoch);     p+=15;

    /* block number */
    if (type==2001 || type==2011) {
        setbitu(rtcm->buff,p,12,blkinf->bn); p+=12;
    }
    else if((2002<=type && type<=2006) || (2012<=type && type<=2016)) {
        setbitu(rtcm->buff,p,14,blkinf->bn); p+=14;
    }

    /* grid pitch and mask */
    if(2001<=type && type<=2006){
        /* grid pitch */
        setbitu(rtcm->buff,p, 1,blkinf->gpitch); p+=1;
        /* grid mask */
        if(blkinf->gpitch) {
            setbitu(rtcm->buff,p,32,blkinf->mask[0]); p+=32;
            setbitu(rtcm->buff,p,32,blkinf->mask[1]); p+=32;
        }
        else      {
            setbitu(rtcm->buff,p,16,blkinf->mask[0]); p+=16;
        }
    }

    /* number of station */
    if (type==2011) {
        setbitu(rtcm->buff,p, 9,blkinf->n);  p+=9;
    } 
    else if(2012<=type && type<=2016){
        setbitu(rtcm->buff,p, 8,blkinf->n);  p+=8;
    }
    return p;
}
/* encode local tropospheric delay correction grid RTCM message ----------------
* encode local tropospheric correction grid to RTCM message
* args   : rtcm_t *rtcm     IO   RTCM control struct
*          int type         I    message type
* return : status (1: success, other: error)
*-----------------------------------------------------------------------------*/
int encode_lcltrop(rtcm_t *rtcm, int type)
{
    int i,p=0,n=0,tp=0;
    unsigned int base,detail,stdev,lat,lon;
    double zhd,zazel[]={0.0,PI/2.0},zwd[MAXTRPSTA]={0.0};
    trp_t *trpp;
    blkinf_t *blkinf;

    blkinf=&rtcm->lclblk.tblkinf[rtcm->lclblk.outtn];
    /* base value */
    for (i=0,base=-1;i<blkinf->n;i++) {
        if(type==2001) {
            tp=blkinf->gp[i];
        }
        else if(type==2011) {
            tp=i;
        }
        trpp=&rtcm->lclblk.tstat[rtcm->lclblk.outtn][tp].trpd;
        if (timediff(rtcm->time, trpp->time) > 0 || trpp->time.time == 0) {
            continue;
        }
        rtcm->time=trpp->time;
        /* zenith wet delay */
        zhd=tropmodel(trpp->time,blkinf->grid[tp],zazel,0.0);
        zwd[i]=trpp->trp[0]-zhd;
        if ((base==-1)||(base>(unsigned int)(zwd[i]/TRP_BAS_LSB))) {
            base=(unsigned int)(zwd[i]/TRP_BAS_LSB);
        }
        n++;
    }
    if (!n) {
        return 0;
    }

    /* encode trop message */
    p=encode_lclhead(rtcm,type,blkinf);
    trace(NULL,3,"encode_lcltrop : %s,bn=%4d,btype=%d,gp=%d,gnum=%d\n",
        time_str(rtcm->time,0), blkinf->bn, blkinf->btype, blkinf->gpitch, 
        blkinf->n);
    setbitu(rtcm->buff,p, 6,base); p+=6;       /* Base Value LSB:0.1m */
    for (i=0;i<blkinf->n;i++) {
        if(type==2001) {
            tp=blkinf->gp[i];
        }
        else if(type==2011) {
            tp=i;
        }
        trpp=&rtcm->lclblk.tstat[rtcm->lclblk.outtn][tp].trpd;
        if (trpp->std[0] == 0) {
            continue;
        }

        /* latitude and longitude */
        if(type==2011) {
            lat=(unsigned int)((blkinf->bpos[0]-blkinf->grid[i][0])*R2D/LAT_LSB);
            lon=(unsigned int)((blkinf->grid[i][1]-blkinf->bpos[1])*R2D/LON_LSB);
            setbitu(rtcm->buff,p,12,lat);    p+=12;
            setbitu(rtcm->buff,p,12,lon);    p+=12;
        }

        /* zenith wet delay */
        detail=(unsigned int)((zwd[i]-base*TRP_BAS_LSB)/TRP_DET_LSB);
        stdev =(unsigned int)(log(trpp->std[0]/TRP_STD_LSB)/log(2));
        if(zwd[i]<base*TRP_BAS_LSB){
            detail=0;
            stdev=7;
            trace(NULL,4,"encode_lcltrop zwd<base: %s,bn=%4d,gn=%d,lat=%.3f,lon=%.3f,base=%lf,zwd=%lf\n",
                time_str(rtcm->time,0),blkinf->bn,tp,
                blkinf->grid[tp][0]*R2D, blkinf->grid[tp][1]*R2D,
                base*ION_BAS_LSB,zwd[i]);
        }
        else if (TRP_STD_MAX <= trpp->std[0]) {
            stdev=7;
            trace(NULL,4,"encode_lcltrop TRP_STD_MAX<std: %s,bn=%4d,gn=%d,lat=%.3f,lon=%.3f,base=%lf,zwd=%lf\n",
                time_str(rtcm->time,0),blkinf->bn,tp,
                blkinf->grid[tp][0]*R2D, blkinf->grid[tp][1]*R2D,
                base*ION_BAS_LSB,zwd[i]);
        }

        setbitu(rtcm->buff,p, 9,detail); p+=9; /* Detail Value */
        setbitu(rtcm->buff,p, 3,stdev);  p+=3; /* STD Index */

        trace(NULL,4,"encode_lcltrop : %s,bn=%4d,gn=%d,lat=%.3f,lon=%.3f,base=%.3f,detail=%.3f,std=%.3f\n",
            time_str(rtcm->time,0), blkinf->bn,tp,
            blkinf->grid[tp][0]*R2D, blkinf->grid[tp][1]*R2D,
            base*TRP_BAS_LSB,detail*TRP_DET_LSB, pow(2.0,stdev)*TRP_STD_LSB);
    }
    rtcm->nbit=p;

    return 1;
}
/* encode local ionospheric delay correction grid RTCM message -----------------
* encode local ionospheric correction grid to RTCM message
* args   : rtcm_t *rtcm     IO   RTCM control struct
*          int type         I    message type
* return : status (1: success, other: error)
*-----------------------------------------------------------------------------*/
int encode_lcliono(rtcm_t *rtcm, int type)
{
    int i,j,p=0,n=0,nsat,msys,minprn,sys,prn,ssat,tp=0;
    double ion[MAXIONSTA][MAXOBS]={{0}},std[MAXIONSTA][MAXOBS]={{0}};
    unsigned int base[MAXOBS]={-1},satmask[2],detail,stdev,lat,lon;
    ion_t *ionp;
    blkinf_t *blkinf;

    switch (type) {
        case 2002: msys=SYS_GPS; minprn=MINPRNGPS; nsat=NSATGPS; break;
        case 2003: msys=SYS_GLO; minprn=MINPRNGLO; nsat=NSATGLO; break;
        case 2004: msys=SYS_GAL; minprn=MINPRNGAL; nsat=NSATGAL; break;
        case 2005: msys=SYS_QZS; minprn=MINPRNQZS; nsat=NSATQZS; break;
        case 2006: msys=SYS_CMP; minprn=MINPRNCMP; nsat=NSATCMP; break;
        case 2012: msys=SYS_GPS; minprn=MINPRNGPS; nsat=NSATGPS; break;
        case 2013: msys=SYS_GLO; minprn=MINPRNGLO; nsat=NSATGLO; break;
        case 2014: msys=SYS_GAL; minprn=MINPRNGAL; nsat=NSATGAL; break;
        case 2015: msys=SYS_QZS; minprn=MINPRNQZS; nsat=NSATQZS; break;
        case 2016: msys=SYS_CMP; minprn=MINPRNCMP; nsat=NSATCMP; break;
        default: return 0;
    }

    ssat=satno(msys, minprn);
    blkinf=&rtcm->lclblk.iblkinf[rtcm->lclblk.outin];
    /* search base value */
    for (i = 0; i < nsat; i++) {
        base[i] = -1;
    }
    for (i=0,satmask[0]=0,satmask[1]=0; i<blkinf->n; i++) {
        if (2002<=type && type<=2006){
            tp=blkinf->gp[i];
        }
        else if (2012<=type && type<=2016) {
            tp=i;
        }
        for (j=ssat-1;j<ssat+nsat-1;j++) {
            ionp=&rtcm->lclblk.istat[rtcm->lclblk.outin][tp].iond[j];
            sys=satsys(j+1,&prn);
            prn-=minprn;
            if (ionp->ion<=0.0){
                continue;
            }
            rtcm->time=ionp->time;
            ion[i][prn]=ionp->ion;
            std[i][prn]=ionp->std;
            /* use the smallest VTEC value in the entire area as the base */
            if ((base[prn]==-1)||(base[prn]>(unsigned int)(ionp->ion/ION_BAS_LSB))) {
                base[prn]=(unsigned int)(ionp->ion/ION_BAS_LSB);
            }
            if (prn < 32) {
                satmask[0] |= (1 << (prn));
            } else {
                satmask[1] |= (1 << (prn - 32));
            }
            n++;
        }
    }
    if (!n) {
        return 0;
    }

    /* encode iono message */
    p=encode_lclhead(rtcm,type,blkinf);
    trace(NULL,3,"encode_lcliono : %s,bn=%4d,btype=%d,gp=%d,gnum=%d\n",
        time_str(rtcm->time,0), blkinf->bn, blkinf->btype, blkinf->gpitch,
        blkinf->n);
    if (nsat>32) {
        setbitu(rtcm->buff,p, 32,      satmask[0]); p+=32;
        setbitu(rtcm->buff,p, nsat-32, satmask[1]); p+=nsat-32;
    }
    else {
        setbitu(rtcm->buff,p, nsat, satmask[0]); p+=nsat;
    }

    /* latitude and longitude */
    if (2012<=type && type<=2016) {
        for (i=0;i<blkinf->n;i++) {
            lat=(unsigned int)((blkinf->bpos[0]-blkinf->grid[i][0])*R2D/LAT_LSB);
            lon=(unsigned int)((blkinf->grid[i][1]-blkinf->bpos[1])*R2D/LON_LSB);
            setbitu(rtcm->buff,p,11,lat);    p+=11;
            setbitu(rtcm->buff,p,11,lon);    p+=11;
        }
    }
    for (i=0;i<nsat;i++) {
        if (i < 32) {
            if (((satmask[0] >> i) & 1) == 0) {
                continue;
            }
        } else {
            if (((satmask[1] >> (i - 32)) & 1) == 0) {
                continue;
            }
        }

        setbitu(rtcm->buff,p, 8,base[i]); p+=8;    /* base value */
        for (j=0;j<blkinf->n;j++) {
            detail=(unsigned int)((ion[j][i]-base[i]*ION_BAS_LSB)/ION_DET_LSB);
            stdev =(unsigned int)(log(std[j][i]/ION_STD_LSB)/log(2));
            if(ion[j][i]<base[i]*ION_BAS_LSB) {
                detail=0;
                stdev=7;
                trace(NULL,4,"encode_lcliono ion<base: %s,sys=%d,prn=%d,bn=%4d,gn=%d,base=%lf,ion=%lf\n",
                    time_str(rtcm->time,0),msys,i+1,blkinf->bn,blkinf->gp[j],
                    base[i]*ION_BAS_LSB,ion[j][i]);
            }
            else if (ION_STD_MAX <= std[j][i]) {
                stdev=7;
                trace(NULL,4,"encode_lcliono ION_STD_MAX<std: %s,sys=%d,prn=%d,bn=%4d,gn=%d,base=%lf,ion=%lf\n",
                    time_str(rtcm->time,0),msys,i+1,blkinf->bn,blkinf->gp[j],
                    base[i]*ION_BAS_LSB,ion[j][i]);
            }

            setbitu(rtcm->buff,p, 9,detail); p+=9; /* detail value */
            setbitu(rtcm->buff,p, 3,stdev);  p+=3; /* std index */
            trace(NULL,4,"encode_lcliono : %s,sys=%d,prn=%d,bn=%4d,gn=%d,base=%lf,detail=%lf,std=%lf\n",
                time_str(rtcm->time,0),msys,i+1,blkinf->bn,blkinf->gp[j],
                base[i]*ION_BAS_LSB,detail*ION_DET_LSB, pow(2.0,stdev)*ION_STD_LSB);
        }
    }
    rtcm->nbit=p;
    return 1;
}
