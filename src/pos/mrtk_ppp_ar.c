/*------------------------------------------------------------------------------
* mrtk_ppp_ar.c : PPP ambiguity resolution functions
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
* reference :
*    [1] H.Okumura, C-gengo niyoru saishin algorithm jiten (in Japanese),
*        Software Technology, 1991
*    [2] CAO IS-QZSS-MDC-004, May, 2025
*
* version : $Revision:$ $Date:$
* history : 2013/03/11  1.0  new
*           2015/05/15  1.1  refine complete algorithms
*           2015/05/31  1.2  delete WL-ambiguity resolution by ILS
*                            add PAR (partial ambiguity resolution)
*           2015/11/26  1.3  support option opt->pppopt=-TRACE_AR
*           2023/02/01  1.4  branch from MALIB PP for MADOCALIB
*           2023/03/08  1.5  change sys to GPS only
*           2024/01/10  1.6  add references [2]
*                            support MADOCA-PPP ionospheric corrections, add NM
*                            delete L5-receiver-dcb estimation, ND
*                            support option opt->arsys in gen_sat_sd()
*           2024/09/27  1.7  delete NM
*           2025/03/18  1.8  support triple/quad-frequency ppp-ar.
*                            support bds-2 and bds-3.
*-----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_ppp_ar.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_lambda.h"
#include "mrtklib/mrtk_sys.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_madoca.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_const.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "mrtklib/mrtk_trace.h"

/* forward declarations */
extern int satsys(int sat, int *prn);
extern int satsys_bd2(int sat, int *prn);
extern void satno2id(int sat, char *id);
extern int satid2no(const char *id);
extern char *time_str(gtime_t t, int n);
extern void time2str(gtime_t t, char *s, int n);
extern void time2epoch(gtime_t t, double *ep);
extern gtime_t epoch2time(const double *ep);
extern gtime_t timeadd(gtime_t t, double sec);
extern double timediff(gtime_t t1, gtime_t t2);
extern double norm(const double *a, int n);
extern void matcpy(double *A, const double *B, int n, int m);
extern void matmul(const char *tr, int n, int k, int m, double alpha, const double *A, const double *B, double beta, double *C);
extern double *mat(int n, int m);
extern double *zeros(int n, int m);
extern int filter(double *x, double *P, const double *H, const double *v, const double *R, int n, int m);
extern int lambda(int n, int m, const double *a, const double *Q, double *F, double *s);
extern unsigned int tickget(void);
extern int freq_idx2freq_num(int sys, int freq_idx);
extern double freq_num2freq(int sys, int freq_num, int fcn);

#define MIN_AMB_RES       4        /* min number of ambiguities for ILS-AR */
#define CONST_AMB         0.01     /* constraint to fixed ambiguity */
#define CONST_AMB_BDS     0.05     /* constraint to fixed ambiguity for BDS2,BDS3 */
#define CONST_AMB_BDS_B2a 0.10     /* constraint to fixed ambiguity for BDS3 B2a */
#define CONST_AMB_GAL_E6  0.10     /* constraint to fixed ambiguity for GAL E6 */
#define CONST_AMB_EWL     0.10     /* constraint to fixed ewl ambiguity for GPS,BDS */
#define MAX_STD_FIX       0.15     /* max std-dev (3d-pos) to fix solution */
#define MIN_MATCH_RATE    0.10     /* min match rate between the first and second candidates to fix solution */
#define REL_MATCH_RATE    0.90     /* match rate between the first and second candidates to relax threshold */
#define MAX_FRAC_WL_FIX   0.20     /* max fraction to fix wide-lane ambiguity (cycle) */
#define MAX_STD_WL_FIX    1.00     /* max std-dev to fix wide-lane ambiguity (cycle) */

#define STEP_EWL          3
#define STEP_WL           2
#define STEP_NL           1

#define MIN(x,y)    ((x)<(y)?(x):(y))
#define MAX(x,y)    ((x)>(y)?(x):(y))
#define SQR(x)      ((x)*(x))
#define ROUND(x)    (int)floor((x)+0.5)
#define SWAP_I(x,y) do {int    _tmp=x; x=y; y=_tmp;} while (0)
#define SWAP_D(x,y) do {double _tmp=x; x=y; y=_tmp;} while (0)

/* number and index of ekf states */
#define NF(opt)     ((opt)->ionoopt==IONOOPT_IFLC?1:(opt)->nf)
#define NP(opt)     ((opt)->dynamics?9:3)
#define NC(opt)     (NSYS+1) /* BDS3 and BDS2 */
static inline int NT(const prcopt_t* opt) {
    if (opt->tropopt < TROPOPT_EST) return 0;
    if (opt->tropopt == TROPOPT_EST) return 1;
    return 3;
}
#define NI(opt)     ((opt)->ionoopt==IONOOPT_EST?MAXSAT:0)
#define N3(opt)     (NF(opt)>=3?NC(opt):0)   /* 3rd-frequency receivre bias */
#define N4(opt)     (NF(opt)>=4?NC(opt):0)   /* 4th-frequency receivre bias */
#define NR(opt)     (NP(opt)+NC(opt)+NT(opt)+NI(opt)+N3(opt)+N4(opt))
#define IT(opt)     (NP(opt)+NC(opt))
#define IB(s,f,opt) (NR(opt)+MAXSAT*(f)+(s)-1)

/* get wave length -----------------------------------------------------------*/
static void get_wavelength(int sat, double *lam)
{
    double freq;
    int i,sys;

    sys=satsys_bd2(sat,NULL);
    for (i=0;i<NFREQ;i++) {
        freq=freq_num2freq(sys,freq_idx2freq_num(sys,i),0);
        if (freq > 0.0) {
            lam[i] = CLIGHT / freq;
        } else {
            lam[i] = 0.0;
        }
    }
}
/* exclude satellite ---------------------------------------------------------*/
static int exc_sat_sd(rtk_t *rtk, const obsd_t *obs, int exc,
                        double el, int nf, int sys)
{
    double elmask;
    int arsys=rtk->opt.arsys;

    elmask=MAX(rtk->opt.elmaskar,rtk->opt.elmin);

    trace(NULL,4,"exc_sat_sd(%4d): nf=%d arsys=%d elmaskar=%2.0f\n",__LINE__,
        nf,arsys,rtk->opt.elmaskar);

    if (exc) {
        return 0;
    }
    if (!(satsys_bd2(obs->sat, NULL) & sys)) {
        return 0;
    }
    if (!(satsys(obs->sat, NULL) & arsys)) {
        return 0;
    }
    if (!rtk->ssat[obs->sat-1].vsat[0]||el<elmask) {
        trace(NULL,2,"exc_sat_sd(%4d): sys=%2d sat=%3d excluded by el=%4.1f / %4.1f\n",__LINE__,
            satsys_bd2(obs->sat,NULL),obs->sat,el*R2D,elmask*R2D);
        return 0;
    }
    trace(NULL,4,"exc_sat_sd(%4d): sat=%d\n",__LINE__,obs->sat);

    return 1;
}
/* generate satellite SD (single-difference) ---------------------------------*/
static int gen_sat_sd(rtk_t *rtk, const obsd_t *obs, int n, const int *exc,
                      const double *azel, int nf, int *sat1, int *sat2,
                      double *el1, double *el2)
{
    const int sys[]={SYS_GPS,SYS_GAL,SYS_CMP,SYS_QZS,SYS_BD2,0};
    double elmask,el[MAXOBS],maxel;
    int i,j,k,m,f,ns=0,sat[MAXOBS];
    int stat=0,maxsat,excmask[MAXOBS];
    char s1[5],s2[5];

    trace(NULL,3,"gen_sat_sd(%4d): nf=%d arsys=%d elmaskar=%2.0f\n",__LINE__,
        nf,rtk->opt.arsys,rtk->opt.elmaskar*R2D);

    elmask=MAX(rtk->opt.elmaskar,rtk->opt.elmin);

    for (i=0;sys[i];i++) { /* for each system */

        for (j = 0; j < n; j++) {
            excmask[j] = 0;
        }

        /* search highest elevation angle */
        for (j=0,maxsat=0,maxel=0.0;j<n;j++) {

            if (!exc_sat_sd(rtk,obs+j,exc[j],azel[1+j*2],nf,sys[i])) {
                excmask[j]=1;
                continue;
            }
            for (f=0,stat=0;f<2;f++) {
                stat|=(obs[j].L[f]==0.0);
                trace(NULL,4,"gen_sat_sd(%4d): sys=%d sat=%3d f=%d obs[j].L[f]=%14.3f stat=%d\n",__LINE__,
                    sys[i],obs[j].sat,f,obs[j].L[f],stat);
            }
            if (stat) {
                continue;
            }

            /* exclude IGSO for reference satellite */
            if (obs[j].sat == satid2no("C38") || obs[j].sat == satid2no("C39") || obs[j].sat == satid2no("C40")) {
                continue;
            }

            if (azel[1+j*2]>maxel) {
                maxel =azel[1+j*2];
                maxsat=obs[j].sat;
            }
        }
        if (maxsat==0) {
            trace(NULL,4,"gen_sat_sd(%4d): no reference satellite sys=%d maxsat=%3d maxel=%4.1f\n",__LINE__,
                    sys[i],maxsat,maxel*R2D);
            continue;
        }
        trace(NULL,4,"gen_sat_sd(%4d): sys=%d maxsat=%3d maxel=%4.1f\n",__LINE__,
                    sys[i],maxsat,maxel*R2D);

        /* sort by elevation angle */
        for (j=m=0;j<n;j++) {
            if (excmask[j]) {
                continue;
            }
            if (obs[j].sat == maxsat) {
                continue;
            }

            sat[m]=obs[j].sat;
            el[m++]=azel[1+j*2];
        }
        for (j = 0; j < m - 1; j++) {
            for (k = j + 1; k < m; k++) {
                if (el[j] >= el[k]) {
                    continue;
                }
                SWAP_I(sat[j], sat[k]);
                SWAP_D(el[j], el[k]);
            }
        }
        /* generate SD referenced to max elevation angle */
        for (j=0;j<m;j++) {
            el1[ns]=el[j];
            el2[ns]=maxel;

            sat1[ns  ]=sat[j];
            sat2[ns++]=maxsat;

            satno2id(sat[j],s1);
            satno2id(maxsat,s2);
            trace(NULL,3,"gen_sat_sd(%4d): %s %s el=%4.1f %4.1f[deg]\n",
                __LINE__,s1,s2,el[j]*R2D,maxel*R2D);
        }
    }
    trace(NULL,4,"gen_sat_sd(%4d): ns=%d\n",__LINE__,ns);
    return ns; /* # of Satellite */
}
/* write debug trace for PAR -------------------------------------------------*/
static void write_trace_amb(rtk_t *rtk, const double *a, const double *Q, int na,
                            const int *sat1s, const int *sat2s, const int *frqs,
                            const char label[NFREQ][10], int step)
{
    char s1[32],s2[32];
    double ep[6]={0.0};
    gtime_t t0;
    int i;
    trace(NULL,2,"write_trace_amb: EPOCH=%s\n",time_str(rtk->sol.time,0));

    time2epoch(timeadd(rtk->sol.time,1.0),ep);
    ep[3]=0.0;ep[4]=0.0;ep[5]=0.0;
    t0=epoch2time(ep);

    for (i=0;i<na;i++) {
        satno2id(sat1s[i],s1);
        satno2id(sat2s[i],s2);
        trace(NULL,2,"write_trace_amb:, STEP_%d,  %5.0f, %s_%s, %s, %14.3f, %7.3f, %7.3f\n",
            step,timediff(rtk->sol.time,t0),s2,s1,label[frqs[i]],a[i],sqrt(Q[i+i*na]),ROUND(a[i])-a[i]);
    }
}
static void write_trace_sd_amb(rtk_t *rtk, const double *x, const double *P,
                              const int *sat1, const int *sat2, int ns, int nf,
                              int *sat1s, int *sat2s, int *frqs,
                              double *a, int post, int step)
{
    int i,f,j,k,j1,j2,k1,k2,na=0;
    double *D,*Q,*W;
    double lam[NFREQ];
    const char label1[NFREQ][10]={"PRE-N1 ","PRE-WL ","PRE-EWL","PRE-EW2"};
    const char label2[NFREQ][10]={"POST-N1 ","POST-WL ","POST-EWL","POST-EW2"};

    if (!strstr(rtk->opt.pppopt,"-TRACE_AR")) {
        return;
    }
    trace(NULL,4,"write_trace_sd_amb(%4d): ns=%d nf=%d\n",__LINE__,ns,nf);

    D=zeros(rtk->nx,ns*nf);
    Q=mat(ns*NFREQ,ns*nf);

    /* freq-ambiguity SD-matrix */
    for (i=0;i<ns;i++) {
        if (sat1[i] <= 0 || sat2[i] <= 0) {
            continue;
        }
        get_wavelength(sat1[i],lam);

        for (f=0;f<nf;f++) {

            sat1s[na]=sat1[i];
            sat2s[na]=sat2[i];
            frqs [na]=f;

            if (f==0) { /* N1 */
                j=IB(sat1[i],0,&rtk->opt);
                k=IB(sat2[i],0,&rtk->opt);

                if (lam[0] == 0.0) {
                    continue;
                }
                if (x[j] == 0.0 || x[k] == 0.0) {
                    continue;
                }

                D[j+na*rtk->nx]= 1.0/lam[0];
                D[k+na*rtk->nx]=-1.0/lam[0];
                na++;
            }
            else if (f==1) { /* WL (F1-F2) */
                j1=IB(sat1[i],0,&rtk->opt);
                j2=IB(sat1[i],1,&rtk->opt);
                k1=IB(sat2[i],0,&rtk->opt);
                k2=IB(sat2[i],1,&rtk->opt);

                if (lam[0] == 0.0 || lam[1] == 0.0) {
                    continue;
                }
                if (x[j1] == 0.0 || x[j2] == 0.0) {
                    continue;
                }
                if (x[k1] == 0.0 || x[k2] == 0.0) {
                    continue;
                }

                D[j1+na*rtk->nx]= 1.0/lam[0];
                D[j2+na*rtk->nx]=-1.0/lam[1];
                D[k1+na*rtk->nx]=-1.0/lam[0];
                D[k2+na*rtk->nx]= 1.0/lam[1];
                na++;
            }
            else if (f==2) { /* EWL (F2-F3) */
                j1=IB(sat1[i],1,&rtk->opt);
                j2=IB(sat1[i],2,&rtk->opt);
                k1=IB(sat2[i],1,&rtk->opt);
                k2=IB(sat2[i],2,&rtk->opt);

                if (lam[1] == 0.0 || lam[2] == 0.0) {
                    continue;
                }
                if (x[j1] == 0.0 || x[j2] == 0.0) {
                    continue;
                }
                if (x[k1] == 0.0 || x[k2] == 0.0) {
                    continue;
                }

                D[j1+na*rtk->nx]= 1.0/lam[1];
                D[j2+na*rtk->nx]=-1.0/lam[2];
                D[k1+na*rtk->nx]=-1.0/lam[1];
                D[k2+na*rtk->nx]= 1.0/lam[2];
                na++;
            }
            else if (f==3) { /* EWL (F2-F4) */
                j1=IB(sat1[i],1,&rtk->opt);
                j2=IB(sat1[i],3,&rtk->opt);
                k1=IB(sat2[i],1,&rtk->opt);
                k2=IB(sat2[i],3,&rtk->opt);

                if (lam[1] == 0.0 || lam[3] == 0.0) {
                    continue;
                }
                if (x[j1] == 0.0 || x[j2] == 0.0) {
                    continue;
                }
                if (x[k1] == 0.0 || x[k2] == 0.0) {
                    continue;
                }

                D[j1+na*rtk->nx]= 1.0/lam[1];
                D[j2+na*rtk->nx]=-1.0/lam[3];
                D[k1+na*rtk->nx]=-1.0/lam[1];
                D[k2+na*rtk->nx]= 1.0/lam[3];
                na++;
            }
        }
    }

    /* a=D'*x, Q_wl=D'*P*D */
    W=mat(rtk->nx,na);
    matmul("TN",na,1,rtk->nx,1.0,D,x,0.0,a);
    matmul("TN",na,rtk->nx,rtk->nx,1.0,D,P,0.0,W);
    matmul("NN",na,na,rtk->nx,1.0,W,D,0.0,Q);

    if (post) {
        write_trace_amb(rtk, a, Q, na, sat1s, sat2s, frqs, label2, step);
    } else {
        write_trace_amb(rtk, a, Q, na, sat1s, sat2s, frqs, label1, step);
    }

    free(W);
    free(D); free(Q);

}
/* search extra wide-lane integer ambiguity ----------------------------------*/
static int search_amb_ewl(rtk_t *rtk, const double *x, const double *P,
                         const int *sat1, const int *sat2, int ns, int nf,
                         int *sat1s, int *sat2s, int *frqs,
                         double *a, double *N, double *D)
{

    int i,j1,j2,k1,k2,na=0;
    int f,f1,f2;
    double lam[NFREQ];
    double b,n,var,frac;
    double th_frac=MAX_FRAC_WL_FIX,th_std=MAX_STD_WL_FIX;
    char s1[32],s2[32];

    if (nf < 3) {
        return 0;
    }

    /* initialize D-matrix */
    for (i = 0; i < rtk->nx * (ns * 2); i++) {
        D[i] = 0.0;
    }

    /* freq-ambiguity SD-matrix extra WL (F2-F3, F2-F4) */
    for (i=0;i<ns;i++) {
        if (sat1[i] <= 0 || sat2[i] <= 0) {
            continue;
        }
        get_wavelength(sat1[i],lam);
        satno2id(sat1[i],s1);
        satno2id(sat2[i],s2);

        for (f=2;f<rtk->opt.nf;f++) {
            if (f==2) {
                f1=1; /* 2nd frequency */
                f2=2; /* 3rd frequency */
            }
            else if (f==3) {
                f1=1; /* 2nd frequency */
                f2=3; /* 4th frequency */
            }

            j1=IB(sat1[i],f1,&rtk->opt);
            j2=IB(sat1[i],f2,&rtk->opt);
            k1=IB(sat2[i],f1,&rtk->opt);
            k2=IB(sat2[i],f2,&rtk->opt);

            if (lam[f1]==0.0||lam[f2]==0.0||
                x[j1]==0.0||x[j2]==0.0||x[k1]==0.0||x[k2]==0.0) {
                trace(NULL,3,"search_amb_ewl(%4d): skipped %s %s x(j1,j2,k1,k2)=%8.3f %8.3f %8.3f %8.3f\n",__LINE__,
                    s1,s2,x[j1],x[j2],x[k1],x[k2]);
                continue;
            }

            b=(x[j1]/lam[f1]-x[j2]/lam[f2])-(x[k1]/lam[f1]-x[k2]/lam[f2]);
            n=ROUND(b);
            frac=fabs(n-b);
            var= P[j1+j1*rtk->nx]/SQR(lam[f1])+P[j2+j2*rtk->nx]/SQR(lam[f2])-2.0*P[j1+j2*rtk->nx]/(lam[f1]*lam[f2]);
            var+=P[k1+k1*rtk->nx]/SQR(lam[f1])+P[k2+k2*rtk->nx]/SQR(lam[f2])-2.0*P[k1+k2*rtk->nx]/(lam[f1]*lam[f2]);

            if (sqrt(var)>th_std || frac>th_frac) {
                trace(NULL,2,"search_amb_ewl(%4d): excluded %s %s N,a= %8.3f %8.3f "
                        "sig=%8.3f frac=%8.3f/%8.3f std=%8.3f/%8.3f\n",__LINE__,
                    s1,s2,n,b,sqrt(var),frac,th_frac,sqrt(var),th_std);
                continue;
            }

            sat1s[na]=sat1[i];
            sat2s[na]=sat2[i];
            frqs [na]=f;

            a[na]=b;
            N[na]=n;
            D[j1+na*rtk->nx]= 1.0/lam[f1];
            D[j2+na*rtk->nx]=-1.0/lam[f2];
            D[k1+na*rtk->nx]=-1.0/lam[f1];
            D[k2+na*rtk->nx]= 1.0/lam[f2];
            rtk->ssat[sat1s[na]-1].fix[f2]=rtk->ssat[sat2s[na]-1].fix[f2]=1;
            na++;

            if (strstr(rtk->opt.pppopt,"-TRACE_AR")) {
                trace(NULL,3,"search_amb_ewl(%4d): %s %s N,a= %8.3f %8.3f sig=%8.3f\n",__LINE__,
                    s1,s2,n,b,sqrt(var));
            }
        }
    }
    trace(NULL,4,"search_amb_ewl(%4d): number of fixed extra wide-lane ambiguity = %d / %d\n",
        __LINE__,na,ns);
    return na;
}
/* search wide-lane integer ambiguity ----------------------------------------*/
static int search_amb_wl(rtk_t *rtk, const double *x, const double *P,
                         int *sat1, int *sat2, int ns, int nf,
                         int *sat1s, int *sat2s, int *frqs,
                         double *a, double *N, double *D)
{

    int i,j1,j2,k1,k2,na=0;
    double lam[NFREQ];
    double b,n,var,frac;
    double th_frac=MAX_FRAC_WL_FIX,th_std=MAX_STD_WL_FIX;
    char s1[32],s2[32];

    if (nf < 2) {
        return 0;
    }

    /* initialize D-matrix */
    for (i = 0; i < rtk->nx * ns; i++) {
        D[i] = 0.0;
    }

    /* freq-ambiguity SD-matrix WL (L1-L2) */
    for (i=0;i<ns;i++) {
        if (sat1[i] <= 0 || sat2[i] <= 0) {
            continue;
        }
        get_wavelength(sat1[i],lam);
        satno2id(sat1[i],s1);
        satno2id(sat2[i],s2);

        j1=IB(sat1[i],0,&rtk->opt);
        j2=IB(sat1[i],1,&rtk->opt);
        k1=IB(sat2[i],0,&rtk->opt);
        k2=IB(sat2[i],1,&rtk->opt);

        if (lam[0]==0.0||lam[1]==0.0||
            x[j1]==0.0||x[j2]==0.0||x[k1]==0.0||x[k2]==0.0) {
            trace(NULL,3,"search_amb_wl(%4d): excluded %s %s x(j1,j2,k1,k2)=%.3f %.3f %.3f %.3f\n",__LINE__,
                s1,s2,x[j1],x[j2],x[k1],x[k2]);

            sat1[i]=0;
            sat2[i]=0;
            continue;
        }

        b=(x[j1]/lam[0]-x[j2]/lam[1])-(x[k1]/lam[0]-x[k2]/lam[1]);
        n=ROUND(b);
        frac=fabs(n-b);
        var= P[j1+j1*rtk->nx]/SQR(lam[0])+P[j2+j2*rtk->nx]/SQR(lam[1])-2.0*P[j1+j2*rtk->nx]/(lam[0]*lam[1]);
        var+=P[k1+k1*rtk->nx]/SQR(lam[0])+P[k2+k2*rtk->nx]/SQR(lam[1])-2.0*P[k1+k2*rtk->nx]/(lam[0]*lam[1]);

        if (sqrt(var)>th_std || frac>th_frac) {
            trace(NULL,2,"search_amb_wl(%4d): excluded %s %s N,a= %8.3f %8.3f "
                    "sig=%8.3f frac=%8.3f/%8.3f std=%8.3f/%8.3f\n",__LINE__,
                s1,s2,n,b,sqrt(var),frac,th_frac,sqrt(var),th_std);

            sat1[i]=0;
            sat2[i]=0;

            continue;
        }

        sat1s[na]=sat1[i];
        sat2s[na]=sat2[i];
        frqs [na]=1;

        a[na]=b;
        N[na]=n;
        D[j1+na*rtk->nx]= 1.0/lam[0];
        D[j2+na*rtk->nx]=-1.0/lam[1];
        D[k1+na*rtk->nx]=-1.0/lam[0];
        D[k2+na*rtk->nx]= 1.0/lam[1];
        rtk->ssat[sat1s[na]-1].fix[1]=rtk->ssat[sat2s[na]-1].fix[1]=1;
        na++;

        if (strstr(rtk->opt.pppopt,"-TRACE_AR")) {
            trace(NULL,3,"search_amb_wl(%4d): %s %s N,a= %8.3f %8.3f sig=%8.3f\n",__LINE__,
                s1,s2,n,b,sqrt(var));
        }
    }
    trace(NULL,4,"search_amb_wl(%4d): number of fixed wide-lane ambiguity = %d / %d\n",
        __LINE__,na,ns);
    return na;
}
/* generate SD-matrix of narrow-lane ambiguity --------------------------------*/
static int gen_sd_matrix_n1(rtk_t *rtk, const double *x, const double *P,
                            const int *sat1, const int *sat2, int ns, int nf,
                            int *sat1s, int *sat2s, int *frqs,
                            double *a, double *Q, double *D)
{
    int i,j,k,na=0,ind[MAXOBS][2],i0,i1,j0,j1;
    double q,lam[NFREQ];
    char s1[32],s2[32];
    const char label[NFREQ][10]={"N1 ","WL ","EWL","EW2"};
    unsigned int tick=tickget();

    /* initialize D-matrix */
    for (i = 0; i < rtk->nx * ns; i++) {
        D[i] = 0.0;
    }

    /* freq-ambiguity SD-matrix N1 */
    for (i=0;i<ns;i++) {
        if (sat1[i] <= 0 || sat2[i] <= 0) {
            continue;
        }
        get_wavelength(sat1[i],lam);
        satno2id(sat1[i],s1);
        satno2id(sat2[i],s2);

        sat1s[na]=sat1[i];
        sat2s[na]=sat2[i];
        frqs [na]=0;

        j=IB(sat1[i],0,&rtk->opt);
        k=IB(sat2[i],0,&rtk->opt);

        if (lam[0] == 0.0) {
            continue;
        }
        if (x[j] == 0.0 || x[k] == 0.0) {
            continue;
        }

        D[j+na*rtk->nx]= 1.0/lam[0];
        D[k+na*rtk->nx]=-1.0/lam[0];
        ind[na][0]=j;
        ind[na][1]=k;
        na++;
    }
    trace(NULL,3,"gen_sd_matrix_n1(%4d): ns=%d proc_time=%d [ms]\n",__LINE__,
        ns,(int)(tickget()-tick));

    /* a=D'*x, Q=D'*P*D */
    for (j=0;j<na;j++) {
        j0=ind[j][0]; j1=ind[j][1];
        a[j]=D[j0+j*rtk->nx]*x[j0]+D[j1+j*rtk->nx]*x[j1];

        for (i=j;i<na;i++) {
            i0=ind[i][0]; i1=ind[i][1];
            q= D[i0+i*rtk->nx]*P[i0+j0*rtk->nx]*D[j0+j*rtk->nx];
            q+=D[i0+i*rtk->nx]*P[i0+j1*rtk->nx]*D[j1+j*rtk->nx];
            q+=D[i1+i*rtk->nx]*P[i1+j0*rtk->nx]*D[j0+j*rtk->nx];
            q+=D[i1+i*rtk->nx]*P[i1+j1*rtk->nx]*D[j1+j*rtk->nx];
            Q[i+j*na]=Q[j+i*na]=q;
        }
    }
    if (strstr(rtk->opt.pppopt,"-TRACE_AR")) {
        write_trace_amb(rtk,a,Q,na,sat1s,sat2s,frqs,label,8);
    }
    trace(NULL,3,"gen_sd_matrix_n1(%4d): na=%d proc_time=%d [ms]\n",__LINE__,
        na,(int)(tickget()-tick));
    return na;
}
/* search integer ambiguity by LAMBDA ----------------------------------------*/
static int search_amb_lambda(rtk_t *rtk, const obsd_t *obs, int n,
                           const double *azel,
                           const int *sat1s, const int *sat2s, const int *frq,
                           const double *x, const double *P,
                           const double *D, const double *Q, const double *a,
                           double *N, int na, int *exc_sat_flag)
{
    const double thres_fact[]={5.0,5.0,3.0,2.0,1.5};
    double s[2],thres=0.0;
    int i,trace_AR=0,nm=0;
    char s1[32],s2[32],tstr[30];
    unsigned int tick=tickget();
    trace(NULL,3,"search_amb_lambda(%4d): na=%d\n",__LINE__,na);

    if (strstr(rtk->opt.pppopt, "-TRACE_AR")) {
        trace_AR = 1;
    }

    time2str(rtk->sol.time,tstr,0);

    /* integer least-square (a->N) */
    if (lambda(na,2,a,Q,N,s)||s[0]<=0.0) {
        trace(NULL,2,"search_amb_lambda(%4d): error lambda\n",__LINE__);
        return 0;
    }

    /* threshold for ratio test */
    thres=rtk->opt.thresar[0];
    if (na-MIN_AMB_RES<(sizeof(thres_fact)/sizeof(thres_fact[0]))) {
        thres*=thres_fact[na-MIN_AMB_RES];
        trace(NULL,2,"search_amb_lambda(%4d): inflated threshold na-MIN_AMB_RES=%d thres_fact=%.2f thres=%.2f\n",__LINE__,
            na-MIN_AMB_RES,thres_fact[na-MIN_AMB_RES],thres);
    }
    for (i = 0; i < MAXSAT; i++) {
        exc_sat_flag[i] = 0;
    }
    for (i=0;i<na;i++) {
        if (fabs(N[i] - N[i + na]) < 0.001) {
            nm++;
        } else {
            exc_sat_flag[sat1s[i] - 1] = (int)fabs(N[i] - N[i + na]);
        }
    }
    trace(NULL,3,"search_amb_lambda(%4d): %s na=%2d nm= %2d match_rate=%7.2f\n",__LINE__,
        tstr,na,nm,(double)nm/(double)na);
    if ((double)nm / (double)na > REL_MATCH_RATE) {
        thres *= 0.8;
    }
    if ((double)nm / (double)na < MIN_MATCH_RATE) {
        thres = 99.99;
    }

    if (trace_AR) {
        trace(NULL,2,"search_amb_lambda(%4d): na= %2d ratio= %.3f (s[1](%.3f)/s[0](%.3f)) thres= %.3f\n",__LINE__,
            na,s[1]/s[0],s[1],s[0],thres);
        trace(NULL,2,"search_amb_lambda(%4d): first candidate\n",__LINE__);
        for (i=0;i<na;i++) {
            satno2id(sat1s[i],s1);
            satno2id(sat2s[i],s2);
            trace(NULL,2,"search_amb_lambda(%4d): i= %2d na= %2d %s %s N,a= %8.3f %8.3f frac=%8.3f\n",__LINE__,
                i,na,s1,s2,N[i],a[i],N[i]-a[i]);
        }
        trace(NULL,2,"search_amb_lambda(%4d): second candidate\n",__LINE__);
        for (i=0;i<na;i++) {
            satno2id(sat1s[i],s1);
            satno2id(sat2s[i],s2);
            trace(NULL,2,"search_amb_lambda(%4d): i= %2d na= %2d %s %s N,a= %8.3f %8.3f frac=%8.3f\n",__LINE__,
                i,na,s1,s2,N[i+na],a[i],N[i+na]-a[i]);
        }
        trace(NULL,2,"search_amb_lambda(%4d): difference\n",__LINE__);
        for (i=0;i<na;i++) {
            satno2id(sat1s[i],s1);
            satno2id(sat2s[i],s2);
            trace(NULL,2,"search_amb_lambda(%4d): i= %2d na= %2d %s %s N1,N2= %8.3f %8.3f diff=%8.3f\n",__LINE__,
                i,na,s1,s2,N[i],N[i+na],N[i]-N[i+na]);
        }
    }

    rtk->sol.ratio=(float)MIN(s[1]/s[0],999.9);
    if (s[1]/s[0]<thres) {
        trace(NULL,2,"search_amb_lambda(%4d): failed to fix. na=%d ratio=%.2f/%.2f proc_time=%d [ms]\n",__LINE__,
            na,s[1]/s[0],thres,(int)(tickget()-tick));
        return 0;
    }

    /* update fix flags */
    for (i=0;i<na;i++) {
        rtk->ssat[sat1s[i]-1].fix[frq[i]]=rtk->ssat[sat2s[i]-1].fix[frq[i]]=2;
    }

    trace(NULL,2,"search_amb_lambda(%4d): successed to fix. na=%d ratio=%.2f/%.2f proc_time=%d [ms]\n",__LINE__,
            na,s[1]/s[0],thres,(int)(tickget()-tick));
    return na;
}
/* update states with integer ambiguity constraints --------------------------*/
static int update_states(double *x, double *P, const double *D, int nx,
                         const double *N, const double *a, int na,
                         const int *sats, const int *frqs, int step)
{
    double *R,b[MAXOBS*NFREQ*2];
    double var;
    int i,info,sys;
    unsigned int tick=tickget();

    R=zeros(na,na);
    for (i=0;i<na;i++) {
        var=SQR(CONST_AMB);

        /* de-weight for a specific system or frequency */
        sys=satsys_bd2(sats[i],NULL);
        if (sys&(SYS_BD2)) {
            var=SQR(CONST_AMB_BDS);
        }
        if (sys&(SYS_CMP)) {
            var=SQR(CONST_AMB_BDS);
            if (freq_idx2freq_num(SYS_CMP,frqs[i])==5) { /* B2a */
                var=SQR(CONST_AMB_BDS_B2a);
            }
        }
        if (step==STEP_EWL) {
            switch(sys){
                case SYS_GPS : var=SQR(CONST_AMB_EWL); break;
                case SYS_QZS : break;
                case SYS_CMP : var=SQR(CONST_AMB_EWL); break;
                case SYS_BD2 : var=SQR(CONST_AMB_EWL); break;
                case SYS_GAL :
                    if (freq_idx2freq_num(SYS_GAL,frqs[i])==6) { /* E6 */
                        var=SQR(CONST_AMB_GAL_E6);
                    }
                    break;
            }
        }
        b[i]=N[i]-a[i];
        R[i+i*na]=var;
        trace(NULL,4,"update_states(%4d): sat=%3d frqs=%d fn=%d i=%2d na=%2d N,a=%10.3f %10.3f frac=%8.3f sig=%6.3f\n",__LINE__,
            sats[i],frqs[i],freq_idx2freq_num(sys,frqs[i]),i,na,N[i],a[i],b[i],sqrt(var));
    }
    if ((info=filter(x,P,D,b,R,nx,na))) {
        trace(NULL,1,"update_states(%4d): filter error (info=%d)\n",__LINE__,info);
    }
    free(R);

    trace(NULL,3,"update_states(%4d): na=%d proc_time=%d [ms]\n",__LINE__,
        na,(int)(tickget()-tick));
    return info;
}
/* exclude satellite for partial AR ------------------------------------------*/
static int exc_sat_par(rtk_t *rtk, int *sat1, int *sat2,
                       const double *el1, const double *el2,
                       int ns, const int *exc_sat_flag)
{
    int i,minidx=0,sys;
    double minel=9999.9;
    double el,offset=PI/2.0;
    char s1[32],s2[32];

    for (i=0;i<ns;i++) {
        if (!sat1[i]) {
            continue;
        }
        if (!exc_sat_flag[sat1[i] - 1]) {
            continue;
        }
        el=el1[i];

        /* exclude IGSO satellites first */
        sys=satsys_bd2(sat1[i],NULL);
        if (sys&SYS_BD2) {
            el-=offset;
        }
        else if (sys&SYS_CMP) {
            if (sat1[i] == satid2no("C38") || sat1[i] == satid2no("C39") || sat1[i] == satid2no("C40")) {
                el -= offset;
            }
        }
        else if (sys&SYS_QZS) {
            el-=offset;
        }

        if (el<minel) {
            minidx=i;
            minel=el;
        }
        trace(NULL,4,"exc_sat_par(%4d): i=%2d sat1=%3d sat2=%3d el=%4.1f %4.1f minidx=%2d minel=%.3f\n",
            __LINE__,i,sat1[i],sat2[i],el1[i]*R2D,el2[i]*R2D,minidx,minel*R2D);
    }
    satno2id(sat1[minidx],s1);
    satno2id(sat2[minidx],s2);
    trace(NULL,2,"exc_sat_par(%4d): excluded %s %s sat1=%3d sat2=%3d el=%4.1f %4.1f\n",
        __LINE__,s1,s2,sat1[minidx],sat2[minidx],el1[minidx]*R2D,el2[minidx]*R2D);
    sat1[minidx]=0;
    sat2[minidx]=0;

    for (i=0;i<ns;i++) {
        trace(NULL,4,"exc_sat_par(%4d): i=%2d sat1=%3d sat2=%3d el1=%4.1f\n",
            __LINE__,i,sat1[i],sat2[i],el1[i]*R2D);
    }

    return ns-1;
}
/* ambiguity resolution by ILS (integer-least-square) ------------------------*/
static int ppp_amb_ILS(rtk_t *rtk, const obsd_t *obs, int n, int *exc,
                       const nav_t *nav, const double *azel, double *x,
                       double *P)
{
    double *D,*Q,a[MAXOBS*NFREQ],N[MAXOBS*NFREQ*2],*xp,*Pp;
    double el1[MAXOBS],el2[MAXOBS],std[3];
    int sat1[MAXOBS],sat2[MAXOBS],exc_sat_flag[MAXSAT];
    int sat1s[MAXOBS*NFREQ],sat2s[MAXOBS*NFREQ],frqs[MAXOBS*NFREQ];
    int i,ns,na=0,info=0;
    int nf=rtk->opt.nf,sol_fix_wl=1;
    char tstr[30];
    unsigned int tick=tickget();

    if (strstr(rtk->opt.pppopt,"-AR_NOFIX_WL")) {
        sol_fix_wl=0;
    }

    time2str(rtk->sol.time,tstr,0);
    trace(NULL,3,"ppp_amb_ILS(%4d): nf=%d\n",__LINE__,nf);

    /* generate satellite SD */
    ns=gen_sat_sd(rtk,obs,n,exc,azel,nf,sat1,sat2,el1,el2);
    if (ns<=0) {
        trace(NULL,2,"ppp_amb_ILS(%4d): no generated satellite SD ns= %d\n",__LINE__,ns);
        return 0;
    }
    trace(NULL,3,"ppp_amb_ILS(%4d): %s generate satellite SD ns= %d\n",__LINE__,tstr,ns);

    Pp=mat(rtk->nx,rtk->nx);
    xp=mat(rtk->nx,1);
    D=zeros(rtk->nx,n*nf);
    Q=mat(ns*nf,ns*nf);

    write_trace_sd_amb(rtk,x,P,sat1,sat2,ns,nf,sat1s,sat2s,frqs,a,0,STEP_EWL);

    /* extra wide-lane */
    na=search_amb_ewl(rtk,x,P,sat1,sat2,ns,nf,sat1s,sat2s,frqs,a,N,D);
    info=update_states(x,P,D,rtk->nx,N,a,na,sat1s,frqs,STEP_EWL);
    if (info) {
        trace(NULL,1,"ppp_amb_ILS(%4d): %s update error\n",__LINE__,tstr);
        free(D); free(Q); free(xp); free(Pp);
        return 0;
    }
    trace(NULL,3,"ppp_amb_ILS(%4d): %s extra wide-lane ambiguity fixed. nfix=%2d nsat=%2d\n",__LINE__,
        tstr,na,ns);

    write_trace_sd_amb(rtk,x,P,sat1,sat2,ns,nf,sat1s,sat2s,frqs,a,0,STEP_WL);

    /* wide-lane */
    na=search_amb_wl(rtk,x,P,sat1,sat2,ns,nf,sat1s,sat2s,frqs,a,N,D);
    if (na==0) {
        trace(NULL,2,"ppp_amb_ILS(%4d): %s wide-lane ambiguity search error\n",__LINE__,tstr);
        rtk->sol.age=0;
        free(D); free(Q); free(xp); free(Pp);
        return 0;
    }
    info=update_states(x,P,D,rtk->nx,N,a,na,sat1s,frqs,STEP_WL);
    if (info) {
        trace(NULL,1,"ppp_amb_ILS(%4d): %s update error\n",__LINE__,tstr);
        free(D); free(Q); free(xp); free(Pp);
        return 0;
    }
    trace(NULL,3,"ppp_amb_ILS(%4d): %s wide-lane ambiguity fixed. nfix=%2d nsat=%2d proc_time=%d [ms]\n",__LINE__,
        tstr,na,ns,(int)(tickget()-tick));

    matcpy(Pp,P,rtk->nx,rtk->nx);
    matcpy(xp,x,rtk->nx,1);
    write_trace_sd_amb(rtk,x,P,sat1,sat2,ns,nf,sat1s,sat2s,frqs,a,0,STEP_NL);

    /* narrow-lane */
    for (i = 0; i < 3; i++) {
        std[i] = sqrt(P[i + i * rtk->nx]);
    }
    if (norm(std,3)<rtk->opt.thresar[1]) {
        trace(NULL,3,"ppp_amb_ILS(%4d): %s narrow-lane search. nsat=%d std=%.2f\n",__LINE__,
            tstr,ns,norm(std,3));

        for (i=0;i<rtk->opt.armaxiter;i++) {
            na=gen_sd_matrix_n1(rtk,x,P,sat1,sat2,ns,nf,sat1s,sat2s,frqs,a,Q,D);
            if (na<MIN_AMB_RES) {
                trace(NULL,2,"ppp_amb_ILS(%4d): %s no integer ambiguity candidate. na=%d/%d\n",__LINE__,
                    tstr,na,MIN_AMB_RES);
                na=0;
                break;
            }

            na=search_amb_lambda(rtk,obs,n,azel,sat1s,sat2s,frqs,x,P,D,Q,a,N,na,exc_sat_flag);
            if (na>0) {
                break;
            }

            /* exclude satellite for partial AR */
            exc_sat_par(rtk,sat1,sat2,el1,el2,ns,exc_sat_flag);
        }
    }
    else {
        na=0; i=0;
        trace(NULL,2,"ppp_amb_ILS(%4d): %s std is greater than the threshold. std=%.2f / %.2f\n",__LINE__,
            tstr,norm(std,3),rtk->opt.thresar[1]);
    }
    if (na==0) {
        rtk->sol.age=(float)i;
        free(D); free(Q); free(xp); free(Pp);
        trace(NULL,2,"ppp_amb_ILS(%4d): %s narrow-lane search error. iter=%d proc_time=%d [ms]\n",__LINE__,
            tstr,(int)rtk->sol.age,(int)(tickget()-tick));
        return sol_fix_wl; /* wide-lane fixed */
    }
    info=update_states(x,P,D,rtk->nx,N,a,na,sat1s,frqs,STEP_NL);
    rtk->sol.age=(float)i;

    /* test by standard deviation of position */
    for (i = 0; i < 3; i++) {
        std[i] = sqrt(P[i + i * rtk->nx]);
    }
    if (norm(std,3)>MAX_STD_FIX) {
        /* reset fix flag */
        for (i=0;i<na;i++) {
            rtk->ssat[sat1s[i]-1].fix[0]=rtk->ssat[sat2s[i]-1].fix[0]=0;
        }
        matcpy(P,Pp,rtk->nx,rtk->nx);
        matcpy(x,xp,rtk->nx,1);
        free(D); free(Q); free(xp); free(Pp);
        trace(NULL,2,"ppp_amb_ILS(%4d): %s failed to std test. std=%.3f / %.3f iter=%d proc_time=%d [ms]\n",__LINE__,
            tstr,norm(std,3),MAX_STD_FIX,(int)rtk->sol.age,(int)(tickget()-tick));
        return sol_fix_wl; /* wide-lane fixed */
    }

    write_trace_sd_amb(rtk,x,P,sat1,sat2,ns,nf,sat1s,sat2s,frqs,a,1,9);
    trace(NULL,3,"ppp_amb_ILS(%4d): %s ambiguity fixed. nfix=%2d nsat=%2d std=%.3f iter=%d proc_time=%d [ms]\n",__LINE__,
        tstr,na,ns,norm(std,3),(int)rtk->sol.age,(int)(tickget()-tick));

    free(D); free(Q); free(xp); free(Pp);
    return info?sol_fix_wl:2; /* narrow-lane fixed */
}
/* ambiguity resolution in ppp -----------------------------------------------*/
extern int ppp_ar(mrtk_ctx_t *ctx, rtk_t *rtk, const obsd_t *obs, int n, int *exc,
                  const nav_t *nav, const double *azel, double *x, double *P)
{
    if (n <= 0 || rtk->opt.modear < ARMODE_CONT) {
        return 0;
    }

    if (rtk->opt.ionoopt != IONOOPT_EST) {
        return 0;
    }
    return ppp_amb_ILS(rtk,obs,n,exc,nav,azel,x,P);
}
