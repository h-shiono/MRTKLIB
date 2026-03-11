/*------------------------------------------------------------------------------
 * mrtk_madoca_local_corr.c : MADOCA local correction common functions
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
#include "mrtklib/mrtk_madoca_local_corr.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_atmos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mrtklib/mrtk_trace.h"

/* local constants -----------------------------------------------------------*/
#define PI          3.1415926535897932
#define D2R         (PI/180.0)
#define R2D         (180.0/PI)

/* local macros --------------------------------------------------------------*/
#define PRN_ZTD        1E-4      /* process noise of ztd (m/sqrt(s)) */
#define PRN_GRAD       1E-5      /* process noise of gradient (m/sqrt(s)) */
#define STD_ZTD_DIST   1E-4      /* distance dependent factor ztd (m/km) */
#define STD_GRAD_DIST  1E-5      /* distance dependent factor gradient (m/km) */
#define STD_VTEC_DIST  1E-3      /* distance dependent factor vtec (m/km) */

#define SQR(x)      ((x)*(x))
#define MAX(x,y)    ((x)>(y)?(x):(y))
#define GN(gp)      (gp==1?64:16)

/* forward declarations (implemented in rtkcmn.c, resolved at link time) -----*/
extern int satid2no(const char *id);
extern char *time_str(gtime_t t, int n);

/* initblkinf moved to mrtk_rtcm3_local_corr.c (mrtklib) */
/* get point no ----------------------------------------------------------------
* search for a point by name or add a new point if not found
* args   : stat_t *stat        IO   local correction data
*          const char* pntname I    point name
* return : point index (>=0: found or added, 0: error)
*-----------------------------------------------------------------------------*/
extern int getstano(stat_t *stat, const char* pntname)
{
    int i;

    /* search point */
    for (i=0;i<stat->nst;i++){
        if (!strcmp(stat->strp[i].site.name, pntname)) {
            break;
        }
    }
    if (i >= MAXSITES) {
        return 0;
    }
    /* new point */
    if (i>=stat->nst) {
        memset(&stat->strp[stat->nst],0x00,sizeof(sitetrp_t));
        memset(&stat->sion[stat->nsi],0x00,sizeof(siteion_t));
        strcpy(stat->strp[stat->nst].site.name,pntname);
        strcpy(stat->sion[stat->nsi].site.name,pntname);
        stat->nst++;
        stat->nsi++;
    }
    return i;
}
/* decode stat message -------------------------------------------------------*/
static int decode_stat(sstat_t *sstat, char *buff, int *nbyte)
{
    double tow=0,ecef[3]={0},azel[2]={0},ion=0,ztd={0},grad[2]={0},std[2]={0};
    char sat[64]="",sys;
    int prn,week=0,qflag=0,rcv=0,satno=0;
    gtime_t time;

    buff[*nbyte]='\0';
    *nbyte=0;
    /* point position */
    if (sscanf(buff,"$POS,%d,%lf,%d,%lf,%lf,%lf",&week,&tow,&qflag,ecef,ecef+1,
               ecef+2)>=6) {
        sstat->time=gpst2time(week,tow);
        memcpy(sstat->sion.site.ecef,ecef,sizeof(ecef));
        memcpy(sstat->strp.site.ecef,ecef,sizeof(ecef));
    }
    /* ionosphere */
    else if (sscanf(buff,"$ION,%d,%lf,%d,%c%d,%lf,%lf,%lf,%lf",&week,&tow,
                    &qflag,&sys,&prn,azel,azel+1,&ion,std)>=9) {
            sprintf(sat,"%c%02d",sys,prn);
        satno=(unsigned char)satid2no(sat);
        sstat->time=gpst2time(week,tow);
        sstat->sion.iond[satno-1].time=sstat->time;
        sstat->sion.iond[satno-1].ion=ion;
        sstat->sion.iond[satno-1].std=std[0];
        sstat->sion.iond[satno-1].azel[0]=azel[0]*D2R;
        sstat->sion.iond[satno-1].azel[1]=azel[1]*D2R;
        sstat->sion.iond[satno-1].qflag=(unsigned char)qflag;
        return 0; /* no update */
        }
    /* troposphere ztd */
    else if (sscanf(buff,"$TROP,%d,%lf,%d,%d,%lf,%lf",&week,&tow,&qflag,&rcv,
                        &ztd,std)>=6) {
        sstat->time=gpst2time(week,tow);
        sstat->strp.trpd.time=sstat->time;
        sstat->strp.trpd.trp[0]=ztd;
        sstat->strp.trpd.std[0]=std[0];
        sstat->strp.trpd.qflag=(unsigned char)qflag;
        return 0; /* no update */
        }
    /* troposphere gradient */
    else if (sscanf(buff,"$TRPG,%d,%lf,%d,%d,%lf,%lf,%lf,%lf",&week,&tow,
                    &qflag,&rcv,grad,grad+1,std,std+1)>=8) {
            time=gpst2time(week,tow);
            if (timediff(time, sstat->strp.trpd.time) != 0.0) {
                return 0; /* no update */
            }
        sstat->strp.trpd.trp[1]=grad[0];
        sstat->strp.trpd.trp[2]=grad[1];
        sstat->strp.trpd.std[1]=std[0];
        sstat->strp.trpd.std[2]=std[1];
        return 0; /* no update */
    }

    return 11; /* update */
}
/* stack local correction ------------------------------------------------------
* stack stat message and synchronize frame with stat preamble
* args   : sstat_t *sstat    IO  local correction data
*          const char data   I   stat 1byte data
*          char  *buff       O   buffer for message
*          int   *nbyte      O   number of bytes
* return : status (-1: error message, 0: no message,
*                  11: input stat messages)
*-----------------------------------------------------------------------------*/
extern int input_stat(sstat_t *sstat, const char data, char* buff, int *nbyte)
{
    trace(NULL,4,"input_stat: data=%c,%d\n",data,*nbyte);

    /* synchronize frame */
    if (*nbyte==0) {
        if (data != '$') {
            return 0;
        }
        buff[*nbyte]=data;
        (*nbyte)++;
        return 0;
    }
    buff[(*nbyte)++]=data;

    if (data != '\n') {
        return 0;
    }

    return decode_stat(sstat,buff,nbyte);
}
/* input stat message from file ------------------------------------------------
* fetch next stat message and input a messsage from file
* args   : sstat_t *sstat    IO   local correction data
*          FILE  *fp         I    file pointer
*          char  *buff       O    buffer for message
*          int   *nbyte      O    number of bytes
* return : status (-2: end of file, -1...11: same as input_stat())
* note   : same as input_stat()
*-----------------------------------------------------------------------------*/
extern int input_statf(sstat_t *sstat, FILE *fp, char *buff, int *nbyte)
{
    int i,data,ret;

    trace(NULL,4,"input_statf:\n");

    for (i=0;i<4096;i++) {
        if ((data = fgetc(fp)) == EOF) {
            return -2;
        }
        if ((ret = input_stat(sstat, (char)data, buff, nbyte))) {
            return ret;
        }
    }
    return 0; /* return at every 4k bytes */
}
/* calculate distance between two positions -----------------------------------
* calculate the Euclidean distance between two positions
* args   : double *ecef1    I   position 1 (ECEF coordinates)
*          double *ecef2    I   position 2 (ECEF coordinates)
* return : distance between the two positions
*-----------------------------------------------------------------------------*/
extern double posdist(const double *ecef1, const double *ecef2)
{
    int i;
    double r[3];
    for (i = 0; i < 3; i++) {
        r[i] = ecef1[i] - ecef2[i];
    }
    return norm(r,3);
}
/* get neighbour sites -------------------------------------------------------*/
static int get_site(const stat_t *stat, const double *llh, int iontrp,
                    int *siteno, double maxdist, double *dist, double gllh[][3])
{
    double ecef[3],d;
    int i,n=0,nsta;
    const spos_t *site;

    if(iontrp==TYPE_TRP){
        nsta=stat->nst;
    }
    else if(iontrp==TYPE_ION){
        nsta=stat->nsi;
    }
    else{
        return 0;
    }

    pos2ecef(llh,ecef);
    for (i=0;i<nsta;i++) {
        if(iontrp==TYPE_TRP){
            site=&stat->strp[i].site;
        }
        else if(iontrp==TYPE_ION){
            site=&stat->sion[i].site;
        }
        else{
            return 0;
        }
        d=posdist(site->ecef,ecef);
        ecef2pos(site->ecef,gllh[n]);
        trace(NULL,4,"get_site:targetpos llh=%f,%f,%f site pos llh=%f,%f,%f dist=%f\n",
            llh[0]*R2D,llh[1]*R2D,llh[2],
            gllh[n][0]*R2D,gllh[n][1]*R2D,gllh[n][2],d);
        if ((dist[n] = MAX(d / 1E3, 0.1)) > maxdist) {
            continue;
        }
        siteno[n++]=i;
    }
    return n;
}
/* get trop correction for a point -------------------------------------------*/
extern int get_trop_sta(gtime_t time, const trp_t *trpd, double *trp,
                        double *std)
{
    double tt;
    int i;

    tt=timediff(trpd->time,time);
    if (tt < -600.0 || tt > 5.) {
        return 0;
    }

    for (i=0;i<3;i++) {
        trp[i]=trpd->trp[i];
        std[i]=sqrt(SQR(trpd->std[i])+SQR(tt*(i==0?PRN_ZTD:PRN_GRAD)));
        trace(NULL,4,"trpd %s %d ion=%f std=%f\n",
            time_str(time,0), i+1, trp[i], std[i]);
    }
    return 1;
}
/* get iono correction for a point -------------------------------------------*/
extern int get_iono_sta(gtime_t time, const ion_t *iond, double *ion,
                        double *std, double *el)
{
    double tt;
    int i,cnt=0;

    for (i=0;i<MAXSAT;i++) {
        tt=timediff(iond[i].time,time);
        if (tt < -35.0) {
            continue;
        }
        if (tt > 5.) {
            continue;
        }
        ion[i]=iond[i].ion;
        std[i]=iond[i].std;
        el[i]=iond[i].azel[1];
        trace(NULL,4,"iond %s sat=%d ion=%f std=%f\n",
            time_str(time,0), i+1, ion[i], std[i]);
        cnt++;
    }
    return cnt;
}
/* calculate tropospheric correction using weighted interpolation by distance --
* interpolation tropospheric weighted average by distance
* args   : const stat_t *stat I   correction data
*          gtime_t time       I   time (GPST)
*          const double *llh  I   receiver ecef position {lat, lon, hgt} (rad, m)
*          double maxdist     I   max distance thresholds (km)
*          double *trp        O   tropos parameters {ztd, grade, gradn} (m)
*          double *std        O   standard deviation (m)
* return : status (1:ok, 0:error)
*-----------------------------------------------------------------------------*/
extern int corr_trop_distwgt(const stat_t *stat, gtime_t time,
                             const double *llh,double maxdist, double *trp,
                             double *std)
{
     const double azel[]={0.0,PI/2.0};
     double trp_s[3],std_s[3];
     double var,wgt[3]={0},cof[3]={0},dist[MAXSITES],llh_s[MAXSITES][3];
     int i,j,n,siteno[MAXSITES];

     for (i = 0; i < 3; i++) {
         trp[i] = std[i] = 0.0;
     }

     if (!(n = get_site(stat, llh, TYPE_TRP, siteno, maxdist, dist, llh_s))) {
         return 0;
     }

     for (i=0;i<n;i++) {
         /* get trop correction for a station */
         if(get_trop_sta(time, &stat->strp[siteno[i]].trpd, trp_s, std_s)==0) {
            continue;
        }

        /* convert ztd to zwd */
        trp_s[0]-=tropmodel(time,llh_s[i],azel,0.0);

        for (j=0;j<3;j++) {
            var=SQR(std_s[j])+SQR(dist[i]*(j==0?STD_ZTD_DIST:STD_GRAD_DIST));
            trp[j]+=trp_s[j]/dist[i];
            wgt[j]+=1.0/dist[i];
            cof[j]+=1.0/var;
        }
     }
     if (wgt[0] == 0.0) {
         return 0;
     }

     for (i=0;i<3;i++) {
        trp[i]/=wgt[i];
        std[i]=sqrt(1.0/cof[i]);
        trace(NULL,3,"interpolation trp  %s %d trp=%f std=%f lat=%.2f lat=%.2f n=%d\n",
            time_str(time,0), i+1, trp[i], std[i], llh[0]*R2D, llh[1]*R2D, n);
     }
     /* convert zwd to ztd */
     trp[0]+=tropmodel(time,llh,azel,0.0);
     return 1;
}
/* calculate ionospherec correction using weighted interpolation by distance ---
* interpolation ionospherec weighted average by distance
* args   : const stat_t *stat I   local correction data
*          gtime_t time       I   time (GPST)
*          const double *llh  I   receiver ecef position {lat, lon, hgt} (rad, m)
*          const double *el   I   receiver to satellite elevation (rad)
*          double maxdist     I   max distance thresholds (km)
*          double *ion        O   L1 slant ionos delay for each sat (MAXSAT x 1)
*                                 (ion[i]==0: no correction data)
*          double *std        O   standard deviation (m)
*          double maxres      I   maximum residual threshold (m)
*          int *nrej          O   number of rejected stations
*          int *ncnt          O   number of counted stations
* return : status (1:ok, 0:error)
*-----------------------------------------------------------------------------*/
extern int corr_iono_distwgt(const stat_t *stat, gtime_t time,
                             const double *llh,const double *el, double maxdist,
                             double *ion, double *std, double maxres, int *nrej,
                             int *ncnt)
{
    double ion_s[MAXSITES][MAXSAT]={{0.0}},std_s[MAXSAT]={0.0},el_s[MAXSAT]={0.0};
    double var,wgt[MAXSAT]={0},cof[MAXSAT]={0},dist[MAXSITES],llh_s[MAXSITES][3];
    int i,j,n,siteno[MAXSITES];
    double eltmp, res;

    for (i = 0; i < MAXSAT; i++) {
        ion[i] = 0.0;
    }

    if (!(n = get_site(stat, llh, TYPE_ION, siteno, maxdist, dist, llh_s))) {
        return 0;
    }

    for (i=0;i<n;i++) {
        for (j = 0; j < MAXSAT; j++) {
            ion_s[i][j] = std_s[j] = el_s[j] = 0.0;
        }

        /* get iono correction for a station */
        if(get_iono_sta(time, stat->sion[siteno[i]].iond, ion_s[i], std_s,
            el_s)==0) {
            continue;
        }

        for (j=0;j<MAXSAT;j++) {
            if(el==NULL){
                eltmp=el_s[j];
            }else{
                eltmp=el[j];
            }
            if (ion_s[i][j] == 0.0 || eltmp <= 0.0) {
                continue;
            }
            var=SQR(std_s[j])+SQR(dist[i]*STD_VTEC_DIST/sin(eltmp));
            ion[j]+=ion_s[i][j]/dist[i];
            wgt[j]+=1.0/dist[i];
            cof[j]+=1.0/var;
        }
    }
    for (i=0;i<MAXSAT;i++) {
        if (wgt[i] == 0.0 || (cof[i] <= 0.0)) {
            continue;
        }
        ion[i]/=wgt[i];
        std[i]=sqrt(1.0/cof[i]);
        trace(NULL,3,"interpolation ion  %s sat=%d ion=%f std=%f lat=%.2f lat=%.2f n=%d\n",
            time_str(time,0), i+1, ion[i], std[i], llh[0]*R2D, llh[1]*R2D, n);
    }
    if(maxres > 0.0){
        for (i=0;i<n;i++) {
            for (j=0;j<MAXSAT;j++) {
                if (ion_s[i][j] == 0.0 || ion[j] == 0.0) {
                    continue;
                }
                res = ion[j]-ion_s[i][j];
                ncnt[siteno[i]]++;
                if (fabs(res) > maxres) {
                    nrej[siteno[i]]++;
                }
            }
        }
    }
    return 1;
}
/* convert rtcm3 block to stats ------------------------------------------------
* convert RTCM3 block data to station statistics
* args   : rtcm_t *rtcm    IO   rtcm control struct
*          stat_t *stat    IO   station data struct
* return : status (0: error, 1: success)
*-----------------------------------------------------------------------------*/
extern int block2stat(rtcm_t *rtcm, stat_t *stat)
{
    int i, j, k, gp;
    double d;

    if (rtcm->lclblk.tnum == 0 && rtcm->lclblk.inum == 0) {
        return 0;
    }

    /* convert trop corrections */
    for (i=0;i<rtcm->lclblk.tnum;i++) {
        for(j=0;j<rtcm->lclblk.tblkinf[i].n;j++){
            if(rtcm->lclblk.tblkinf[i].btype==BTYPE_GRID){
                gp=rtcm->lclblk.tblkinf[i].gp[j];
            }
            else{
                gp=j;
            }
            for(k=0;k<stat->nst;k++){
                d=posdist(rtcm->lclblk.tstat[i][gp].site.ecef,
                    stat->strp[k].site.ecef);
                if (d < 0.1) {
                    break;
                }
            }
            if(k>=stat->nst) {
                stat->nst++;
            }
            memcpy(&stat->time[k], &rtcm->time,sizeof(gtime_t));
            memcpy(&stat->strp[k], &rtcm->lclblk.tstat[i][gp],
                sizeof(sitetrp_t));
        }
    }
    /* update iono corrections */
    for (i=0;i<rtcm->lclblk.inum;i++) {
        for(j=0;j<rtcm->lclblk.iblkinf[i].n;j++){
            gp=rtcm->lclblk.iblkinf[i].gp[j];
            if(rtcm->lclblk.iblkinf[i].btype==BTYPE_GRID){
                gp=rtcm->lclblk.iblkinf[i].gp[j];
            }
            else{
                gp=j;
            }
            for(k=0;k<stat->nsi;k++){
                d=posdist(rtcm->lclblk.istat[i][gp].site.ecef,
                    stat->sion[k].site.ecef);
                if (d < 0.1) {
                    break;
                }
            }
            if(k>=stat->nsi) {
                stat->nsi++;
            }
            memcpy(&stat->time[k], &rtcm->time,sizeof(gtime_t));
            memcpy(&stat->sion[k], &rtcm->lclblk.istat[i][gp],
                sizeof(siteion_t));
        }
    }
    return 1;
}
