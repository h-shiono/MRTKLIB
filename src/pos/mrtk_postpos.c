/*------------------------------------------------------------------------------
 * mrtk_postpos.c : post-processing positioning functions
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
/* mrtklib modular headers */
#include "mrtklib/mrtk_postpos.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_sol.h"
#include "mrtklib/mrtk_rtkpos.h"
#include "mrtklib/mrtk_ppp.h"
#include "mrtklib/mrtk_spp.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_sys.h"
#include "mrtklib/mrtk_rinex.h"
#include "mrtklib/mrtk_antenna.h"
#include "mrtklib/mrtk_station.h"
#include "mrtklib/mrtk_tides.h"
#include "mrtklib/mrtk_geoid.h"
#include "mrtklib/mrtk_ionex.h"
#include "mrtklib/mrtk_peph.h"
#include "mrtklib/mrtk_sbas.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_madoca.h"
#include "mrtklib/mrtk_madoca_local_corr.h"
#include "mrtklib/mrtk_clas.h"
#include "mrtklib/mrtk_rcvraw.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "mrtklib/mrtk_trace.h"

/* local constants (duplicated from rtklib.h to avoid rtklib.h dependency) ----*/
#define R2D         (180.0/3.1415926535897932384626433832795)
#define D2R         (3.1415926535897932384626433832795/180.0)

#define COMMENTH    "%"                 /* comment line indicator for solution */

#include "mrtklib/mrtk_version.h"

#ifndef PROGNAME
#define PROGNAME    "rnx2rtkp"          /* default program name */
#endif

#define FILEPATHSEP '/'

/* forward declarations for functions still in legacy rtkcmn.c ----------------*/
extern int  showmsg (const char *format, ...);
extern void settspan(gtime_t ts, gtime_t te);
extern void settime (gtime_t time);

#define MIN(x,y)    ((x)<(y)?(x):(y))
#define SQRT(x)     ((x)<=0.0||(x)!=(x)?0.0:sqrt(x))

#define MAXPRCDAYS  100          /* max days of continuous processing */
#define MAXINFILE   1000         /* max number of input files */

/* constants/global variables ------------------------------------------------*/

static pcvs_t pcvss={0};        /* satellite antenna parameters */
static pcvs_t pcvsr={0};        /* receiver antenna parameters */
static obs_t obss={0};          /* observation data */
static nav_t navs={0};          /* navigation data */
static sbs_t sbss={0};          /* sbas messages */
static sta_t stas[MAXRCV];      /* station information */
static int nepoch=0;            /* number of observation epochs */
static int iobsu =0;            /* current rover observation data index */
static int iobsr =0;            /* current reference observation data index */
static int isbs  =0;            /* current sbas message index */
static int revs  =0;            /* analysis direction (0:forward,1:backward) */
static int aborts=0;            /* abort status */
static sol_t *solf;             /* forward solutions */
static sol_t *solb;             /* backward solutions */
static double *rbf;             /* forward base positions */
static double *rbb;             /* backward base positions */
static int isolf=0;             /* current forward solutions index */
static int isolb=0;             /* current backward solutions index */
static char proc_rov [64]="";   /* rover for current processing */
static char proc_base[64]="";   /* base station for current processing */
static char rtcm_file[1024]=""; /* rtcm data file */
static char rtcm_path[1024]=""; /* rtcm data path */
static rtcm_t rtcm;             /* rtcm control struct */
static rtcm_t l6e;              /* l6e control struct */
static FILE *fp_rtcm=NULL;      /* rtcm data file pointer */
static char qzssl6e_file[1024]="";/* QZSS L6E data file */
static char qzssl6e_path[1024]="";/* QZSS L6E data path */
static FILE *fp_qzssl6e=NULL;     /* QZSS L6E data file pointer */
static mdcl6d_t mdcl6d[MIONO_MAX_PRN]; /* QZSS L6D control struct */
static char qzssl6d_file[MIONO_MAX_PRN][1024]; /* QZSS L6D data file */
static char qzssl6d_path[MIONO_MAX_PRN][1024]; /* QZSS L6D data path */
static FILE *fp_qzssl6d[MIONO_MAX_PRN]; /* QZSS L6D data file pointer */
static char stat_file[1024]=""; /* stat data file */
static char cstat_file[1024]="";/* current stat data file */
static FILE *fp_stat=NULL;      /* stat data file pointer */
static clas_ctx_t *clas_ctx=NULL;                  /* CLAS decoder context */
static FILE *fp_clas[CLAS_CH_NUM];                 /* CLAS L6 file pointers */
static char clas_file[CLAS_CH_NUM][1024];          /* CLAS L6 data file */
static char clas_path[CLAS_CH_NUM][1024];          /* CLAS L6 data path */

/* show message and check break ----------------------------------------------*/
static int checkbrk(const char *format, ...)
{
    va_list arg;
    char buff[1024],*p=buff;
    if (!*format) return showmsg("");
    va_start(arg,format);
    p+=vsprintf(p,format,arg);
    va_end(arg);
    if (*proc_rov&&*proc_base) sprintf(p," (%s-%s)",proc_rov,proc_base);
    else if (*proc_rov ) sprintf(p," (%s)",proc_rov );
    else if (*proc_base) sprintf(p," (%s)",proc_base);
    return showmsg(buff);
}
/* output reference position -------------------------------------------------*/
static void outrpos(FILE *fp, const double *r, const solopt_t *opt)
{
    double pos[3],dms1[3],dms2[3];
    const char *sep=opt->sep;
    
    trace(NULL,3,"outrpos :\n");
    
    if (opt->posf==SOLF_LLH||opt->posf==SOLF_ENU) {
        ecef2pos(r,pos);
        if (opt->degf) {
            deg2dms(pos[0]*R2D,dms1,5);
            deg2dms(pos[1]*R2D,dms2,5);
            fprintf(fp,"%3.0f%s%02.0f%s%08.5f%s%4.0f%s%02.0f%s%08.5f%s%10.4f",
                    dms1[0],sep,dms1[1],sep,dms1[2],sep,dms2[0],sep,dms2[1],
                    sep,dms2[2],sep,pos[2]);
        }
        else {
            fprintf(fp,"%13.9f%s%14.9f%s%10.4f",pos[0]*R2D,sep,pos[1]*R2D,
                    sep,pos[2]);
        }
    }
    else if (opt->posf==SOLF_XYZ) {
        fprintf(fp,"%14.4f%s%14.4f%s%14.4f",r[0],sep,r[1],sep,r[2]);
    }
}
/* output header -------------------------------------------------------------*/
static void outheader(FILE *fp, char **file, int n, const prcopt_t *popt,
                      const solopt_t *sopt)
{
    const char *s1[]={"GPST","UTC","JST"};
    gtime_t ts,te;
    double t1,t2;
    int i,j,w1,w2;
    char s2[32],s3[32];
    
    trace(NULL,3,"outheader: n=%d\n",n);
    
    if (sopt->posf==SOLF_NMEA||sopt->posf==SOLF_STAT) {
        return;
    }
    if (sopt->outhead) {
        if (!*sopt->prog) {
            fprintf(fp,"%s program   : %s(%s ver.%s)\n",COMMENTH,PROGNAME,MRTKLIB_SOFTNAME,MRTKLIB_VERSION_STRING);
        }
        else {
            fprintf(fp,"%s program   : %s\n",COMMENTH,sopt->prog);
        }
        for (i=0;i<n;i++) {
            fprintf(fp,"%s inp file  : %s\n",COMMENTH,file[i]);
        }
        for (i=0;i<obss.n;i++)    if (obss.data[i].rcv==1) break;
        for (j=obss.n-1;j>=0;j--) if (obss.data[j].rcv==1) break;
        if (j<i) {fprintf(fp,"\n%s no rover obs data\n",COMMENTH); return;}
        ts=obss.data[i].time;
        te=obss.data[j].time;
        t1=time2gpst(ts,&w1);
        t2=time2gpst(te,&w2);
        if (sopt->times>=1) ts=gpst2utc(ts);
        if (sopt->times>=1) te=gpst2utc(te);
        if (sopt->times==2) ts=timeadd(ts,9*3600.0);
        if (sopt->times==2) te=timeadd(te,9*3600.0);
        time2str(ts,s2,1);
        time2str(te,s3,1);
        fprintf(fp,"%s obs start : %s %s (week%04d %8.1fs)\n",COMMENTH,s2,s1[sopt->times],w1,t1);
        fprintf(fp,"%s obs end   : %s %s (week%04d %8.1fs)\n",COMMENTH,s3,s1[sopt->times],w2,t2);
    }
    if (sopt->outopt) {
        outprcopt(fp,popt);
    }
    if (PMODE_DGPS<=popt->mode&&popt->mode<=PMODE_FIXED&&popt->mode!=PMODE_MOVEB) {
        fprintf(fp,"%s ref pos   :",COMMENTH);
        outrpos(fp,popt->rb,sopt);
        fprintf(fp,"\n");
    }
    if (sopt->outhead||sopt->outopt) fprintf(fp,"%s\n",COMMENTH);
    
    outsolhead(fp,sopt);
}
/* search next observation data index ----------------------------------------*/
static int nextobsf(const obs_t *obs, int *i, int rcv)
{
    double tt;
    int n;
    
    for (;*i<obs->n;(*i)++) if (obs->data[*i].rcv==rcv) break;
    for (n=0;*i+n<obs->n;n++) {
        tt=timediff(obs->data[*i+n].time,obs->data[*i].time);
        if (obs->data[*i+n].rcv!=rcv||tt>DTTOL) break;
    }
    return n;
}
static int nextobsb(const obs_t *obs, int *i, int rcv)
{
    double tt;
    int n;
    
    for (;*i>=0;(*i)--) if (obs->data[*i].rcv==rcv) break;
    for (n=0;*i-n>=0;n++) {
        tt=timediff(obs->data[*i-n].time,obs->data[*i].time);
        if (obs->data[*i-n].rcv!=rcv||tt<-DTTOL) break;
    }
    return n;
}
/* update rtcm ssr correction ------------------------------------------------*/
static void update_rtcm(gtime_t time)
{
    char path[1024],tstr[32];
    int i;
    
    /* open or swap rtcm file */
    reppath(rtcm_file,path,time,"","");
    
    if (strcmp(path,rtcm_path)) {
        strcpy(rtcm_path,path);
        
        if (fp_rtcm) fclose(fp_rtcm);
        fp_rtcm=fopen(path,"rb");
        if (fp_rtcm) {
            rtcm.time=time;
            input_rtcm3f(&rtcm,fp_rtcm);
            trace(NULL,2,"rtcm file open: %s\n",path);
        }
    }
    if (!fp_rtcm) return;
    
    /* read rtcm file until current time */
    while (timediff(rtcm.time,time)<1E-3) {
        strcpy(tstr,time_str(rtcm.time, 3));
        if (input_rtcm3f(&rtcm,fp_rtcm)<-1) break;
        trace(NULL,3, "update_rtcm: %s %s\n", time_str(time, 3), tstr);
        
        /* update ssr corrections */
        for (i=0;i<MAXSAT;i++) {
            if (!rtcm.ssr[i].update||
                rtcm.ssr[i].iod[0]!=rtcm.ssr[i].iod[1]||
                timediff(time,rtcm.ssr[i].t0[0])<-1E-3) continue;
            navs.ssr_ch[0][i]=rtcm.ssr[i];
            rtcm.ssr[i].update=0;
        }
        /* update iono/trop corrections */
        block2stat(&rtcm, &navs.stat);
    }
}
/* update QZSS L6E MADOCA-PPP corrections ------------------------------------*/
static void update_qzssl6e(gtime_t time)
{
    static int init_flg=1;
    char path[1024],tstr[32];
    int i;

    /* open or swap QZSS L6E file, record bit size is 1744(w/o R-S) or 2000 */
    reppath(qzssl6e_file,path,time,"", "");

    if (strcmp(path, qzssl6e_path)) {
        strcpy(qzssl6e_path, path);

        if (fp_qzssl6e) fclose(fp_qzssl6e);
        fp_qzssl6e=fopen(path, "rb");
        if (fp_qzssl6e) {
            trace(NULL,2, "qzssl6e file open: %s\n", path);
        }
    }
    if (!fp_qzssl6e) return;

    if(init_flg){
        init_mcssr(time);
        init_flg=0;
    }

    while (timediff(l6e.time,time)<1E-3) {
        strcpy(tstr,time_str(l6e.time, 3));
        trace(NULL,3, "update_qzssl6e: %s %s\n", time_str(time, 3), tstr);

        /* update QZSS L6E MADOCA-PPP corrections */
        for (i = 0; i < MAXSAT; i++) {
            if (!l6e.ssr[i].update ||
                l6e.ssr[i].iod[0]!=l6e.ssr[i].iod[1]||
                timediff(time,l6e.ssr[i].t0[0])<-1E-3) continue;
            navs.ssr_ch[0][i]=l6e.ssr[i];
            l6e.ssr[i].update=0;
        }

        if (input_qzssl6ef(&l6e, fp_qzssl6e)<-1) break;
    }
}
/* initialize QZSS L6D control struct ----------------------------------------*/
static void init_mdcl6d(gtime_t time, const char *pppopt)
{
    int i,j,k;
    trace(NULL,2, "init_mdcl6d: time=%s\n",time_str(time, 0));

    for (k=0;k<MIONO_MAX_PRN;k++) {
        strcpy(mdcl6d[k].opt, pppopt);
        mdcl6d[k].time=time;
        mdcl6d[k].nbyte=mdcl6d[k].re.rvalid=0;
        for (i = 0; i < MIONO_MAX_ANUM; i++) {
            mdcl6d[k].re.area[i].avalid=0;
            for (j = 0; j < MAXSAT; j++) {
                mdcl6d[k].re.area[i].sat[j].t0.time=0;
            }
        }
        init_miono(time);
    }
}
/* update QZSS L6D MADOCA-PPP ionospheric corrections ------------------------*/
static void update_qzssl6d(gtime_t time, int n, const char *pppopt)
{
    static int init_flg=1;
    char path[1024],tstr[32];
    int i,j;

    /* open or swap QZSS L6D file */
    reppath(qzssl6d_file[n],path,timeadd(time,-1),"", "");

    if (strcmp(path, qzssl6d_path[n])) {
        strcpy(qzssl6d_path[n], path);

        if (fp_qzssl6d[n]) fclose(fp_qzssl6d[n]);
        fp_qzssl6d[n]=fopen(path, "rb");
        if (fp_qzssl6d[n]) {
            trace(NULL,2, "qzssl6d file open: %s\n", path);
        }
        else {
            trace(NULL,2, "qzssl6d file open error: %s\n", path);
        }
    }
    if (!fp_qzssl6d[n]) return;

    if (init_flg) {
        init_mdcl6d(time,pppopt);
        init_flg=0;
    }
    while (timediff(mdcl6d[n].time,time)<1E-3) {
        strcpy(tstr,time_str(mdcl6d[n].time, 3));
        trace(NULL,3, "update_qzssl6d: %s %s\n", time_str(time, 3), tstr);

        /* update QZSS L6D MADOCA-PPP ionospheric corrections */
        if(mdcl6d[n].re.rvalid) {
            navs.pppiono->re[mdcl6d[n].rid] = mdcl6d[n].re;
            mdcl6d[n].re.rvalid=0;
            for (i = 0; i < MIONO_MAX_ANUM; i++) {
                mdcl6d[n].re.area[i].avalid=0;
                for (j = 0; j < MAXSAT; j++) {
                    mdcl6d[n].re.area[i].sat[j].t0.time=0;
                }
            }
        }

        if (input_qzssl6df(&mdcl6d[n], fp_qzssl6d[n])<-1) break;
    }
}
/* update stat corrections ---------------------------------------------------*/
static void update_stat(gtime_t time)
{
    char path[1024],tstr[32],*p,st[64]="";
    int i,siteno=0;
    static sstat_t sstat={0};
    static char buff[4096];
    static int nbyte=0;

    /* open or swap stat file */
    reppath(stat_file,path,time,"", "");

    if (strcmp(path, cstat_file)!=0) {
        if (fp_stat) fclose(fp_stat);
        fp_stat=fopen(path, "rb");
        if (fp_stat==NULL) {
            trace(NULL,2, "stat file open error: %s\n", path);
        } else {
            strcpy(cstat_file, path);
        }
    }
    if (!fp_stat) return;
    
    /* station name as first 4 character of file */
    if ((p=strrchr(cstat_file,FILEPATHSEP))) strncpy(st,p+1,4);
    else strncpy(st,cstat_file,4);
    st[4]='\0';
    for (i=0;i<4&&st[i];i++) st[i]=toupper(st[i]);
    
    siteno = getstano(&navs.stat,st);

    while (timediff(sstat.time,time)<1E-3) {
        strcpy(tstr,time_str(sstat.time,3));
        trace(NULL,3, "update_stat: %s %s st=%d\n", time_str(time, 3), tstr, siteno);
        
        navs.stat.time[siteno]=sstat.time;
        memcpy(&navs.stat.sion[siteno],&sstat.sion,sizeof(ion_t)*MAXSAT);
        memcpy(&navs.stat.strp[siteno],&sstat.strp,sizeof(trp_t));
        
        if(input_statf(&sstat, fp_stat, buff, &nbyte)<-1) break;
    }
    trace(NULL,3, "update_stat: %s %s\n", time_str(time, 3), tstr);
}
/* update CLAS L6 corrections -----------------------------------------------*/
static void update_clas(gtime_t time)
{
    int ch,ret;
    char path[1024];

    /* initialize GPS week reference from observation time (once) */
    if (clas_ctx && clas_ctx->week_ref[0]==0 && time.time!=0) {
        int i,week;
        time2gpst(time,&week);
        for (i=0;i<CSSR_REF_MAX;i++) {
            clas_ctx->week_ref[i]=week;
            clas_ctx->tow_ref[i]=-1;  /* mark as unset for rollover detection */
        }
        trace(NULL,2,"clas week_ref initialized: week=%d\n",week);
    }

    for (ch=0;ch<CLAS_CH_NUM;ch++) {
        if (!*clas_file[ch]) continue;

        /* open or swap CLAS L6 file */
        reppath(clas_file[ch],path,time,"","");

        if (strcmp(path,clas_path[ch])) {
            strcpy(clas_path[ch],path);
            if (fp_clas[ch]) fclose(fp_clas[ch]);
            fp_clas[ch]=fopen(path,"rb");
            if (fp_clas[ch]) {
                trace(NULL,2,"clas l6 file open: %s\n",path);
            }
        }
        if (!fp_clas[ch]) continue;

        /* read CSSR until current observation time */
        while (timediff(clas_ctx->l6buf[ch].time,time)<1E-3) {
            ret=clas_input_cssrf(clas_ctx,fp_clas[ch],ch);
            if (ret<-1) break; /* EOF */
            if (ret==10) {
                int net=clas_ctx->grid[ch].network;
                if (net>0) {
                    /* normal: merge bank for known network */
                    if (clas_bank_get_close(clas_ctx,clas_ctx->l6buf[ch].time,
                                           net,ch,&clas_ctx->current[ch])==0) {
                        clas_update_global(&navs,&clas_ctx->current[ch],ch);
                        clas_check_grid_status(clas_ctx,&clas_ctx->current[ch],ch);
                    }
                }
            }
        }
        /* bootstrap: when network is unknown, scan all networks to
         * populate grid_stat so clas_get_grid_index() can determine
         * the correct network from the rover position.
         * Uses a heap-allocated temporary corr to avoid corrupting
         * current[ch] and stack overflow (clas_corr_t is ~352KB). */
        if (clas_ctx->grid[ch].network<=0 && clas_ctx->bank[ch]->use) {
            clas_corr_t *tmp_corr=(clas_corr_t *)calloc(1,sizeof(clas_corr_t));
            int net;
            if (tmp_corr) {
                for (net=1;net<CLAS_MAX_NETWORK;net++) {
                    if (clas_bank_get_close(clas_ctx,clas_ctx->l6buf[ch].time,
                                           net,ch,tmp_corr)==0) {
                        clas_check_grid_status(clas_ctx,tmp_corr,ch);
                        /* apply global corrections (orbit/clock/bias) from any
                         * successful network — these are shared across networks */
                        clas_update_global(&navs,tmp_corr,ch);
                    }
                }
                free(tmp_corr);
            }
            trace(NULL,3,"clas bootstrap: scanned %d networks for ch=%d\n",
                  CLAS_MAX_NETWORK-1,ch);
        }
    }
}
/* input obs data, navigation messages and sbas correction -------------------*/
static int inputobs(obsd_t *obs, int solq, const prcopt_t *popt)
{
    gtime_t time={0};
    int i,nu,nr,n=0;
    
    trace(NULL,3,"infunc  : revs=%d iobsu=%d iobsr=%d isbs=%d\n",revs,iobsu,iobsr,isbs);
    
    if (0<=iobsu&&iobsu<obss.n) {
        settime((time=obss.data[iobsu].time));
        if (checkbrk("processing : %s Q=%d",time_str(time,0),solq)) {
            aborts=1; showmsg("aborted"); return -1;
        }
    }
    if (!revs) { /* input forward data */
        if ((nu=nextobsf(&obss,&iobsu,1))<=0) return -1;
        if (popt->intpref) {
            for (;(nr=nextobsf(&obss,&iobsr,2))>0;iobsr+=nr)
                if (timediff(obss.data[iobsr].time,obss.data[iobsu].time)>-DTTOL) break;
        }
        else {
            for (i=iobsr;(nr=nextobsf(&obss,&i,2))>0;iobsr=i,i+=nr)
                if (timediff(obss.data[i].time,obss.data[iobsu].time)>DTTOL) break;
        }
        nr=nextobsf(&obss,&iobsr,2);
        if (nr<=0) {
            nr=nextobsf(&obss,&iobsr,2);
        }
        for (i=0;i<nu&&n<MAXOBS*2;i++) obs[n++]=obss.data[iobsu+i];
        for (i=0;i<nr&&n<MAXOBS*2;i++) obs[n++]=obss.data[iobsr+i];
        iobsu+=nu;
        
        /* update sbas corrections */
        while (isbs<sbss.n) {
            time=gpst2time(sbss.msgs[isbs].week,sbss.msgs[isbs].tow);
            
            if (getbitu(sbss.msgs[isbs].msg,8,6)!=9) { /* except for geo nav */
                sbsupdatecorr(sbss.msgs+isbs,&navs);
            }
            if (timediff(time,obs[0].time)>-1.0-DTTOL) break;
            isbs++;
        }
        /* update rtcm ssr corrections */
        if (*rtcm_file) {
            update_rtcm(obs[0].time);
        }
        /* update QZSS L6E MADOCA-PPP corrections */
        if (*qzssl6e_file) {
            update_qzssl6e(obs[0].time);
        }
        /* update QZSS L6D MADOCA-PPP ionospheric corrections */
        if (popt->ionocorr) {
            int j;
            for (j=0;j<MIONO_MAX_PRN;j++) {
                if (*qzssl6d_file[j]) {
                    update_qzssl6d(obs[0].time, j, popt->pppopt);
                }
            }
        }
        /* update CLAS L6 corrections */
        if (clas_ctx) {
            update_clas(obs[0].time);
        }
        /* update stat corrections */
        if (*stat_file) {
            update_stat(obs[0].time);
        }
    }
    else { /* input backward data */
        if ((nu=nextobsb(&obss,&iobsu,1))<=0) return -1;
        if (popt->intpref) {
            for (;(nr=nextobsb(&obss,&iobsr,2))>0;iobsr-=nr)
                if (timediff(obss.data[iobsr].time,obss.data[iobsu].time)<DTTOL) break;
        }
        else {
            for (i=iobsr;(nr=nextobsb(&obss,&i,2))>0;iobsr=i,i-=nr)
                if (timediff(obss.data[i].time,obss.data[iobsu].time)<-DTTOL) break;
        }
        nr=nextobsb(&obss,&iobsr,2);
        for (i=0;i<nu&&n<MAXOBS*2;i++) obs[n++]=obss.data[iobsu-nu+1+i];
        for (i=0;i<nr&&n<MAXOBS*2;i++) obs[n++]=obss.data[iobsr-nr+1+i];
        iobsu-=nu;
        
        /* update sbas corrections */
        while (isbs>=0) {
            time=gpst2time(sbss.msgs[isbs].week,sbss.msgs[isbs].tow);
            
            if (getbitu(sbss.msgs[isbs].msg,8,6)!=9) { /* except for geo nav */
                sbsupdatecorr(sbss.msgs+isbs,&navs);
            }
            if (timediff(time,obs[0].time)<1.0+DTTOL) break;
            isbs--;
        }
    }
    return n;
}
/* process positioning -------------------------------------------------------*/
static void procpos(mrtk_ctx_t *ctx, FILE *fp, const prcopt_t *popt, const solopt_t *sopt,
                    int mode)
{
    gtime_t time={0};
    sol_t sol={{0}};
    rtk_t rtk;
    obsd_t obs[MAXOBS*2]; /* for rover and base */
    double rb[3]={0};
    int i,nobs,n,solstatic,pri[]={6,1,2,3,4,5,1,6};

    trace(ctx,3,"procpos : mode=%d\n",mode);
    
    solstatic=sopt->solstatic&&
              (popt->mode==PMODE_STATIC||popt->mode==PMODE_PPP_STATIC);
    
    rtkinit(&rtk,popt);
    rtcm_path[0]='\0';
    
    while ((nobs=inputobs(obs,rtk.sol.stat,popt))>=0) {
        
        /* exclude satellites */
        for (i=n=0;i<nobs;i++) {
            if ((satsys(obs[i].sat,NULL)&popt->navsys)&&
                popt->exsats[obs[i].sat-1]!=1) obs[n++]=obs[i];
        }
        if (n<=0) continue;
        
        if (!rtkpos(ctx,&rtk,obs,n,&navs)) continue;

        for (i=0;i<6;i++) rtk.prev_qr[i] = rtk.sol.qr[i];

        if (mode==0) { /* forward/backward */
            if (!solstatic) {
                outsol(fp,&rtk.sol,rtk.rb,sopt);
            }
            else if (time.time==0||pri[rtk.sol.stat]<=pri[sol.stat]) {
                sol=rtk.sol;
                for (i=0;i<3;i++) rb[i]=rtk.rb[i];
                if (time.time==0||timediff(rtk.sol.time,time)<0.0) {
                    time=rtk.sol.time;
                }
            }
        }
        else if (!revs) { /* combined-forward */
            if (isolf>=nepoch) return;
            solf[isolf]=rtk.sol;
            for (i=0;i<3;i++) rbf[i+isolf*3]=rtk.rb[i];
            isolf++;
        }
        else { /* combined-backward */
            if (isolb>=nepoch) return;
            solb[isolb]=rtk.sol;
            for (i=0;i<3;i++) rbb[i+isolb*3]=rtk.rb[i];
            isolb++;
        }
    }
    if (mode==0&&solstatic&&time.time!=0.0) {
        sol.time=time;
        outsol(fp,&sol,rb,sopt);
    }
    rtkfree(&rtk);
}
/* validation of combined solutions ------------------------------------------*/
static int valcomb(const sol_t *solf, const sol_t *solb)
{
    double dr[3],var[3];
    int i;
    char tstr[32];
    
    trace(NULL,3,"valcomb :\n");
    
    /* compare forward and backward solution */
    for (i=0;i<3;i++) {
        dr[i]=solf->rr[i]-solb->rr[i];
        var[i]=solf->qr[i]+solb->qr[i];
    }
    for (i=0;i<3;i++) {
        if (dr[i]*dr[i]<=16.0*var[i]) continue; /* ok if in 4-sigma */
        
        time2str(solf->time,tstr,2);
        trace(NULL,2,"degrade fix to float: %s dr=%.3f %.3f %.3f std=%.3f %.3f %.3f\n",
              tstr+11,dr[0],dr[1],dr[2],SQRT(var[0]),SQRT(var[1]),SQRT(var[2]));
        return 0;
    }
    return 1;
}
/* combine forward/backward solutions and output results ---------------------*/
static void combres(FILE *fp, const prcopt_t *popt, const solopt_t *sopt)
{
    gtime_t time={0};
    sol_t sols={{0}},sol={{0}};
    double tt,Qf[9],Qb[9],Qs[9],rbs[3]={0},rb[3]={0},rr_f[3],rr_b[3],rr_s[3];
    int i,j,k,solstatic,pri[]={0,1,2,3,4,5,1,6};
    
    trace(NULL,3,"combres : isolf=%d isolb=%d\n",isolf,isolb);
    
    solstatic=sopt->solstatic&&
              (popt->mode==PMODE_STATIC||popt->mode==PMODE_PPP_STATIC);
    
    for (i=0,j=isolb-1;i<isolf&&j>=0;i++,j--) {
        
        if ((tt=timediff(solf[i].time,solb[j].time))<-DTTOL) {
            sols=solf[i];
            for (k=0;k<3;k++) rbs[k]=rbf[k+i*3];
            j++;
        }
        else if (tt>DTTOL) {
            sols=solb[j];
            for (k=0;k<3;k++) rbs[k]=rbb[k+j*3];
            i--;
        }
        else if (solf[i].stat<solb[j].stat) {
            sols=solf[i];
            for (k=0;k<3;k++) rbs[k]=rbf[k+i*3];
        }
        else if (solf[i].stat>solb[j].stat) {
            sols=solb[j];
            for (k=0;k<3;k++) rbs[k]=rbb[k+j*3];
        }
        else {
            sols=solf[i];
            sols.time=timeadd(sols.time,-tt/2.0);
            
            if ((popt->mode==PMODE_KINEMA||popt->mode==PMODE_MOVEB)&&
                sols.stat==SOLQ_FIX) {
                
                /* degrade fix to float if validation failed */
                if (!valcomb(solf+i,solb+j)) sols.stat=SOLQ_FLOAT;
            }
            for (k=0;k<3;k++) {
                Qf[k+k*3]=solf[i].qr[k];
                Qb[k+k*3]=solb[j].qr[k];
            }
            Qf[1]=Qf[3]=solf[i].qr[3];
            Qf[5]=Qf[7]=solf[i].qr[4];
            Qf[2]=Qf[6]=solf[i].qr[5];
            Qb[1]=Qb[3]=solb[j].qr[3];
            Qb[5]=Qb[7]=solb[j].qr[4];
            Qb[2]=Qb[6]=solb[j].qr[5];
            
            if (popt->mode==PMODE_MOVEB) {
                for (k=0;k<3;k++) rr_f[k]=solf[i].rr[k]-rbf[k+i*3];
                for (k=0;k<3;k++) rr_b[k]=solb[j].rr[k]-rbb[k+j*3];
                if (smoother(rr_f,Qf,rr_b,Qb,3,rr_s,Qs)) continue;
                for (k=0;k<3;k++) sols.rr[k]=rbs[k]+rr_s[k];
            }
            else {
                if (smoother(solf[i].rr,Qf,solb[j].rr,Qb,3,sols.rr,Qs)) continue;
            }
            sols.qr[0]=(float)Qs[0];
            sols.qr[1]=(float)Qs[4];
            sols.qr[2]=(float)Qs[8];
            sols.qr[3]=(float)Qs[1];
            sols.qr[4]=(float)Qs[5];
            sols.qr[5]=(float)Qs[2];
            
            /* smoother for velocity solution */
            if (popt->dynamics) {
                for (k=0;k<3;k++) {
                    Qf[k+k*3]=solf[i].qv[k];
                    Qb[k+k*3]=solb[j].qv[k];
                }
                Qf[1]=Qf[3]=solf[i].qv[3];
                Qf[5]=Qf[7]=solf[i].qv[4];
                Qf[2]=Qf[6]=solf[i].qv[5];
                Qb[1]=Qb[3]=solb[j].qv[3];
                Qb[5]=Qb[7]=solb[j].qv[4];
                Qb[2]=Qb[6]=solb[j].qv[5];
                if (smoother(solf[i].rr+3,Qf,solb[j].rr+3,Qb,3,sols.rr+3,Qs)) continue;
                sols.qv[0]=(float)Qs[0];
                sols.qv[1]=(float)Qs[4];
                sols.qv[2]=(float)Qs[8];
                sols.qv[3]=(float)Qs[1];
                sols.qv[4]=(float)Qs[5];
                sols.qv[5]=(float)Qs[2];
            }
        }
        if (!solstatic) {
            outsol(fp,&sols,rbs,sopt);
        }
        else if (time.time==0||pri[sols.stat]<=pri[sol.stat]) {
            sol=sols;
            for (k=0;k<3;k++) rb[k]=rbs[k];
            if (time.time==0||timediff(sols.time,time)<0.0) {
                time=sols.time;
            }
        }
    }
    if (solstatic&&time.time!=0.0) {
        sol.time=time;
        outsol(fp,&sol,rb,sopt);
    }
}
/* read prec ephemeris, sbas data, tec grid and open rtcm --------------------*/
static void readpreceph(char **infile, int n, const prcopt_t *prcopt,
                        nav_t *nav, sbs_t *sbs)
{
    seph_t seph0={0};
    int i;
    char *ext;
    
    trace(NULL,2,"readpreceph: n=%d\n",n);
    
    nav->ne=nav->nemax=0;
    nav->nc=nav->ncmax=0;
    sbs->n =sbs->nmax =0;
    
    /* read precise ephemeris files */
    for (i=0;i<n;i++) {
        if (strstr(infile[i],"%r")||strstr(infile[i],"%b")) continue;
        readsp3(infile[i],nav,0);
    }
    /* read precise clock files */
    for (i=0;i<n;i++) {
        if (strstr(infile[i],"%r")||strstr(infile[i],"%b")) continue;
        readrnxc(infile[i],nav);
    }
    /* read sbas message files */
    for (i=0;i<n;i++) {
        if (strstr(infile[i],"%r")||strstr(infile[i],"%b")) continue;
        sbsreadmsg(infile[i],prcopt->sbassatsel,sbs);
    }
    /* allocate sbas ephemeris */
    nav->ns=nav->nsmax=NSATSBS*2;
    if (!(nav->seph=(seph_t *)malloc(sizeof(seph_t)*nav->ns))) {
         showmsg("error : sbas ephem memory allocation");
         trace(NULL,1,"error : sbas ephem memory allocation");
         return;
    }
    for (i=0;i<nav->ns;i++) nav->seph[i]=seph0;
    
    /* set rtcm file and initialize rtcm struct */
    rtcm_file[0]   =rtcm_path[0]   ='\0'; fp_rtcm   =NULL;
    qzssl6e_file[0]=qzssl6e_path[0]='\0'; fp_qzssl6e=NULL;
    for (i=0;i<MIONO_MAX_PRN;i++) {
        qzssl6d_file[i][0]=qzssl6d_path[i][0]='\0'; fp_qzssl6d[i]=NULL;
    }
    for (i=0;i<CLAS_CH_NUM;i++) {
        clas_file[i][0]=clas_path[i][0]='\0'; fp_clas[i]=NULL;
    }
    stat_file[0]   =cstat_file[0]='\0'  ; fp_stat=NULL;

    for (i=0;i<n;i++) {
        if ((ext=strrchr(infile[i],'.'))&&
            (!strcmp(ext,".rtcm3")||!strcmp(ext,".RTCM3"))) {
            strcpy(rtcm_file,infile[i]);
            init_rtcm(&rtcm);
            strcpy(rtcm.opt, prcopt->rtcmopt);
            break;
        }
    }

    /* MADOCA-PPP L6 and CLAS L6 files: discriminate by PRN and mode */
    {
        int nf_l6d=0,nf_clas=0;
        int clas_mode=(prcopt->mode==PMODE_PPP_RTK)||
                      (prcopt->mode>=PMODE_SSR2OSR&&prcopt->mode<=PMODE_SSR2OSR_FIXED)||
                      strstr(prcopt->pppopt,"-clas")!=NULL;

        for (i=0;i<n;i++) {
            if ((ext=strrchr(infile[i],'.'))&&
                (!strcmp(ext,".l6")||!strcmp(ext,".L6"))) {

                /* L6D (PRN=200,201) — always detected */
                if ((ext-infile[i]>=4) &&
                    (!strcmp(ext-4,".200.l6")||!strcmp(ext-4,".200.L6")||
                     !strcmp(ext-4,".201.l6")||!strcmp(ext-4,".201.L6"))) {
                    if (nf_l6d<MIONO_MAX_PRN) strcpy(qzssl6d_file[nf_l6d++],infile[i]);
                }
                /* CLAS: all non-L6D .l6 files in PPP-RTK or -clas mode */
                else if (clas_mode) {
                    if (nf_clas<CLAS_CH_NUM) strcpy(clas_file[nf_clas++],infile[i]);
                }
                else if (!*qzssl6e_file) { /* L6E (first match only) */
                    strcpy(qzssl6e_file,infile[i]);
                    init_rtcm(&l6e);
                    strcpy(l6e.opt, prcopt->rtcmopt);
                }
            }
        }

        /* allocate pppiono if L6D files found */
        if (nf_l6d > 0 && !nav->pppiono) {
            nav->pppiono = (pppiono_t *)calloc(1, sizeof(pppiono_t));
        }

        /* initialize CLAS context if files found */
        if (nf_clas>0&&!clas_ctx) {
            clas_ctx=(clas_ctx_t *)calloc(1,sizeof(clas_ctx_t));
            if (clas_ctx) clas_ctx_init(clas_ctx);
        }
        /* wire CLAS context into nav for PPP-RTK engine access */
        navs.clas_ctx=clas_ctx;
    }

    for (i=0;i<n;i++) {
        if ((ext=strrchr(infile[i],'.'))&&
            (!strcmp(ext,".stat")||!strcmp(ext,".STAT"))) {
            strcpy(stat_file,infile[i]);
            break;
        }
    }
}
/* free prec ephemeris and sbas data -----------------------------------------*/
static void freepreceph(nav_t *nav, sbs_t *sbs)
{
    int i;
    
    trace(NULL,3,"freepreceph:\n");
    
    free(nav->peph); nav->peph=NULL; nav->ne=nav->nemax=0;
    free(nav->pclk); nav->pclk=NULL; nav->nc=nav->ncmax=0;
    free(nav->seph); nav->seph=NULL; nav->ns=nav->nsmax=0;
    free(sbs->msgs); sbs->msgs=NULL; sbs->n =sbs->nmax =0;
    for (i=0;i<nav->nt;i++) {
        free(nav->tec[i].data);
        free(nav->tec[i].rms );
    }
    free(nav->tec ); nav->tec =NULL; nav->nt=nav->ntmax=0;
    
    if (fp_rtcm) fclose(fp_rtcm);
    free_rtcm(&rtcm);
    free_rtcm(&l6e);

    /* free CLAS context */
    if (clas_ctx) {
        for (i=0;i<CLAS_CH_NUM;i++) {
            if (fp_clas[i]) { fclose(fp_clas[i]); fp_clas[i]=NULL; }
        }
        clas_ctx_free(clas_ctx);
        free(clas_ctx);
        clas_ctx=NULL;
    }
}
/* read obs and nav data -----------------------------------------------------*/
static int readobsnav(gtime_t ts, gtime_t te, double ti, char **infile,
                      const int *index, int n, const prcopt_t *prcopt,
                      obs_t *obs, nav_t *nav, sta_t *sta)
{
    int i,j,ind=0,nobs=0,rcv=1;
    
    trace(NULL,3,"readobsnav: ts=%s n=%d\n",time_str(ts,0),n);
    
    obs->data=NULL; obs->n =obs->nmax =0;
    nav->eph =NULL; nav->n =nav->nmax =0;
    nav->geph=NULL; nav->ng=nav->ngmax=0;
    nav->seph=NULL; nav->ns=nav->nsmax=0;
    nepoch=0;
    
    for (i=0;i<n;i++) {
        const char *ext;
        if (checkbrk("")) return 0;

        if (index[i]!=ind) {
            if (obs->n>nobs) rcv++;
            ind=index[i]; nobs=obs->n;
        }
        ext=strrchr(infile[i],'.');
        if (ext&&!strcmp(ext,".bnx")) {
            /* read binex file */
            if (readbnxt(infile[i],rcv,ts,te,ti,prcopt->rnxopt[rcv<=1?0:1],obs,nav,
                         rcv<=2?sta+rcv-1:NULL,prcopt)<0) {
                checkbrk("error : insufficient memory");
                trace(NULL,1,"insufficient memory\n");
                return 0;
            }
        } else {
            /* read rinex obs and nav file */
            if (readrnxt(infile[i],rcv,ts,te,ti,prcopt->rnxopt[rcv<=1?0:1],obs,nav,
                         rcv<=2?sta+rcv-1:NULL)<0) {
                checkbrk("error : insufficient memory");
                trace(NULL,1,"insufficient memory\n");
                return 0;
            }
        }
    }
    if (obs->n<=0) {
        checkbrk("error : no obs data");
        trace(NULL,1,"\n");
        return 0;
    }
    if (nav->n<=0&&nav->ng<=0&&nav->ns<=0) {
        checkbrk("error : no nav data");
        trace(NULL,1,"\n");
        return 0;
    }
    /* sort observation data */
    nepoch=sortobs(obs);
    
    /* delete duplicated ephemeris */
    uniqnav(nav);
    
    /* set time span for progress display */
    if (ts.time==0||te.time==0) {
        for (i=0;   i<obs->n;i++) if (obs->data[i].rcv==1) break;
        for (j=obs->n-1;j>=0;j--) if (obs->data[j].rcv==1) break;
        if (i<j) {
            if (ts.time==0) ts=obs->data[i].time;
            if (te.time==0) te=obs->data[j].time;
            settspan(ts,te);
        }
    }
    return 1;
}
/* free obs and nav data -----------------------------------------------------*/
static void freeobsnav(obs_t *obs, nav_t *nav)
{
    trace(NULL,3,"freeobsnav:\n");
    
    free(obs->data); obs->data=NULL; obs->n =obs->nmax =0;
    free(nav->eph ); nav->eph =NULL; nav->n =nav->nmax =0;
    free(nav->geph); nav->geph=NULL; nav->ng=nav->ngmax=0;
    free(nav->seph); nav->seph=NULL; nav->ns=nav->nsmax=0;
    free(nav->isb ); nav->isb =NULL; nav->ni=nav->nimax=0;
}
/* average of single position ------------------------------------------------*/
static int avepos(mrtk_ctx_t *ctx, double *ra, int rcv, const obs_t *obs, const nav_t *nav,
                  const prcopt_t *opt)
{
    obsd_t data[MAXOBS];
    gtime_t ts={0};
    sol_t sol={{0}};
    int i,j,n=0,m,iobs;
    char msg[128];

    trace(ctx,3,"avepos: rcv=%d obs.n=%d\n",rcv,obs->n);
    
    for (i=0;i<3;i++) ra[i]=0.0;
    
    for (iobs=0;(m=nextobsf(obs,&iobs,rcv))>0;iobs+=m) {
        
        for (i=j=0;i<m&&i<MAXOBS;i++) {
            data[j]=obs->data[iobs+i];
            if ((satsys(data[j].sat,NULL)&opt->navsys)&&
                opt->exsats[data[j].sat-1]!=1) j++;
        }
        if (j<=0||!screent(data[0].time,ts,ts,1.0)) continue; /* only 1 hz */
        
        if (!pntpos(ctx,data,j,nav,opt,&sol,NULL,NULL,msg)) continue;
        
        for (i=0;i<3;i++) ra[i]+=sol.rr[i];
        n++;
    }
    if (n<=0) {
        trace(ctx,1,"no average of base station position\n");
        return 0;
    }
    for (i=0;i<3;i++) ra[i]/=n;
    return 1;
}
/* station position from file ------------------------------------------------*/
static int getstapos(const char *file, char *name, double *r)
{
    FILE *fp;
    char buff[256],sname[256],*p,*q;
    double pos[3];
    
    trace(NULL,3,"getstapos: file=%s name=%s\n",file,name);
    
    if (!(fp=fopen(file,"r"))) {
        trace(NULL,1,"station position file open error: %s\n",file);
        return 0;
    }
    while (fgets(buff,sizeof(buff),fp)) {
        if ((p=strchr(buff,'%'))) *p='\0';
        
        if (sscanf(buff,"%lf %lf %lf %s",pos,pos+1,pos+2,sname)<4) continue;
        
        for (p=sname,q=name;*p&&*q;p++,q++) {
            if (toupper((int)*p)!=toupper((int)*q)) break;
        }
        if (!*p) {
            pos[0]*=D2R;
            pos[1]*=D2R;
            pos2ecef(pos,r);
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    trace(NULL,1,"no station position: %s %s\n",name,file);
    return 0;
}
/* antenna phase center position ---------------------------------------------*/
static int antpos(mrtk_ctx_t *ctx, prcopt_t *opt, int rcvno, const obs_t *obs, const nav_t *nav,
                  const sta_t *sta, const char *posfile)
{
    double *rr=rcvno==1?opt->ru:opt->rb,del[3],pos[3],dr[3]={0};
    int i,postype=rcvno==1?opt->rovpos:opt->refpos;
    char *name;

    trace(ctx,3,"antpos  : rcvno=%d\n",rcvno);
    
    if (postype==POSOPT_SINGLE) { /* average of single position */
        if (!avepos(ctx,rr,rcvno,obs,nav,opt)) {
            showmsg("error : station pos computation");
            return 0;
        }
    }
    else if (postype==POSOPT_FILE) { /* read from position file */
        name=stas[rcvno==1?0:1].name;
        if (!getstapos(posfile,name,rr)) {
            showmsg("error : no position of %s in %s",name,posfile);
            return 0;
        }
    }
    else if (postype==POSOPT_RINEX) { /* get from rinex header */
        if (norm(stas[rcvno==1?0:1].pos,3)<=0.0) {
            showmsg("error : no position in rinex header");
            trace(ctx,1,"no position in rinex header\n");
            return 0;
        }
        /* antenna delta */
        if (stas[rcvno==1?0:1].deltype==0) { /* enu */
            for (i=0;i<3;i++) del[i]=stas[rcvno==1?0:1].del[i];
            del[2]+=stas[rcvno==1?0:1].hgt;
            ecef2pos(stas[rcvno==1?0:1].pos,pos);
            enu2ecef(pos,del,dr);
        }
        else { /* xyz */
            for (i=0;i<3;i++) dr[i]=stas[rcvno==1?0:1].del[i];
        }
        for (i=0;i<3;i++) rr[i]=stas[rcvno==1?0:1].pos[i]+dr[i];
    }
    return 1;
}
/* open procssing session ----------------------------------------------------*/
static int openses(mrtk_ctx_t *ctx, const prcopt_t *popt, const solopt_t *sopt,
                   const filopt_t *fopt, nav_t *nav, pcvs_t *pcvs, pcvs_t *pcvr)
{
    trace(ctx,3,"openses :\n");

    /* read satellite antenna parameters */
    if (*fopt->satantp&&!(readpcv(fopt->satantp,pcvs))) {
        showmsg("error : no sat ant pcv in %s",fopt->satantp);
        trace(ctx,1,"sat antenna pcv read error: %s\n",fopt->satantp);
        return 0;
    }
    /* read receiver antenna parameters */
    if (*fopt->rcvantp&&!(readpcv(fopt->rcvantp,pcvr))) {
        showmsg("error : no rec ant pcv in %s",fopt->rcvantp);
        trace(ctx,1,"rec antenna pcv read error: %s\n",fopt->rcvantp);
        return 0;
    }
    /* open geoid data */
    if (sopt->geoid>0&&*fopt->geoid) {
        if (!opengeoid(sopt->geoid,fopt->geoid)) {
            showmsg("error : no geoid data %s",fopt->geoid);
            trace(ctx,2,"no geoid data %s\n",fopt->geoid);
        }
    }
    return 1;
}
/* close procssing session ---------------------------------------------------*/
static void closeses(mrtk_ctx_t *ctx, nav_t *nav, pcvs_t *pcvs, pcvs_t *pcvr)
{
    trace(ctx,3,"closeses:\n");

    /* free antenna parameters */
    free(pcvs->pcv); pcvs->pcv=NULL; pcvs->n=pcvs->nmax=0;
    free(pcvr->pcv); pcvr->pcv=NULL; pcvr->n=pcvr->nmax=0;

    /* close geoid data */
    closegeoid();

    /* free erp data */
    free(nav->erp.data); nav->erp.data=NULL; nav->erp.n=nav->erp.nmax=0;

    /* close solution statistics and debug trace */
    rtkclosestat();
    traceclose(ctx);
}
/* set antenna parameters ----------------------------------------------------*/
static void setpcv(gtime_t time, prcopt_t *popt, nav_t *nav, const pcvs_t *pcvs,
                   const pcvs_t *pcvr, const sta_t *sta)
{
    pcv_t *pcv,pcv0={0};
    double pos[3],del[3];
    int i,j,mode=PMODE_DGPS<=popt->mode&&popt->mode<=PMODE_FIXED;
    char id[64];
    
    /* set satellite antenna parameters */
    for (i=0;i<MAXSAT;i++) {
        nav->pcvs[i]=pcv0;
        if (!(satsys(i+1,NULL)&popt->navsys)) continue;
        if (!(pcv=searchpcv(i+1,"",time,pcvs))) {
            satno2id(i+1,id);
            trace(NULL,3,"no satellite antenna pcv: %s\n",id);
            continue;
        }
        nav->pcvs[i]=*pcv;
    }
    for (i=0;i<(mode?2:1);i++) {
        popt->pcvr[i]=pcv0;
        if (!strcmp(popt->anttype[i],"*")) { /* set by station parameters */
            strcpy(popt->anttype[i],sta[i].antdes);
            if (sta[i].deltype==1) { /* xyz */
                if (norm(sta[i].pos,3)>0.0) {
                    ecef2pos(sta[i].pos,pos);
                    ecef2enu(pos,sta[i].del,del);
                    for (j=0;j<3;j++) popt->antdel[i][j]=del[j];
                }
            }
            else { /* enu */
                for (j=0;j<3;j++) popt->antdel[i][j]=stas[i].del[j];
            }
        }
        if (!(pcv=searchpcv(0,popt->anttype[i],time,pcvr))) {
            trace(NULL,2,"no receiver antenna pcv: %s\n",popt->anttype[i]);
            *popt->anttype[i]='\0';
            continue;
        }
        strcpy(popt->anttype[i],pcv->type);
        popt->pcvr[i]=*pcv;
    }
}
/* read ocean tide loading parameters ----------------------------------------*/
static void readotl(prcopt_t *popt, const char *file, const sta_t *sta)
{
    int i,mode=PMODE_DGPS<=popt->mode&&popt->mode<=PMODE_FIXED;
    
    for (i=0;i<(mode?2:1);i++) {
        readblq(file,sta[i].name,popt->odisp[i]);
    }
}
/* write header to output file -----------------------------------------------*/
static int outhead(const char *outfile, char **infile, int n,
                   const prcopt_t *popt, const solopt_t *sopt)
{
    FILE *fp=stdout;
    
    trace(NULL,3,"outhead: outfile=%s n=%d\n",outfile,n);
    
    if (*outfile) {
        createdir(outfile);
        
        if (!(fp=fopen(outfile,"wb"))) {
            showmsg("error : open output file %s",outfile);
            return 0;
        }
    }
    /* output header */
    outheader(fp,infile,n,popt,sopt);
    
    if (*outfile) fclose(fp);
    
    return 1;
}
/* open output file for append -----------------------------------------------*/
static FILE *openfile(const char *outfile)
{
    trace(NULL,3,"openfile: outfile=%s\n",outfile);
    
    return !*outfile?stdout:fopen(outfile,"ab");
}
/* execute processing session ------------------------------------------------*/
static int execses(mrtk_ctx_t *ctx, gtime_t ts, gtime_t te, double ti, const prcopt_t *popt,
                   const solopt_t *sopt, const filopt_t *fopt, int flag,
                   char **infile, const int *index, int n, char *outfile)
{
    FILE *fp;
    prcopt_t popt_=*popt;
    char tracefile[1024],statfile[1024],path[1024],*ext;

    trace(ctx,3,"execses : n=%d outfile=%s\n",n,outfile);

    /* open debug trace */
    if (flag&&sopt->trace>0) {
        if (*outfile) {
            strcpy(tracefile,outfile);
            strcat(tracefile,".trace");
        }
        else {
            strcpy(tracefile,fopt->trace);
        }
        traceclose(ctx);
        traceopen(ctx,tracefile);
        tracelevel(ctx,sopt->trace);
    }
    /* read ionosphere data file */
    if (*fopt->iono&&(ext=strrchr(fopt->iono,'.'))) {
        if (strlen(ext)==4&&(ext[3]=='i'||ext[3]=='I')) {
            reppath(fopt->iono,path,ts,"","");
            readtec(path,&navs,1);
        }
    }
    /* read erp data */
    if (*fopt->eop) {
        free(navs.erp.data); navs.erp.data=NULL; navs.erp.n=navs.erp.nmax=0;
        reppath(fopt->eop,path,ts,"","");
        if (!readerp(path,&navs.erp)) {
            showmsg("error : no erp data %s",path);
            trace(ctx,2,"no erp data %s\n",path);
        }
    }
    /* read obs and nav data */
    if (!readobsnav(ts,te,ti,infile,index,n,&popt_,&obss,&navs,stas)) {
        freeobsnav(&obss,&navs);
        return 0;
    }
    /* read dcb parameters */
    if (*fopt->dcb) {
        reppath(fopt->dcb,path,ts,"","");
        readdcb(path,&navs,stas);
    }
    /* set antenna paramters */
    if (popt_.mode!=PMODE_SINGLE) {
        setpcv(obss.n>0?obss.data[0].time:timeget(),&popt_,&navs,&pcvss,&pcvsr,
               stas);
    }
    /* read ocean tide loading parameters */
    if (popt_.mode>PMODE_SINGLE&&*fopt->blq) {
        readotl(&popt_,fopt->blq,stas);
    }
    /* read CLAS grid definition file */
    if (clas_ctx&&*fopt->grid) {
        if (clas_read_grid_def(clas_ctx,fopt->grid)!=0) {
            trace(NULL,1,"clas grid file error: %s\n",fopt->grid);
        }
    }
    /* read ISB correction table */
    if (clas_ctx && popt_.isb == ISBOPT_TABLE && *fopt->isb) {
        readisb(fopt->isb, &navs);
        trace(NULL, 3, "execses: ISB table loaded ni=%d\n", navs.ni);
    }
    /* read L2C phase shift table */
    if (clas_ctx && popt_.phasshft == ISBOPT_TABLE && *fopt->phacyc) {
        readL2C(fopt->phacyc, &navs);
        trace(NULL, 3, "execses: L2C table loaded n=%d\n", navs.sfts.n);
    }
    /* read grid BLQ file for CLAS grid OTL (tidecorr=3) */
    if (clas_ctx && popt_.tidecorr == 3 && *fopt->blq) {
        if (!readblqgrid(fopt->blq, clas_ctx)) {
            trace(NULL, 1, "grid blq file error: %s\n", fopt->blq);
        } else {
            trace(NULL, 3, "execses: grid BLQ loaded from %s\n", fopt->blq);
        }
    }
    /* copy station info to nav and set ISB */
    if (clas_ctx) {
        memcpy(navs.stas, stas, sizeof(sta_t) * MAXRCV);
        if (popt_.isb == ISBOPT_TABLE) {
            setisb(&navs, popt_.rectype[0], popt_.rectype[1],
                   &navs.stas[0], &navs.stas[1]);
        }
    }
    /* rover/reference fixed position */
    if (popt_.mode==PMODE_FIXED) {
        if (!antpos(ctx,&popt_,1,&obss,&navs,stas,fopt->stapos)) {
            freeobsnav(&obss,&navs);
            return 0;
        }
    }
    else if (PMODE_DGPS<=popt_.mode&&popt_.mode<=PMODE_STATIC) {
        if (!antpos(ctx,&popt_,2,&obss,&navs,stas,fopt->stapos)) {
            freeobsnav(&obss,&navs);
            return 0;
        }
    }
    /* open solution statistics */
    if (flag&&sopt->sstat>0) {
        strcpy(statfile,outfile);
        strcat(statfile,".stat");
        rtkclosestat();
        rtkopenstat(statfile,sopt->sstat);
    }
    /* write header to output file */
    if (flag&&!outhead(outfile,infile,n,&popt_,sopt)) {
        freeobsnav(&obss,&navs);
        return 0;
    }
    iobsu=iobsr=isbs=revs=aborts=0;
    
    /* check and set station name */
    if(strlen(popt_.staname)==0){
        strncpy(popt_.staname,stas[0].name,sizeof(popt_.staname));
        popt_.staname[sizeof(popt_.staname)-1]='\0';
    }
    
    if (popt_.mode==PMODE_SINGLE||popt_.soltype==0) {
        if ((fp=openfile(outfile))) {
            procpos(ctx,fp,&popt_,sopt,0); /* forward */
            fclose(fp);
        }
    }
    else if (popt_.soltype==1) {
        if ((fp=openfile(outfile))) {
            revs=1; iobsu=iobsr=obss.n-1; isbs=sbss.n-1;
            procpos(ctx,fp,&popt_,sopt,0); /* backward */
            fclose(fp);
        }
    }
    else { /* combined */
        solf=(sol_t *)malloc(sizeof(sol_t)*nepoch);
        solb=(sol_t *)malloc(sizeof(sol_t)*nepoch);
        rbf=(double *)malloc(sizeof(double)*nepoch*3);
        rbb=(double *)malloc(sizeof(double)*nepoch*3);

        if (solf&&solb) {
            isolf=isolb=0;
            procpos(ctx,NULL,&popt_,sopt,1); /* forward */
            revs=1; iobsu=iobsr=obss.n-1; isbs=sbss.n-1;
            procpos(ctx,NULL,&popt_,sopt,1); /* backward */
            
            /* combine forward/backward solutions */
            if (!aborts&&(fp=openfile(outfile))) {
                combres(fp,&popt_,sopt);
                fclose(fp);
            }
        }
        else showmsg("error : memory allocation");
        free(solf);
        free(solb);
        free(rbf);
        free(rbb);
    }
    /* free obs and nav data */
    freeobsnav(&obss,&navs);
    
    return aborts?1:0;
}
/* execute processing session for each rover ---------------------------------*/
static int execses_r(mrtk_ctx_t *ctx, gtime_t ts, gtime_t te, double ti, const prcopt_t *popt,
                     const solopt_t *sopt, const filopt_t *fopt, int flag,
                     char **infile, const int *index, int n, char *outfile,
                     const char *rov)
{
    gtime_t t0={0};
    int i,stat=0;
    char *ifile[MAXINFILE],ofile[1024],*rov_,*p,*q,s[64]="";

    trace(ctx,3,"execses_r: n=%d outfile=%s\n",n,outfile);
    
    for (i=0;i<n;i++) if (strstr(infile[i],"%r")) break;
    
    if (i<n) { /* include rover keywords */
        if (!(rov_=(char *)malloc(strlen(rov)+1))) return 0;
        strcpy(rov_,rov);
        
        for (i=0;i<n;i++) {
            if (!(ifile[i]=(char *)malloc(1024))) {
                free(rov_); for (;i>=0;i--) free(ifile[i]);
                return 0;
            }
        }
        for (p=rov_;;p=q+1) { /* for each rover */
            if ((q=strchr(p,' '))) *q='\0';
            
            if (*p) {
                strcpy(proc_rov,p);
                if (ts.time) time2str(ts,s,0); else *s='\0';
                if (checkbrk("reading    : %s",s)) {
                    stat=1;
                    break;
                }
                for (i=0;i<n;i++) reppath(infile[i],ifile[i],t0,p,"");
                reppath(outfile,ofile,t0,p,"");
                
                /* execute processing session */
                stat=execses(ctx,ts,te,ti,popt,sopt,fopt,flag,ifile,index,n,ofile);
            }
            if (stat==1||!q) break;
        }
        free(rov_); for (i=0;i<n;i++) free(ifile[i]);
    }
    else {
        /* execute processing session */
        stat=execses(ctx,ts,te,ti,popt,sopt,fopt,flag,infile,index,n,outfile);
    }
    return stat;
}
/* execute processing session for each base station --------------------------*/
static int execses_b(mrtk_ctx_t *ctx, gtime_t ts, gtime_t te, double ti, const prcopt_t *popt,
                     const solopt_t *sopt, const filopt_t *fopt, int flag,
                     char **infile, const int *index, int n, char *outfile,
                     const char *rov, const char *base)
{
    gtime_t t0={0};
    int i,stat=0;
    char *ifile[MAXINFILE],ofile[1024],*base_,*p,*q,s[64];

    trace(ctx,3,"execses_b: n=%d outfile=%s\n",n,outfile);
    
    /* read prec ephemeris and sbas data */
    readpreceph(infile,n,popt,&navs,&sbss);
    
    for (i=0;i<n;i++) if (strstr(infile[i],"%b")) break;
    
    if (i<n) { /* include base station keywords */
        if (!(base_=(char *)malloc(strlen(base)+1))) {
            freepreceph(&navs,&sbss);
            return 0;
        }
        strcpy(base_,base);
        
        for (i=0;i<n;i++) {
            if (!(ifile[i]=(char *)malloc(1024))) {
                free(base_); for (;i>=0;i--) free(ifile[i]);
                freepreceph(&navs,&sbss);
                return 0;
            }
        }
        for (p=base_;;p=q+1) { /* for each base station */
            if ((q=strchr(p,' '))) *q='\0';
            
            if (*p) {
                strcpy(proc_base,p);
                if (ts.time) time2str(ts,s,0); else *s='\0';
                if (checkbrk("reading    : %s",s)) {
                    stat=1;
                    break;
                }
                for (i=0;i<n;i++) reppath(infile[i],ifile[i],t0,"",p);
                reppath(outfile,ofile,t0,"",p);
                
                stat=execses_r(ctx,ts,te,ti,popt,sopt,fopt,flag,ifile,index,n,ofile,rov);
            }
            if (stat==1||!q) break;
        }
        free(base_); for (i=0;i<n;i++) free(ifile[i]);
    }
    else {
        stat=execses_r(ctx,ts,te,ti,popt,sopt,fopt,flag,infile,index,n,outfile,rov);
    }
    /* free prec ephemeris and sbas data */
    freepreceph(&navs,&sbss);
    
    return stat;
}
/* post-processing positioning -------------------------------------------------
* post-processing positioning
* args   : gtime_t ts       I   processing start time (ts.time==0: no limit)
*        : gtime_t te       I   processing end time   (te.time==0: no limit)
*          double ti        I   processing interval  (s) (0:all)
*          double tu        I   processing unit time (s) (0:all)
*          prcopt_t *popt   I   processing options
*          solopt_t *sopt   I   solution options
*          filopt_t *fopt   I   file options
*          char   **infile  I   input files (see below)
*          int    n         I   number of input files
*          char   *outfile  I   output file ("":stdout, see below)
*          char   *rov      I   rover id list        (separated by " ")
*          char   *base     I   base station id list (separated by " ")
* return : status (0:ok,0>:error,1:aborted)
* notes  : input files should contain observation data, navigation data, precise 
*          ephemeris/clock (optional), sbas log file (optional), ssr message
*          log file (optional) and tec grid file (optional). only the first 
*          observation data file in the input files is recognized as the rover
*          data.
*
*          the type of an input file is recognized by the file extention as ]
*          follows:
*              .sp3,.SP3,.eph*,.EPH*: precise ephemeris (sp3c)
*              .sbs,.SBS,.ems,.EMS  : sbas message log files (rtklib or ems)
*              .rtcm3,.RTCM3        : ssr message log files (rtcm3)
*              .*i,.*I              : tec grid files (ionex)
*              others               : rinex obs, nav, gnav, hnav, qnav or clock
*
*          inputs files can include wild-cards (*). if an file includes
*          wild-cards, the wild-card expanded multiple files are used.
*
*          inputs files can include keywords. if an file includes keywords,
*          the keywords are replaced by date, time, rover id and base station
*          id and multiple session analyses run. refer reppath() for the
*          keywords.
*
*          the output file can also include keywords. if the output file does
*          not include keywords. the results of all multiple session analyses
*          are output to a single output file.
*
*          ssr corrections are valid only for forward estimation.
*-----------------------------------------------------------------------------*/
extern int postpos(mrtk_ctx_t *ctx, gtime_t ts, gtime_t te, double ti, double tu,
                   const prcopt_t *popt, const solopt_t *sopt,
                   const filopt_t *fopt, char **infile, int n, char *outfile,
                   const char *rov, const char *base)
{
    gtime_t tts,tte,ttte;
    double tunit,tss;
    int i,j,k,nf,stat=0,week,flag=1,index[MAXINFILE]={0};
    char *ifile[MAXINFILE],ofile[1024],*ext;

    trace(ctx,3,"postpos : ti=%.0f tu=%.0f n=%d outfile=%s\n",ti,tu,n,outfile);
    
    /* open processing session */
    if (!openses(ctx,popt,sopt,fopt,&navs,&pcvss,&pcvsr)) return -1;
    
    if (ts.time!=0&&te.time!=0&&tu>=0.0) {
        if (timediff(te,ts)<0.0) {
            showmsg("error : no period");
            closeses(ctx,&navs,&pcvss,&pcvsr);
            return 0;
        }
        for (i=0;i<MAXINFILE;i++) {
            if (!(ifile[i]=(char *)malloc(1024))) {
                for (;i>=0;i--) free(ifile[i]);
                closeses(ctx,&navs,&pcvss,&pcvsr);
                return -1;
            }
        }
        if (tu==0.0||tu>86400.0*MAXPRCDAYS) tu=86400.0*MAXPRCDAYS;
        settspan(ts,te);
        tunit=tu<86400.0?tu:86400.0;
        tss=tunit*(int)floor(time2gpst(ts,&week)/tunit);
        
        for (i=0;;i++) { /* for each periods */
            tts=gpst2time(week,tss+i*tu);
            tte=timeadd(tts,tu-DTTOL);
            if (timediff(tts,te)>0.0) break;
            if (timediff(tts,ts)<0.0) tts=ts;
            if (timediff(tte,te)>0.0) tte=te;
            
            strcpy(proc_rov ,"");
            strcpy(proc_base,"");
            if (checkbrk("reading    : %s",time_str(tts,0))) {
                stat=1;
                break;
            }
            for (j=k=nf=0;j<n;j++) {
                
                ext=strrchr(infile[j],'.');
                
                if (ext&&(!strcmp(ext,".rtcm3")||!strcmp(ext,".RTCM3")||
                          !strcmp(ext,".l6")   ||!strcmp(ext,".L6"))) {
                    strcpy(ifile[nf++],infile[j]);
                }
                else {
                    /* include next day precise ephemeris or rinex brdc nav */
                    ttte=tte;
                    if (ext&&(!strcmp(ext,".sp3")||!strcmp(ext,".SP3")||
                              !strcmp(ext,".eph")||!strcmp(ext,".EPH"))) {
                        ttte=timeadd(ttte,3600.0);
                    }
                    else if (strstr(infile[j],"brdc")) {
                        ttte=timeadd(ttte,7200.0);
                    }
                    nf+=reppaths(infile[j],ifile+nf,MAXINFILE-nf,tts,ttte,"","");
                }
                if (ext&&(!strcmp(ext,".bia")||!strcmp(ext,".BIA"))) {
                    strcpy(navs.biapath,infile[j]);
                }
                if (ext&&(!strcmp(ext,".fcb")||!strcmp(ext,".FCB"))) {
                    strcpy(navs.fcbpath,infile[j]);
                }
                while (k<nf) index[k++]=j;
                
                if (nf>=MAXINFILE) {
                    trace(ctx,2,"too many input files. trancated\n");
                    break;
                }
            }
            if (!reppath(outfile,ofile,tts,"","")&&i>0) flag=0;
            
            if(strlen(navs.biapath)==0 && strlen(fopt->bia)>0){
                strcpy(navs.biapath,fopt->bia);
            }
            if(strlen(navs.fcbpath)==0 && strlen(fopt->fcb)>0){
                strcpy(navs.fcbpath,fopt->fcb);
            }
            /* execute processing session */
            stat=execses_b(ctx,tts,tte,ti,popt,sopt,fopt,flag,ifile,index,nf,ofile,
                           rov,base);

            if (stat==1) break;
        }
        for (i=0;i<MAXINFILE;i++) free(ifile[i]);
    }
    else if (ts.time!=0) {
        for (i=0;i<n&&i<MAXINFILE;i++) {
            if (!(ifile[i]=(char *)malloc(1024))) {
                for (;i>=0;i--) free(ifile[i]);
                return -1;
            }
            reppath(infile[i],ifile[i],ts,"","");
            index[i]=i;
        }
        reppath(outfile,ofile,ts,"","");
        
        /* execute processing session */
        stat=execses_b(ctx,ts,te,ti,popt,sopt,fopt,1,ifile,index,n,ofile,rov,
                       base);

        for (i=0;i<n&&i<MAXINFILE;i++) free(ifile[i]);
    }
    else {
        for (i=0;i<n;i++) index[i]=i;
        
        /* execute processing session */
        stat=execses_b(ctx,ts,te,ti,popt,sopt,fopt,1,infile,index,n,outfile,rov,
                       base);
    }
    /* close processing session */
    closeses(ctx,&navs,&pcvss,&pcvsr);
    
    return stat;
}
