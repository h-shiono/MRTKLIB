/*------------------------------------------------------------------------------
 * mrtk_station.c : station position and BLQ functions
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
 * @file mrtk_station.c
 * @brief MRTKLIB Station Module — Implementation.
 *
 * All functions and static data in this file are cut-and-paste extractions
 * from the legacy rtkcmn.c with zero algorithmic changes.
 */

#include "mrtklib/mrtk_station.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "mrtklib/mrtk_trace.h"

/*============================================================================
 * Forward Declarations (resolved at link time)
 *===========================================================================*/

/*============================================================================
 * Private Constants
 *===========================================================================*/

#define PI          3.1415926535897932  /* pi */
#define D2R         (PI/180.0)          /* deg to rad */

/*============================================================================
 * Private Functions
 *===========================================================================*/

/* read blq record -----------------------------------------------------------*/
extern int readblqrecord(FILE *fp, double *odisp)
{
    double v[11];
    char buff[256];
    int i,n=0;

    while (fgets(buff,sizeof(buff),fp)) {
        if (!strncmp(buff, "$$", 2)) {
            continue;
        }
        if (sscanf(buff, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", v, v + 1, v + 2, v + 3, v + 4, v + 5, v + 6,
                   v + 7, v + 8, v + 9, v + 10) < 11) {
            continue;
        }
        for (i = 0; i < 11; i++) {
            odisp[n + i * 6] = v[i];
        }
        if (++n == 6) {
            return 1;
        }
    }
    return 0;
}

/*============================================================================
 * Public Functions
 *===========================================================================*/

/* read elevation mask file ----------------------------------------------------
* read elevation mask file
* args   : char   *file     I   elevation mask file
*          int16_t *elmask  O   elevation mask vector (360 x 1) (0.1 deg)
*                                 elmask[i]: elevation mask at azimuth i (deg)
* return : status (1:ok,0:file open error)
* notes  : text format of the elevation mask file
*            AZ1  EL1
*            AZ2  EL2
*            ...
*          (1) EL{i} defines the elevation mask (deg) at the azimuth angle az
*              (AZ{i} <= az (deg) < AZ{i+1}, AZ1 < AZ2 < ... < AZ{n}, n <= 360)
*          (2) text after % or # is treated as comments
*-----------------------------------------------------------------------------*/
extern int readelmask(const char *file, int16_t *elmask)
{
    FILE *fp;
    double az[360],el[360],mask=0.0;
    char buff[256],*p;
    int i,j=0,n=0;

    trace(NULL,3,"readelmask: file=%s\n",file);

    for (i=0;i<360;i++) {
        elmask[i]=0;
    }
    if (!(fp=fopen(file,"r"))) {
        fprintf(stderr,"elevation mask file open error : %s\n",file);
        return 0;
    }
    while (fgets(buff,sizeof(buff),fp)&&n<360) {
        if ((p = strchr(buff, '%'))) {
            *p = '\0';
        }
        if ((p = strchr(buff, '#'))) {
            *p = '\0';
        }
        if (sscanf(buff, "%lf %lf", az + n, el + n) == 2) {
            n++;
        }
    }
    fclose(fp);

    for (i=0;i<360;i++) {
        if (j<n&&i>=(int)az[j]) {
            mask=el[j++];
        }
        elmask[i]=(int16_t)(mask/0.1);
    }
    return 1;
}
/* read station positions ------------------------------------------------------
* read positions from station position file
* args   : char  *file      I   station position file containing
*                               lat(deg) lon(deg) height(m) name in a line
*          char  *rcvs      I   station name
*          double *pos      O   station position {lat,lon,h} (rad/m)
*                               (all 0 if search error)
* return : none
*-----------------------------------------------------------------------------*/
extern void readpos(const char *file, const char *rcv, double *pos)
{
    static double poss[2048][3];
    static char stas[2048][16];
    FILE *fp;
    int i,j,len,np=0;
    char buff[256],str[256];

    trace(NULL,3,"readpos: file=%s\n",file);

    if (!(fp=fopen(file,"r"))) {
        fprintf(stderr,"reference position file open error : %s\n",file);
        return;
    }
    while (np<2048&&fgets(buff,sizeof(buff),fp)) {
        if (buff[0] == '%' || buff[0] == '#') {
            continue;
        }
        if (sscanf(buff, "%lf %lf %lf %s", &poss[np][0], &poss[np][1], &poss[np][2], str) < 4) {
            continue;
        }
        sprintf(stas[np++],"%.15s",str);
    }
    fclose(fp);
    len=(int)strlen(rcv);
    for (i=0;i<np;i++) {
        if (strncmp(stas[i], rcv, len)) {
            continue;
        }
        for (j = 0; j < 3; j++) {
            pos[j] = poss[i][j];
        }
        pos[0]*=D2R; pos[1]*=D2R;
        return;
    }
    pos[0]=pos[1]=pos[2]=0.0;
}
/* read blq ocean tide loading parameters --------------------------------------
* read blq ocean tide loading parameters
* args   : char   *file       I   BLQ ocean tide loading parameter file
*          char   *sta        I   station name
*          double *odisp      O   ocean tide loading parameters
* return : status (1:ok,0:file open error)
*-----------------------------------------------------------------------------*/
extern int readblq(const char *file, const char *sta, double *odisp)
{
    FILE *fp;
    char buff[256],staname[32]="",name[32],*p;

    /* station name to upper case */
    sscanf(sta,"%16s",staname);
    for (p = staname; (*p = (char)toupper((int)(*p))); p++) {
        /* convert in-place via loop header */
    }

    if (!(fp=fopen(file,"r"))) {
        trace(NULL,2,"blq file open error: file=%s\n",file);
        return 0;
    }
    while (fgets(buff,sizeof(buff),fp)) {
        if (!strncmp(buff, "$$", 2) || strlen(buff) < 2) {
            continue;
        }

        if (sscanf(buff + 2, "%16s", name) < 1) {
            continue;
        }
        for (p = name; (*p = (char)toupper((int)(*p))); p++) {
            /* convert in-place via loop header */
        }
        if (strcmp(name, staname)) {
            continue;
        }

        /* read blq record */
        if (readblqrecord(fp,odisp)) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    trace(NULL,2,"no otl parameters: sta=%s file=%s\n",sta,file);
    return 0;
}
