/*------------------------------------------------------------------------------
 * rnx2rtkp.c : read rinex obs/nav files and compute receiver positions
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
 *-----------------------------------------------------------------------------*/
/**
 * @file rnx2rtkp.c
 * @brief Post-processing positioning from RINEX OBS/NAV files.
 *
 * History:
 *   2007/01/16  1.0 new
 *   2007/03/15  1.1 add library mode
 *   2007/05/08  1.2 separate from postpos.c
 *   2009/01/20  1.3 support rtklib 2.2.0 api
 *   2009/12/12  1.4 support glonass
 *                   add option -h, -a, -l, -x
 *   2010/01/28  1.5 add option -k
 *   2010/08/12  1.6 add option -y implementation (2.4.0_p1)
 *   2014/01/27  1.7 fix bug on default output time format
 *   2015/05/15  1.8 -r or -l options for fixed or ppp-fixed mode
 *   2015/06/12  1.9 output patch level in header
 *   2016/09/07  1.10 add option -sys
 *   2021/01/07  1.11 add option -ver
 *   2023/01/12  1.12 fix bugs
 *   2024/02/01  1.13 branch from ver.2.4.3b35 for MALIB
 *                    add option -ign_chierr
 *   2024/08/02  1.14 change initial value of glomodear
 *   2024/09/26  1.15 update version info
 *   2024/12/20  1.16 add option -sta
 */
#include <stdarg.h>

#include "mrtklib/mrtk_cli.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_options.h"
#include "mrtklib/mrtk_postpos.h"
#include "mrtklib/mrtklib.h"
#include "rtklib.h"

#define PROGNAME "rnx2rtkp" /* program name (kept for trace file + .pos header) */
#define MAXFILE 16          /* max number of input files */

/* long-option aliases -------------------------------------------------------*/
/* Note: -h is reserved for "fix and hold AR" in this subcommand, so we do not
 * map --help to -h. Instead, --help / -? are recognized explicitly below. */
static const mrtk_optmap_t opt_aliases[] = {
    {"--config", "-k"},    {"--output", "-o"}, {"--start", "-ts"}, {"--end", "-te"},
    {"--interval", "-ti"}, {"--freq", "-f"},   {"--trace", "-x"},  {NULL, NULL},
};

/* help text -----------------------------------------------------------------*/
static const char* help_lines[] = {
    "mrtk post: post-processing positioning from RINEX OBS/NAV (rnx2rtkp)",
    "",
    "Usage: mrtk post [OPTIONS] FILE [FILE...]",
    "",
    "  Reads RINEX OBS/NAV/GNAV/HNAV/CLK, SP3, and SBAS message logs and computes",
    "  receiver (rover) positions. The first OBS file is the rover; for relative",
    "  modes the second OBS file is the base. At least one NAV/GNAV/HNAV file is",
    "  required. SP3 precise ephemeris files use extension .sp3 or .eph.",
    "  Wild-cards (*) in file paths are supported; quote them as \"...\" to avoid",
    "  shell expansion. With -k, options come from a configuration file; command",
    "  line options take precedence.",
    "",
    "Options:",
    "  -k,  --config FILE         Configuration file (TOML or legacy)   [none]",
    "  -o,  --output FILE         Output file                           [stdout]",
    "  -ts, --start Y/M/D H:M:S   Start day/time (GPST)                 [obs start]",
    "  -te, --end   Y/M/D H:M:S   End day/time   (GPST)                 [obs end]",
    "  -ti, --interval SEC        Time interval (s)                     [all]",
    "  -p   MODE                  Positioning mode 0..7                 [2]",
    "                               (0:single,1:dgps,2:kinematic,3:static,",
    "                                4:moving-base,5:fixed,6:ppp-kinematic,",
    "                                7:ppp-static)",
    "  -m   DEG                   Elevation mask angle                  [15]",
    "  -sys S[,S...]              Nav systems                           [G,R]",
    "                               (G:GPS,R:GLO,E:GAL,J:QZS,C:BDS,I:IRN)",
    "  -f,  --freq N              Number of frequencies (relative)      [2]",
    "                               (1:L1, 2:L1+L2, 3:L1+L2+L5)",
    "  -v   THRES                 AR validation threshold (0.0: no AR)  [3.0]",
    "  -b                         Backward solutions                    [off]",
    "  -c                         Forward/backward combined solutions   [off]",
    "  -i                         Instantaneous integer AR              [off]",
    "  -h                         Fix-and-hold integer AR               [off]",
    "  -e                         Output ECEF x/y/z position            [lat/lon/h]",
    "  -a                         Output e/n/u baseline                 [lat/lon/h]",
    "  -n                         Output NMEA-0183 GGA                  [off]",
    "  -g                         Output lat/lon as ddd mm ss.ss'       [ddd.ddd]",
    "  -t                         Output time as yyyy/mm/dd hh:mm:ss.ss [sssss.ss]",
    "  -u                         Output time in UTC                    [GPST]",
    "  -d   COL                   Decimals in time                      [3]",
    "  -s   SEP                   Field separator                       [' ']",
    "  -r   X Y Z                 Reference (base) ECEF position (m)    [single avg]",
    "                               For fixed/ppp-fixed: rover ECEF position",
    "  -l   LAT LON HGT           Reference (base) lat/lon/h (deg, m)   [single avg]",
    "                               For fixed/ppp-fixed: rover lat/lon/h",
    "  -ign_chierr                Ignore chi-square error               [off]",
    "  -sta NAME                  Station name                          [RINEX MARKER]",
    "  -y   LEVEL                 Solution status (0:off,1:states,2:resid) [0]",
    "  -x,  --trace LEVEL         Debug trace level (0..5)              [0]",
    "  -ver                       Print version",
    "  -?,  --help                Show this help",
    "                               (-h is the fix-and-hold AR flag, not help)",
    "",
    "Examples:",
    "  mrtk post --config conf/claslib/post.toml obs.obs nav.nav clas.l6",
    "  mrtk post -k conf.toml --start 2024/01/15 00:00:00 -o out.pos *.obs *.nav",
    NULL,
};

static void printhelp(void) {
    int i;
    for (i = 0; help_lines[i]; i++) {
        fprintf(stderr, "%s\n", help_lines[i]);
    }
    exit(0);
}
/* print version -------------------------------------------------------------*/
static void printver(void) {
    fprintf(stderr, "%s(%s ver.%s git %s)\n", PROGNAME, MRTKLIB_SOFTNAME, MRTKLIB_VERSION_STRING, mrtklib_git_hash_str);
    exit(0);
}
/* rnx2rtkp main -------------------------------------------------------------*/
int mrtk_post(int argc, char** argv) {
    prcopt_t prcopt = prcopt_default;
    solopt_t solopt = solopt_default;
    filopt_t filopt = {""};
    gtime_t ts = {0}, te = {0};
    double tint = 0.0, es[] = {2000, 1, 1, 0, 0, 0}, ee[] = {2000, 12, 31, 23, 59, 59}, pos[3];
    int i, j, n, ret;
    char *infile[MAXFILE], *outfile = "", *p;
    mrtk_ctx_t* ctx;

    /* Initialize MRTKLIB runtime context */
    ctx = mrtk_ctx_create();
    g_mrtk_ctx = ctx;
    g_mrtk_legacy_ctx = mrtk_context_new();

    prcopt.mode = PMODE_KINEMA;
    prcopt.navsys = 0;
    prcopt.refpos = 1;
    prcopt.glomodear = 0;
    solopt.timef = 0;
    sprintf(solopt.prog, "%s(%s ver.%s)", PROGNAME, MRTKLIB_SOFTNAME, MRTKLIB_VERSION_STRING);
    sprintf(filopt.trace, "%s.trace", PROGNAME);

    /* translate --long flags to their -short aliases before parsing */
    mrtk_normalize_args(argc, argv, opt_aliases);

    /* recognise help flags up-front (-? is the legacy short flag here because
     * -h is reserved for fix-and-hold AR; --help is the unified long form) */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-?") || !strcmp(argv[i], "--help")) {
            printhelp();
        }
    }

    /* load options from configuration file(s)
     * resetsysopts() is called once before the loop so that multiple -k flags
     * are layered: each subsequent file overrides only the keys it specifies. */
    resetsysopts();
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-k") && i + 1 < argc) {
            if (!loadopts(argv[++i], sysopts)) {
                return -1;
            }
            getsysopts(&prcopt, &solopt, &filopt);
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
        } else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
            prcopt.mode = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-f") && i + 1 < argc) {
            prcopt.nf = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-sys") && i + 1 < argc) {
            for (p = argv[++i]; *p; p++) {
                switch (*p) {
                    case 'G':
                        prcopt.navsys |= SYS_GPS;
                        break;
                    case 'R':
                        prcopt.navsys |= SYS_GLO;
                        break;
                    case 'E':
                        prcopt.navsys |= SYS_GAL;
                        break;
                    case 'J':
                        prcopt.navsys |= SYS_QZS;
                        break;
                    case 'C':
                        prcopt.navsys |= SYS_CMP;
                        break;
                    case 'I':
                        prcopt.navsys |= SYS_IRN;
                        break;
                }
                if (!(p = strchr(p, ','))) {
                    break;
                }
            }
        } else if (!strcmp(argv[i], "-m") && i + 1 < argc) {
            prcopt.elmin = atof(argv[++i]) * D2R;
        } else if (!strcmp(argv[i], "-v") && i + 1 < argc) {
            prcopt.thresar[0] = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-s") && i + 1 < argc) {
            strcpy(solopt.sep, argv[++i]);
        } else if (!strcmp(argv[i], "-d") && i + 1 < argc) {
            solopt.timeu = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-b")) {
            prcopt.soltype = 1;
        } else if (!strcmp(argv[i], "-c")) {
            prcopt.soltype = 2;
        } else if (!strcmp(argv[i], "-i")) {
            prcopt.modear = 2;
        } else if (!strcmp(argv[i], "-h")) {
            prcopt.modear = 3;
        } else if (!strcmp(argv[i], "-t")) {
            solopt.timef = 1;
        } else if (!strcmp(argv[i], "-u")) {
            solopt.times = TIMES_UTC;
        } else if (!strcmp(argv[i], "-e")) {
            solopt.posf = SOLF_XYZ;
        } else if (!strcmp(argv[i], "-a")) {
            solopt.posf = SOLF_ENU;
        } else if (!strcmp(argv[i], "-n")) {
            solopt.posf = SOLF_NMEA;
        } else if (!strcmp(argv[i], "-g")) {
            solopt.degf = 1;
        } else if (!strcmp(argv[i], "-r") && i + 3 < argc) {
            prcopt.refpos = prcopt.rovpos = 0;
            for (j = 0; j < 3; j++) {
                prcopt.rb[j] = atof(argv[++i]);
            }
            matcpy(prcopt.ru, prcopt.rb, 3, 1);
        } else if (!strcmp(argv[i], "-l") && i + 3 < argc) {
            prcopt.refpos = prcopt.rovpos = 0;
            for (j = 0; j < 3; j++) {
                pos[j] = atof(argv[++i]);
            }
            for (j = 0; j < 2; j++) {
                pos[j] *= D2R;
            }
            pos2ecef(pos, prcopt.rb);
            matcpy(prcopt.ru, prcopt.rb, 3, 1);
        } else if (!strcmp(argv[i], "-l6msg") && i + 1 < argc) {
            prcopt.l6mrg = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-ign_chierr")) {
            prcopt.ign_chierr = 1;
        } else if (!strcmp(argv[i], "-sta") && i + 1 < argc) {
            strcpy(prcopt.staname, argv[++i]);
        } else if (!strcmp(argv[i], "-y") && i + 1 < argc) {
            solopt.sstat = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-x") && i + 1 < argc) {
            solopt.trace = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-ver")) {
            printver();
        } else if (*argv[i] == '-') {
            printhelp();
        } else if (n < MAXFILE) {
            infile[n++] = argv[i];
        }
    }
    if (!prcopt.navsys) {
        prcopt.navsys = SYS_GPS | SYS_GLO;
    }
    if (n <= 0) {
        showmsg("error : no input file");
        mrtk_context_free(g_mrtk_legacy_ctx);
        g_mrtk_ctx = NULL;
        mrtk_ctx_destroy(ctx);
        return -2;
    }
    {
        char cmsg[256] = "";
        if (!resolve_correction(&prcopt, cmsg, sizeof(cmsg))) {
            fprintf(stderr, "error : %s\n", cmsg);
            mrtk_context_free(g_mrtk_legacy_ctx);
            g_mrtk_ctx = NULL;
            mrtk_ctx_destroy(ctx);
            return -2;
        }
    }
    /* #135: the legacy pppsig signal selection reshapes the obsdef tables and
     * destructively drops bands the receiver does not nominally provide. For
     * conventional IGS-product PPP, skip it so the receiver's actual 2nd band
     * (e.g. GAL E5b / BDS B2I on u-blox F9P) survives and the iono-free pair is
     * chosen from the available observations (see prange/corr_meas). MADOCA/CLAS
     * and all other correction sources keep the legacy behaviour unchanged. */
    if (prcopt.correction != CORR_IGS) {
        apply_pppsig(prcopt.pppsig);
        if (prcopt.correction == CORR_QZS_CLAS) {
            apply_claslib_ppprtk_obsdef();
        }
    }
    mrtk_use_claslib_qzs_l1_priority(prcopt.correction == CORR_QZS_CLAS);
    ret = postpos(ctx, ts, te, tint, 0.0, &prcopt, &solopt, &filopt, infile, n, outfile, "", "");

    if (!ret) {
        fprintf(stderr, "%40s\r", "");
    }
    mrtk_context_free(g_mrtk_legacy_ctx);
    g_mrtk_ctx = NULL;
    mrtk_ctx_destroy(ctx);
    return ret;
}
