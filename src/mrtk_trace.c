/*------------------------------------------------------------------------------
 * mrtk_trace.c : debug trace and logging functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#include "mrtklib/mrtk_trace.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_obs.h"

#ifdef TRACE

#include "mrtklib/mrtklib.h"

#include <stdio.h>
#include <stdarg.h>

/** @brief Tick time captured at traceopen, used by tracet() for elapsed time */
static uint32_t tick_trace=0;

/**
 * @brief Map RTKLIB trace level (1=most critical) to mrtk_log_level_t.
 *
 * RTKLIB levels:  1 (error) -> 5 (verbose debug)
 * MRTK levels:    DEBUG(0) < INFO(1) < WARN(2) < ERROR(3) < NONE(4)
 */
static mrtk_log_level_t trace_level_to_mrtk(int level)
{
    switch (level) {
        case 1:  return MRTK_LOG_ERROR;
        case 2:  return MRTK_LOG_WARN;
        case 3:  return MRTK_LOG_INFO;
        default: return MRTK_LOG_DEBUG; /* levels 4, 5 */
    }
}
extern void traceopen(const char *file)
{
    (void)file;
    tick_trace=tickget();
}
extern void traceclose(void)
{
    /* no-op: file I/O removed */
}
extern void tracelevel(int level)
{
    mrtk_log_level_t mlevel;

    if (!g_mrtk_legacy_ctx) return;

    /* Map RTKLIB threshold to mrtk threshold.
     * RTKLIB level_trace=N means "show messages with level <= N".
     * RTKLIB level 1 = most critical, 5 = most verbose.
     * We map the threshold so that messages at the requested verbosity
     * and above are shown. */
    if      (level<=0) mlevel=MRTK_LOG_NONE;
    else if (level==1) mlevel=MRTK_LOG_ERROR;
    else if (level==2) mlevel=MRTK_LOG_WARN;
    else if (level==3) mlevel=MRTK_LOG_INFO;
    else               mlevel=MRTK_LOG_DEBUG;

    mrtk_context_set_log_level(g_mrtk_legacy_ctx, mlevel);
}
extern void trace(int level, const char *format, ...)
{
    va_list ap;

    if (!g_mrtk_legacy_ctx) return;

    va_start(ap, format);
    mrtk_log_v(g_mrtk_legacy_ctx, trace_level_to_mrtk(level), format, ap);
    va_end(ap);
}
extern void tracet(int level, const char *format, ...)
{
    va_list ap;
    char buf[1024];
    int  off;

    if (!g_mrtk_legacy_ctx) return;

    /* Prepend elapsed time (seconds since traceopen) */
    off=snprintf(buf, sizeof(buf), "%d %9.3f: ", level,
                 (tickget()-tick_trace)/1000.0);
    if (off<0) off=0;

    va_start(ap, format);
    vsnprintf(buf+off, sizeof(buf)-(size_t)off, format, ap);
    va_end(ap);

    mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level), "%s", buf);
}
extern void tracemat(int level, const double *A, int n, int m, int p, int q)
{
    char buf[1024];
    int i,j,off;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<n;i++) {
        off=0;
        for (j=0;j<m;j++) {
            off+=snprintf(buf+off, sizeof(buf)-(size_t)off, " %*.*f", p, q,
                          A[i+j*n]);
            if (off>=(int)sizeof(buf)) break;
        }
        mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level), "%s", buf);
    }
}
extern void traceobs(int level, const obsd_t *obs, int n)
{
    char str[64],id[16];
    int i;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<n;i++) {
        time2str(obs[i].time,str,3);
        satno2id(obs[i].sat,id);
        mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                 " (%2d) %s %-3s rcv%d %13.3f %13.3f %13.3f %13.3f"
                 " %d %d %d %d %3.1f %3.1f",
                 i+1,str,id,obs[i].rcv,obs[i].L[0],obs[i].L[1],obs[i].P[0],
                 obs[i].P[1],obs[i].LLI[0],obs[i].LLI[1],obs[i].code[0],
                 obs[i].code[1],obs[i].SNR[0]*SNR_UNIT,obs[i].SNR[1]*SNR_UNIT);
    }
}
extern void tracenav(int level, const nav_t *nav)
{
    char s1[64],s2[64],id[16];
    int i;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<nav->n;i++) {
        time2str(nav->eph[i].toe,s1,0);
        time2str(nav->eph[i].ttr,s2,0);
        satno2id(nav->eph[i].sat,id);
        mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                 "(%3d) %-3s : %s %s %3d %3d %02x",
                 i+1,id,s1,s2,nav->eph[i].iode,nav->eph[i].iodc,
                 nav->eph[i].svh);
    }
    mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
             "(ion) %9.4e %9.4e %9.4e %9.4e",
             nav->ion_gps[0],nav->ion_gps[1],nav->ion_gps[2],nav->ion_gps[3]);
    mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
             "(ion) %9.4e %9.4e %9.4e %9.4e",
             nav->ion_gps[4],nav->ion_gps[5],nav->ion_gps[6],nav->ion_gps[7]);
    mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
             "(ion) %9.4e %9.4e %9.4e %9.4e",
             nav->ion_gal[0],nav->ion_gal[1],nav->ion_gal[2],nav->ion_gal[3]);
}
extern void tracegnav(int level, const nav_t *nav)
{
    char s1[64],s2[64],id[16];
    int i;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<nav->ng;i++) {
        time2str(nav->geph[i].toe,s1,0);
        time2str(nav->geph[i].tof,s2,0);
        satno2id(nav->geph[i].sat,id);
        mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                 "(%3d) %-3s : %s %s %2d %2d %8.3f",
                 i+1,id,s1,s2,nav->geph[i].frq,nav->geph[i].svh,
                 nav->geph[i].taun*1E6);
    }
}
extern void tracehnav(int level, const nav_t *nav)
{
    char s1[64],s2[64],id[16];
    int i;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<nav->ns;i++) {
        time2str(nav->seph[i].t0,s1,0);
        time2str(nav->seph[i].tof,s2,0);
        satno2id(nav->seph[i].sat,id);
        mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                 "(%3d) %-3s : %s %s %2d %2d",
                 i+1,id,s1,s2,nav->seph[i].svh,nav->seph[i].sva);
    }
}
extern void tracepeph(int level, const nav_t *nav)
{
    char s[64],id[16];
    int i,j;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<nav->ne;i++) {
        time2str(nav->peph[i].time,s,0);
        for (j=0;j<MAXSAT;j++) {
            satno2id(j+1,id);
            mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                     "%-3s %d %-3s %13.3f %13.3f %13.3f %13.3f"
                     " %6.3f %6.3f %6.3f %6.3f",
                     s,nav->peph[i].index,id,
                     nav->peph[i].pos[j][0],nav->peph[i].pos[j][1],
                     nav->peph[i].pos[j][2],nav->peph[i].pos[j][3]*1E9,
                     nav->peph[i].std[j][0],nav->peph[i].std[j][1],
                     nav->peph[i].std[j][2],nav->peph[i].std[j][3]*1E9);
        }
    }
}
extern void tracepclk(int level, const nav_t *nav)
{
    char s[64],id[16];
    int i,j;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<nav->nc;i++) {
        time2str(nav->pclk[i].time,s,0);
        for (j=0;j<MAXSAT;j++) {
            satno2id(j+1,id);
            mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level),
                     "%-3s %d %-3s %13.3f %6.3f",
                     s,nav->pclk[i].index,id,
                     nav->pclk[i].clk[j][0]*1E9,nav->pclk[i].std[j][0]*1E9);
        }
    }
}
extern void traceb(int level, const uint8_t *p, int n)
{
    char buf[1024];
    int i,off=0;

    if (!g_mrtk_legacy_ctx) return;

    for (i=0;i<n;i++) {
        off+=snprintf(buf+off, sizeof(buf)-(size_t)off, "%02X%s",
                      *p++, i%8==7?" ":"");
        if (off>=(int)sizeof(buf)) break;
    }
    mrtk_log(g_mrtk_legacy_ctx, trace_level_to_mrtk(level), "%s", buf);
}
#else
extern void traceopen(const char *file) {}
extern void traceclose(void) {}
extern void tracelevel(int level) {}
extern void trace   (int level, const char *format, ...) {}
extern void tracet  (int level, const char *format, ...) {}
extern void tracemat(int level, const double *A, int n, int m, int p, int q) {}
extern void traceobs(int level, const obsd_t *obs, int n) {}
extern void tracenav(int level, const nav_t *nav) {}
extern void tracegnav(int level, const nav_t *nav) {}
extern void tracehnav(int level, const nav_t *nav) {}
extern void tracepeph(int level, const nav_t *nav) {}
extern void tracepclk(int level, const nav_t *nav) {}
extern void traceb  (int level, const uint8_t *p, int n) {}

#endif /* TRACE */
