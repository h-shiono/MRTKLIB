/*------------------------------------------------------------------------------
 * mrtk_clas_grid.c : CLAS grid interpolation for troposphere and STEC
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
 * @file mrtk_clas_grid.c
 * @brief CLAS grid interpolation for troposphere and STEC.
 *
 * References:
 *   [1] IS-QZSS-L6-006, QZSS L6 Signal CLAS IF Specification
 *
 * History:
 *   2026/03/02  1.0  ported from claslib grid.c / stec.c / rtkcmn.c
 *                    - singleton globals replaced by clas_ctx_t context
 *                    - nav_t dependency replaced by clas_corr_t
 *                    - MOPS standard atmosphere helpers embedded as static
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_clas.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_geoid.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_trace.h"
#include "mrtklib/mrtk_station.h"

/*============================================================================
 * Local Constants
 *===========================================================================*/
#define MAX_GRID_CACHE   20         /* candidate grids sorted by distance */
#define MAX_DIST         120.0      /* max distance to grid (km) */
#define MAXAGESSR_TROP   120.0      /* max age of trop correction (s) */
#define MAXAGESSR_IONO   120.0      /* max age of iono correction (s) */

#define SQR(x)           ((x)*(x))
#define MIN(x,y)         ((x)<(y)?(x):(y))
#define MAX(x,y)         ((x)>(y)?(x):(y))

/*============================================================================
 * Local Types
 *===========================================================================*/

/** Grid candidate info for sorting by distance */
typedef struct {
    double pos[3];      /* grid position {lat,lon,hgt} (deg) */
    double weight;      /* distance metric (m) */
    int    network;     /* network ID (1-based) */
    int    index;       /* grid point index */
} grid_info;

/*============================================================================
 * MOPS Standard Atmosphere Helpers (from rtkcmn.c)
 *
 * Used by trop_data() to normalize decoded ZTD/ZWD values against
 * the MOPS standard atmosphere model.
 *===========================================================================*/

/* linear interpolation ------------------------------------------------------*/
static double Interp1(const double *x, const double *y, int nx, int ny,
                      double xi)
{
    int i;
    double dx;

    for (i = 0; i < nx - 1; i++) if (xi < x[i + 1]) break;
    dx = (xi - x[i]) / (x[i + 1] - x[i]);

    return y[i] * (1.0 - dx) + y[i + 1] * dx;
}

/* get MOPS meteorological parameters ----------------------------------------*/
static int get_mops(const double lat, const double doy, double *pre,
                    double *temp, double *wpre, double *ltemp, double *wvap)
{
    int i;
    double cos_, latdeg = lat * R2D; /* rad->deg */
    const double dminN = 28;         /* 28@north hemisphere */
    const double interval[] = {15.0, 30.0, 45.0, 60.0, 75.0};

    /* meteorological parameter average table */
    const double pTave[]  = {1013.25, 1017.25, 1015.75, 1011.75, 1013.00};
    const double tTave[]  = {299.65,  294.15,  283.15,  272.15,  263.65};
    const double wTave[]  = {26.31,   21.79,   11.66,   6.78,    4.11};
    const double tlTave[] = {6.30e-3, 6.05e-3, 5.58e-3, 5.39e-3, 4.53e-3};
    const double wlTave[] = {2.77,    3.15,    2.57,    1.81,    1.55};

    /* meteorological parameter seasonal variation table */
    const double pTableS[]  = {0.00,   -3.75,  -2.25,  -1.75,  -0.50};
    const double tTableS[]  = {0.00,    7.00,  11.00,  15.00,  14.50};
    const double wTableS[]  = {0.00,    8.85,   7.24,   5.36,   3.39};
    const double tlTableS[] = {0.00e-3, 0.25e-3, 0.32e-3, 0.81e-3, 0.62e-3};
    const double wlTableS[] = {0.00,    0.33,   0.46,   0.74,   0.30};

    double calcPsta[5], calcTsta[5], calcWsta[5], calcTLsta[5], calcWLsta[5];

    for (i = 0; i < 5; i++) {
        cos_ = cos(2.0 * PI * (doy - dminN) / 365.25);
        calcPsta[i]  = pTave[i]  - pTableS[i]  * cos_;
        calcTsta[i]  = tTave[i]  - tTableS[i]  * cos_;
        calcWsta[i]  = wTave[i]  - wTableS[i]  * cos_;
        calcTLsta[i] = tlTave[i] - tlTableS[i] * cos_;
        calcWLsta[i] = wlTave[i] - wlTableS[i] * cos_;
    }

    if (75.0 <= latdeg) {
        *pre   = calcPsta[4];
        *temp  = calcTsta[4];
        *wpre  = calcWsta[4];
        *ltemp = calcTLsta[4];
        *wvap  = calcWLsta[4];
    } else if (latdeg <= 15.0) {
        *pre   = calcPsta[0];
        *temp  = calcTsta[0];
        *wpre  = calcWsta[0];
        *ltemp = calcTLsta[0];
        *wvap  = calcWLsta[0];
    } else if (15.0 < latdeg && latdeg < 75.0) {
        *pre   = Interp1(interval, calcPsta,  5, 5, latdeg);
        *temp  = Interp1(interval, calcTsta,  5, 5, latdeg);
        *wpre  = Interp1(interval, calcWsta,  5, 5, latdeg);
        *ltemp = Interp1(interval, calcTLsta, 5, 5, latdeg);
        *wvap  = Interp1(interval, calcWLsta, 5, 5, latdeg);
    } else {
        return 1;
    }
    return 0;
}

/* dry zenith delay scaling factor -------------------------------------------*/
static double get_Tdv(const double lat, const double Hs, const double Ps)
{
    return 2.2768 / (1.0 - 0.00266 * cos(2.0 * lat) - (2.8e-7) * Hs)
           * Ps * 0.001;
}

/* wet zenith delay scaling factor -------------------------------------------*/
static double get_Twv(const double Ts, const double es)
{
    return 2.2768 * (1255.0 / Ts + 0.05) * es * 0.001;
}

/* standard atmosphere dry/wet zenith delay ----------------------------------
 *
 *  args:  gtime_t t     I  time (GPST)
 *         double  lat   I  latitude (rad)
 *         double  hs    I  ellipsoidal height (m)
 *         double  hg    I  geoid height (m)
 *         double *tdv   O  dry zenith delay scaling factor
 *         double *twv   O  wet zenith delay scaling factor
 *  return: 0=ok, 1=error
 *---------------------------------------------------------------------------*/
/* non-static: called from mrtk_clas_osr.c via extern declaration */
int get_stTv(gtime_t t, const double lat, const double hs,
             const double hg, double *tdv, double *twv)
{
    double P0, T0, e0, beta0, lambda0, Hs, Ts, Ps, es, t0, tk, tc, ET, doy;
    const double g = 9.80665, Rd = 287.0537625; /* J/(kg.K) */

    doy = time2doy(t);

    if (get_mops(lat, doy, &P0, &T0, &e0, &beta0, &lambda0)) return 1;

    Hs = hs - hg; /* elevation = ellipsoidal height - geoid height */

    Ts = T0 - beta0 * Hs;
    Ps = P0 * pow(1 - beta0 * Hs / T0, g / (Rd * beta0));
    es = e0 * pow(1 - beta0 * Hs / T0, (lambda0 + 1.0) * g / (Rd * beta0));

    t0 = 273.15; tk = Ts; tc = tk - t0;
    ET = 6.11 * pow(tk / t0, -5.3) * exp(25.2 * tc / tk);

    if (es > ET) es = ET;

    *tdv = get_Tdv(lat, Hs + hg, Ps); /* dry */
    *twv = get_Twv(Ts, es);           /* wet */

    return 0;
}

/*============================================================================
 * Grid Candidate Search and Selection (from grid.c)
 *===========================================================================*/

/* pickup candidate grids within MAX_DIST of receiver position ---------------*/
static int pickup_candidates_of_grid(const clas_ctx_t *ctx,
                                     grid_info *gridinfo,
                                     const int network,
                                     const double *pos, int ch)
{
    int start = (network > 0 ? network : 1);
    int end   = (network > 0 ? network + 1 : CLAS_MAX_NETWORK);
    int inet, i, j, k, n = 0;
    double d, dd[2];

    for (inet = start; inet < end; ++inet) {
        for (i = 0; i < CLAS_MAX_GP; ++i) {
            /* sentinel check: {-1,-1,-1} marks end of grid */
            if (fabs(-1.0 - ctx->grid_pos[inet][i][0]) < 0.0001 &&
                fabs(-1.0 - ctx->grid_pos[inet][i][1]) < 0.0001 &&
                fabs(-1.0 - ctx->grid_pos[inet][i][2]) < 0.0001) {
                break;
            }
            /* skip grids with non-zero height or invalid status */
            if (fabs(ctx->grid_pos[inet][i][2]) > 0.0001 ||
                ctx->grid_stat[ch][inet][i] == 0) {
                continue;
            }

            /* distance to grid (m) */
            dd[0] = RE_WGS84 * (pos[0] - ctx->grid_pos[inet][i][0] * D2R);
            dd[1] = RE_WGS84 * (pos[1] - ctx->grid_pos[inet][i][1] * D2R)
                    * cos(pos[0]);
            if ((d = MAX(norm(dd, 2), 1.0)) > MAX_DIST * 1000.0) {
                continue;
            }

            trace(NULL, 5, "get_grid_index: inet=%2d, pos=%3.3f %3.3f, "
                  "dist=%6.3f[km]\n", inet, ctx->grid_pos[inet][i][0],
                  ctx->grid_pos[inet][i][1], d / 1000.0);

            /* insertion sort into gridinfo[] by distance */
            if (n <= 0) {
                gridinfo[n].pos[0]  = ctx->grid_pos[inet][i][0];
                gridinfo[n].pos[1]  = ctx->grid_pos[inet][i][1];
                gridinfo[n].pos[2]  = ctx->grid_pos[inet][i][2];
                gridinfo[n].network = inet;
                gridinfo[n].weight  = d;
                gridinfo[n].index   = i;
                ++n;
            } else {
                for (j = 0; j < n; j++) if (d < gridinfo[j].weight) break;
                if (j >= MAX_GRID_CACHE) continue;
                for (k = MIN(n, MAX_GRID_CACHE - 1); k > j; k--) {
                    gridinfo[k] = gridinfo[k - 1];
                }
                gridinfo[j].pos[0]  = ctx->grid_pos[inet][i][0];
                gridinfo[j].pos[1]  = ctx->grid_pos[inet][i][1];
                gridinfo[j].pos[2]  = ctx->grid_pos[inet][i][2];
                gridinfo[j].network = inet;
                gridinfo[j].weight  = d;
                gridinfo[j].index   = i;
                if (n < MAX_GRID_CACHE) n++;
            }
        }
    }
    return n;
}

/* find 3 or 4 grid points surrounding receiver position ---------------------*/
static int find_grid_surround_pos(grid_info **gridindex, int n,
                                  grid_info *gridinfo)
{
    int first_index;
    double dd12[2], dd13[2], dd14[2];
    grid_info *grid2 = NULL, *grid3 = NULL;
    int flag, i;

    first_index = (gridinfo[1].network == gridinfo[2].network &&
                   gridinfo[0].network != gridinfo[1].network) ? 1 : 0;

    /* find 2nd grid in same network */
    for (i = 1, gridindex[0] = &gridinfo[first_index], flag = 0;
         i < n; ++i) {
        if (gridinfo[first_index].network == gridinfo[i].network &&
            first_index != i) {
            dd12[0] = gridinfo[i].pos[0] * D2R
                    - gridinfo[first_index].pos[0] * D2R;
            dd12[1] = gridinfo[i].pos[1] * D2R
                    - gridinfo[first_index].pos[1] * D2R;
            gridindex[1] = &gridinfo[i];
            grid2 = &gridinfo[i];
            flag = 1;
            break;
        }
    }
    if (!flag) {
        trace(NULL, 2, "can't find second grid: network=%d\n",
              gridinfo[first_index].network);
        return 1;
    }

    /* find 3rd grid: non-collinear with dd12 */
    for (i = 1, flag = 0; i < n; i++) {
        if (gridindex[1]->index == gridinfo[i].index ||
            gridinfo[first_index].network != gridinfo[i].network ||
            first_index == i) {
            continue;
        }
        dd13[0] = gridinfo[i].pos[0] * D2R
                - gridinfo[first_index].pos[0] * D2R;
        dd13[1] = gridinfo[i].pos[1] * D2R
                - gridinfo[first_index].pos[1] * D2R;
        if (fabs(dot(dd12, dd13, 2) / (norm(dd12, 2) * norm(dd13, 2)))
            < 1.0) {
            gridindex[2] = &gridinfo[i];
            grid3 = &gridinfo[i];
            flag = 1;
            break;
        }
    }
    if (!flag) {
        trace(NULL, 2, "can't find third grid: network=%d\n",
              gridinfo[first_index].network);
        return 1;
    }

    /* find 4th grid: non-collinear with both dd12 and dd13, rectangular */
    for (i = 1, flag = 0; i < n; i++) {
        if (gridindex[1]->index == gridinfo[i].index ||
            gridindex[2]->index == gridinfo[i].index ||
            gridinfo[first_index].network != gridinfo[i].network ||
            first_index == i) {
            continue;
        }
        dd14[0] = gridinfo[i].pos[0] * D2R
                - gridinfo[first_index].pos[0] * D2R;
        dd14[1] = gridinfo[i].pos[1] * D2R
                - gridinfo[first_index].pos[1] * D2R;
        if (fabs(dot(dd12, dd14, 2) / (norm(dd12, 2) * norm(dd14, 2)))
            < 1.0 &&
            fabs(dot(dd13, dd14, 2) / (norm(dd13, 2) * norm(dd14, 2)))
            < 1.0) {
            if ((fabs(grid2->pos[1] - gridinfo[i].pos[1]) < 0.04 &&
                 fabs(grid3->pos[0] - gridinfo[i].pos[0]) < 0.04) ||
                (fabs(grid2->pos[0] - gridinfo[i].pos[0]) < 0.04 &&
                 fabs(grid3->pos[1] - gridinfo[i].pos[1]) < 0.04)) {
                gridindex[3] = &gridinfo[i];
                flag = 1;
                break;
            }
        }
    }

    if (!flag) {
        trace(NULL, 2, "can't find fourth grid: network=%d\n",
              gridinfo[first_index].network);
        n = 3;
    } else {
        n = 4;
    }
    return n;
}

/* barycentric weights for 3-point interpolation -----------------------------*/
static void calc_3points_weight(double *weight, const double *pos,
                                grid_info **gridindex)
{
    double *U, *E, dd[2];
    int i;

    U = mat(2, 2); E = mat(2, 1);
    for (i = 1; i < 3; i++) {
        U[(i - 1) * 2 + 0] = gridindex[i]->pos[0] - gridindex[0]->pos[0];
        U[(i - 1) * 2 + 1] = gridindex[i]->pos[1] - gridindex[0]->pos[1];
    }
    trace(NULL, 5, "Umat=\n");
    tracemat(NULL, 5, U, 2, 2, 10, 5);

    if (matinv(U, 2)) trace(NULL, 2, "calculation error of G inverse\n");
    trace(NULL, 5, "Umat=\n");
    tracemat(NULL, 5, U, 2, 2, 10, 5);

    E[0] = pos[0] * R2D - gridindex[0]->pos[0];
    E[1] = pos[1] * R2D - gridindex[0]->pos[1];
    matmul("NN", 2, 1, 2, 1.0, U, E, 0.0, dd);
    free(U); free(E);

    weight[0] = 1.0 - dd[0] - dd[1];
    weight[1] = dd[0];
    weight[2] = dd[1];
}

/* check if receiver is inside grid polygon (stub) ---------------------------*/
static int is_exists_inside(const double *pos, int n, grid_info **gridindex,
                            grid_info *nearestgrid)
{
    int flag = 0xff; /* always true — upstream uses this as disabled stub */

    if (flag == 0x00) {
        if (nearestgrid->weight < gridindex[0]->weight) {
            trace(NULL, 2, "change to the nearest grid: inet=%d, index=%d, "
                  "pos=%6.3f %6.3f %6.3f, distance=%.2fKm\n",
                  nearestgrid->network, nearestgrid->index,
                  nearestgrid->pos[0], nearestgrid->pos[1],
                  nearestgrid->pos[2], nearestgrid->weight / 1000.0);
            gridindex[0] = nearestgrid;
            return 1;
        }
        trace(NULL, 2, "change to the nearest grid: inet=%d, index=%d, "
              "pos=%6.3f %6.3f %6.3f, distance=%.2fKm\n",
              gridindex[0]->network, gridindex[0]->index,
              gridindex[0]->pos[0], gridindex[0]->pos[1],
              gridindex[0]->pos[2], gridindex[0]->weight / 1000.0);
        n = 1;
    }
    return n;
}

/* compute grid interpolation weights ----------------------------------------*/
static void calc_grid_weight(double *weight, const double *pos, int n,
                             grid_info **gridindex)
{
    double sum;
    int i;

    if (n != 3) {
        sum = 0.0;
        for (i = 0; i < n; i++) sum += 1.0 / gridindex[i]->weight;
        for (i = 0; i < n; i++) weight[i] = 1.0 / (gridindex[i]->weight * sum);
    } else {
        calc_3points_weight(weight, pos, gridindex);
    }
}

/* log selected grid points (trace only on change) ---------------------------*/
static void output_selected_grid(const double *pos, int n,
                                 grid_info **gridindex, double *weight)
{
    static int savenetwork[4];
    static int saveindex[4];
    static int savenum = -1;
    int flag = 0;
    int i;

    for (i = 0; i < savenum; ++i) {
        if (savenetwork[i] != gridindex[i]->network ||
            saveindex[i] != gridindex[i]->index) {
            flag = 1;
            break;
        }
    }
    if (savenum == n && !flag) return;

    trace(NULL, 1, "pos est: pos=%6.3f %6.3f\n", pos[0] / D2R, pos[1] / D2R);
    for (i = 0; i < n; i++) {
        trace(NULL, 1, "selected_grid%d: inet=%d, weight=%.2f, "
              "pos=%6.3f %6.3f, dist=%.2fkm, index=%d\n",
              i + 1, gridindex[i]->network, weight[i],
              gridindex[i]->pos[0], gridindex[i]->pos[1],
              gridindex[i]->weight / 1000.0, gridindex[i]->index);
    }

    for (i = 0; i < n; ++i) {
        savenetwork[i] = gridindex[i]->network;
        saveindex[i]   = gridindex[i]->index;
    }
    savenum = n;
}

/*============================================================================
 * Public: Grid Selection
 *===========================================================================*/

/* select surrounding grid points and compute interpolation weights ----------
 *
 *  args:  clas_ctx_t   *ctx      I  CLAS context (grid_pos, grid_stat)
 *         const double *pos      I  receiver position {lat,lon,hgt} (rad,m)
 *         clas_grid_t  *grid     O  interpolation result
 *         int           gridsel  I  grid selection threshold (m), 0=always multi
 *         gtime_t       obstime  I  observation time (reserved)
 *  return: number of selected grids (0=error, 1/3/4=ok)
 *---------------------------------------------------------------------------*/
extern int clas_get_grid_index(clas_ctx_t *ctx, const double *pos,
                               clas_grid_t *grid, int gridsel,
                               gtime_t obstime)
{
    grid_info gridinfo[CLAS_CH_NUM][MAX_GRID_CACHE];
    grid_info *gridindex[4];
    double dlat, dlon;
    int i, n = 0, ch, nch[CLAS_CH_NUM] = {0};

    trace(NULL, 3, "clas_get_grid: pos=%.3f %.3f\n",
          pos[0] * R2D, pos[1] * R2D);

    /* search both channels, select channel with more candidates */
    for (ch = 0; ch < CLAS_CH_NUM; ch++) {
        nch[ch] = pickup_candidates_of_grid(ctx, gridinfo[ch],
                                            grid->network, pos, ch);
    }
    ch = nch[0] >= nch[1] ? 0 : 1;
    n  = nch[ch];

    if (n >= 3 && gridinfo[ch][0].network <= 12) {
        if (gridsel == 0 || gridinfo[ch][0].weight > (double)gridsel) {
            n = find_grid_surround_pos(&gridindex[0], n, gridinfo[ch]);
        } else {
            gridindex[0] = &gridinfo[ch][0];
            n = 1;
        }
    } else if (n == 0) {
        trace(NULL, 2, "can't find nearby grid: rover pos=%3.3f %3.3f\n",
              pos[0] * R2D, pos[1] * R2D);
        return 0;
    } else {
        gridindex[0] = &gridinfo[ch][0];
        n = 1;
    }

    n = is_exists_inside(pos, n, &gridindex[0], &gridinfo[ch][0]);
    calc_grid_weight(grid->weight, pos, n, &gridindex[0]);
    output_selected_grid(pos, n, gridindex, grid->weight);

    /* build 4×4 interpolation matrix for bilinear case */
    if (n == 4) {
        double *U = mat(n, n);
        for (i = 0; i < n; i++) {
            dlat = gridindex[i]->pos[0] - gridindex[0]->pos[0];
            dlon = gridindex[i]->pos[1] - gridindex[0]->pos[1];
            U[0 * n + i] = dlat;
            U[1 * n + i] = dlon;
            U[2 * n + i] = dlat * dlon;
            U[3 * n + i] = 1.0;
        }
        trace(NULL, 5, "Umat=\n");
        tracemat(NULL, 5, U, 4, n, 10, 5);

        if (matinv(U, n))
            trace(NULL, 2, "calculation error of G inverse\n");
        matcpy(grid->Gmat, U, n, n);
        trace(NULL, 5, "Gmat=\n");
        tracemat(NULL, 5, grid->Gmat, 4, n, 10, 5);
        free(U);
    }

    /* basis vector for polynomial evaluation */
    dlat = pos[0] * R2D - gridindex[0]->pos[0];
    dlon = pos[1] * R2D - gridindex[0]->pos[1];
    grid->Emat[0] = dlat;
    grid->Emat[1] = dlon;
    grid->Emat[2] = dlat * dlon;
    grid->Emat[3] = 1.0;

    grid->network = (grid->network == 0 ? gridindex[0]->network
                                        : grid->network);
    for (i = 0; i < n; i++) {
        grid->index[i] = gridindex[i]->index;
    }
    grid->num = n;
    return n;
}

/*============================================================================
 * STEC Data Retrieval and Interpolation (from stec.c / grid.c)
 *===========================================================================*/

/* search STEC data for a single satellite at one grid point -----------------*/
static int stec_data(stec_t *stec, gtime_t time, int sat, double *iono,
                     double *rate, double *rms, double *quality, int *slip)
{
    int k, flag;
    double tt;

    if (stec->n <= 0) return 0;

    /* search by satellite */
    for (k = flag = 0; k < stec->n; k++) {
        if (sat == stec->data[k].sat) {
            flag = 1;
            break;
        }
    }
    if (flag == 0) {
        trace(NULL, 3, "search_data: no iono (no sat data) %s sat=%2d\n",
              time_str(time, 0), sat);
        return 0;
    }

    tt = timediff(time, stec->data[k].time);
    if (fabs(tt) > MAXAGESSR_IONO) {
        trace(NULL, 2, "age of ssr iono error %s sat=%2d tt=%.0f\n",
              time_str(time, 0), sat, tt);
        return 0;
    }
    *iono    = stec->data[k].iono + stec->data[k].rate * tt;
    *rate    = stec->data[k].rate;
    *rms     = stec->data[k].rms;
    *slip    = stec->data[k].slip;
    *quality = stec->data[k].quality;

    return 1;
}

/* interpolate STEC (ionosphere delay) from grid points ----------------------
 *
 *  args:  clas_corr_t *corr    I  merged correction snapshot
 *         int         *index   I  selected grid point indices
 *         gtime_t      time    I  time (GPST)
 *         int          sat     I  satellite number
 *         int          n       I  number of grids (1-4)
 *         double      *weight  I  interpolation weights
 *         double      *Gmat    I  interpolation matrix (4×4, can be NULL)
 *         double      *Emat    I  basis vector (4×1, can be NULL)
 *         double      *iono    O  interpolated L1 ionosphere delay (m)
 *         double      *rate    O  interpolated ionosphere rate (m/s)
 *         double      *var     O  interpolated variance (m^2)
 *         int         *brk     O  slip break flag
 *  return: 1=ok, 0=error
 *---------------------------------------------------------------------------*/
extern int clas_stec_grid_data(clas_corr_t *corr, const int *index,
                               gtime_t time, int sat, int n,
                               const double *weight, const double *Gmat,
                               const double *Emat, double *iono,
                               double *rate, double *var, int *brk)
{
    int i, slip;
    double *ionos, *rates, *rms, *quals;
    double *ionos_, *rates_, *sqrms, *sqrms_, *quals_;

    if (n <= 0) return 0;

    ionos  = mat(n, 1); rates  = mat(n, 1); rms   = mat(n, 1); quals  = mat(n, 1);
    ionos_ = mat(n, 1); rates_ = mat(n, 1); sqrms = mat(n, 1); sqrms_ = mat(n, 1);
    quals_ = mat(n, 1);

    if (n == 1) {
        if (!stec_data(&corr->stec[index[0]], time, sat, iono, rate,
                       rms, quals, &slip)) {
            free(ionos); free(rates); free(rms); free(quals);
            free(ionos_); free(rates_); free(sqrms); free(sqrms_); free(quals_);
            return 0;
        }
        if (slip) *brk = 1;
        *var = SQR(rms[0]);
        free(ionos); free(rates); free(rms); free(quals);
        free(ionos_); free(rates_); free(sqrms); free(sqrms_); free(quals_);
        return 1;
    }

    /* fetch STEC at each grid point */
    for (i = 0; i < n; i++) {
        if (!stec_data(&corr->stec[index[i]], time, sat,
                       ionos + i, rates + i, rms + i, quals + i, &slip)) {
            free(ionos); free(rates); free(rms); free(quals);
            free(ionos_); free(rates_); free(sqrms); free(sqrms_); free(quals_);
            return 0;
        }
        if (slip) *brk = 1;
    }

    *iono = *rate = *var = 0.0;

    if (n == 4 && Gmat && Emat) {
        /* bilinear polynomial interpolation */
        for (i = 0; i < n; i++) sqrms[i] = SQR(rms[i]);
        matmul("NN", n, 1, n, 1.0, Gmat, ionos, 0.0, ionos_);
        matmul("NN", n, 1, n, 1.0, Gmat, rates, 0.0, rates_);
        matmul("NN", n, 1, n, 1.0, Gmat, sqrms, 0.0, sqrms_);
        matmul("NN", n, 1, n, 1.0, Gmat, quals, 0.0, quals_);
        *iono = dot(Emat, ionos_, 4);
        *rate = dot(Emat, rates_, 4);
        *var  = dot(Emat, sqrms_, 4);
    } else {
        /* weighted average */
        for (i = 0; i < n; i++) {
            *iono += ionos[i] * weight[i];
            *rate += rates[i] * weight[i];
            *var  += SQR(rms[i]) * weight[i];
        }
    }

    free(ionos); free(rates); free(rms); free(quals);
    free(ionos_); free(rates_); free(sqrms); free(sqrms_); free(quals_);

    return 1;
}

/*============================================================================
 * Troposphere Data Retrieval and Interpolation (from grid.c)
 *===========================================================================*/

/* fetch troposphere delay at a single grid point ----------------------------*/
static int trop_data(zwd_t *z, gtime_t time, double *ztd, double *zwd,
                     double *quality, int idx, int *valid)
{
    double tt;
    int k = 0;
    double tdvd, twvd, gh, pos[2];

    trace(NULL, 4, "trop_data: %s\n", time_str(time, 0));

    if (z->n <= 0) {
        return 0;
    }

    tt = timediff(time, z->data[k].time);
    if (fabs(tt) > MAXAGESSR_TROP) {
        trace(NULL, 2, "age of ssr trop error %s tt=%.0f idx=%d\n",
              time_str(time, 0), tt, idx);
        return 0;
    }

    pos[0] = z->pos[0] * D2R;
    pos[1] = z->pos[1] * D2R;
    gh = geoidh(pos);
    if (get_stTv(time, pos[0], 0.0, gh, &tdvd, &twvd)) return 0;

    if (z->data[k].ztd == CSSR_INVALID_VALUE ||
        z->data[k].zwd == CSSR_INVALID_VALUE) {
        trace(NULL, 2, "invalid trop correction %s ztd=%.2f zwd=%.2f\n",
              time_str(time, 0), z->data[k].ztd, z->data[k].zwd);
        return 0;
    }
    *ztd     = (z->data[k].ztd - z->data[k].zwd) / tdvd;
    *zwd     = z->data[k].zwd / twvd;
    *quality = z->data[k].quality;
    *valid   = z->data[k].valid;

    return 1;
}

/* interpolate troposphere delay from grid points ----------------------------
 *
 *  args:  clas_corr_t *corr    I  merged correction snapshot
 *         int         *index   I  selected grid point indices
 *         gtime_t      time    I  time (GPST)
 *         int          n       I  number of grids (1-4)
 *         double      *weight  I  interpolation weights
 *         double      *Gmat    I  interpolation matrix (4×4, can be NULL)
 *         double      *Emat    I  basis vector (4×1, can be NULL)
 *         double      *zwd     O  interpolated zenith wet delay
 *         double      *ztd     O  interpolated zenith total delay
 *         int         *tbrk    O  validity flag (0=all valid, 1=partial)
 *  return: 1=ok, 0=error
 *---------------------------------------------------------------------------*/
extern int clas_trop_grid_data(clas_corr_t *corr, const int *index,
                               gtime_t time, int n, const double *weight,
                               const double *Gmat, const double *Emat,
                               double *zwd, double *ztd, int *tbrk)
{
    int i, *valid, valid_ = 0, ret = 0;
    double *zwds, *ztds, *zwds_, *ztds_, *quals, *quals_;

    if (n <= 0) return 0;

    zwds = mat(n, 1); ztds = mat(n, 1); quals = mat(n, 1);
    valid = imat(n, 1);

    if (n == 1) {
        ret = trop_data(&corr->zwd[index[0]], time, ztd, zwd, quals,
                        index[0], valid);
        free(zwds); free(ztds); free(quals); free(valid);
        return ret;
    }

    /* fetch trop at each grid point */
    for (i = 0; i < n; i++) {
        if (!trop_data(&corr->zwd[index[i]], time, ztds + i, zwds + i,
                       quals + i, index[i], valid + i)) {
            free(zwds); free(ztds); free(quals); free(valid);
            return 0;
        }
    }

    *zwd = *ztd = 0.0;

    if (n == 4 && Gmat && Emat) {
        /* bilinear polynomial interpolation */
        zwds_ = mat(n, 1); ztds_ = mat(n, 1); quals_ = mat(n, 1);
        matmul("NN", n, 1, n, 1.0, Gmat, ztds,  0.0, ztds_);
        matmul("NN", n, 1, n, 1.0, Gmat, zwds,  0.0, zwds_);
        matmul("NN", n, 1, n, 1.0, Gmat, quals, 0.0, quals_);
        *ztd = dot(Emat, ztds_, 4);
        *zwd = dot(Emat, zwds_, 4);
        free(zwds_); free(ztds_); free(quals_);
    } else {
        /* weighted average */
        for (i = 0; i < n; i++) {
            *zwd += zwds[i] * weight[i];
            *ztd += ztds[i] * weight[i];
        }
    }

    for (i = 0; i < n; i++) valid_ += valid[i];
    *tbrk = valid_ == n ? 0 : 1;

    free(zwds); free(ztds); free(quals); free(valid);

    return 1;
}

/*============================================================================
 * Ocean Loading Initialization
 *===========================================================================*/

/* initialize ocean loading data in context ----------------------------------*/
extern void clas_init_oload(clas_ctx_t *ctx)
{
    int i;
    for (i = 0; i < CLAS_MAX_NETWORK; i++) {
        ctx->oload[i].gridnum = 0;
    }
}
/* read blq ocean tide loading parameters for CLAS grid ----------------------
* read blq ocean tide loading parameters for each grid point
* args   : char       *file  I   BLQ ocean tide loading parameter file
*          clas_ctx_t *ctx    IO  CLAS context (oload[] populated)
* return : status (1:ok, 0:file open error)
* notes  : file format: "net-gridno" header, 3 comment lines (last has lon/lat),
*          then 6 lines x 11 values (standard BLQ record)
*          ported from upstream claslib rtkcmn.c:readblqgrid()
*----------------------------------------------------------------------------*/
extern int readblqgrid(const char *file, clas_ctx_t *ctx)
{
    FILE *fp;
    char buff[256], *str, *str2;
    int net, gridno, i;

    trace(NULL, 3, "readblqgrid: file=%s\n", file);

    if (!(fp = fopen(file, "r"))) {
        trace(NULL, 2, "blq grid file open error: file=%s\n", file);
        return 0;
    }
    while (fgets(buff, sizeof(buff), fp)) {
        if (!strncmp(buff, "$$", 2) || strlen(buff) < 2) continue;

        /* parse "net-gridno" format (e.g., "1-1", "10-23") */
        str = strtok(buff, "-");
        if (!str) continue;
        net = atoi(str);
        str = strtok(NULL, "-");
        if (!str) continue;
        gridno = atoi(str);

        if (net < 1 || net > CLAS_MAX_NETWORK ||
            gridno < 1 || gridno > CLAS_MAX_GP) {
            trace(NULL, 1, "readblqgrid: net/grid overflow [%d-%d]\n",
                  net, gridno);
            continue;
        }

        /* skip 3 comment/header lines; the 3rd contains lon/lat */
        for (i = 0; i < 3; i++) {
            if (!fgets(buff, sizeof(buff), fp)) break;
        }
        if (i < 3) break;

        /* parse lon/lat from last comment line:
         * "$$ 1-1, ... lon/lat: 125.3700 24.7500 0.000" */
        str = strtok(buff, ":");
        str = strtok(NULL, ":");
        if (str) {
            str2 = strtok(str, " ");
            if (str2) ctx->oload[net - 1].pos[gridno - 1][0] = atof(str2);
            str2 = strtok(NULL, " ");
            if (str2) ctx->oload[net - 1].pos[gridno - 1][1] = atof(str2);
        }

        /* read 6-line BLQ record (11 constituents x 6 components) */
        if (!readblqrecord(fp, ctx->oload[net - 1].odisp[gridno - 1])) {
            trace(NULL, 1, "readblqgrid: format error at net=%d grid=%d\n",
                  net, gridno);
            fclose(fp);
            return 0;
        }
        ctx->oload[net - 1].gridnum++;
    }
    fclose(fp);

    trace(NULL, 3, "readblqgrid: loaded grid OTL data\n");
    return 1;
}
