/*------------------------------------------------------------------------------
 * mrtk_sys.c : system utility functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_sys.c
 * @brief MRTKLIB System Module — Implementation.
 *
 * All functions and static data in this file are cut-and-paste extractions
 * from the legacy rtkcmn.c with zero algorithmic changes.
 */

#define _POSIX_C_SOURCE 199506

#include "mrtklib/mrtk_sys.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

/*============================================================================
 * Forward Declarations (resolved at link time from rtkcmn.c)
 *===========================================================================*/

extern void trace(int level, const char *format, ...);
extern void tracet(int level, const char *format, ...);

/*============================================================================
 * Platform Constants
 *===========================================================================*/

#define FILEPATHSEP '/'

/*============================================================================
 * String Conversion Functions
 *===========================================================================*/

/* string to number ------------------------------------------------------------
* convert substring in string to number
* args   : char   *s        I   string ("... nnn.nnn ...")
*          int    i,n       I   substring position and width
* return : converted number (0.0:error)
*-----------------------------------------------------------------------------*/
extern double str2num(const char *s, int i, int n)
{
    double value;
    char str[256],*p=str;

    if (i<0||(int)strlen(s)<i||(int)sizeof(str)-1<n) return 0.0;
    for (s+=i;*s&&--n>=0;s++) *p++=*s=='d'||*s=='D'?'E':*s;
    *p='\0';
    return sscanf(str,"%lf",&value)==1?value:0.0;
}

/*============================================================================
 * Command / Path Functions
 *===========================================================================*/

/* execute command -------------------------------------------------------------
* execute command line by operating system shell
* args   : char   *cmd      I   command line
* return : execution status (0:ok,0>:error)
*-----------------------------------------------------------------------------*/
extern int execcmd(const char *cmd)
{
    trace(3,"execcmd: cmd=%s\n",cmd);

    return system(cmd);
}
/* expand file path ------------------------------------------------------------
* expand file path with wild-card (*) in file
* args   : char   *path     I   file path to expand (captal insensitive)
*          char   *paths    O   expanded file paths
*          int    nmax      I   max number of expanded file paths
* return : number of expanded file paths
* notes  : the order of expanded files is alphabetical order
*-----------------------------------------------------------------------------*/
extern int expath(const char *path, char *paths[], int nmax)
{
    int i,j,n=0;
    char tmp[1024];
    struct dirent *d;
    DIR *dp;
    const char *file=path;
    char dir[1024]="",s1[1024],s2[1024],*p,*q,*r;

    trace(3,"expath  : path=%s nmax=%d\n",path,nmax);

    if ((p=strrchr(path,'/'))||(p=strrchr(path,'\\'))) {
        file=p+1; strncpy(dir,path,p-path+1); dir[p-path+1]='\0';
    }
    if (!(dp=opendir(*dir?dir:"."))) return 0;
    while ((d=readdir(dp))) {
        if (*(d->d_name)=='.') continue;
        sprintf(s1,"^%s$",d->d_name);
        sprintf(s2,"^%s$",file);
        for (p=s1;*p;p++) *p=(char)tolower((int)*p);
        for (p=s2;*p;p++) *p=(char)tolower((int)*p);

        for (p=s1,q=strtok_r(s2,"*",&r);q;q=strtok_r(NULL,"*",&r)) {
            if ((p=strstr(p,q))) p+=strlen(q); else break;
        }
        if (p&&n<nmax) sprintf(paths[n++],"%s%s",dir,d->d_name);
    }
    closedir(dp);
    /* sort paths in alphabetical order */
    for (i=0;i<n-1;i++) {
        for (j=i+1;j<n;j++) {
            if (strcmp(paths[i],paths[j])>0) {
                strcpy(tmp,paths[i]);
                strcpy(paths[i],paths[j]);
                strcpy(paths[j],tmp);
            }
        }
    }
    for (i=0;i<n;i++) trace(3,"expath  : file=%s\n",paths[i]);

    return n;
}
/* generate local directory recursively --------------------------------------*/
static int mkdir_r(const char *dir)
{
    char pdir[1024],*p;
    FILE *fp;

    if (!*dir) return 1;

    sprintf(pdir,"%.1023s",dir);
    if ((p=strrchr(pdir,FILEPATHSEP))) {
        *p='\0';
        if (!(fp=fopen(pdir,"r"))) {
            if (!mkdir_r(pdir)) return 0;
        }
        else fclose(fp);
    }
    if (!mkdir(dir,0777)||errno==EEXIST) return 1;
    trace(2,"directory generation error: dir=%s\n",dir);
    return 0;
}
/* create directory ------------------------------------------------------------
* create directory if not exists
* args   : char   *path     I   file path to be saved
* return : none
* notes  : recursively.
*-----------------------------------------------------------------------------*/
extern void createdir(const char *path)
{
    char buff[1024],*p;

    tracet(3,"createdir: path=%s\n",path);

    strcpy(buff,path);
    if (!(p=strrchr(buff,FILEPATHSEP))) return;
    *p='\0';

    mkdir_r(buff);
}
/* replace string ------------------------------------------------------------*/
static int repstr(char *str, const char *pat, const char *rep)
{
    int len=(int)strlen(pat);
    char buff[1024],*p,*q,*r;

    for (p=str,r=buff;*p;p=q+len) {
        if (!(q=strstr(p,pat))) break;
        strncpy(r,p,q-p);
        r+=q-p;
        r+=sprintf(r,"%s",rep);
    }
    if (p<=str) return 0;
    strcpy(r,p);
    strcpy(str,buff);
    return 1;
}
/* replace keywords in file path -----------------------------------------------
* replace keywords in file path with date, time, rover and base station id
* args   : char   *path     I   file path (see below)
*          char   *rpath    O   file path in which keywords replaced (see below)
*          gtime_t time     I   time (gpst)  (time.time==0: not replaced)
*          char   *rov      I   rover id string        ("": not replaced)
*          char   *base     I   base station id string ("": not replaced)
* return : status (1:keywords replaced, 0:no valid keyword in the path,
*                  -1:no valid time)
* notes  : the following keywords in path are replaced by date, time and name
*              %Y -> yyyy : year (4 digits) (1900-2099)
*              %y -> yy   : year (2 digits) (00-99)
*              %m -> mm   : month           (01-12)
*              %d -> dd   : day of month    (01-31)
*              %h -> hh   : hours           (00-23)
*              %M -> mm   : minutes         (00-59)
*              %S -> ss   : seconds         (00-59)
*              %n -> ddd  : day of year     (001-366)
*              %W -> wwww : gps week        (0001-9999)
*              %D -> d    : day of gps week (0-6)
*              %H -> h    : hour code       (a=0,b=1,c=2,...,x=23)
*              %ha-> hh   : 3 hours         (00,03,06,...,21)
*              %hb-> hh   : 6 hours         (00,06,12,18)
*              %hc-> hh   : 12 hours        (00,12)
*              %t -> mm   : 15 minutes      (00,15,30,45)
*              %r -> rrrr : rover id
*              %b -> bbbb : base station id
*-----------------------------------------------------------------------------*/
extern int reppath(const char *path, char *rpath, gtime_t time, const char *rov,
                   const char *base)
{
    double ep[6],ep0[6]={2000,1,1,0,0,0};
    int week,dow,doy,stat=0;
    char rep[64];

    strcpy(rpath,path);

    if (!strstr(rpath,"%")) return 0;
    if (*rov ) stat|=repstr(rpath,"%r",rov );
    if (*base) stat|=repstr(rpath,"%b",base);
    if (time.time!=0) {
        time2epoch(time,ep);
        ep0[0]=ep[0];
        dow=(int)floor(time2gpst(time,&week)/86400.0);
        doy=(int)floor(timediff(time,epoch2time(ep0))/86400.0)+1;
        sprintf(rep,"%02d",  ((int)ep[3]/3)*3);   stat|=repstr(rpath,"%ha",rep);
        sprintf(rep,"%02d",  ((int)ep[3]/6)*6);   stat|=repstr(rpath,"%hb",rep);
        sprintf(rep,"%02d",  ((int)ep[3]/12)*12); stat|=repstr(rpath,"%hc",rep);
        sprintf(rep,"%04.0f",ep[0]);              stat|=repstr(rpath,"%Y",rep);
        sprintf(rep,"%02.0f",fmod(ep[0],100.0));  stat|=repstr(rpath,"%y",rep);
        sprintf(rep,"%02.0f",ep[1]);              stat|=repstr(rpath,"%m",rep);
        sprintf(rep,"%02.0f",ep[2]);              stat|=repstr(rpath,"%d",rep);
        sprintf(rep,"%02.0f",ep[3]);              stat|=repstr(rpath,"%h",rep);
        sprintf(rep,"%02.0f",ep[4]);              stat|=repstr(rpath,"%M",rep);
        sprintf(rep,"%02.0f",floor(ep[5]));       stat|=repstr(rpath,"%S",rep);
        sprintf(rep,"%03d",  doy);                stat|=repstr(rpath,"%n",rep);
        sprintf(rep,"%04d",  week);               stat|=repstr(rpath,"%W",rep);
        sprintf(rep,"%d",    dow);                stat|=repstr(rpath,"%D",rep);
        sprintf(rep,"%c",    'a'+(int)ep[3]);     stat|=repstr(rpath,"%H",rep);
        sprintf(rep,"%02d",  ((int)ep[4]/15)*15); stat|=repstr(rpath,"%t",rep);
    }
    else if (strstr(rpath,"%ha")||strstr(rpath,"%hb")||strstr(rpath,"%hc")||
             strstr(rpath,"%Y" )||strstr(rpath,"%y" )||strstr(rpath,"%m" )||
             strstr(rpath,"%d" )||strstr(rpath,"%h" )||strstr(rpath,"%M" )||
             strstr(rpath,"%S" )||strstr(rpath,"%n" )||strstr(rpath,"%W" )||
             strstr(rpath,"%D" )||strstr(rpath,"%H" )||strstr(rpath,"%t" )) {
        return -1; /* no valid time */
    }
    return stat;
}
/* replace keywords in file path and generate multiple paths -------------------
* replace keywords in file path with date, time, rover and base station id
* generate multiple keywords-replaced paths
* args   : char   *path     I   file path (see below)
*          char   *rpath[]  O   file paths in which keywords replaced
*          int    nmax      I   max number of output file paths
*          gtime_t ts       I   time start (gpst)
*          gtime_t te       I   time end   (gpst)
*          char   *rov      I   rover id string        ("": not replaced)
*          char   *base     I   base station id string ("": not replaced)
* return : number of replaced file paths
* notes  : see reppath() for replacements of keywords.
*          minimum interval of time replaced is 900s.
*-----------------------------------------------------------------------------*/
extern int reppaths(const char *path, char *rpath[], int nmax, gtime_t ts,
                    gtime_t te, const char *rov, const char *base)
{
    gtime_t time;
    double tow,tint=86400.0;
    int i,n=0,week;

    trace(3,"reppaths: path =%s nmax=%d rov=%s base=%s\n",path,nmax,rov,base);

    if (ts.time==0||te.time==0||timediff(ts,te)>0.0) return 0;

    if (strstr(path,"%S")||strstr(path,"%M")||strstr(path,"%t")) tint=900.0;
    else if (strstr(path,"%h")||strstr(path,"%H")) tint=3600.0;

    tow=time2gpst(ts,&week);
    time=gpst2time(week,floor(tow/tint)*tint);

    while (timediff(time,te)<=0.0&&n<nmax) {
        reppath(path,rpath[n],time,rov,base);
        if (n==0||strcmp(rpath[n],rpath[n-1])) n++;
        time=timeadd(time,tint);
    }
    for (i=0;i<n;i++) trace(3,"reppaths: rpath=%s\n",rpath[i]);
    return n;
}

/*============================================================================
 * File Compression Functions
 *===========================================================================*/

/* uncompress file -------------------------------------------------------------
* uncompress (uncompress/unzip/uncompact hatanaka-compression/tar) file
* args   : char   *file     I   input file
*          char   *uncfile  O   uncompressed file
* return : status (-1:error,0:not compressed file,1:uncompress completed)
* note   : creates uncompressed file in tempolary directory
*          gzip, tar and crx2rnx commands have to be installed in commands path
*-----------------------------------------------------------------------------*/
extern int rtk_uncompress(const char *file, char *uncfile)
{
    int stat=0;
    char *p,cmd[64+2048]="",tmpfile[1024]="",buff[1024],*fname,*dir="";

    trace(3,"rtk_uncompress: file=%s\n",file);

    strcpy(tmpfile,file);
    if (!(p=strrchr(tmpfile,'.'))) return 0;

    /* uncompress by gzip */
    if (!strcmp(p,".z"  )||!strcmp(p,".Z"  )||
        !strcmp(p,".gz" )||!strcmp(p,".GZ" )||
        !strcmp(p,".zip")||!strcmp(p,".ZIP")) {

        strcpy(uncfile,tmpfile); uncfile[p-tmpfile]='\0';
        sprintf(cmd,"gzip -f -d -c \"%s\" > \"%s\"",tmpfile,uncfile);

        if (execcmd(cmd)) {
            remove(uncfile);
            return -1;
        }
        strcpy(tmpfile,uncfile);
        stat=1;
    }
    /* extract tar file */
    if ((p=strrchr(tmpfile,'.'))&&!strcmp(p,".tar")) {

        strcpy(uncfile,tmpfile); uncfile[p-tmpfile]='\0';
        strcpy(buff,tmpfile);
        fname=buff;
        if ((p=strrchr(buff,'/'))) {
            *p='\0'; dir=fname; fname=p+1;
        }
        sprintf(cmd,"tar -C \"%s\" -xf \"%s\"",dir,tmpfile);
        if (execcmd(cmd)) {
            if (stat) remove(tmpfile);
            return -1;
        }
        if (stat) remove(tmpfile);
        stat=1;
    }
    /* extract hatanaka-compressed file by cnx2rnx */
    else if ((p=strrchr(tmpfile,'.'))&&
             ((strlen(p)>3&&(*(p+3)=='d'||*(p+3)=='D'))||
              !strcmp(p,".crx")||!strcmp(p,".CRX"))) {

        strcpy(uncfile,tmpfile);
        uncfile[p-tmpfile+3]=*(p+3)=='D'?'O':'o';
        sprintf(cmd,"crx2rnx < \"%s\" > \"%s\"",tmpfile,uncfile);

        if (execcmd(cmd)) {
            remove(uncfile);
            if (stat) remove(tmpfile);
            return -1;
        }
        if (stat) remove(tmpfile);
        stat=1;
    }
    trace(3,"rtk_uncompress: stat=%d\n",stat);
    return stat;
}
