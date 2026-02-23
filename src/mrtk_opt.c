/*------------------------------------------------------------------------------
 * mrtk_opt.c : processing and solution option defaults
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
 * @file mrtk_opt.c
 * @brief MRTKLIB Options Module — Default processing and solution options.
 *
 * Pure cut-and-paste extraction from rtkcmn.c with zero algorithmic changes.
 *
 * Original: Copyright (C) by T.TAKASU, All rights reserved.
 */
#include "mrtklib/mrtk_opt.h"

/*--- local constants (duplicated to avoid rtklib.h dependency) -------------*/
static const double D2R = 3.1415926535897932 / 180.0;

#define SYS_GPS     0x01

/*--- default option instances -----------------------------------------------*/

const prcopt_t prcopt_default={ /* defaults processing options */
    PMODE_SINGLE,0,2,SYS_GPS,   /* mode,soltype,nf,navsys */
    15.0*D2R,{{0,0}},           /* elmin,snrmask */
    0,1,0,0,                    /* sateph,modear,glomodear,bdsmodear */
    SYS_GPS,                    /* arsys */
    5,0,10,1,                   /* maxout,minlock,minfix,armaxiter */
    0,0,0,0,                    /* estion,esttrop,dynamics,tidecorr */
    1,0,0,0,0,                  /* niter,codesmooth,intpref,sbascorr,sbassatsel */
    0,0,                        /* rovpos,refpos */
    {100.0,100.0},              /* eratio[] */
    {100.0,0.003,0.003,0.0,1.0}, /* err[] */
    {30.0,0.03,0.3},            /* std[] */
    {1E-4,1E-3,1E-4,1E-1,1E-2,0.0}, /* prn[] */
    5E-12,                      /* sclkstab */
    {3.0,0.9999,0.25,0.1,0.05}, /* thresar */
    0.0,0.0,0.05,               /* elmaskar,almaskhold,thresslip */
    30.0,30.0,30.0,             /* maxtdif,maxinno,maxgdop */
    {0},{0},{0},                /* baseline,ru,rb */
    {"",""},                    /* anttype */
    {{0}},{{0}},{0}             /* antdel,pcv,exsats */
    ,0,0                        /* ign_chierr,bds2bias */
    ,0,0,0,0                    /* pppsatcb,pppsatpb,unbias,maxbiasdt */
};
const solopt_t solopt_default={ /* defaults solution output options */
    SOLF_LLH,TIMES_GPST,1,3,    /* posf,times,timef,timeu */
    0,1,0,0,0,0,0,              /* degf,outhead,outopt,outvel,datum,height,geoid */
    0,0,0,                      /* solstatic,sstat,trace */
    {0.0,0.0},                  /* nmeaintv */
    " ",""                      /* separator/program name */
};
