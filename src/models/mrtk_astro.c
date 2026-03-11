/*------------------------------------------------------------------------------
 * mrtk_astro.c : astronomical functions for sun/moon position
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
 * @file mrtk_astro.c
 * @brief MRTKLIB Astronomy Module — Implementation.
 *
 * All functions and static data in this file are cut-and-paste extractions
 * from the legacy rtkcmn.c with zero algorithmic changes.
 */

#include "mrtklib/mrtk_astro.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_coords.h"

#include <math.h>
#include "mrtklib/mrtk_trace.h"

/*============================================================================
 * Forward Declarations (resolved at link time)
 *===========================================================================*/

/*============================================================================
 * Private Constants
 *===========================================================================*/

#define PI          3.1415926535897932  /* pi */
#define D2R         (PI/180.0)          /* deg to rad */
#define AS2R        (D2R/3600.0)        /* arc sec to radian */
#define AU          149597870691.0      /* 1 AU (m) */
#define RE_WGS84    6378137.0           /* earth semimajor axis (WGS84) (m) */

/*============================================================================
 * Private Functions
 *===========================================================================*/

/* astronomical arguments: f={l,l',F,D,OMG} (rad) ----------------------------*/
static void ast_args(double t, double *f)
{
    static const double fc[][5]={ /* coefficients for iau 1980 nutation */
        { 134.96340251, 1717915923.2178,  31.8792,  0.051635, -0.00024470},
        { 357.52910918,  129596581.0481,  -0.5532,  0.000136, -0.00001149},
        {  93.27209062, 1739527262.8478, -12.7512, -0.001037,  0.00000417},
        { 297.85019547, 1602961601.2090,  -6.3706,  0.006593, -0.00003169},
        { 125.04455501,   -6962890.2665,   7.4722,  0.007702, -0.00005939}
    };
    double tt[4];
    int i,j;

    for (tt[0] = t, i = 1; i < 4; i++) {
        tt[i] = tt[i - 1] * t;
    }
    for (i=0;i<5;i++) {
        f[i]=fc[i][0]*3600.0;
        for (j = 0; j < 4; j++) {
            f[i] += fc[i][j + 1] * tt[j];
        }
        f[i]=fmod(f[i]*AS2R,2.0*PI);
    }
}

/*============================================================================
 * Public Functions
 *===========================================================================*/

/* sun and moon position in eci (ref [4] 5.1.1, 5.2.1) -----------------------*/
extern void sunmoonpos_eci(gtime_t tut, double *rsun, double *rmoon)
{
    const double ep2000[]={2000,1,1,12,0,0};
    double t,f[5],eps,Ms,ls,rs,lm,pm,rm,sine,cose,sinp,cosp,sinl,cosl;

    trace(NULL,4,"sunmoonpos_eci: tut=%s\n",time_str(tut,3));

    t=timediff(tut,epoch2time(ep2000))/86400.0/36525.0;

    /* astronomical arguments */
    ast_args(t,f);

    /* obliquity of the ecliptic */
    eps=23.439291-0.0130042*t;
    sine=sin(eps*D2R); cose=cos(eps*D2R);

    /* sun position in eci */
    if (rsun) {
        Ms=357.5277233+35999.05034*t;
        ls=280.460+36000.770*t+1.914666471*sin(Ms*D2R)+0.019994643*sin(2.0*Ms*D2R);
        rs=AU*(1.000140612-0.016708617*cos(Ms*D2R)-0.000139589*cos(2.0*Ms*D2R));
        sinl=sin(ls*D2R); cosl=cos(ls*D2R);
        rsun[0]=rs*cosl;
        rsun[1]=rs*cose*sinl;
        rsun[2]=rs*sine*sinl;

        trace(NULL,5,"rsun =%.3f %.3f %.3f\n",rsun[0],rsun[1],rsun[2]);
    }
    /* moon position in eci */
    if (rmoon) {
        lm=218.32+481267.883*t+6.29*sin(f[0])-1.27*sin(f[0]-2.0*f[3])+
           0.66*sin(2.0*f[3])+0.21*sin(2.0*f[0])-0.19*sin(f[1])-0.11*sin(2.0*f[2]);
        pm=5.13*sin(f[2])+0.28*sin(f[0]+f[2])-0.28*sin(f[2]-f[0])-
           0.17*sin(f[2]-2.0*f[3]);
        rm=RE_WGS84/sin((0.9508+0.0518*cos(f[0])+0.0095*cos(f[0]-2.0*f[3])+
                   0.0078*cos(2.0*f[3])+0.0028*cos(2.0*f[0]))*D2R);
        sinl=sin(lm*D2R); cosl=cos(lm*D2R);
        sinp=sin(pm*D2R); cosp=cos(pm*D2R);
        rmoon[0]=rm*cosp*cosl;
        rmoon[1]=rm*(cose*cosp*sinl-sine*sinp);
        rmoon[2]=rm*(sine*cosp*sinl+cose*sinp);

        trace(NULL,5,"rmoon=%.3f %.3f %.3f\n",rmoon[0],rmoon[1],rmoon[2]);
    }
}
/* sun and moon position -------------------------------------------------------
* get sun and moon position in ecef
* args   : gtime_t tut      I   time in ut1
*          double *erpv     I   erp value {xp,yp,ut1_utc,lod} (rad,rad,s,s/d)
*          double *rsun     IO  sun position in ecef  (m) (NULL: not output)
*          double *rmoon    IO  moon position in ecef (m) (NULL: not output)
*          double *gmst     O   gmst (rad)
* return : none
*-----------------------------------------------------------------------------*/
extern void sunmoonpos(gtime_t tutc, const double *erpv, double *rsun,
                       double *rmoon, double *gmst)
{
    gtime_t tut;
    double rs[3],rm[3],U[9],gmst_;

    trace(NULL,4,"sunmoonpos: tutc=%s\n",time_str(tutc,3));

    tut=timeadd(tutc,erpv[2]); /* utc -> ut1 */

    /* sun and moon position in eci */
    sunmoonpos_eci(tut,rsun?rs:NULL,rmoon?rm:NULL);

    /* eci to ecef transformation matrix */
    eci2ecef(tutc,erpv,U,&gmst_);

    /* sun and moon position in ecef */
    if (rsun) {
        matmul("NN", 3, 1, 3, 1.0, U, rs, 0.0, rsun);
    }
    if (rmoon) {
        matmul("NN", 3, 1, 3, 1.0, U, rm, 0.0, rmoon);
    }
    if (gmst) {
        *gmst = gmst_;
    }
}
