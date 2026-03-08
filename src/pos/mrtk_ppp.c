/*------------------------------------------------------------------------------
* mrtk_ppp.c : precise point positioning functions
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
* options : -DIERS_MODEL  use IERS tide model
*           -DOUTSTAT_AMB output ambiguity parameters to solution status
*
* references :
*    [1] D.D.McCarthy, IERS Technical Note 21, IERS Conventions 1996, July 1996
*    [2] D.D.McCarthy and G.Petit, IERS Technical Note 32, IERS Conventions
*        2003, November 2003
*    [3] D.A.Vallado, Fundamentals of Astrodynamics and Applications 2nd ed,
*        Space Technology Library, 2004
*    [4] J.Kouba, A Guide to using International GNSS Service (IGS) products,
*        May 2009
*    [5] RTCM Paper, April 12, 2010, Proposed SSR Messages for SV Orbit Clock,
*        Code Biases, URA
*    [6] MacMillan et al., Atmospheric gradients and the VLBI terrestrial and
*        celestial reference frames, Geophys. Res. Let., 1997
*    [7] G.Petit and B.Luzum (eds), IERS Technical Note No. 36, IERS
*         Conventions (2010), 2010
*    [8] J.Kouba, A simplified yaw-attitude model for eclipsing GPS satellites,
*        GPS Solutions, 13:1-12, 2009
*    [9] F.Dilssner, GPS IIF-1 satellite antenna phase center and attitude
*        modeling, InsideGNSS, September, 2010
*    [10] F.Dilssner, The GLONASS-M satellite yaw-attitude model, Advances in
*        Space Research, 2010
*    [11] IGS MGEX (http://igs.org/mgex)
*    [12] CAO IS-QZSS-MDC-004, TBD, 2025
*
* version : $Revision:$ $Date:$
* history : 2010/07/20 1.0  new
*                           added api:
*                               tidedisp()
*           2010/12/11 1.1  enable exclusion of eclipsing satellite
*           2012/02/01 1.2  add gps-glonass h/w bias correction
*                           move windupcorr() to rtkcmn.c
*           2013/03/11 1.3  add otl and pole tides corrections
*                           involve iers model with -DIERS_MODEL
*                           change initial variances
*                           suppress acos domain error
*           2013/09/01 1.4  pole tide model by iers 2010
*                           add mode of ionosphere model off
*           2014/05/23 1.5  add output of trop gradient in solution status
*           2014/10/13 1.6  fix bug on P0(a[3]) computation in tide_oload()
*                           fix bug on m2 computation in tide_pole()
*           2015/03/19 1.7  fix bug on ionosphere correction for GLO and BDS
*           2015/05/10 1.8  add function to detect slip by MW-LC jump
*                           fix ppp solutin problem with large clock variance
*           2015/06/08 1.9  add precise satellite yaw-models
*                           cope with day-boundary problem of satellite clock
*           2015/07/31 1.10 fix bug on nan-solution without glonass nav-data
*                           pppoutsolsat() -> pppoutstat()
*           2015/11/13 1.11 add L5-receiver-dcb estimation
*                           merge post-residual validation by rnx2rtkp_test
*                           support support option opt->pppopt=-GAP_RESION=nnnn
*           2016/01/22 1.12 delete support for yaw-model bug
*                           add support for ura of ephemeris
*           2018/10/10 1.13 support api change of satexclude()
*           2020/11/30 1.14 use sat2freq() to get carrier frequency
*                           use E1-E5b for Galileo iono-free LC
*           2023/02/01 1.15 branch from ver.2.4.3b34 for MADOCALIB
*           2023/03/08 1.16 change for suppress -Wstringop-overflow warnings.
*           2024/01/10 1.17 support MADOCA-PPP ionospheric corrections, add
*                               udbsys_ppp(), NM, IM
*                           change udstate_ppp(), ppp_res()
*                           delete L5-receiver-dcb estimation, uddcb_ppp(), ND, ID
*                           fix bug on L1 iono computation in udbias_ppp()
*                           considered signal selection for PCO and PCV, add
*                               signal_sel_pco(), signal_sel_pcv()
*           2024/03/15 1.18 change the value of $ION from vertical to slant.
*           2024/05/24 1.19 estimate receiver clock of QZS.
*                           delete processing regarding MADOCA-PPP ionospheric
*                           corrections system bias.
*                           delete udbsys_ppp().
*           2024/06/06 1.20 coonsider URA ratio in the calculation of the
*                           variance of phase and code residuals.
*           2024/07/23 1.21 support cycle slip detection by ssr phase bias
*                           discontinuity indicator.
*                           apply phase bias correction only when performing
*                           ambiguity resolution.
*                           change the sign of the phase bias correction to
*                           correspond to IS-QZSS-MDC (ref.[12]).
*           2024/09/27 1.22 delete NM,IM
*           2024/11/13 1.23 apply phase bias correction even when ambiguity
*                           resolution is not performed.
*           2025/03/18 1.24 support triple/quad-frequency ppp.
*                           support bds-2 and bds-3.
*-----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_ppp.h"
#include "mrtklib/mrtk_ppp_ar.h"
#include "mrtklib/mrtk_ppp_rtk.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_atmos.h"
#include "mrtklib/mrtk_antenna.h"
#include "mrtklib/mrtk_tides.h"
#include "mrtklib/mrtk_peph.h"
#include "mrtklib/mrtk_astro.h"
#include "mrtklib/mrtk_ionex.h"
#include "mrtklib/mrtk_bias_sinex.h"
#include "mrtklib/mrtk_sbas.h"
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
extern double sat2freq(int sat, uint8_t code, const nav_t *nav);
extern int satexclude(int sat, double var, int svh, const prcopt_t *opt);
extern char *time_str(gtime_t t, int n);
extern void time2str(gtime_t t, char *s, int n);
extern double time2gpst(gtime_t t, int *week);
extern void sunmoonpos(gtime_t tut, const double *erpv, double *rsun, double *rmoon, double *gmst);
extern double geodist(const double *rs, const double *rr, double *e);
extern double satazel(const double *pos, const double *e, double *azel);
extern void satantoff(gtime_t time, const double *rs, int sat, const nav_t *nav, double *dant);
extern void ecef2pos(const double *r, double *pos);
extern void ecef2enu(const double *pos, const double *r, double *e);
extern void xyz2enu(const double *pos, double *E);
extern void covecef(const double *pos, const double *Q, double *P);
extern void covenu(const double *pos, const double *P, double *Q);
extern double dot(const double *a, const double *b, int n);
extern double norm(const double *a, int n);
extern int normv3(const double *a, double *b);
extern void cross3(const double *a, const double *b, double *c);
extern void matcpy(double *A, const double *B, int n, int m);
extern void matmul(const char *tr, int n, int k, int m, double alpha, const double *A, const double *B, double beta, double *C);
extern double *mat(int n, int m);
extern int *imat(int n, int m);
extern double *zeros(int n, int m);
extern double *eye(int n);
extern int filter(double *x, double *P, const double *H, const double *v, const double *R, int n, int m);
extern double sbstropcorr(gtime_t time, const double *pos, const double *azel, double *var);
extern double ionmodel(gtime_t t, const double *ion, const double *pos, const double *azel);
extern int iontec(gtime_t time, const nav_t *nav, const double *pos, const double *azel, int opt, double *delay, double *var);
extern int sbsioncorr(gtime_t time, const nav_t *nav, const double *pos, const double *azel, double *delay, double *var);
extern double tropmodel(gtime_t time, const double *pos, const double *azel, double humi);
extern double tropmapf(gtime_t time, const double *pos, const double *azel, double *mapfw);
extern int testsnr(int base, int freq, double el, double snr, const snrmask_t *mask);
extern void antmodel(const pcv_t *pcv, const double *del, const double *azel, int opt, double *dant);
extern void antmodel_s(const pcv_t *pcv, double nadir, double *dant);
extern void satposs(gtime_t teph, const obsd_t *obs, int n, const nav_t *nav, int ephopt, double *rs, double *dts, double *var, int *svh);
extern gtime_t gpst2utc(gtime_t t);
extern void tidedisp(gtime_t tutc, const double *rr, int opt, const erp_t *erp, const double *odisp, double *dr);
extern unsigned int tickget(void);
extern char *code2obs(uint8_t code);
extern int mcssr_sel_biascode(const int sys, const int code);
/* freq_idx2freq_num, freq_num2freq, freq_idx2ant_idx declared in mrtk_nav.h */
extern int const_iono_corr(rtk_t *rtk, const obsd_t *obs, const nav_t *nav, const int n, const double *azel, const int *exc, const double *rr, const double *x, double *v, double *H, double *var);

#define SQR(x)      ((x)*(x))
#define SQRT(x)     ((x)<=0.0||(x)!=(x)?0.0:sqrt(x))
#define MAX(x,y)    ((x)>(y)?(x):(y))
#define MIN(x,y)    ((x)<(y)?(x):(y))
#define ROUND(x)    (int)floor((x)+0.5)

#define PPP_NFREQ   4               /* number of freq. for PPP */
#define SOLQ_FIX_WL 99              /* solution status: wide-lane fixed */

#define MAX_ITER    8               /* max number of iterations */
#define MIN_NSAT_SOL 4              /* min satellite number for solution */
#define THRES_REJECT 4.0            /* reject threshold of posfit-res (sigma) */

#define THRES_MW_JUMP 10.0

#define VAR_POS     SQR(60.0)       /* init variance receiver position (m^2) */
#define VAR_VEL     SQR(10.0)       /* init variance of receiver vel ((m/s)^2) */
#define VAR_ACC     SQR(10.0)       /* init variance of receiver acc ((m/ss)^2) */
#define VAR_CLK     SQR(60.0)       /* init variance receiver clock (m^2) */
#define VAR_ZTD     SQR(0.12)       /* init variance ztd (m^2) */
#define VAR_GRA     SQR(0.01)       /* init variance gradient (m^2) */
#define VAR_IFB     SQR(30.0)       /* init variance ifb (m^2) */
#define VAR_BIAS    SQR(60.0)       /* init variance phase-bias (m^2) */
#define VAR_IONO    SQR(60.0)       /* init variance iono-delay */
#define VAR_GLO_IFB SQR( 0.6)       /* variance of glonass ifb */

#define ERR_SAAS    0.3             /* saastamoinen model error std (m) */
#define ERR_BRDCI   0.5             /* broadcast iono model error factor */
#define REL_HUMI    0.7             /* relative humidity for saastamoinen model */
#define GAP_RESION  120             /* default gap to reset ionos parameters (ep) */

/* number and index of states */
#define NF(opt)     ((opt)->ionoopt==IONOOPT_IFLC?1:(opt)->nf)
#define NP(opt)     ((opt)->dynamics?9:3)
#define NC(opt)     (NSYS+1) /* BDS3 and BDS2 */
#define NT(opt)     ((opt)->tropopt<TROPOPT_EST?0:((opt)->tropopt==TROPOPT_EST?1:3))
#define NI(opt)     ((opt)->ionoopt==IONOOPT_EST?MAXSAT:0)
#define N3(opt)     (NF(opt)>=3?NC(opt):0)   /* 3rd-frequency receivre bias */
#define N4(opt)     (NF(opt)>=4?NC(opt):0)   /* 4th-frequency receivre bias */
#define NR(opt)     (NP(opt)+NC(opt)+NT(opt)+NI(opt)+N3(opt)+N4(opt))
#define NB(opt)     (NF(opt)*MAXSAT)
#define NX(opt)     (NR(opt)+NB(opt))
#define IC(s,opt)   (NP(opt)+(s))
#define IT(opt)     (NP(opt)+NC(opt))
#define II(s,opt)   (NP(opt)+NC(opt)+NT(opt)+(s)-1)
#define I3(s,opt)   (NP(opt)+NC(opt)+NT(opt)+NI(opt)+(s))
#define I4(s,opt)   (NP(opt)+NC(opt)+NT(opt)+NI(opt)+N3(opt)+(s))
#define IB(s,f,opt) (NR(opt)+MAXSAT*(f)+(s)-1)

/* standard deviation of state -----------------------------------------------*/
static double STD(rtk_t *rtk, int i)
{
    if (rtk->sol.stat==SOLQ_FIX) return SQRT(rtk->Pa[i+i*rtk->nx]);
    return SQRT(rtk->P[i+i*rtk->nx]);
}
/* write solution status for PPP ---------------------------------------------*/
extern int pppoutstat(rtk_t *rtk, char *buff)
{
    ssat_t *ssat;
    double tow,pos[3],vel[3],acc[3],*x;
    int i,j,week;
    char id[32],*p=buff;

    if (!rtk->sol.stat) return 0;

    /* PPP-RTK uses different state layout (no clocks, different indices) */
    if (rtk->opt.mode==PMODE_PPP_RTK) {
        return ppprtk_outstat(rtk,buff);
    }

    trace(NULL,3,"pppoutstat:\n");

    tow=time2gpst(rtk->sol.time,&week);

    x=rtk->sol.stat==SOLQ_FIX?rtk->xa:rtk->x;

    /* receiver position */
    p+=sprintf(p,"$POS,%d,%.3f,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",week,tow,
               rtk->sol.stat,x[0],x[1],x[2],STD(rtk,0),STD(rtk,1),STD(rtk,2));

    /* receiver velocity and acceleration */
    if (rtk->opt.dynamics) {
        ecef2pos(rtk->sol.rr,pos);
        ecef2enu(pos,rtk->x+3,vel);
        ecef2enu(pos,rtk->x+6,acc);
        p+=sprintf(p,"$VELACC,%d,%.3f,%d,%.4f,%.4f,%.4f,%.5f,%.5f,%.5f,%.4f,%.4f,"
                   "%.4f,%.5f,%.5f,%.5f\n",week,tow,rtk->sol.stat,vel[0],vel[1],
                   vel[2],acc[0],acc[1],acc[2],0.0,0.0,0.0,0.0,0.0,0.0);
    }
    /* receiver clocks */
    i=IC(0,&rtk->opt);
    p+=sprintf(p,"$CLK,%d,%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f\n",
               week,tow,rtk->sol.stat,1,x[i]*1E9/CLIGHT,x[i+1]*1E9/CLIGHT,
               STD(rtk,i)*1E9/CLIGHT,STD(rtk,i+1)*1E9/CLIGHT);

    /* tropospheric parameters */
    if (rtk->opt.tropopt==TROPOPT_EST||rtk->opt.tropopt==TROPOPT_ESTG) {
        i=IT(&rtk->opt);
        p+=sprintf(p,"$TROP,%d,%.3f,%d,%d,%.4f,%.4f\n",week,tow,rtk->sol.stat,
                   1,x[i],STD(rtk,i));
    }
    if (rtk->opt.tropopt==TROPOPT_ESTG) {
        i=IT(&rtk->opt);
        p+=sprintf(p,"$TRPG,%d,%.3f,%d,%d,%.5f,%.5f,%.5f,%.5f\n",week,tow,
                   rtk->sol.stat,1,x[i+1],x[i+2],STD(rtk,i+1),STD(rtk,i+2));
    }
    /* ionosphere parameters */
    if (rtk->opt.ionoopt==IONOOPT_EST) {
        ecef2pos(rtk->sol.rr,pos);
        for (i=0;i<MAXSAT;i++) {
            ssat=rtk->ssat+i;
            if (!ssat->vs) continue;
            j=II(i+1,&rtk->opt);
            if (rtk->x[j]==0.0) continue;
            satno2id(i+1,id);
            p+=sprintf(p,"$ION,%d,%.3f,%d,%s,%.1f,%.1f,%.4f,%.4f\n",week,tow,
                       rtk->sol.stat,id,rtk->ssat[i].azel[0]*R2D,
                       rtk->ssat[i].azel[1]*R2D,x[j],STD(rtk,j));
        }
    }
#ifdef OUTSTAT_AMB
    /* ambiguity parameters */
    for (i=0;i<MAXSAT;i++) for (j=0;j<NF(&rtk->opt);j++) {
        k=IB(i+1,j,&rtk->opt);
        if (rtk->x[k]==0.0) continue;
        satno2id(i+1,id);
        p+=sprintf(p,"$AMB,%d,%.3f,%d,%s,%d,%.4f,%.4f\n",week,tow,
                   rtk->sol.stat,id,j+1,x[k],STD(rtk,k));
    }
#endif
    return (int)(p-buff);
}
/* exclude meas of eclipsing satellite (block IIA) ---------------------------*/
static void testeclipse(const obsd_t *obs, int n, const nav_t *nav, double *rs)
{
    double rsun[3],esun[3],r,ang,erpv[5]={0},cosa;
    int i,j;
    const char *type;

    trace(NULL,3,"testeclipse:\n");

    /* unit vector of sun direction (ecef) */
    sunmoonpos(gpst2utc(obs[0].time),erpv,rsun,NULL,NULL);
    normv3(rsun,esun);

    for (i=0;i<n;i++) {
        type=nav->pcvs[obs[i].sat-1].type;

        if ((r=norm(rs+i*6,3))<=0.0) continue;

        /* only block IIA */
        if (*type&&!strstr(type,"BLOCK IIA")) continue;

        /* sun-earth-satellite angle */
        cosa=dot(rs+i*6,esun,3)/r;
        cosa=cosa<-1.0?-1.0:(cosa>1.0?1.0:cosa);
        ang=acos(cosa);

        /* test eclipse */
        if (ang<PI/2.0||r*sin(ang)>RE_WGS84) continue;

        trace(NULL,3,"eclipsing sat excluded %s sat=%2d\n",time_str(obs[0].time,0),
              obs[i].sat);

        for (j=0;j<3;j++) rs[j+i*6]=0.0;
    }
}
/* nominal yaw-angle ---------------------------------------------------------*/
static double yaw_nominal(double beta, double mu)
{
    if (fabs(beta)<1E-12&&fabs(mu)<1E-12) return PI;
    return atan2(-tan(beta),sin(mu))+PI;
}
/* yaw-angle of satellite ----------------------------------------------------*/
extern int yaw_angle(int sat, const char *type, int opt, double beta, double mu,
                     double *yaw)
{
    *yaw=yaw_nominal(beta,mu);
    return 1;
}
/* satellite attitude model --------------------------------------------------*/
static int sat_yaw(gtime_t time, int sat, const char *type, int opt,
                   const double *rs, double *exs, double *eys)
{
    double rsun[3],ri[6],es[3],esun[3],n[3],p[3],en[3],ep[3],ex[3],E,beta,mu;
    double yaw,cosy,siny,erpv[5]={0};
    int i;

    sunmoonpos(gpst2utc(time),erpv,rsun,NULL,NULL);

    /* beta and orbit angle */
    matcpy(ri,rs,6,1);
    ri[3]-=OMGE*ri[1];
    ri[4]+=OMGE*ri[0];
    cross3(ri,ri+3,n);
    cross3(rsun,n,p);
    if (!normv3(rs,es)||!normv3(rsun,esun)||!normv3(n,en)||
        !normv3(p,ep)) return 0;
    beta=PI/2.0-acos(dot(esun,en,3));
    E=acos(dot(es,ep,3));
    mu=PI/2.0+(dot(es,esun,3)<=0?-E:E);
    if      (mu<-PI/2.0) mu+=2.0*PI;
    else if (mu>=PI/2.0) mu-=2.0*PI;

    /* yaw-angle of satellite */
    if (!yaw_angle(sat,type,opt,beta,mu,&yaw)) return 0;

    /* satellite fixed x,y-vector */
    cross3(en,es,ex);
    cosy=cos(yaw);
    siny=sin(yaw);
    for (i=0;i<3;i++) {
        exs[i]=-siny*en[i]+cosy*ex[i];
        eys[i]=-cosy*en[i]-siny*ex[i];
    }
    return 1;
}
/* phase windup model --------------------------------------------------------*/
static int model_phw(gtime_t time, int sat, const char *type, int opt,
                     const double *rs, const double *rr, double *phw)
{
    double exs[3],eys[3],ek[3],exr[3],eyr[3],eks[3],ekr[3],E[9];
    double dr[3],ds[3],drs[3],r[3],pos[3],cosp,ph;
    int i;

    if (opt<=0) return 1; /* no phase windup */

    /* satellite yaw attitude model */
    if (!sat_yaw(time,sat,type,opt,rs,exs,eys)) return 0;

    /* unit vector satellite to receiver */
    for (i=0;i<3;i++) r[i]=rr[i]-rs[i];
    if (!normv3(r,ek)) return 0;

    /* unit vectors of receiver antenna */
    ecef2pos(rr,pos);
    xyz2enu(pos,E);
    exr[0]= E[1]; exr[1]= E[4]; exr[2]= E[7]; /* x = north */
    eyr[0]=-E[0]; eyr[1]=-E[3]; eyr[2]=-E[6]; /* y = west  */

    /* phase windup effect */
    cross3(ek,eys,eks);
    cross3(ek,eyr,ekr);
    for (i=0;i<3;i++) {
        ds[i]=exs[i]-ek[i]*dot(ek,exs,3)-eks[i];
        dr[i]=exr[i]-ek[i]*dot(ek,exr,3)+ekr[i];
    }
    cosp=dot(ds,dr,3)/norm(ds,3)/norm(dr,3);
    if      (cosp<-1.0) cosp=-1.0;
    else if (cosp> 1.0) cosp= 1.0;
    ph=acos(cosp)/2.0/PI;
    cross3(ds,dr,drs);
    if (dot(ek,drs,3)<0.0) ph=-ph;

    *phw=ph+floor(*phw-ph+0.5); /* in cycle */
    return 1;
}
/* measurement error variance ------------------------------------------------*/
static double varerr(int sat, int sys, double el, int idx, int type,
                     const prcopt_t *opt)
{
    double fact=1.0,sinel=sin(el);

    if (type==1) fact*=opt->eratio[idx==0?0:1];
    fact*=sys==SYS_GLO?EFACT_GLO:(sys==SYS_GAL?EFACT_GAL:
          (sys==SYS_QZS?EFACT_QZS:(sys==SYS_CMP?EFACT_CMP:
          (sys==SYS_IRN?EFACT_IRN:(sys==SYS_SBS?EFACT_SBS:EFACT_GPS)))));
    if (opt->ionoopt==IONOOPT_IFLC) fact*=3.0;
    return SQR(fact*opt->err[1])+SQR(fact*opt->err[2]/sinel);
}
/* initialize state and covariance -------------------------------------------*/
static void initx(rtk_t *rtk, double xi, double var, int i)
{
    int j;
    rtk->x[i]=xi;
    for (j=0;j<rtk->nx;j++) {
        rtk->P[i+j*rtk->nx]=rtk->P[j+i*rtk->nx]=i==j?var:0.0;
    }
}
/* geometry-free phase measurement -------------------------------------------*/
static double gfmeas(const obsd_t *obs, const nav_t *nav, int f)
{
    double freq1,freq2;

    freq1=sat2freq(obs->sat,obs->code[0],nav);
    freq2=sat2freq(obs->sat,obs->code[f],nav);
    if (freq1==0.0||freq2==0.0||obs->L[0]==0.0||obs->L[f]==0.0) return 0.0;
    return (obs->L[0]/freq1-obs->L[f]/freq2)*CLIGHT;
}
/* Melbourne-Wubbena linear combination --------------------------------------*/
static double mwmeas(const obsd_t *obs, const nav_t *nav, int f)
{
    double freq1,freq2;

    freq1=sat2freq(obs->sat,obs->code[0],nav);
    freq2=sat2freq(obs->sat,obs->code[f],nav);

    if (freq1==0.0||freq2==0.0||obs->L[0]==0.0||obs->L[f]==0.0||
        obs->P[0]==0.0||obs->P[f]==0.0) return 0.0;
    return (obs->L[0]-obs->L[f])*CLIGHT/(freq1-freq2)-
           (freq1*obs->P[0]+freq2*obs->P[f])/(freq1+freq2);
}
/* antenna corrected measurements --------------------------------------------*/
static int corr_meas(const obsd_t *obs, const nav_t *nav, const double *azel,
                     const prcopt_t *opt, const double *dantr,
                     const double *dants, double phw, double *L, double *P,
                     double *Lc, double *Pc, int tl)
{
    char satid[8],*tstr=time_str(obs->time, 3);
    double freq[NFREQ]={0},C1,C2,cb=0.0,pb=0.0;
    int i,sys=satsys(obs->sat,NULL),ssrcode=CODE_NONE,ant_idx;
    int ssr_bias=0;

    if (strstr(opt->pppopt,"-SSR_BIAS")) {
        ssr_bias=1;
    }

    satno2id(obs->sat, satid);
    for (i=0;i<opt->nf&&i<NFREQ;i++) {
        L[i]=P[i]=cb=pb=0.0;
        freq[i]=sat2freq(obs->sat,obs->code[i],nav);

        if (freq[i]==0.0||obs->L[i]==0.0||obs->P[i]==0.0) continue;
        if (testsnr(0,0,azel[1],obs->SNR[i]*SNR_UNIT,&opt->snrmask)) continue;

        /* antenna index */
        ant_idx=freq_idx2ant_idx(satsys_bd2(obs->sat,NULL),i);

        /* antenna phase center and phase windup correction */
        L[i]=obs->L[i]*CLIGHT/freq[i]-dants[ant_idx]-dantr[ant_idx]-phw*CLIGHT/freq[i];
        P[i]=obs->P[i]-dants[ant_idx]-dantr[ant_idx];

        /* SSR cbias, pbias correction */
        ssrcode = mcssr_sel_biascode(sys, obs->code[i]);

        /* for backward compatible to IS-QZSS-MDC-003 */
        if (sys==SYS_GPS && ssrcode==CODE_L5Q) {
            if (nav->ssr_ch[0][obs->sat-1].pbias[ssrcode-1]==0.0&&
                nav->ssr_ch[0][obs->sat-1].cbias[ssrcode-1]==0.0) ssrcode=CODE_L5X;
        }

        if(ssrcode != CODE_NONE) {
            if(nav->ssr_ch[0][obs->sat-1].pbias[ssrcode-1]!=0.0) pb=nav->ssr_ch[0][obs->sat-1].pbias[ssrcode-1];
            if(nav->ssr_ch[0][obs->sat-1].cbias[ssrcode-1]!=0.0) cb=nav->ssr_ch[0][obs->sat-1].cbias[ssrcode-1];
        }
        if(cb!=0.0) {
            P[i]+=cb;
            trace(NULL,tl>0?4:4,"corr_meas: %s cbias %s obscode=C%s ssrcode=C%s cbias=%7.3f\n",
                tstr,satid,code2obs(obs->code[i]),code2obs(ssrcode),cb);
        }
        else {
            if (!nav->ssr_ch[0][obs->sat-1].vcbias[ssrcode-1]) {
                P[i]=0.0;
                trace(NULL,tl>0?3:4,"corr_meas: %s cbias dose not exist. %s obscode=C%s ssrcode=C%s cbias=%7.3f\n",
                    tstr,satid,code2obs(obs->code[i]),code2obs(ssrcode),cb);
            }
        }
        if (cb<=(SSR_INVALID_CBIAS+0.005)) {
            P[i]=0.0;
            trace(NULL,tl>0?2:4,"corr_meas: %s invalid cbias %s obscode=C%s ssrcode=C%s cbias=%7.3f\n",
                tstr,satid,code2obs(obs->code[i]),code2obs(ssrcode),cb);
        }

        if(pb!=0.0) {
            L[i]+=pb;
            trace(NULL,tl>0?4:4,"corr_meas: %s pbias %s obscode=L%s ssrcode=L%s pbias=%7.3f\n",
                tstr,satid,code2obs(obs->code[i]),code2obs(ssrcode),pb);
        }
        else if (sys!=SYS_GLO) {
            if (!nav->ssr_ch[0][obs->sat-1].vpbias[ssrcode-1]) {
                L[i]=0.0;
                trace(NULL,tl>0?3:4,"corr_meas: %s pbias dose not exist. %s obscode=C%s ssrcode=C%s pbias=%7.3f\n",
                    tstr,satid,code2obs(obs->code[i]),code2obs(ssrcode),pb);
            }
        }
        if(pb<=(SSR_INVALID_PBIAS+0.00005)) {
            L[i]=0.0;
            trace(NULL,tl>0?2:4,"corr_meas: %s invalid pbias %s obscode=L%s ssrcode=L%s pbias=%7.3f\n",
                tstr,satid,code2obs(obs->code[i]),code2obs(ssrcode),pb);
        }
    }

    /* iono-free LC */
    *Lc=*Pc=0.0;
    if (freq[0]==0.0||freq[1]==0.0) return 0;
    C1= SQR(freq[0])/(SQR(freq[0])-SQR(freq[1]));
    C2=-SQR(freq[1])/(SQR(freq[0])-SQR(freq[1]));

    if (P[0]==0.0||P[1]==0.0||L[0]==0.0||L[1]==0.0) {
        trace(NULL,tl>0?2:4,"corr_meas: no sufficient obs data %s %s \n",
                tstr,satid);
        return 0;
    }
    *Lc=C1*L[0]+C2*L[1];
    *Pc=C1*P[0]+C2*P[1];

    return 1;
}
/* detect cycle slip by LLI --------------------------------------------------*/
/* detect cycle slip by Doppler/phase rate comparison (demo5, single-receiver) */
static void detslp_dop(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    (void)nav;
    int i,f,sat,ndop=0,nf=rtk->opt.nf<NFREQ?rtk->opt.nf:NFREQ;
    double dph,dpt,tt,mean_dop=0.0;
    double dopdif[MAXOBS][NFREQ]={{0}},tts[MAXOBS][NFREQ]={{0}};

    if (rtk->opt.thresdop<=0.0) return;

    for (i=0;i<n&&i<MAXOBS;i++) {
        sat=obs[i].sat;
        for (f=0;f<nf;f++) {
            if (obs[i].L[f]==0.0||obs[i].D[f]==0.0||
                rtk->ssat[sat-1].ph[0][f]==0.0) continue;
            tt=timediff(obs[i].time,rtk->ssat[sat-1].pt[0][f]);
            if (fabs(tt)<DTTOL) continue;
            dph=(obs[i].L[f]-rtk->ssat[sat-1].ph[0][f])/tt;
            dpt=-obs[i].D[f];
            dopdif[i][f]=dph-dpt;
            tts[i][f]=tt;
            if (fabs(dopdif[i][f])<3.0*rtk->opt.thresdop) {
                mean_dop+=dopdif[i][f]; ndop++;
            }
        }
    }
    if (ndop==0) return;
    mean_dop/=ndop;

    for (i=0;i<n&&i<MAXOBS;i++) {
        sat=obs[i].sat;
        for (f=0;f<nf;f++) {
            if (dopdif[i][f]==0.0) continue;
            if (fabs(dopdif[i][f]-mean_dop)>rtk->opt.thresdop) {
                rtk->ssat[sat-1].slip[f]|=1;
                trace(NULL,3,"ppp detslp_dop: sat=%2d F=%d dL=%.3f off=%.3f tt=%.2f\n",
                      sat,f+1,dopdif[i][f]-mean_dop,mean_dop,tts[i][f]);
            }
        }
    }
}
/* detect cycle slip by observation code change (demo5, single-receiver) */
static void detslp_code(rtk_t *rtk, const obsd_t *obs, int n)
{
    int i,f,sat,nf=rtk->opt.nf<NFREQ?rtk->opt.nf:NFREQ;
    for (i=0;i<n&&i<MAXOBS;i++) {
        sat=obs[i].sat;
        for (f=0;f<nf;f++) {
            uint8_t code=obs[i].code[f];
            if (code==CODE_NONE) continue;
            uint8_t ccode=rtk->ssat[sat-1].codeprev[f][0];
            if (code!=ccode) {
                rtk->ssat[sat-1].codeprev[f][0]=code;
                if (ccode!=CODE_NONE) {
                    rtk->ssat[sat-1].slip[f]|=1;
                    trace(NULL,3,"ppp detslp_code: sat=%2d F=%d %s->%s\n",
                          sat,f+1,code2obs(ccode),code2obs(code));
                }
            }
        }
    }
}
static void detslp_ll(rtk_t *rtk, const obsd_t *obs, int n)
{
    int i,j;
    char satid[4];
    trace(NULL,3,"detslp_ll: n=%d\n",n);

    /* Note, When using gcc, use constants to suppress
             -Wstringop-overflow warnings. */
    for (i=0;i<n&&i<MAXOBS;i++) for (j=0;j<rtk->opt.nf&&j<NFREQ;j++) {
        if (obs[i].L[j]==0.0||!(obs[i].LLI[j]&3)) continue;
        satno2id(obs[i].sat, satid);
        trace(NULL,3,"detslp_ll: slip detected %s sat=%2d f=%d\n",satid,obs[i].sat,j+1);

        rtk->ssat[obs[i].sat-1].slip[j]=1;
    }
}
/* detect cycle slip by geometry free phase jump -----------------------------*/
static void detslp_gf(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    double g0,g1;
    int i,f;
    char satid[4];
    trace(NULL,3,"detslp_gf: n=%d\n",n);

    for (i=0;i<n&&i<MAXOBS;i++) {
        for (f=1;f<rtk->opt.nf&&f<NFREQ;f++) {
            if ((g1=gfmeas(obs+i,nav,f))==0.0) continue;

            g0=rtk->ssat[obs[i].sat-1].gf[f];
            rtk->ssat[obs[i].sat-1].gf[f]=g1;

            trace(NULL,4,"detslip_gf: sat=%2d f=%d gf0=%8.3f gf1=%8.3f\n",obs[i].sat,f+1,g0,g1);

            if (g0!=0.0&&fabs(g1-g0)>rtk->opt.thresslip) {
                satno2id(obs[i].sat, satid);
                trace(NULL,2,"detslip_gf: slip detected %s sat=%2d f=%d gf=%8.3f->%8.3f\n",
                      satid,obs[i].sat,f+1,g0,g1);

                rtk->ssat[obs[i].sat-1].slip[0]|=1;
                rtk->ssat[obs[i].sat-1].slip[f]|=1;
            }
        }
    }
}
/* detect slip by Melbourne-Wubbena linear combination jump ------------------*/
static void detslp_mw(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    double w0,w1;
    int i,f;
    char satid[4];
    trace(NULL,3,"detslp_mw: n=%d\n",n);

    for (i=0;i<n&&i<MAXOBS;i++) {
        for (f=1;f<rtk->opt.nf&&f<NFREQ;f++) {
            if ((w1=mwmeas(obs+i,nav,f))==0.0) continue;

            w0=rtk->ssat[obs[i].sat-1].mw[f];
            rtk->ssat[obs[i].sat-1].mw[f]=w1;

            trace(NULL,4,"detslip_mw: sat=%2d f=%d mw0=%8.3f mw1=%8.3f\n",obs[i].sat,f+1,w0,w1);

            if (w0!=0.0&&fabs(w1-w0)>THRES_MW_JUMP) {
                satno2id(obs[i].sat, satid);
                trace(NULL,2,"detslip_mw: slip detected %s sat=%2d f=%d mw=%8.3f->%8.3f\n",
                      satid,obs[i].sat,f+1,w0,w1);

                rtk->ssat[obs[i].sat-1].slip[0]|=1;
                rtk->ssat[obs[i].sat-1].slip[f]|=1;
            }
        }
    }
}
/* detect cycle slip by ssr phase bias discontinuity indicator ---------------*/
static void detslp_ssr(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    int discont0,discont1,ssrcode;
    int sys,i,j;
    char satid[4];
    trace(NULL,3,"detslp_ssr: n=%d\n",n);

    if (rtk->opt.sateph!=EPHOPT_SSRAPC && rtk->opt.sateph!=EPHOPT_SSRCOM) {
        return;
    }

    for (i=0;i<n&&i<MAXOBS;i++) {
        if (!nav->ssr_ch[0][obs[i].sat-1].t0[5].time) continue;

        sys=satsys(obs[i].sat, NULL);

        /* detect slip by discontinuity indicator */
        for (j=0;j<rtk->opt.nf&&j<NFREQ;j++) {
            ssrcode = mcssr_sel_biascode(sys, obs[i].code[j]);
            if (ssrcode == CODE_NONE) continue;
            if (nav->ssr_ch[0][obs[i].sat-1].pbias[ssrcode-1]==0.0) continue;

            discont0=rtk->ssat[obs[i].sat-1].discont[j];
            discont1=nav->ssr_ch[0][obs[i].sat-1].discnt[ssrcode-1];
            rtk->ssat[obs[i].sat-1].discont[j]=discont1;

            if (discont0!=discont1) {
                satno2id(obs[i].sat, satid);
                trace(NULL,2,"detslp_ssr: discont detected sys=%2d %s sat=%2d f=%d code=%2d discont0=%d discont1=%d\n",
                    sys,satid,obs[i].sat,j+1,ssrcode,discont0,discont1);

                rtk->ssat[obs[i].sat-1].slip[j]|=1;
            }
        }
    }
}
/* temporal update of position -----------------------------------------------*/
static void udpos_ppp(rtk_t *rtk)
{
    double *F,*P,*FP,*x,*xp,pos[3],Q[9]={0},Qv[9];
    int i,j,*ix,nx;

    trace(NULL,3,"udpos_ppp:\n");

    /* fixed mode */
    if (rtk->opt.mode==PMODE_PPP_FIXED) {
        for (i=0;i<3;i++) initx(rtk,rtk->opt.ru[i],1E-8,i);
        return;
    }
    /* initialize position for first epoch */
    if (norm(rtk->x,3)<=0.0) {
        for (i=0;i<3;i++) initx(rtk,rtk->sol.rr[i],VAR_POS,i);
        if (rtk->opt.dynamics) {
            for (i=3;i<6;i++) initx(rtk,rtk->sol.rr[i],VAR_VEL,i);
            for (i=6;i<9;i++) initx(rtk,1E-6,VAR_ACC,i);
        }
    }
    /* static ppp mode */
    if (rtk->opt.mode==PMODE_PPP_STATIC) {
        for (i=0;i<3;i++) {
            rtk->P[i*(1+rtk->nx)]+=SQR(rtk->opt.prn[5])*fabs(rtk->tt);
        }
        return;
    }
    /* kinmatic mode without dynamics */
    if (!rtk->opt.dynamics) {
        for (i=0;i<3;i++) {
            initx(rtk,rtk->sol.rr[i],VAR_POS,i);
        }
        return;
    }
    /* generate valid state index */
    ix=imat(rtk->nx,1);
    for (i=nx=0;i<rtk->nx;i++) {
        if (rtk->x[i]!=0.0&&rtk->P[i+i*rtk->nx]>0.0) ix[nx++]=i;
    }
    if (nx<9) {
        free(ix);
        return;
    }
    /* state transition of position/velocity/acceleration */
    F=eye(nx); P=mat(nx,nx); FP=mat(nx,nx); x=mat(nx,1); xp=mat(nx,1);

    for (i=0;i<6;i++) {
        F[i+(i+3)*nx]=rtk->tt;
    }
    for (i=0;i<3;i++) {
        F[i+(i+6)*nx]=SQR(rtk->tt)/2.0;
    }
    for (i=0;i<nx;i++) {
        x[i]=rtk->x[ix[i]];
        for (j=0;j<nx;j++) {
            P[i+j*nx]=rtk->P[ix[i]+ix[j]*rtk->nx];
        }
    }
    /* x=F*x, P=F*P*F+Q */
    matmul("NN",nx,1,nx,1.0,F,x,0.0,xp);
    matmul("NN",nx,nx,nx,1.0,F,P,0.0,FP);
    matmul("NT",nx,nx,nx,1.0,FP,F,0.0,P);

    for (i=0;i<nx;i++) {
        rtk->x[ix[i]]=xp[i];
        for (j=0;j<nx;j++) {
            rtk->P[ix[i]+ix[j]*rtk->nx]=P[i+j*nx];
        }
    }
    /* process noise added to only acceleration */
    Q[0]=Q[4]=SQR(rtk->opt.prn[3])*fabs(rtk->tt);
    Q[8]=SQR(rtk->opt.prn[4])*fabs(rtk->tt);
    ecef2pos(rtk->x,pos);
    covecef(pos,Q,Qv);
    for (i=0;i<3;i++) for (j=0;j<3;j++) {
        rtk->P[i+6+(j+6)*rtk->nx]+=Qv[i+j*3];
    }
    free(ix); free(F); free(P); free(FP); free(x); free(xp);
}
/* temporal update of clock --------------------------------------------------*/
static void udclk_ppp(rtk_t *rtk)
{
    double dtr;
    int i;

    trace(NULL,3,"udclk_ppp:\n");

    /* initialize every epoch for clock (white noise) */
    for (i=0;i<NC(rtk->opt);i++) {
        if (rtk->opt.sateph==EPHOPT_PREC) {
            /* time of prec ephemeris is based gpst */
            /* negelect receiver inter-system bias  */
            dtr=rtk->sol.dtr[0];
        }
        else {
            dtr=i==0?rtk->sol.dtr[0]:rtk->sol.dtr[0]+rtk->sol.dtr[i];
        }
        initx(rtk,CLIGHT*dtr,VAR_CLK,IC(i,&rtk->opt));
    }
}
/* temporal update of tropospheric parameters --------------------------------*/
static void udtrop_ppp(rtk_t *rtk)
{
    double pos[3],azel[]={0.0,PI/2.0},ztd,var;
    int i=IT(&rtk->opt),j;

    trace(NULL,3,"udtrop_ppp:\n");

    if (rtk->x[i]==0.0) {
        ecef2pos(rtk->sol.rr,pos);
        ztd=sbstropcorr(rtk->sol.time,pos,azel,&var);
        initx(rtk,ztd,VAR_ZTD,i);

        if (rtk->opt.tropopt>=TROPOPT_ESTG) {
            for (j=i+1;j<i+3;j++) initx(rtk,1E-6,VAR_GRA,j);
        }
    }
    else {
        rtk->P[i+i*rtk->nx]+=SQR(rtk->opt.prn[2])*fabs(rtk->tt);
        trace(NULL,2,"udtrop_ppp: rtk->opt.prn[2]=%.3e\n",rtk->opt.prn[2]);
        if (rtk->opt.tropopt>=TROPOPT_ESTG) {
            for (j=i+1;j<i+3;j++) {
                rtk->P[j+j*rtk->nx]+=SQR(rtk->opt.prn[2]*0.1)*fabs(rtk->tt);
            }
        }
    }
}
/* temporal update of ionospheric parameters ---------------------------------*/
static void udiono_ppp(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    double freq1,freq2,ionp,ionc,sinel,pos[3],*azel,el;
    char *p,satid[4];
    int i,j,gap_resion=GAP_RESION;
    double L[NFREQ],P[NFREQ],Lc,Pc;
    double dantr[NFREQPCV]={0},dants[NFREQPCV]={0},dion;
    uint8_t *slip;
    trace(NULL,3,"udiono_ppp:\n");

    if ((p=strstr(rtk->opt.pppopt,"-GAP_RESION="))) {
        sscanf(p,"-GAP_RESION=%d",&gap_resion);
    }
    for (i=0;i<MAXSAT;i++) {
        j=II(i+1,&rtk->opt);
        if (rtk->x[j]!=0.0&&(int)rtk->ssat[i].outc[0]>gap_resion) {
            rtk->x[j]=0.0;

            if ((int)rtk->ssat[i].outc[0]==gap_resion+1 &&
                rtk->ssat[i].azel[1]>=rtk->opt.elmin) {
                satno2id(i+1, satid);
                el=rtk->ssat[i].azel[1]*R2D;
                trace(NULL,3,"udiono_ppp(%4d): %s reset ionospheric estimates. %s outc=%d el=%2.0f\n",__LINE__,
                    time_str(obs[0].time,0),satid,(int)rtk->ssat[i].outc[0],el);
            }
        }
    }
    for (i=0;i<n;i++) {
        j=II(obs[i].sat,&rtk->opt);
        corr_meas(obs+i,nav,rtk->ssat[obs[i].sat-1].azel,&rtk->opt,dantr,dants,
                  0.0,L,P,&Lc,&Pc,0);
        freq1=sat2freq(obs[i].sat,obs[i].code[0],nav);
        freq2=sat2freq(obs[i].sat,obs[i].code[1],nav);
        slip=rtk->ssat[obs[i].sat-1].slip;

        if (rtk->x[j]==0.0) {
            if (P[0]==0.0||P[1]==0.0||freq1==0.0||freq2==0.0) {
                continue;
            }
            ionp=(P[0]-P[1])/(SQR(FREQ1/freq1)-SQR(FREQ1/freq2));
            ecef2pos(rtk->sol.rr,pos);
            azel=rtk->ssat[obs[i].sat-1].azel;
            initx(rtk,ionp,VAR_IONO,j);

            if (L[0]==0.0||L[1]==0.0||slip[0]||slip[1]) {
                rtk->ssat[obs[i].sat-1].ionc=0.0;
            }
            else {
                rtk->ssat[obs[i].sat-1].ionc=-1.0*(L[0]-L[1])/(SQR(FREQ1/freq1)-SQR(FREQ1/freq2));
            }
            if (azel[1]>=rtk->opt.elmin) {
                satno2id(obs[i].sat, satid);
                trace(NULL,3,"udiono_ppp(%4d): %s initialize ionospheric estimates. %s el=%2.0f x=%8.4f\n",__LINE__,
                        time_str(obs[i].time,0),satid,azel[1]*R2D,rtk->x[j]);
            }
        }
        else {
            sinel=sin(MAX(rtk->ssat[obs[i].sat-1].azel[1],5.0*D2R));
            rtk->P[j+j*rtk->nx]+=SQR(rtk->opt.prn[1]/sinel)*fabs(rtk->tt);

            if (L[0]==0.0||L[1]==0.0||freq1==0.0||freq2==0.0||
                slip[0]||slip[1]) {
                rtk->ssat[obs[i].sat-1].ionc=0.0;
                continue;
            }
            ionc=-1.0*(L[0]-L[1])/(SQR(FREQ1/freq1)-SQR(FREQ1/freq2));
            dion=0.0;
            if (rtk->ssat[obs[i].sat-1].ionc!=0.0) {
                dion=ionc-rtk->ssat[obs[i].sat-1].ionc;
            }
            rtk->ssat[obs[i].sat-1].ionc=ionc;
            rtk->x[j]+=dion;

        }
    }
}
/* temporal update of 3rd/4th-freqency-receiver-bias parameters --------------*/
static void udifb_ppp(rtk_t *rtk)
{
    int i,j;

    trace(NULL,3,"udifb_ppp:\n");

    /* 3rd-freqency-receiver-bias */
    for (i=0;i<N3(&rtk->opt);i++) {
        j=I3(i,&rtk->opt);

        if (rtk->x[j]==0.0) {
            initx(rtk,1e-6,VAR_IFB,j);
        }
        else {
            /* add process noise */
            rtk->P[j+j*rtk->nx]+=SQR(rtk->opt.prn[6])*fabs(rtk->tt);
        }
    }

    /* 4th-freqency-receiver-bias */
    for (i=0;i<N4(&rtk->opt);i++) {
        j=I4(i,&rtk->opt);

        if (rtk->x[j]==0.0) {
            initx(rtk,1e-6,VAR_IFB,j);
        }
        else {
            /* add process noise */
            rtk->P[j+j*rtk->nx]+=SQR(rtk->opt.prn[6])*fabs(rtk->tt);
        }
    }
}
/* temporal update of phase biases -------------------------------------------*/
static void udbias_ppp(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    double L[NFREQ],P[NFREQ],Lc,Pc,bias[MAXOBS],offset=0.0,pos[3]={0};
    double freq1,freq2,freq3,ion,dantr[NFREQPCV]={0},dants[NFREQPCV]={0};
    int i,j,k,f,sat,slip[MAXOBS]={0},clk_jump=0;

    trace(NULL,3,"udbias  : n=%d\n",n);

    /* handle day-boundary clock jump */
    if (rtk->opt.posopt[5]) {
        clk_jump=ROUND(time2gpst(obs[0].time,NULL)*10)%864000==0;
    }
    for (i=0;i<MAXSAT;i++) for (j=0;j<rtk->opt.nf&&j<NFREQ;j++) {
        rtk->ssat[i].slip[j]=0;
    }
    /* detect cycle slip by LLI */
    detslp_ll(rtk,obs,n);

    /* detect cycle slip by geometry-free phase jump */
    detslp_gf(rtk,obs,n,nav);

    /* detect slip by Melbourne-Wubbena linear combination jump */
    detslp_mw(rtk,obs,n,nav);

    /* detect cycle slip by ssr */
    detslp_ssr(rtk,obs,n,nav);

    /* detect cycle slip by Doppler rate (demo5) */
    detslp_dop(rtk,obs,n,nav);

    /* detect cycle slip by observation code change (demo5) */
    detslp_code(rtk,obs,n);

    ecef2pos(rtk->sol.rr,pos);

    for (f=0;f<NF(&rtk->opt);f++) {

        /* reset phase-bias if expire obs outage counter */
        for (i=0;i<MAXSAT;i++) {
            if (++rtk->ssat[i].outc[f]>(uint32_t)rtk->opt.maxout||
                rtk->opt.modear==ARMODE_INST||clk_jump) {
                initx(rtk,0.0,0.0,IB(i+1,f,&rtk->opt));
            }
        }
        for (i=k=0;i<n&&i<MAXOBS;i++) {
            sat=obs[i].sat;
            j=IB(sat,f,&rtk->opt);
            corr_meas(obs+i,nav,rtk->ssat[sat-1].azel,&rtk->opt,dantr,dants,
                      0.0,L,P,&Lc,&Pc,0);

            bias[i]=0.0;

            if (rtk->opt.ionoopt==IONOOPT_IFLC) {
                bias[i]=Lc-Pc;
                slip[i]=rtk->ssat[sat-1].slip[0]||rtk->ssat[sat-1].slip[1];
            }
            else if (L[f]!=0.0&&P[f]!=0.0) {
                freq1=sat2freq(sat,obs[i].code[0],nav);
                freq2=sat2freq(sat,obs[i].code[1],nav);
                freq3=sat2freq(sat,obs[i].code[f],nav);
                slip[i]=rtk->ssat[sat-1].slip[f];
                if (obs[i].P[0]==0.0||obs[i].P[1]==0.0||obs[i].P[f]==0.0||
                    freq1==0.0||freq2==0.0||freq3==0.0) {
                    continue;
                }
                ion=(obs[i].P[0]-obs[i].P[1])/(1.0-SQR(freq1/freq2));
                bias[i]=L[f]-P[f]+2.0*ion*SQR(freq1/freq3);
            }
            if (rtk->x[j]==0.0||slip[i]||bias[i]==0.0) continue;

            offset+=bias[i]-rtk->x[j];
            k++;
        }
        /* correct phase-code jump to ensure phase-code coherency */
        if (k>=2&&fabs(offset/k)>0.0005*CLIGHT) {
            for (i=0;i<MAXSAT;i++) {
                j=IB(i+1,f,&rtk->opt);
                if (rtk->x[j]!=0.0) rtk->x[j]+=offset/k;
            }
            trace(NULL,2,"phase-code jump corrected: %s n=%2d dt=%12.9fs\n",
                  time_str(rtk->sol.time,0),k,offset/k/CLIGHT);
        }
        for (i=0;i<n&&i<MAXOBS;i++) {
            sat=obs[i].sat;
            j=IB(sat,f,&rtk->opt);

            rtk->P[j+j*rtk->nx]+=SQR(rtk->opt.prn[0])*fabs(rtk->tt);

            if (bias[i]==0.0||(rtk->x[j]!=0.0&&!slip[i])) continue;

            /* reinitialize phase-bias if detecting cycle slip */
            initx(rtk,bias[i],VAR_BIAS,IB(sat,f,&rtk->opt));

            /* reset fix flags */
            for (k=0;k<MAXSAT;k++) rtk->ambc[sat-1].flags[k]=0;

            trace(NULL,5,"udbias_ppp: sat=%2d bias=%.3f\n",sat,bias[i]);
        }
    }
}
/* temporal update of states --------------------------------------------------*/
static void udstate_ppp(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    trace(NULL,3,"udstate_ppp: n=%d\n",n);

    /* temporal update of position */
    udpos_ppp(rtk);

    /* temporal update of clock */
    udclk_ppp(rtk);

    /* temporal update of tropospheric parameters */
    if (rtk->opt.tropopt==TROPOPT_EST||rtk->opt.tropopt==TROPOPT_ESTG) {
        udtrop_ppp(rtk);
    }
    /* temporal update of 3rd/4th-freqency-receiver-bias parameters */
    if (NF(&rtk->opt)>=3) {
        udifb_ppp(rtk);
    }
    /* temporal update of phase-bias */
    udbias_ppp(rtk,obs,n,nav);

    /* temporal update of ionospheric parameters */
    if (rtk->opt.ionoopt==IONOOPT_EST) {
        udiono_ppp(rtk,obs,n,nav);
    }

}
/* satellite antenna phase center variation ----------------------------------*/
static void satantpcv(const double *rs, const double *rr, const pcv_t *pcv,
                      double *dant)
{
    double ru[3],rz[3],eu[3],ez[3],nadir,cosa;
    int i;

    return; /* disable for madoca-ppp */

    for (i=0;i<3;i++) {
        ru[i]=rr[i]-rs[i];
        rz[i]=-rs[i];
    }
    if (!normv3(ru,eu)||!normv3(rz,ez)) return;

    cosa=dot(eu,ez,3);
    cosa=cosa<-1.0?-1.0:(cosa>1.0?1.0:cosa);
    nadir=acos(cosa);

    antmodel_s(pcv,nadir,dant);
}
/* precise tropospheric model ------------------------------------------------*/
static double trop_model_prec(gtime_t time, const double *pos,
                              const double *azel, const double *x, double *dtdx,
                              double *var)
{
    const double zazel[]={0.0,PI/2.0};
    double zhd,m_h,m_w,cotz,grad_n,grad_e;

    /* zenith hydrostatic delay */
    zhd=tropmodel(time,pos,zazel,0.0);

    /* mapping function */
    m_h=tropmapf(time,pos,azel,&m_w);

    if (azel[1]>0.0) {

        /* m_w=m_0+m_0*cot(el)*(Gn*cos(az)+Ge*sin(az)): ref [6] */
        cotz=1.0/tan(azel[1]);
        grad_n=m_w*cotz*cos(azel[0]);
        grad_e=m_w*cotz*sin(azel[0]);
        m_w+=grad_n*x[1]+grad_e*x[2];
        dtdx[1]=grad_n*(x[0]-zhd);
        dtdx[2]=grad_e*(x[0]-zhd);
    }
    dtdx[0]=m_w;
    *var=SQR(0.01);
    return m_h*zhd+m_w*(x[0]-zhd);
}
/* tropospheric model ---------------------------------------------------------*/
static int model_trop(gtime_t time, const double *pos, const double *azel,
                      const prcopt_t *opt, const double *x, double *dtdx,
                      const nav_t *nav, double *dtrp, double *var)
{
    double trp[3]={0};

    if (opt->tropopt==TROPOPT_SAAS) {
        *dtrp=tropmodel(time,pos,azel,REL_HUMI);
        *var=SQR(ERR_SAAS);
        return 1;
    }
    if (opt->tropopt==TROPOPT_SBAS) {
        *dtrp=sbstropcorr(time,pos,azel,var);
        return 1;
    }
    if (opt->tropopt==TROPOPT_EST||opt->tropopt==TROPOPT_ESTG) {
        matcpy(trp,x+IT(opt),opt->tropopt==TROPOPT_EST?1:3,1);
        *dtrp=trop_model_prec(time,pos,azel,trp,dtdx,var);
        return 1;
    }
    return 0;
}
/* ionospheric model ---------------------------------------------------------*/
static int model_iono(gtime_t time, const double *pos, const double *azel,
                      const prcopt_t *opt, int sat, const double *x,
                      const nav_t *nav, double *dion, double *var)
{
    if (opt->ionoopt==IONOOPT_SBAS) {
        return sbsioncorr(time,nav,pos,azel,dion,var);
    }
    if (opt->ionoopt==IONOOPT_TEC) {
        return iontec(time,nav,pos,azel,1,dion,var);
    }
    if (opt->ionoopt==IONOOPT_BRDC) {
        *dion=ionmodel(time,nav->ion_gps,pos,azel);
        *var=SQR(*dion*ERR_BRDCI);
        return 1;
    }
    if (opt->ionoopt==IONOOPT_EST) {
        *dion=x[II(sat,opt)];
        *var=0.0;
        return 1;
    }
    if (opt->ionoopt==IONOOPT_IFLC) {
        *dion=*var=0.0;
        return 1;
    }
    return 0;
}
/* phase and code residuals --------------------------------------------------*/
static int ppp_res(int post, const obsd_t *obs, int n, const double *rs,
                   const double *dts, const double *var_rs, const int *svh,
                   const double *dr, int *exc, const nav_t *nav,
                   const double *x, const double *cov, rtk_t *rtk, double *v, double *H, double *R,
                   double *azel, double *resrms)
{
    prcopt_t *opt=&rtk->opt;
    double y,r,cdtr,bias,C=0.0,rr[3],pos[3],e[3],dtdx[3]={0.0},L[NFREQ],P[NFREQ],Lc,Pc;
    double var[MAXOBS*2*NFREQ],dtrp=0.0,dion=0.0,vart=0.0,vari=0.0,freq,ifb,ifb_std,bias_std,dion_std,dtrp_std;
    double dantr[NFREQPCV]={0},dants[NFREQPCV]={0};
    double ve[MAXOBS*2*NFREQ]={0},vmax=0,resc_rms=0,resp_rms=0,resc_ave=0,resp_ave=0;
    char str[32],satstr[6];
    int ne=0,obsi[MAXOBS*2*NFREQ]={0},frqi[MAXOBS*2*NFREQ],maxobs,maxfrq,rej;
    int i,j,k,sat,sys,nv=0,nx=rtk->nx,stat=1,nresc=0,nresp=0;

    time2str(obs[0].time,str,2);

    for (i=0;i<MAXSAT;i++) for (j=0;j<opt->nf&&j<NFREQ;j++) rtk->ssat[i].vsat[j]=0;

    for (i=0;i<3;i++) rr[i]=x[i]+dr[i];
    ecef2pos(rr,pos);

    for (i=0;i<n&&i<MAXOBS;i++) {
        sat=obs[i].sat;
        satno2id(sat,satstr);

        if ((r=geodist(rs+i*6,rr,e))<=0.0||
            satazel(pos,e,azel+i*2)<opt->elmin) {
            exc[i]=1;
            continue;
        }
        if (!(sys=satsys(sat,NULL))||!rtk->ssat[sat-1].vs||
            satexclude(obs[i].sat,var_rs[i],svh[i],opt)||exc[i]) {
            exc[i]=1;
            continue;
        }
        /* tropospheric and ionospheric model */
        if (!model_trop(obs[i].time,pos,azel+i*2,opt,x,dtdx,nav,&dtrp,&vart)||
            !model_iono(obs[i].time,pos,azel+i*2,opt,sat,x,nav,&dion,&vari)) {
            continue;
        }
        /* receiver antenna model */
        if (opt->posopt[0]) satantpcv(rs+i*6,rr,nav->pcvs+sat-1,dants);
        antmodel(opt->pcvr,opt->antdel[0],azel+i*2,opt->posopt[1],dantr);

        /* phase windup model */
        if (!model_phw(rtk->sol.time,sat,nav->pcvs[sat-1].type,
                       opt->posopt[2]?2:0,rs+i*6,rr,&rtk->ssat[sat-1].phw)) {
            continue;
        }
        /* corrected phase and code measurements */
        if (!corr_meas(obs+i,nav,azel+i*2,&rtk->opt,dantr,dants,
                       rtk->ssat[sat-1].phw,L,P,&Lc,&Pc,post)) {
            continue;
        }
        trace(NULL,post>0?3:4,"ppp_res: %s %s sat=%3d nf=%d\n",str,satstr,sat,NF(opt));

        /* stack phase and code residuals {L1,P1,L2,P2,...} */
        for (j=0;j<2*NF(opt);j++) {

            bias=ifb=0.0;
            bias_std=ifb_std=dion_std=dtrp_std=0.0;

            if (opt->ionoopt==IONOOPT_IFLC) {
                if ((y=j%2==0?Lc:Pc)==0.0) continue;
            }
            else {
                if ((y=j%2==0?L[j/2]:P[j/2])==0.0) continue;

                if ((freq=sat2freq(sat,obs[i].code[j/2],nav))==0.0) continue;
                C=SQR(FREQ1/freq)*(j%2==0?-1.0:1.0);
            }
            for (k=0;k<nx;k++) H[k+nx*nv]=k<3?-e[k]:0.0;

            /* receiver clock */
            switch (satsys_bd2(sat,NULL)) {
                case SYS_GLO: k=1; break;
                case SYS_GAL: k=2; break;
                case SYS_CMP: k=3; break; /* BDS-3 */
                case SYS_IRN: k=4; break;
                case SYS_QZS: k=5; break;
                case SYS_BD2: k=6; break; /* BDS-2 */
                default:      k=0; break;
            }
            cdtr=x[IC(k,opt)];
            H[IC(k,opt)+nx*nv]=1.0;

            if (j/2==2&&j%2==1) { /* 3rd-frequency-receiver-bias */
                ifb=x[I3(k,opt)];
                ifb_std=SQRT(cov[I3(k,opt)+I3(k,opt)*rtk->nx]);
                H[I3(k,opt)+nx*nv]=1.0;
            }
            if (j/2==3&&j%2==1) { /* 4th-frequency-receiver-bias */
                ifb=x[I4(k,opt)];
                ifb_std=SQRT(cov[I4(k,opt)+I4(k,opt)*rtk->nx]);
                H[I4(k,opt)+nx*nv]=1.0;
            }

            if (opt->tropopt==TROPOPT_EST||opt->tropopt==TROPOPT_ESTG) {
                dtrp_std=SQRT(cov[IT(opt)+IT(opt)*rtk->nx]);
                for (k=0;k<(opt->tropopt>=TROPOPT_ESTG?3:1);k++) {
                    H[IT(opt)+k+nx*nv]=dtdx[k];
                }
            }
            if (opt->ionoopt==IONOOPT_EST) {
                if (rtk->x[II(sat,opt)]==0.0) continue;
                dion_std=SQRT(cov[II(sat,opt)+II(sat,opt)*rtk->nx]);
                H[II(sat,opt)+nx*nv]=C;
            }
            if (j%2==0) { /* phase bias */
                if ((bias=x[IB(sat,j/2,opt)])==0.0) continue;
                bias_std=SQRT(cov[IB(obs[i].sat,j/2,opt)+IB(obs[i].sat,j/2,opt)*rtk->nx]);;
                H[IB(sat,j/2,opt)+nx*nv]=1.0;
            }
            /* residual */
            v[nv]=y-(r+cdtr-CLIGHT*dts[i*2]+dtrp+C*dion+bias+ifb);

            if (j%2==0) rtk->ssat[sat-1].resc[j/2]=v[nv];
            else        rtk->ssat[sat-1].resp[j/2]=v[nv];

            /* variance */
            var[nv]=varerr(obs[i].sat,sys,azel[1+i*2],j/2,j%2,opt)+
                    vart+SQR(C)*vari+opt->uraratio*var_rs[i];
            if (sys==SYS_GLO&&j%2==1) var[nv]+=VAR_GLO_IFB;

            trace(NULL,post>0?3:4,"ppp_res: post=%d %s %s sat=%3d %s%s res=%9.4f sig=%9.4f el=%4.1f cdtr=%7.4f "
                             "C=%7.4f dion=%7.4f (%5.2f) dtrp=%7.4f (%5.2f) bias=%9.4f (%5.2f) ifb=%9.4f (%5.2f) fix=%d\n",
                post,str,satstr,sat,j%2?"C":"L",code2obs(obs[i].code[j/2]),v[nv],sqrt(var[nv]),azel[1+i*2]*R2D,
                cdtr,C,dion,dion_std,dtrp,dtrp_std,bias,bias_std,ifb,ifb_std,rtk->ssat[sat-1].fix[j/2]);

            /* reject satellite by pre-fit residuals */
            if (!post&&opt->maxinno>0.0&&fabs(v[nv])>opt->maxinno) {
                trace(NULL,2,"outlier (%d) rejected %s %s sat=%2d %s%s res=%9.4f el=%4.1f\n",
                      post,str,satstr,sat,j%2?"C":"L",code2obs(obs[i].code[j/2]),v[nv],azel[1+i*2]*R2D);
                exc[i]=1; rtk->ssat[sat-1].rejc[j%2]++;
                continue;
            }
            /* record large post-fit residuals */
            if (post&&fabs(v[nv])>sqrt(var[nv])*THRES_REJECT) {
                obsi[ne]=i; frqi[ne]=j; ve[ne]=v[nv]; ne++;
            }
            if (j%2==0) rtk->ssat[sat-1].vsat[j/2]=1;
            if (j%2==0) {
                resc_rms+=(v[nv]*v[nv]);
                resc_ave+=v[nv];
                nresc++;
            }
            else {
                resp_rms+=(v[nv]*v[nv]);
                resp_ave+=v[nv];
                nresp++;
            }
            nv++;
        }
    }
    resp_rms=nresp?sqrt(resp_rms/nresp):0.0;
    resp_ave=nresp?resp_ave/nresp:0.0;
    resc_rms=nresc?sqrt(resc_rms/nresc):0.0;
    resc_ave=nresc?resc_ave/nresc:0.0;
    resrms[0]=resp_rms;
    resrms[1]=resc_rms;
    trace(NULL,2,"ppp_res: post=%d, %s, nresp=, %2d, resp_rms=, %7.3f, resp_ave=, %7.3f, nresc=, %2d, resc_rms=, %7.3f, resc_ave=, %7.3f\n",
                post,str,nresp,resp_rms,resp_ave,nresc,resc_rms,resc_ave);

    /* reject satellite with large and max post-fit residual */
    if (post&&ne>0) {
        vmax=ve[0]; maxobs=obsi[0]; maxfrq=frqi[0]; rej=0;
        for (j=1;j<ne;j++) {
            if (fabs(vmax)>=fabs(ve[j])) continue;
            vmax=ve[j]; maxobs=obsi[j]; maxfrq=frqi[j]; rej=j;
        }
        sat=obs[maxobs].sat;
        satno2id(sat,satstr);
        trace(NULL,2,"outlier (%d) rejected %s %s sat=%2d %s%d res=%9.4f el=%4.1f\n",
            post,str,satstr,sat,maxfrq%2?"P":"L",maxfrq/2+1,vmax,azel[1+maxobs*2]*R2D);
        exc[maxobs]=1; rtk->ssat[sat-1].rejc[maxfrq%2]++; stat=0;
        ve[rej]=0;
    }

    /* constraint to ionospheric correction */
    if(!post && opt->ionocorr) {
        nv+=const_iono_corr(rtk,obs,nav,n,azel,exc,rr,x,v+nv,H+nv*rtk->nx,var+nv);
    }

    for (i=0;i<nv;i++) for (j=0;j<nv;j++) {
        R[i+j*nv]=i==j?var[i]:0.0;
    }
    return post?stat:nv;
}
/* number of estimated states ------------------------------------------------*/
extern int pppnx(const prcopt_t *opt)
{
    return NX(opt);
}
/* update solution status ----------------------------------------------------*/
static void update_stat(rtk_t *rtk, const obsd_t *obs, int n, int stat)
{
    const prcopt_t *opt=&rtk->opt;
    int i,j;

    /* test # of valid satellites */
    rtk->sol.ns=0;
    for (i=0;i<n&&i<MAXOBS;i++) {
        for (j=0;j<opt->nf&&j<NFREQ;j++) {
            if (!rtk->ssat[obs[i].sat-1].vsat[j]) continue;
            rtk->ssat[obs[i].sat-1].lock[j]++;
            rtk->ssat[obs[i].sat-1].outc[j]=0;
            if (j==0) rtk->sol.ns++;
        }
    }

    if (stat==SOLQ_FIX||stat==SOLQ_FIX_WL) {
        for (i=0;i<3;i++) {
            rtk->sol.rr[i]=rtk->xa[i];
            rtk->sol.qr[i]=(float)rtk->Pa[i+i*rtk->na];
        }
        rtk->sol.qr[3]=(float)rtk->Pa[1];
        rtk->sol.qr[4]=(float)rtk->Pa[1+2*rtk->na];
        rtk->sol.qr[5]=(float)rtk->Pa[2];
    }
    else {
        for (i=0;i<3;i++) {
            rtk->sol.rr[i]=rtk->x[i];
            rtk->sol.qr[i]=(float)rtk->P[i+i*rtk->nx];
        }
        rtk->sol.qr[3]=(float)rtk->P[1];
        rtk->sol.qr[4]=(float)rtk->P[2+rtk->nx];
        rtk->sol.qr[5]=(float)rtk->P[2];
    }
    if (stat==SOLQ_FIX_WL) stat=SOLQ_PPP;
    rtk->sol.stat=rtk->sol.ns<MIN_NSAT_SOL?SOLQ_NONE:stat;

    rtk->sol.dtr[0]=rtk->x[IC(0,opt)]/CLIGHT;  /* [s] */
    for (i=1;i<7;i++) {
        rtk->sol.dtr[i]=(rtk->x[IC(i,opt)]-rtk->x[IC(0,opt)])/CLIGHT;
    }

    for (i=0;i<n&&i<MAXOBS;i++) for (j=0;j<opt->nf&&j<NFREQ;j++) {
        rtk->ssat[obs[i].sat-1].snr[j]=obs[i].SNR[j];
    }
    /* update previous phase/time for Doppler slip detection */
    for (i=0;i<n&&i<MAXOBS;i++) for (j=0;j<opt->nf&&j<NFREQ;j++) {
        if (obs[i].L[j]!=0.0) {
            rtk->ssat[obs[i].sat-1].pt[0][j]=obs[i].time;
            rtk->ssat[obs[i].sat-1].ph[0][j]=obs[i].L[j];
        }
    }
    for (i=0;i<MAXSAT;i++) for (j=0;j<opt->nf&&j<NFREQ;j++) {
        if (rtk->ssat[i].slip[j]&3) rtk->ssat[i].slipc[j]++;
    }
}
/* test hold ambiguity -------------------------------------------------------*/
static int test_hold_amb(rtk_t *rtk)
{
    int i,j,stat=0;

    /* no fix-and-hold mode */
    if (rtk->opt.modear!=ARMODE_FIXHOLD) return 0;

    /* reset # of continuous fixed if new ambiguity introduced */
    for (i=0;i<MAXSAT;i++) {
        if (rtk->ssat[i].fix[0]!=2) continue;
        for (j=0;j<MAXSAT;j++) {
            if (rtk->ssat[j].fix[0]!=2) continue;
            if (!rtk->ambc[j].flags[i]||!rtk->ambc[i].flags[j]) stat=1;
            rtk->ambc[j].flags[i]=rtk->ambc[i].flags[j]=1;
        }
    }
    if (stat) {
        rtk->nfix=0;
        return 0;
    }
    /* test # of continuous fixed */
    return ++rtk->nfix>=ROUND(rtk->opt.minfix/fabs(rtk->tt));
}
/* precise point positioning -------------------------------------------------*/
extern void pppos(mrtk_ctx_t *ctx, rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav)
{
    const prcopt_t *opt=&rtk->opt;
    double *rs,*dts,*var,*v,*H,*R,*azel,*xp,*Pp,dr[3]={0},std[3]={0.0};
    double resrms[2]={0},resrms_ar[2]={0};
    char str[32];
    int i,j,nv,info,svh[MAXOBS],exc[MAXOBS]={0},stat=SOLQ_SINGLE,ret;

    time2str(obs[0].time,str,2);
    trace(NULL,3,"pppos   : time=%s nx=%d n=%d\n",str,rtk->nx,n);

    rs=mat(6,n); dts=mat(2,n); var=mat(1,n); azel=zeros(2,n);

    for (i=0;i<MAXSAT;i++) for (j=0;j<opt->nf&&j<NFREQ;j++) rtk->ssat[i].fix[j]=0;

    /* temporal update of ekf states */
    udstate_ppp(rtk,obs,n,nav);

    /* satellite positions and clocks */
    satposs(obs[0].time,obs,n,nav,rtk->opt.sateph,rs,dts,var,svh);

    /* exclude measurements of eclipsing satellite (block IIA) */
    if (rtk->opt.posopt[3]) {
        testeclipse(obs,n,nav,rs);
    }
    /* earth tides correction */
    if (opt->tidecorr) {
        tidedisp(gpst2utc(obs[0].time),rtk->x,opt->tidecorr==1?1:7,&nav->erp,
                 opt->odisp[0],dr);
    }
    nv=n*rtk->opt.nf*2+MAXSAT+3;
    xp=mat(rtk->nx,1); Pp=zeros(rtk->nx,rtk->nx);
    v=mat(nv,1); H=mat(rtk->nx,nv); R=mat(nv,nv);

    for (i=0;i<MAX_ITER;i++) {

        matcpy(xp,rtk->x,rtk->nx,1);
        matcpy(Pp,rtk->P,rtk->nx,rtk->nx);

        /* prefit residuals */
        if (!(nv=ppp_res(0,obs,n,rs,dts,var,svh,dr,exc,nav,xp,Pp,rtk,v,H,R,azel,resrms))) {
            trace(NULL,2,"%s ppp (%d) no valid obs data\n",str,i+1);
            break;
        }
        /* measurement update of ekf states */
        if ((info=filter(xp,Pp,H,v,R,rtk->nx,nv))) {
            trace(NULL,2,"%s ppp (%d) filter error info=%d\n",str,i+1,info);
            break;
        }
        /* postfit residuals */
        if (ppp_res(i+1,obs,n,rs,dts,var,svh,dr,exc,nav,xp,Pp,rtk,v,H,R,azel,resrms)) {
            matcpy(rtk->x,xp,rtk->nx,1);
            matcpy(rtk->P,Pp,rtk->nx,rtk->nx);
            stat=SOLQ_PPP;
            break;
        }
    }
    if (i>=MAX_ITER) {
        trace(NULL,2,"%s ppp (%d) iteration overflows\n",str,i);
    }
    if (stat==SOLQ_PPP) {

        /* ambiguity resolution in ppp */
        if ((ret=ppp_ar(ctx,rtk,obs,n,exc,nav,azel,xp,Pp))&&
            ppp_res(9,obs,n,rs,dts,var,svh,dr,exc,nav,xp,Pp,rtk,v,H,R,azel,resrms_ar)) {

            matcpy(rtk->xa,xp,rtk->nx,1);
            matcpy(rtk->Pa,Pp,rtk->nx,rtk->nx);

            for (i=0;i<3;i++) std[i]=sqrt(Pp[i+i*rtk->nx]);

            if (ret>1) stat=SOLQ_FIX;    /* narrow-lane fixed */
            else       stat=SOLQ_FIX_WL; /* only wide-lane fixed */

            trace(NULL,2,"pppos(%4d): time=%s stat=%d std=%.3f resp=%7.3f -> %7.3f (%6.3f) resc=%7.3f -> %7.3f (%6.3f)\n",__LINE__,str,stat,norm(std,3),
                resrms[0],resrms_ar[0],resrms_ar[0]/resrms[0],resrms[1],resrms_ar[1],resrms_ar[1]/resrms[1]);
        }
        else {
            rtk->nfix=0;
        }
        /* update solution status */
        update_stat(rtk,obs,n,stat);

        /* hold fixed ambiguities */
        if (stat==SOLQ_FIX&&test_hold_amb(rtk)) {
            matcpy(rtk->x,xp,rtk->nx,1);
            matcpy(rtk->P,Pp,rtk->nx,rtk->nx);
            trace(NULL,3,"pppos(%4d): %s hold ambiguity nfix=%4d / %4d\n",__LINE__,str,rtk->nfix,rtk->opt.minfix);
        }
    }
    free(rs); free(dts); free(var); free(azel);
    free(xp); free(Pp); free(v); free(H); free(R);
}
