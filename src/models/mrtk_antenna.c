/*------------------------------------------------------------------------------
 * mrtk_antenna.c : antenna model and PCV functions
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
 * @file mrtk_antenna.c
 * @brief MRTKLIB Antenna Module — Implementation.
 *
 * All functions and static data in this file are cut-and-paste extractions
 * from the legacy rtkcmn.c with zero algorithmic changes.
 */

#include "mrtklib/mrtk_antenna.h"
#include "mrtklib/mrtk_mat.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mrtklib/mrtk_trace.h"

/*============================================================================
 * Forward Declarations (resolved at link time)
 *===========================================================================*/

/* satid2no remains in rtkcmn.c */
extern int satid2no(const char *id);
extern int satsys(int sat, int *prn);
extern int freq_num2ant_idx(int sys, int freq_num);

/*============================================================================
 * Private Constants
 *===========================================================================*/

#define PI          3.1415926535897932  /* pi */
#define D2R         (PI/180.0)          /* deg to rad */
#define R2D         (180.0/PI)          /* rad to deg */

/*============================================================================
 * Private Functions
 *===========================================================================*/

/* decode antenna parameter field --------------------------------------------*/
static int decodef(char *p, int n, double *v)
{
    int i;

    for (i=0;i<n;i++) v[i]=0.0;
    for (i=0,p=strtok(p," ");p&&i<n;p=strtok(NULL," ")) {
        v[i++]=atof(p)*1E-3;
    }
    return i;
}
/* add antenna parameter -----------------------------------------------------*/
static void addpcv(const pcv_t *pcv, pcvs_t *pcvs)
{
    pcv_t *pcvs_pcv;

    if (pcvs->nmax<=pcvs->n) {
        pcvs->nmax+=256;
        if (!(pcvs_pcv=(pcv_t *)realloc(pcvs->pcv,sizeof(pcv_t)*pcvs->nmax))) {
            trace(NULL,1,"addpcv: memory allocation error\n");
            free(pcvs->pcv); pcvs->pcv=NULL; pcvs->n=pcvs->nmax=0;
            return;
        }
        pcvs->pcv=pcvs_pcv;
    }
    pcvs->pcv[pcvs->n++]=*pcv;
}
/* read ngs antenna parameter file -------------------------------------------*/
static int readngspcv(const char *file, pcvs_t *pcvs)
{
    FILE *fp;
    static const pcv_t pcv0={0};
    pcv_t pcv;
    double neu[3];
    int n=0;
    char buff[256];

    if (!(fp=fopen(file,"r"))) {
        trace(NULL,2,"ngs pcv file open error: %s\n",file);
        return 0;
    }
    while (fgets(buff,sizeof(buff),fp)) {

        if (strlen(buff)>=62&&buff[61]=='|') continue;

        if (buff[0]!=' ') n=0; /* start line */
        if (++n==1) {
            pcv=pcv0;
            sprintf(pcv.type,"%.61s",buff);
        }
        else if (n==2) {
            if (decodef(buff,3,neu)<3) continue;
            pcv.off[0][0]=neu[1];
            pcv.off[0][1]=neu[0];
            pcv.off[0][2]=neu[2];
        }
        else if (n==3) decodef(buff,10,pcv.var[0]);
        else if (n==4) decodef(buff,9,pcv.var[0]+10);
        else if (n==5) {
            if (decodef(buff,3,neu)<3) continue;;
            pcv.off[1][0]=neu[1];
            pcv.off[1][1]=neu[0];
            pcv.off[1][2]=neu[2];
        }
        else if (n==6) decodef(buff,10,pcv.var[1]);
        else if (n==7) {
            decodef(buff,9,pcv.var[1]+10);
            addpcv(&pcv,pcvs);
        }
    }
    fclose(fp);

    return 1;
}
/* read antex file (madocalib version with freq_num2ant_idx) -----------------*/
static int readantex(const char *file, pcvs_t *pcvs)
{
    FILE *fp;
    static const pcv_t pcv0={0};
    pcv_t pcv;
    double neu[3];
    int i,f,idx=-1,state=0,sys;
    char buff[256],s=0;

    trace(NULL,3,"readantex: file=%s\n",file);

    if (!(fp=fopen(file,"r"))) {
        trace(NULL,2,"antex pcv file open error: %s\n",file);
        return 0;
    }
    while (fgets(buff,sizeof(buff),fp)) {

        if (strlen(buff)<60||strstr(buff+60,"COMMENT")) continue;

        if (strstr(buff+60,"START OF ANTENNA")) {
            pcv=pcv0;
            state=1;
        }
        if (strstr(buff+60,"END OF ANTENNA")) {
            addpcv(&pcv,pcvs);
            state=0;
        }
        if (!state) continue;

        if (strstr(buff+60,"TYPE / SERIAL NO")) {
            strncpy(pcv.type,buff   ,20); pcv.type[20]='\0';
            strncpy(pcv.code,buff+20,20); pcv.code[20]='\0';
            if (!strncmp(pcv.code+3,"        ",8)) {
                pcv.sat=satid2no(pcv.code);
            }
        }
        else if (strstr(buff+60,"VALID FROM")) {
            if (!str2time(buff,0,43,&pcv.ts)) continue;
        }
        else if (strstr(buff+60,"VALID UNTIL")) {
            if (!str2time(buff,0,43,&pcv.te)) continue;
        }
        else if (strstr(buff+60,"START OF FREQUENCY")) {
            if (pcv.sat) { /* for satellite */
                if (sscanf(buff+4,"%d",&f)<1) continue;
                s=0;
                sys=satsys(pcv.sat,NULL);
            }
            else { /* for station */
                if (sscanf(buff+3,"%c%d",&s,&f)<2) continue;
                sys=satsys(satid2no(buff+3),NULL);
            }
            if ((idx=freq_num2ant_idx(sys,f))>=NFREQPCV) continue;
        }
        else if (strstr(buff+60,"END OF FREQUENCY")) {
            idx=-1;
        }
        else if (strstr(buff+60,"NORTH / EAST / UP")) {
            if (idx<0||(NFREQPCV-1)<=idx) continue;
            if (decodef(buff,3,neu)<3) continue;
            pcv.off[idx][0]=neu[pcv.sat?0:1]; /* x or e */
            pcv.off[idx][1]=neu[pcv.sat?1:0]; /* y or n */
            pcv.off[idx][2]=neu[2];           /* z or u */
        }
        else if (strstr(buff,"NOAZI")) {
            if (idx<0||NFREQPCV<=idx) continue;
            if ((i=decodef(buff+8,19,pcv.var[idx]))<=0) continue;
            for (;i<19;i++) pcv.var[idx][i]=pcv.var[idx][i-1];
        }
    }
    fclose(fp);

    return 1;
}
/* interpolate antenna phase center variation --------------------------------*/
static double interpvar(double ang, const double *var)
{
    double a=ang/5.0; /* ang=0-90 */
    int i=(int)a;
    if (i<0) return var[0]; else if (i>=18) return var[18];
    return var[i]*(1.0-a+i)+var[i+1]*(a-i);
}

/*============================================================================
 * Public Functions
 *===========================================================================*/

/* read antenna parameters ------------------------------------------------------
* read antenna parameters
* args   : char   *file       I   antenna parameter file (antex)
*          pcvs_t *pcvs       IO  antenna parameters
* return : status (1:ok,0:file open error)
* notes  : file with the externsion .atx or .ATX is recognized as antex
*          file except for antex is recognized ngs antenna parameters
*          see reference [3]
*          only support non-azimuth-depedent parameters
*-----------------------------------------------------------------------------*/
extern int readpcv(const char *file, pcvs_t *pcvs)
{
    pcv_t *pcv;
    char *ext;
    int i,f,stat;

    trace(NULL,3,"readpcv: file=%s\n",file);

    if (!(ext=strrchr(file,'.'))) ext="";

    if (!strcmp(ext,".pcv")||!strcmp(ext,".PCV")) {
        stat=readngspcv(file,pcvs);
    }
    else {
        stat=readantex(file,pcvs);
    }

    /* use G02 if GPS IIF */
    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        if (pcv->sat==0) continue;
        if (!strstr(pcv->type,"BLOCK IIF" )) continue;
        if (norm(pcv->off[2],3)>0.0||
            norm(pcv->var[2],19)>0.0) continue;
        matcpy(pcv->off[2],pcv->off[1], 3,1);
        matcpy(pcv->var[2],pcv->var[1],19,1);
    }
    /* antenna index=2 : use G02 if no G05 E05 J05 C05 S05 I05 */
    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        if (pcv->sat>0) continue;
        if (norm(pcv->off[2],3)>0.0||
            norm(pcv->var[2],19)>0.0) continue;
        matcpy(pcv->off[2],pcv->off[1], 3,1);
        matcpy(pcv->var[2],pcv->var[1],19,1);
    }
    /* antenna index=3 : use G01 if no R01 R04 */
    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        if (pcv->sat>0) continue;
        if (norm(pcv->off[3],3)>0.0||
            norm(pcv->var[3],19)>0.0) continue;
        matcpy(pcv->off[3],pcv->off[0], 3,1);
        matcpy(pcv->var[3],pcv->var[0],19,1);
    }
    /* antenna index=4 : use G01 if no C02 */
    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        if (pcv->sat>0) continue;
        if (norm(pcv->off[4],3)>0.0||
            norm(pcv->var[4],19)>0.0) continue;
        matcpy(pcv->off[4],pcv->off[0], 3,1);
        matcpy(pcv->var[4],pcv->var[0],19,1);
    }
    /* antenna index=5 : use G02 if no E06 J06 */
    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        if (pcv->sat>0) continue;
        if (norm(pcv->off[5],3)>0.0||
            norm(pcv->var[5],19)>0.0) continue;
        matcpy(pcv->off[5],pcv->off[1], 3,1);
        matcpy(pcv->var[5],pcv->var[1],19,1);
    }
    /* antenna index=6 : use G02 if no C06 */
    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        if (pcv->sat>0) continue;
        if (norm(pcv->off[6],3)>0.0||
            norm(pcv->var[6],19)>0.0) continue;
        matcpy(pcv->off[6],pcv->off[1], 3,1);
        matcpy(pcv->var[6],pcv->var[1],19,1);
    }
    /* antenna index=7 : use G02 if no R02 R06 */
    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        if (pcv->sat>0) continue;
        if (norm(pcv->off[7],3)>0.0||
            norm(pcv->var[7],19)>0.0) continue;
        matcpy(pcv->off[7],pcv->off[1], 3,1);
        matcpy(pcv->var[7],pcv->var[1],19,1);
    }
    /* antenna index=8 : use G02 if no E07 C07 */
    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        if (pcv->sat>0) continue;
        if (norm(pcv->off[8],3)>0.0||
            norm(pcv->var[8],19)>0.0) continue;
        matcpy(pcv->off[8],pcv->off[1], 3,1);
        matcpy(pcv->var[8],pcv->var[1],19,1);
    }
    /* antenna index=9 : use G02 if no R03 */
    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        if (pcv->sat>0) continue;
        if (norm(pcv->off[9],3)>0.0||
            norm(pcv->var[9],19)>0.0) continue;
        matcpy(pcv->off[9],pcv->off[1], 3,1);
        matcpy(pcv->var[9],pcv->var[1],19,1);
    }
    /* antenna index=10 : use G05 if no E08 C08 */
    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        if (pcv->sat>0) continue;
        if (norm(pcv->off[10],3)>0.0||
            norm(pcv->var[10],19)>0.0) continue;
        matcpy(pcv->off[10],pcv->off[2], 3,1);
        matcpy(pcv->var[10],pcv->var[2],19,1);
    }

    for (i=0;i<pcvs->n;i++) {
        pcv=pcvs->pcv+i;
        for (f=0;f<NFREQPCV;f++) {
            if (norm(pcv->off[f],3)==0.0) continue;
            trace(NULL,4,"readpcv: sat=%2d type=%20s code=%s f=%2d off=%8.4f %8.4f %8.4f\n",
                  pcv->sat,pcv->type,pcv->code,f,
                  pcv->off[f][0],pcv->off[f][1],pcv->off[f][2]);
        }
    }
    return stat;
}
/* search antenna parameter ----------------------------------------------------
* read satellite antenna phase center position
* args   : int    sat         I   satellite number (0: receiver antenna)
*          char   *type       I   antenna type for receiver antenna
*          gtime_t time       I   time to search parameters
*          pcvs_t *pcvs       IO  antenna parameters
* return : antenna parameter (NULL: no antenna)
*-----------------------------------------------------------------------------*/
extern pcv_t *searchpcv(int sat, const char *type, gtime_t time,
                        const pcvs_t *pcvs)
{
    pcv_t *pcv;
    char buff[MAXANT],*types[2],*p;
    int i,j,n=0;

    trace(NULL,3,"searchpcv: sat=%2d type=%s\n",sat,type);

    if (sat) { /* search satellite antenna */
        for (i=0;i<pcvs->n;i++) {
            pcv=pcvs->pcv+i;
            if (pcv->sat!=sat) continue;
            if (pcv->ts.time!=0&&timediff(pcv->ts,time)>0.0) continue;
            if (pcv->te.time!=0&&timediff(pcv->te,time)<0.0) continue;
            return pcv;
        }
    }
    else {
        strcpy(buff,type);
        for (p=strtok(buff," ");p&&n<2;p=strtok(NULL," ")) types[n++]=p;
        if (n<=0) return NULL;

        /* search receiver antenna with radome at first */
        for (i=0;i<pcvs->n;i++) {
            pcv=pcvs->pcv+i;
            for (j=0;j<n;j++) if (!strstr(pcv->type,types[j])) break;
            if (j>=n) return pcv;
        }
        /* search receiver antenna without radome */
        for (i=0;i<pcvs->n;i++) {
            pcv=pcvs->pcv+i;
            if (strstr(pcv->type,types[0])!=pcv->type) continue;

            trace(NULL,2,"pcv without radome is used type=%s\n",type);
            return pcv;
        }
    }
    return NULL;
}
/* receiver antenna model ------------------------------------------------------
* compute antenna offset by antenna phase center parameters
* args   : pcv_t *pcv       I   antenna phase center parameters
*          double *del      I   antenna delta {e,n,u} (m)
*          double *azel     I   azimuth/elevation for receiver {az,el} (rad)
*          int     opt      I   option (0:only offset,1:offset+pcv)
*          double *dant     O   range offsets for each frequency (m)
* return : none
* notes  : current version does not support azimuth dependent terms
*-----------------------------------------------------------------------------*/
extern void antmodel(const pcv_t *pcv, const double *del, const double *azel,
                     int opt, double *dant)
{
    double e[3],off[3],cosel=cos(azel[1]);
    int i,j;

    trace(NULL,4,"antmodel: azel=%6.1f %4.1f opt=%d\n",azel[0]*R2D,azel[1]*R2D,opt);

    e[0]=sin(azel[0])*cosel;
    e[1]=cos(azel[0])*cosel;
    e[2]=sin(azel[1]);

    for (i=0;i<NFREQPCV;i++) {
        for (j=0;j<3;j++) off[j]=pcv->off[i][j]+del[j];

        dant[i]=-dot(off,e,3)+(opt?interpvar(90.0-azel[1]*R2D,pcv->var[i]):0.0);
    }
    trace(NULL,5,"antmodel: dant=%6.3f %6.3f\n",dant[0],dant[1]);
}
/* satellite antenna model ------------------------------------------------------
* compute satellite antenna phase center parameters
* args   : pcv_t *pcv       I   antenna phase center parameters
*          double nadir     I   nadir angle for satellite (rad)
*          double *dant     O   range offsets for each frequency (m)
* return : none
*-----------------------------------------------------------------------------*/
extern void antmodel_s(const pcv_t *pcv, double nadir, double *dant)
{
    int i;

    trace(NULL,4,"antmodel_s: nadir=%6.1f\n",nadir*R2D);

    for (i=0;i<NFREQPCV;i++) {
        dant[i]=interpvar(nadir*R2D*5.0,pcv->var[i]);
    }
    trace(NULL,5,"antmodel_s: dant=%6.3f %6.3f\n",dant[0],dant[1]);
}
