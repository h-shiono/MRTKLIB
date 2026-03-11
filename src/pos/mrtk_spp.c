/*------------------------------------------------------------------------------
 * mrtk_spp.c : standard single-point positioning
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
#include "mrtklib/mrtk_spp.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_atmos.h"
#include "mrtklib/mrtk_eph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mrtklib/mrtk_trace.h"

/* local constants -----------------------------------------------------------*/
static const double CLIGHT    = 299792458.0;
static const double D2R       = 3.1415926535897932 / 180.0;
static const double R2D       = 180.0 / 3.1415926535897932;
static const double OMGE      = 7.2921151467E-5;

static const double FREQ1     = 1.57542E9;
static const double FREQ2     = 1.22760E9;
static const double FREQ5     = 1.17645E9;
static const double FREQ7     = 1.20714E9;
static const double FREQ9     = 2.492028E9;
static const double FREQ1_GLO = 1.60200E9;
static const double FREQ2_GLO = 1.24600E9;
static const double FREQ1_CMP = 1.561098E9;
static const double FREQ2_CMP = 1.20714E9;

static const double EFACT_GPS = 1.0;
static const double EFACT_GLO = 1.5;
static const double EFACT_SBS = 3.0;

#define SYS_NONE    0x00
#define SYS_GPS     0x01
#define SYS_SBS     0x02
#define SYS_GLO     0x04
#define SYS_GAL     0x08
#define SYS_QZS     0x10
#define SYS_CMP     0x20
#define SYS_IRN     0x40
#define SYS_BD2     0x100

/*--- forward declarations for legacy functions resolved at link time -------*/

/* chi-sqr(n) table (alpha=0.001) — moved from rtkcmn.c */
const double chisqr[100]={
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

/* constants/macros ----------------------------------------------------------*/

#define SQR(x)      ((x)*(x))

#define NT          7           /* # of estimated time system (gps,glo,gal,bds3,irn,qzs,bds2) */
#define NX          (3+NT)      /* # of estimated parameters */
#define MAXITR      10          /* max number of iteration for point pos */
#define ERR_CBIAS   0.3         /* code bias error Std (m) */
#define MIN_EL      (5.0*D2R)   /* min elevation for measurement error (rad) */

/* pseudorange measurement error variance ------------------------------------*/
static double varerr(const prcopt_t *opt, double el, int sys)
{
    double fact,varr;
    fact=sys==SYS_GLO?EFACT_GLO:(sys==SYS_SBS?EFACT_SBS:EFACT_GPS);
    if (el < MIN_EL) {
        el = MIN_EL;
    }
    varr=SQR(opt->err[0])*(SQR(opt->err[1])+SQR(opt->err[2])/sin(el));
    if (opt->ionoopt == IONOOPT_IFLC) {
        varr *= SQR(3.0); /* iono-free */
    }
    return SQR(fact)*varr;
}
/* get group delay parameter (m) ---------------------------------------------*/
static double gettgd(int sat, const nav_t *nav, int type)
{
    int i,sys=satsys(sat,NULL);

    if (sys==SYS_GLO) {
        for (i=0;i<nav->ng;i++) {
            if (nav->geph[i].sat == sat) {
                break;
            }
        }
        return (i>=nav->ng)?0.0:-nav->geph[i].dtaun*CLIGHT;
    }
    else {
        for (i=0;i<nav->n;i++) {
            if (nav->eph[i].sat == sat) {
                break;
            }
        }
        return (i>=nav->n)?0.0:nav->eph[i].tgd[type]*CLIGHT;
    }
}
/* test SNR mask -------------------------------------------------------------*/
static int snrmask(const obsd_t *obs, const double *azel, const prcopt_t *opt)
{
    if (testsnr(0,0,azel[1],obs->SNR[0]*SNR_UNIT,&opt->snrmask)) {
        return 0;
    }
    if (opt->ionoopt==IONOOPT_IFLC) {
        if (testsnr(0, 1, azel[1], obs->SNR[1] * SNR_UNIT, &opt->snrmask)) {
            return 0;
        }
    }
    return 1;
}
/* psendorange with code bias correction -------------------------------------*/
static double prange(const obsd_t *obs, const nav_t *nav, const prcopt_t *opt,
                     double *var)
{
    double P1,P2,gamma,b1,b2,freq1,freq2;
    int sat,sys;

    sat=obs->sat;
    sys=satsys(sat,NULL);
    P1=obs->P[0];
    P2=obs->P[1];
    *var=0.0;

    if (P1 == 0.0 || (opt->ionoopt == IONOOPT_IFLC && P2 == 0.0)) {
        return 0.0;
    }

    freq1=sat2freq(sat,obs->code[0],nav);
    freq2=sat2freq(sat,obs->code[1],nav);
    gamma=SQR(freq1/freq2);

    /* P1-C1,P2-C2 DCB correction */
    if (sys==SYS_GPS||sys==SYS_GLO) {
        if (obs->code[0] == CODE_L1C) {
            P1 += nav->cbias[sat - 1][1]; /* C1->P1 */
        }
        if (obs->code[1] == CODE_L2C) {
            P2 += nav->cbias[sat - 1][2]; /* C2->P2 */
        }
    }
    if (opt->ionoopt==IONOOPT_IFLC) { /* dual-frequency */

        if (P1 == 0.0 || P2 == 0.0) {
            return 0.0;
        }

        if (sys==SYS_GPS||sys==SYS_QZS) {
            return (P2-gamma*P1)/(1.0-gamma);
        }
        else if (sys==SYS_GLO) {
            return (P2-gamma*P1)/(1.0-gamma);
        }
        else if (sys==SYS_GAL) {
            if (getseleph(SYS_GAL)) { /* F/NAV */
                P2-=gettgd(sat,nav,0)-gettgd(sat,nav,1); /* BGD_E5aE5b */
            }
            return (P2-gamma*P1)/(1.0-gamma);
        }
        else if (sys==SYS_CMP) {
            if (obs->code[0] == CODE_L2I) {
                b1 = gettgd(sat, nav, 0); /* TGD_B1I */
            } else if (obs->code[0] == CODE_L1P) {
                b1 = gettgd(sat, nav, 2); /* TGD_B1Cp */
            } else {
                b1 = gettgd(sat, nav, 2) + gettgd(sat, nav, 4); /* TGD_B1Cp+ISC_B1Cd */
            }
            b2=gettgd(sat,nav,1); /* TGD_B2I/B2bI (m) */
            return ((P2-gamma*P1)-(b2-gamma*b1))/(1.0-gamma);
        }
        else if (sys==SYS_IRN) {
            return (P2-gamma*P1)/(1.0-gamma);
        }
    }
    else { /* single-freq (L1/E1/B1) */
        *var=SQR(ERR_CBIAS);

        if (sys==SYS_GPS||sys==SYS_QZS) { /* L1 */
            b1=gettgd(sat,nav,0); /* TGD (m) */
            return P1-b1;
        }
        else if (sys==SYS_GLO) { /* G1 */
            b1=gettgd(sat,nav,0); /* -dtaun (m) */
            return P1-b1/(gamma-1.0);
        }
        else if (sys==SYS_GAL) { /* E1 */
            if (getseleph(SYS_GAL)) {
                b1 = gettgd(sat, nav, 0); /* BGD_E1E5a */
            } else {
                b1 = gettgd(sat, nav, 1); /* BGD_E1E5b */
            }
            return P1-b1;
        }
        else if (sys==SYS_CMP) { /* B1I/B1Cp/B1Cd */
            if (obs->code[0] == CODE_L2I) {
                b1 = gettgd(sat, nav, 0); /* TGD_B1I */
            } else if (obs->code[0] == CODE_L1P) {
                b1 = gettgd(sat, nav, 2); /* TGD_B1Cp */
            } else {
                b1 = gettgd(sat, nav, 2) + gettgd(sat, nav, 4); /* TGD_B1Cp+ISC_B1Cd */
            }
            return P1-b1;
        }
        else if (sys==SYS_IRN) { /* L5 */
            b1=gettgd(sat,nav,0); /* TGD (m) */
            return P1-gamma*b1;
        }
    }
    return P1;
}
/* ionocorr, tropcorr moved to mrtk_atmos.c */

/* pseudorange residuals -----------------------------------------------------*/
static int rescode(int iter, const obsd_t *obs, int n, const double *rs,
                   const double *dts, const double *vare, const int *svh,
                   const nav_t *nav, const double *x, const prcopt_t *opt,
                   double *v, double *H, double *var, double *azel, int *vsat,
                   double *resp, int *ns)
{
    gtime_t time;
    double r,freq,dion=0.0,dtrp=0.0,vmeas,vion=0.0,vtrp=0.0,rr[3],pos[3],dtr;
    double e[3],P,fact_ion;
    int i,j,nv=0,sat,sys,mask[NX-3]={0};

    trace(NULL,3,"resprng : n=%d\n",n);

    for (i = 0; i < 3; i++) {
        rr[i] = x[i];
    }
    dtr=x[3];

    ecef2pos(rr,pos);

    for (i=*ns=0;i<n&&i<MAXOBS;i++) {
        vsat[i]=0; azel[i*2]=azel[1+i*2]=resp[i]=0.0;
        time=obs[i].time;
        sat=obs[i].sat;
        if (!(sys = satsys_bd2(sat, NULL))) {
            continue;
        }

        /* reject duplicated observation data */
        if (i<n-1&&i<MAXOBS-1&&sat==obs[i+1].sat) {
            trace(NULL,2,"duplicated obs data %s sat=%d\n",time_str(time,3),sat);
            i++;
            continue;
        }
        /* excluded satellite? */
        if (satexclude(sat, vare[i], svh[i], opt)) {
            continue;
        }

        /* geometric distance */
        if ((r = geodist(rs + i * 6, rr, e)) <= 0.0) {
            continue;
        }

        if (iter>0) {
            /* test elevation mask */
            if (satazel(pos, e, azel + i * 2) < opt->elmin) {
                continue;
            }

            /* test SNR mask */
            if (!snrmask(obs + i, azel + i * 2, opt)) {
                continue;
            }

            /* ionospheric correction */
            if (!ionocorr(time,nav,sat,pos,azel+i*2,opt->ionoopt,&dion,&vion)) {
                continue;
            }
            if ((freq = sat2freq(sat, obs[i].code[0], nav)) == 0.0) {
                continue;
            }
            fact_ion=SQR(FREQ1/freq);
            dion*=fact_ion;
            vion*=SQR(fact_ion);

            /* tropospheric correction */
            if (!tropcorr(time,nav,pos,azel+i*2,opt->tropopt,&dtrp,&vtrp)) {
                continue;
            }
        }
        /* psendorange with code bias correction */
        if ((P = prange(obs + i, nav, opt, &vmeas)) == 0.0) {
            continue;
        }

        /* pseudorange residual */
        v[nv]=P-(r+dtr-CLIGHT*dts[i*2]+dion+dtrp);

        /* design matrix */
        for (j=0;j<NX;j++) {
            H[j+nv*NX]=j<3?-e[j]:(j==3?1.0:0.0);
        }
        /* time system offset and receiver bias correction */
        if      (sys==SYS_GLO) {v[nv]-=x[4]; H[4+nv*NX]=1.0; mask[1]=1;}
        else if (sys==SYS_GAL) {v[nv]-=x[5]; H[5+nv*NX]=1.0; mask[2]=1;}
        else if (sys==SYS_CMP) {v[nv]-=x[6]; H[6+nv*NX]=1.0; mask[3]=1;} /* BDS-3 */
        else if (sys==SYS_IRN) {v[nv]-=x[7]; H[7+nv*NX]=1.0; mask[4]=1;}
        else if (sys==SYS_QZS) {v[nv]-=x[8]; H[8+nv*NX]=1.0; mask[5]=1;}
        else if (sys==SYS_BD2) {v[nv]-=x[9]; H[9+nv*NX]=1.0; mask[6]=1;
        } /* BDS-2 */
        else {
            mask[0] = 1;
        }

        vsat[i]=1; resp[i]=v[nv]; (*ns)++;

        /* variance of pseudorange error */
        var[nv++]=varerr(opt,azel[1+i*2],sys)+vare[i]+vmeas+vion+vtrp;

        trace(NULL,4,"sat=%2d azel=%5.1f %4.1f res=%7.3f sig=%5.3f\n",obs[i].sat,
              azel[i*2]*R2D,azel[1+i*2]*R2D,resp[i],sqrt(var[nv-1]));
    }
    /* constraint to avoid rank-deficient */
    for (i=0;i<NT;i++) {
        if (mask[i]) {
            continue;
        }
        v[nv]=0.0;
        for (j = 0; j < NX; j++) {
            H[j + nv * NX] = j == i + 3 ? 1.0 : 0.0;
        }
        var[nv++]=0.01;
    }
    return nv;
}
/* validate solution ---------------------------------------------------------*/
static int valsol(const double *azel, const int *vsat, int n,
                  const prcopt_t *opt, const double *v, int nv, int nx,
                  char *msg)
{
    double azels[MAXOBS*2],dop[4],vv;
    int i,ns;

    trace(NULL,3,"valsol  : n=%d nv=%d\n",n,nv);

    /* Chi-square validation of residuals */
    vv=dot(v,v,nv);
    if (nv>nx&&vv>chisqr[nv-nx-1]) {
        sprintf(msg,"chi-square error nv=%d vv=%.1f cs=%.1f",nv,vv,chisqr[nv-nx-1]);
        if (!opt->ign_chierr) {
            return 0;
        }
        trace(NULL,2,"ignore %s\n",msg);
    }
    /* large GDOP check */
    for (i=ns=0;i<n;i++) {
        if (!vsat[i]) {
            continue;
        }
        azels[  ns*2]=azel[  i*2];
        azels[1+ns*2]=azel[1+i*2];
        ns++;
    }
    dops(ns,azels,opt->elmin,dop);
    trace(NULL,4,"valsol  : n=%d nv=%d vv=%.1f cs=%.1f maxgdop=%.1f gdop=%.1f pdop=%.1f hdop=%.1f vdop=%.1f\n",
        n,nv,vv,chisqr[nv-nx-1],opt->maxgdop,dop[0],dop[1],dop[2],dop[3]);
    if (dop[0]<=0.0||dop[0]>opt->maxgdop) {
        sprintf(msg,"gdop error nv=%d gdop=%.1f",nv,dop[0]);
        return 0;
    }
    return 1;
}
/* estimate receiver position ------------------------------------------------*/
static int estpos(const obsd_t *obs, int n, const double *rs, const double *dts,
                  const double *vare, const int *svh, const nav_t *nav,
                  const prcopt_t *opt, sol_t *sol, double *azel, int *vsat,
                  double *resp, char *msg)
{
    double x[NX]={0},dx[NX],Q[NX*NX],*v,*H,*var,sig;
    int i,j,k,info,stat=1,nv,ns;

    trace(NULL,3,"estpos  : n=%d\n",n);

    v=mat(n+NT,1); H=mat(NX,n+NT); var=mat(n+NT,1);

    for (i = 0; i < 3; i++) {
        x[i] = sol->rr[i];
    }

    for (i=0;i<MAXITR;i++) {

        /* pseudorange residuals (m) */
        nv=rescode(i,obs,n,rs,dts,vare,svh,nav,x,opt,v,H,var,azel,vsat,resp,
                   &ns);

        if (nv<NX) {
            sprintf(msg,"lack of valid sats ns=%d",nv);
            break;
        }
        /* weighted by Std */
        for (j=0;j<nv;j++) {
            sig=sqrt(var[j]);
            v[j]/=sig;
            for (k = 0; k < NX; k++) {
                H[k + j * NX] /= sig;
            }
        }
        /* least square estimation */
        if ((info=lsq(H,v,NX,nv,dx,Q))) {
            sprintf(msg,"lsq error info=%d",info);
            break;
        }
        for (j=0;j<NX;j++) {
            x[j]+=dx[j];
        }
        if (norm(dx,NX)<1E-4) {
            sol->type=0;
            sol->time=timeadd(obs[0].time,-x[3]/CLIGHT);
            sol->dtr[0]=x[3]/CLIGHT; /* receiver clock bias (s) */
            sol->dtr[1]=x[4]/CLIGHT; /* GLO-GPS  time offset (s) */
            sol->dtr[2]=x[5]/CLIGHT; /* GAL-GPS  time offset (s) */
            sol->dtr[3]=x[6]/CLIGHT; /* BDS3-GPS time offset (s) */
            sol->dtr[4]=x[7]/CLIGHT; /* IRN-GPS  time offset (s) */
            sol->dtr[5]=x[8]/CLIGHT; /* QZS-GPS  time offset (s) */
            sol->dtr[6]=x[9]/CLIGHT; /* BDS2-GPS time offset (s) */
            for (j = 0; j < 6; j++) {
                sol->rr[j] = j < 3 ? x[j] : 0.0;
            }
            for (j = 0; j < 3; j++) {
                sol->qr[j] = (float)Q[j + j * NX];
            }
            sol->qr[3]=(float)Q[1];    /* cov xy */
            sol->qr[4]=(float)Q[2+NX]; /* cov yz */
            sol->qr[5]=(float)Q[2];    /* cov zx */
            sol->ns=(uint8_t)ns;
            sol->age=sol->ratio=0.0;

            /* validate solution */
            if (!opt->posopt[4]||(stat=valsol(azel,vsat,n,opt,v,nv,NX,msg))) {
                sol->stat=opt->sateph==EPHOPT_SBAS?SOLQ_SBAS:SOLQ_SINGLE;
            }
            free(v); free(H); free(var);
            return stat;
        }
    }
    if (i >= MAXITR) {
        sprintf(msg, "iteration divergent i=%d", i);
    }

    free(v); free(H); free(var);
    return 0;
}
/* RAIM FDE (failure detection and exclution) -------------------------------*/
static int raim_fde(const obsd_t *obs, int n, const double *rs,
                    const double *dts, const double *vare, const int *svh,
                    const nav_t *nav, const prcopt_t *opt, sol_t *sol,
                    double *azel, int *vsat, double *resp, char *msg)
{
    obsd_t *obs_e;
    sol_t sol_e={{0}};
    char tstr[32],name[16],msg_e[128];
    double *rs_e,*dts_e,*vare_e,*azel_e,*resp_e,rms_e,rms=100.0;
    int i,j,k,nvsat,stat=0,*svh_e,*vsat_e,sat=0;

    trace(NULL,3,"raim_fde: %s n=%2d\n",time_str(obs[0].time,0),n);

    if (!(obs_e = (obsd_t*)malloc(sizeof(obsd_t) * n))) {
        return 0;
    }
    rs_e = mat(6,n); dts_e = mat(2,n); vare_e=mat(1,n); azel_e=zeros(2,n);
    svh_e=imat(1,n); vsat_e=imat(1,n); resp_e=mat(1,n);

    for (i=0;i<n;i++) {

        /* satellite exclution */
        for (j=k=0;j<n;j++) {
            if (j == i) {
                continue;
            }
            obs_e[k]=obs[j];
            matcpy(rs_e +6*k,rs +6*j,6,1);
            matcpy(dts_e+2*k,dts+2*j,2,1);
            vare_e[k]=vare[j];
            svh_e[k++]=svh[j];
        }
        /* estimate receiver position without a satellite */
        if (!estpos(obs_e,n-1,rs_e,dts_e,vare_e,svh_e,nav,opt,&sol_e,azel_e,
                    vsat_e,resp_e,msg_e)) {
            trace(NULL,3,"raim_fde: exsat=%2d (%s)\n",obs[i].sat,msg);
            continue;
        }
        for (j=nvsat=0,rms_e=0.0;j<n-1;j++) {
            if (!vsat_e[j]) {
                continue;
            }
            rms_e+=SQR(resp_e[j]);
            nvsat++;
        }
        if (nvsat<5) {
            trace(NULL,3,"raim_fde: exsat=%2d lack of satellites nvsat=%2d\n",
                  obs[i].sat,nvsat);
            continue;
        }
        rms_e=sqrt(rms_e/nvsat);

        trace(NULL,3,"raim_fde: exsat=%2d rms=%8.3f\n",obs[i].sat,rms_e);

        if (rms_e > rms) {
            continue;
        }

        /* save result */
        for (j=k=0;j<n;j++) {
            if (j == i) {
                continue;
            }
            matcpy(azel+2*j,azel_e+2*k,2,1);
            vsat[j]=vsat_e[k];
            resp[j]=resp_e[k++];
        }
        stat=1;
        *sol=sol_e;
        sat=obs[i].sat;
        rms=rms_e;
        vsat[i]=0;
        strcpy(msg,msg_e);
    }
    if (stat) {
        time2str(obs[0].time,tstr,2); satno2id(sat,name);
        trace(NULL,2,"%s: %s excluded by raim\n",tstr+11,name);
    }
    free(obs_e);
    free(rs_e ); free(dts_e ); free(vare_e); free(azel_e);
    free(svh_e); free(vsat_e); free(resp_e);
    return stat;
}
/* range rate residuals ------------------------------------------------------*/
static int resdop(const obsd_t *obs, int n, const double *rs, const double *dts,
                  const nav_t *nav, const double *rr, const double *x,
                  const double *azel, const int *vsat, double err, double *v,
                  double *H)
{
    double freq,rate,pos[3],E[9],a[3],e[3],vs[3],cosel,sig;
    int i,j,nv=0;

    trace(NULL,3,"resdop  : n=%d\n",n);

    ecef2pos(rr,pos); xyz2enu(pos,E);

    for (i=0;i<n&&i<MAXOBS;i++) {

        freq=sat2freq(obs[i].sat,obs[i].code[0],nav);

        if (obs[i].D[0]==0.0||freq==0.0||!vsat[i]||norm(rs+3+i*6,3)<=0.0) {
            continue;
        }
        /* LOS (line-of-sight) vector in ECEF */
        cosel=cos(azel[1+i*2]);
        a[0]=sin(azel[i*2])*cosel;
        a[1]=cos(azel[i*2])*cosel;
        a[2]=sin(azel[1+i*2]);
        matmul("TN",3,1,3,1.0,E,a,0.0,e);

        /* satellite velocity relative to receiver in ECEF */
        for (j=0;j<3;j++) {
            vs[j]=rs[j+3+i*6]-x[j];
        }
        /* range rate with earth rotation correction */
        rate=dot(vs,e,3)+OMGE/CLIGHT*(rs[4+i*6]*rr[0]+rs[1+i*6]*x[0]-
                                      rs[3+i*6]*rr[1]-rs[  i*6]*x[1]);

        /* Std of range rate error (m/s) */
        sig=(err<=0.0)?1.0:err*CLIGHT/freq;

        /* range rate residual (m/s) */
        v[nv]=(-obs[i].D[0]*CLIGHT/freq-(rate+x[3]-CLIGHT*dts[1+i*2]))/sig;

        /* design matrix */
        for (j=0;j<4;j++) {
            H[j+nv*4]=((j<3)?-e[j]:1.0)/sig;
        }
        nv++;
    }
    return nv;
}
/* estimate receiver velocity ------------------------------------------------*/
static void estvel(const obsd_t *obs, int n, const double *rs, const double *dts,
                   const nav_t *nav, const prcopt_t *opt, sol_t *sol,
                   const double *azel, const int *vsat)
{
    double x[4]={0},dx[4],Q[16],*v,*H;
    double err=opt->err[4]; /* Doppler error (Hz) */
    int i,j,nv;

    trace(NULL,3,"estvel  : n=%d\n",n);

    v=mat(n,1); H=mat(4,n);

    for (i=0;i<MAXITR;i++) {

        /* range rate residuals (m/s) */
        if ((nv=resdop(obs,n,rs,dts,nav,sol->rr,x,azel,vsat,err,v,H))<4) {
            break;
        }
        /* least square estimation */
        if (lsq(H, v, 4, nv, dx, Q)) {
            break;
        }

        for (j = 0; j < 4; j++) {
            x[j] += dx[j];
        }

        if (norm(dx,4)<1E-6) {
            matcpy(sol->rr+3,x,3,1);
            sol->qv[0]=(float)Q[0];  /* xx */
            sol->qv[1]=(float)Q[5];  /* yy */
            sol->qv[2]=(float)Q[10]; /* zz */
            sol->qv[3]=(float)Q[1];  /* xy */
            sol->qv[4]=(float)Q[6];  /* yz */
            sol->qv[5]=(float)Q[2];  /* zx */
            break;
        }
    }
    free(v); free(H);
}
/* single-point positioning ----------------------------------------------------
* compute receiver position, velocity, clock bias by single-point positioning
* with pseudorange and doppler observables
* args   : mrtk_ctx_t *ctx  I   MRTKLIB context (for trace logging)
*          obsd_t *obs      I   observation data
*          int    n         I   number of observation data
*          nav_t  *nav      I   navigation data
*          prcopt_t *opt    I   processing options
*          sol_t  *sol      IO  solution
*          double *azel     IO  azimuth/elevation angle (rad) (NULL: no output)
*          ssat_t *ssat     IO  satellite status              (NULL: no output)
*          char   *msg      O   error message for error exit
* return : status(1:ok,0:error)
*-----------------------------------------------------------------------------*/
int pntpos(mrtk_ctx_t *ctx, const obsd_t *obs, int n, const nav_t *nav,
                  const prcopt_t *opt, sol_t *sol, double *azel, ssat_t *ssat,
                  char *msg)
{
    prcopt_t opt_=*opt;
    double *rs,*dts,*var,*azel_,*resp;
    int i,stat,vsat[MAXOBS]={0},svh[MAXOBS];

    trace(ctx,3,"pntpos  : tobs=%s n=%d\n",time_str(obs[0].time,3),n);

    sol->stat=SOLQ_NONE;

    if (n<=0) {
        strcpy(msg,"no observation data");
        return 0;
    }
    sol->time=obs[0].time;
    msg[0]='\0';

    rs=mat(6,n); dts=mat(2,n); var=mat(1,n); azel_=zeros(2,n); resp=mat(1,n);

    if (opt_.mode!=PMODE_SINGLE) { /* for precise positioning */
        if (opt_.nf > 1) {
            opt_.ionoopt = IONOOPT_IFLC;
        } else {
            opt_.ionoopt = IONOOPT_BRDC;
        }
        opt_.tropopt=TROPOPT_SAAS;
    }
    /* satellite positons, velocities and clocks */
    satposs(sol->time,obs,n,nav,opt_.sateph,rs,dts,var,svh);

    /* estimate receiver position with pseudorange */
    stat=estpos(obs,n,rs,dts,var,svh,nav,&opt_,sol,azel_,vsat,resp,msg);

    /* RAIM FDE */
    if (!stat&&n>=6&&opt->posopt[4]) {
        stat=raim_fde(obs,n,rs,dts,var,svh,nav,&opt_,sol,azel_,vsat,resp,msg);
    }
    /* estimate receiver velocity with Doppler */
    if (stat) {
        estvel(obs,n,rs,dts,nav,&opt_,sol,azel_,vsat);
    }
    if (azel) {
        for (i = 0; i < n * 2; i++) {
            azel[i] = azel_[i];
        }
    }
    if (ssat) {
        for (i=0;i<MAXSAT;i++) {
            ssat[i].vs=0;
            ssat[i].azel[0]=ssat[i].azel[1]=0.0;
            ssat[i].resp[0]=ssat[i].resc[0]=0.0;
            ssat[i].snr[0]=0;
        }
        for (i=0;i<n;i++) {
            ssat[obs[i].sat-1].azel[0]=azel_[  i*2];
            ssat[obs[i].sat-1].azel[1]=azel_[1+i*2];
            ssat[obs[i].sat-1].snr[0]=obs[i].SNR[0];
            if (!vsat[i]) {
                continue;
            }
            ssat[obs[i].sat-1].vs=1;
            ssat[obs[i].sat-1].resp[0]=resp[i];
        }
    }
    free(rs); free(dts); free(var); free(azel_); free(resp);
    return stat;
}
