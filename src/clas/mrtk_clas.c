/*------------------------------------------------------------------------------
 * mrtk_clas.c : QZSS CLAS (L6D) CSSR decoder and bank management
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
 * @file mrtk_clas.c
 * @brief QZSS CLAS (L6D) CSSR decoder and bank management.
 *
 * References:
 *   [1] claslib (https://github.com/mf-arai/claslib) — upstream implementation
 *   [2] IS-QZSS-L6-003 — CLAS Compact SSR Specification
 *
 * Notes:
 *   - Ported from upstream claslib/cssr.c with singleton→context conversion.
 *   - All output_cssr_* / CSV debug functions are omitted (not needed in core).
 *   - Decoders write to ctx->dec_ssr[] (clas_dec_ssr_t) as intermediate buffer,
 *     NOT to nav_t.ssr[] directly, to isolate from MRTKLIB ssr_t differences.
 */
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "mrtklib/mrtk_clas.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_trace.h"

/*============================================================================
 * Constants
 *===========================================================================*/

/* SSR update intervals (seconds), indexed by 4-bit UDI field */
static const double ssrudint[16] = {
    1, 2, 5, 10, 15, 30, 60, 120, 240, 300, 600, 900, 1800, 3600, 7200, 10800
};

#define SQR(x)   ((x)*(x))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

/*============================================================================
 * Context Management
 *===========================================================================*/

extern int clas_ctx_init(clas_ctx_t *ctx)
{
    int i;
    if (!ctx) return -1;

    memset(ctx, 0, sizeof(clas_ctx_t));

    for (i = 0; i < CLAS_CH_NUM; i++) {
        ctx->bank[i] = (clas_bank_ctrl_t *)calloc(1, sizeof(clas_bank_ctrl_t));
        if (!ctx->bank[i]) {
            clas_ctx_free(ctx);
            return -1;
        }
    }
    /* initialize banks (sets bank->use = 1) */
    for (i = 0; i < CLAS_CH_NUM; i++) {
        clas_bank_init(ctx, i);
    }
    /* initialize tow_ref to -1 (unset) */
    for (i = 0; i < CSSR_REF_MAX; i++) {
        ctx->tow_ref[i] = -1;
    }
    for (i = 0; i < CLAS_CH_NUM; i++) {
        ctx->l6facility[i] = -1;
        ctx->l6delivery[i] = -1;
    }
    ctx->initialized = 1;
    return 0;
}

extern void clas_ctx_free(clas_ctx_t *ctx)
{
    int i;
    if (!ctx) return;

    for (i = 0; i < CLAS_CH_NUM; i++) {
        free(ctx->bank[i]);
        ctx->bank[i] = NULL;
    }
    ctx->initialized = 0;
}

extern void clas_bank_init(clas_ctx_t *ctx, int ch)
{
    clas_bank_ctrl_t *bank;
    if (!ctx || ch < 0 || ch >= CLAS_CH_NUM || !ctx->bank[ch]) return;

    bank = ctx->bank[ch];
    memset(&bank->LatestTrop, 0, sizeof(clas_latest_trop_t));
    memset(bank->OrbitBank, 0, sizeof(bank->OrbitBank));
    memset(bank->ClockBank, 0, sizeof(bank->ClockBank));
    memset(bank->BiasBank, 0, sizeof(bank->BiasBank));
    memset(bank->TropBank, 0, sizeof(bank->TropBank));
    bank->Facility = -1;
    bank->separation = 0;
    bank->NextOrbit = 0;
    bank->NextClock = 0;
    bank->NextBias = 0;
    bank->NextTrop = 0;
    bank->use = 1;

    /* initialize fastfix=1 for all networks (matches upstream init_fastfix_flag):
     * fastfix=1 → tropless=0 → accept trop entries regardless of future timestamp.
     * This allows clas_bank_get_close() to find trop data that may be timestamped
     * slightly ahead of the current observation time. */
    for (int i = 1; i < CLAS_MAX_NETWORK; i++) {
        bank->fastfix[i] = 1;
    }
}

/*============================================================================
 * Helper Functions
 *===========================================================================*/

static double decode_sval(const uint8_t *buff, int i, int n, double lsb)
{
    int slim = -((1 << (n - 1)) - 1) - 1, v;
    v = getbits(buff, i, n);
    return (v == slim) ? CSSR_INVALID_VALUE : (double)v * lsb;
}

extern int cssr_sys2gnss(int sys, int *prn0)
{
    int id = CSSR_SYS_NONE;
    if (prn0) *prn0 = 1;

    switch (sys) {
        case SYS_GPS: id = CSSR_SYS_GPS; break;
        case SYS_GLO: id = CSSR_SYS_GLO; break;
        case SYS_GAL: id = CSSR_SYS_GAL; break;
        case SYS_CMP: id = CSSR_SYS_BDS; break;
        case SYS_SBS: id = CSSR_SYS_SBS; if (prn0) *prn0 = 120; break;
        case SYS_QZS: id = CSSR_SYS_QZS; if (prn0) *prn0 = 193; break;
    }
    return id;
}

extern int cssr_gnss2sys(int gnss, int *prn0)
{
    int sys = SYS_NONE;
    if (prn0) *prn0 = 1;

    switch (gnss) {
        case CSSR_SYS_GPS: sys = SYS_GPS; break;
        case CSSR_SYS_GLO: sys = SYS_GLO; break;
        case CSSR_SYS_GAL: sys = SYS_GAL; break;
        case CSSR_SYS_BDS: sys = SYS_CMP; break;
        case CSSR_SYS_SBS: sys = SYS_SBS; if (prn0) *prn0 = 120; break;
        case CSSR_SYS_QZS: sys = SYS_QZS; if (prn0) *prn0 = 193; break;
    }
    return sys;
}

static int svmask2nsat(uint64_t svmask)
{
    int j, nsat = 0;
    for (j = 0; j < CSSR_MAX_SV_GNSS; j++) {
        if ((svmask >> (CSSR_MAX_SV_GNSS - 1 - j)) & 1) nsat++;
    }
    return nsat;
}

static int sigmask2nsig(uint16_t sigmask)
{
    int j, nsig = 0;
    for (j = 0; j < CSSR_MAX_SIG; j++) {
        if ((sigmask >> j) & 1) nsig++;
    }
    return nsig;
}

static int svmask2nsatlist(uint64_t svmask, int id, int *sat)
{
    int j, nsat = 0, sys, prn_min;
    sys = cssr_gnss2sys(id, &prn_min);
    for (j = 0; j < CSSR_MAX_SV_GNSS; j++) {
        if ((svmask >> (CSSR_MAX_SV_GNSS - 1 - j)) & 1) {
            sat[nsat++] = satno(sys, prn_min + j);
        }
    }
    return nsat;
}

static int svmask2sat(uint64_t *svmask, int *sat)
{
    int j, id, nsat = 0, sys, prn_min;
    for (id = 0; id < CSSR_MAX_GNSS; id++) {
        sys = cssr_gnss2sys(id, &prn_min);
        for (j = 0; j < CSSR_MAX_SV_GNSS; j++) {
            if ((svmask[id] >> (CSSR_MAX_SV_GNSS - 1 - j)) & 1) {
                if (sat) sat[nsat] = satno(sys, prn_min + j);
                nsat++;
            }
        }
    }
    return nsat;
}

static float decode_cssr_quality_stec(int a, int b)
{
    if ((a == 0 && b == 0) || (a == 7 && b == 7)) return 9999.0f * 1000.0f;
    return (float)((1.0 + b * 0.25) * pow(3.0, a) - 1.0);
}

static float decode_cssr_quality_trop(int a, int b)
{
    if ((a == 0 && b == 0) || (a == 7 && b == 7)) return 9999.0f;
    return (float)((1.0 + b * 0.25) * pow(3.0, a) - 1.0);
}

static void check_week_ref(clas_ctx_t *ctx, clas_l6buf_t *l6, int tow, int i)
{
    if (ctx->obs_ref[i].time != 0 || ctx->obs_ref[i].sec != 0.0) {
        gtime_t time = gpst2time(ctx->week_ref[i], tow);
        if (timediff(time, ctx->obs_ref[i]) > (86400 * 7 / 2)) {
            trace(NULL,2, "check_week_ref(): adjust ref week, subtype=%2d, week=%d\n",
                  i + 1, --ctx->week_ref[i]);
        }
        ctx->obs_ref[i].time = 0;
        ctx->obs_ref[i].sec = 0.0;
    }
    if (l6->tow0 != -1) {
        if (ctx->tow_ref[i] != -1 && ((tow - ctx->tow_ref[i]) < (-86400 * 7 / 2))) {
            ++ctx->week_ref[i];
        }
        ctx->tow_ref[i] = tow;
    }
}

/* signal mask to per-satellite signal indices (position-based) */
static int sigmask2sig_p(int nsat, int *sat, uint16_t *sigmask,
                         uint16_t *cellmask, int *nsig, int *sig)
{
    int j, k, id, sys, sys_p = -1, nsig_s = 0, code[CSSR_MAX_SIG];

    for (j = 0; j < nsat; j++) {
        sys = satsys(sat[j], NULL);
        if (sys != sys_p) {
            id = cssr_sys2gnss(sys, NULL);
            for (k = 0, nsig_s = 0; k < CSSR_MAX_SIG; k++) {
                if ((sigmask[id] >> (CSSR_MAX_SIG - 1 - k)) & 1)
                    code[nsig_s++] = k;
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

/* signal mask to per-satellite RTKLIB signal codes */
static int sigmask2sig(int nsat, int *sat, uint16_t *sigmask,
                       uint16_t *cellmask, int *nsig, int *sig)
{
    int j, k, id, sys, sys_p = -1, nsig_s = 0, code[CSSR_MAX_SIG], csize = 0;
    int *codes = NULL;
    static const int codes_gps[] = {
        CODE_L1C,CODE_L1P,CODE_L1W,CODE_L1S,CODE_L1L,CODE_L1X,
        CODE_L2S,CODE_L2L,CODE_L2X,CODE_L2P,CODE_L2W,
        CODE_L5I,CODE_L5Q,CODE_L5X
    };
    static const int codes_glo[] = {
        CODE_L1C,CODE_L1P,CODE_L2C,CODE_L2P,CODE_L3I,CODE_L3Q,CODE_L3X
    };
    static const int codes_gal[] = {
        CODE_L1B,CODE_L1C,CODE_L1X,CODE_L5I,CODE_L5Q,
        CODE_L5X,CODE_L7I,CODE_L7Q,CODE_L7X,CODE_L8I,CODE_L8Q,CODE_L8X
    };
    static const int codes_qzs[] = {
        CODE_L1C,CODE_L1S,CODE_L1L,CODE_L1X,CODE_L2S,CODE_L2L,CODE_L2X,
        CODE_L5I,CODE_L5Q,CODE_L5X
    };
    static const int codes_bds[] = {
        CODE_L2I,CODE_L2Q,CODE_L2X,
        CODE_L6I,CODE_L6Q,CODE_L6X,
        CODE_L7I,CODE_L7Q,CODE_L7X
    };
    static const int codes_sbs[] = {
        CODE_L1C,CODE_L5I,CODE_L5Q,CODE_L5X
    };

    for (j = 0; j < nsat; j++) {
        sys = satsys(sat[j], NULL);
        if (sys != sys_p) {
            id = cssr_sys2gnss(sys, NULL);
            switch (sys) {
                case SYS_GPS: codes=(int*)codes_gps; csize=sizeof(codes_gps)/sizeof(int); break;
                case SYS_GLO: codes=(int*)codes_glo; csize=sizeof(codes_glo)/sizeof(int); break;
                case SYS_GAL: codes=(int*)codes_gal; csize=sizeof(codes_gal)/sizeof(int); break;
                case SYS_CMP: codes=(int*)codes_bds; csize=sizeof(codes_bds)/sizeof(int); break;
                case SYS_QZS: codes=(int*)codes_qzs; csize=sizeof(codes_qzs)/sizeof(int); break;
                case SYS_SBS: codes=(int*)codes_sbs; csize=sizeof(codes_sbs)/sizeof(int); break;
                default: codes=NULL; csize=0; break;
            }
            for (k = 0, nsig_s = 0; k < csize && k < CSSR_MAX_SIG; k++) {
                if ((sigmask[id] >> (CSSR_MAX_SIG - 1 - k)) & 1)
                    code[nsig_s++] = codes[k];
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

/*============================================================================
 * Bank Internal Helpers — GET-SAME (find exact time match in ring buffer)
 *===========================================================================*/

static clas_orbit_bank_t *get_same_orbit(clas_bank_ctrl_t *bank, gtime_t time, int network)
{
    int i;
    for (i = 0; i < CLAS_BANK_NUM; i++) {
        if (bank->OrbitBank[i].use && bank->OrbitBank[i].network == network &&
            timediff(bank->OrbitBank[i].time, time) == 0.0)
            return &bank->OrbitBank[i];
    }
    return NULL;
}

static clas_clock_bank_t *get_same_clock(clas_bank_ctrl_t *bank, gtime_t time, int network)
{
    int i;
    for (i = 0; i < CLAS_BANK_NUM; i++) {
        if (bank->ClockBank[i].use && bank->ClockBank[i].network == network &&
            timediff(bank->ClockBank[i].time, time) == 0.0)
            return &bank->ClockBank[i];
    }
    return NULL;
}

static clas_bias_bank_t *get_same_bias(clas_bank_ctrl_t *bank, gtime_t time, int network)
{
    int i;
    for (i = 0; i < CLAS_BANK_NUM; i++) {
        if (bank->BiasBank[i].use && bank->BiasBank[i].network == network &&
            timediff(bank->BiasBank[i].time, time) == 0.0)
            return &bank->BiasBank[i];
    }
    return NULL;
}

static clas_trop_bank_t *get_same_trop(clas_bank_ctrl_t *bank, gtime_t time)
{
    int i;
    for (i = 0; i < CLAS_BANK_NUM; i++) {
        if (bank->TropBank[i].use && timediff(bank->TropBank[i].time, time) == 0.0)
            return &bank->TropBank[i];
    }
    return NULL;
}

/*============================================================================
 * Bank Internal Helpers — GET-CLOSE (find nearest time match in ring buffer)
 *===========================================================================*/

static clas_orbit_bank_t *get_close_orbit(clas_bank_ctrl_t *bank, gtime_t time,
                                          int network, double age)
{
    int search = (bank->separation & (1 << (network - 1))) ? network : 0;
    int pos = -1, i;

    for (i = 0; i < CLAS_BANK_NUM; i++) {
        if (!bank->OrbitBank[i].use || bank->OrbitBank[i].network != search)
            continue;
        if (fabs(timediff(bank->OrbitBank[i].time, time)) > age) continue;
        if (pos != -1 && fabs(timediff(bank->OrbitBank[pos].time, time)) <
            fabs(timediff(bank->OrbitBank[i].time, time))) continue;
        if (timediff(bank->OrbitBank[i].time, time) > 0.0) continue;
        pos = i;
    }
    return (pos != -1) ? &bank->OrbitBank[pos] : NULL;
}

static clas_clock_bank_t *get_close_clock(clas_bank_ctrl_t *bank, gtime_t obstime,
                                          gtime_t time, int network, double age)
{
    int search = (bank->separation & (1 << (network - 1))) ? network : 0;
    int pos = -1, i;

    for (i = 0; i < CLAS_BANK_NUM; i++) {
        if (!bank->ClockBank[i].use || bank->ClockBank[i].network != search)
            continue;
        if (timediff(bank->ClockBank[i].time, time) >= age) continue;
        if (pos != -1 && timediff(bank->ClockBank[pos].time, bank->ClockBank[i].time) > 0.0)
            continue;
        if (timediff(bank->ClockBank[i].time, obstime) > 0.0) continue;
        pos = i;
    }
    return (pos != -1) ? &bank->ClockBank[pos] : NULL;
}

static clas_bias_bank_t *get_close_cbias(clas_bank_ctrl_t *bank, gtime_t time,
                                         int network, double age)
{
    int pos = -1, i;
    if (network < 0 || network >= CLAS_MAX_NETWORK) return NULL;

    for (i = 0; i < CLAS_BANK_NUM; i++) {
        if (!bank->BiasBank[i].use || bank->BiasBank[i].network != network)
            continue;
        if (timediff(bank->BiasBank[i].time, time) > age) continue;
        if (pos != -1 && timediff(bank->BiasBank[pos].time, bank->BiasBank[i].time) > 0.0)
            continue;
        if ((bank->BiasBank[i].bflag & 0x01) == 0x01) pos = i;
    }
    return (pos != -1) ? &bank->BiasBank[pos] : NULL;
}

static clas_bias_bank_t *get_close_pbias(clas_bank_ctrl_t *bank, gtime_t time,
                                         int network, double age)
{
    int pos = -1, i;
    if (network < 0 || network >= CLAS_MAX_NETWORK) return NULL;

    for (i = 0; i < CLAS_BANK_NUM; i++) {
        if (!bank->BiasBank[i].use || bank->BiasBank[i].network != network)
            continue;
        if (timediff(bank->BiasBank[i].time, time) > age) continue;
        if (pos != -1 && timediff(bank->BiasBank[pos].time, bank->BiasBank[i].time) > 0.0)
            continue;
        if ((bank->BiasBank[i].bflag & 0x02) == 0x02) pos = i;
    }
    return (pos != -1) ? &bank->BiasBank[pos] : NULL;
}

static clas_bias_bank_t *get_close_net_cbias(clas_bank_ctrl_t *bank, gtime_t time,
                                             int network, double age)
{
    clas_bias_bank_t *cb;
    if (network != 0 && !(cb = get_close_cbias(bank, time, network, age)))
        cb = get_close_cbias(bank, time, 0, age);
    else
        cb = get_close_cbias(bank, time, (network == 0) ? 0 : network, age);
    return cb;
}

static clas_bias_bank_t *get_close_net_pbias(clas_bank_ctrl_t *bank, gtime_t time,
                                             int network, double age)
{
    clas_bias_bank_t *pb;
    if (network != 0 && !(pb = get_close_pbias(bank, time, network, age)))
        pb = get_close_pbias(bank, time, 0, age);
    else
        pb = get_close_pbias(bank, time, (network == 0) ? 0 : network, age);
    return pb;
}

static clas_trop_bank_t *get_close_trop(clas_bank_ctrl_t *bank, gtime_t obstime,
                                        gtime_t time, int network, double age, int tropless)
{
    int pos = -1, i;
    if (network < 1 || network >= CLAS_MAX_NETWORK) return NULL;

    for (i = 0; i < CLAS_BANK_NUM; i++) {
        if (!bank->TropBank[i].use) continue;
        if (fabs(timediff(time, bank->TropBank[i].time)) > age) continue;
        if (pos != -1 && timediff(bank->TropBank[pos].time, bank->TropBank[i].time) > 0.0)
            continue;
        if (tropless && timediff(obstime, bank->TropBank[i].time) < 0.0) continue;
        if (bank->TropBank[i].gridnum[network - 1] > 0) pos = i;
    }
    return (pos != -1) ? &bank->TropBank[pos] : NULL;
}

static int get_bias_save_point(clas_bias_bank_t *bias, int prn, int mode)
{
    int i;
    for (i = 0; i < MAXCODE; i++) {
        if (bias->smode[prn - 1][i] == mode) return i;
    }
    for (i = 0; i < MAXCODE; i++) {
        if (bias->smode[prn - 1][i] == 0) return i;
    }
    return -1;
}

/*============================================================================
 * Bank SET Functions — store decoded corrections into ring buffers
 *
 * Read from ctx->dec_ssr[] (intermediate decoder buffer) instead of nav->ssr_ch[][]
 *===========================================================================*/

static void check_cssr_changed_facility(clas_bank_ctrl_t *bank, int facility)
{
    if (bank->Facility != facility) {
        trace(NULL,4, "bank clear: facility changed %d->%d\n", bank->Facility + 1, facility + 1);
        memset(&bank->LatestTrop, 0, sizeof(clas_latest_trop_t));
        memset(bank->OrbitBank, 0, sizeof(bank->OrbitBank));
        memset(bank->ClockBank, 0, sizeof(bank->ClockBank));
        memset(bank->BiasBank, 0, sizeof(bank->BiasBank));
        memset(bank->TropBank, 0, sizeof(bank->TropBank));
        bank->Facility = facility;
        bank->separation = 0;
        bank->NextOrbit = bank->NextClock = bank->NextBias = bank->NextTrop = 0;
    }
}

static void set_cssr_bank_orbit(clas_ctx_t *ctx, int ch, gtime_t time, int network)
{
    clas_bank_ctrl_t *bank = ctx->bank[ch];
    clas_orbit_bank_t *orbit;
    clas_dec_ssr_t *ssr;
    int i;

    if ((orbit = get_same_orbit(bank, time, network)) == NULL) {
        orbit = &bank->OrbitBank[bank->NextOrbit];
        orbit->network = network;
        orbit->time = time;
        orbit->use = 1;
        if (++bank->NextOrbit >= CLAS_BANK_NUM) bank->NextOrbit = 0;
        memset(orbit->prn, 0, sizeof(orbit->prn));
    } else {
        /* clear existing entries */
        for (i = 0; i < MAXSAT; i++) {
            if (!orbit->prn[i]) continue;
            orbit->prn[i] = 0;
            orbit->udi[i] = 0.0;
            orbit->iod[i] = 0;
            orbit->iode[i] = 0;
            orbit->deph0[i] = orbit->deph1[i] = orbit->deph2[i] = 0.0;
        }
    }
    for (i = 0; i < MAXSAT; i++) {
        ssr = &ctx->dec_ssr[i];
        if (ssr->update_oc != 1) continue;
        if (ssr->deph[0] != CSSR_INVALID_VALUE) {
            orbit->prn[i] = i + 1;
            orbit->udi[i] = ssr->udi[0];
            orbit->iod[i] = ssr->iod[0];
            orbit->iode[i] = ssr->iode;
            orbit->deph0[i] = ssr->deph[0];
            orbit->deph1[i] = ssr->deph[1];
            orbit->deph2[i] = ssr->deph[2];
        } else {
            orbit->prn[i] = 0;
        }
    }
}

static void set_cssr_bank_clock(clas_ctx_t *ctx, int ch, gtime_t time, int network)
{
    clas_bank_ctrl_t *bank = ctx->bank[ch];
    clas_clock_bank_t *clk;
    clas_dec_ssr_t *ssr;
    int i;

    if ((clk = get_same_clock(bank, time, network)) == NULL) {
        clk = &bank->ClockBank[bank->NextClock];
        clk->network = network;
        clk->time = time;
        clk->use = 1;
        if (++bank->NextClock >= CLAS_BANK_NUM) bank->NextClock = 0;
        memset(clk->prn, 0, sizeof(clk->prn));
    } else {
        for (i = 0; i < MAXSAT; i++) {
            if (!clk->prn[i]) continue;
            clk->prn[i] = 0;
            clk->udi[i] = 0.0;
            clk->iod[i] = 0;
            clk->c0[i] = 0.0;
        }
    }
    for (i = 0; i < MAXSAT; i++) {
        ssr = &ctx->dec_ssr[i];
        if (ssr->update_cc != 1) continue;
        if (ssr->dclk[0] != CSSR_INVALID_VALUE) {
            clk->prn[i] = i + 1;
            clk->udi[i] = ssr->udi[1];
            clk->iod[i] = ssr->iod[1];
            clk->c0[i] = ssr->dclk[0];
        } else {
            clk->prn[i] = 0;
        }
    }
}

static void set_cssr_bank_cbias(clas_ctx_t *ctx, int ch, gtime_t time, int network, int iod)
{
    clas_bank_ctrl_t *bank = ctx->bank[ch];
    clas_bias_bank_t *bias;
    clas_dec_ssr_t *ssr;
    int i, j, pos;

    if ((bias = get_same_bias(bank, time, network)) == NULL) {
        bias = &bank->BiasBank[bank->NextBias];
        bias->network = network;
        bias->bflag = 0;
        bias->time = time;
        bias->use = 1;
        if (++bank->NextBias >= CLAS_BANK_NUM) bank->NextBias = 0;
        memset(bias->smode, 0, sizeof(bias->smode));
        memset(bias->sflag, 0, sizeof(bias->sflag));
        memset(bias->prn, 0, sizeof(bias->prn));
    }
    /* clear previous code bias entries */
    if (bias->bflag & 0x01) {
        for (i = 0; i < MAXSAT; i++) {
            for (j = 0; j < MAXCODE; j++) {
                if (bias->sflag[i][j] & 0x01) {
                    bias->sflag[i][j] ^= 0x01;
                    bias->cbias[i][j] = 0.0;
                }
            }
        }
        bias->bflag ^= 0x01;
    }
    for (i = 0; i < MAXSAT; i++) {
        ssr = &ctx->dec_ssr[i];
        if (ssr->update_cb != 1 || ssr->nsig <= 0) continue;
        bias->prn[i] = i + 1;
        bias->bflag |= 0x01;
        bias->udi[i] = ssr->udi[4];
        bias->iod[i] = ssr->iod[4];
        for (j = 0; j < MAXCODE; j++) {
            if (ssr->smode[j] != 0 &&
                (pos = get_bias_save_point(bias, bias->prn[i], ssr->smode[j])) != -1) {
                bias->cbias[i][pos] = ssr->cbias[ssr->smode[j] - 1];
                bias->smode[i][pos] = ssr->smode[j];
                bias->sflag[i][pos] |= 0x01;
            }
        }
    }
}

static void set_cssr_bank_pbias(clas_ctx_t *ctx, int ch, gtime_t time, int network, int iod)
{
    clas_bank_ctrl_t *bank = ctx->bank[ch];
    clas_bias_bank_t *bias;
    clas_dec_ssr_t *ssr;
    int i, j, pos;

    if ((bias = get_same_bias(bank, time, network)) == NULL) {
        bias = &bank->BiasBank[bank->NextBias];
        bias->network = network;
        bias->bflag = 0;
        bias->time = time;
        bias->use = 1;
        if (++bank->NextBias >= CLAS_BANK_NUM) bank->NextBias = 0;
        memset(bias->smode, 0, sizeof(bias->smode));
        memset(bias->sflag, 0, sizeof(bias->sflag));
        memset(bias->prn, 0, sizeof(bias->prn));
    }
    /* clear previous phase bias entries */
    if (bias->bflag & 0x02) {
        for (i = 0; i < MAXSAT; i++) {
            for (j = 0; j < MAXCODE; j++) {
                if (bias->sflag[i][j] & 0x02) {
                    bias->sflag[i][j] ^= 0x02;
                    bias->pbias[i][j] = 0.0;
                }
            }
        }
        bias->bflag ^= 0x02;
    }
    for (i = 0; i < MAXSAT; i++) {
        ssr = &ctx->dec_ssr[i];
        if (ssr->update_pb != 1 || ssr->nsig <= 0) continue;
        bias->prn[i] = i + 1;
        bias->bflag |= 0x02;
        bias->udi[i] = ssr->udi[5];
        bias->iod[i] = ssr->iod[5];
        for (j = 0; j < MAXCODE; j++) {
            if (ssr->smode[j] != 0 &&
                (pos = get_bias_save_point(bias, bias->prn[i], ssr->smode[j])) != -1) {
                bias->pbias[i][pos] = ssr->pbias[ssr->smode[j] - 1];
                bias->smode[i][pos] = ssr->smode[j];
                bias->sflag[i][pos] |= 0x02;
            }
        }
    }
}

/*============================================================================
 * Latest Trop/STEC Helpers
 *===========================================================================*/

static void set_cssr_latest_trop(clas_bank_ctrl_t *bank, gtime_t time,
                                 ssrn_t *ssrn, int network)
{
    int i, j, sat;

    for (i = 0; i < ssrn->ngp; i++) {
        for (j = 0; j < ssrn->nsat[i]; j++) {
            if (ssrn->stec[i][j] != CSSR_INVALID_VALUE) {
                sat = ssrn->sat[i][j];
                bank->LatestTrop.stec0[network - 1][i][sat - 1] = ssrn->stec0[i][j];
                bank->LatestTrop.stec[network - 1][i][sat - 1] = ssrn->stec[i][j];
                bank->LatestTrop.stectime[network - 1][i][sat - 1] = time;
                bank->LatestTrop.prn[network - 1][i][sat - 1] = sat;
            }
        }
        if (ssrn->trop_total[i] != CSSR_INVALID_VALUE &&
            ssrn->trop_wet[i] != CSSR_INVALID_VALUE) {
            bank->LatestTrop.total[network - 1][i] = ssrn->trop_total[i];
            bank->LatestTrop.wet[network - 1][i] = ssrn->trop_wet[i];
            bank->LatestTrop.troptime[network - 1][i] = time;
        }
    }
    bank->LatestTrop.gridnum[network - 1] = ssrn->ngp;
}

static int get_cssr_latest_trop(clas_bank_ctrl_t *bank, double *total, double *wet,
                                gtime_t time, int network, int index)
{
    if (index < bank->LatestTrop.gridnum[network - 1] &&
        timediff(time, bank->LatestTrop.troptime[network - 1][index]) <= CSSR_TROPVALIDAGE) {
        *total = bank->LatestTrop.total[network - 1][index];
        *wet = bank->LatestTrop.wet[network - 1][index];
        return 1;
    }
    *total = CSSR_INVALID_VALUE;
    *wet = CSSR_INVALID_VALUE;
    return 0;
}

static int get_cssr_latest_iono(clas_bank_ctrl_t *bank, double *iono,
                                gtime_t time, int network, int index, int sat)
{
    if (index < bank->LatestTrop.gridnum[network - 1] &&
        bank->LatestTrop.prn[network - 1][index][sat - 1] == sat &&
        timediff(time, bank->LatestTrop.stectime[network - 1][index][sat - 1]) <= CSSR_STECVALIDAGE) {
        if (bank->LatestTrop.stec[network - 1][index][sat - 1] == CSSR_INVALID_VALUE) {
            *iono = 40.3E16 / (FREQ1 * FREQ2) *
                    bank->LatestTrop.stec0[network - 1][index][sat - 1];
        } else {
            *iono = 40.3E16 / (FREQ1 * FREQ2) *
                    bank->LatestTrop.stec[network - 1][index][sat - 1];
        }
        return 1;
    }
    *iono = CSSR_INVALID_VALUE;
    return 0;
}

/*============================================================================
 * Bank SET — Troposphere/STEC
 *===========================================================================*/

static void set_cssr_bank_trop(clas_ctx_t *ctx, int ch, gtime_t time,
                               ssrn_t *ssrn, int network)
{
    clas_bank_ctrl_t *bank = ctx->bank[ch];
    clas_trop_bank_t *trop;
    int i, j;

    if ((trop = get_same_trop(bank, time)) == NULL) {
        trop = &bank->TropBank[bank->NextTrop];
        trop->time = time;
        trop->use = 1;
        if (++bank->NextTrop >= CLAS_BANK_NUM) bank->NextTrop = 0;
        memset(trop->gridnum, 0, sizeof(trop->gridnum));
        memset(trop->satnum, 0, sizeof(trop->satnum));
    }

    trop->gridnum[network - 1] = ssrn->ngp;
    for (i = 0; i < ssrn->ngp; i++) {
        trop->gridpos[network - 1][i][0] = ssrn->grid[i][0];
        trop->gridpos[network - 1][i][1] = ssrn->grid[i][1];
        trop->gridpos[network - 1][i][2] = ssrn->grid[i][2];
        trop->total[network - 1][i] = ssrn->trop_total[i];
        trop->wet[network - 1][i] = ssrn->trop_wet[i];
        trop->satnum[network - 1][i] = ssrn->nsat[i];
        /* use latest trop if current is invalid */
        if (trop->total[network - 1][i] == CSSR_INVALID_VALUE ||
            trop->wet[network - 1][i] == CSSR_INVALID_VALUE) {
            get_cssr_latest_trop(bank, &trop->total[network - 1][i],
                                 &trop->wet[network - 1][i], time, network, i);
        }
        for (j = 0; j < trop->satnum[network - 1][i]; j++) {
            if (ssrn->stec[i][j] == CSSR_INVALID_VALUE) {
                get_cssr_latest_iono(bank, &trop->iono[network - 1][i][j],
                                     time, network, i, ssrn->sat[i][j]);
            } else {
                trop->iono[network - 1][i][j] = 40.3E16 / (FREQ1 * FREQ2) * ssrn->stec[i][j];
            }
            trop->prn[network - 1][i][j] = ssrn->sat[i][j];
        }
    }
}

/*============================================================================
 * Add Base Value Helpers — merge base-epoch bias with delta corrections
 *
 * NOTE: uses static local storage (thread-safety limitation from upstream)
 *===========================================================================*/

static clas_bias_bank_t *add_base_value_to_cbias(clas_bank_ctrl_t *bank,
                                                  clas_bias_bank_t *srcbias, int network)
{
    gtime_t basetime = timeadd(srcbias->time,
                               fmod(time2gpst(srcbias->time, NULL), 30.0) * -1.0);
    static clas_bias_bank_t cbias;
    clas_bias_bank_t *temp;
    int i, j, pos;

    if ((temp = get_same_bias(bank, basetime, network)) && (temp->bflag & 0x01)) {
        memcpy(&cbias, srcbias, sizeof(clas_bias_bank_t));
        for (i = 0; i < MAXSAT; i++) {
            if (cbias.prn[i] == 0 || temp->prn[i] == 0) continue;
            for (j = 0; j < MAXCODE; j++) {
                if (temp->smode[i][j] == 0 || !(temp->sflag[i][j] & 0x01)) continue;
                if ((pos = get_bias_save_point(&cbias, cbias.prn[i], temp->smode[i][j])) != -1) {
                    if (cbias.cbias[i][pos] != CSSR_INVALID_VALUE &&
                        temp->cbias[i][j] != CSSR_INVALID_VALUE) {
                        cbias.cbias[i][pos] += temp->cbias[i][j];
                    } else {
                        cbias.cbias[i][pos] = CSSR_INVALID_VALUE;
                    }
                    cbias.smode[i][pos] = temp->smode[i][j];
                    cbias.sflag[i][pos] |= 0x01;
                }
            }
        }
        return &cbias;
    }
    return srcbias;
}

static clas_bias_bank_t *add_base_value_to_pbias(clas_bank_ctrl_t *bank,
                                                  clas_bias_bank_t *srcbias, int network)
{
    gtime_t basetime = timeadd(srcbias->time,
                               fmod(time2gpst(srcbias->time, NULL), 30.0) * -1.0);
    static clas_bias_bank_t pbias;
    clas_bias_bank_t *temp;
    int i, j, pos;

    if ((temp = get_same_bias(bank, basetime, network)) && (temp->bflag & 0x02)) {
        memcpy(&pbias, srcbias, sizeof(clas_bias_bank_t));
        for (i = 0; i < MAXSAT; i++) {
            if (pbias.prn[i] == 0 || temp->prn[i] == 0) continue;
            for (j = 0; j < MAXCODE; j++) {
                if (temp->smode[i][j] == 0 || !(temp->sflag[i][j] & 0x02)) continue;
                if ((pos = get_bias_save_point(&pbias, pbias.prn[i], temp->smode[i][j])) != -1) {
                    if (pbias.pbias[i][pos] != CSSR_INVALID_VALUE &&
                        temp->pbias[i][j] != CSSR_INVALID_VALUE) {
                        pbias.pbias[i][pos] += temp->pbias[i][j];
                    } else {
                        pbias.pbias[i][pos] = CSSR_INVALID_VALUE;
                    }
                    pbias.smode[i][pos] = temp->smode[i][j];
                    pbias.sflag[i][pos] |= 0x02;
                }
            }
        }
        return &pbias;
    }
    return srcbias;
}

/*============================================================================
 * Grid/STEC Data Helpers (from upstream grid.c)
 *===========================================================================*/

static void init_grid_index(clas_corr_t *corr)
{
    int i;
    for (i = 0; i < CLAS_MAX_GP; i++) {
        corr->stec[i].data = &corr->stecdata[i][0];
        corr->stec[i].nmax = MAXSAT;
        corr->zwd[i].data = &corr->zwddata[i];
        corr->zwd[i].nmax = 1;
    }
}

static void set_grid_data(clas_corr_t *corr, double *pos, int index)
{
    corr->stec[index].pos[0] = pos[0] * R2D;
    corr->stec[index].pos[1] = pos[1] * R2D;
    corr->stec[index].pos[2] = pos[2];
    corr->stec[index].n = 0;
    corr->zwd[index].pos[0] = (float)(pos[0] * R2D);
    corr->zwd[index].pos[1] = (float)(pos[1] * R2D);
    corr->zwd[index].pos[2] = (float)pos[2];
    corr->zwd[index].n = 0;
}

static int add_data_stec(stec_t *stec, gtime_t time, int sat, int slip,
                         double iono, double rate, double rms, double quality)
{
    if (stec->n >= stec->nmax) return 0;
    stec->data[stec->n].flag = 1;
    stec->data[stec->n].time = time;
    stec->data[stec->n].sat = (unsigned char)sat;
    stec->data[stec->n].slip = (unsigned char)slip;
    stec->data[stec->n].iono = (float)iono;
    stec->data[stec->n].rate = (float)rate;
    stec->data[stec->n].quality = (float)quality;
    stec->data[stec->n++].rms = (float)rms;
    return 1;
}

static int add_data_trop(zwd_t *z, gtime_t time, double zwd, double ztd,
                         double quality, double rms, int valid)
{
    if (z->n >= z->nmax) return 0;
    z->data[z->n].time = time;
    z->data[z->n].valid = (unsigned char)valid;
    z->data[z->n].zwd = (float)zwd;
    z->data[z->n].ztd = (float)ztd;
    z->data[z->n].quality = (float)quality;
    z->data[z->n++].rms = (float)rms;
    return 1;
}

/*============================================================================
 * Bank MERGE — combine closest corrections into clas_corr_t snapshot
 *===========================================================================*/

static int sub_get_close_cssr(clas_bank_ctrl_t *bank, gtime_t time, int network,
                              clas_orbit_bank_t **orbit, clas_clock_bank_t **clock,
                              clas_bias_bank_t **cbias, clas_bias_bank_t **pbias,
                              clas_trop_bank_t **trop, int *flag)
{
    if (!(*orbit = get_close_orbit(bank, time, network, 180.0)) ||
        !(*pbias = get_close_net_pbias(bank, (*orbit)->time, network, 0.0))) {
        trace(NULL,2, "sub_get_close_cssr(): orbit or pbias not found, net=%d\n", network);
        return 0;
    }
    if (!(*trop = get_close_trop(bank, time, (*orbit)->time, network, 30.0,
          bank->fastfix[network] ? 0 : 1))) {
        trace(NULL,2, "sub_get_close_cssr(): trop not found, net=%d\n", network);
        return 0;
    }
    /* re-align pbias to trop if needed */
    if (timediff((*trop)->time, (*pbias)->time) < 0.0 ||
        timediff((*trop)->time, (*pbias)->time) >= 30.0) {
        if (!(*pbias = get_close_net_pbias(bank, (*trop)->time, network, 0.0))) {
            trace(NULL,2, "sub_get_close_cssr(): pbias re-align failed, net=%d\n", network);
            return 0;
        }
    }
    if (!(*cbias = get_close_net_cbias(bank, (*pbias)->time, network, 0.0))) {
        trace(NULL,2, "sub_get_close_cssr(): cbias not found, net=%d\n", network);
        return 0;
    }
    if (!(*clock = get_close_clock(bank, time, (*orbit)->time, network, 30.0))) {
        trace(NULL,2, "sub_get_close_cssr(): clock not found, net=%d\n", network);
        return 0;
    }
    /* temporal consistency checks */
    if (timediff((*pbias)->time, (*cbias)->time) < -30.0 ||
        timediff((*pbias)->time, (*cbias)->time) > 0.0) return 0;
    if (timediff((*trop)->time, (*orbit)->time) < -30.0 ||
        timediff((*trop)->time, (*orbit)->time) >= 30.0) return 0;
    if (timediff((*trop)->time, (*pbias)->time) < 0.0 ||
        timediff((*trop)->time, (*pbias)->time) >= 30.0) return 0;

    *cbias = add_base_value_to_cbias(bank, *cbias, ((*cbias)->network == 0) ? network : 0);
    *pbias = add_base_value_to_pbias(bank, *pbias, ((*pbias)->network == 0) ? network : 0);
    *flag = (timediff((*cbias)->time, (*pbias)->time) == 0.0) ? 1 : 0;
    return 1;
}

extern int clas_bank_get_close(const clas_ctx_t *ctx, gtime_t time,
                               int network, int ch, clas_corr_t *corr)
{
    clas_bank_ctrl_t *bank = ctx->bank[ch];
    clas_orbit_bank_t *orbit;
    clas_clock_bank_t *clk;
    clas_bias_bank_t *cbias, *pbias;
    clas_trop_bank_t *trop;
    int i, j, sis;

    if (!bank->use) return -1;

    if (!sub_get_close_cssr(bank, time, network, &orbit, &clk, &cbias, &pbias, &trop, &sis))
        return -1;

    memset(corr, 0, sizeof(clas_corr_t));
    init_grid_index(corr);

    /* bias corrections */
    for (i = 0; i < MAXSAT; i++) {
        if (cbias->prn[i] != 0) {
            for (j = 0; j < MAXCODE; j++) {
                if (cbias->smode[i][j] != 0 && (cbias->sflag[i][j] & 0x01)) {
                    corr->cbias[i][j] = cbias->cbias[i][j];
                    corr->smode[i][j] = cbias->smode[i][j];
                }
            }
        }
        if (pbias->prn[i] != 0) {
            for (j = 0; j < MAXCODE; j++) {
                if (pbias->smode[i][j] != 0 && (pbias->sflag[i][j] & 0x02)) {
                    corr->pbias[i][j] = pbias->pbias[i][j];
                    corr->smode[i][j] = pbias->smode[i][j];
                }
            }
        }
        corr->time[i][4] = cbias->time;
        corr->prn[i][4] = cbias->prn[i];
        corr->udi[i][4] = cbias->udi[i];
        corr->iod[i][4] = cbias->iod[i];
        corr->time[i][5] = pbias->time;
        corr->prn[i][5] = pbias->prn[i];
        corr->udi[i][5] = pbias->udi[i];
        corr->iod[i][5] = pbias->iod[i];
        corr->flag[i] |= 0x04;
    }
    /* orbit corrections */
    for (i = 0; i < MAXSAT; i++) {
        if (orbit->prn[i] != 0) {
            corr->time[i][0] = orbit->time;
            corr->prn[i][0] = orbit->prn[i];
            corr->udi[i][0] = orbit->udi[i];
            corr->iod[i][0] = orbit->iod[i];
            corr->iode[i] = orbit->iode[i];
            corr->deph0[i] = orbit->deph0[i];
            corr->deph1[i] = orbit->deph1[i];
            corr->deph2[i] = orbit->deph2[i];
            corr->flag[i] |= 0x01;
        }
    }
    /* trop/iono grid corrections */
    corr->gridnum = trop->gridnum[network - 1];
    for (i = 0; i < corr->gridnum; i++) {
        set_grid_data(corr, &trop->gridpos[network - 1][i][0], i);
        for (j = 0; j < trop->satnum[network - 1][i]; j++) {
            if (trop->iono[network - 1][i][j] != CSSR_INVALID_VALUE) {
                add_data_stec(&corr->stec[i], trop->time,
                              trop->prn[network - 1][i][j], 0,
                              trop->iono[network - 1][i][j], 0.0, 0.0, 0.0);
            }
        }
        add_data_trop(&corr->zwd[i], trop->time, trop->wet[network - 1][i],
                      trop->total[network - 1][i], 9999, 0, 1);
    }
    /* clock corrections */
    for (i = 0; i < MAXSAT; i++) {
        if (clk->prn[i] != 0) {
            corr->time[i][1] = clk->time;
            corr->prn[i][1] = clk->prn[i];
            corr->udi[i][1] = clk->udi[i];
            corr->iod[i][1] = clk->iod[i];
            corr->c0[i] = clk->c0[i];
            corr->flag[i] |= 0x02;
        }
    }
    corr->orbit_time = orbit->time;
    corr->clock_time = clk->time;
    corr->bias_time = cbias->time;
    corr->trop_time = trop->time;
    corr->facility = bank->Facility;
    corr->update_time = time;
    corr->network = network;
    corr->use = 1;
    return 0;
}

/*============================================================================
 * Grid Status Check (from upstream cssr.c:check_cssr_grid_status)
 *===========================================================================*/

extern void clas_check_grid_status(clas_ctx_t *ctx, const clas_corr_t *corr,
                                   int ch)
{
    int i, valid, network, nvalid=0;

    if (!corr->use) return;

    network = corr->network;
    if (network <= 0 || network >= CLAS_MAX_NETWORK) return;

    /* orbit + trop corrections must exist */
    if (corr->orbit_time.time == 0 || corr->trop_time.time == 0) {
        for (i = 0; i < CLAS_MAX_GP; i++)
            ctx->grid_stat[ch][network][i] = 0;
        return;
    }

    for (i = 0; i < corr->gridnum && i < CLAS_MAX_GP; i++) {
        /* check trop validity */
        if (!corr->zwddata[i].valid) {
            ctx->grid_stat[ch][network][i] = 0;
            continue;
        }
        /* count valid STEC entries (added by add_data_stec with flag=1) */
        valid = corr->stec[i].n;

        ctx->grid_stat[ch][network][i] = (valid >= 8) ? 1 : 0;
        if (valid >= 8) nvalid++;
    }
    trace(NULL, 4, "grid_status: ch=%d net=%d gridnum=%d nvalid=%d\n",
          ch, network, corr->gridnum, nvalid);
    /* clear remaining grid points */
    for (i = corr->gridnum; i < CLAS_MAX_GP; i++)
        ctx->grid_stat[ch][network][i] = 0;
}

/*============================================================================
 * Update Functions — apply merged corrections to nav_t
 *===========================================================================*/

extern void clas_update_global(nav_t *nav, const clas_corr_t *corr, int ch)
{
    ssr_t *ssr;
    int i, j, sat;

    for (sat = 1; sat <= MAXSAT; sat++) {
        ssr = &nav->ssr_ch[ch][sat - 1];

        /* bias corrections */
        if (corr->prn[sat - 1][4] != 0) {
            for (i = 0; i < MAXCODE; i++) {
                ssr->discnt[i] = 0;
                ssr->pbias[i] = 0.0;
                ssr->cbias[i] = 0.0;
            }
            for (i = 0; i < MAXCODE; i++) {
                if (corr->smode[sat - 1][i] != 0) {
                    j = corr->smode[sat - 1][i] - 1;
                    ssr->cbias[j] = (float)corr->cbias[sat - 1][i];
                    ssr->pbias[j] = corr->pbias[sat - 1][i];
                }
            }
            ssr->udi[4] = corr->udi[sat - 1][4];
            ssr->udi[5] = corr->udi[sat - 1][5];
            ssr->iod[4] = corr->iod[sat - 1][4];
            ssr->iod[5] = corr->iod[sat - 1][5];
            ssr->t0[4] = corr->time[sat - 1][4];
            ssr->t0[5] = corr->time[sat - 1][5];
        } else {
            memset(&ssr->t0[4], 0, sizeof(ssr->t0[4]));
            memset(&ssr->t0[5], 0, sizeof(ssr->t0[5]));
        }

        /* orbit corrections */
        if (corr->prn[sat - 1][0] != 0) {
            ssr->t0[0] = corr->time[sat - 1][0];
            ssr->udi[0] = corr->udi[sat - 1][0];
            ssr->iod[0] = corr->iod[sat - 1][0];
            ssr->iode = corr->iode[sat - 1];
            ssr->deph[0] = corr->deph0[sat - 1];
            ssr->deph[1] = corr->deph1[sat - 1];
            ssr->deph[2] = corr->deph2[sat - 1];
            ssr->ddeph[0] = ssr->ddeph[1] = ssr->ddeph[2] = 0.0;
        } else {
            memset(&ssr->t0[0], 0, sizeof(ssr->t0[0]));
        }

        /* clock corrections */
        if (corr->prn[sat - 1][1] != 0) {
            ssr->t0[1] = corr->time[sat - 1][1];
            ssr->udi[1] = corr->udi[sat - 1][1];
            ssr->iod[1] = corr->iod[sat - 1][1];
            ssr->dclk[0] = corr->c0[sat - 1];
            ssr->dclk[1] = ssr->dclk[2] = 0.0;
            ssr->update = 1;
        } else {
            memset(&ssr->t0[1], 0, sizeof(ssr->t0[1]));
        }
    }
    nav->facility[ch] = corr->facility;
}

extern void clas_update_local(nav_t *nav, const clas_corr_t *corr, int ch)
{
    /* local corrections (stec/zwd) are stored in ctx->current[ch]
     * and accessed directly by the positioning engine.
     * This function is a placeholder for Phase 4 integration. */
    (void)nav; (void)corr; (void)ch;
}

/*============================================================================
 * CSSR Header Decoder
 *===========================================================================*/

static int decode_cssr_head(clas_ctx_t *ctx, clas_l6buf_t *l6, cssr_t *cssr,
                            int *sync, int *tow, int *iod, double *udint,
                            int *ngnss, int i0)
{
    int i = i0, udi;

    if (l6->subtype == CSSR_TYPE_MASK) {
        *tow = getbitu(l6->buff, i, 20); i += 20;
    } else {
        *tow = l6->tow0 + getbitu(l6->buff, i, 12); i += 12;
    }
    udi = getbitu(l6->buff, i, 4); i += 4;
    *sync = getbitu(l6->buff, i, 1); i += 1;
    *udint = ssrudint[udi];
    *iod = getbitu(l6->buff, i, 4); i += 4;

    if (l6->subtype == CSSR_TYPE_MASK) {
        cssr->iod = *iod;
        *ngnss = getbitu(l6->buff, i, 4); i += 4;
    }
    return i;
}

/*============================================================================
 * Bit Width Check Functions — verify buffer has enough bits before decoding
 *===========================================================================*/

static int check_bit_width_mask(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int k, ngnss, cmi, nsat, nsig;
    uint64_t svmask;
    uint16_t sigmask;

    if (i0 + 49 > l6->havebit) return 0;
    ngnss = getbitu(l6->buff, i0 + 45, 4); i0 += 49;
    for (k = 0; k < ngnss; k++) {
        if (i0 + 61 > l6->havebit) return 0;
        cmi = getbitu(l6->buff, i0 + 60, 1); i0 += 61;
        if (cmi) {
            svmask = (uint64_t)getbitu(l6->buff, i0, 8) << 32; i0 += 8;
            svmask |= (uint64_t)getbitu(l6->buff, i0, 32); i0 += 32;
            sigmask = getbitu(l6->buff, i0, 16); i0 += 16;
            nsat = svmask2nsat(svmask);
            nsig = sigmask2nsig(sigmask);
            if (i0 + nsat * nsig > l6->havebit) return 0;
            i0 += nsat * nsig;
        }
    }
    return 1;
}

static int check_bit_width_oc(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int k, sat[CSSR_MAX_SV], nsat, prn;
    i0 += 21;
    if (i0 > l6->havebit) return 0;
    nsat = svmask2sat(cssr->svmask, sat);
    for (k = 0; k < nsat; k++) {
        i0 += (satsys(sat[k], &prn) == SYS_GAL) ? 51 : 49;
        if (i0 > l6->havebit) return 0;
    }
    return 1;
}

static int check_bit_width_cc(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int nsat = svmask2sat(cssr->svmask, NULL);
    return (i0 + 21 + 15 * nsat <= l6->havebit);
}

static int check_bit_width_cb(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int nsig[CSSR_MAX_SV], nsig_total = 0, k, sat[CSSR_MAX_SV], nsat;
    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, NULL);
    for (k = 0; k < nsat; k++) nsig_total += nsig[k];
    return i0 + 21 + nsig_total * 11 <= l6->havebit;
}

static int check_bit_width_pb(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int nsig[CSSR_MAX_SV], nsig_total = 0, k, sat[CSSR_MAX_SV], nsat;
    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, NULL);
    for (k = 0; k < nsat; k++) nsig_total += nsig[k];
    return i0 + 21 + nsig_total * 17 <= l6->havebit;
}

static int check_bit_width_bias(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int j, k, nsat, slen = 0, cbflag, pbflag, netflag, netmask = 0;
    int sat[CSSR_MAX_SV], nsig[CSSR_MAX_SV];
    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, NULL);
    if (i0 + 24 > l6->havebit) return 0;
    i0 += 21;
    cbflag = getbitu(l6->buff, i0, 1); i0++;
    pbflag = getbitu(l6->buff, i0, 1); i0++;
    netflag = getbitu(l6->buff, i0, 1); i0++;
    if (netflag) {
        if (i0 + 5 + nsat > l6->havebit) return 0;
        i0 += 5;
        netmask = getbitu(l6->buff, i0, nsat); i0 += nsat;
    }
    if (cbflag) slen += 11;
    if (pbflag) slen += 17;
    for (k = 0; k < nsat; k++) {
        if (netflag && !((netmask >> (nsat - 1 - k)) & 1)) continue;
        for (j = 0; j < nsig[k]; j++) {
            if (i0 + slen > l6->havebit) return 0;
            i0 += slen;
        }
    }
    return 1;
}

static int check_bit_width_ura(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int nsat = svmask2sat(cssr->svmask, NULL);
    return i0 + 21 + 6 * nsat <= l6->havebit;
}

static int check_bit_width_stec(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int j, sat[CSSR_MAX_SV], nsat, stec_type, nsat_local = 0;
    uint64_t net_svmask;
    static const int slen_t[4] = {20, 44, 54, 70};
    nsat = svmask2sat(cssr->svmask, sat);
    if (i0 + 28 + nsat > l6->havebit) return 0;
    i0 += 21;
    stec_type = getbitu(l6->buff, i0, 2); i0 += 2;
    i0 += 5;
    net_svmask = getbitu(l6->buff, i0, nsat); i0 += nsat;
    for (j = 0; j < nsat; j++) {
        if ((net_svmask >> (nsat - 1 - j)) & 1) nsat_local++;
    }
    return i0 + nsat_local * slen_t[stec_type] <= l6->havebit;
}

static int check_bit_width_grid(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int k, nsat, trop_type, ngp, sz_trop, sz_idx, sz_stec, nsat_local = 0;
    uint64_t net_svmask;
    nsat = svmask2sat(cssr->svmask, NULL);
    if (i0 + 41 + nsat > l6->havebit) return 0;
    i0 += 21;
    trop_type = getbitu(l6->buff, i0, 2); i0 += 2;
    sz_idx = getbitu(l6->buff, i0, 1); i0++;
    i0 += 5;
    net_svmask = getbitu(l6->buff, i0, nsat); i0 += nsat;
    i0 += 6;
    ngp = getbitu(l6->buff, i0, 6); i0 += 6;
    sz_trop = (trop_type == 0) ? 0 : 17;
    sz_stec = (sz_idx == 0) ? 7 : 16;
    for (k = 0; k < nsat; k++) {
        if ((net_svmask >> (nsat - 1 - k)) & 1) nsat_local++;
    }
    return i0 + ngp * (sz_trop + nsat_local * sz_stec) <= l6->havebit;
}

static int check_bit_width_combo(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int sat[CSSR_MAX_SV], nsat, j, flg_orbit, flg_clock, flg_net, sz;
    uint64_t net_svmask = 0;
    nsat = svmask2sat(cssr->svmask, sat);
    if (i0 + 24 > l6->havebit) return 0;
    i0 += 21;
    flg_orbit = getbitu(l6->buff, i0, 1); i0++;
    flg_clock = getbitu(l6->buff, i0, 1); i0++;
    flg_net = getbitu(l6->buff, i0, 1); i0++;
    if (flg_net) {
        if (i0 + 5 + nsat > l6->havebit) return 0;
        i0 += 5;
        net_svmask = getbitu(l6->buff, i0, nsat); i0 += nsat;
    }
    for (j = 0; j < nsat; j++) {
        if (flg_net && !((net_svmask >> (nsat - 1 - j)) & 1)) continue;
        if (flg_orbit) {
            sz = (satsys(sat[j], NULL) == SYS_GAL) ? 10 : 8;
            i0 += sz + 41;
            if (i0 > l6->havebit) return 0;
        }
        if (flg_clock) {
            if ((i0 += 15) > l6->havebit) return 0;
        }
    }
    return 1;
}

static int check_bit_width_atmos(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int trop_avail, stec_avail, trop_type, stec_type, ngp, sz_idx, sz, j, nsat;
    uint64_t net_svmask;
    static const int dstec_sz_t[4] = {4, 4, 5, 7};
    static const int trop_sz_t[3] = {9, 23, 30};
    static const int stec_sz_t[4] = {14, 38, 48, 64};
    nsat = svmask2sat(cssr->svmask, NULL);
    if (i0 + 36 > l6->havebit) return 0;
    i0 += 21;
    trop_avail = getbitu(l6->buff, i0, 2); i0 += 2;
    stec_avail = getbitu(l6->buff, i0, 2); i0 += 2;
    i0 += 5;
    ngp = getbitu(l6->buff, i0, 6); i0 += 6;
    if (trop_avail != 0) {
        if (i0 + 6 > l6->havebit) return 0;
        i0 += 6;
    }
    if ((trop_avail & 0x01) == 0x01) {
        if (i0 + 2 > l6->havebit) return 0;
        trop_type = getbitu(l6->buff, i0, 2); i0 += 2;
        sz = trop_sz_t[trop_type];
        if (i0 + sz > l6->havebit) return 0;
        i0 += sz;
    }
    if ((trop_avail & 0x02) == 0x02) {
        if (i0 + 5 > l6->havebit) return 0;
        sz_idx = getbitu(l6->buff, i0, 1); i0++;
        i0 += 4;
        sz = (sz_idx == 0) ? 6 : 8;
        if (i0 + sz * ngp > l6->havebit) return 0;
        i0 += sz * ngp;
    }
    if (stec_avail != 0) {
        if (i0 + nsat > l6->havebit) return 0;
        net_svmask = getbitu(l6->buff, i0, nsat); i0 += nsat;
        for (j = 0; j < nsat; j++) {
            if (!((net_svmask >> (nsat - 1 - j)) & 1)) continue;
            if (i0 + 6 > l6->havebit) return 0;
            i0 += 6;
            if ((stec_avail & 0x01) == 0x01) {
                if (i0 + 2 > l6->havebit) return 0;
                stec_type = getbitu(l6->buff, i0, 2); i0 += 2;
                sz = stec_sz_t[stec_type];
                if (i0 + sz > l6->havebit) return 0;
                i0 += sz;
            }
            if ((stec_avail & 0x02) == 0x02) {
                if (i0 + 2 > l6->havebit) return 0;
                sz_idx = getbitu(l6->buff, i0, 2); i0 += 2;
                i0 += ngp * dstec_sz_t[sz_idx];
                if (i0 > l6->havebit) return 0;
            }
        }
    }
    return 1;
}

static int check_bit_width_si(clas_l6buf_t *l6, cssr_t *cssr, int i0)
{
    int data_sz;
    if (i0 + 6 > l6->havebit) return 0;
    i0 += 4;
    data_sz = getbitu(l6->buff, i0, 2); i0 += 2;
    return i0 + 40 * (data_sz + 1) <= l6->havebit;
}

/*============================================================================
 * ST1: Mask Message Decoder
 *===========================================================================*/

static int decode_cssr_mask(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, k, l, sync, tow, ngnss, iod, nsat_g, id, nsig, ncell = 0;
    int sat[CSSR_MAX_SV];
    double udint;

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    l6->tow0 = (int)(floor(tow / 3600.0) * 3600.0);
    check_week_ref(ctx, l6, tow, CSSR_REF_MASK);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_MASK], tow);

    for (j = 0; j < CSSR_MAX_GNSS; j++) {
        cssr->cmi[j] = 0;
        cssr->svmask[j] = 0;
        cssr->sigmask[j] = 0;
    }
    for (j = 0; j < CSSR_MAX_SV; j++) cssr->cellmask[j] = 0;

    trace(NULL,2, "decode_cssr_mask: facility=%d tow=%d iod=%d\n",
          ctx->l6facility[ch] + 1, tow, cssr->iod);

    for (k = 0; k < ngnss; k++) {
        id = getbitu(l6->buff, i, 4); i += 4;
        cssr->svmask[id] = (uint64_t)getbitu(l6->buff, i, 8) << 32; i += 8;
        cssr->svmask[id] |= getbitu(l6->buff, i, 32); i += 32;
        cssr->sigmask[id] = getbitu(l6->buff, i, 16); i += 16;
        cssr->cmi[id] = getbitu(l6->buff, i, 1); i++;

        nsig = sigmask2nsig(cssr->sigmask[id]);
        nsat_g = svmask2nsatlist(cssr->svmask[id], id, sat);

        if (cssr->cmi[id]) {
            for (j = 0; j < nsat_g; j++) {
                cssr->cellmask[ncell] = getbitu(l6->buff, i, nsig);
                i += nsig;
                ncell++;
            }
        } else {
            for (j = 0; j < nsat_g; j++) {
                for (l = 0; l < nsig; l++)
                    cssr->cellmask[ncell] |= ((uint16_t)1 << (nsig - 1 - l));
                ncell++;
            }
        }
    }
    cssr->l6delivery = ctx->l6delivery[ch];
    cssr->l6facility = ctx->l6facility[ch];
    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST2: Orbit Correction Decoder
 *===========================================================================*/

static int decode_cssr_oc(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, k, iod, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat, sys, iode;
    double udint;
    clas_dec_ssr_t *ssr;

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    nsat = svmask2sat(cssr->svmask, sat);
    check_week_ref(ctx, l6, tow, CSSR_REF_ORBIT);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_ORBIT], tow);

    trace(NULL,2, "decode_cssr_oc: facility=%d tow=%d iod=%d\n",
          ctx->l6facility[ch] + 1, tow, iod);
    if (cssr->l6facility != ctx->l6facility[ch]) return -1;
    if (cssr->iod != iod) return -1;

    for (j = 0; j < MAXSAT; j++) {
        ssr = &ctx->dec_ssr[j];
        ssr->t0[0].sec = 0.0; ssr->t0[0].time = 0;
        ssr->udi[0] = 0; ssr->iod[0] = 0;
        ssr->update_oc = 0; ssr->update = 0;
        ssr->iode = 0;
        ssr->deph[0] = ssr->deph[1] = ssr->deph[2] = 0.0;
    }

    for (j = 0; j < nsat; j++) {
        ssr = &ctx->dec_ssr[sat[j] - 1];
        sys = satsys(sat[j], NULL);

        iode = (sys == SYS_GAL) ?
               getbitu(l6->buff, i, 10) : getbitu(l6->buff, i, 8);
        i += (sys == SYS_GAL) ? 10 : 8;

        ssr->deph[0] = decode_sval(l6->buff, i, 15, 0.0016); i += 15;
        ssr->deph[1] = decode_sval(l6->buff, i, 13, 0.0064); i += 13;
        ssr->deph[2] = decode_sval(l6->buff, i, 13, 0.0064); i += 13;
        ssr->iode = iode;

        ssr->t0[0] = l6->time;
        ssr->udi[0] = udint;
        ssr->iod[0] = cssr->iod;
        if (ssr->deph[0] == CSSR_INVALID_VALUE || ssr->deph[1] == CSSR_INVALID_VALUE ||
            ssr->deph[2] == CSSR_INVALID_VALUE) {
            ssr->deph[0] = ssr->deph[1] = ssr->deph[2] = CSSR_INVALID_VALUE;
        }
        for (k = 0; k < 3; k++) ssr->ddeph[k] = 0.0;
        ssr->update_oc = 1;
        ssr->update = 1;
    }

    check_cssr_changed_facility(ctx->bank[ch], cssr->l6facility);
    set_cssr_bank_orbit(ctx, ch, l6->time, 0);
    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST3: Clock Correction Decoder
 *===========================================================================*/

static int decode_cssr_cc(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, iod, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat;
    double udint;
    clas_dec_ssr_t *ssr;

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    check_week_ref(ctx, l6, tow, CSSR_REF_CLOCK);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_CLOCK], tow);
    nsat = svmask2sat(cssr->svmask, sat);

    trace(NULL,2, "decode_cssr_cc: facility=%d tow=%d iod=%d\n",
          ctx->l6facility[ch] + 1, tow, iod);
    if (cssr->l6facility != ctx->l6facility[ch]) return -1;
    if (cssr->iod != iod) return -1;

    for (j = 0; j < MAXSAT; j++) {
        ssr = &ctx->dec_ssr[j];
        ssr->t0[1].sec = 0.0; ssr->t0[1].time = 0;
        ssr->udi[1] = 0; ssr->iod[1] = 0;
        ssr->update_cc = 0; ssr->update = 0;
        ssr->dclk[0] = 0.0;
    }

    for (j = 0; j < nsat; j++) {
        ssr = &ctx->dec_ssr[sat[j] - 1];
        ssr->t0[1] = l6->time;
        ssr->udi[1] = udint;
        ssr->iod[1] = cssr->iod;
        ssr->dclk[0] = decode_sval(l6->buff, i, 15, 0.0016); i += 15;
        ssr->dclk[1] = ssr->dclk[2] = 0.0;
        ssr->update_cc = 1;
        ssr->update = 1;
    }

    check_cssr_changed_facility(ctx->bank[ch], cssr->l6facility);
    set_cssr_bank_clock(ctx, ch, l6->time, 0);
    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST4: Code Bias Decoder
 *===========================================================================*/

static int decode_cssr_cb(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, k, iod, s, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat;
    int nsig[CSSR_MAX_SV], sig[CSSR_MAX_SV * CSSR_MAX_SIG];
    double udint;
    clas_dec_ssr_t *ssr;

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    check_week_ref(ctx, l6, tow, CSSR_REF_CBIAS);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_CBIAS], tow);
    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig);

    trace(NULL,2, "decode_cssr_cb: facility=%d tow=%d iod=%d\n",
          ctx->l6facility[ch] + 1, tow, iod);
    if (cssr->l6facility != ctx->l6facility[ch]) return -1;
    if (cssr->iod != iod) return -1;

    for (k = 0; k < MAXSAT; k++) {
        ssr = &ctx->dec_ssr[k];
        ssr->t0[4].sec = 0.0; ssr->t0[4].time = 0;
        ssr->udi[4] = 0; ssr->iod[4] = 0;
        ssr->update_cb = 0; ssr->update = 0; ssr->nsig = 0;
        for (j = 0; j < MAXCODE; j++) {
            ssr->cbias[j] = 0.0;
            ssr->smode[j] = 0;
        }
    }

    for (k = 0; k < nsat; k++) {
        ssr = &ctx->dec_ssr[sat[k] - 1];
        ssr->t0[4] = l6->time;
        ssr->udi[4] = udint;
        ssr->iod[4] = cssr->iod;
        ssr->update_cb = 1;
        ssr->update = 1;
        for (j = 0; j < nsig[k]; j++) {
            s = sig[k * CSSR_MAX_SIG + j];
            ssr->cbias[s - 1] = decode_sval(l6->buff, i, 11, 0.02); i += 11;
            ssr->smode[j] = s;
        }
        ssr->nsig = nsig[k];
    }

    check_cssr_changed_facility(ctx->bank[ch], cssr->l6facility);
    set_cssr_bank_cbias(ctx, ch, l6->time, 0, 0);
    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST5: Phase Bias Decoder
 *===========================================================================*/

static int decode_cssr_pb(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, k, iod, s, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat;
    int nsig[CSSR_MAX_SV], sig[CSSR_MAX_SV * CSSR_MAX_SIG];
    double udint;
    clas_dec_ssr_t *ssr;

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    check_week_ref(ctx, l6, tow, CSSR_REF_PBIAS);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_PBIAS], tow);
    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig);

    trace(NULL,2, "decode_cssr_pb: facility=%d tow=%d iod=%d\n",
          ctx->l6facility[ch] + 1, tow, iod);
    if (cssr->l6facility != ctx->l6facility[ch]) return -1;
    if (cssr->iod != iod) return -1;

    for (k = 0; k < MAXSAT; k++) {
        ssr = &ctx->dec_ssr[k];
        ssr->t0[5].sec = 0.0; ssr->t0[5].time = 0;
        ssr->udi[5] = 0; ssr->iod[5] = 0;
        ssr->update_pb = 0; ssr->update = 0; ssr->nsig = 0;
        for (j = 0; j < MAXCODE; j++) {
            ssr->pbias[j] = 0.0;
            ssr->smode[j] = 0;
        }
    }

    for (k = 0; k < nsat; k++) {
        ssr = &ctx->dec_ssr[sat[k] - 1];
        ssr->t0[5] = l6->time;
        ssr->udi[5] = udint;
        ssr->iod[5] = cssr->iod;
        ssr->update_pb = 1;
        ssr->update = 1;
        for (j = 0; j < nsig[k]; j++) {
            s = sig[k * CSSR_MAX_SIG + j];
            ssr->pbias[s - 1] = decode_sval(l6->buff, i, 15, 0.001); i += 15;
            ssr->discnt[s - 1] = getbitu(l6->buff, i, 2); i += 2;
            ssr->smode[j] = s;
        }
        ssr->nsig = nsig[k];
    }

    check_cssr_changed_facility(ctx->bank[ch], cssr->l6facility);
    set_cssr_bank_pbias(ctx, ch, l6->time, 0, 0);
    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST6: Combined Bias Decoder
 *===========================================================================*/

static int decode_cssr_bias(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, k, iod, s, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat;
    int nsig[CSSR_MAX_SV], sig[CSSR_MAX_SV * CSSR_MAX_SIG];
    int cbflag, pbflag, netflag, network, netmask;
    double udint;
    clas_dec_ssr_t *ssr;

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    check_week_ref(ctx, l6, tow, CSSR_REF_BIAS);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_BIAS], tow);

    cbflag = getbitu(l6->buff, i, 1); i++;
    pbflag = getbitu(l6->buff, i, 1); i++;
    netflag = getbitu(l6->buff, i, 1); i++;
    network = getbitu(l6->buff, i, netflag ? 5 : 0); i += netflag ? 5 : 0;

    nsat = svmask2sat(cssr->svmask, sat);
    sigmask2sig(nsat, sat, cssr->sigmask, cssr->cellmask, nsig, sig);
    netmask = getbitu(l6->buff, i, netflag ? nsat : 0); i += netflag ? nsat : 0;

    trace(NULL,2, "decode_cssr_bias: facility=%d tow=%d iod=%d net=%d\n",
          ctx->l6facility[ch] + 1, tow, iod, network);
    if (cssr->l6facility != ctx->l6facility[ch]) return -1;
    if (cssr->iod != iod) return -1;

    for (k = 0; k < MAXSAT; k++) {
        ssr = &ctx->dec_ssr[k];
        if (cbflag) {
            ssr->t0[4].sec = 0.0; ssr->t0[4].time = 0;
            ssr->udi[4] = 0; ssr->iod[4] = 0;
            ssr->update_cb = 0; ssr->update = 0; ssr->nsig = 0;
        }
        if (pbflag) {
            ssr->t0[5].sec = 0.0; ssr->t0[5].time = 0;
            ssr->udi[5] = 0; ssr->iod[5] = 0;
            ssr->update_pb = 0; ssr->update = 0; ssr->nsig = 0;
        }
        for (j = 0; j < MAXCODE; j++) {
            if (cbflag) { ssr->smode[j] = 0; ssr->cbias[j] = 0.0; }
            if (pbflag) { ssr->smode[j] = 0; ssr->pbias[j] = 0.0; ssr->discnt[j] = 0; }
        }
    }

    for (k = 0; k < nsat; k++) {
        if (netflag && !((netmask >> (nsat - 1 - k)) & 1)) continue;
        ssr = &ctx->dec_ssr[sat[k] - 1];
        if (cbflag) {
            ssr->t0[4] = l6->time; ssr->udi[4] = udint;
            ssr->iod[4] = cssr->iod; ssr->update_cb = 1; ssr->update = 1;
        }
        if (pbflag) {
            ssr->t0[5] = l6->time; ssr->udi[5] = udint;
            ssr->iod[5] = cssr->iod; ssr->update_pb = 1; ssr->update = 1;
        }
        for (j = 0; j < nsig[k]; j++) {
            s = sig[k * CSSR_MAX_SIG + j];
            if (cbflag) {
                ssr->cbias[s - 1] = decode_sval(l6->buff, i, 11, 0.02); i += 11;
            }
            if (pbflag) {
                ssr->pbias[s - 1] = decode_sval(l6->buff, i, 15, 0.001); i += 15;
                ssr->discnt[s - 1] = getbitu(l6->buff, i, 2); i += 2;
            }
            ssr->smode[j] = s;
        }
        ssr->nsig = nsig[k];
    }

    if (cbflag) {
        check_cssr_changed_facility(ctx->bank[ch], cssr->l6facility);
        set_cssr_bank_cbias(ctx, ch, l6->time, netflag ? network : 0, cssr->iod);
    }
    if (pbflag) {
        check_cssr_changed_facility(ctx->bank[ch], cssr->l6facility);
        set_cssr_bank_pbias(ctx, ch, l6->time, netflag ? network : 0, cssr->iod);
    }
    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST7: URA Decoder
 *===========================================================================*/

static int decode_cssr_ura(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, iod, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat;
    double udint;
    clas_dec_ssr_t *ssr;

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    check_week_ref(ctx, l6, tow, CSSR_REF_URA);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_URA], tow);
    nsat = svmask2sat(cssr->svmask, sat);

    trace(NULL,3, "decode_cssr_ura: facility=%d tow=%d iod=%d\n",
          ctx->l6facility[ch] + 1, tow, iod);
    if (cssr->l6facility != ctx->l6facility[ch]) return -1;
    if (cssr->iod != iod) return -1;

    for (j = 0; j < nsat; j++) {
        ssr = &ctx->dec_ssr[sat[j] - 1];
        ssr->t0[3] = l6->time;
        ssr->udi[3] = udint;
        ssr->iod[3] = iod;
        ssr->ura = getbitu(l6->buff, i, 6); i += 6;
        ssr->update_ura = 1;
        ssr->update = 1;
    }

    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST8: STEC Correction Decoder
 *===========================================================================*/

static int decode_cssr_stec(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, k, iod, s, sync, tow, ngnss, sat[CSSR_MAX_SV], nsat, inet, a, b;
    double udint;
    ssrn_t *ssrn;

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    check_week_ref(ctx, l6, tow, CSSR_REF_STEC);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_STEC], tow);
    nsat = svmask2sat(cssr->svmask, sat);
    cssr->opt.stec_type = getbitu(l6->buff, i, 2); i += 2;
    inet = getbitu(l6->buff, i, 5); i += 5;

    trace(NULL,2, "decode_cssr_stec: facility=%d tow=%d iod=%d net=%d type=%d\n",
          ctx->l6facility[ch] + 1, tow, iod, inet, cssr->opt.stec_type);
    if (cssr->l6facility != ctx->l6facility[ch]) return -1;
    if (cssr->iod != iod) return -1;
    if (inet >= CSSR_MAX_NET) return -1;

    ssrn = &cssr->ssrn[inet];
    cssr->net_svmask[inet] = getbitu(l6->buff, i, nsat); i += nsat;

    ssrn->t0[1] = l6->time;
    ssrn->udi[1] = udint;
    ssrn->iod[1] = iod;

    for (j = 0, s = 0; j < nsat; j++) {
        if ((cssr->net_svmask[inet] >> (nsat - 1 - j)) & 1) {
            ssrn->sat_f[s] = sat[j];
            a = getbitu(l6->buff, i, 3); i += 3;
            b = getbitu(l6->buff, i, 3); i += 3;
            ssrn->quality_f[s] = decode_cssr_quality_stec(a, b);
            for (k = 0; k < 4; k++) ssrn->a[s][k] = 0.0;

            ssrn->a[s][0] = decode_sval(l6->buff, i, 14, 0.05); i += 14;
            if (cssr->opt.stec_type > 0) {
                ssrn->a[s][1] = decode_sval(l6->buff, i, 12, 0.02); i += 12;
                ssrn->a[s][2] = decode_sval(l6->buff, i, 12, 0.02); i += 12;
            }
            if (cssr->opt.stec_type > 1) {
                ssrn->a[s][3] = decode_sval(l6->buff, i, 10, 0.02); i += 10;
            }
            s++;
        }
    }
    ssrn->nsat_f = s;
    ssrn->update[1] = 1;

    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST9: Grid Correction Decoder
 *===========================================================================*/

static int decode_cssr_grid(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, k, s, ii, sync, iod, tow, ngnss, sat[CSSR_MAX_SV], nsat, sz;
    int trop_type, sz_idx, inet, a, b, hs, wet, valid;
    double udint, stec0, dlat, dlon, dstec;
    ssrn_t *ssrn;

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    check_week_ref(ctx, l6, tow, CSSR_REF_GRID);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_GRID], tow);
    nsat = svmask2sat(cssr->svmask, sat);

    trop_type = getbitu(l6->buff, i, 2); i += 2;
    sz_idx = getbitu(l6->buff, i, 1); i++;
    inet = getbitu(l6->buff, i, 5); i += 5;

    if (inet >= CSSR_MAX_NET) return -1;
    ssrn = &cssr->ssrn[inet];

    cssr->net_svmask[inet] = getbitu(l6->buff, i, nsat); i += nsat;
    a = getbitu(l6->buff, i, 3); i += 3;
    b = getbitu(l6->buff, i, 3); i += 3;
    ssrn->ngp = getbitu(l6->buff, i, 6); i += 6;
    ssrn->t0[0] = l6->time;
    ssrn->quality = decode_cssr_quality_trop(a, b);

    trace(NULL,2, "decode_cssr_grid: facility=%d tow=%d iod=%d net=%d trop=%d ngp=%d\n",
          ctx->l6facility[ch] + 1, tow, iod, inet, trop_type, ssrn->ngp);
    if (cssr->l6facility != ctx->l6facility[ch]) return -1;
    if (cssr->iod != iod) return -1;

    for (j = 0; j < CSSR_MAX_GP; j++) {
        ssrn->grid[j][0] = ssrn->grid[j][1] = ssrn->grid[j][2] = 0.0;
        ssrn->nsat[j] = 0;
    }

    for (j = 0; j < ssrn->ngp && j < CLAS_MAX_GP; j++) {
        ssrn->grid[j][0] = ctx->grid_pos[inet][j][0] * D2R;
        ssrn->grid[j][1] = ctx->grid_pos[inet][j][1] * D2R;
        ssrn->grid[j][2] = ctx->grid_pos[inet][j][2];
    }
    sz = sz_idx ? 16 : 7;

    for (j = 0; j < ssrn->ngp && j < CLAS_MAX_GP; j++) {
        valid = 1;
        switch (trop_type) {
        case 0: break;
        case 1:
            hs = getbits(l6->buff, i, 9); i += 9;
            wet = getbits(l6->buff, i, 8); i += 8;
            if (hs == (-P2_S9_MAX - 1)) valid = 0;
            if (wet == (-P2_S8_MAX - 1)) valid = 0;
            if (valid) {
                ssrn->trop_wet[j] = wet * 0.004 + 0.252;
                ssrn->trop_total[j] = (hs + wet) * 0.004 + 0.252 + CSSR_TROP_HS_REF;
            } else {
                ssrn->trop_wet[j] = CSSR_INVALID_VALUE;
                ssrn->trop_total[j] = CSSR_INVALID_VALUE;
            }
            break;
        }

        dlat = (ssrn->grid[j][0] - ssrn->grid[0][0]) * R2D;
        dlon = (ssrn->grid[j][1] - ssrn->grid[0][1]) * R2D;

        for (k = 0, s = 0, ii = 0; k < nsat; k++) {
            if ((cssr->net_svmask[inet] >> (nsat - 1 - k)) & 1) {
                dstec = decode_sval(l6->buff, i, sz, 0.04); i += sz;
                stec0 = ssrn->a[ii][0] + ssrn->a[ii][1] * dlat +
                        ssrn->a[ii][2] * dlon + ssrn->a[ii][3] * dlat * dlon;
                if (dstec == CSSR_INVALID_VALUE) {
                    ssrn->stec[j][s] = CSSR_INVALID_VALUE;
                } else {
                    ssrn->stec[j][s] = dstec + stec0;
                }
                ssrn->stec0[j][s] = stec0;
                ssrn->sat[j][s] = sat[k];
                ii++;
                s++;
            }
        }
        ssrn->nsat[j] = s;
    }

    ssrn->update[0] = 1;
    ctx->updateac = 1;

    check_cssr_changed_facility(ctx->bank[ch], cssr->l6facility);
    set_cssr_latest_trop(ctx->bank[ch], ssrn->t0[0], ssrn, inet);
    set_cssr_bank_trop(ctx, ch, ssrn->t0[0], ssrn, inet);

    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST10: Service Information Decoder
 *===========================================================================*/

static int decode_cssr_si(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, sync;

    i = i0;
    sync = getbitu(l6->buff, i, 1); i++;
    cssr->si_cnt = getbitu(l6->buff, i, 3); i += 3;
    cssr->si_sz = getbitu(l6->buff, i, 2); i += 2;

    for (j = 0; j < cssr->si_sz; j++) {
        cssr->si_data[j] = (uint64_t)getbitu(l6->buff, i, 8) << 32; i += 8;
        cssr->si_data[j] |= getbitu(l6->buff, i, 32); i += 32;
    }
    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST11: Combined Orbit/Clock Decoder
 *===========================================================================*/

static int decode_cssr_combo(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, k, sync, iod, tow, ngnss, sat[CSSR_MAX_SV], nsat, iode;
    int flg_orbit, flg_clock, flg_net, net_svmask, netid;
    double udint;
    clas_dec_ssr_t *ssr;

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    check_week_ref(ctx, l6, tow, CSSR_REF_COMBINED);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_COMBINED], tow);
    nsat = svmask2sat(cssr->svmask, sat);

    flg_orbit = getbitu(l6->buff, i, 1); i++;
    flg_clock = getbitu(l6->buff, i, 1); i++;
    flg_net = getbitu(l6->buff, i, 1); i++;
    netid = getbitu(l6->buff, i, flg_net ? 5 : 0); i += flg_net ? 5 : 0;
    net_svmask = getbitu(l6->buff, i, flg_net ? nsat : 0); i += flg_net ? nsat : 0;

    trace(NULL,2, "decode_cssr_combo: facility=%d tow=%d iod=%d net=%d flag=%d %d %d\n",
          ctx->l6facility[ch] + 1, tow, iod, netid, flg_orbit, flg_clock, flg_net);
    if (cssr->l6facility != ctx->l6facility[ch]) return -1;
    if (cssr->iod != iod) return -1;

    for (j = 0; j < MAXSAT; j++) {
        ssr = &ctx->dec_ssr[j];
        if (flg_orbit) {
            ssr->t0[0].sec = 0.0; ssr->t0[0].time = 0;
            ssr->udi[0] = 0; ssr->iod[0] = 0;
            ssr->update_oc = 0; ssr->update = 0; ssr->iode = 0;
            ssr->deph[0] = ssr->deph[1] = ssr->deph[2] = 0.0;
        }
        if (flg_clock) {
            ssr->t0[1].sec = 0.0; ssr->t0[1].time = 0;
            ssr->udi[1] = 0; ssr->iod[1] = 0;
            ssr->update_cc = 0; ssr->update = 0;
            ssr->dclk[0] = 0.0;
        }
    }

    for (j = 0; j < nsat; j++) {
        if (flg_net && !((net_svmask >> (nsat - 1 - j)) & 1)) continue;
        ssr = &ctx->dec_ssr[sat[j] - 1];

        if (flg_orbit) {
            iode = (satsys(sat[j], NULL) == SYS_GAL) ?
                   getbitu(l6->buff, i, 10) : getbitu(l6->buff, i, 8);
            i += (satsys(sat[j], NULL) == SYS_GAL) ? 10 : 8;

            ssr->deph[0] = decode_sval(l6->buff, i, 15, 0.0016); i += 15;
            ssr->deph[1] = decode_sval(l6->buff, i, 13, 0.0064); i += 13;
            ssr->deph[2] = decode_sval(l6->buff, i, 13, 0.0064); i += 13;
            ssr->iode = iode;
            ssr->t0[0] = l6->time; ssr->udi[0] = udint; ssr->iod[0] = cssr->iod;
            for (k = 0; k < 3; k++) ssr->ddeph[k] = 0.0;
            ssr->update_oc = 1; ssr->update = 1;
            if (ssr->deph[0] == CSSR_INVALID_VALUE || ssr->deph[1] == CSSR_INVALID_VALUE ||
                ssr->deph[2] == CSSR_INVALID_VALUE) {
                ssr->deph[0] = ssr->deph[1] = ssr->deph[2] = CSSR_INVALID_VALUE;
            }
        }
        if (flg_clock) {
            ssr->dclk[0] = decode_sval(l6->buff, i, 15, 0.0016); i += 15;
            ssr->dclk[1] = ssr->dclk[2] = 0.0;
            ssr->t0[1] = l6->time; ssr->udi[1] = udint; ssr->iod[1] = cssr->iod;
            ssr->update_cc = 1; ssr->update = 1;
        }
    }

    check_cssr_changed_facility(ctx->bank[ch], cssr->l6facility);
    if (flg_net) ctx->bank[ch]->separation |= (1 << (netid - 1));
    if (flg_orbit) set_cssr_bank_orbit(ctx, ch, l6->time, flg_net ? netid : 0);
    if (flg_clock) set_cssr_bank_clock(ctx, ch, l6->time, flg_net ? netid : 0);

    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * ST12: Atmospheric Correction Decoder
 *===========================================================================*/

static int decode_cssr_atmos(clas_ctx_t *ctx, int ch, clas_l6buf_t *l6, int i0)
{
    cssr_t *cssr = &ctx->cssr[ch];
    int i, j, k, s, sync, tow, iod, ngnss, sat[CSSR_MAX_SV], nsat;
    int trop_avail, stec_avail, trop_type = -1, stec_type = -1;
    int inet, a = 0, b = 0, sz_idx, sz;
    double udint, stec0, ct[6] = {0}, ci[6] = {0};
    double trop_ofst = 0.0, trop_residual, dlat, dlon, dstec;
    ssrn_t *ssrn;
    static const double dstec_lsb_t[4] = {0.04, 0.12, 0.16, 0.24};
    static const int dstec_sz_t[4] = {4, 4, 5, 7};

    i = decode_cssr_head(ctx, l6, cssr, &sync, &tow, &iod, &udint, &ngnss, i0);
    check_week_ref(ctx, l6, tow, CSSR_REF_ATMOSPHERIC);
    l6->time = gpst2time(ctx->week_ref[CSSR_REF_ATMOSPHERIC], tow);
    nsat = svmask2sat(cssr->svmask, sat);

    trop_avail = getbitu(l6->buff, i, 2); i += 2;
    stec_avail = getbitu(l6->buff, i, 2); i += 2;
    inet = getbitu(l6->buff, i, 5); i += 5;

    trace(NULL,2, "decode_cssr_atmos: facility=%d tow=%d iod=%d net=%d\n",
          ctx->l6facility[ch] + 1, tow, iod, inet);
    if (cssr->l6facility != ctx->l6facility[ch]) return -1;
    if (cssr->iod != iod) return -1;
    if (inet >= CSSR_MAX_NET) return -1;

    ssrn = &cssr->ssrn[inet];
    ssrn->ngp = getbitu(l6->buff, i, 6); i += 6;
    ssrn->t0[0] = l6->time;

    for (j = 0; j < CSSR_MAX_GP; j++) {
        ssrn->trop_total[j] = CSSR_INVALID_VALUE;
        ssrn->trop_wet[j] = CSSR_INVALID_VALUE;
        ssrn->grid[j][0] = ssrn->grid[j][1] = ssrn->grid[j][2] = 0.0;
        ssrn->nsat[j] = 0;
    }

    if (trop_avail != 0) {
        a = getbitu(l6->buff, i, 3); i += 3;
        b = getbitu(l6->buff, i, 3); i += 3;
        ssrn->quality = decode_cssr_quality_trop(a, b);
    }

    if ((trop_avail & 0x01) == 0x01) {
        trop_type = getbitu(l6->buff, i, 2); i += 2;
        for (k = 0; k < 4; k++) ct[k] = 0.0;
        ct[0] = decode_sval(l6->buff, i, 9, 0.004); i += 9;
        if (trop_type > 0) {
            ct[1] = decode_sval(l6->buff, i, 7, 0.002); i += 7;
            ct[2] = decode_sval(l6->buff, i, 7, 0.002); i += 7;
        }
        if (trop_type > 1) {
            ct[3] = decode_sval(l6->buff, i, 7, 0.001); i += 7;
        }
    }

    for (j = 0; j < ssrn->ngp && j < CLAS_MAX_GP; j++) {
        ssrn->grid[j][0] = ctx->grid_pos[inet][j][0] * D2R;
        ssrn->grid[j][1] = ctx->grid_pos[inet][j][1] * D2R;
        ssrn->grid[j][2] = ctx->grid_pos[inet][j][2];

        dlat = (ssrn->grid[j][0] - ssrn->grid[0][0]) * R2D;
        dlon = (ssrn->grid[j][1] - ssrn->grid[0][1]) * R2D;

        ssrn->trop_total[j] = CSSR_TROP_HS_REF + ct[0];
        if (trop_type > 0) ssrn->trop_total[j] += ct[1] * dlat + ct[2] * dlon;
        if (trop_type > 1) ssrn->trop_total[j] += ct[3] * dlat * dlon;
    }

    if ((trop_avail & 0x02) == 0x02) {
        int sz_idx_t = getbitu(l6->buff, i, 1); i++;
        trop_ofst = getbitu(l6->buff, i, 4) * 0.02; i += 4;
        sz = (sz_idx_t == 0) ? 6 : 8;

        for (j = 0; j < ssrn->ngp && j < CLAS_MAX_GP; j++) {
            trop_residual = decode_sval(l6->buff, i, sz, 0.004); i += sz;
            if (trop_residual != CSSR_INVALID_VALUE) {
                ssrn->trop_wet[j] = trop_residual + trop_ofst;
                ssrn->trop_total[j] += ssrn->trop_wet[j];
            } else {
                ssrn->trop_total[j] = CSSR_INVALID_VALUE;
            }
        }
    }

    if (stec_avail != 0) {
        cssr->net_svmask[inet] = getbitu(l6->buff, i, nsat); i += nsat;
    }
    for (j = s = 0; j < nsat; j++) {
        if (stec_avail != 0) {
            if (!((cssr->net_svmask[inet] >> (nsat - 1 - j)) & 1)) continue;
            a = getbitu(l6->buff, i, 3); i += 3;
            b = getbitu(l6->buff, i, 3); i += 3;
            (void)decode_cssr_quality_stec(a, b);
        }
        for (k = 0; k < 6; k++) ci[k] = 0.0;
        if ((stec_avail & 0x01) == 0x01) {
            stec_type = getbitu(l6->buff, i, 2); i += 2;
            ci[0] = decode_sval(l6->buff, i, 14, 0.05); i += 14;
            if (stec_type > 0) {
                ci[1] = decode_sval(l6->buff, i, 12, 0.02); i += 12;
                ci[2] = decode_sval(l6->buff, i, 12, 0.02); i += 12;
            }
            if (stec_type > 1) {
                ci[3] = decode_sval(l6->buff, i, 10, 0.02); i += 10;
            }
            if (stec_type > 2) {
                ci[4] = decode_sval(l6->buff, i, 8, 0.005); i += 8;
                ci[5] = decode_sval(l6->buff, i, 8, 0.005); i += 8;
            }
        }
        if (stec_avail != 0) {
            if ((stec_avail & 0x02) == 0x02) {
                sz_idx = getbitu(l6->buff, i, 2); i += 2;
            } else {
                sz_idx = 0;
            }

            for (k = 0; k < ssrn->ngp && k < CLAS_MAX_GP; k++) {
                dlat = (ssrn->grid[k][0] - ssrn->grid[0][0]) * R2D;
                dlon = (ssrn->grid[k][1] - ssrn->grid[0][1]) * R2D;

                dstec = 0.0;
                if ((stec_avail & 0x02) == 0x02) {
                    dstec = decode_sval(l6->buff, i, dstec_sz_t[sz_idx],
                                        dstec_lsb_t[sz_idx]);
                    i += dstec_sz_t[sz_idx];
                }

                stec0 = ci[0];
                if (stec_type > 0) stec0 += ci[1] * dlat + ci[2] * dlon;
                if (stec_type > 1) stec0 += ci[3] * dlat * dlon;
                if (stec_type > 2) stec0 += ci[4] * dlat * dlat + ci[5] * dlon * dlon;

                if (dstec == CSSR_INVALID_VALUE) {
                    ssrn->stec[k][s] = CSSR_INVALID_VALUE;
                } else {
                    ssrn->stec[k][s] = stec0 + dstec;
                }
                ssrn->stec0[k][s] = stec0;
                ssrn->sat[k][s] = sat[j];
                ssrn->nsat[k]++;
            }
            s++;
        }
    }

    ssrn->update[0] = 1;
    ctx->updateac = 1;

    check_cssr_changed_facility(ctx->bank[ch], cssr->l6facility);
    set_cssr_latest_trop(ctx->bank[ch], ssrn->t0[0], ssrn, inet);
    set_cssr_bank_trop(ctx, ch, ssrn->t0[0], ssrn, inet);

    l6->nbit = i;
    return sync ? 0 : 10;
}

/*============================================================================
 * Dispatch — decode QZS L6 CLAS stream
 *===========================================================================*/

static int decode_qzs_msg(clas_ctx_t *ctx, int ch)
{
    cssr_t *cssr = &ctx->cssr[ch];
    clas_l6buf_t *l6 = &ctx->l6buf[ch];
    int startbit, i, ret = 0;

    if (l6->frame == 0x00 || l6->nbit == -1) return 0;

    i = startbit = l6->nbit;
    if ((i + 16) > l6->havebit) return 0;

    l6->ctype = getbitu(l6->buff, i, 12); i += 12;
    if (l6->ctype != 4073) {
        l6->nbit = -1;
        l6->frame = 0;
        return 0;
    }
    l6->subtype = getbitu(l6->buff, i, 4); i += 4;

    if (l6->subtype != CSSR_TYPE_MASK && !l6->decode_start) return 0;

    /* bit width check */
    switch (l6->subtype) {
    case CSSR_TYPE_MASK:  if (!check_bit_width_mask(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_OC:    if (!check_bit_width_oc(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_CC:    if (!check_bit_width_cc(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_CB:    if (!check_bit_width_cb(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_PB:    if (!check_bit_width_pb(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_BIAS:  if (!check_bit_width_bias(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_URA:   if (!check_bit_width_ura(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_STEC:  if (!check_bit_width_stec(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_GRID:  if (!check_bit_width_grid(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_COMBO: if (!check_bit_width_combo(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_ATMOS: if (!check_bit_width_atmos(l6, cssr, i)) return 0; break;
    case CSSR_TYPE_SI:    if (!check_bit_width_si(l6, cssr, i)) return 0; break;
    case 0: return 0;
    }

    /* decode */
    switch (l6->subtype) {
    case CSSR_TYPE_MASK:
        ret = decode_cssr_mask(ctx, ch, l6, i);
        if (!l6->decode_start) {
            trace(NULL,1, "start CSSR decoding: ch=%d\n", ch);
            l6->decode_start = 1;
        }
        break;
    case CSSR_TYPE_OC:
        ret = decode_cssr_oc(ctx, ch, l6, i);
        break;
    case CSSR_TYPE_CC:
        ret = decode_cssr_cc(ctx, ch, l6, i);
        break;
    case CSSR_TYPE_CB:
        ret = decode_cssr_cb(ctx, ch, l6, i);
        break;
    case CSSR_TYPE_PB:
        ret = decode_cssr_pb(ctx, ch, l6, i);
        break;
    case CSSR_TYPE_BIAS:
        ret = decode_cssr_bias(ctx, ch, l6, i);
        break;
    case CSSR_TYPE_URA:
        ret = decode_cssr_ura(ctx, ch, l6, i);
        break;
    case CSSR_TYPE_STEC:
        ret = decode_cssr_stec(ctx, ch, l6, i);
        break;
    case CSSR_TYPE_GRID:
        ret = decode_cssr_grid(ctx, ch, l6, i);
        break;
    case CSSR_TYPE_COMBO:
        ret = decode_cssr_combo(ctx, ch, l6, i);
        break;
    case CSSR_TYPE_ATMOS:
        ret = decode_cssr_atmos(ctx, ch, l6, i);
        break;
    case CSSR_TYPE_SI:
        ret = decode_cssr_si(ctx, ch, l6, i);
        break;
    default: break;
    }

    /* on facility/IOD mismatch, invalidate frame */
    if (ret == -1) {
        l6->nbit = -1;
        cssr->iod = -1;
        l6->frame = 0;
        return 0;
    }
    return ret;
}

/*============================================================================
 * L6 Frame Assembly — assemble 5 subframes into bit buffer
 *===========================================================================*/

static void read_qzs_msg_l6(clas_l6buf_t *l6, uint8_t *pbuff)
{
    int i = 0, j, k, jn, jn0 = -1;
    uint8_t *buff;

    for (j = 0; j < 5; j++) {
        buff = pbuff + j * BLEN_MSG;
        if (buff[5] & 0x01) { /* subframe indicator (head) */
            jn0 = j;
            break;
        }
    }
    if (jn0 < 0) {
        memset(l6->buff, 0, sizeof(l6->buff));
        return;
    }

    for (j = 0, jn = jn0; j < 5; j++, jn++) {
        if (jn >= 5) jn = 0;
        buff = pbuff + jn * BLEN_MSG;
        setbitu(l6->buff, i, 7, buff[6] & 0x7f); i += 7;
        for (k = 0; k < 211; k++) {
            setbitu(l6->buff, i, 8, buff[7 + k]); i += 8;
        }
    }
}

/*============================================================================
 * I/O Functions — byte-by-byte and file-based input
 *===========================================================================*/

extern int clas_get_correct_fac(int msgid)
{
    int fac = (msgid & 0x18) >> 3;
    int ptn = (msgid & 0x06) >> 1;

    if (ptn == 0) {
        switch (fac) {
        case 0: return 0; case 1: return 2;
        case 2: return 1; case 3: return 3;
        default: return -1;
        }
    } else {
        switch (fac) {
        case 0: return 2; case 1: return 0;
        case 2: return 3; case 3: return 1;
        default: return -1;
        }
    }
}

extern int clas_input_cssr(clas_ctx_t *ctx, uint8_t data, int ch)
{
    clas_l6buf_t *l6 = &ctx->l6buf[ch];
    uint8_t prn, msgid, alert;

    trace(NULL,5, "clas_input_cssr: data=%02x ch=%d\n", data, ch);

    /* synchronize L6 frame preamble */
    if (l6->nbyte == 0) {
        l6->preamble = (l6->preamble << 8) | data;
        if (l6->preamble != L6FRMPREAMB) return 0;
        l6->preamble = 0;
        l6->fbuff[l6->nbyte++] = (L6FRMPREAMB >> 24) & 0xff;
        l6->fbuff[l6->nbyte++] = (L6FRMPREAMB >> 16) & 0xff;
        l6->fbuff[l6->nbyte++] = (L6FRMPREAMB >> 8) & 0xff;
        l6->fbuff[l6->nbyte++] = data;
        return 0;
    }
    l6->fbuff[l6->nbyte++] = data;
    l6->len = BLEN_MSG;

    if (l6->nbyte < l6->len) return 0;
    l6->nbyte = 0;

    prn = l6->fbuff[4];
    msgid = l6->fbuff[5];
    alert = (l6->fbuff[6] >> 7) & 0x1;
    if (alert != 0) {
        trace(NULL,1, "CSSR frame alert!: ch=%d\n", ch);
    }

    ctx->l6delivery[ch] = prn;
    ctx->l6facility[ch] = clas_get_correct_fac(msgid);

    if (msgid & 0x01) { /* subframe indicator */
        l6->havebit = 0;
        l6->decode_start = 1;
        l6->nbit = 0;
        l6->frame = 0;
        l6->nframe = 0;
    } else if (l6->nframe >= 5) {
        return 0;
    }

    if (l6->decode_start) {
        int ii = 1695 * l6->nframe, jj;
        setbitu(l6->buff, ii, 7, l6->fbuff[6] & 0x7f); ii += 7;
        for (jj = 0; jj < 211; jj++) {
            setbitu(l6->buff, ii, 8, l6->fbuff[7 + jj]); ii += 8;
        }
        l6->havebit += 1695;
        l6->frame |= (1 << l6->nframe);
        l6->nframe++;
    }
    return 0;
}

extern int clas_input_cssrf(clas_ctx_t *ctx, FILE *fp, int ch)
{
    int i, data, ret;

    for (i = 0; i < 4096; i++) {
        if ((ret = decode_qzs_msg(ctx, ch))) return ret;
        if ((data = fgetc(fp)) == EOF) return -2;
        if ((ret = clas_input_cssr(ctx, (uint8_t)data, ch))) return ret;
    }
    return 0;
}

/*============================================================================
 * Grid Definition File Reader
 *===========================================================================*/

extern int clas_read_grid_def(clas_ctx_t *ctx, const char *gridfile)
{
    int no, lath, latm, lonh, lonm;
    double lat, lon, alt;
    char buff[1024], *temp, *p;
    int inet, grid[CLAS_MAX_NETWORK], isqzss = 0, ret;
    FILE *fp;

    memset(grid, 0, sizeof(grid));

    for (inet = 0; inet < CLAS_MAX_NETWORK; inet++) {
        ctx->grid_pos[inet][0][0] = -1.0;
        ctx->grid_pos[inet][0][1] = -1.0;
        ctx->grid_pos[inet][0][2] = -1.0;
    }

    trace(NULL,2, "clas_read_grid_def: gridfile=%s\n", gridfile);
    fp = fopen(gridfile, "r");
    if (!fp) return -1;

    while (fgets(buff, sizeof(buff), fp)) {
        if (strstr(buff, "<CSSR Grid Definition>")) {
            while (fgets(buff, sizeof(buff), fp)) {
                if ((temp = strstr(buff, "<Version>"))) {
                    p = temp + 9;
                    if ((temp = strstr(buff, "</Version>"))) *temp = '\0';
                    ctx->gridsel = atoi(p);
                    break;
                }
            }
            break;
        } else if (strstr(buff, "Compact Network ID    GRID No.")) {
            ctx->gridsel = 3;
            isqzss = 1;
            break;
        } else {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);

    fp = fopen(gridfile, "r");
    if (!fp) return -1;

    if (isqzss == 0) {
        while (fgets(buff, sizeof(buff), fp)) {
            if (sscanf(buff, "<Network%d>", &inet)) {
                while (fscanf(fp, "%d\t%d\t%d\t%lf\t%d\t%d\t%lf\t%lf",
                              &no, &lath, &latm, &lat, &lonh, &lonm, &lon, &alt) > 0) {
                    if (inet >= 0 && inet < CLAS_MAX_NETWORK && grid[inet] < CLAS_MAX_GP) {
                        ctx->grid_pos[inet][grid[inet]][0] =
                            (double)lath + (double)latm / 60.0 + lat / 3600.0;
                        ctx->grid_pos[inet][grid[inet]][1] =
                            (double)lonh + (double)lonm / 60.0 + lon / 3600.0;
                        ctx->grid_pos[inet][grid[inet]][2] = alt;
                        grid[inet]++;
                        if (grid[inet] < CLAS_MAX_GP) {
                            ctx->grid_pos[inet][grid[inet]][0] = -1.0;
                            ctx->grid_pos[inet][grid[inet]][1] = -1.0;
                            ctx->grid_pos[inet][grid[inet]][2] = -1.0;
                        }
                    }
                }
            }
        }
    } else {
        if (!fgets(buff, sizeof(buff), fp)) { fclose(fp); return -1; }
        while ((ret = fscanf(fp, "%d %d %lf %lf %lf", &inet, &no, &lat, &lon, &alt)) != EOF) {
            if (inet >= 0 && inet < CLAS_MAX_NETWORK && ret == 5 && grid[inet] < CLAS_MAX_GP) {
                ctx->grid_pos[inet][grid[inet]][0] = lat;
                ctx->grid_pos[inet][grid[inet]][1] = lon;
                ctx->grid_pos[inet][grid[inet]][2] = alt;
                grid[inet]++;
                if (grid[inet] < CLAS_MAX_GP) {
                    ctx->grid_pos[inet][grid[inet]][0] = -1.0;
                    ctx->grid_pos[inet][grid[inet]][1] = -1.0;
                    ctx->grid_pos[inet][grid[inet]][2] = -1.0;
                }
            }
        }
    }
    fclose(fp);
    return 0;
}
