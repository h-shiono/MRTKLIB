/*------------------------------------------------------------------------------
* mrtk_ppp_iono.c : PPP ionospheric correction functions
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
*
* references :
*     [1]  CAO IS-QZSS-MDC-002, November, 2023
*
* version : $Revision:$ $Date:$
* history : 2024/01/10 1.0  new, for MADOCALIB
*           2024/05/24 1.1  algorithm correction for const_iono_corr().
*                           change convergence threshold (H,V) from
*                           0.3,0.5 to 2.0,3.0.
*           2024/09/27 1.2  delete NM
*           2025/03/25 1.3  unuse mapping function.
*-----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_rtkpos.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_mat.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "mrtklib/mrtk_trace.h"

/* forward declarations */
extern int satsys(int sat, int *prn);
extern int satid2no(const char *id);
extern void satno2id(int sat, char *id);
extern char *time_str(gtime_t t, int n);
extern double timediff(gtime_t t1, gtime_t t2);
extern gtime_t gpst2time(int week, double sec);
extern void ecef2pos(const double *r, double *pos);
extern void covenu(const double *pos, const double *P, double *Q);
extern double sat2freq(int sat, uint8_t code, const nav_t *nav);

#define IONO_REJ_STD     1.0 /* reject   STEC corr. std (m) */
#define IONO_COV_RATIO   1.0 /* ratio of STEC corr. cov (m) */
#define IONO_THRE_H      2.0 /* Horizontal convergence threshold (m) */
#define IONO_THRE_V      3.0 /* Vertical   convergence threshold (m) */

#define SQR(x)      ((x)*(x))
#define SQRT(x)     ((x)<=0.0||(x)!=(x)?0.0:sqrt(x))

/* number and index of states */
#define NP(opt)     ((opt)->dynamics?9:3)
#define NC(opt)     (NSYS+1) /* BDS3 and BDS2 */
#define NT(opt)     ((opt)->tropopt<TROPOPT_EST?0:((opt)->tropopt==TROPOPT_EST?1:3))
#define II(s,opt)   (NP(opt)+NC(opt)+NT(opt)+(s)-1)

/* standard deviation by STEC URA --------------------------------------------*/
static double miono_std_stecura(int ura)
{
    /* ref [1] 6.3.2.3 (5) */
    if (ura<= 0) return 5.4665;     /* STEC URA undefined/unknown */
    if (ura>=63) return 5.4665;
    return (pow(3.0,(ura>>3)&7)*(1.0+(ura&7)/4.0)-1.0)*1E-3;
}

/* L1 ionospheric delay ------------------------------------------------------*/
static double miono_delay(const nav_t *nav, const int sat,
                          const miono_area_t *a, const double *ll)
{
    const double *c=a->sat[sat-1].coef;
    double freq,fact,stec,delay;

    freq = sat2freq(sat, CODE_L1C, nav);

    /* ref [1] 6.3.2.3 (5) */
    fact = 40.31E16 / freq / freq;  /* TECU -> L1 ionospheric delay(m) */
    stec = c[0];                    /* STEC poly.coef.{C00,C01,C10,C11,C02,C20} */
    if(a->type >= 1) stec += c[1] *    (ll[0] - a->ref[0]) + c[2] *    (ll[1] - a->ref[1]);
    if(a->type >= 2) stec += c[3] *    (ll[0] - a->ref[0])        *    (ll[1] - a->ref[1]);
    if(a->type >= 3) stec += c[4] * SQR(ll[0] - a->ref[0]) + c[2] * SQR(ll[1] - a->ref[1]);

    delay = fact * stec;
    trace(NULL,5,"miono_delay: sat=%d,fact=%5.3f,stec=%8.3f,delay=%7.3f,coef=%7.2f,%7.2f,%7.2f,%7.2f,%8.3f,%8.3f\n",
        sat,fact,stec,delay,c[0],c[1],c[2],c[3],c[4],c[5]);
    return delay;
}

/* select MADOCA-PPP ionospheric correction area -----------------------------*/
static miono_area_t *miono_sel_area(pppiono_t *pppiono, const double *rr,
                                    const double *ll)
{
    const miono_region_t *re;
    int i,j,k,min_i=-1,min_j=0;
    double ref_ecef[3],ref_llh[3]={0},diff[3],dist,min_dist=1E5;

    trace(NULL,2,"miono_sel_area: lat=%.2f,lon=%.2f\n", ll[0],ll[1]);

    for (i = 0; i < MIONO_MAX_RID; i++) {
        re = &pppiono->re[i];
        if(!re->rvalid) continue;
        trace(NULL,5,"miono_sel_area: region i=%d,ralert=%d\n",i,re->ralert);
        if(re->ralert) continue;

        for (j = 0; j < MIONO_MAX_ANUM; j++) {
            if(!re->area[j].avalid) continue;
            for(k = 0; k < 2; k++) ref_llh[k] = re->area[j].ref[k] * D2R; /* deg -> rad */
            pos2ecef(ref_llh, ref_ecef);
            for(k = 0; k < 3; k++) diff[k] = rr[k] - ref_ecef[k];
            dist=norm(diff, 3) / 1E3;  /* m -> km */
            trace(NULL,5,"miono_sel_area: area i=%d,j=%d,sid=%d,ref=%6.2f,%7.2f,dist=%10.3f,min_dist=%10.3f\n",
                i,j,re->area[j].sid, re->area[j].ref[0], re->area[j].ref[1], dist, min_dist);
            if(dist > min_dist) continue;

            if(re->area[j].sid == 0) { /* rectangle */
                if((ll[0] < (re->area[j].ref[0] - re->area[j].span[0])) ||
                   (ll[0] > (re->area[j].ref[0] + re->area[j].span[0])) ||
                   (ll[1] < (re->area[j].ref[1] - re->area[j].span[1])) ||
                   (ll[1] > (re->area[j].ref[1] + re->area[j].span[1]))) continue;
            }
            else {                     /* circle */
                if(dist > re->area[j].span[0]) continue;
            }
            trace(NULL,2,"miono_sel_area: closest RegionID=%d,AreaNo=%d,dist=%.3f\n", i, j, dist);
            min_i = i; min_j = j; min_dist = dist;
        }
    }

    if(min_i == -1) {
        trace(NULL,2,"miono_sel_area: no corresponding area. lat=%.2f,lon=%.2f\n", ll[0],ll[1]);
        return NULL;
    }
    pppiono->rid  = min_i;
    pppiono->anum = min_j;
    return &pppiono->re[min_i].area[min_j];
}

/* get MADOCA-PPP ionospheric correction data --------------------------------
* get MADOCA-PPP ionospheric correction data using the user position
* args   : double  *rr     I   user position (ECEF)
*          nav_t   *nav    IO  navigation data (pppiono correction updated)
* return : status (0: not updated, 1: updated)
*-----------------------------------------------------------------------------*/
extern int miono_get_corr(const double *rr, nav_t *nav)
{
    miono_area_t *area;
    int i;
    double pos[3],latlon[2];

    if (!nav->pppiono) return 0;

    trace(NULL,4,"miono_get_corr: rr=%.3f,%.3f,%.3f\n",rr[0],rr[1],rr[2]);

    if((rr[0] == 0.0) && (rr[1] == 0.0) && (rr[2] == 0.0)) return 0;

    ecef2pos(rr, pos);
    latlon[0]=pos[0]*R2D;
    latlon[1]=pos[1]*R2D;

    if (!(area = miono_sel_area(nav->pppiono, rr, latlon))) return 0;

    for(i = 0; i < MAXSAT; i++) {
        if (!area->sat[i].t0.time) {
            nav->pppiono->corr.t0[i].time = 0;
            continue;
        }
        nav->pppiono->corr.t0[i]  = area->sat[i].t0;
        nav->pppiono->corr.dly[i] = miono_delay(nav, i+1, area, latlon);
        nav->pppiono->corr.std[i] = miono_std_stecura(area->sat[i].sqi);
    }
    return 1;
}

/* read stat correction data -------------------------------------------------
* read ppp ionosphere correction data from stat file
* args   : pppiono_corr_t *corr IO  ppp ionosphere correction data
*          FILE           *fp   I   file pointer
* return : status (-2: end of file, 0: no data, 1: input correction data)
*-----------------------------------------------------------------------------*/
extern int input_statcorr(pppiono_corr_t *corr, FILE *fp)
{
    gtime_t time;
    int week,stat,sat,ret=0;
    double tow,pos[3],std[3],azel[2],dly,dlystd;
    char buff[1024],satid[64],*p;

    /* Note, File format :
       $POS,GPS Week,TOW,status(1:FIX,6:PPP),X(m),Y(m),Z(m),X STD(m), Y STD(m),Z STD(m)
       $ION,GPS Week,TOW,status(1:FIX,6:PPP),Sat,Az(deg),El(deg),L1 Slant Delay(m),L1 Slant Delay STD(m)
    */

    while (fgets(buff,sizeof(buff),fp)) {

        for (p=buff;*p;p++) if (*p==',') *p=' ';

        /* $POS record */
        if (sscanf(buff,"$POS%d%lf%d%lf%lf%lf%lf%lf%lf",
                &week,&tow,&stat,pos,pos+1,pos+2,std,std+1,std+2)>=9) {
            time=gpst2time(week,tow);
            if((corr->time.time != 0)&&(corr->time.time != time.time)) return ret;
        }

        /* $ION record */
        if (sscanf(buff,"$ION%d%lf%d%s%lf%lf%lf%lf",
                &week,&tow,&stat,satid,azel,azel+1,&dly,&dlystd)>=8) {
            time=gpst2time(week,tow);
            if((sat=satid2no(satid))!=0) {
                corr->t0[sat-1] =corr->time=time;
                corr->dly[sat-1]=dly;
                corr->std[sat-1]=dlystd;
                ret=1;
            }
            trace(NULL,4,"input_statcorr : %s,%s,az=%7.2f,el=%5.2f,dly=%8.4f,std=%8.4f\n",
                time_str(time,0),satid,azel[0],azel[1],dly,dlystd);
        }
    }
    return -2;
}

/* constraint to ionospheric correction --------------------------------------*/
extern int const_iono_corr(rtk_t *rtk, const obsd_t *obs, const nav_t *nav,
                           const int n, const double *azel, const int *exc,
                           const double *rr, const double *x, double *v,
                           double *H, double *var)
{
    gtime_t time=obs[0].time;
    pppiono_corr_t corr;
    int i,j,k,sat,nv=0,s,ns;
    double tt,std,sd[2],pos[3],P[9],Q[9];
    double rejstd=IONO_REJ_STD,covratio=IONO_COV_RATIO,thre[2]={IONO_THRE_H,IONO_THRE_V};
    double ave,bsys[NSYS]={0.0};
    char *p,*tstr,satid[8];
    const int sys[NSYS]={SYS_GPS,SYS_GLO,SYS_GAL,SYS_QZS,0};

    if (!nav->pppiono) return 0;

    if ((p=strstr(rtk->opt.pppopt,"-IONOCORR_THRE_H="))) {
        sscanf(p,"-IONOCORR_THRE_H=%lf",&thre[0]);
    }
    if ((p=strstr(rtk->opt.pppopt,"-IONOCORR_THRE_V="))) {
        sscanf(p,"-IONOCORR_THRE_V=%lf",&thre[1]);
    }
    if ((p=strstr(rtk->opt.pppopt,"-IONOCORR_REJ_STD="))) {
        sscanf(p,"-IONOCORR_REJ_STD=%lf",&rejstd);
    }
    if ((p=strstr(rtk->opt.pppopt,"-IONOCORR_COV_RATIO="))) {
        sscanf(p,"-IONOCORR_COV_RATIO=%lf",&covratio);
    }

    ecef2pos(rr,pos);
    P[0]     =rtk->prev_qr[0]; /* xx */
    P[4]     =rtk->prev_qr[1]; /* yy */
    P[8]     =rtk->prev_qr[2]; /* zz */
    P[1]=P[3]=rtk->prev_qr[3]; /* xy */
    P[5]=P[7]=rtk->prev_qr[4]; /* yz */
    P[2]=P[6]=rtk->prev_qr[5]; /* zx */
    covenu(pos,P,Q);
    sd[0] = SQRT(Q[0]+Q[4]);   /* horizontal std */
    sd[1] = SQRT(Q[8]);        /* vertical   std */

    trace(NULL,3,"const_iono_corr: sd=%6.2f,%6.2f,thre=%6.2f,%6.2f,rejstd=%.3f,covratio=%.3f\n",
        sd[0],sd[1],thre[0],thre[1],rejstd,covratio);

    if((sd[0]!=0.0)&&(sd[1]!=0.0)&&(sd[0]<thre[0])&&(sd[1]<thre[1])) {
        trace(NULL,3,"const_iono_corr: skipped to constraint. std H:%6.2f<%6.2f V:%6.2f<%6.2f\n",
            sd[0],thre[0],sd[1],thre[1]);
        return 0;
    }

    corr = nav->pppiono->corr;

    /* estimate system bias */
    for (s=0;sys[s];s++) { /* for each system */
        ave=0.0;ns=0;
        for (i=0;i<n;i++) {
            sat=obs[i].sat;
            if (satsys(sat,NULL)!=sys[s]) continue;
            if(corr.t0[sat-1].time == 0.0) continue;
            tt = fabs(timediff(time,corr.t0[sat-1]));
            tstr = time_str(corr.t0[sat-1],0);

            satno2id(sat, satid);
            if (exc[i]) continue;
            if (tt > MIONO_MAX_AGE) continue;
            if (corr.std[sat-1] > rejstd) continue;

            j=II(sat,&rtk->opt);
            ave+=(corr.dly[sat-1]-x[j]);
            ns++;
        }
        if (ns>0) bsys[s]=ave/(double)ns;
    }

    /* constraint to external ionosphere correction */
    for (i=0;i<n;i++) {
        sat=obs[i].sat;
        if(corr.t0[sat-1].time == 0.0) continue;
        tt = fabs(timediff(time,corr.t0[sat-1]));
        tstr = time_str(corr.t0[sat-1],0);

        satno2id(sat, satid);

        switch (satsys(sat,NULL)) {
            case SYS_GPS: s=0; break;
            case SYS_GLO: s=1; break;
            case SYS_GAL: s=2; break;
            case SYS_QZS: s=3; break;
            default:      continue;
        }

        if (exc[i]) continue;
        if (tt > MIONO_MAX_AGE) continue;
        if (corr.std[sat-1] > rejstd) continue;

        std = corr.std[sat-1];
        j=II(sat,&rtk->opt);
        v[nv]=corr.dly[sat-1]-x[j]-bsys[s];

        trace(NULL,2,"const_iono_corr: %s,%s,tt=%11d,az=%5.1f,el=%4.1f,sdly=%8.4f,std=%8.4f,v=%8.4f,x=%8.4f,bsys=%8.4f\n",
            satid,tstr,(int)tt,(azel+i*2)[0]*R2D,(azel+i*2)[1]*R2D,corr.dly[sat-1],corr.std[sat-1],v[nv],x[j],bsys[s]);

        for (k=0;k<rtk->nx;k++) H[k+nv*rtk->nx]=k==j?1.0:0.0;
        var[nv++]=SQR(std)*covratio;
    }
    return nv;
}
