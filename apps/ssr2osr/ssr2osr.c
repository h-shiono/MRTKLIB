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

#include "mrtklib/mrtk_cli.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_options.h"
#include "mrtklib/mrtk_postpos.h"
#include "mrtklib/mrtklib.h"
#include "rtklib.h"

#define PROGNAME "ssr2osr" /* program name */
#define MAXFILE 8          /* max number of input files */

/* long-option aliases */
static const mrtk_optmap_t opt_aliases[] = {
    {"--config", "-k"},    {"--output", "-o"}, {"--start", "-ts"}, {"--end", "-te"},
    {"--interval", "-ti"}, {"--trace", "-x"},  {NULL, NULL},
};

/* help text -----------------------------------------------------------------*/
static const char* help[] = {
    "mrtk ssr2osr: convert SSR corrections to OSR (per-satellite)",
    "",
    "Usage: mrtk ssr2osr [OPTIONS] FILE...",
    "",
    "  Reads RINEX OBS/NAV and QZSS L6 (.l6) files, computes per-satellite",
    "  observation-space-representation (OSR) corrections, and writes them",
    "  to stdout or a file.",
    "",
    "Options:",
    "  -k,  --config FILE         Configuration file (TOML or legacy)   [none]",
    "  -ti, --interval SEC        Time interval (s)                     [all]",
    "  -ts, --start Y/M/D H:M:S   Start day/time (GPST)                 [obs start]",
    "  -te, --end   Y/M/D H:M:S   End day/time   (GPST)                 [obs end]",
    "  -o,  --output FILE         Output file                           [stdout]",
    "  -x,  --trace LEVEL         Debug trace level                     [0]",
    "  -h,  --help                Show this help",
    "",
    "Examples:",
    "  mrtk ssr2osr --config conf.toml --output osr.csv obs.obs nav.nav corr.l6",
    NULL,
};
/* print help ----------------------------------------------------------------*/
static void printhelp(void) {
    int i;
    for (i = 0; help[i]; i++) {
        fprintf(stderr, "%s\n", help[i]);
    }
    exit(0);
}
/* ssr2osr main --------------------------------------------------------------*/
int mrtk_ssr2osr(int argc, char** argv) {
    prcopt_t prcopt = prcopt_default;
    solopt_t solopt = solopt_default;
    filopt_t filopt = {""};
    gtime_t ts = {0}, te = {0};
    double tint = 1.0, es[] = {2000, 1, 1, 0, 0, 0}, ee[] = {2000, 12, 31, 23, 59, 59};
    int i, n, ret = 0;
    char *infile[MAXFILE], *outfile = "";
    mrtk_ctx_t* ctx;

    /* Initialize MRTKLIB runtime context */
    ctx = mrtk_ctx_create();
    g_mrtk_ctx = ctx;
    g_mrtk_legacy_ctx = mrtk_context_new();

    prcopt.mode = PMODE_SSR2OSR;
    prcopt.navsys = SYS_GPS | SYS_QZS;
    prcopt.refpos = 1;
    prcopt.glomodear = 1;
    solopt.timef = 0;
    sprintf(solopt.prog, "%s(%s ver.%s)", PROGNAME, MRTKLIB_SOFTNAME, MRTKLIB_VERSION_STRING);
    sprintf(filopt.trace, "%s.trace", PROGNAME);

    /* translate --long flags to their -short aliases before parsing */
    mrtk_normalize_args(argc, argv, opt_aliases);

    /* up-front help recognition */
    for (i = 1; i < argc; i++) {
        if (mrtk_is_help_flag(argv[i])) {
            printhelp();
        }
    }

    /* load options from configuration file */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-k") && i + 1 < argc) {
            resetsysopts();
            if (!loadopts(argv[++i], sysopts)) {
                mrtk_context_free(g_mrtk_legacy_ctx);
                g_mrtk_ctx = NULL;
                mrtk_ctx_destroy(ctx);
                return -1;
            }
            getsysopts(&prcopt, &solopt, &filopt);
            mrtk_apply_obsdef(&prcopt, 0);
        }
    }
    for (i = 1, n = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            outfile = argv[++i];
        } else if (!strcmp(argv[i], "-ts") && i + 2 < argc) {
            sscanf(argv[++i], "%lf/%lf/%lf", es, es + 1, es + 2);
            sscanf(argv[++i], "%lf:%lf:%lf", es + 3, es + 4, es + 5);
            ts = epoch2time(es);
        } else if (!strcmp(argv[i], "-te") && i + 2 < argc) {
            sscanf(argv[++i], "%lf/%lf/%lf", ee, ee + 1, ee + 2);
            sscanf(argv[++i], "%lf:%lf:%lf", ee + 3, ee + 4, ee + 5);
            te = epoch2time(ee);
        } else if (!strcmp(argv[i], "-ti") && i + 1 < argc) {
            tint = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-k") && i + 1 < argc) {
            ++i;
            continue;
        } else if (!strcmp(argv[i], "-x") && i + 1 < argc) {
            solopt.trace = atoi(argv[++i]);
        } else if (*argv[i] == '-') {
            printhelp();
        } else if (n < MAXFILE) {
            infile[n++] = argv[i];
        }
    }
    if (n <= 0) {
        showmsg("error : no input file");
        mrtk_context_free(g_mrtk_legacy_ctx);
        g_mrtk_ctx = NULL;
        mrtk_ctx_destroy(ctx);
        return -2;
    }

    /* generate OSR via postpos pipeline */
    ret = postpos(ctx, ts, te, tint, 0.0, &prcopt, &solopt, &filopt, infile, n, outfile, "", "");

    if (!ret) {
        fprintf(stderr, "%40s\r", "");
    }
    mrtk_context_free(g_mrtk_legacy_ctx);
    g_mrtk_ctx = NULL;
    mrtk_ctx_destroy(ctx);
    return ret;
}
