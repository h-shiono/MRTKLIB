/*------------------------------------------------------------------------------
 * cssr_parse.c : Byte-identical reproduction of upstream CLAS "dumpcssr"
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2015- Mitsubishi Electric Corp. (claslib cssr.c)
 * Copyright (C) 2007- T.TAKASU (RTKLIB rtkcmn.c primitives)
 *
 * Self-contained CSSR (Compact SSR / CLAS L6D) parser that reproduces the
 * upstream claslib dumpcssr CSV output (parse_cssr_type*.csv / header).
 *
 * This file is intentionally fully self-contained: it includes NO mrtklib
 * headers and exports a single entry point, cssr_parse_dump(). All decoding
 * logic, primitive helpers, and data types are vendored verbatim from
 * claslib-0.8.0 (src/cssr.c, src/cssr.h, src/rtkcmn.c, src/rtklib.h) so the
 * raw bit decode and printf formatting match upstream exactly. Glue functions
 * used only for the positioning bank (set_cssr_bank_*, etc.) are replaced by
 * no-op stubs because they have no effect on the dumped CSV fields.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/* ---- rtklib.h vendored macros --------------------------------------------*/
#define PI 3.1415926535897932
#define D2R (PI / 180.0)
#define R2D (180.0 / PI)
#define TRUE 1
#define FALSE 0

#define SYS_NONE 0x00
#define SYS_GPS 0x01
#define SYS_SBS 0x02
#define SYS_GLO 0x04
#define SYS_GAL 0x08
#define SYS_QZS 0x10
#define SYS_CMP 0x20
#define SYS_IRN 0x40
#define SYS_LEO 0x80
#define SYS_ALL 0xFF

#define MINPRNGPS 1
#define MAXPRNGPS 32
#define NSATGPS (MAXPRNGPS - MINPRNGPS + 1)
#define MINPRNGLO 1
#define MAXPRNGLO 24
#define NSATGLO (MAXPRNGLO - MINPRNGLO + 1)
#define MINPRNGAL 1
#define MAXPRNGAL 36
#define NSATGAL (MAXPRNGAL - MINPRNGAL + 1)
#define MINPRNQZS 193
#define MAXPRNQZS 199
#define NSATQZS (MAXPRNQZS - MINPRNQZS + 1)
#define MINPRNCMP 1
#define MAXPRNCMP 35
#define NSATCMP (MAXPRNCMP - MINPRNCMP + 1)
#define MINPRNLEO 1
#define MAXPRNLEO 10
#define NSATLEO (MAXPRNLEO - MINPRNLEO + 1)
#define MINPRNSBS 120
#define MAXPRNSBS 142
#define NSATSBS (MAXPRNSBS - MINPRNSBS + 1)
#define MAXSAT (NSATGPS + NSATGLO + NSATGAL + NSATQZS + NSATCMP + NSATSBS + NSATLEO)

#define MAXCODE 48
#define RTCM_SSR_MAX_GP 128
#define RTCM_SSR_GP_MAX_SAT 32
#define RTCM_SSR_GP_MAX_SIG 16
#define RTCM_SSR_VTEC_MAX_LAYER 4
#define MAX_INDEX_SSR 10
#define MAX_SAT_STEC_SSR 32
#define MAX_COEFS_STEC_SSR 6
#define MAX_SIGNAL_NUMB 10
#define SSR_CH_NUM 2

#define CSSR_MAX_SIG 16
#define CSSR_MAX_NETWORK 32

/* obs codes (rtklib.h) */
#define CODE_NONE 0
#define CODE_L1C 1
#define CODE_L1P 2
#define CODE_L1W 3
#define CODE_L1Y 4
#define CODE_L1M 5
#define CODE_L1N 6
#define CODE_L1S 7
#define CODE_L1L 8
#define CODE_L1E 9
#define CODE_L1A 10
#define CODE_L1B 11
#define CODE_L1X 12
#define CODE_L1Z 13
#define CODE_L2C 14
#define CODE_L2D 15
#define CODE_L2S 16
#define CODE_L2L 17
#define CODE_L2X 18
#define CODE_L2P 19
#define CODE_L2W 20
#define CODE_L2Y 21
#define CODE_L2M 22
#define CODE_L2N 23
#define CODE_L5I 24
#define CODE_L5Q 25
#define CODE_L5X 26
#define CODE_L7I 27
#define CODE_L7Q 28
#define CODE_L7X 29
#define CODE_L6A 30
#define CODE_L6B 31
#define CODE_L6C 32
#define CODE_L6X 33
#define CODE_L6Z 34
#define CODE_L6S 35
#define CODE_L6L 36
#define CODE_L8I 37
#define CODE_L8Q 38
#define CODE_L8X 39
#define CODE_L2I 40
#define CODE_L2Q 41
#define CODE_L6I 42
#define CODE_L6Q 43
#define CODE_L3I 44
#define CODE_L3Q 45
#define CODE_L3X 46
#define CODE_L1I 47
#define CODE_L1Q 48

/* ---- rtklib.h vendored types ---------------------------------------------*/
typedef struct { /* time struct */
    time_t time; /* time (s) expressed by standard time_t */
    double sec;  /* fraction of second under 1 s */
} gtime_t;

typedef struct { /* SSR correction type */
    gtime_t t0[MAX_INDEX_SSR];
    double udi[MAX_INDEX_SSR];
    int iod[MAX_INDEX_SSR];
    int iode;
    int iodcrc;
    int ura;
    int refd;
    double deph[3];
    double ddeph[3];
    double dclk[3];
    double hrclk;
    float cbias[MAXCODE];
    float pbias[MAXCODE];
    double pbiasn[CSSR_MAX_NETWORK][CSSR_MAX_SIG];
    float ppbiasl0[MAX_SIGNAL_NUMB];
    float l0bias;
    float ppbias[MAX_SIGNAL_NUMB];
    unsigned int discontinuity[MAXCODE];
    double yaw;
    double yawr;
    unsigned char update;
    unsigned char update_ura;
    unsigned char update_oc;
    unsigned char update_cc;
    unsigned char update_cb;
    unsigned char update_pb;
    unsigned int lng;
    int nsig;
    int ambfix;
    int smode[MAXCODE];
    float psig[MAXCODE];
    char hold_oc;
    int hold_oc_tow;
    double dlt_dclk;
} ssr_t;

typedef struct {
    float height;
    int order;
    int degree;
    int nc, ns;
    float c[153];
    float s[136];
    unsigned char update;
} ssr_vtec_t;

typedef struct {
    int model;
    float quality[MAX_SAT_STEC_SSR];
    int gp_origin;
    double gp0[3];
    int poly_type;
    int nsat;
    int sat[MAX_SAT_STEC_SSR];
    unsigned char flag[MAX_SAT_STEC_SSR];
    double a[MAX_SAT_STEC_SSR][MAX_COEFS_STEC_SSR];
    unsigned char update;
    int network;
} ssr_stec_t;

typedef struct { /* RTCM SSR ground point data */
    int type;
    double pos[3];
    int network;
    int update;
} ssr_gp_t;

typedef struct { /* RTCM SSR ionosphere correction type */
    gtime_t t0;
    double udi;
    int iod;
    int nlayer;
    float quality;
    ssr_vtec_t vtec[RTCM_SSR_VTEC_MAX_LAYER];
    ssr_stec_t stec;
    unsigned char update;
} ssrion_t;

typedef struct { /* RTCM SSR gridded troposphere/ionosphere correction type */
    gtime_t t0;
    double udi;
    int iod;
    int ngp;
    float quality;
    ssr_gp_t gp[RTCM_SSR_MAX_GP];
    float trop_total[RTCM_SSR_MAX_GP];
    float trop_wet[RTCM_SSR_MAX_GP];
    int nsv[RTCM_SSR_MAX_GP];
    int sat[RTCM_SSR_MAX_GP][RTCM_SSR_GP_MAX_SAT];
    float stec0[RTCM_SSR_MAX_GP][RTCM_SSR_GP_MAX_SAT];
    float stec[RTCM_SSR_MAX_GP][RTCM_SSR_GP_MAX_SAT];
    int updatesat[RTCM_SSR_MAX_GP][RTCM_SSR_GP_MAX_SAT];
    float resp[RTCM_SSR_MAX_GP][RTCM_SSR_GP_MAX_SAT][RTCM_SSR_GP_MAX_SIG];
    float resc[RTCM_SSR_MAX_GP][RTCM_SSR_GP_MAX_SAT][RTCM_SSR_GP_MAX_SIG];
    unsigned char nsig[RTCM_SSR_MAX_GP][RTCM_SSR_GP_MAX_SAT];
    unsigned char smode[RTCM_SSR_MAX_GP][RTCM_SSR_GP_MAX_SAT][RTCM_SSR_GP_MAX_SIG];
    unsigned char fix[RTCM_SSR_MAX_GP][RTCM_SSR_GP_MAX_SAT][RTCM_SSR_GP_MAX_SIG];
    unsigned char update;
    int network;
} ssrgp_t;

/* minimal navigation data: only fields the dump path touches */
typedef struct {
    int updateac;
    ssr_t ssr[MAXSAT];
} nav_t;

/* minimal RTCM control: only fields the dump path touches */
typedef struct {
    int ctype;
    int subtype;
    gtime_t time;
    nav_t nav;
    int nbyte;
    int nbit;
    int len;
    int havebit;
    unsigned char buff[1200];
    ssrion_t ssr_ion[CSSR_MAX_NETWORK];
    ssrgp_t ssrg[CSSR_MAX_NETWORK];
    gtime_t obs_ref[12];
    int week_ref[12];
    int tow_ref[12];
    int tow0;
} rtcm_t;

/* ---- cssr.h vendored macros/types ----------------------------------------*/
#define CSSR_MAX_GNSS 16
#define CSSR_MAX_SV_GNSS 40
#define CSSR_MAX_SV 64
#define CSSR_MAX_CELLMASK 64
#define CSSR_MAX_NET 32
#define CSSR_MAX_LOCAL_SV 32
#define CSSR_MAX_GP 128

#define CSSR_SYS_GPS 0
#define CSSR_SYS_GLO 1
#define CSSR_SYS_GAL 2
#define CSSR_SYS_BDS 3
#define CSSR_SYS_QZS 4
#define CSSR_SYS_SBS 5
#define CSSR_SYS_NONE -1

#define CSSR_TYPE_NUM 14
#define CSSR_TYPE_MASK 1
#define CSSR_TYPE_OC 2
#define CSSR_TYPE_CC 3
#define CSSR_TYPE_CB 4
#define CSSR_TYPE_PB 5
#define CSSR_TYPE_BIAS 6
#define CSSR_TYPE_URA 7
#define CSSR_TYPE_STEC 8
#define CSSR_TYPE_GRID 9
#define CSSR_TYPE_SI 10
#define CSSR_TYPE_COMBO 11
#define CSSR_TYPE_ATMOS 12

#define P2_S9_MAX 255
#define P2_S8_MAX 127

#define CSSR_TROP_HS_REF 2.3
#define CSSR_TROP_WET_REF 0.252

#define TROPVALIDAGE 3600
#define STECVALIDAGE 3600

#define INVALID_VALUE -10000

typedef struct {
    int stec_type;
} cssropt_t;

typedef struct {
    gtime_t t0[2];
    double udi[2];
    int iod[2];
    int ngp;
    float quality_f[CSSR_MAX_LOCAL_SV];
    float trop_wet[CSSR_MAX_GP];
    float trop_total[CSSR_MAX_GP];
    int nsat_f;
    int sat_f[CSSR_MAX_LOCAL_SV];
    float quality;
    double a[CSSR_MAX_LOCAL_SV][4];
    int nsat[CSSR_MAX_GP];
    int sat[CSSR_MAX_GP][CSSR_MAX_LOCAL_SV];
    float stec[CSSR_MAX_GP][CSSR_MAX_LOCAL_SV];
    double grid[CSSR_MAX_GP][3];
    int update[3];
} ssrn_t;

typedef struct {
    int ver;
    cssropt_t opt;
    int iod;
    int iod_sv;
    int inet;
    int week;
    uint8_t cmi[CSSR_MAX_GNSS];
    uint64_t svmask[CSSR_MAX_GNSS];
    uint16_t sigmask[CSSR_MAX_GNSS];
    uint16_t cellmask[CSSR_MAX_SV];
    uint64_t net_svmask[CSSR_MAX_NET];
    int ngnss;
    int nsat;
    int ncell;
    int sat[CSSR_MAX_SV];
    int nsat_n[CSSR_MAX_NET];
    int sat_n[CSSR_MAX_NET][CSSR_MAX_LOCAL_SV];
    int nsig[CSSR_MAX_SV];
    int sigmask_s[CSSR_MAX_SV];
    int amb_bias[MAXSAT][MAXCODE];
    uint8_t disc[MAXSAT][MAXCODE];
    float quality_i;
    int l6delivery;
    int l6facility;
    ssrn_t ssrn[CSSR_MAX_NET];
    int si_cnt;
    int si_sz;
    uint64_t si_data[4];
} cssr_t;

/* ---- cssr.c local macros -------------------------------------------------*/
#define L6FRMPREAMB 0x1ACFFC1Du /* L6 message frame preamble */
#define BLEN_MSG 218
#define L6_CH_NUM 2

enum {
    ref_mask = 0,
    ref_orbit,
    ref_clock,
    ref_cbias,
    ref_pbias,
    ref_bias,
    ref_ura,
    ref_stec,
    ref_grid,
    ref_service,
    ref_combined,
    ref_atmospheric
};

/* ssr update intervals ------------------------------------------------------*/
static const double ssrudint[16] = {1, 2, 5, 10, 15, 30, 60, 120, 240, 300, 600, 900, 1800, 3600, 7200, 10800};

static int l6delivery[L6_CH_NUM] = {-1, -1};
static int l6facility[L6_CH_NUM] = {-1, -1};

/* current reading cssr object idx */
static int chidx = 0;

/* grid storage filled by read_grid_def() */
static double clas_grid[CSSR_MAX_NETWORK][RTCM_SSR_MAX_GP][3];

/* grid definition version (read_grid_def references it via extern in upstream) */
static int gridsel = 0;

/* the dump path touches only the .separation field of the bank object */
static struct {
    int separation;
} cssrObject[L6_CH_NUM];

/* ---- rtkcmn.c vendored primitives ----------------------------------------*/
static const double gpst0[] = {1980, 1, 6, 0, 0, 0}; /* gps time reference */

static unsigned int getbitu(const unsigned char* buff, int pos, int len) {
    unsigned int bits = 0;
    int i;
    for (i = pos; i < pos + len; i++) bits = (bits << 1) + ((buff[i / 8] >> (7 - i % 8)) & 1u);
    return bits;
}
static int getbits(const unsigned char* buff, int pos, int len) {
    unsigned int bits = getbitu(buff, pos, len);
    if (len <= 0 || 32 <= len || !(bits & (1u << (len - 1)))) return (int)bits;
    return (int)(bits | (~0u << len)); /* extend sign */
}
static void setbitu(unsigned char* buff, int pos, int len, unsigned int data) {
    unsigned int mask = 1u << (len - 1);
    int i;
    if (len <= 0 || 32 < len) return;
    for (i = pos; i < pos + len; i++, mask >>= 1) {
        if (data & mask)
            buff[i / 8] |= 1u << (7 - i % 8);
        else
            buff[i / 8] &= ~(1u << (7 - i % 8));
    }
}
static int satno(int sys, int prn) {
    if (prn <= 0) return 0;
    switch (sys) {
        case SYS_GPS:
            if (prn < MINPRNGPS || MAXPRNGPS < prn) return 0;
            return prn - MINPRNGPS + 1;
        case SYS_GLO:
            if (prn < MINPRNGLO || MAXPRNGLO < prn) return 0;
            return NSATGPS + prn - MINPRNGLO + 1;
        case SYS_GAL:
            if (prn < MINPRNGAL || MAXPRNGAL < prn) return 0;
            return NSATGPS + NSATGLO + prn - MINPRNGAL + 1;
        case SYS_QZS:
            if (prn < MINPRNQZS || MAXPRNQZS < prn) return 0;
            return NSATGPS + NSATGLO + NSATGAL + prn - MINPRNQZS + 1;
        case SYS_CMP:
            if (prn < MINPRNCMP || MAXPRNCMP < prn) return 0;
            return NSATGPS + NSATGLO + NSATGAL + NSATQZS + prn - MINPRNCMP + 1;
        case SYS_LEO:
            if (prn < MINPRNLEO || MAXPRNLEO < prn) return 0;
            return NSATGPS + NSATGLO + NSATGAL + NSATQZS + NSATCMP + prn - MINPRNLEO + 1;
        case SYS_SBS:
            if (prn < MINPRNSBS || MAXPRNSBS < prn) return 0;
            return NSATGPS + NSATGLO + NSATGAL + NSATQZS + NSATCMP + NSATLEO + prn - MINPRNSBS + 1;
    }
    return 0;
}
static int satsys(int sat, int* prn) {
    int sys = SYS_NONE;
    if (sat <= 0 || MAXSAT < sat)
        sat = 0;
    else if (sat <= NSATGPS) {
        sys = SYS_GPS;
        sat += MINPRNGPS - 1;
    } else if ((sat -= NSATGPS) <= NSATGLO) {
        sys = SYS_GLO;
        sat += MINPRNGLO - 1;
    } else if ((sat -= NSATGLO) <= NSATGAL) {
        sys = SYS_GAL;
        sat += MINPRNGAL - 1;
    } else if ((sat -= NSATGAL) <= NSATQZS) {
        sys = SYS_QZS;
        sat += MINPRNQZS - 1;
    } else if ((sat -= NSATQZS) <= NSATCMP) {
        sys = SYS_CMP;
        sat += MINPRNCMP - 1;
    } else if ((sat -= NSATCMP) <= NSATLEO) {
        sys = SYS_LEO;
        sat += MINPRNLEO - 1;
    } else if ((sat -= NSATLEO) <= NSATSBS) {
        sys = SYS_SBS;
        sat += MINPRNSBS - 1;
    } else
        sat = 0;
    if (prn) *prn = sat;
    return sys;
}
static gtime_t epoch2time(const double* ep) {
    const int doy[] = {1, 32, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
    gtime_t time = {0};
    int days, sec, year = (int)ep[0], mon = (int)ep[1], day = (int)ep[2];
    if (year < 1970 || 2099 < year || mon < 1 || 12 < mon) return time;
    days = (year - 1970) * 365 + (year - 1969) / 4 + doy[mon - 1] + day - 2 + (year % 4 == 0 && mon >= 3 ? 1 : 0);
    sec = (int)floor(ep[5]);
    time.time = (time_t)days * 86400 + (int)ep[3] * 3600 + (int)ep[4] * 60 + sec;
    time.sec = ep[5] - sec;
    return time;
}
static void time2epoch(gtime_t t, double* ep) {
    const int mday[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
                        31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int days, sec, mon, day;
    days = (int)(t.time / 86400);
    sec = (int)(t.time - (time_t)days * 86400);
    for (day = days % 1461, mon = 0; mon < 48; mon++) {
        if (day >= mday[mon])
            day -= mday[mon];
        else
            break;
    }
    ep[0] = 1970 + days / 1461 * 4 + mon / 12;
    ep[1] = mon % 12 + 1;
    ep[2] = day + 1;
    ep[3] = sec / 3600;
    ep[4] = sec % 3600 / 60;
    ep[5] = sec % 60 + t.sec;
}
static gtime_t gpst2time(int week, double sec) {
    gtime_t t = epoch2time(gpst0);
    if (sec < -1E9 || 1E9 < sec) sec = 0.0;
    t.time += (time_t)86400 * 7 * week + (int)sec;
    t.sec = sec - (int)sec;
    return t;
}
static double time2gpst(gtime_t t, int* week) {
    gtime_t t0 = epoch2time(gpst0);
    time_t sec = t.time - t0.time;
    int w = (int)(sec / (86400 * 7));
    if (week) *week = w;
    return (double)(sec - (double)w * 86400 * 7) + t.sec;
}
static gtime_t timeadd(gtime_t t, double sec) {
    double tt;
    t.sec += sec;
    tt = floor(t.sec);
    t.time += (int)tt;
    t.sec -= tt;
    return t;
}
static double timediff(gtime_t t1, gtime_t t2) { return difftime(t1.time, t2.time) + t1.sec - t2.sec; }
static gtime_t timeget(void) {
    double ep[6] = {0};
    struct timeval tv;
    struct tm* tt;
    if (!gettimeofday(&tv, NULL) && (tt = gmtime(&tv.tv_sec))) {
        ep[0] = tt->tm_year + 1900;
        ep[1] = tt->tm_mon + 1;
        ep[2] = tt->tm_mday;
        ep[3] = tt->tm_hour;
        ep[4] = tt->tm_min;
        ep[5] = tt->tm_sec + tv.tv_usec * 1E-6;
    }
    return timeadd(epoch2time(ep), 0.0);
}
static void time2str(gtime_t t, char* s, int n) {
    double ep[6];
    if (n < 0)
        n = 0;
    else if (n > 12)
        n = 12;
    if (1.0 - t.sec < 0.5 / pow(10.0, n)) {
        t.time++;
        t.sec = 0.0;
    };
    time2epoch(t, ep);
    sprintf(s, "%04.0f/%02.0f/%02.0f %02.0f:%02.0f:%0*.*f", ep[0], ep[1], ep[2], ep[3], ep[4], n <= 0 ? 2 : n + 3,
            n <= 0 ? 0 : n, ep[5]);
}

/* ---- no-op glue stubs ----------------------------------------------------*/
static void trace(int level, const char* format, ...) {
    (void)level;
    (void)format;
}
static void showmsg(const char* format, ...) { (void)format; }
static void init_rtcm(rtcm_t* rtcm) { memset(rtcm, 0, sizeof(rtcm_t)); }
static void free_rtcm(rtcm_t* rtcm) { (void)rtcm; }
static void set_cssr_bank_orbit(gtime_t time, nav_t* nav, int network) {
    (void)time;
    (void)nav;
    (void)network;
}
static void set_cssr_bank_clock(gtime_t time, nav_t* nav, int network) {
    (void)time;
    (void)nav;
    (void)network;
}
static void set_cssr_bank_cbias(gtime_t time, nav_t* nav, int network, int iod) {
    (void)time;
    (void)nav;
    (void)network;
    (void)iod;
}
static void set_cssr_bank_pbias(gtime_t time, nav_t* nav, int network, int iod) {
    (void)time;
    (void)nav;
    (void)network;
    (void)iod;
}
static void set_cssr_bank_trop(gtime_t time, ssrgp_t* ssrg, int network) {
    (void)time;
    (void)ssrg;
    (void)network;
}
static void set_cssr_latest_trop(gtime_t time, ssrgp_t* ssrg, int network) {
    (void)time;
    (void)ssrg;
    (void)network;
}
static void check_cssr_changed_facility(int facility) { (void)facility; }
static double decode_sval(unsigned char* buff, int i, int n, double lsb) {
    int slim = -((1 << (n - 1)) - 1) - 1, v;
    v = getbits(buff, i, n);
    return (v == slim) ? INVALID_VALUE : (double)v * lsb;
}

static int sys2gnss(int sys, int* prn_min) {
    int id = CSSR_SYS_NONE;

    if (prn_min) {
        *prn_min = 1;
    }

    switch (sys) {
        case SYS_GPS:
            id = CSSR_SYS_GPS;
            break;
        case SYS_GLO:
            id = CSSR_SYS_GLO;
            break;
        case SYS_GAL:
            id = CSSR_SYS_GAL;
            break;
        case SYS_CMP:
            id = CSSR_SYS_BDS;
            break;
        case SYS_SBS:
            id = CSSR_SYS_SBS;
            if (prn_min) {
                *prn_min = 120;
            }
            break;
        case SYS_QZS:
            id = CSSR_SYS_QZS;
            if (prn_min) {
                *prn_min = 193;
            }
            break;
    }

    return id;
}

/* convert GNSS ID of cssr to system id of rtklib */
static int gnss2sys(int id, int* prn_min) {
    int sys = SYS_NONE;

    if (prn_min) {
        *prn_min = 1;
    }

    switch (id) {
        case CSSR_SYS_GPS:
            sys = SYS_GPS;
            break;
        case CSSR_SYS_GLO:
            sys = SYS_GLO;
            break;
        case CSSR_SYS_GAL:
            sys = SYS_GAL;
            break;
        case CSSR_SYS_BDS:
            sys = SYS_CMP;
            break;
        case CSSR_SYS_SBS:
            sys = SYS_SBS;
            if (prn_min) {
                *prn_min = 120;
            }
            break;
        case CSSR_SYS_QZS:
            sys = SYS_QZS;
            if (prn_min) {
                *prn_min = 193;
            }
            break;
    }

    return sys;
}

/*
 * count number of satellite in satellite mask
 */
static int svmask2nsat(uint64_t svmask) {
    int j, nsat = 0;

    for (j = 0; j < CSSR_MAX_SV_GNSS; j++) {
        if ((svmask >> (CSSR_MAX_SV_GNSS - 1 - j)) & 1) {
            nsat++;
        }
    }

    return nsat;
}

/*
 * count number of signals in signal mask
 */
static int sigmask2nsig(uint16_t sigmask) {
    int j, nsig = 0;

    for (j = 0; j < CSSR_MAX_SIG; j++) {
        if ((sigmask >> j) & 1) {
            nsig++;
        }
    }
    return nsig;
}

static int svmask2nsatlist(uint64_t svmask, int id, int* sat) {
    int j, nsat = 0, sys, prn_min;

    sys = gnss2sys(id, &prn_min);
    for (j = 0; j < CSSR_MAX_SV_GNSS; j++) {
        if ((svmask >> (CSSR_MAX_SV_GNSS - 1 - j)) & 1) {
            sat[nsat++] = satno(sys, prn_min + j);
        }
    }
    return nsat;
}

/* convert from svmask to satellite list */
static int svmask2sat(uint64_t* svmask, int* sat) {
    int j, id, nsat = 0, sys, prn_min;

    for (id = 0; id < CSSR_MAX_GNSS; id++) {
        sys = gnss2sys(id, &prn_min);
        for (j = 0; j < CSSR_MAX_SV_GNSS; j++) {
            if ((svmask[id] >> (CSSR_MAX_SV_GNSS - 1 - j)) & 1) {
                if (sat) sat[nsat] = satno(sys, prn_min + j);
                nsat++;
            }
        }
    }
    return nsat;
}

/* decode stec quality indicator */
static float decode_cssr_quality_stec(int a, int b) {
    float quality;

    if ((a == 0 && b == 0) || (a == 7 && b == 7)) {
        quality = 9999 * 1000;
    } else {
        quality = (1.0 + b * 0.25) * pow(3.0, a) - 1.0;
    }

    return quality;
}

/* decode tropo quality indicator */
static float decode_cssr_quality_trop(int a, int b) {
    float quality;

    if ((a == 0 && b == 0) || (a == 7 && b == 7)) {
        quality = 9999;
    } else {
        quality = (1.0 + b * 0.25) * pow(3.0, a) - 1.0;
    }

    return quality;
}

static void check_week_ref(rtcm_t* rtcm, int tow, int i) {
    if (rtcm->obs_ref[i].time != 0 || rtcm->obs_ref[i].sec != 0.0) {
        gtime_t time = gpst2time(rtcm->week_ref[i], tow);
        if (timediff(time, rtcm->obs_ref[i]) > (86400 * 7 / 2)) {
            char temp1[64], temp2[64];
            time2str(rtcm->obs_ref[i], temp1, 0);
            time2str(time, temp2, 0);
            trace(2, "check_week_ref(): CSSR message time is big, subtype=%2d, time=%s, obstime=%s\n", i + 1, temp2,
                  temp1);
            trace(2, "check_week_ref(): adjust reference week, subtype=%2d, week=%d\n", i + 1, --rtcm->week_ref[i]);
        }
        rtcm->obs_ref[i].time = rtcm->obs_ref[i].sec = 0.0;
    }
    if (rtcm->tow0 != -1) {
        if (rtcm->tow_ref[i] != -1 && ((tow - rtcm->tow_ref[i]) < (-86400 * 7 / 2))) {
            ++rtcm->week_ref[i];
        }
        rtcm->tow_ref[i] = tow;
    }
}

static void output_cssr_head(rtcm_t* rtcm, cssr_t* cssr, int sync, int tow, int udi, int iod, int ngnss, FILE* fp) {
    if (fp == NULL) return;

    fprintf(fp, "%d, %d, %d, %d, %d, %d", rtcm->ctype, rtcm->subtype, tow, udi, sync, iod);
    if (rtcm->subtype == CSSR_TYPE_MASK) {
        fprintf(fp, ", %d", ngnss);
    }
}

/* decode cssr header ---------------------------------------------------------*/
static int decode_cssr_head(rtcm_t* rtcm, cssr_t* cssr, int* sync, int* tow, int* iod, int* iod_sv, double* udint,
                            int* ngnss, int i0, int header, FILE* fp) {
    int i = i0, udi;

    if (rtcm->subtype == CSSR_TYPE_MASK) {
        *tow = getbitu(rtcm->buff, i, 20);
        i += 20; /* gps epoch time */
    } else {
        *tow = rtcm->tow0 + getbitu(rtcm->buff, i, 12);
        i += 12; /* gps epoch time (hourly) */
    }

    trace(4, "decode_cssr_head: subtype=%d epoch=%4d\n", rtcm->subtype, *tow);

    udi = getbitu(rtcm->buff, i, 4);
    i += 4; /* update interval */
    *sync = getbitu(rtcm->buff, i, 1);
    i += 1; /* multiple message indicator */
    *udint = ssrudint[udi];
    *iod = getbitu(rtcm->buff, i, 4);
    i += 4; /* iod ssr */

    if (rtcm->subtype == CSSR_TYPE_MASK) {
        cssr->iod = *iod;
        *ngnss = getbitu(rtcm->buff, i, 4);
        i += 4; /* number of gnss */
    }
    output_cssr_head(rtcm, cssr, *sync, *tow, udi, *iod, *ngnss, fp);

    return i;
}

static void output_cssr_mask(rtcm_t* rtcm, cssr_t* cssr, int ngnss, int* id_, FILE* fp) {
    int nsat_g = 0, ncell = 0, prn, sat[CSSR_MAX_SV];
    int j, k, id;

    if (fp == NULL) return;

    trace(4, "output_cssr_mask: ngnss=%d\n", ngnss);
    for (k = 0; k < ngnss; k++) {
        if (k != 0) {
            fprintf(fp, ",,,,,,, ");
        } else {
            fprintf(fp, ", ");
        }
        id = id_[k];
        fprintf(fp, "%d, ", id);
        fprintf(fp, "0x%02x", (uint32_t)(cssr->svmask[id] >> 32));
        fprintf(fp, "%08lx, ", (uint64_t)cssr->svmask[id] & 0x00000000ffffffff);
        fprintf(fp, "0x%04x, ", cssr->sigmask[id]);
        fprintf(fp, "%d", cssr->cmi[id]);
        nsat_g = svmask2nsatlist(cssr->svmask[id], id, sat);

        if (cssr->cmi[id]) { /* cell-mask is included */
            for (j = 0; j < nsat_g; j++) {
                satsys(sat[j], &prn);
                if (j != 0) {
                    fprintf(fp, ",,,,,,,,,,, %d, 0x%04x\n", prn, cssr->cellmask[ncell]);
                } else {
                    fprintf(fp, ", %d, 0x%04x\n", prn, cssr->cellmask[ncell]);
                }
                ncell++;
            }
        } else {
            fprintf(fp, "\n");
        }
    }
    if (ngnss <= 0) {
        fprintf(fp, "\n");
    }
}

/* decode mask message */
static int decode_cssr_mask(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp) {
    int i, j, k, l, sync, tow, ngnss, iod, nsat_g = 0, id, nsig, ncell = 0, prn, sat[CSSR_MAX_SV];
    int id_[CSSR_MAX_GNSS];
    double udint;

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp);
    rtcm->tow0 = floor(tow / 3600.0) * 3600.0;
    check_week_ref(rtcm, tow, ref_mask);
    rtcm->time = gpst2time(rtcm->week_ref[ref_mask], tow);
    for (j = 0; j < CSSR_MAX_GNSS; j++) {
        cssr->cmi[j] = 0;
        cssr->svmask[j] = 0;
        cssr->sigmask[j] = 0;
    }
    for (j = 0; j < CSSR_MAX_SV; j++) {
        cssr->cellmask[j] = 0;
    }

    trace(2, "decode_cssr_mask: facility=%d tow=%d iod=%d\n", l6facility[chidx] + 1, tow, cssr->iod);

    for (k = 0; k < ngnss; k++) {
        id_[k] = id = getbitu(rtcm->buff, i, 4);
        i += 4; /* gnss id */
        cssr->svmask[id] = (uint64_t)getbitu(rtcm->buff, i, 8) << 32;
        i += 8; /* sv mask */
        cssr->svmask[id] |= getbitu(rtcm->buff, i, 32);
        i += 32; /* sv mask */
        cssr->sigmask[id] = getbitu(rtcm->buff, i, 16);
        i += 16; /* signal mask */
        cssr->cmi[id] = getbitu(rtcm->buff, i, 1);
        i++; /* cell mask availability */

        nsig = sigmask2nsig(cssr->sigmask[id]);
        nsat_g = svmask2nsatlist(cssr->svmask[id], id, sat);

        if (cssr->cmi[id]) { /* cell-mask is included */
            for (j = 0; j < nsat_g; j++) {
                cssr->cellmask[ncell] = getbitu(rtcm->buff, i, nsig);
                i += nsig;
                satsys(sat[j], &prn);
                ncell++;
            }
        } else {
            for (j = 0; j < nsat_g; j++) {
                for (l = 0; l < nsig; l++) {
                    cssr->cellmask[ncell] |= ((uint16_t)1 << (nsig - 1 - l));
                }
                ++ncell;
            }
        }
    }
    output_cssr_mask(rtcm, cssr, ngnss, id_, fp);
    cssr->l6delivery = l6delivery[chidx];
    cssr->l6facility = l6facility[chidx];
    rtcm->nbit = i;
    return sync ? 0 : 10;
}

/* check if the buffer length is enough to decode the mask message */
static int check_bit_width_mask(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int k, ngnss = 0, cmi = 0, nsig, nsat;
    uint16_t sigmask;
    uint64_t svmask;

    if (i0 + 49 > rtcm->havebit) return FALSE;

    ngnss = getbitu(rtcm->buff, i0 + 45, 4);
    i0 += 49;

    for (k = 0; k < ngnss; k++) {
        if (i0 + 61 > rtcm->havebit) return FALSE;
        cmi = getbitu(rtcm->buff, i0 + 60, 1);
        i0 += 61;
        if (cmi) {
            svmask = (uint64_t)getbitu(rtcm->buff, i0, 8) << 32;
            i0 += 8;
            svmask |= (uint64_t)getbitu(rtcm->buff, i0, 32);
            i0 += 32;
            sigmask = getbitu(rtcm->buff, i0, 16);
            i0 += 16;
            nsat = svmask2nsat(svmask);
            nsig = sigmask2nsig(sigmask);
            if (i0 + nsat * nsig > rtcm->havebit) return FALSE;
            i0 += nsat * nsig;
        }
    }
    return TRUE;
}

static void output_cssr_oc(rtcm_t* rtcm, cssr_t* cssr, FILE* fp) {
    int j, prn, sat[CSSR_MAX_SV], iode, nsat, gnss;
    ssr_t* ssr = NULL;

    if (fp == NULL) return;
    nsat = svmask2sat(cssr->svmask, sat);
    for (j = 0; j < nsat; j++) {
        ssr = &rtcm->nav.ssr[sat[j] - 1];
        gnss = sys2gnss(satsys(sat[j], &prn), NULL);
        iode = ssr->iode;
        if (j != 0) {
            fprintf(fp, ",,,,,, %d, %d, %d, ", gnss, prn, iode);
        } else {
            fprintf(fp, ", %d, %d, %d, ", gnss, prn, iode);
        }

        if (ssr->deph[0] != INVALID_VALUE) {
            fprintf(fp, "%f, ", (double)ssr->deph[0]);
        } else {
            fprintf(fp, "#N/A, ");
        }
        if (ssr->deph[1] != INVALID_VALUE) {
            fprintf(fp, "%f, ", (double)ssr->deph[1]);
        } else {
            fprintf(fp, "#N/A, ");
        }
        if (ssr->deph[2] != INVALID_VALUE) {
            fprintf(fp, "%f\n", (double)ssr->deph[2]);
        } else {
            fprintf(fp, "#N/A\n");
        }
    }
    if (nsat == 0) {
        fprintf(fp, "\n");
    }
}

/* decode orbit correction */
static int decode_cssr_oc(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp) {
    int i, j, k, iod, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat, sys, iode;
    double udint;
    ssr_t* ssr = NULL;

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp);
    nsat = svmask2sat(cssr->svmask, sat);
    check_week_ref(rtcm, tow, ref_orbit);
    rtcm->time = gpst2time(rtcm->week_ref[ref_orbit], tow);

    trace(2, "decode_cssr_oc:   facility=%d tow=%d iod=%d\n", l6facility[chidx] + 1, tow, iod);
    if (cssr->l6facility != l6facility[chidx]) {
        trace(2, "cssr: facility mismatch: tow=%d mask_facility=%d subtype=%d facility=%d\n", tow, cssr->l6facility,
              rtcm->subtype, l6facility[chidx]);
        return -1;
    }
    if (cssr->iod != iod) {
        trace(2, "cssr: iod mismatch: tow=%d mask_iod=%d subtype=%d iod=%d\n", tow, cssr->iod, rtcm->subtype, iod);
        return -1;
    }

    for (j = 0; j < MAXSAT; ++j) {
        ssr = &rtcm->nav.ssr[j];
        ssr->t0[0].sec = 0.0;
        ssr->t0[0].time = 0;
        ssr->udi[0] = 0;
        ssr->iod[0] = 0;
        ssr->update_oc = 0;
        ssr->update = 0;
        ssr->iode = 0;
        ssr->deph[0] = 0.0;
        ssr->deph[1] = 0.0;
        ssr->deph[2] = 0.0;
    }

    for (j = 0; j < nsat; j++) {
        ssr = &rtcm->nav.ssr[sat[j] - 1];
        sys = satsys(sat[j], NULL);

        if (sys == SYS_GAL) {
            iode = getbitu(rtcm->buff, i, 10);
            i += 10; /* iode */
        } else {
            iode = getbitu(rtcm->buff, i, 8);
            i += 8; /* iode */
        }

        /* delta radial/along-track/cross-track */
        ssr->deph[0] = decode_sval(rtcm->buff, i, 15, 0.0016);
        i += 15;
        ssr->deph[1] = decode_sval(rtcm->buff, i, 13, 0.0064);
        i += 13;
        ssr->deph[2] = decode_sval(rtcm->buff, i, 13, 0.0064);
        i += 13;
        if (ssr->deph[0] == INVALID_VALUE || ssr->deph[1] == INVALID_VALUE || ssr->deph[2] == INVALID_VALUE) {
            trace(3, "invalid orbit value: tow=%d, sat=%d, value=%d %d %d\n", tow, sat[j], ssr->deph[0], ssr->deph[1],
                  ssr->deph[2]);
        }
        ssr->iode = iode;

        ssr->t0[0] = rtcm->time;
        ssr->udi[0] = udint;
        ssr->iod[0] = cssr->iod;
        if (ssr->deph[0] == INVALID_VALUE || ssr->deph[1] == INVALID_VALUE || ssr->deph[2] == INVALID_VALUE) {
            ssr->deph[0] = INVALID_VALUE;
            ssr->deph[1] = INVALID_VALUE;
            ssr->deph[2] = INVALID_VALUE;
        }

        for (k = 0; k < 3; k++) ssr->ddeph[k] = 0.0;

        ssr->update_oc = 1;
        ssr->update = 1;
        trace(4, "ssr orbit: prn=%2d, tow=%d, udi=%.1f, iod=%2d, orb=%f,%f,%f\n", sat[j], tow, udint, cssr->iod,
              ssr->deph[0], ssr->deph[1], ssr->deph[2]);
    }
    output_cssr_oc(rtcm, cssr, fp);

    check_cssr_changed_facility(cssr->l6facility);
    set_cssr_bank_orbit(rtcm->time, &rtcm->nav, 0);
    rtcm->nbit = i;
    return sync ? 0 : 10;
}

/* check if the buffer length is enough to decode the orbit correction message */
static int check_bit_width_oc(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int k, sat[CSSR_MAX_SV], nsat, prn;

    i0 += 21;
    if (i0 > rtcm->havebit) return FALSE;
    nsat = svmask2sat(cssr->svmask, sat);
    for (k = 0; k < nsat; k++) {
        i0 += (satsys(sat[k], &prn) == SYS_GAL) ? 51 : 49;
        if (i0 > rtcm->havebit) return FALSE;
    }
    return TRUE;
}

static void output_cssr_cc(rtcm_t* rtcm, cssr_t* cssr, FILE* fp) {
    int j, tow, sat[CSSR_MAX_SV], nsat, prn, gnss, week;
    ssr_t* ssr = NULL;

    if (fp == NULL) return;
    tow = time2gpst(rtcm->time, &week);
    nsat = svmask2sat(cssr->svmask, sat);

    for (j = 0; j < nsat; j++) {
        ssr = &rtcm->nav.ssr[sat[j] - 1];
        gnss = sys2gnss(satsys(sat[j], &prn), NULL);
        if (j != 0) {
            fprintf(fp, ",,,,,, %d, %d, ", gnss, prn);
        } else {
            fprintf(fp, ", %d, %d, ", gnss, prn);
        }

        if (ssr->dclk[0] == INVALID_VALUE) {
            trace(3, "invalid clock value: tow=%d, sat=%d, value=%d\n", tow, sat[j], ssr->dclk[0]);
            fprintf(fp, "#N/A\n");
        } else {
            fprintf(fp, "%f\n", (double)ssr->dclk[0]);
        }
    }
    if (nsat == 0) {
        fprintf(fp, "\n");
    }
}

/* decode clock correction */
static int decode_cssr_cc(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp) {
    int i, j, iod, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat;
    double udint;
    ssr_t* ssr = NULL;

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp);
    check_week_ref(rtcm, tow, ref_clock);
    rtcm->time = gpst2time(rtcm->week_ref[ref_clock], tow);
    nsat = svmask2sat(cssr->svmask, sat);

    trace(2, "decode_cssr_cc:   facility=%d tow=%d iod=%d\n", l6facility[chidx] + 1, tow, iod);
    if (cssr->l6facility != l6facility[chidx]) {
        trace(2, "cssr: facility mismatch: tow=%d mask_facility=%d subtype=%d facility=%d\n", tow, cssr->l6facility,
              rtcm->subtype, l6facility[chidx]);
        return -1;
    }
    if (cssr->iod != iod) {
        trace(2, "cssr: iod mismatch: tow=%d mask_iod=%d subtype=%d iod=%d\n", tow, cssr->iod, rtcm->subtype, iod);
        return -1;
    }

    for (j = 0; j < MAXSAT; ++j) {
        ssr = &rtcm->nav.ssr[j];
        ssr->t0[1].sec = 0.0;
        ssr->t0[1].time = 0;
        ssr->udi[1] = 0;
        ssr->iod[1] = 0;
        ssr->update_cc = 0;
        ssr->update = 0;
        ssr->dclk[0] = 0.0;
    }

    for (j = 0; j < nsat; j++) {
        ssr = &rtcm->nav.ssr[sat[j] - 1];
        ssr->t0[1] = rtcm->time;
        ssr->udi[1] = udint;
        ssr->iod[1] = cssr->iod;

        ssr->dclk[0] = decode_sval(rtcm->buff, i, 15, 0.0016);
        i += 15;
        if (ssr->dclk[0] == INVALID_VALUE) {
            trace(3, "invalid clock value: tow=%d, sat=%d, value=%d\n", tow, sat[j], ssr->dclk[0]);
        }
        ssr->dclk[1] = ssr->dclk[2] = 0.0;
        ssr->update_cc = 1;
        ssr->update = 1;
        trace(4, "ssr clock: prn=%2d, tow=%d, udi=%.1f, iod=%2d, clk=%f\n", sat[j], tow, udint, cssr->iod,
              ssr->dclk[0]);
    }
    output_cssr_cc(rtcm, cssr, fp);
    check_cssr_changed_facility(cssr->l6facility);
    set_cssr_bank_clock(rtcm->time, &rtcm->nav, 0);
    rtcm->nbit = i;
    return sync ? 0 : 10;
}

static int check_bit_width_cc(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int nsat;

    nsat = svmask2sat(cssr->svmask, NULL);
    return (i0 + 21 + 15 * nsat <= rtcm->havebit);
}

static int sigmask2sig_p(int nsat, int* sat, uint16_t* sigmask, uint16_t* cellmask, int* nsig, int* sig) {
    int j, k, id, sys, sys_p = -1, nsig_s = 0, code[CSSR_MAX_SIG];

    for (j = 0; j < nsat; j++) {
        sys = satsys(sat[j], NULL);
        if (sys != sys_p) {
            id = sys2gnss(sys, NULL);
            for (k = 0, nsig_s = 0; k < CSSR_MAX_SIG; k++) {
                if ((sigmask[id] >> (CSSR_MAX_SIG - 1 - k)) & 1) {
                    code[nsig_s] = k;
                    nsig_s++;
                }
            }
        }
        sys_p = sys;

        for (k = 0, nsig[j] = 0; k < nsig_s; k++) {
            if ((cellmask[j] >> (nsig_s - 1 - k)) & 1) {
                if (sig) sig[j * CSSR_MAX_SIG + nsig[j]] = code[k];
                nsig[j]++;
            }
        }
    }

    return 1;
}

/* decode available signals from sigmask */
static int sigmask2sig(int nsat, int* sat, uint16_t* sigmask, uint16_t* cellmask, int* nsig, int* sig) {
    int j, k, id, *codes = NULL, sys, sys_p = -1, ofst = 0, nsig_s = 0, code[CSSR_MAX_SIG];
    const int codes_gps[] = {CODE_L1C, CODE_L1P, CODE_L1W, CODE_L1S, CODE_L1L, CODE_L1X, CODE_L2S,
                             CODE_L2L, CODE_L2X, CODE_L2P, CODE_L2W, CODE_L5I, CODE_L5Q, CODE_L5X};
    const int codes_glo[] = {CODE_L1C, CODE_L1P, CODE_L2C, CODE_L2P, CODE_L3I, CODE_L3Q, CODE_L3X};
    const int codes_gal[] = {CODE_L1B, CODE_L1C, CODE_L1X, CODE_L5I, CODE_L5Q, CODE_L5X,
                             CODE_L7I, CODE_L7Q, CODE_L7X, CODE_L8I, CODE_L8Q, CODE_L8X};
    const int codes_qzs[] = {CODE_L1C, CODE_L1S, CODE_L1L, CODE_L1X, CODE_L2S,
                             CODE_L2L, CODE_L2X, CODE_L5I, CODE_L5Q, CODE_L5X};
    const int codes_bds[] = {CODE_L2I, CODE_L2Q, CODE_L2X, CODE_L6I, CODE_L6Q, CODE_L6X, CODE_L7I, CODE_L7Q, CODE_L7X};
    const int codes_sbs[] = {CODE_L1C, CODE_L5I, CODE_L5Q, CODE_L5X};

    for (j = 0; j < nsat; j++, ofst += nsig_s) {
        sys = satsys(sat[j], NULL);
        if (sys != sys_p) {
            id = sys2gnss(sys, NULL);
            ofst = 0;
            switch (sys) {
                case SYS_GPS:
                    codes = (int*)codes_gps;
                    break;
                case SYS_GLO:
                    codes = (int*)codes_glo;
                    break;
                case SYS_GAL:
                    codes = (int*)codes_gal;
                    break;
                case SYS_CMP:
                    codes = (int*)codes_bds;
                    break;
                case SYS_QZS:
                    codes = (int*)codes_qzs;
                    break;
                case SYS_SBS:
                    codes = (int*)codes_sbs;
                    break;
            }
            for (k = 0, nsig_s = 0; k < CSSR_MAX_SIG; k++) {
                if ((sigmask[id] >> (CSSR_MAX_SIG - 1 - k)) & 1) {
                    code[nsig_s] = codes[k];
                    nsig_s++;
                }
            }
        }
        sys_p = sys;

        for (k = 0, nsig[j] = 0; k < nsig_s; k++) {
            if ((cellmask[j] >> (nsig_s - 1 - k)) & 1) {
                if (sig) sig[j * CSSR_MAX_SIG + nsig[j]] = code[k];
                nsig[j]++;
            }
        }
    }

    return 1;
}

static void output_cssr_cb(rtcm_t* rtcm, cssr_t* cssr, FILE* fp) {
    int k, j, sat[CSSR_MAX_SV], s, tow, lcnt, prn, nsat, gnss, week;
    ssr_t* ssr = NULL;
    int first = TRUE;
    int nsig[CSSR_MAX_SV], sig[CSSR_MAX_SV * CSSR_MAX_SIG], sig_p[CSSR_MAX_SV * CSSR_MAX_SIG];

    if (fp == NULL) return;
    tow = time2gpst(rtcm->time, &week);
    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig_p(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig_p);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig);

    for (k = lcnt = 0; k < nsat; k++) {
        ssr = &rtcm->nav.ssr[sat[k] - 1];
        for (j = 0; j < nsig[k]; j++) {
            gnss = sys2gnss(satsys(sat[k], &prn), NULL);
            if (first == FALSE) {
                fprintf(fp, ",,,,,, %d, %d, %d, ", gnss, prn, sig_p[k * CSSR_MAX_SIG + j]);
            } else {
                fprintf(fp, ", %d, %d, %d, ", gnss, prn, sig_p[k * CSSR_MAX_SIG + j]);
                first = FALSE;
            }
            s = sig[k * CSSR_MAX_SIG + j];
            /* code bias */
            if (ssr->cbias[s - 1] == INVALID_VALUE) {
                trace(3, "invalid cb value: tow=%d, sat=%d, value=%.2f\n", tow, sat[k], ssr->cbias[s - 1]);
                fprintf(fp, "#N/A\n");
            } else {
                fprintf(fp, "%f\n", ssr->cbias[s - 1]);
            }
            ++lcnt;
            ssr->smode[j] = s;
        }
        ssr->nsig = nsig[k];
    }
    if (lcnt == 0) {
        fprintf(fp, "\n");
    }
}

/* decode code bias message */
static int decode_cssr_cb(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp) {
    int i, j, k, iod, s, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat, lcnt;
    int nsig[CSSR_MAX_SV], sig[CSSR_MAX_SV * CSSR_MAX_SIG], sig_p[CSSR_MAX_SV * CSSR_MAX_SIG];
    double udint;
    ssr_t* ssr = NULL;

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp);
    check_week_ref(rtcm, tow, ref_cbias);
    rtcm->time = gpst2time(rtcm->week_ref[ref_cbias], tow);
    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig_p(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig_p);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig);

    trace(2, "decode_cssr_cb:   facility=%d tow=%d iod=%d\n", l6facility[chidx] + 1, tow, iod);
    if (cssr->l6facility != l6facility[chidx]) {
        trace(2, "cssr: facility mismatch: tow=%d mask_facility=%d subtype=%d facility=%d\n", tow, cssr->l6facility,
              rtcm->subtype, l6facility[chidx]);
        return -1;
    }
    if (cssr->iod != iod) {
        trace(2, "cssr: iod mismatch: tow=%d mask_iod=%d subtype=%d iod=%d\n", tow, cssr->iod, rtcm->subtype, iod);
        return -1;
    }

    for (k = 0; k < MAXSAT; ++k) {
        ssr = &rtcm->nav.ssr[k];
        ssr->t0[4].sec = 0.0;
        ssr->t0[4].time = 0;
        ssr->udi[4] = 0;
        ssr->iod[4] = 0;
        ssr->update_cb = 0;
        ssr->update = 0;
        ssr->nsig = 0;

        for (j = 0; j < MAXCODE; ++j) {
            /* code bias */
            ssr->cbias[j] = 0.0;
            ssr->smode[j] = 0;
        }
    }

    for (k = lcnt = 0; k < nsat; k++) {
        ssr = &rtcm->nav.ssr[sat[k] - 1];
        ssr->t0[4] = rtcm->time;
        ssr->udi[4] = udint;
        ssr->iod[4] = cssr->iod;
        ssr->update_cb = 1;
        ssr->update = 1;

        for (j = 0; j < nsig[k]; j++) {
            s = sig[k * CSSR_MAX_SIG + j];
            /* code bias */
            ssr->cbias[s - 1] = decode_sval(rtcm->buff, i, 11, 0.02);
            i += 11;
            if (ssr->cbias[s - 1] == INVALID_VALUE) {
                trace(3, "invalid cb value: tow=%d, sat=%d, value=%d\n", tow, sat[k], ssr->cbias[s - 1]);
            }
            trace(4, "ssr cbias: prn=%2d, tow=%d, udi=%.1f, iod=%2d, cbias=%f\n", sat[k], tow, udint, cssr->iod,
                  ssr->cbias[s - 1]);
            ++lcnt;
            ssr->smode[j] = s;
        }
        ssr->nsig = nsig[k];
    }
    output_cssr_cb(rtcm, cssr, fp);
    check_cssr_changed_facility(cssr->l6facility);
    set_cssr_bank_cbias(rtcm->time, &rtcm->nav, 0, 0);
    rtcm->nbit = i;
    return sync ? 0 : 10;
}

static void output_cssr_pb(rtcm_t* rtcm, cssr_t* cssr, FILE* fp) {
    int j, k, s, tow, sat[CSSR_MAX_SV], nsat, gnss, prn, week;
    int lcnt;
    int first = TRUE;
    ssr_t* ssr = NULL;
    int nsig[CSSR_MAX_SV], sig[CSSR_MAX_SV * CSSR_MAX_SIG], sig_p[CSSR_MAX_SV * CSSR_MAX_SIG];

    if (fp == NULL) return;
    tow = time2gpst(rtcm->time, &week);
    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig_p(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig_p);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig);

    for (k = lcnt = 0; k < nsat; k++) {
        ssr = &rtcm->nav.ssr[sat[k] - 1];
        gnss = sys2gnss(satsys(sat[k], &prn), NULL);

        for (j = 0; j < nsig[k]; j++) {
            s = sig[k * CSSR_MAX_SIG + j];
            if (first == FALSE) {
                fprintf(fp, ",,,,,, %d, %d, %d, ", gnss, prn, sig_p[k * CSSR_MAX_SIG + j]);
            } else {
                fprintf(fp, ", %d, %d, %d, ", gnss, prn, sig_p[k * CSSR_MAX_SIG + j]);
                first = FALSE;
            }
            if (ssr->pbias[s - 1] != INVALID_VALUE) {
                fprintf(fp, "%f, ", (double)ssr->pbias[s - 1]);
                trace(4, "ssr pbias: prn=%2d, tow=%d, udi=%.1f, iod=%2d, pbias=%f\n", sat[k], tow, ssr->udi[5],
                      cssr->iod, ssr->pbias[s - 1]);
            } else {
                fprintf(fp, "#N/A, ");
                trace(3, "invalid pb value: tow=%d, sat=%d, value=%d\n", tow, sat[k], ssr->pbias[s - 1]);
            }
            fprintf(fp, "%d\n", ssr->discontinuity[s - 1]);
            ++lcnt;
        }
    }
    if (lcnt == 0) {
        fprintf(fp, "\n");
    }
}

/* decode phase bias message */
static int decode_cssr_pb(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp) {
    int i, j, k, iod, s, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat;
    int lcnt;
    int nsig[CSSR_MAX_SV], sig[CSSR_MAX_SV * CSSR_MAX_SIG], sig_p[CSSR_MAX_SV * CSSR_MAX_SIG];
    double udint;
    ssr_t* ssr = NULL;

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp);
    check_week_ref(rtcm, tow, ref_pbias);
    rtcm->time = gpst2time(rtcm->week_ref[ref_pbias], tow);
    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig_p(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig_p);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig);

    trace(2, "decode_cssr_pb:   facility=%d tow=%d iod=%d\n", l6facility[chidx] + 1, tow, iod);
    if (cssr->l6facility != l6facility[chidx]) {
        trace(2, "cssr: facility mismatch: tow=%d mask_facility=%d subtype=%d facility=%d\n", tow, cssr->l6facility,
              rtcm->subtype, l6facility[chidx]);
        return -1;
    }
    if (cssr->iod != iod) {
        trace(2, "cssr: iod mismatch: tow=%d mask_iod=%d subtype=%d iod=%d\n", tow, cssr->iod, rtcm->subtype, iod);
        return -1;
    }

    for (k = 0; k < MAXSAT; ++k) {
        ssr = &rtcm->nav.ssr[k];
        ssr->t0[5].sec = 0.0;
        ssr->t0[5].time = 0;
        ssr->udi[5] = 0;
        ssr->iod[5] = 0;
        ssr->update_pb = 0;
        ssr->update = 0;
        ssr->nsig = 0;

        for (j = 0; j < MAXCODE; ++j) {
            /* phase bias */
            ssr->pbias[j] = 0.0;
            ssr->smode[j] = 0;
        }
    }

    for (k = lcnt = 0; k < nsat; k++) {
        ssr = &rtcm->nav.ssr[sat[k] - 1];
        ssr->t0[5] = rtcm->time;
        ssr->udi[5] = udint;
        ssr->iod[5] = cssr->iod;
        ssr->update_pb = 1;
        ssr->update = 1;

        for (j = 0; j < nsig[k]; j++) {
            s = sig[k * CSSR_MAX_SIG + j];
            ssr->pbias[s - 1] = decode_sval(rtcm->buff, i, 15, 0.001);
            i += 15;
            ssr->discontinuity[s - 1] = getbitu(rtcm->buff, i, 2);
            i += 2;
            ssr->smode[j] = s;
            ++lcnt;
        }
        ssr->nsig = nsig[k];
    }
    output_cssr_pb(rtcm, cssr, fp);
    check_cssr_changed_facility(cssr->l6facility);
    set_cssr_bank_pbias(rtcm->time, &rtcm->nav, 0, 0);

    rtcm->nbit = i;
    return sync ? 0 : 10;
}

/* check if the buffer length is sufficient to decode the code bias message */
static int check_bit_width_cb(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int nsig[CSSR_MAX_SV], nsig_total = 0;
    int k, sat[CSSR_MAX_SV], nsat;

    if (rtcm->subtype != CSSR_TYPE_CB && rtcm->subtype != CSSR_TYPE_BIAS) return FALSE;

    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, NULL);

    for (k = 0; k < nsat; k++) {
        nsig_total += nsig[k];
    }
    return i0 + 21 + nsig_total * 11 <= rtcm->havebit;
}

static int check_bit_width_pb(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int nsig[CSSR_MAX_SV], nsig_total = 0;
    int k, sat[CSSR_MAX_SV], nsat;

    if (rtcm->subtype != CSSR_TYPE_PB && rtcm->subtype != CSSR_TYPE_BIAS) return FALSE;

    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, NULL);

    for (k = 0; k < nsat; k++) {
        nsig_total += nsig[k];
    }
    return i0 + 21 + nsig_total * 17 <= rtcm->havebit;
}

static void output_cssr_bias(rtcm_t* rtcm, cssr_t* cssr, int cbflag, int pbflag, int netflag, int network, int netmask,
                             FILE* fp) {
    int j, k, s, sat[CSSR_MAX_SV], nsat;
    int nsig[CSSR_MAX_SV], sig[CSSR_MAX_SV * CSSR_MAX_SIG], sig_p[CSSR_MAX_SV * CSSR_MAX_SIG];
    int lcnt, gnss, prn;
    ssr_t* ssr = NULL;

    if (fp == NULL) return;

    fprintf(fp, ", %d", cbflag);
    fprintf(fp, ", %d", pbflag);
    fprintf(fp, ", %d", netflag);
    if (netflag == 1) {
        fprintf(fp, ", %d", network);
    }

    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig_p(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig_p);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig);
    if (netflag == 1) {
        fprintf(fp, ", 0x%08x", netmask);
    }

    for (k = lcnt = 0; k < nsat; k++) {
        ssr = &rtcm->nav.ssr[sat[k] - 1];
        if (!((netmask >> (nsat - 1 - k)) & 1)) {
            continue;
        }
        gnss = sys2gnss(satsys(sat[k], &prn), NULL);
        for (j = 0; j < nsig[k]; j++) {
            if (lcnt != 0) {
                if (netflag == 1) {
                    fprintf(fp, ",,,,,,,,,,, %d, %d, %d, ", gnss, prn, sig_p[k * CSSR_MAX_SIG + j]);
                } else {
                    fprintf(fp, ",,,,,,,,, %d, %d, %d, ", gnss, prn, sig_p[k * CSSR_MAX_SIG + j]);
                }
            } else {
                fprintf(fp, ", %d, %d, %d, ", gnss, prn, sig_p[k * CSSR_MAX_SIG + j]);
            }
            s = sig[k * CSSR_MAX_SIG + j];
            if (cbflag == 1) {
                /* code bias */
                if (ssr->cbias[s - 1] != INVALID_VALUE) {
                    fprintf(fp, "%f, ", (double)ssr->cbias[s - 1]);
                }
            } else {
                fprintf(fp, ", ");
            }
            if (pbflag == 1) {
                /* phase bias */
                if (ssr->pbias[s - 1] != INVALID_VALUE) {
                    fprintf(fp, "%f, ", (double)ssr->pbias[s - 1]);
                } else {
                    fprintf(fp, "#N/A, ");
                }
                fprintf(fp, "%d", ssr->discontinuity[s - 1]);
            } else {
                fprintf(fp, ", ");
            }
            fprintf(fp, "\n");
            ++lcnt;
        }
    }
    if (lcnt == 0) {
        fprintf(fp, "\n");
    }
}

/* code bias correction */
static int decode_cssr_bias(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp) {
    int i, j, k, iod, s, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat;
    int nsig[CSSR_MAX_SV], sig[CSSR_MAX_SV * CSSR_MAX_SIG], sig_p[CSSR_MAX_SV * CSSR_MAX_SIG];
    int cbflag, pbflag, netflag, network, netmask, lcnt;
    double udint;
    ssr_t* ssr = NULL;

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp);
    check_week_ref(rtcm, tow, ref_bias);
    rtcm->time = gpst2time(rtcm->week_ref[ref_bias], tow);

    cbflag = getbitu(rtcm->buff, i, 1);
    i += 1;
    pbflag = getbitu(rtcm->buff, i, 1);
    i += 1;
    netflag = getbitu(rtcm->buff, i, 1);
    i += 1;
    network = getbitu(rtcm->buff, i, (netflag ? 5 : 0));
    i += (netflag ? 5 : 0);

    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig_p(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig_p);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig);
    netmask = getbitu(rtcm->buff, i, (netflag ? nsat : 0));
    i += (netflag ? nsat : 0);

    trace(2, "decode_cssr_bias: facility=%d tow=%d iod=%d net=%2d mask=0x%x flag=%d %d %d\n", l6facility[chidx] + 1,
          tow, iod, network, netmask, cbflag, pbflag, netflag);
    if (cssr->l6facility != l6facility[chidx]) {
        trace(2, "cssr: facility mismatch: tow=%d mask_facility=%d subtype=%d facility=%d\n", tow, cssr->l6facility,
              rtcm->subtype, l6facility[chidx]);
        return -1;
    }
    if (cssr->iod != iod) {
        trace(2, "cssr: iod mismatch: tow=%d mask_iod=%d subtype=%d iod=%d\n", tow, cssr->iod, rtcm->subtype, iod);
        return -1;
    }

    for (k = 0; k < MAXSAT; ++k) {
        ssr = &rtcm->nav.ssr[k];

        if (rtcm->subtype == CSSR_TYPE_BIAS && cbflag == 1) {
            ssr->t0[4].sec = 0.0;
            ssr->t0[4].time = 0;
            ssr->udi[4] = 0;
            ssr->iod[4] = 0;
            ssr->update_cb = 0;
            ssr->update = 0;
            ssr->nsig = 0;
        }
        if (rtcm->subtype == CSSR_TYPE_BIAS && pbflag == 1) {
            ssr->t0[5].sec = 0.0;
            ssr->t0[5].time = 0;
            ssr->udi[5] = 0;
            ssr->iod[5] = 0;
            ssr->update_pb = 0;
            ssr->update = 0;
            ssr->nsig = 0;
        }

        for (j = 0; j < MAXCODE; ++j) {
            if (rtcm->subtype == CSSR_TYPE_BIAS && cbflag == 1) {
                /* code bias */
                ssr->smode[j] = 0;
                ssr->cbias[j] = 0.0;
            }
            if (rtcm->subtype == CSSR_TYPE_BIAS && pbflag == 1) {
                /* phase bias */
                ssr->smode[j] = 0;
                ssr->pbias[j] = 0.0;
                ssr->discontinuity[j] = 0;
            }
        }
    }

    for (k = lcnt = 0; k < nsat; k++) {
        if (netflag && !((netmask >> (nsat - 1 - k)) & 1)) {
            continue;
        }
        ssr = &rtcm->nav.ssr[sat[k] - 1];

        if (rtcm->subtype == CSSR_TYPE_BIAS && cbflag == 1) {
            ssr->t0[4] = rtcm->time;
            ssr->udi[4] = udint;
            ssr->iod[4] = cssr->iod;
            ssr->update_cb = 1;
            ssr->update = 1;
        }
        if (rtcm->subtype == CSSR_TYPE_BIAS && pbflag == 1) {
            ssr->t0[5] = rtcm->time;
            ssr->udi[5] = udint;
            ssr->iod[5] = cssr->iod;
            ssr->update_pb = 1;
            ssr->update = 1;
        }

        for (j = 0; j < nsig[k]; j++) {
            s = sig[k * CSSR_MAX_SIG + j];
            if (cbflag == 1) { /* code bias */
                ssr->cbias[s - 1] = decode_sval(rtcm->buff, i, 11, 0.02);
                i += 11;
                if (ssr->cbias[s - 1] == INVALID_VALUE) {
                    trace(3, "invalid cb value: tow=%d, sat=%d, value=%d\n", tow, sat[k], ssr->cbias[s - 1]);
                }
                trace(4, "ssr cbias: network=%d, prn=%2d, tow=%d, udi=%.1f, iod=%2d, s=%d, cbias=%f\n", network, sat[k],
                      tow, udint, cssr->iod, s, ssr->cbias[s - 1]);
            }
            if (pbflag == 1) { /* phase bias */
                /* phase bias */
                ssr->pbias[s - 1] = decode_sval(rtcm->buff, i, 15, 0.001);
                i += 15;
                if (ssr->pbias[s - 1] == INVALID_VALUE) {
                    trace(3, "invalid pb value: tow=%d, sat=%d, value=%d\n", tow, sat[k], ssr->pbias[s - 1]);
                }
                ssr->discontinuity[s - 1] = getbitu(rtcm->buff, i, 2);
                i += 2;
                trace(4, "ssr pbias: network=%d, prn=%2d, tow=%d, udi=%.1f, iod=%2d, s=%d, pbias=%f\n", network, sat[k],
                      tow, udint, cssr->iod, s, ssr->pbias[s - 1]);
            }
            ++lcnt;
            ssr->smode[j] = s;
        }
        ssr->nsig = nsig[k];
    }
    output_cssr_bias(rtcm, cssr, cbflag, pbflag, netflag, network, netmask, fp);
    if (cbflag == 1) {
        check_cssr_changed_facility(cssr->l6facility);
        set_cssr_bank_cbias(rtcm->time, &rtcm->nav, (netflag ? network : 0), cssr->iod);
    }
    if (pbflag == 1) {
        check_cssr_changed_facility(cssr->l6facility);
        set_cssr_bank_pbias(rtcm->time, &rtcm->nav, (netflag ? network : 0), cssr->iod);
    }
    rtcm->nbit = i;
    return sync ? 0 : 10;
}

/* check if the buffer length is sufficient to decode the bias message */
static int check_bit_width_bias(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int j, k, nsat, slen = 0, cbflag, pbflag, netflag, netmask = 0;
    int sat[CSSR_MAX_SV], nsig[CSSR_MAX_SV], sig[CSSR_MAX_SV * CSSR_MAX_SIG];

    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig);
    if (i0 + 24 > rtcm->havebit) return FALSE;

    i0 += 21;
    cbflag = getbitu(rtcm->buff, i0, 1);
    i0 += 1;
    pbflag = getbitu(rtcm->buff, i0, 1);
    i0 += 1;
    netflag = getbitu(rtcm->buff, i0, 1);
    i0 += 1;

    if (netflag) {
        if (i0 + 5 + nsat > rtcm->havebit) return FALSE;
        i0 += 5;
        netmask = getbitu(rtcm->buff, i0, nsat);
        i0 += nsat;
    }

    if (cbflag) slen += 11;
    if (pbflag) slen += 17;

    for (k = 0; k < nsat; k++) {
        if (netflag && !((netmask >> (nsat - 1 - k)) & 1)) continue;
        for (j = 0; j < nsig[k]; j++) {
            if (i0 + slen > rtcm->havebit) return FALSE;
            i0 += slen;
        }
    }
    return TRUE;
}

static void output_cssr_ura(rtcm_t* rtcm, cssr_t* cssr, FILE* fp) {
    int j, sat[CSSR_MAX_SV], nsat, gnss, prn;
    ssr_t* ssr = NULL;

    if (fp == NULL) return;
    nsat = svmask2sat(cssr->svmask, sat);
    for (j = 0; j < nsat; j++) {
        ssr = &rtcm->nav.ssr[sat[j] - 1];
        gnss = sys2gnss(satsys(sat[j], &prn), NULL);
        if (j != 0) {
            fprintf(fp, ",,,,,, %d, %d, 0x%02x\n", gnss, prn, ssr->ura);
        } else {
            fprintf(fp, ", %d, %d, 0x%02x\n", gnss, prn, ssr->ura);
        }
    }
    if (nsat == 0) {
        fprintf(fp, "\n");
    }
}

/* decode ura correction */
static int decode_cssr_ura(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp) {
    int i, j, iod, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat;
    double udint;
    ssr_t* ssr = NULL;

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp);
    check_week_ref(rtcm, tow, ref_ura);
    rtcm->time = gpst2time(rtcm->week_ref[ref_ura], tow);
    nsat = svmask2sat(cssr->svmask, sat);

    trace(3, "decode_cssr_ura:  facility=%d tow=%d iod=%d\n", l6facility[chidx] + 1, tow, iod);
    if (cssr->l6facility != l6facility[chidx]) {
        trace(2, "cssr: facility mismatch: tow=%d mask_facility=%d subtype=%d facility=%d\n", tow, cssr->l6facility,
              rtcm->subtype, l6facility[chidx]);
        return -1;
    }
    if (cssr->iod != iod) {
        trace(2, "cssr: iod mismatch: tow=%d mask_iod=%d subtype=%d iod=%d\n", tow, cssr->iod, rtcm->subtype, iod);
        return -1;
    }

    for (j = 0; j < nsat; j++) {
        ssr = &rtcm->nav.ssr[sat[j] - 1];
        ssr->t0[3] = rtcm->time;
        ssr->udi[3] = udint;
        ssr->iod[3] = iod;
        ssr->ura = getbitu(rtcm->buff, i, 6);
        i += 6; /* ssr ura */
        ssr->update_ura = 1;
        ssr->update = 1;
    }
    output_cssr_ura(rtcm, cssr, fp);

    rtcm->nbit = i;
    return sync ? 0 : 10;
}

/* check if the buffer length is sufficient to decode the ura message */
static int check_bit_width_ura(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int nsat;

    nsat = svmask2sat(cssr->svmask, NULL);
    return i0 + 21 + 6 * nsat <= rtcm->havebit;
}

static void output_cssr_stec(rtcm_t* rtcm, cssr_t* cssr, int* qual_stec, int inet, FILE* fp) {
    int j, s, sat[CSSR_MAX_SV], nsat, gnss, prn;
    ssrion_t* ssr_ion;

    if (fp == NULL) return;
    ssr_ion = &rtcm->ssr_ion[inet];

    nsat = svmask2sat(cssr->svmask, sat);

    fprintf(fp, ", %d, ", cssr->opt.stec_type);
    fprintf(fp, "%d, ", inet);

    fprintf(fp, "0x%02x", (uint32_t)(cssr->net_svmask[inet] >> 32));
    fprintf(fp, "%08lx", cssr->net_svmask[inet] & 0xffffffff);

    for (j = 0, s = 0; j < nsat; j++) {
        if ((cssr->net_svmask[inet] >> (nsat - 1 - j)) & 1) {
            gnss = sys2gnss(satsys(sat[j], &prn), NULL);
            if (s != 0) {
                fprintf(fp, ",,,,,,,,, %d, %d, ", gnss, prn);
            } else {
                fprintf(fp, ", %d, %d, ", gnss, prn);
            }
            fprintf(fp, "0x%02x, ", qual_stec[j]);

            if (ssr_ion->stec.a[s][0] != INVALID_VALUE) {
                fprintf(fp, "%f", (double)ssr_ion->stec.a[s][0]);
            } else {
                fprintf(fp, "#N/A");
            }
            if (cssr->opt.stec_type > 0) {
                if (ssr_ion->stec.a[s][1] != INVALID_VALUE) {
                    fprintf(fp, ", %f, ", (double)ssr_ion->stec.a[s][1]);
                } else {
                    fprintf(fp, ", #N/A, ");
                }

                if (ssr_ion->stec.a[s][2] != INVALID_VALUE) {
                    fprintf(fp, "%f", (double)ssr_ion->stec.a[s][2]);
                } else {
                    fprintf(fp, "#N/A");
                }
            }
            if (cssr->opt.stec_type > 1) {
                if (ssr_ion->stec.a[s][3] != INVALID_VALUE) {
                    fprintf(fp, ", %f", (double)ssr_ion->stec.a[s][3]);
                } else {
                    fprintf(fp, ", #N/A");
                }
            }
            fprintf(fp, "\n");
            s++;
        }
    }
    if (s == 0) {
        fprintf(fp, "\n");
    }
}

/* decode stec correction */
static int decode_cssr_stec(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp) {
    int i, j, k, iod, s, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat, inet, a, b, qual_stec[CSSR_MAX_SV];
    double udint;
    ssrgp_t* ssrg;
    ssrion_t* ssr_ion;

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp);
    check_week_ref(rtcm, tow, ref_stec);
    rtcm->time = gpst2time(rtcm->week_ref[ref_stec], tow);
    nsat = svmask2sat(cssr->svmask, sat);
    cssr->opt.stec_type = getbitu(rtcm->buff, i, 2);
    i += 2; /* stec correction type */
    inet = getbitu(rtcm->buff, i, 5);
    i += 5; /* network id */

    trace(2, "decode_cssr_stec: facility=%d tow=%d iod=%d net=%2d type=%d\n", l6facility[chidx] + 1, tow, iod, inet,
          cssr->opt.stec_type);
    if (cssr->l6facility != l6facility[chidx]) {
        trace(2, "cssr: facility mismatch: tow=%d mask_facility=%d subtype=%d facility=%d\n", tow, cssr->l6facility,
              rtcm->subtype, l6facility[chidx]);
        return -1;
    }
    if (cssr->iod != iod) {
        trace(2, "cssr: iod mismatch: tow=%d mask_iod=%d subtype=%d iod=%d\n", tow, cssr->iod, rtcm->subtype, iod);
        return -1;
    }

    ssrg = &rtcm->ssrg[inet];
    ssr_ion = &rtcm->ssr_ion[inet];

    cssr->net_svmask[inet] = getbitu(rtcm->buff, i, nsat);
    i += nsat; /* stec correction type */
    trace(4, "decode_cssr_stec: mask=0x%x\n", cssr->net_svmask[inet]);

    ssrg->t0 = rtcm->time;
    ssrg->udi = udint;
    ssrg->iod = iod;

    for (j = 0, s = 0; j < nsat; j++) {
        if ((cssr->net_svmask[inet] >> (nsat - 1 - j)) & 1) {
            ssr_ion->stec.sat[s] = sat[j];
            a = getbitu(rtcm->buff, i, 3);
            i += 3;
            b = getbitu(rtcm->buff, i, 3);
            i += 3;
            ssr_ion->stec.quality[s] = decode_cssr_quality_stec(a, b);
            qual_stec[j] = (a << 3) | b;
            for (k = 0; k < 4; k++) ssr_ion->stec.a[s][k] = 0.0;

            ssr_ion->stec.a[s][0] = decode_sval(rtcm->buff, i, 14, 0.05);
            i += 14;
            if (cssr->opt.stec_type > 0) {
                ssr_ion->stec.a[s][1] = decode_sval(rtcm->buff, i, 12, 0.02);
                i += 12;
                ssr_ion->stec.a[s][2] = decode_sval(rtcm->buff, i, 12, 0.02);
                i += 12;
            }
            if (cssr->opt.stec_type > 1) {
                ssr_ion->stec.a[s][3] = decode_sval(rtcm->buff, i, 10, 0.02);
                i += 10;
            }
            trace(4, "decode_cssr_stec: tow=%d, sat=%d\n", tow, sat[j]);
            s++;
        }
    }
    output_cssr_stec(rtcm, cssr, qual_stec, inet, fp);
    ssr_ion->stec.network = inet;
    ssr_ion->stec.nsat = s;
    ssrg->update = 1;

    rtcm->nbit = i;
    trace(3, "decode_cssr_stec(): tow=%d, net=%d, bits=%d\n", tow, inet, i - i0);
    return sync ? 0 : 10;
}

/* check if the buffer length is sufficient to decode the stec message */
static int check_bit_width_stec(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int j, sat[CSSR_MAX_SV], nsat, stec_type, slen = 0, nsat_local = 0;
    uint64_t net_svmask;
    const int slen_t[4] = {20, 44, 54, 70};

    nsat = svmask2sat(cssr->svmask, sat);
    if (i0 + 28 + nsat > rtcm->havebit) return FALSE;

    i0 += 21;
    stec_type = getbitu(rtcm->buff, i0, 2);
    i0 += 2;
    i0 += 5;
    net_svmask = getbitu(rtcm->buff, i0, nsat);
    i0 += nsat;

    slen = slen_t[stec_type];

    for (j = 0; j < nsat; j++) { /* number of local satellites */
        if ((net_svmask >> (nsat - 1 - j)) & 1) {
            nsat_local++;
        }
    }
    return i0 + nsat_local * slen <= rtcm->havebit;
}

static void output_cssr_grid(rtcm_t* rtcm, cssr_t* cssr, int trop_type, int sz_idx, int trop_qual, int inet,
                             double dstec_[][MAXSAT], FILE* fp) {
    int j, k, ii, s, sat[CSSR_MAX_SV], nsat;
    int gnss, prn;
    ssrgp_t* ssrg;

    if (fp == NULL) return;
    nsat = svmask2sat(cssr->svmask, sat);

    fprintf(fp, ", %d, ", trop_type);
    fprintf(fp, "%d, ", sz_idx);
    fprintf(fp, "%d, ", inet);

    ssrg = &rtcm->ssrg[inet];

    fprintf(fp, "0x%02x", (uint32_t)(cssr->net_svmask[inet] >> 32));
    fprintf(fp, "%08lx, ", cssr->net_svmask[inet] & 0xffffffff);
    fprintf(fp, "0x%02x, ", trop_qual);
    fprintf(fp, "%d", ssrg->ngp);

    for (j = 0; j < ssrg->ngp; j++) {
        if (j != 0) {
            fprintf(fp, ",,,,,,,,,,,, %d, ", j + 1);
        } else {
            fprintf(fp, ", %d, ", j + 1);
        }
        switch (trop_type) {
            case 0:
                break;
            case 1:
                if (ssrg->trop_wet[j] != INVALID_VALUE || ssrg->trop_total[j] != INVALID_VALUE) {
                    fprintf(fp, "%f, ", (double)ssrg->trop_total[j] - (double)ssrg->trop_wet[j]);
                    fprintf(fp, "%f", (double)ssrg->trop_wet[j]);
                } else {
                    fprintf(fp, "#N/A, ");
                    fprintf(fp, "#N/A");
                }
                break;
        }

        for (k = 0, s = 0, ii = 0; k < nsat; k++) {
            if ((cssr->net_svmask[inet] >> (nsat - 1 - k)) & 1) {
                gnss = sys2gnss(satsys(sat[k], &prn), NULL);
                if (ii != 0) {
                    fprintf(fp, ",,,,,,,,,,,,,,, %d, %d, ", gnss, prn);
                } else {
                    fprintf(fp, ", %d, %d, ", gnss, prn);
                }
                if (ssrg->stec[j][s] == INVALID_VALUE) {
                    fprintf(fp, "#N/A\n");
                } else {
                    fprintf(fp, "%f\n", (double)dstec_[j][s]);
                }
                ++ii;
                ++s;
            }
        }
        if (ii == 0) {
            fprintf(fp, "\n");
        }
    }
    if (ssrg->ngp == 0) {
        fprintf(fp, "\n");
    }
}

/* decode grid correction */
static int decode_cssr_grid(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp) {
    int i, j, k, ii, s, sync, iod, tow, ngnss, sat[CSSR_MAX_SV], trop_qual, nsat, sz;
    int trop_type, sz_idx, inet, a, b, hs, wet;
    double udint, stec0, dlat, dlon, dstec, dstec_[RTCM_SSR_MAX_GP][MAXSAT];
    nav_t* nav = &rtcm->nav;
    ssrgp_t* ssrg;
    ssrion_t* ssr_ion;
    int valid;

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp);
    check_week_ref(rtcm, tow, ref_grid);
    rtcm->time = gpst2time(rtcm->week_ref[ref_grid], tow);
    nsat = svmask2sat(cssr->svmask, sat);

    trop_type = getbitu(rtcm->buff, i, 2);
    i += 2; /* troposphere correction type */
    sz_idx = getbitu(rtcm->buff, i, 1);
    i++; /* stec range */
    inet = getbitu(rtcm->buff, i, 5);
    i += 5; /* network id */

    ssrg = &rtcm->ssrg[inet];
    ssr_ion = &rtcm->ssr_ion[inet];

    cssr->net_svmask[inet] = getbitu(rtcm->buff, i, nsat);
    i += nsat; /* stec correction type */
    a = getbitu(rtcm->buff, i, 3);
    i += 3;
    b = getbitu(rtcm->buff, i, 3);
    i += 3;
    trop_qual = (a << 3) | b;
    ssrg->ngp = getbitu(rtcm->buff, i, 6);
    i += 6;
    ssrg->t0 = rtcm->time;
    ssrg->quality = decode_cssr_quality_trop(a, b);
    ssrg->network = inet;

    trace(2, "decode_cssr_grid: facility=%d tow=%d iod=%d net=%2d trop=%d sz_idx=%d ngp=%d\n", l6facility[chidx] + 1,
          tow, iod, inet, trop_type, sz_idx, ssrg->ngp);
    if (cssr->l6facility != l6facility[chidx]) {
        trace(2, "cssr: facility mismatch: tow=%d mask_facility=%d subtype=%d facility=%d\n", tow, cssr->l6facility,
              rtcm->subtype, l6facility[chidx]);
        return -1;
    }
    if (cssr->iod != iod) {
        trace(2, "cssr: iod mismatch: tow=%d mask_iod=%d subtype=%d iod=%d\n", tow, cssr->iod, rtcm->subtype, iod);
        return -1;
    }

    for (j = 0; j < RTCM_SSR_MAX_GP; j++) {
        ssrg->gp[j].pos[0] = 0.0;
        ssrg->gp[j].pos[1] = 0.0;
        ssrg->gp[j].pos[2] = 0.0;
        ssrg->gp[j].network = 0;
        ssrg->gp[j].update = 0;
        ssrg->nsv[j] = 0;
    }

    for (j = 0; j < ssrg->ngp; j++) {
        ssrg->gp[j].pos[0] = clas_grid[inet][j][0] * D2R;
        ssrg->gp[j].pos[1] = clas_grid[inet][j][1] * D2R;
        ssrg->gp[j].pos[2] = clas_grid[inet][j][2];
        ssrg->gp[j].network = inet;
        ssrg->gp[j].update = 1;

        trace(4, "gp check:pos=%f,%f,%f,%d,%d\n", ssrg->gp[j].pos[0] * R2D, ssrg->gp[j].pos[1] * R2D,
              ssrg->gp[j].pos[2], inet, ssrg->ngp);
    }
    sz = (sz_idx) ? 16 : 7;

    for (j = 0; j < ssrg->ngp; j++) {
        valid = 1;
        switch (trop_type) {
            case 0:
                break;
            case 1:
                hs = getbits(rtcm->buff, i, 9);
                i += 9;
                wet = getbits(rtcm->buff, i, 8);
                i += 8;
                if (hs == (-P2_S9_MAX - 1)) {
                    trace(2, "trop(hs) is invalid: tow=%d, inet=%d, grid=%d, hs=%d\n", tow, inet, j, hs);
                    valid = 0;
                }
                if (wet == (-P2_S8_MAX - 1)) {
                    trace(2, "trop(wet) is invalid: tow=%d, inet=%d, grid=%d, wet=%d\n", tow, inet, j, wet);
                    valid = 0;
                }
                if (valid == 1) {
                    ssrg->trop_wet[j] = wet * 0.004 + 0.252;
                    ssrg->trop_total[j] = (hs + wet) * 0.004 + 0.252 + CSSR_TROP_HS_REF;
                } else {
                    ssrg->trop_wet[j] = INVALID_VALUE;
                    ssrg->trop_total[j] = INVALID_VALUE;
                }
                trace(4, "decode_cssr_grid: grid=%d, total=%.3f, wet=%.3f\n", j, ssrg->trop_total[j],
                      ssrg->trop_wet[j]);
                break;
        }

        dlat = (ssrg->gp[j].pos[0] - ssrg->gp[0].pos[0]) * R2D;
        dlon = (ssrg->gp[j].pos[1] - ssrg->gp[0].pos[1]) * R2D;

        for (k = 0, s = 0, ii = 0; k < nsat; k++) {
            if ((cssr->net_svmask[inet] >> (nsat - 1 - k)) & 1) {
                dstec_[j][s] = dstec = decode_sval(rtcm->buff, i, sz, 0.04);
                i += sz;
                stec0 = ssr_ion->stec.a[ii][0] + ssr_ion->stec.a[ii][1] * dlat + ssr_ion->stec.a[ii][2] * dlon +
                        ssr_ion->stec.a[ii][3] * dlat * dlon;
                if (dstec == INVALID_VALUE) {
                    trace(2, "dstec is invalid: tow=%d, inet=%d, grid=%d, sat=%d, dstec=%f\n", tow, inet, j, sat[k],
                          dstec);
                    ssrg->stec[j][s] = INVALID_VALUE;
                } else {
                    ssrg->stec[j][s] = dstec;
                    ssrg->stec[j][s] += stec0;
                }
                ssrg->stec0[j][s] = stec0;
                ssrg->sat[j][s] = sat[k];
                ++ii;
                ++s;
            }
        }
        ssrg->nsv[j] = s;
    }
    output_cssr_grid(rtcm, cssr, trop_type, sz_idx, trop_qual, inet, dstec_, fp);
    ssrg->update = 1;
    nav->updateac = 1;

    check_cssr_changed_facility(cssr->l6facility);
    set_cssr_latest_trop(ssrg->t0, ssrg, inet);
    set_cssr_bank_trop(ssrg->t0, ssrg, inet);

    rtcm->nbit = i;
    trace(3, "decode_cssr_grid(): tow=%d, net=%d, bits=%d\n", tow, inet, i - i0);
    return sync ? 0 : 10;
}

/* check if the buffer length is sufficient to decode the grid message */
static int check_bit_width_grid(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int k, nsat, trop_type, ngp, sz_trop, sz_idx, sz_stec, nsat_local = 0;
    uint64_t net_svmask;

    nsat = svmask2sat(cssr->svmask, NULL);
    if (i0 + 41 + nsat > rtcm->havebit) return FALSE;
    i0 += 21;
    trop_type = getbitu(rtcm->buff, i0, 2);
    i0 += 2;
    sz_idx = getbitu(rtcm->buff, i0, 1);
    i0++;
    i0 += 5; /* network id */
    net_svmask = getbitu(rtcm->buff, i0, nsat);
    i0 += nsat;
    i0 += 6; /* trop quality indicator */
    ngp = getbitu(rtcm->buff, i0, 6);
    i0 += 6;

    sz_trop = (trop_type == 0) ? 0 : 17;
    sz_stec = (sz_idx == 0) ? 7 : 16;

    for (k = 0; k < nsat; k++) {
        if ((net_svmask >> (nsat - 1 - k)) & 1) {
            nsat_local++;
        }
    }

    return i0 + ngp * (sz_trop + nsat_local * sz_stec) <= rtcm->havebit;
}

static void output_cssr_combo(rtcm_t* rtcm, cssr_t* cssr, int flg_orbit, int flg_clock, int flg_net, int netid,
                              int net_svmask, FILE* fp) {
    int j, sat[CSSR_MAX_SV], nsat;
    int s, gnss, prn;
    ssr_t* ssr = NULL;

    if (fp == NULL) return;

    nsat = svmask2sat(cssr->svmask, sat);

    fprintf(fp, ", %d", flg_orbit);
    fprintf(fp, ", %d", flg_clock);
    fprintf(fp, ", %d", flg_net);
    fprintf(fp, ", %d", netid);
    fprintf(fp, ", 0x%04x", net_svmask & 0xffff);

    for (j = s = 0; j < nsat; ++j) {
        if ((net_svmask >> (nsat - 1 - j)) & 1) {
            ssr = &rtcm->nav.ssr[sat[j] - 1];
            gnss = sys2gnss(satsys(sat[j], &prn), NULL);
            if (s != 0) {
                fprintf(fp, ",,,,,,,,,,, %d, %d", gnss, prn);
            } else {
                fprintf(fp, ", %d, %d", gnss, prn);
            }

            if (flg_orbit == 1) {
                fprintf(fp, ", %d", ssr->iode);
                /* delta radial,along-track,cross-track */
                if (ssr->deph[0] != INVALID_VALUE) {
                    fprintf(fp, ", %f", (double)ssr->deph[0]);
                } else {
                    fprintf(fp, ", #N/A");
                }
                if (ssr->deph[1] != INVALID_VALUE) {
                    fprintf(fp, ", %f", (double)ssr->deph[1]);
                } else {
                    fprintf(fp, ", #N/A");
                }
                if (ssr->deph[2] != INVALID_VALUE) {
                    fprintf(fp, ", %f", (double)ssr->deph[2]);
                } else {
                    fprintf(fp, ", #N/A");
                }
            } else {
                fprintf(fp, ",,,,");
            }
            if (flg_clock == 1) {
                if (ssr->dclk[0] == INVALID_VALUE) {
                    fprintf(fp, ", #N/A");
                } else {
                    fprintf(fp, ", %f", (double)ssr->dclk[0]);
                }
            }
            fprintf(fp, "\n");
            ++s;
        }
    }
    if (s == 0) {
        fprintf(fp, "\n");
    }
}

/* decode orbit/clock combination message */
static int decode_cssr_combo(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp) {
    int i, j, k, sync, iod, tow, ngnss, sat[CSSR_MAX_SV], nsat, iode;
    int flg_orbit, flg_clock, flg_net, net_svmask, netid, s;
    double udint;
    ssr_t* ssr = NULL;

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp);
    check_week_ref(rtcm, tow, ref_combined);
    rtcm->time = gpst2time(rtcm->week_ref[ref_combined], tow);
    nsat = svmask2sat(cssr->svmask, sat);

    flg_orbit = getbitu(rtcm->buff, i, 1);
    i += 1;
    flg_clock = getbitu(rtcm->buff, i, 1);
    i += 1;
    flg_net = getbitu(rtcm->buff, i, 1);
    i += 1;
    netid = getbitu(rtcm->buff, i, (flg_net ? 5 : 0));
    i += (flg_net ? 5 : 0);
    net_svmask = getbitu(rtcm->buff, i, (flg_net ? nsat : 0));
    i += (flg_net ? nsat : 0);

    trace(2, "decode_cssr_combo:facility=%d tow=%d iod=%d net=%2d flag=%d %d %d\n", l6facility[chidx] + 1, tow, iod,
          netid, flg_orbit, flg_clock, flg_net);
    if (cssr->l6facility != l6facility[chidx]) {
        trace(2, "cssr: facility mismatch: tow=%d mask_facility=%d subtype=%d facility=%d\n", tow, cssr->l6facility,
              rtcm->subtype, l6facility[chidx]);
        return -1;
    }
    if (cssr->iod != iod) {
        trace(2, "cssr: iod mismatch: tow=%d mask_iod=%d subtype=%d iod=%d\n", tow, cssr->iod, rtcm->subtype, iod);
        return -1;
    }

    for (j = 0; j < MAXSAT; ++j) {
        ssr = &rtcm->nav.ssr[j];
        if (flg_orbit == 1) {
            ssr->t0[0].sec = 0.0;
            ssr->t0[0].time = 0;
            ssr->udi[0] = 0;
            ssr->iod[0] = 0;
            ssr->update_oc = 0;
            ssr->update = 0;
            ssr->iode = 0;
            ssr->deph[0] = 0.0;
            ssr->deph[1] = 0.0;
            ssr->deph[2] = 0.0;
        }
        if (flg_clock == 1) {
            ssr->t0[1].sec = 0.0;
            ssr->t0[1].time = 0;
            ssr->udi[1] = 0;
            ssr->iod[1] = 0;
            ssr->update_cc = 0;
            ssr->update = 0;
            ssr->dclk[0] = 0.0;
        }
    }

    for (j = s = 0; j < nsat; ++j) {
        if (flg_net && !((net_svmask >> (nsat - 1 - j)) & 1)) {
            continue;
        }
        ssr = &rtcm->nav.ssr[sat[j] - 1];

        if (flg_orbit == 1) {
            if (satsys(sat[j], NULL) == SYS_GAL) {
                iode = getbitu(rtcm->buff, i, 10);
                i += 10; /* iode */
            } else {
                iode = getbitu(rtcm->buff, i, 8);
                i += 8; /* iode */
            }

            /* delta radial,along-track,cross-track */
            ssr->deph[0] = decode_sval(rtcm->buff, i, 15, 0.0016);
            i += 15;
            ssr->deph[1] = decode_sval(rtcm->buff, i, 13, 0.0064);
            i += 13;
            ssr->deph[2] = decode_sval(rtcm->buff, i, 13, 0.0064);
            i += 13;
            if (ssr->deph[0] == INVALID_VALUE || ssr->deph[1] == INVALID_VALUE || ssr->deph[2] == INVALID_VALUE) {
                trace(3, "invalid orbit value: tow=%d, sat=%d, value=%d %d %d\n", tow, sat[j], ssr->deph[0],
                      ssr->deph[1], ssr->deph[2]);
            }
            ssr->iode = iode;

            ssr->t0[0] = rtcm->time;
            ssr->udi[0] = udint;
            ssr->iod[0] = cssr->iod;

            for (k = 0; k < 3; k++) ssr->ddeph[k] = 0.0;

            ssr->update_oc = 1;
            ssr->update = 1;

            trace(4, "combined orbit: network=%d, tow=%d, sat=%d, iode=%d, deph=%f, %f, %f\n", netid, tow, sat[j],
                  ssr->iode, ssr->deph[0], ssr->deph[1], ssr->deph[2]);
            if (ssr->deph[0] == INVALID_VALUE || ssr->deph[1] == INVALID_VALUE || ssr->deph[2] == INVALID_VALUE) {
                trace(3, "invalid orbit value: tow=%d, sat=%d, value=%f %f %f\n", tow, sat[j], ssr->deph[0],
                      ssr->deph[1], ssr->deph[2]);
                ssr->deph[0] = INVALID_VALUE;
                ssr->deph[1] = INVALID_VALUE;
                ssr->deph[2] = INVALID_VALUE;
            }
        }
        if (flg_clock == 1) {
            ssr->dclk[0] = decode_sval(rtcm->buff, i, 15, 0.0016);
            i += 15;
            if (ssr->dclk[0] == INVALID_VALUE) {
                trace(3, "invalid clock value: tow=%d, sat=%d, value=%d\n", tow, sat[j], ssr->dclk[0]);
            }
            ssr->dclk[1] = ssr->dclk[2] = 0.0;
            ssr->t0[1] = rtcm->time;
            ssr->udi[1] = udint;
            ssr->iod[1] = cssr->iod;
            ssr->update_cc = 1;
            ssr->update = 1;
            trace(4, "combined clock: network=%d, tow=%d, sat=%d, dclk=%f\n", netid, tow, sat[j], ssr->dclk[0]);
        }
        ++s;
    }
    output_cssr_combo(rtcm, cssr, flg_orbit, flg_clock, flg_net, netid, net_svmask, fp);
    check_cssr_changed_facility(cssr->l6facility);
    if (flg_net == 1) {
        cssrObject[chidx].separation |= (1 << (netid - 1));
    }

    if (flg_orbit == 1) {
        set_cssr_bank_orbit(rtcm->time, &rtcm->nav, (flg_net ? netid : 0));
    }
    if (flg_clock == 1) {
        set_cssr_bank_clock(rtcm->time, &rtcm->nav, (flg_net ? netid : 0));
    }

    rtcm->nbit = i;
    return sync ? 0 : 10;
}

static int check_bit_width_combo(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int sat[CSSR_MAX_SV], nsat, j, flg_orbit, flg_clock, flg_net, sz;
    uint64_t net_svmask = 0;

    nsat = svmask2sat(cssr->svmask, sat);
    if (i0 + 24 > rtcm->havebit) return FALSE;

    i0 += 21;
    flg_orbit = getbitu(rtcm->buff, i0, 1);
    i0 += 1;
    flg_clock = getbitu(rtcm->buff, i0, 1);
    i0 += 1;
    flg_net = getbitu(rtcm->buff, i0, 1);
    i0 += 1;

    if (flg_net) {
        if (i0 + 5 + nsat > rtcm->havebit) return FALSE;
        i0 += 5; /* network id */
        net_svmask = getbitu(rtcm->buff, i0, nsat);
        i0 += nsat;
    }

    for (j = 0; j < nsat; j++) {
        if (flg_net && !((net_svmask >> (nsat - 1 - j)) & 1)) {
            continue;
        }
        if (flg_orbit) {
            sz = (satsys(sat[j], NULL) == SYS_GAL) ? 10 : 8;
            i0 += sz + 41;
            if (i0 > rtcm->havebit) return FALSE;
        }
        if (flg_clock) {
            if ((i0 += 15) > rtcm->havebit) return FALSE;
        }
    }
    return TRUE;
}

static void output_cssr_atmos(rtcm_t* rtcm, cssr_t* cssr, int trop_ctype, int stec_ctype, int inet, int trop_qual,
                              int trop_type, int sz_idx_t, int* sz_idx_s, double trop_ofst, double* ct, double ci_[][6],
                              int* stec_qual, int* stec_type_, double* total, double* wet, double stec[][CSSR_MAX_SV],
                              FILE* fp1, FILE* fp2) {
    int j, k, s, gnss, sat[CSSR_MAX_SV], nsat, prn;
    const int ct_num[3] = {1, 3, 4}, ci_num[4] = {1, 3, 4, 6};
    ssrgp_t* ssrg;

    if (fp1 == NULL || fp2 == NULL) return;
    ssrg = &rtcm->ssrg[inet];
    nsat = svmask2sat(cssr->svmask, sat);

    fprintf(fp1, ", %d", trop_ctype);
    fprintf(fp1, ", %d", stec_ctype);
    fprintf(fp1, ", %d", inet);
    fprintf(fp2, ", %d", inet);
    fprintf(fp1, ", %d", ssrg->ngp);
    if (trop_ctype != 0) {
        fprintf(fp1, ", 0x%02x", trop_qual);
    } else {
        fprintf(fp1, ",");
    }

    if ((trop_ctype & 0x01) == 0x01) {
        fprintf(fp1, ", %d", trop_type);
        for (k = 0; k < 4; k++) {
            if (k < ct_num[trop_type]) {
                fprintf(fp1, ", %.3f", ct[k]);
            } else {
                fprintf(fp1, ", ");
            }
        }
    } else {
        fprintf(fp1, ",,,,,");
    }

    if ((trop_ctype & 0x02) == 0x02) {
        fprintf(fp1, ", %d", sz_idx_t);
        fprintf(fp1, ", %.2f", trop_ofst);
    } else {
        fprintf(fp1, ",,");
    }
    if (stec_ctype != 0) {
        fprintf(fp1, ", 0x%lx", cssr->net_svmask[inet]);
    }

    for (j = s = 0; j < nsat; ++j) {
        if (stec_ctype != 0) {
            if (!((cssr->net_svmask[inet] >> (nsat - 1 - j)) & 1)) {
                continue;
            }
            gnss = sys2gnss(satsys(sat[j], &prn), NULL);
            if (s != 0) {
                fprintf(fp1, "\n,,,,,,,,,,,,,,,,,,, %d, %d", gnss, prn);
            } else {
                fprintf(fp1, ", %d, %d", gnss, prn);
            }
        }
        if ((stec_ctype & 0x01) == 0x01) {
            fprintf(fp1, ", 0x%02x", stec_qual[j]);
            fprintf(fp1, ", %d", stec_type_[j]);

            for (k = 0; k < 6; k++) {
                if (k < ci_num[stec_type_[j]]) {
                    if (k > 3) {
                        fprintf(fp1, ", %.3f", ci_[j][k]);
                    } else {
                        fprintf(fp1, ", %.2f", ci_[j][k]);
                    }
                } else {
                    fprintf(fp1, ", ");
                }
            }
        }
        if (stec_ctype != 0) {
            s++;
        }
        if ((stec_ctype & 0x02) == 0x02) {
            fprintf(fp1, ", %d", sz_idx_s[j]);
        }
    }
    if (stec_ctype == 0) {
        fprintf(fp1, ",,,,,,,,,,,,");
    }

    fprintf(fp2, ", 0x%lx", cssr->net_svmask[inet]);
    fprintf(fp2, ", %d", ssrg->ngp);

    for (j = 0; j < ssrg->ngp; ++j) {
        if (j != 0) {
            fprintf(fp2, "\n,,,,,,,,, %d", j + 1);
        } else {
            fprintf(fp2, ", %d", j + 1);
        }

        if (trop_ctype != 0) {
            if (total[j] != INVALID_VALUE) {
                fprintf(fp2, ", %.3f", total[j] - wet[j]);
            } else {
                fprintf(fp2, ", #N/A");
            }
            if (wet[j] != INVALID_VALUE) {
                fprintf(fp2, ", %.3f", wet[j]);
            } else {
                fprintf(fp2, ", #N/A");
            }
        } else {
            fprintf(fp2, ", ,");
        }

        if (stec_ctype != 0) {
            for (k = s = 0; k < nsat; ++k) {
                if (!((cssr->net_svmask[inet] >> (nsat - 1 - k)) & 1)) {
                    continue;
                }

                gnss = sys2gnss(satsys(sat[k], &prn), NULL);
                if (s != 0) {
                    fprintf(fp2, "\n,,,,,,,,,,,, %d, %d", gnss, prn);
                } else {
                    fprintf(fp2, ", %d, %d", gnss, prn);
                }

                if (stec[j][s] != INVALID_VALUE) {
                    fprintf(fp2, ", %.4f", stec[j][s]);
                } else {
                    fprintf(fp2, ", #N/A");
                }
                ++s;
            }
        } else {
            fprintf(fp2, ",,,");
        }
    }

    fprintf(fp1, "\n");
    fprintf(fp2, "\n");
}

static int decode_cssr_atmos(rtcm_t* rtcm, cssr_t* cssr, int i0, int header, FILE* fp1, FILE* fp2) {
    int i, j, k, s, sync, tow, iod, ngnss, sat[CSSR_MAX_SV], nsat, sz_idx, sz_idx_t = 0, sz_idx_s[CSSR_MAX_SV], sz;
    int trop_avail, stec_avail, trop_type = -1, stec_type = -1, inet, a = 0, b = 0, quality, trop_qual = -1,
                                stec_qual[CSSR_MAX_SV], stec_type_[CSSR_MAX_SV];
    double total[CSSR_MAX_GP], wet[CSSR_MAX_GP], stec[CSSR_MAX_GP][CSSR_MAX_SV];
    double udint, stec0, ct[6] = {0}, ci[6] = {0}, ci_[CSSR_MAX_SV][6] = {0}, trop_ofst = 0.0, trop_residual, dlat,
                         dlon, dstec;
    nav_t* nav = &rtcm->nav;
    ssrgp_t* ssrg;
    const double dstec_lsb_t[4] = {0.04, 0.12, 0.16, 0.24};
    const int dstec_sz_t[4] = {4, 4, 5, 7};

    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp1);
    i = decode_cssr_head(rtcm, cssr, &sync, &tow, &iod, NULL, &udint, &ngnss, i0, header, fp2);
    check_week_ref(rtcm, tow, ref_atmospheric);
    rtcm->time = gpst2time(rtcm->week_ref[ref_atmospheric], tow);
    nsat = svmask2sat(cssr->svmask, sat);

    trop_avail = getbitu(rtcm->buff, i, 2);
    i += 2; /* troposphere correction availability */
    stec_avail = getbitu(rtcm->buff, i, 2);
    i += 2; /* stec correction availability */
    inet = getbitu(rtcm->buff, i, 5);
    i += 5; /* network id */

    trace(2, "decode_cssr_atmos:facility=%d tow=%d iod=%d net=%2d\n", l6facility[chidx] + 1, tow, iod, inet);
    if (cssr->l6facility != l6facility[chidx]) {
        trace(2, "cssr: facility mismatch: tow=%d mask_facility=%d subtype=%d facility=%d\n", tow, cssr->l6facility,
              rtcm->subtype, l6facility[chidx]);
        return -1;
    }
    if (cssr->iod != iod) {
        trace(2, "cssr: iod mismatch: tow=%d mask_iod=%d subtype=%d iod=%d\n", tow, cssr->iod, rtcm->subtype, iod);
        return -1;
    }

    ssrg = &rtcm->ssrg[inet];

    ssrg->ngp = getbitu(rtcm->buff, i, 6);
    i += 6;
    ssrg->t0 = rtcm->time;
    ssrg->network = inet;

    for (j = 0; j < RTCM_SSR_MAX_GP; ++j) {
        ssrg->trop_total[j] = INVALID_VALUE;
        ssrg->trop_wet[j] = INVALID_VALUE;
        ssrg->gp[j].pos[0] = 0.0;
        ssrg->gp[j].pos[1] = 0.0;
        ssrg->gp[j].pos[2] = 0.0;
        ssrg->gp[j].network = 0;
        ssrg->gp[j].update = 0;
        ssrg->nsv[j] = 0;
    }

    if (trop_avail != 0) {
        a = getbitu(rtcm->buff, i, 3);
        i += 3;
        b = getbitu(rtcm->buff, i, 3);
        i += 3;
        ssrg->quality = decode_cssr_quality_trop(a, b);
        trop_qual = (a << 3) | b;
    }

    if ((trop_avail & 0x01) == 0x01) {
        trop_type = getbitu(rtcm->buff, i, 2);
        i += 2;
        for (k = 0; k < 4; k++) ct[k] = 0.0;
        ct[0] = decode_sval(rtcm->buff, i, 9, 0.004);
        i += 9;
        if (trop_type > 0) {
            ct[1] = decode_sval(rtcm->buff, i, 7, 0.002);
            i += 7;
            ct[2] = decode_sval(rtcm->buff, i, 7, 0.002);
            i += 7;
        }
        if (trop_type > 1) {
            ct[3] = decode_sval(rtcm->buff, i, 7, 0.001);
            i += 7;
        }
    }

    for (j = 0; j < ssrg->ngp; ++j) {
        ssrg->gp[j].pos[0] = clas_grid[inet][j][0] * D2R;
        ssrg->gp[j].pos[1] = clas_grid[inet][j][1] * D2R;
        ssrg->gp[j].pos[2] = clas_grid[inet][j][2];

        dlat = (ssrg->gp[j].pos[0] - ssrg->gp[0].pos[0]) * R2D;
        dlon = (ssrg->gp[j].pos[1] - ssrg->gp[0].pos[1]) * R2D;

        ssrg->gp[j].network = inet;
        ssrg->gp[j].update = 1;

        ssrg->trop_total[j] = CSSR_TROP_HS_REF + ct[0];
        if (trop_type > 0) {
            ssrg->trop_total[j] += (ct[1] * dlat) + (ct[2] * dlon);
        }
        if (trop_type > 1) {
            ssrg->trop_total[j] += ct[3] * dlat * dlon;
        }
    }

    if ((trop_avail & 0x02) == 0x02) {
        sz_idx_t = getbitu(rtcm->buff, i, 1);
        i += 1;
        trop_ofst = getbitu(rtcm->buff, i, 4) * 0.02;
        i += 4;
        trace(3,
              "decode_cssr_atmos: network=%d, tow=%d, trop=0x%02x, stec=0x%02x, ngp=%d, trop_type=%d, ct=%.3f %.3f "
              "%.3f %.3f, sz_idx_t=%d, offset=%.3f\n",
              ssrg->network, tow, trop_avail, stec_avail, ssrg->ngp, trop_type, ct[0], ct[1], ct[2], ct[3], sz_idx_t,
              trop_ofst);
        sz = (sz_idx_t == 0) ? 6 : 8;

        for (j = 0; j < ssrg->ngp; ++j) {
            trop_residual = decode_sval(rtcm->buff, i, sz, 0.004);
            i += sz;
            if (trop_residual != INVALID_VALUE) {
                ssrg->trop_wet[j] = trop_residual + trop_ofst;
                ssrg->trop_total[j] += ssrg->trop_wet[j];
                total[j] = ssrg->trop_total[j];
                wet[j] = ssrg->trop_wet[j];
            } else {
                ssrg->trop_total[j] = INVALID_VALUE;
                total[j] = INVALID_VALUE;
                wet[j] = INVALID_VALUE;
                trace(2, "trop(wet) is invalid: tow=%d, inet=%d, grid=%d\n", tow, inet, j);
            }
            trace(3, "decode_cssr_atmos: pos=%.3f %.3f %.3f, total=%.3f, wet=%.3f\n", ssrg->gp[j].pos[0] * R2D,
                  ssrg->gp[j].pos[1] * R2D, ssrg->gp[j].pos[2], ssrg->trop_total[j], ssrg->trop_wet[j]);
        }
    }

    if (stec_avail != 0) {
        cssr->net_svmask[inet] = getbitu(rtcm->buff, i, nsat);
        i += nsat; /* stec correction type */
        trace(4, "decode_cssr_atmos: mask=0x%x\n", cssr->net_svmask[inet]);
    }
    for (j = s = 0; j < nsat; ++j) {
        if (stec_avail != 0) {
            if (!((cssr->net_svmask[inet] >> (nsat - 1 - j)) & 1)) {
                continue;
            }
            a = getbitu(rtcm->buff, i, 3);
            i += 3;
            b = getbitu(rtcm->buff, i, 3);
            i += 3;
            quality = decode_cssr_quality_stec(a, b);
        }
        for (k = 0; k < 6; k++) ci[k] = ci_[j][k] = 0.0;
        if ((stec_avail & 0x01) == 0x01) {
            stec_qual[j] = (a << 3) | b;
            stec_type_[j] = stec_type = getbitu(rtcm->buff, i, 2);
            i += 2;

            for (k = 0; k < 6; k++) ci[k] = 0.0;
            ci_[j][0] = ci[0] = decode_sval(rtcm->buff, i, 14, 0.05);
            i += 14;
            if (stec_type > 0) {
                ci_[j][1] = ci[1] = decode_sval(rtcm->buff, i, 12, 0.02);
                i += 12;
                ci_[j][2] = ci[2] = decode_sval(rtcm->buff, i, 12, 0.02);
                i += 12;
            }
            if (stec_type > 1) {
                ci_[j][3] = ci[3] = decode_sval(rtcm->buff, i, 10, 0.02);
                i += 10;
            }
            if (stec_type > 2) {
                ci_[j][4] = ci[4] = decode_sval(rtcm->buff, i, 8, 0.005);
                i += 8;
                ci_[j][5] = ci[5] = decode_sval(rtcm->buff, i, 8, 0.005);
                i += 8;
            }
        }
        if (stec_avail != 0) {
            if ((stec_avail & 0x02) == 0x02) {
                sz_idx_s[j] = sz_idx = getbitu(rtcm->buff, i, 2);
                i += 2;
                trace(3, "decode_cssr_atmos: stec_type=%d, ct=%.2f %.2f %.2f %.2f %.2f %.2f, sz_idx=%d\n", stec_type,
                      ci[0], ci[1], ci[2], ci[3], ci[4], ci[5], sz_idx);
            }

            for (k = 0; k < ssrg->ngp; ++k) {
                dlat = (ssrg->gp[k].pos[0] - ssrg->gp[0].pos[0]) * R2D;
                dlon = (ssrg->gp[k].pos[1] - ssrg->gp[0].pos[1]) * R2D;

                dstec = 0.0;
                if ((stec_avail & 0x02) == 0x02) {
                    dstec = decode_sval(rtcm->buff, i, dstec_sz_t[sz_idx], dstec_lsb_t[sz_idx]);
                    i += dstec_sz_t[sz_idx];
                    trace(5, "sz_idx=%d, dstec=%f\n", dstec_sz_t[sz_idx], dstec);
                }

                stec0 = ci[0];
                if (stec_type > 0) {
                    stec0 += (ci[1] * dlat) + (ci[2] * dlon);
                }
                if (stec_type > 1) {
                    stec0 += ci[3] * dlat * dlon;
                }
                if (stec_type > 2) {
                    stec0 += (ci[4] * dlat * dlat) + (ci[5] * dlon * dlon);
                }
                if (dstec == INVALID_VALUE) {
                    trace(2, "dstec is invalid: tow=%d, inet=%d, grid=%d, sat=%d, dstec=%d\n", tow, inet, k, sat[j],
                          (int)dstec);
                    ssrg->stec[k][s] = INVALID_VALUE;
                    stec[k][s] = INVALID_VALUE;
                } else {
                    ssrg->stec[k][s] = stec0 + dstec;
                    stec[k][s] = stec0 + dstec;
                }
                ssrg->stec0[k][s] = stec0;
                ssrg->sat[k][s] = sat[j];
                ++ssrg->nsv[k];
                trace(3, "decode_cssr_atmos: sat=%d, grid=%d, stec=%.4f\n", ssrg->sat[k][s], k + 1, ssrg->stec[k][s]);
            }
            s++;
        }
    }

    if (stec_avail != 0) {
        for (k = 0; k < ssrg->ngp; ++k) {
            trace(4, "decode_cssr_atmos: grid=%d, nsv=%d\n", k + 1, ssrg->nsv[k]);
        }
    }

    output_cssr_atmos(rtcm, cssr, trop_avail, stec_avail, inet, trop_qual, trop_type, sz_idx_t, sz_idx_s, trop_ofst, ct,
                      ci_, stec_qual, stec_type_, total, wet, stec, fp1, fp2);
    ssrg->update = 1;
    nav->updateac = 1;

    check_cssr_changed_facility(cssr->l6facility);
    set_cssr_latest_trop(ssrg->t0, ssrg, inet);
    set_cssr_bank_trop(ssrg->t0, ssrg, inet);

    rtcm->nbit = i;
    trace(3, "decode_cssr_atmos(): tow=%d, net=%d, bits=%d\n", tow, inet, i - i0);
    return sync ? 0 : 10;
}

static int check_bit_width_atmos(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int trop_avail, stec_avail, trop_type, stec_type, ngp, sz_idx, sz, j, nsat;
    uint64_t net_svmask;
    const int dstec_sz_t[4] = {4, 4, 5, 7};
    const int trop_sz_t[3] = {9, 23, 30};
    const int stec_sz_t[4] = {14, 38, 48, 64};

    nsat = svmask2sat(cssr->svmask, NULL);
    if (i0 + 36 > rtcm->havebit) return FALSE;
    i0 += 21;

    trop_avail = getbitu(rtcm->buff, i0, 2);
    i0 += 2;
    stec_avail = getbitu(rtcm->buff, i0, 2);
    i0 += 2;
    i0 += 5;
    ngp = getbitu(rtcm->buff, i0, 6);
    i0 += 6;

    if (trop_avail != 0) {
        if (i0 + 6 > rtcm->havebit) return FALSE;
        i0 += 6;
    }
    if ((trop_avail & 0x01) == 0x01) {
        if (i0 + 2 > rtcm->havebit) return FALSE;
        trop_type = getbitu(rtcm->buff, i0, 2);
        i0 += 2;
        sz = trop_sz_t[trop_type];
        if (i0 + sz > rtcm->havebit) return FALSE;
        i0 += sz;
    }
    if ((trop_avail & 0x02) == 0x02) {
        if (i0 + 5 > rtcm->havebit) return FALSE;
        sz_idx = getbitu(rtcm->buff, i0, 1);
        i0 += 1;
        i0 += 4;
        sz = (sz_idx == 0) ? 6 : 8;
        if (i0 + sz * ngp > rtcm->havebit) return FALSE;
        i0 += sz * ngp;
    }

    if (stec_avail != 0) {
        if (i0 + nsat > rtcm->havebit) return FALSE;
        net_svmask = getbitu(rtcm->buff, i0, nsat);
        i0 += nsat;
        for (j = 0; j < nsat; j++) {
            if (!((net_svmask >> (nsat - 1 - j)) & 1)) continue;
            if (i0 + 6 > rtcm->havebit) return FALSE;
            i0 += 6;
            if ((stec_avail & 0x01) == 0x01) {
                if (i0 + 2 > rtcm->havebit) return FALSE;
                stec_type = getbitu(rtcm->buff, i0, 2);
                i0 += 2;
                sz = stec_sz_t[stec_type];
                if (i0 + sz > rtcm->havebit) return FALSE;
                i0 += sz;
            }
            if ((stec_avail & 0x02) == 0x02) {
                if (i0 + 2 > rtcm->havebit) return FALSE;
                sz_idx = getbitu(rtcm->buff, i0, 2);
                i0 += 2;
                i0 += ngp * dstec_sz_t[sz_idx];
                if (i0 > rtcm->havebit) return FALSE;
            }
        }
    }
    trace(4, "check_bit_width_atmos(): i0=%d, havebit=%d\n", i0, rtcm->havebit);
    return TRUE;
}

/*
 * decode service information message
 */
static int decode_cssr_si(rtcm_t* rtcm, cssr_t* cssr, int i0, int header) {
    int i, j, sync;

    i = i0;
    sync = getbitu(rtcm->buff, i, 1);
    i += 1; /* multiple message indicator */
    cssr->si_cnt = getbitu(rtcm->buff, i, 3);
    i += 3; /* information message counter */
    cssr->si_sz = getbitu(rtcm->buff, i, 2);
    i += 2; /* data size */

    for (j = 0; j < cssr->si_sz; j++) {
        cssr->si_data[j] = (uint64_t)getbitu(rtcm->buff, i, 8) << 32;
        i += 8;
        cssr->si_data[j] |= getbitu(rtcm->buff, i, 32);
        i += 32;
    }
    rtcm->nbit = i;

    return sync ? 0 : 10;
}

/* check if the buffer length is sufficient to decode the service information message */
static int check_bit_width_si(rtcm_t* rtcm, cssr_t* cssr, int i0) {
    int data_sz = 0;
    if (i0 + 6 > rtcm->havebit) return FALSE;
    i0 += 4;
    data_sz = getbitu(rtcm->buff, i0, 2);
    i0 += 2;

    return i0 + 40 * (data_sz + 1) <= rtcm->havebit;
}

/* read list of grid position from ascii file */
static int read_grid_def(const char* gridfile) {
    int no, lath, latm, lonh, lonm;
    double lat, lon, alt;
    char buff[1024], *temp, *p;
    int inet,
        grid[CSSR_MAX_NETWORK] =
            {
                0,
            },
        isqzss = 0, ret;
    FILE* fp;

    for (inet = 0; inet < CSSR_MAX_NETWORK; ++inet) {
        clas_grid[inet][0][0] = -1.0;
        clas_grid[inet][0][1] = -1.0;
        clas_grid[inet][0][2] = -1.0;
    }

    trace(2, "read_grid_def(): gridfile=%s\n", gridfile);
    fp = fopen(gridfile, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(buff, sizeof(buff), fp)) {
        if (strstr(buff, "<CSSR Grid Definition>")) {
            while (fgets(buff, sizeof(buff), fp)) {
                if ((temp = strstr(buff, "<Version>"))) {
                    p = temp + 9;
                    if ((temp = strstr(buff, "</Version>"))) {
                        *temp = '\0';
                    }
                    gridsel = atoi(p);
                    trace(2, "grid definition: version=%d\n", gridsel);
                    break;
                }
            }
            break;
        } else if (strstr(buff, "Compact Network ID    GRID No.  Latitude     Longitude   Ellipsoidal height")) {
            gridsel = 3;
            isqzss = 1;
            trace(2, "grid definition: IS attached file version%d\n", gridsel);
            break;
        } else {
            trace(1, "grid definition: invalid format%d\n", gridsel);
            return -1;
        }
    }
    fclose(fp);

    fp = fopen(gridfile, "r");
    if (fp == NULL) {
        return -1;
    }

    if (isqzss == 0) {
        while (fgets(buff, sizeof(buff), fp)) {
            if (sscanf(buff, "<Network%d>", &inet)) {
                while (fscanf(fp, "%d\t%d\t%d\t%lf\t%d\t%d\t%lf\t%lf", &no, &lath, &latm, &lat, &lonh, &lonm, &lon,
                              &alt) > 0) {
                    if (inet >= 0 && inet < CSSR_MAX_NETWORK) {
                        clas_grid[inet][grid[inet]][0] = (double)lath + ((double)latm / 60.0) + (lat / 3600.0);
                        clas_grid[inet][grid[inet]][1] = (double)lonh + ((double)lonm / 60.0) + (lon / 3600.0);
                        clas_grid[inet][grid[inet]][2] = alt;
                        ++grid[inet];
                        clas_grid[inet][grid[inet]][0] = -1.0;
                        clas_grid[inet][grid[inet]][1] = -1.0;
                        clas_grid[inet][grid[inet]][2] = -1.0;
                    }
                }
            }
        }
    } else {
        if (!fgets(buff, sizeof(buff), fp)) return -1;
        while ((ret = fscanf(fp, "%d %d %lf %lf %lf", &inet, &no, &lat, &lon, &alt)) != EOF) {
            if (inet >= 0 && inet < CSSR_MAX_NETWORK && ret == 5) {
                clas_grid[inet][grid[inet]][0] = lat;
                clas_grid[inet][grid[inet]][1] = lon;
                clas_grid[inet][grid[inet]][2] = alt;
                ++grid[inet];
                clas_grid[inet][grid[inet]][0] = -1.0;
                clas_grid[inet][grid[inet]][1] = -1.0;
                clas_grid[inet][grid[inet]][2] = -1.0;
            }
            trace(3, "grid_info(fscanf:%d), %d, %d, %lf, %lf, %lf\n", ret, inet, no, lat, lon, alt);
        }
    }
    fclose(fp);
    return 0;
}

/* decode QZS L6 CLAS stream */
static int decode_qzs_msg(rtcm_t* rtcm, int head, uint8_t* frame, FILE** ofp) {
    static int startdecode[L6_CH_NUM] = {
        0,
    };
    static int savefacility[L6_CH_NUM] = {-1, -1};
    static int savedelivery[L6_CH_NUM] = {-1, -1};
    static cssr_t _cssr[L6_CH_NUM] = {
        0,
    };

    cssr_t* cssr = &_cssr[chidx];
    int startbit, week;
    int i, ret = 0;
    double tow;

    if (*frame == 0x00 || rtcm->nbit == -1) {
        return 0;
    }

    i = startbit = rtcm->nbit;
    if ((i + 16) > rtcm->havebit) {
        return 0;
    }
    rtcm->ctype = getbitu(rtcm->buff, i, 12);
    i += 12;
    if (rtcm->ctype != 4073) {
        trace(4, "cssr: decode terminate: frame=%02x, havebit=%d, nbit=%d\n", *frame, rtcm->havebit, rtcm->nbit);
        rtcm->nbit = -1;
        *frame = 0;
        return 0;
    }
    rtcm->subtype = getbitu(rtcm->buff, i, 4);
    i += 4;
    if (rtcm->subtype != 1 && startdecode[chidx] == FALSE) {
        return 0;
    }

    switch (rtcm->subtype) {
        case CSSR_TYPE_MASK:
            if (!check_bit_width_mask(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_OC:
            if (!check_bit_width_oc(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_CC:
            if (!check_bit_width_cc(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_CB:
            if (!check_bit_width_cb(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_PB:
            if (!check_bit_width_pb(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_BIAS:
            if (!check_bit_width_bias(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_URA:
            if (!check_bit_width_ura(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_STEC:
            if (!check_bit_width_stec(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_GRID:
            if (!check_bit_width_grid(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_COMBO:
            if (!check_bit_width_combo(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_ATMOS:
            if (!check_bit_width_atmos(rtcm, cssr, i)) return FALSE;
            break;
        case CSSR_TYPE_SI:
            if (!check_bit_width_si(rtcm, cssr, i)) return FALSE;
            break;
        case 0:
            trace(1, "invalid process: frame=%02x, havebit=%d, nbit=%d\n", *frame, rtcm->havebit, rtcm->nbit);
            return 0;
    }

    trace(4, "cssr: frame=0x%02x, ctype=%d, subtype=%d\n", *frame, rtcm->ctype, rtcm->subtype);
    tow = time2gpst(timeget(), &week);
    if (savefacility[chidx] != l6facility[chidx]) {
        if (savefacility[chidx] != -1) {
            trace(1, "L6 data[ch%d]: change facility, week=%d, tow=%.1f, %d(%d) ---> %d(%d)\n", chidx, week, tow,
                  (savefacility[chidx] == -1 ? 0 : savefacility[chidx] + 1),
                  (savefacility[chidx] == -1 ? 0 : savedelivery[chidx]), l6facility[chidx] + 1, l6delivery[chidx]);
        } else {
            trace(1, "L6 data[ch%d]: change facility, week=%d, tow=%.1f,        ---> %d(%d)\n", chidx, week, tow,
                  l6facility[chidx] + 1, l6delivery[chidx]);
        }
        savedelivery[chidx] = l6delivery[chidx];
        savefacility[chidx] = l6facility[chidx];
    }

    switch (rtcm->subtype) {
        case CSSR_TYPE_MASK:
            ret = decode_cssr_mask(rtcm, cssr, i, head, ofp[0]);
            if (startdecode[chidx] == FALSE) {
                trace(1, "start CSSR decoding: week=%d, tow=%.1f\n", week, tow);
                startdecode[chidx] = TRUE;
            }
            break;
        case CSSR_TYPE_OC:
            ret = decode_cssr_oc(rtcm, cssr, i, head, ofp[1]);
            if (ret == -1) {
                rtcm->nbit = -1;
                cssr->iod = -1;
                *frame = 0;
                return 0;
            }
            break;
        case CSSR_TYPE_CC:
            ret = decode_cssr_cc(rtcm, cssr, i, head, ofp[2]);
            if (ret == -1) {
                rtcm->nbit = -1;
                cssr->iod = -1;
                *frame = 0;
                return 0;
            }
            break;
        case CSSR_TYPE_CB:
            ret = decode_cssr_cb(rtcm, cssr, i, head, ofp[3]);
            if (ret == -1) {
                rtcm->nbit = -1;
                cssr->iod = -1;
                *frame = 0;
                return 0;
            }
            break;
        case CSSR_TYPE_PB:
            ret = decode_cssr_pb(rtcm, cssr, i, head, ofp[4]);
            if (ret == -1) {
                rtcm->nbit = -1;
                cssr->iod = -1;
                *frame = 0;
                return 0;
            }
            break;
        case CSSR_TYPE_BIAS:
            ret = decode_cssr_bias(rtcm, cssr, i, head, ofp[5]);
            if (ret == -1) {
                rtcm->nbit = -1;
                cssr->iod = -1;
                *frame = 0;
                return 0;
            }
            break;
        case CSSR_TYPE_URA:
            ret = decode_cssr_ura(rtcm, cssr, i, head, ofp[6]);
            if (ret == -1) {
                rtcm->nbit = -1;
                cssr->iod = -1;
                *frame = 0;
                return 0;
            }
            break;
        case CSSR_TYPE_STEC:
            ret = decode_cssr_stec(rtcm, cssr, i, head, ofp[7]);
            if (ret == -1) {
                rtcm->nbit = -1;
                cssr->iod = -1;
                *frame = 0;
                return 0;
            }
            break;
        case CSSR_TYPE_GRID:
            ret = decode_cssr_grid(rtcm, cssr, i, head, ofp[8]);
            if (ret == -1) {
                rtcm->nbit = -1;
                cssr->iod = -1;
                *frame = 0;
                return 0;
            }
            break;
        case CSSR_TYPE_COMBO:
            ret = decode_cssr_combo(rtcm, cssr, i, head, ofp[11]);
            if (ret == -1) {
                rtcm->nbit = -1;
                cssr->iod = -1;
                *frame = 0;
                return 0;
            }
            break;
        case CSSR_TYPE_ATMOS:
            ret = decode_cssr_atmos(rtcm, cssr, i, head, ofp[12], ofp[13]);
            if (ret == -1) {
                rtcm->nbit = -1;
                cssr->iod = -1;
                *frame = 0;
                return 0;
            }
            break;
        case CSSR_TYPE_SI:
            ret = decode_cssr_si(rtcm, cssr, i, head);
            break;
        default:
            break;
    }

    return ret;
}

static int GetCSSRTime(unsigned char* buff, int* value) {
    static int basetow = -1;
    static int savetow = -1;
    int i = 32 + 8 + 8 + 1;
    int type, subtype, tow;

    type = getbitu(buff, i, 12);
    i += 12;
    *value = -1;
    if (type != 4073) {
        return -1;
    }
    subtype = getbitu(buff, i, 4);
    i += 4;

    if (subtype == CSSR_TYPE_MASK) {
        tow = getbitu(buff, i, 20);
        i += 20;
        basetow = (int)floor((double)tow / 3600.0) * 3600;
        *value = subtype;
    } else {
        tow = getbitu(buff, i, 12);
        i += 12;
        if (basetow != -1) {
            tow += basetow;
            if ((tow < savetow) && (savetow - tow) < 3600 * 24 * 7 / 2) {
                if ((tow < savetow) && (savetow - tow) > 3000) {
                    return -1;
                }
            }
            savetow = tow;
        }
    }
    return tow;
}

static void output_cssr_header(int chidx, int* nframe, unsigned char buff[][BLEN_MSG], int* tow, FILE* fp) {
    char str1[256] = {'\0'}, cVenderid[256] = {'\0'}, cMsggenid[256] = {'\0'}, cTransptn[256] = {'\0'},
         cSubframe[256] = {'\0'};
    int msgid, alert, binary, base, venderid, msggenid, transptn, subframe;
    int type;
    char* endp;

    if (fp == NULL) return;

    if (nframe[chidx] == 0) {
        tow[chidx] = GetCSSRTime(buff[chidx], &type);
    }

    if (tow[chidx] != -1) {
        fprintf(fp, "%d, 0x%02x-0x%02x-0x%02x-0x%02x, ", tow[chidx] + nframe[chidx], buff[chidx][0], buff[chidx][1],
                buff[chidx][2], buff[chidx][3]);
    } else {
        fprintf(fp, "#N/A, 0x%02x-0x%02x-0x%02x-0x%02x, ", buff[chidx][0], buff[chidx][1], buff[chidx][2],
                buff[chidx][3]);
    }
    binary = 0;
    base = 1;
    msgid = buff[chidx][5];
    while (msgid > 0) {
        binary = binary + (msgid % 2) * base;
        msgid = msgid / 2;
        base = base * 10;
    }
    sprintf(str1, "%d", binary);
    memcpy(cVenderid, &str1[0], 3);
    memcpy(cMsggenid, &str1[3], 2);
    memcpy(cTransptn, &str1[5], 2);
    memcpy(cSubframe, &str1[7], 1);

    venderid = strtol(cVenderid, &endp, 2);
    msggenid = strtol(cMsggenid, &endp, 2);
    transptn = strtol(cTransptn, &endp, 2);
    subframe = strtol(cSubframe, &endp, 2);
    fprintf(fp, "%d, %d, %d, %d, %d, %d, %d\n", buff[chidx][4], buff[chidx][5], venderid, msggenid, transptn, subframe,
            alert = (buff[chidx][6] >> 7) & 0x1);

    return;
}

/* decode cssr messages in the QZS L6 subframe */
static int input_cssr(rtcm_t* cssr, unsigned char data, uint8_t* frame, FILE** ofp) {
    static uint32_t preamble[L6_CH_NUM] = {
        0,
    };
    static uint64_t data_p[L6_CH_NUM] = {
        0,
    };
    uint8_t prn, msgid, alert;
    static int nframe[L6_CH_NUM] = {
        0,
    };
    static unsigned char buff[L6_CH_NUM][BLEN_MSG];
    static int decode_start[L6_CH_NUM] = {
        0,
    };
    static int tow[L6_CH_NUM];

    trace(5, "input_cssr: data=%02x\n", data);

    /* synchronize frame */
    if (cssr->nbyte == 0) {
        preamble[chidx] = (preamble[chidx] << 8) | data;
        data_p[chidx] = (data_p[chidx] << 8) | data;
        if (preamble[chidx] != L6FRMPREAMB) {
            return 0;
        }
        preamble[chidx] = 0;
        buff[chidx][cssr->nbyte++] = (L6FRMPREAMB >> 24) & 0xff;
        buff[chidx][cssr->nbyte++] = (L6FRMPREAMB >> 16) & 0xff;
        buff[chidx][cssr->nbyte++] = (L6FRMPREAMB >> 8) & 0xff;
        buff[chidx][cssr->nbyte++] = data;
        return 0;
    }
    buff[chidx][cssr->nbyte++] = data;
    cssr->len = BLEN_MSG;

    if (cssr->nbyte < cssr->len) return 0;
    cssr->nbyte = 0;

    prn = buff[chidx][4];
    msgid = buff[chidx][5];
    alert = (buff[chidx][6] >> 7) & 0x1;
    if (alert != 0) {
        trace(1, "CSSR frame alert!: tow=%.1f\n", time2gpst(timeget(), NULL));
    }

    l6delivery[chidx] = prn;
    l6facility[chidx] = (msgid & 0x18) >> 3;

    if (msgid & 0x01) { /* Subframe indicator */
        if (decode_start[chidx] == 0) {
            trace(1, "CSSR frame first recieve: tow=%.1f\n", time2gpst(timeget(), NULL));
        }
        cssr->havebit = 0;
        decode_start[chidx] = 1;
        cssr->nbit = 0;
        *frame = 0;
        nframe[chidx] = 0;
    } else if (nframe[chidx] >= 5) {
        return 0;
    }
    if (decode_start[chidx] == 1) {
        int i = 1695 * nframe[chidx], j;

        output_cssr_header(chidx, nframe, buff, tow, ofp[10]);

        setbitu(cssr->buff, i, 7, buff[chidx][6] & 0x7f);
        i += 7;
        for (j = 0; j < 211; j++) {
            setbitu(cssr->buff, i, 8, buff[chidx][7 + j]);
            i += 8;
        }
        cssr->havebit += 1695;
        *frame |= (1 << nframe[chidx]);
        nframe[chidx]++;
    }
    return 0;
}

/* decode cssr messages from file stream ---------------------------------------------*/
static int input_cssrf(rtcm_t* cssr, FILE* fp, FILE** ofp) {
    static uint8_t frame[L6_CH_NUM] = {
        0,
    };
    int i, data = 0, ret;

    trace(4, "input_cssrf: data=%02x\n", data);

    for (i = 0; i < 4096; i++) {
        if ((ret = decode_qzs_msg(cssr, 0, &frame[chidx], ofp))) return ret;
        if ((data = fgetc(fp)) == EOF) return -2;
        if ((ret = input_cssr(cssr, (unsigned char)data, &frame[chidx], ofp))) return ret;
    }
    return 0; /* return at every 4k bytes */
}

/* open QZSS L6 message file -------------------------------------------------*/
static FILE* open_L6(char** infile, int n) {
    FILE* fp;
    char* ext;
    int i;

    for (i = 0; i < n; i++) {
        if (!(ext = strrchr(infile[i], '.'))) continue;
        if (!strcmp(ext, ".l6") || !strcmp(ext, ".L6")) {
            break;
        }
    }
    if (i >= n) {
        fprintf(stderr, "No L6 message file in input files.\n");
        return NULL;
    }
    if (!(fp = fopen(infile[i], "rb"))) {
        fprintf(stderr, "L6 message file open error. %s\n", infile[i]);
        return NULL;
    }
    return fp;
}

static int dumpcssr(char** infile, int n, FILE** ofp, const char* gridfile) {
    int ret;
    static rtcm_t rtcm;
    FILE* fp;

    init_rtcm(&rtcm);

    /* open QZSS L6 message file */
    if (!(fp = open_L6(infile, n))) {
        free_rtcm(&rtcm);
        return 0;
    }
    /* read grid definition file */
    if (read_grid_def(gridfile)) {
        fprintf(stderr, "Grid file read error. %s\n", gridfile);
        showmsg("Grid file read error. %s\n", gridfile);
        return -1;
    }

    while (1) {
        if ((ret = input_cssrf(&rtcm, fp, ofp)) < -1) {
            break;
        }
    }
    return 0;
}

static int open_outputfiles(FILE** ofp) {
    char filename[512];
    static char parsename[512] = "parse_cssr";

    for (int i = 0; i < CSSR_TYPE_NUM; i++) {
        ofp[i] = NULL;
    }

    /* mask */
    sprintf(filename, "%s_type1.csv", parsename);
    if (!(ofp[0] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[0],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,No. of GNSS,");
    fprintf(ofp[0], "GNSS ID,Compact SSR Satellite Mask,Compact SSR Signal Mask,Cell-Mask Availability Flag,");
    fprintf(ofp[0], "[Satellite Number],Compact SSR Cell mask\n");

    /* orbit */
    sprintf(filename, "%s_type2.csv", parsename);
    if (!(ofp[1] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[1],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(ofp[1], "[GNSS ID],[Satellite Number],GNSS IODE,Compact SSR Delta Radial,Compact SSR Delta Along-Track,");
    fprintf(ofp[1], "Compact SSR Delta Cross-Track\n");

    /* clock */
    sprintf(filename, "%s_type3.csv", parsename);
    if (!(ofp[2] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[2],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(ofp[2], "[GNSS ID],[Satellite Number],Compact SSR Delta Clock C0\n");

    /* code bias */
    sprintf(filename, "%s_type4.csv", parsename);
    if (!(ofp[3] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[3],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(ofp[3], "[GNSS ID],[Satellite Number],[Satellite Signal],Compact SSR Code Bias\n");

    /* phase bias */
    sprintf(filename, "%s_type5.csv", parsename);
    if (!(ofp[4] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[4],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(ofp[4], "[GNSS ID],[Satellite Number],[Satellite Signal],Compact SSR Phase Bias,");
    fprintf(ofp[4], "Compact SSR Phase Discontinuity Indicator\n");

    /* bias */
    sprintf(filename, "%s_type6.csv", parsename);
    if (!(ofp[5] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[5],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(
        ofp[5],
        "Code Bias Existing Flag,Phase Bias Existing Flag,Network Bias Correction,Compact Network ID,Network SV Mask,");
    fprintf(ofp[5], "[GNSS ID],[Satellite Number],[Satellite Signal],Compact SSR Code Bias,Compact SSR Phase Bias,");
    fprintf(ofp[5], "Compact SSR Phase Discontinuity Indicator\n");

    /* URA */
    sprintf(filename, "%s_type7.csv", parsename);
    if (!(ofp[6] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[6],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(ofp[6], "[GNSS ID],[Satellite Number],SSR URA\n");

    /* STEC */
    sprintf(filename, "%s_type8.csv", parsename);
    if (!(ofp[7] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[7],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(ofp[7], "Compact SSR STEC Correction Type,Compact Network ID,Network SV Mask,");
    fprintf(ofp[7],
            "[GNSS ID],[Satellite Number],SSR STEC Quality Indicator,Polynomial Coefficients C00,Polynomial "
            "Coefficients C01,");
    fprintf(ofp[7], "Polynomial Coefficients C10,Polynomial Coefficients C11\n");

    /* grid */
    sprintf(filename, "%s_type9.csv", parsename);
    if (!(ofp[8] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[8],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(ofp[8], "Tropospheric Delay Correction Type,STEC Residual Correction Range,Compact Network ID,");
    fprintf(ofp[8], "Network SV Mask,Tropospheric Delay Quality Indicator,No. of Grids,[Grid Number],");
    fprintf(ofp[8], "Troposphere Hydro-Static Vertical Delay,Troposphere Wet Vertical Delay,[GNSS ID],");
    fprintf(ofp[8], "[Satellite Number],STEC Residual Correction\n");

    /* combo */
    sprintf(filename, "%s_type11.csv", parsename);
    if (!(ofp[11] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[11],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(ofp[11], "Orbit Existing Flag,Clock Existing Flag,Network Correction,Network ID,Network SV Mask,");
    fprintf(ofp[11], "[GNSS ID],[Satellite Number],GNSS IODE,Compact SSR Delta Radial,Compact SSR Delta Along-Track,");
    fprintf(ofp[11], "Compact SSR Delta Cross-Track,Compact SSR Delta Clock C0\n");

    /* atmos */
    sprintf(filename, "%s_type12_stec.csv", parsename);
    if (!(ofp[12] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[12],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(ofp[12],
            "Tropospheric Correction Availability,STEC Correction Availability,Compact Network ID,No. of Grids,");
    fprintf(ofp[12],
            "Troposphere Quality Indicator,Tropospheric Correction Type,Troposphere Polynomial Coefficients T00,");
    fprintf(ofp[12], "Troposphere Polynomial Coefficients T01,Troposphere Polynomial Coefficients T10,");
    fprintf(ofp[12], "Troposphere Polynomial Coefficients T11,Troposphere Residual Size,Troposphere Residual Offset,");
    fprintf(ofp[12], "Network SV Mask,[GNSS ID],[Satellite Number],STEC Quality Indicator,STEC Correction Type,");
    fprintf(ofp[12],
            "STEC Polynomial Coefficients C00,STEC Polynomial Coefficients C01,STEC Polynomial Coefficients C10,");
    fprintf(ofp[12],
            "STEC Polynomial Coefficients C11,STEC Polynomial Coefficients C02,STEC Polynomial Coefficients C20,");
    fprintf(ofp[12], "STEC Residual Size\n");

    sprintf(filename, "%s_type12_grid.csv", parsename);
    if (!(ofp[13] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[13],
            "Message Number,Message Sub Type ID,GNSS Epoch Time 1s,SSR Update Interval,Multiple Message Indicator,IOD "
            "SSR,");
    fprintf(ofp[13],
            "Compact Network ID,Network SV Mask,No. of Grids,[Grid Number],Troposphere Hydro-Static Vertical Delay,");
    fprintf(ofp[13], "Troposphere Wet Vertical Delay,[GNSS ID],[Satellite Number],STEC Residual Correction[TECU]\n");

    /* header */
    sprintf(filename, "%s_header.csv", parsename);
    if (!(ofp[10] = fopen(filename, "w"))) {
        return -1;
    }
    fprintf(ofp[10],
            "Epoch Time,Preamble,PRN,L6 message type ID,Vender ID,Message Generation Facility ID,CLAS Transmit Pattern "
            "ID,Subframe indicator,Alert Flag\n");

    return 0;
}

static void close_outputfiles(FILE** ofp) {
    int i;
    for (i = 0; i < CSSR_TYPE_NUM; i++) {
        if (ofp[i] != NULL) {
            fclose(ofp[i]);
            ofp[i] = NULL;  // prevent double close
        }
    }
}

/* ---- public entry point --------------------------------------------------*/
int cssr_parse_dump(const char* gridfile, char** infile, int n) {
    FILE* ofp[CSSR_TYPE_NUM] = {0};
    int stat;

    if (open_outputfiles(ofp) == -1) {
        fprintf(stderr, "Can't open output files.\n");
        return -1;
    }
    stat = dumpcssr(infile, n, ofp, gridfile);
    close_outputfiles(ofp);
    return stat;
}
