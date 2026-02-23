/*------------------------------------------------------------------------------
 * mrtk_bias_sinex.c : BIAS-SINEX file reader and bias functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_bias_sinex.h"
#include "mrtklib/mrtk_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mrtklib/mrtk_trace.h"

/* local constants -----------------------------------------------------------*/
static const double CLIGHT  = 299792458.0;
#define NS2M    (CLIGHT/1E9)
#define M2NS    (1E9/CLIGHT)

/*--- forward declarations for legacy functions resolved at link time -------*/
extern void satno2id(int sat, char *id);
extern int satid2no(const char *id);
extern uint8_t obs2code(const char *obs);
extern char *code2obs(uint8_t code);
extern char *time_str(gtime_t t, int n);

static biass_t biass = {0};
static char syscode[] = "GREJCIS";
static char typeid[][5] = {"DSB ","ISB ","OSB "};

/* get system code number -----------------------------------------------------
* get system code number based on satellite number
* args   : int    sat       I   satellite number
* return : int              O   system code number(0 or positive:valid,-1:error)
*-----------------------------------------------------------------------------*/
int getsysno(int sat)
{
    int i;
    char satid[8];

    satno2id(sat, satid);
    for(i = 0; i < MAXBSNXSYS; i++) {
        if(satid[0] == syscode[i]) return i;
    }
    return -1;
}

/* get bias structure pointer -------------------------------------------------
* get pointer to the bias structure
* args   : none
* return : biass_t *        O   pointer to bias structure
*-----------------------------------------------------------------------------*/
biass_t *getbiass(void)
{
    return &biass;
}

/* free bia ------------------------------------------------------------------*/
static void freebia(bia_t **ary, int *n, int *nmax)
{
    if(*ary != NULL) {
        free(*ary);
        *ary = NULL;
        *n = *nmax = 0;
    }
}

/* delete code/phase bias ----------------------------------------------------*/
static void freebiass(void)
{
    int i, j;

    for(i = 0; i < MAXSAT; i++) {
        if(biass.sat.cb[i] != NULL) {
            freebia(&biass.sat.cb[i], &biass.sat.ncb[i], &biass.sat.ncbmax[i]);
        }
        if(biass.sat.pb[i] != NULL) {
            freebia(&biass.sat.pb[i], &biass.sat.npb[i], &biass.sat.npbmax[i]);
        }
    }

    for(i = 0; i < MAXSTA; i++) {
        for(j = 0; j < MAXBSNXSYS; j++) {
            if(biass.sta[i].syscb[j] != NULL) {
                freebia(&biass.sta[i].syscb[j], &biass.sta[i].nsyscb[j],
                    &biass.sta[i].nsyscbmax[j]);
            }
        }
        for(j = 0; j < MAXSAT; j++) {
            if(biass.sta[i].satcb[j] != NULL) {
                freebia(&biass.sta[i].satcb[j], &biass.sta[i].nsatcb[j],
                    &biass.sta[i].nsatcbmax[j]);
            }
        }
    }
}

/* add bias to array ----------------------------------------------------------
* add a bias entry to the bias array, reallocating memory if necessary
* args   : bia_t  *bia      I   bias entry to add
*          bia_t **ary      IO  pointer to bias array
*          int    *n        IO  current number of entries in array
*          int    *nmax     IO  current maximum number of entries in array
* return : none
*-----------------------------------------------------------------------------*/
void addbia(const bia_t *bia, bia_t **ary, int *n, int *nmax)
{
    bia_t *b;

    if(*nmax <= *n) {
        *nmax += 256;
        if(!(b = (bia_t *)realloc(*ary, sizeof(bia_t)*(*nmax)))) {
            trace(NULL,1, "addbia: memory allocation error\n");
            free(*ary); *ary = NULL; *n = *nmax = 0;
            return;
        }
        *ary = b;
    }
    (*ary)[*n] = *bia;
    *n += 1;
}

/* read BIAS-SINEX file -------------------------------------------------------
* read bias information from a SINEX file and store it in the bias structure
* args   : char   *file     I   SINEX file path
* return : int              O   status (1:ok, 0:error)
*-----------------------------------------------------------------------------*/
int readbsnx(const char *file)
{
    double ep[6] = {0,1,1,0,0,0}, doy, tod, unit;
    char satid[] = "   ", sta[] = "         ", sig1[] = "   ", sig2[] = "   ";
    char buff[256], *p, *dscrpt[4];
    int i, j, n, sat, state = 0, ret = 0;
    FILE *fp;
    bia_t bia;

    trace(NULL,3, "readbsnx : file=%s\n", file);

    if(!(fp = fopen(file, "r"))) {
        trace(NULL,2, "bias sinex file open error: %s\n", file);
        return 0;
    }
    freebiass();

    while(fgets(buff, sizeof(buff), fp)) {
        if(buff[0] == '*') continue;
        if(strstr(buff, "+BIAS/DESCRIPTION")) { state = 1; continue; }
        if(strstr(buff, "-BIAS/DESCRIPTION")) { state = 0; continue; }
        if(strstr(buff, "+BIAS/SOLUTION"))    { state = 2; continue; }
        if(strstr(buff, "-BIAS/SOLUTION"))    { state = 0; continue; }

        switch(state) {
            case 1: /* BIAS/DESCRIPTION */
                dscrpt[0] = strtok(buff + 1, " \n");
                for(n = 1; n < 4; n++) {
                    if((p = strtok(NULL, " \n"))) dscrpt[n] = p;
                    else                          dscrpt[n] = "";
                }
                trace(NULL,4, "readbsnx KEYWORD, %-39s, %s, %s, %s\n",
                    dscrpt[0], dscrpt[1], dscrpt[2], dscrpt[3]);
                break;

            case 2: /* BIAS/SOLUTION */
                if(strlen(buff) < 92) continue;
                for(i = 0; i < 3; i++) {
                    if(strncmp(buff + 1, typeid[i], 4) == 0) { /* BIAS */
                        bia.type = i;
                        break;
                    }
                }
                if(i >= 3) continue;
                strncpy(satid, buff + 11, 3);    /* PRN */
                sat = satid2no(satid);
                strncpy(sta, buff + 15, 9);      /* STATION__ */
                for(i = strlen(sta) - 1; (i >= 0) && (sta[i] == ' '); i--) {
                    sta[i] = '\0';
                }
                strncpy(sig1, buff + 25, 3);     /* OBS1 */
                bia.code[0] = obs2code(&sig1[1]);
                if(bia.type < 2) {
                    strncpy(sig2, buff + 30, 3); /* OBS2 */
                    bia.code[1] = obs2code(&sig2[1]);
                }
                else {
                    memcpy(sig2, "   ", 3);
                    bia.code[1] = CODE_NONE;
                }
                /* BIAS_START____ */
                sscanf(buff + 35, "%lf:%lf:%lf", ep, &doy, &tod);
                bia.t0 = timeadd(epoch2time(ep), (doy - 1) * 86400 + tod);
                /* BIAS_END______ */
                sscanf(buff + 50, "%lf:%lf:%lf", ep, &doy, &tod);
                bia.t1 = timeadd(epoch2time(ep), (doy - 1) * 86400 + tod);
                /* UNIT */
                if(strncmp(buff + 65, "ns", 2) == 0) {
                    unit = NS2M;
                }
                else {
                    unit = 1.0;
                }
                /* __ESTIMATED_VALUE____ */
                bia.val = str2num(buff, 70, 21)*unit;
                if(strlen(buff) > 92) {
                    /* _STD_DEV___ */
                    bia.valstd = str2num(buff, 92, 11)*unit;
                }
                if(strlen(buff) > 127) {
                    /* __ESTIMATED_SLOPE____ */
                    bia.slp = str2num(buff, 104, 21)*unit;
                    /* _STD_DEV___ */
                    bia.slpstd = str2num(buff, 126, 11)*unit;
                }
                else {
                    bia.slp = bia.slpstd = 0.0;
                }
                if(strlen(sta) == 0) {  /* satellite */
                    if(sat == 0) continue;
                    if(sig1[0] == 'C') {/* code bias */
                        addbia(&bia, &biass.sat.cb[sat - 1],
                            &biass.sat.ncb[sat - 1], &biass.sat.ncbmax[sat - 1]);
                    }
                    else {              /* phase bias */
                        addbia(&bia, &biass.sat.pb[sat - 1],
                            &biass.sat.npb[sat - 1], &biass.sat.npbmax[sat - 1]);
                    }
                }
                else {                  /* station */
                    for(i = 0; (i < MAXSTA) && (i < biass.nsta); i++) {
                        if(strcmp(sta, biass.sta[i].name) == 0) break;
                    }
                    if(i >= MAXSTA) {
                        trace(NULL,2, "readbsnx %s has exceeded the station limit of %d.\n",
                            sta, MAXSTA);
                        continue;
                    }
                    if(i >= biass.nsta) {
                        strcpy(biass.sta[i].name, sta);
                        biass.nsta++;
                    }
                    if(sat == 0) {      /* system code bias */
                        for(j = 0; j < MAXBSNXSYS; j++) {
                            if(satid[0] == syscode[j]) break;
                        }
                        if(j >= MAXBSNXSYS) continue;
                        addbia(&bia, &biass.sta[i].syscb[j],
                            &biass.sta[i].nsyscb[j],
                            &biass.sta[i].nsyscbmax[j]);
                    }
                    else {               /* satellite code bias*/
                        addbia(&bia, &biass.sta[i].satcb[sat - 1],
                            &biass.sta[i].nsatcb[sat - 1],
                            &biass.sta[i].nsatcbmax[sat - 1]);
                    }
                }
                trace(NULL,4, "readbsnx %8d,%4s,%-9s, %3s, %3s, %3d, %3s, %3d, %s, %8d,%10.4f,%10.4f,%10.4f,%10.4f\n",
                    ret, typeid[bia.type], sta, satid, sig1, bia.code[0],
                    sig2, bia.code[1], time_str(bia.t0, 0),
                    (int)timediff(bia.t1, bia.t0),
                    bia.val, bia.valstd, bia.slp, bia.slpstd);
                ret++;
        }
    }
    fclose(fp);
    return ret;
}

/* time to bias time(year & day of year & time of day) -----------------------*/
static void time2biast(gtime_t time, double *year, double *doy, double *tod)
{
    gtime_t btime;
    double bep[6] = {0,1,1,0,0,0}, ep[6] = {0,0,0,0,0,0}, tdiff;

    time2epoch(time, ep);
    bep[0] = ep[0];
    btime = epoch2time(bep);

    tdiff = timediff(time, btime);
    *year = ep[0];
    *doy = (int)floor(tdiff / 86400) + 1;
    *tod = (int)floor(tdiff - (*doy - 1) * 86400);
}

/* code to signal string -----------------------------------------------------*/
static char *code2sigs(uint8_t code, int cpflag)
{
    static char sigs[8];

    strcpy(sigs, "   ");

    if(CODE_NONE < code&&code <= MAXCODE) {
        if(cpflag == 0) {
            sprintf(sigs, "C%2s", code2obs(code));
        }
        else if(cpflag == 1) {
            sprintf(sigs, "L%2s", code2obs(code));
        }
    }

    return sigs;
}

/* output BIAS-SINEX file header -----------------------------------------------
* output BIAS-SINEX file header
* args   : FILE   *fp       I   output file pointer
*          gtime_t ts       I   start time
*          gtime_t te       I   end time
*          char   *agency   I   agency name
* return : none
*-----------------------------------------------------------------------------*/
void outbsnxh(FILE *fp, gtime_t ts, gtime_t te, const char *agency)
{
    double year, doy, tod, ep[6];
    gtime_t nt = timeget();

    if(fp == NULL) return;
    trace(NULL,3, "outbsnxh: \n");

    time2epoch(nt, ep);
    time2biast(nt, &year, &doy, &tod);
    fprintf(fp, "%%=BIA 1.00 %3s %04.0f:%03.0f:%05.0f ",
        agency, year, doy, tod);
    time2biast(ts, &year, &doy, &tod);
    fprintf(fp, "%3s %04.0f:%03.0f:%05.0f", agency, year, doy, tod);
    time2biast(te, &year, &doy, &tod);
    fprintf(fp, "%04.0f:%03.0f:%05.0f P %04.0f%02.0f%02.0f%02.0f%02.0f%02.0f\n",
        year, doy, tod, ep[0], ep[1], ep[2], ep[3], ep[4], ep[5]);
}

/* output FILE/REFERENCE header ------------------------------------------------
* output the header of the FILE/REFERENCE section in a BIAS-SINEX file
* args   : FILE   *fp       I   output file pointer
* return : none
*-----------------------------------------------------------------------------*/
void outbsnxrefh(FILE *fp)
{
    if(fp == NULL) return;
    trace(NULL,3, "outbsnxrefh: \n");

    fprintf(fp, "*-------------------------------------------------------------------------------\n");
    fprintf(fp, "+FILE/REFERENCE\n");
    fprintf(fp, "*INFO_TYPE_________ INFO________________________________________________________\n");
}

/* output FILE/REFERENCE body --------------------------------------------------
* output a line in the FILE/REFERENCE section of a BIAS-SINEX file
* args   : FILE   *fp       I   output file pointer
*          int    type      I   type of information (0: DESCRIPTION, 1: OUTPUT, ...)
*          char   *reftext  I   reference text to output
* return : none
*-----------------------------------------------------------------------------*/
void outbsnxrefb(FILE *fp, int type, char *reftext)
{
    char *inft[6] = {"DESCRIPTION","OUTPUT","CONTACT","SOFTWARE","HARDWARE","INPUT"};
    if(fp == NULL) return;
    if(type < 0 || type >= 6) {
        trace(NULL,2, "outbsnxrefb error: type=%d is out of range.[0-5]\n", type);
        return;
    }
    if(strlen(reftext) > 60) {
        trace(NULL,2, "outbsnxrefb error: comment length over %d[60]\n",
            strlen(reftext));
        return;
    }
    trace(NULL,3, "outbsnxrefb: \n");
    fprintf(fp, " %-18s %-60s\n", inft[type], reftext);
}

/* output FILE/REFERENCE footer ------------------------------------------------
* output the end of the FILE/REFERENCE section in a BIAS-SINEX file
* args   : FILE   *fp       I   output file pointer
* return : none
*-----------------------------------------------------------------------------*/
void outbsnxreff(FILE *fp)
{
    if(fp == NULL) return;
    trace(NULL,3, "outbsnxreff: \n");
    fprintf(fp, "-FILE/REFERENCE\n");
}

/* output FILE/COMMENT header --------------------------------------------------
* output the header of the FILE/COMMENT section in a BIAS-SINEX file
* args   : FILE   *fp       I   output file pointer
* return : none
*-----------------------------------------------------------------------------*/
void outbsnxcomh(FILE *fp)
{
    if(fp == NULL) return;
    trace(NULL,3, "outbsnxcomh: \n");
    fprintf(fp, "*-------------------------------------------------------------------------------\n");
    fprintf(fp, "+FILE/COMMENT\n");
}

/* output FILE/COMMENT body ----------------------------------------------------
* output a comment line in the FILE/COMMENT section of a BIAS-SINEX file
* args   : FILE   *fp       I   output file pointer
*          char   *comtext  I   comment text to output
* return : none
*-----------------------------------------------------------------------------*/
void outbsnxcomb(FILE *fp, char *comtext)
{
    if(fp == NULL) return;
    if(strlen(comtext) > 79) {
        trace(NULL,2, "outbsnxcomb error: comment length over %d[79]\n",
            strlen(comtext));
        return;
    }
    trace(NULL,3, "outbsnxcomb: %s\n", comtext);
    fprintf(fp, " %-79s\n", comtext);
}

/* output FILE/COMMENT footer --------------------------------------------------
* output the end of the FILE/COMMENT section in a BIAS-SINEX file
* args   : FILE   *fp       I   output file pointer
* return : none
*-----------------------------------------------------------------------------*/
void outbsnxcomf(FILE *fp)
{
    if(fp == NULL) return;
    trace(NULL,3, "outbsnxcomf: \n");
    fprintf(fp, "-FILE/COMMENT\n");
}

/* output BIAS/SOLUTION data --------------------------------------------------*/
static void outbsnxsold(FILE *fp, int cpflag, int n, const bia_t *biad,
    char *sat, char *sta)
{
    int i;
    char osb1str[8], osb2str[8];
    double sy, sdoy, stod, ey, edoy, etod;

    for(i = 0; i < n; i++) {
        strcpy(osb1str, code2sigs(biad[i].code[0], cpflag)); /* OBS1 */
        strcpy(osb2str, code2sigs(biad[i].code[1], cpflag)); /* OBS2 */
        time2biast(biad[i].t0, &sy, &sdoy, &stod); /* BIAS_START */
        time2biast(biad[i].t1, &ey, &edoy, &etod); /* BIAS_END */

        /* output */
        fprintf(fp, " %.4s %.4s %-3s ", typeid[biad[i].type], "    ", sat);
        fprintf(fp, "%-9s ", sta);
        fprintf(fp, "%-4s ", osb1str);
        fprintf(fp, "%-4s ", osb2str);
        fprintf(fp, "%04.0f:%03.0f:%05.0f %04.0f:%03.0f:%05.0f %-4s %21.14E %11.5E %21.14E %11.5E\n",
            sy, sdoy, stod, ey, edoy, etod, "ns",
            biad[i].val*M2NS, biad[i].valstd*M2NS,
            biad[i].slp*M2NS, biad[i].slpstd*M2NS);
    }
}

/* output BIAS-SINEX BIAS/SOLUTION ---------------------------------------------
* output the BIAS/SOLUTION section in a BIAS-SINEX file
* args   : FILE   *fp       I   output file pointer
* return : none
*-----------------------------------------------------------------------------*/
void outbsnxsol(FILE *fp)
{
    biass_t *biass;
    int i, j;
    char satid[8];

    if(fp == NULL) return;
    trace(NULL,3, "outbsnxsol: \n");

    biass = getbiass();

    fprintf(fp, "*-------------------------------------------------------------------------------\n");
    fprintf(fp, "+BIAS/SOLUTION\n");
    fprintf(fp, "*BIAS SVN_ PRN STATION__ OBS1 OBS2 BIAS_START____ BIAS_END______ UNIT __ESTIMATED_VALUE____ _STD_DEV___ __ESTIMATED_SLOPE____ _STD_DEV___\n");

    /* satellite code bias */
    for(i = 0; i < MAXSAT; i++) {
        satno2id(i + 1, satid);
        outbsnxsold(fp, 0, biass->sat.ncb[i], biass->sat.cb[i], satid, "");
    }

    /* satellite phase bias */
    for(i = 0; i < MAXSAT; i++) {
        satno2id(i + 1, satid);
        outbsnxsold(fp, 1, biass->sat.npb[i], biass->sat.pb[i], satid, "");
    }

    /* station system code bias*/
    for(i = 0; i < biass->nsta; i++) {
        for(j = 0; j < MAXBSNXSYS; j++) {
            sprintf(satid, "%c", syscode[j]);
            outbsnxsold(fp, 0, biass->sta[i].nsyscb[j], biass->sta[i].syscb[j],
                satid, biass->sta[i].name);
        }
    }

    /* station satellite code bias*/
    for(i = 0; i < biass->nsta; i++) {
        for(j = 0; j < MAXSAT; j++) {
            satno2id(j + 1, satid);
            outbsnxsold(fp, 0, biass->sta[i].nsatcb[j], biass->sta[i].satcb[j],
                satid, biass->sta[i].name);
        }
    }
    fprintf(fp, "-BIAS/SOLUTION\n");
    fprintf(fp, "%%=ENDBIA\n");
}

/* update satellite OSB -------------------------------------------------------
* update the satellite OSB (Observable Satellite Bias) structure
* with current biases
* args   : osb_t  *osb      IO  OSB structure to update
*          gtime_t gt       I   current time
*          int    mode      I   mode (0: current, 1: all)
* return : int              O   number of updated biases
*-----------------------------------------------------------------------------*/
int udosb_sat(osb_t *osb, gtime_t gt, int mode)
{
    biass_t *biass;
    bia_t   *bias;
    int i, j, ret = 0;

    biass = getbiass();

    for(j = 0; j < MAXCODE; j++) {
        for(i = 0; i < MAXSAT; i++) {
            osb->vscb[i][j] = 0;
            osb->vspb[i][j] = 0;
        }
    }
    for(i = 0; i < MAXSAT; i++) {
        for(j = 0; j < biass->sat.ncb[i]; j++) {
            bias = &biass->sat.cb[i][j];
            if(bias->code[0] == CODE_NONE) continue;
            if(mode || ((timediff(bias->t0, gt) <= 0.0) &&
                        (timediff(bias->t1, gt) >= 0.0))) {
                osb->vscb[i][bias->code[0] - 1] = 1;
                osb->scb [i][bias->code[0] - 1] = bias->val;
                ret++;
            }
        }
        for(j = 0; j < biass->sat.npb[i]; j++) {
            bias = &biass->sat.pb[i][j];
            if(bias->code[0] == CODE_NONE) continue;
            if(mode || ((timediff(bias->t0, gt) <= 0.0) &&
                        (timediff(bias->t1, gt) >= 0.0))) {
                osb->vspb[i][bias->code[0] - 1] = 1;
                osb->spb [i][bias->code[0] - 1] = bias->val;
                ret++;
            }
        }
    }
    osb->gt[0] = gt;
    return ret;
}

/* update station OSB ---------------------------------------------------------
* update the station OSB (Observable Satellite Bias) structure
* with current biases
* args   : osb_t  *osb      IO  OSB structure to update
*          gtime_t gt       I   current time
*          int    mode      I   mode (0: current, 1: all)
*          char   *name     I   station name
* return : int              O   number of updated biases
*-----------------------------------------------------------------------------*/
int udosb_station(osb_t *osb, gtime_t gt, int mode, char *name)
{
    biass_t *biass;
    bia_t   *bias;
    int i, j, k, ret = 0;

    biass = getbiass();

    for(k = 0; k < biass->nsta; k++) {
        if(strcmp(name, biass->sta[k].name) == 0) break;
    }
    if(k >= biass->nsta) return 0;

    for(j = 0; j < MAXCODE; j++) {
        for(i = 0; i < MAXBSNXSYS; i++) {
            osb->vrsyscb[i][j] = 0;
        }
        for(i = 0; i < MAXSAT; i++) {
            osb->vrsatcb[i][j] = 0;
        }
    }
    for(i = 0; i < MAXBSNXSYS; i++) {
        for(j = 0; j < biass->sta[k].nsyscb[i]; j++) {
            bias = &biass->sta[k].syscb[i][j];
            if(bias->code[0] == CODE_NONE) continue;
            if(mode ||
                ((timediff(bias->t0, gt) <= 0.0) &&
                 (timediff(bias->t1, gt) >= 0.0))) {
                osb->vrsyscb[i][bias->code[0] - 1] = 1;
                osb->rsyscb [i][bias->code[0] - 1] = bias->val;
                ret++;
            }
        }
    }
    for(i = 0; i < MAXSAT; i++) {
        for(j = 0; j < biass->sta[k].nsatcb[i]; j++) {
            bias = &biass->sta[k].satcb[i][j];
            if(bias->code[0] == CODE_NONE) continue;
            if(mode || ((timediff(bias->t0, gt) <= 0.0) &&
                (timediff(bias->t1, gt) >= 0.0))) {
                osb->vrsatcb[i][bias->code[0] - 1] = 1;
                osb->rsatcb [i][bias->code[0] - 1] = bias->val;
                ret++;
            }
        }
    }
    strncpy(osb->staname, name, 9);
    osb->gt[1] = gt;
    return ret;
}
