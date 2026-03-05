/*------------------------------------------------------------------------------
 * ssr2osr.c : read rinex obs/nav files, QZSS L6 Message(.l6)
 *             and output individual corrections of observation space
 *             representation (OSR). Derived from rnx2rtkp.
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
 * @file ssr2osr.c
 * @brief Read RINEX OBS/NAV and QZSS L6 CLAS, output OSR corrections.
 *
 * history : 2018/03/29  1.0 new (upstream claslib)
 *           2026/03/03  1.1 port to MRTKLIB
 */
#include <stdarg.h>
#include "rtklib.h"
#include "mrtklib/mrtklib.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_options.h"
#include "mrtklib/mrtk_postpos.h"

#define PROGNAME    "ssr2osr"           /* program name */
#define MAXFILE     8                   /* max number of input files */

/* help text -----------------------------------------------------------------*/
static const char *help[]={
"",
" usage: ssr2osr [option]... file file [...]",
"",
" Read RINEX OBS/NAV, QZSS L6 MESSAGE(.l6) files and output",
" individual corrections of observation space representation(OSR).",
" -?              print help",
" -k   file       input options from configuration file [off]",
" -ti  tint       time interval (sec) [all]",
" -ts  ds ts      start day/time (ds=y/m/d ts=h:m:s) [obs start time]",
" -te  de te      end day/time   (de=y/m/d te=h:m:s) [obs end time]",
" -o   file       set output file [stdout]",
" -x   level      debug trace level (0:off) [0]"
};
/* show message --------------------------------------------------------------*/
extern int showmsg(const char *format, ...)
{
    va_list arg;
    va_start(arg,format); vfprintf(stderr,format,arg); va_end(arg);
    fprintf(stderr,"\r");
    return 0;
}
extern void settspan(gtime_t ts, gtime_t te) {}
extern void settime(gtime_t time) {}

/* print help ----------------------------------------------------------------*/
static void printhelp(void)
{
    int i;
    for (i=0;i<(int)(sizeof(help)/sizeof(*help));i++) fprintf(stderr,"%s\n",help[i]);
    exit(0);
}
/* ssr2osr main --------------------------------------------------------------*/
int main(int argc, char **argv)
{
    prcopt_t prcopt=prcopt_default;
    solopt_t solopt=solopt_default;
    filopt_t filopt={""};
    gtime_t ts={0},te={0};
    double tint=1.0,es[]={2000,1,1,0,0,0},ee[]={2000,12,31,23,59,59};
    int i,n,ret=0;
    char *infile[MAXFILE],*outfile="";
    mrtk_ctx_t *ctx;

    /* Initialize MRTKLIB runtime context */
    ctx=mrtk_ctx_create();
    g_mrtk_ctx=ctx;
    g_mrtk_legacy_ctx=mrtk_context_new();

    prcopt.mode  =PMODE_SSR2OSR;
    prcopt.navsys=SYS_GPS|SYS_QZS;
    prcopt.refpos=1;
    prcopt.glomodear=1;
    solopt.timef=0;
    sprintf(solopt.prog ,"%s(%s ver.%s)",PROGNAME,MRTKLIB_SOFTNAME,MRTKLIB_VERSION_STRING);
    sprintf(filopt.trace,"%s.trace",PROGNAME);

    /* load options from configuration file */
    for (i=1;i<argc;i++) {
        if (!strcmp(argv[i],"-k")&&i+1<argc) {
            resetsysopts();
            if (!loadopts(argv[++i],sysopts)) {
                mrtk_context_free(g_mrtk_legacy_ctx);
                g_mrtk_ctx=NULL;
                mrtk_ctx_destroy(ctx);
                return -1;
            }
            getsysopts(&prcopt,&solopt,&filopt);
        }
    }
    for (i=1,n=0;i<argc;i++) {
        if      (!strcmp(argv[i],"-o")&&i+1<argc) outfile=argv[++i];
        else if (!strcmp(argv[i],"-ts")&&i+2<argc) {
            sscanf(argv[++i],"%lf/%lf/%lf",es,es+1,es+2);
            sscanf(argv[++i],"%lf:%lf:%lf",es+3,es+4,es+5);
            ts=epoch2time(es);
        }
        else if (!strcmp(argv[i],"-te")&&i+2<argc) {
            sscanf(argv[++i],"%lf/%lf/%lf",ee,ee+1,ee+2);
            sscanf(argv[++i],"%lf:%lf:%lf",ee+3,ee+4,ee+5);
            te=epoch2time(ee);
        }
        else if (!strcmp(argv[i],"-ti")&&i+1<argc) tint=atof(argv[++i]);
        else if (!strcmp(argv[i],"-k")&&i+1<argc) {++i; continue;}
        else if (!strcmp(argv[i],"-x")&&i+1<argc) solopt.trace=atoi(argv[++i]);
        else if (*argv[i]=='-') printhelp();
        else if (n<MAXFILE) infile[n++]=argv[i];
    }
    if (n<=0) {
        showmsg("error : no input file");
        mrtk_context_free(g_mrtk_legacy_ctx);
        g_mrtk_ctx=NULL;
        mrtk_ctx_destroy(ctx);
        return -2;
    }

    /* generate OSR via postpos pipeline */
    ret=postpos(ctx,ts,te,tint,0.0,&prcopt,&solopt,&filopt,infile,n,outfile,"","");

    if (!ret) fprintf(stderr,"%40s\r","");
    mrtk_context_free(g_mrtk_legacy_ctx);
    g_mrtk_ctx=NULL;
    mrtk_ctx_destroy(ctx);
    return ret;
}
