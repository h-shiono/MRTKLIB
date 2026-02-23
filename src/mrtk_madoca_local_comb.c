/*------------------------------------------------------------------------------
 * mrtk_madoca_local_comb.c : MADOCA local correction data combination
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_madoca_local_comb.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_atmos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* local constants -----------------------------------------------------------*/
#define D2R         (3.1415926535897932/180.0)
#define R2D         (180.0/3.1415926535897932)

/* local macros --------------------------------------------------------------*/
#define GN(gp)      (gp==1?64:16)

/* forward declarations (implemented in rtkcmn.c/stream.c, resolved at link) -*/
extern void trace(int level, const char *format, ...);
extern int strwrite(struct stream_tag *stream, uint8_t *buff, int n);

/* substruct vector ----------------------------------------------------------*/
static void vsub(const double a[3], const double b[3], double c[3])
{
    int i;

    for(i=0; i<3; i++) {
        c[i]=a[i]-b[i];
    }
}
/* equation of the plane -----------------------------------------------------*/
static int plane(const double p1[3], const double p2[3], const double p3[3],
                 double peq[4])
{
    double a2b[3], a2c[3];

    vsub(p2,p1,a2b);
    vsub(p3,p1,a2c);
    cross3(a2b, a2c, peq);
    if(!normv3(peq, peq)) return 0;
    peq[3]=-1*(peq[0]*p1[0]+peq[1]*p1[1]+peq[2]*p1[2]);
    return 1;
}
/* interpolation target ------------------------------------------------------*/
static int intpt(const double p1[3], const double p2[3], const double p3[3],
                 double t[3])
{
    double peq[4];
    if(!plane(p1,p2,p3,peq)) return 0;
    t[2]=-1*(t[0]*peq[0]+t[1]*peq[1]+peq[3])/peq[2];
    return 1;
}
/* get noramal vector z ------------------------------------------------------*/
static double normvz(const double a[3], const double b[3], const double tgt[3])
{
    double v1[3],v2[3];

    vsub(a,b,v1);
    vsub(tgt,a,v2);
    return v1[0]*v2[1]-v1[1]*v2[0];
}
/* select peripheral stations based on distance ------------------------------*/
static int selpersta(const stat_t *stat, const double *llh, int flag,
                     double maxdist, int *id)
{
    int i,j,k,n=0;
    double d;
    double sdist[MAXSITES];
    double ecef[3],tllh[2][3];
    const spos_t *spos;

    for(i=0; i<MAXSITES; i++) {
        sdist[i] = maxdist*1E3;
    }
    pos2ecef(llh,ecef);
    for (i=0; i < (flag==TYPE_TRP ? stat->nst : stat->nsi); i++) {
        if(flag==TYPE_TRP) {
            spos = &stat->strp[i].site;
        }else{
            spos = &stat->sion[i].site;
        }
        if(spos->ecef[0]==0.0) continue;

        d = posdist(spos->ecef,ecef);
        ecef2pos(spos->ecef,tllh[0]);
        ecef2pos(ecef,tllh[1]);
        trace(4,"get_site:targetpos llh=%f,%f,%f site pos llh=%f,%f,%f dist=%f\n",
            tllh[0][0]*R2D,tllh[0][1]*R2D,tllh[0][2],
            tllh[1][0]*R2D,tllh[1][1]*R2D,tllh[1][2],d);
        if(d > maxdist*1E3) continue;

        for (j=0; j<MAXSITES; j++) {
            if(sdist[j] < d) continue;
            for(k=n; k>j&&k>0; k--) {
                sdist[k]=sdist[k-1];
                id[k]=id[k-1];
            }
            sdist[j] = d;
            id[j] = i;
            n++;
            break;
        }
    }
    return n;
}
/* collision detection -------------------------------------------------------*/
static int coldet(const double p1[3], const double p2[3], const double p3[3],
                  const double tgt[3])
{
    double z[3]={0.0};

    z[0]=normvz(p2, p1, tgt);
    z[1]=normvz(p3, p2, tgt);
    z[2]=normvz(p1, p3, tgt);

    if((z[0]>0 && z[1]>0 && z[2]>0) || (z[0]<0 && z[1]<0 && z[2]<0)) {
        return 1;
    }
    return 0;
}
/* get stations triangle (Distance designation) ------------------------------*/
static int gettriangledd(const stat_t *stat, const double *llh, int flag,
                         const double maxdist, const int extp, int *staid)
{
    int i,j,k,stanum=0,ret=0;
    int id[MAXSITES]={0};
    double sllh[3][3]={{0.0}};
    spos_t site[3];

    stanum = selpersta(stat, llh, flag, maxdist, id);
    if(stanum<3) return 0;

    /* detect target including triangle */
    for(i=0; i<stanum && i<MAXSITES; i++) {
        if(flag==TYPE_TRP) {
            memcpy(&site[0],&stat->strp[id[i]].site,sizeof(spos_t));
        }
        else{
            memcpy(&site[0],&stat->sion[id[i]].site,sizeof(spos_t));
        }
        ecef2pos(site[0].ecef, sllh[0]);
        staid[0]=id[i];
        for(j=i+1; j<stanum && j<MAXSITES; j++) {
            if(flag==TYPE_TRP) {
                memcpy(&site[1],&stat->strp[id[j]].site,sizeof(spos_t));
            }
            else{
                memcpy(&site[1],&stat->sion[id[j]].site,sizeof(spos_t));
            }
            ecef2pos(site[1].ecef, sllh[1]);
            staid[1]=id[j];
            for(k=j+1; k<stanum && k<MAXSITES; k++) {
                if(flag==TYPE_TRP) {
                    memcpy(&site[2],&stat->strp[id[k]].site,sizeof(spos_t));
                }
                else{
                    memcpy(&site[2],&stat->sion[id[k]].site,sizeof(spos_t));
                }
                ecef2pos(site[2].ecef, sllh[2]);
                staid[2]=id[k];
                ret = coldet(sllh[0], sllh[1], sllh[2], llh);
                if(ret) {
                    trace(3,"interpolation: %.1f %.1f site=%s %s %s\n",
                        llh[0]*R2D, llh[1]*R2D, site[0].name, site[1].name,
                        site[2].name);
                    return 3;
                }
            }
        }
    }
    /* extrapolation by nearest sites */
    if(extp) {
        for(i=0; i<3; i++) {
            if(flag==TYPE_TRP) {
                memcpy(&site[i],&stat->strp[id[i]].site,sizeof(spos_t));
            }
            else{
                memcpy(&site[i],&stat->sion[id[i]].site,sizeof(spos_t));
            }
            staid[i]=id[i];
        }
        trace(3,"extrapolation: %.1f %.1f site=%s %s %s\n",
            llh[0]*R2D, llh[1]*R2D, site[0].name, site[1].name, site[2].name);
        return 3;
    }
    return 0;
}
/* calculate trop correction using planar interpolation with 3 points --------*/
static int corr_trop_plane(const stat_t *stat, gtime_t time, const double *llh,
                           double maxdist, int extp, double *trp, double *std)
{
    int i,j,staid[3];
    double zazel[]={0.0,90.0*D2R}, zhd, trp_s[3][3]={{0.0}}, std_s[3][3]={{0.0}};
    double tri[9]={0.0}, trit[3];

    if( gettriangledd(stat, llh, TYPE_TRP, maxdist, extp, staid) < 3) return 0;

    /* convert station position & time interpolation */
    for(i=0; i<3; i++) {
        if(get_trop_sta(time,&stat->strp[staid[i]].trpd,trp_s[i],std_s[i])==0) {
            return 0;
        }
        if (trp_s[i][0]==0.0) {
            return 0;
        }
        ecef2pos(stat->strp[staid[i]].site.ecef, &tri[i*3]);
        zhd=tropmodel(time, &tri[i*3], zazel, 0.0);
        trp_s[i][0]-=zhd;
    }

    /* set target position & zenith hydrostatic delay */
    memcpy(trit,llh,sizeof(trit));
    /* space interpolation */
    /* loop [0]ztd -> [1]grade -> [2]gradn */
    for(i=0; i<3; i++) {
        for(j=0; j<3; j++){
            tri[j*3+2]=trp_s[j][i];
        }
        intpt(&tri[0], &tri[3], &tri[6], &trit[0]);
        trp[i] = trit[2];
        for(j=0; j<3; j++){
            tri[j*3+2]=std_s[j][i];
        }
        intpt(&tri[0], &tri[3], &tri[6], &trit[0]);
        std[i] = trit[2];
    }
    trp[0]+=tropmodel(time, llh, zazel, 0.0);
    return 1;
}
/* calculate iono correction using planar interpolation with 3 points --------*/
static int corr_iono_plane(const stat_t *stat, gtime_t time, const double *llh,
                           double maxdist, int extp, double *ion, double *std,
                           double maxres, int *nrej, int *ncnt)
{
    int i, j, siteno[3];
    double el_s[3][MAXSAT], ion_s[3][MAXSAT]={{0.0}}, std_s[3][MAXSAT]={{0.0}};
    double tri[9]={0.0}, trit[3], res;

    if(gettriangledd(stat, llh, TYPE_ION, maxdist, extp, siteno) < 3) return 0;

    /* convert station position & time interpolation */
    for(i=0; i<3; i++) {
        if(get_iono_sta(time, stat->sion[siteno[i]].iond, ion_s[i], std_s[i],
            el_s[i])==0) {
            return 0;
        }
        ecef2pos(stat->sion[siteno[i]].site.ecef, &tri[i*3]);
    }

    /* set target position & zenith hydrostatic delay */
    memcpy(trit,llh,sizeof(trit));
    /* space interpolation */
    for(i=0; i<MAXSAT; i++) {
        for(j=0; j<3; j++) {
            if(ion_s[j][i]==0.0) break;
            tri[j*3+2]=ion_s[j][i];
        }
        if(j < 3) continue;
        intpt(&tri[0], &tri[3], &tri[6], &trit[0]);
        ion[i] = trit[2];
        for(j=0; j<3; j++) {
            if(ion_s[j][i]==0.0) break;
            tri[j*3+2]=std_s[j][i];
        }
        intpt(&tri[0], &tri[3], &tri[6], &trit[0]);
        std[i] = trit[2];
    }
    if(maxres > 0.0){
        for (i=0;i<3;i++) {
            for (j=0;j<MAXSAT;j++) {
                if (ion_s[i][j]==0.0) continue;
                res = ion[i]-ion_s[i][j];
                ncnt[siteno[i]]++;
                if(fabs(res)>maxres) nrej[siteno[i]]++;
            }
        }
    }
    return 1;
}
/* initialize grid/station setting ---------------------------------------------
* initialize grid/station settings from a specified file
* args   : const char *setfile  I    grid/station setting file path
*          lclblock_t *lclblk   IO   local block information struct
*          int btype            I    block type (BTYPE_GRID or BTYPE_STA)
* return : status (0: error, 1: success)
*-----------------------------------------------------------------------------*/
extern int initgridsta(const char *setfile, lclblock_t *lclblk, int btype)
{
    FILE *sfp=NULL;
    char *token,buff[256];
    int *bnum;
    double gp;
    blkinf_t *bi;
    char type[8];

    if ((sfp=fopen(setfile,"r")) == NULL) {
        trace(2,"initgridsta : setting file open error :%s\n", setfile);
        return 0;
    }
    while (fgets(buff,sizeof(buff),sfp)!=NULL) {
        if (buff[0]=='#') continue;

        strcpy(type,strtok(buff,","));
        if (strncmp(type,"I",1)==0) {
            bi=&lclblk->iblkinf[lclblk->inum];
            bnum=&lclblk->inum;
            bi->bs=ION_BLKSIZE;
        }
        else if (strncmp(type,"T",1)==0) {
            bi=&lclblk->tblkinf[lclblk->tnum];
            bnum=&lclblk->tnum;
            bi->bs=TRP_BLKSIZE;
        }else{
            trace(2,"initgrid : grid setting file type error :%s\n", type);
            continue;
        }

        /* init grid/station data */
        bi->btype=btype;
        token=strtok(NULL,",");
        if(token == NULL){
            trace(2,"initgrid : grid setting file format error : bn\n");
            continue;
        }
        bi->bn=atoi(token);
        if(btype==BTYPE_GRID) {
            token=strtok(NULL,",");
            if(token == NULL){
                trace(2,"initgrid : grid setting file format error : gp\n");
                continue;
            }
            gp=atoi(token);
            if (gp==16){
                bi->gpitch=0;
            }
            else if(gp==64){
                bi->gpitch=1;
            }
            else{
                trace(2,"initgrid : grid setting file grid pitch error :%d\n", gp);
                continue;
            }
            token=strtok(NULL,",");
            if(token == NULL){
                trace(2,"initgrid : grid setting file format error : mask[0]\n");
                continue;
            }
            bi->mask[0]=strtol(token, NULL, 0);
            token=strtok(NULL,",");
            if(token == NULL){
                trace(2,"initgrid : grid setting file format error : mask[1]\n");
                continue;
            }
            bi->mask[1]=strtol(token, NULL, 0);
        }

        initblkinf(bi);
        *bnum+=1;
        trace(3,"initgrid : read grid setting file [%s]\n",buff);
    }
    fclose(sfp);

    return 1;
}
/* trop grid interpolation -----------------------------------------------------
* interpolate tropospheric grid data based on the given time and stat data
* args   : gtime_t gt         I    time
*          const stat_t *stat I    stat data
*          lclblock_t *lclblk IO   local block information
*-----------------------------------------------------------------------------*/
extern void grid_intp_trop(const gtime_t gt, const stat_t *stat,
                           lclblock_t *lclblk)
{
    int i,j,n=0;
    blkinf_t *bi;
    trp_t *trpp;

    for (i=0;i<lclblk->tnum;i++) {
        bi=&lclblk->tblkinf[i];
        bi->mask[0]=0;
        bi->mask[1]=0;
        memset(bi->gp,0x00,sizeof(bi->gp));
        bi->n=0;
        for (j=0,n=0; j < GN(bi->gpitch); j++) {
            trpp=&lclblk->tstat[i][j].trpd;
            if(lclblk->interp==MODE_PLANE){
                if (!corr_trop_plane(stat, gt, bi->grid[j], lclblk->tdist,
                    lclblk->extp, trpp->trp, trpp->std)) {
                    continue;
                }
            }
            else{
                if (!corr_trop_distwgt(stat, gt, bi->grid[j], lclblk->tdist,
                    trpp->trp, trpp->std)) {
                    continue;
                }
            }
            trpp->time=gt;
            bi->gp[n]=j;
            n++;
        }
        bi->n=n;
        for (j=0;j<n;j++) {
            if (bi->gp[j]<32) {
                bi->mask[0]|=1<<bi->gp[j];
            }
            else{
                bi->mask[1]|=1<<(bi->gp[j]-32);
            }
        }
    }
}
/* select block tropospher -----------------------------------------------------
* select and assign tropospheric data blocks based on station positions
* args   : gtime_t time       I    time
*          const stat_t *stat I    station data
*          lclblock_t *lslblk IO   local block information struct
*-----------------------------------------------------------------------------*/
extern void sta_sel_trop(const gtime_t time, const stat_t *stat,
                         lclblock_t *lslblk)
{
    int i,j,k;
    double llh[3];
    blkinf_t *bi;
    sitetrp_t *strp;

    for (i=0;i<lslblk->tnum;i++) {
        bi=&lslblk->tblkinf[i];
        bi->n=0;
        for (j=0;j<stat->nst;j++) {
            ecef2pos(stat->strp[j].site.ecef,llh);
            if (llh[0]>=bi->bpos[0])           continue;
            if (llh[0]<bi->bpos[0]-bi->bs*D2R) continue;
            if (llh[1]<=bi->bpos[1])           continue;
            if (llh[1]>bi->bpos[1]+bi->bs*D2R) continue;

            strp=&lslblk->tstat[i][bi->n];
            for (k=0;k<3;k++){
                bi->grid[bi->n][k]=llh[k];
                strp->site.ecef[k]=stat->strp[j].site.ecef[k];
            }
            memcpy(&strp->trpd,&stat->strp[j].trpd,sizeof(trp_t));
            bi->n++;
        }
    }
}
/* ionospheric grid interpolation ----------------------------------------------
* interpolate ionospheric grid data based on the given time and stat data
* args   : gtime_t gt         I    time
*          const stat_t *stat I    stat data
*          lclblock_t *lclblk IO   local block information struct
*          double maxres      I    maximum residual
*          int *nrej          O    number of rejections
*          int *ncnt          O    number of counts
*-----------------------------------------------------------------------------*/
extern void grid_intp_iono(const gtime_t gt, const stat_t *stat,
                           lclblock_t *lclblk, double maxres, int *nrej,
                           int *ncnt)
{
    int i,j,k,n=0;
    double ion[MAXSAT]={0.0},std[MAXSAT]={0.0};
    blkinf_t *bi;
    ion_t *ionp;

    for (i=0;i<lclblk->inum;i++) {
        bi=&lclblk->iblkinf[i];
        bi->mask[0]=0;
        bi->mask[1]=0;
        memset(bi->gp,0x00,sizeof(bi->gp));
        bi->n=0;
        for (j=0,n=0; j < GN(bi->gpitch); j++) {
            if(lclblk->interp==MODE_PLANE) {
                if (!corr_iono_plane(stat, gt, bi->grid[j], lclblk->idist,
                    lclblk->extp, ion, std, maxres, nrej, ncnt)) {
                    continue;
                }
            }
            else {
                if (!corr_iono_distwgt(stat, gt, bi->grid[j], NULL,
                    lclblk->idist, ion, std, maxres, nrej, ncnt)) {
                    continue;
                }
            }
            for(k=0;k<MAXSAT;k++){
                ionp=&lclblk->istat[i][j].iond[k];
                if(ion[k]!=0.0){
                    ionp->time=gt;
                    ionp->ion=ion[k];
                    ionp->std=std[k];
                }
                else{
                    memset(ionp,0x00,sizeof(ion_t));
                }
            }
            bi->gp[n]=j;
            n++;
        }
        bi->n=n;
        for (j=0;j<n;j++) {
            if (bi->gp[j]<32) {
                bi->mask[0]|=1<<bi->gp[j];
            }
            else{
                bi->mask[1]|=1<<(bi->gp[j]-32);
            }
        }
    }
}
/* select block ionosphere -----------------------------------------------------
* select and assign ionospheric data blocks based on stat positions
* args   : gtime_t gt         I    time
*          const stat_t *stat I    stat data
*          lclblock_t *lslblk IO   local block information struct
*-----------------------------------------------------------------------------*/
extern void sta_sel_iono(const gtime_t gt, const stat_t *stat,
                         lclblock_t *lslblk)
{
    int i,j,k;
    double llh[3];
    blkinf_t *bi;
    siteion_t *sion;
    double tt;

    for (i=0;i<lslblk->tnum;i++) {
        bi=&lslblk->iblkinf[i];
        bi->n=0;

        for (j=0;j<stat->nsi;j++) {
            ecef2pos(stat->sion[j].site.ecef,llh);
            if (llh[0]>=bi->bpos[0])           continue;
            if (llh[0]<bi->bpos[0]-bi->bs*D2R) continue;
            if (llh[1]<=bi->bpos[1])           continue;
            if (llh[1]>bi->bpos[1]+bi->bs*D2R) continue;

            sion=&lslblk->istat[i][bi->n];
            for (k=0;k<3;k++){
                bi->grid[bi->n][k]=llh[k];
                sion->site.ecef[k]=stat->strp[j].site.ecef[k];
            }
            for(k=0;k<MAXSAT;k++){
                tt = timediff(stat->sion[j].iond[k].time, sion->iond[k].time);
                if(tt > 0.0){
                    memcpy(&sion->iond[k], &stat->sion[j].iond[k],
                        sizeof(sion->iond[k]));
                }
                else{
                    sion->iond[k].ion=0.0;
                    sion->iond[k].std=0.0;
                }
            }

            bi->n++;
        }
    }
}
/* generate rtcm3 local combination correction ---------------------------------
* generate and output RTCM3 local combination correction messages
* args   : rtcm_t *rtcm    IO   rtcm control struct
*          int btype       I    block type (BTYPE_GRID or BTYPE_STA)
*          stream_t *ostr  I    output stream
*-----------------------------------------------------------------------------*/
extern void output_lclcmb(rtcm_t *rtcm, int btype, struct stream_tag *ostr)
{
    int i, j, basemt;

    if(btype==BTYPE_GRID){
        basemt=2001;
    }
    else if(btype==BTYPE_STA){
        basemt=2011;
    }
    else return;

    for(i=0; i < rtcm->lclblk.tnum; i++) {
        rtcm->lclblk.outtn=i;
        gen_rtcm3(rtcm, basemt, 0, 0);
        strwrite(ostr, rtcm->buff, rtcm->nbyte);
    }

    for(i=0; i < rtcm->lclblk.inum; i++) {
        rtcm->lclblk.outin=i;
        for(j=basemt+1;j<=basemt+5;j++){
            gen_rtcm3(rtcm, j, 0, 0);
            strwrite(ostr, rtcm->buff, rtcm->nbyte);
        }
    }
}
