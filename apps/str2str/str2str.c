/*------------------------------------------------------------------------------
 * str2str.c : console version of stream server
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2007-2020 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file str2str.c
 * @brief CLI application for stream-to-stream data relay and conversion.
 *
 * version : $Revision: 1.1 $ $Date: 2008/07/17 21:54:53 $
 * history : 2009/06/17  1.0  new
 *           2011/05/29  1.1  add -f, -l and -x option
 *           2011/11/29  1.2  fix bug on recognize ntrips:// (rtklib_2.4.1_p4)
 *           2012/12/25  1.3  add format conversion functions
 *                            add -msg, -opt and -sta options
 *                            modify -p option
 *           2013/01/25  1.4  fix bug on showing message
 *           2014/02/21  1.5  ignore SIG_HUP
 *           2014/08/10  1.5  fix bug on showing message
 *           2014/08/26  1.6  support input format gw10, binex and rt17
 *           2014/10/14  1.7  use stdin or stdout if option -in or -out omitted
 *           2014/11/08  1.8  add option -a, -i and -o
 *           2015/03/23  1.9  fix bug on parsing of command line options
 *           2016/01/23  1.10 enable septentrio
 *           2016/01/26  1.11 fix bug on station position by -p option (#126)
 *                            add option -px
 *           2016/07/01  1.12 support CMR/CMR+
 *           2016/07/23  1.13 add option -c1 -c2 -c3 -c4
 *           2016/09/03  1.14 support ntrip caster
 *                            add option -ft,-fl
 *           2016/09/06  1.15 add reload soure table by USR2 signal
 *           2016/09/17  1.16 add option -b
 *           2017/05/26  1.17 add input format tersus
 *           2020/11/30  1.18 support api change strsvrstart(),strsvrstat()
 */
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>

#include "mrtklib/mrtk_cli.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/rtklib.h"

#define PRGNAME "str2str"      /* program name */
#define MAXSTR 5               /* max number of streams */
#define TRFILE "str2str.trace" /* trace file */

/* global variables ----------------------------------------------------------*/
static strsvr_t strsvr;          /* stream server */
static volatile int intrflg = 0; /* interrupt flag */

/* long-option aliases -------------------------------------------------------*/
static const mrtk_optmap_t opt_aliases[] = {
    {"--input", "-in"},
    {"--output", "-out"},
    {"--trace", "-t"},
    {NULL, NULL},
};

/* help text -----------------------------------------------------------------*/
static const char* help[] = {
    "mrtk relay: stream multiplexer / format converter (str2str)",
    "",
    "Usage: mrtk relay [-in STREAM] [-out STREAM [-out STREAM...]] [OPTIONS]",
    "",
    "  Reads from one input stream and writes to one or more output streams.",
    "  Input may be serial, tcp client/server, ntrip client, or file. Output",
    "  may be serial, tcp client/server, ntrip server, or file. With #format",
    "  on both endpoints, the input is decoded and re-encoded for output.",
    "  Resident process: stop with Ctrl-C in foreground or SIGINT in background.",
    "  Omitting -in/-out (or passing a null path) defaults to stdin/stdout.",
    "",
    "Options:",
    "  -in,  --input  STREAM[#FORMAT]    Input stream path and format",
    "  -out, --output STREAM[#FORMAT]    Output stream path and format (repeat OK)",
    "",
    "  Stream paths:",
    "    serial       : serial://port[:brate[:bsize[:parity[:stopb[:fctr]]]]]",
    "    tcp server   : tcpsvr://:port",
    "    tcp client   : tcpcli://addr[:port]",
    "    ntrip client : ntrip://[user[:passwd]@]addr[:port][/mntpnt]",
    "    ntrip server : ntrips://[:passwd@]addr[:port]/mntpnt[:str] (only out)",
    "    ntrip caster : ntripc://[user:passwd@][:port]/mntpnt[:srctbl] (only out)",
    "    file         : [file://]path[::T][::+start][::xseppd][::S=swap]",
    "",
    "  Formats:",
    "    rtcm2        : RTCM 2 (only in)",
    "    rtcm3        : RTCM 3",
    "    nov          : NovAtel OEMV/4/6,OEMStar (only in)",
    "    oem3         : NovAtel OEM3 (only in)",
    "    ubx          : ublox LEA-4T/5T/6T (only in)",
    "    ss2          : NovAtel Superstar II (only in)",
    "    hemis        : Hemisphere Eclipse/Crescent (only in)",
    "    stq          : SkyTraq S1315F (only in)",
    "    javad        : Javad (only in)",
    "    nvs          : NVS BINR (only in)",
    "    binex        : BINEX (only in)",
    "    rt17         : Trimble RT17 (only in)",
    "    sbf          : Septentrio SBF (only in)",
    "",
    "  -msg \"TYPE[(TINT)][,TYPE[(TINT)]...]\"  RTCM message types and intervals (s)",
    "  -sta SID            Station ID",
    "  -opt OPT            Receiver-dependent options",
    "  -s   MSEC           Timeout (ms)                                  [10000]",
    "  -r   MSEC           Reconnect interval (ms)                       [10000]",
    "  -n   MSEC           NMEA request cycle (ms)                       [0]",
    "  -f   SEC            File swap margin (s)                          [30]",
    "  -c   FILE           Input commands file                           [none]",
    "  -c1  FILE           Output 1 commands file                        [none]",
    "  -c2  FILE           Output 2 commands file                        [none]",
    "  -c3  FILE           Output 3 commands file                        [none]",
    "  -c4  FILE           Output 4 commands file                        [none]",
    "  -p   LAT LON HGT    Station position (deg, m)",
    "  -px  X Y Z          Station position (ECEF, m)",
    "  -a   ANTINFO        Antenna info (comma-separated)",
    "  -i   RCVINFO        Receiver info (comma-separated)",
    "  -o   E N U          Antenna offset (e,n,u, m)",
    "  -l   LOCAL_DIR      ftp/http local directory                      [none]",
    "  -x   PROXY_ADDR     http/ntrip proxy address                      [none]",
    "  -b   STR_NO         Relay back messages from output to input      [none]",
    "  -t,  --trace LEVEL  Trace level                                   [0]",
    "  -fl  FILE           Log file                                      [str2str.trace]",
    "  -h,  --help         Show this help",
    "",
    "Examples:",
    "  mrtk relay --input ntrip://user:pw@caster:2101/MNT \\",
    "             --output tcpsvr://:9000",
    "  mrtk relay -in serial://ttyUSB0:115200#sbf -out file://out.rtcm3#rtcm3",
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
/* signal handler ------------------------------------------------------------*/
static void sigfunc(int sig) { intrflg = 1; }
/* decode format -------------------------------------------------------------*/
static void decodefmt(char* path, int* fmt) {
    char* p;

    *fmt = -1;

    if ((p = strrchr(path, '#'))) {
        if (!strcmp(p, "#rtcm2"))
            *fmt = STRFMT_RTCM2;
        else if (!strcmp(p, "#rtcm3"))
            *fmt = STRFMT_RTCM3;
        else if (!strcmp(p, "#nov"))
            *fmt = STRFMT_OEM4;
        else if (!strcmp(p, "#oem3"))
            *fmt = STRFMT_OEM3;
        else if (!strcmp(p, "#ubx"))
            *fmt = STRFMT_UBX;
        else if (!strcmp(p, "#ss2"))
            *fmt = STRFMT_SS2;
        else if (!strcmp(p, "#hemis"))
            *fmt = STRFMT_CRES;
        else if (!strcmp(p, "#stq"))
            *fmt = STRFMT_STQ;
        else if (!strcmp(p, "#javad"))
            *fmt = STRFMT_JAVAD;
        else if (!strcmp(p, "#nvs"))
            *fmt = STRFMT_NVS;
        else if (!strcmp(p, "#binex"))
            *fmt = STRFMT_BINEX;
        else if (!strcmp(p, "#rt17"))
            *fmt = STRFMT_RT17;
        else if (!strcmp(p, "#sbf"))
            *fmt = STRFMT_SEPT;
        else
            return;
        *p = '\0';
    }
}
/* decode stream path --------------------------------------------------------*/
static int decodepath(const char* path, int* type, char* strpath, int* fmt) {
    char buff[1024], *p;

    strcpy(buff, path);

    /* decode format */
    decodefmt(buff, fmt);

    /* decode type */
    if (!(p = strstr(buff, "://"))) {
        strcpy(strpath, buff);
        *type = STR_FILE;
        return 1;
    }
    if (!strncmp(path, "serial", 6))
        *type = STR_SERIAL;
    else if (!strncmp(path, "tcpsvr", 6))
        *type = STR_TCPSVR;
    else if (!strncmp(path, "tcpcli", 6))
        *type = STR_TCPCLI;
    else if (!strncmp(path, "ntripc", 6))
        *type = STR_NTRIPCAS;
    else if (!strncmp(path, "ntrips", 6))
        *type = STR_NTRIPSVR;
    else if (!strncmp(path, "ntrip", 5))
        *type = STR_NTRIPCLI;
    else if (!strncmp(path, "file", 4))
        *type = STR_FILE;
    else {
        fprintf(stderr, "stream path error: %s\n", buff);
        return 0;
    }
    strcpy(strpath, p + 3);
    return 1;
}
/* read receiver commands ----------------------------------------------------*/
static void readcmd(const char* file, char* cmd, int type) {
    FILE* fp;
    char buff[MAXSTR], *p = cmd;
    int i = 0;

    *p = '\0';

    if (!(fp = fopen(file, "r"))) return;

    while (fgets(buff, sizeof(buff), fp)) {
        if (*buff == '@')
            i++;
        else if (i == type && p + strlen(buff) + 1 < cmd + MAXRCVCMD) {
            p += sprintf(p, "%s", buff);
        }
    }
    fclose(fp);
}
/* str2str -------------------------------------------------------------------*/
int mrtk_relay(int argc, char** argv) {
    static char cmd_strs[MAXSTR][MAXRCVCMD] = {"", "", "", "", ""};
    static char cmd_periodic_strs[MAXSTR][MAXRCVCMD] = {"", "", "", "", ""};
    const char ss[] = {'E', '-', 'W', 'C', 'C'};
    strconv_t* conv[MAXSTR] = {NULL};
    double pos[3], stapos[3] = {0}, stadel[3] = {0};
    static char s1[MAXSTR][MAXSTRPATH] = {{0}}, s2[MAXSTR][MAXSTRPATH] = {{0}};
    char *paths[MAXSTR], *logs[MAXSTR];
    char *cmdfile[MAXSTR] = {"", "", "", "", ""}, *cmds[MAXSTR], *cmds_periodic[MAXSTR];
    char *local = "", *proxy = "", *msg = "1004,1019", *opt = "", buff[256], *p;
    char strmsg[MAXSTRMSG] = "", *antinfo = "", *rcvinfo = "";
    char *ant[] = {"", "", ""}, *rcv[] = {"", "", ""}, *logfile = "";
    int i, j, n = 0, dispint = 5000, trlevel = 0, opts[] = {10000, 10000, 2000, 32768, 10, 0, 30, 0};
    int types[MAXSTR] = {STR_FILE, STR_FILE}, stat[MAXSTR] = {0}, log_stat[MAXSTR] = {0};
    int bps[MAXSTR] = {0}, fmts[MAXSTR] = {0}, sta = 0;
    uint64_t byte[MAXSTR] = {0};
    mrtk_ctx_t* ctx = NULL;

    /* translate --long flags to their -short aliases before parsing */
    mrtk_normalize_args(argc, argv, opt_aliases);

    for (i = 0; i < MAXSTR; i++) {
        paths[i] = s1[i];
        logs[i] = s2[i];
        cmds[i] = cmd_strs[i];
        cmds_periodic[i] = cmd_periodic_strs[i];
    }
    for (i = 1; i < argc; i++) {
        if (mrtk_is_help_flag(argv[i])) {
            printhelp();
        } else if (!strcmp(argv[i], "-in") && i + 1 < argc) {
            if (!decodepath(argv[++i], types, paths[0], fmts)) return -1;
        } else if (!strcmp(argv[i], "-out") && i + 1 < argc && n < MAXSTR - 1) {
            if (!decodepath(argv[++i], types + n + 1, paths[n + 1], fmts + n + 1)) return -1;
            n++;
        } else if (!strcmp(argv[i], "-p") && i + 3 < argc) {
            pos[0] = atof(argv[++i]) * D2R;
            pos[1] = atof(argv[++i]) * D2R;
            pos[2] = atof(argv[++i]);
            pos2ecef(pos, stapos);
        } else if (!strcmp(argv[i], "-px") && i + 3 < argc) {
            stapos[0] = atof(argv[++i]);
            stapos[1] = atof(argv[++i]);
            stapos[2] = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-o") && i + 3 < argc) {
            stadel[0] = atof(argv[++i]);
            stadel[1] = atof(argv[++i]);
            stadel[2] = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-msg") && i + 1 < argc)
            msg = argv[++i];
        else if (!strcmp(argv[i], "-opt") && i + 1 < argc)
            opt = argv[++i];
        else if (!strcmp(argv[i], "-sta") && i + 1 < argc)
            sta = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-d") && i + 1 < argc)
            dispint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc)
            opts[0] = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-r") && i + 1 < argc)
            opts[1] = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-n") && i + 1 < argc)
            opts[5] = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-f") && i + 1 < argc)
            opts[6] = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-c") && i + 1 < argc)
            cmdfile[0] = argv[++i];
        else if (!strcmp(argv[i], "-c1") && i + 1 < argc)
            cmdfile[1] = argv[++i];
        else if (!strcmp(argv[i], "-c2") && i + 1 < argc)
            cmdfile[2] = argv[++i];
        else if (!strcmp(argv[i], "-c3") && i + 1 < argc)
            cmdfile[3] = argv[++i];
        else if (!strcmp(argv[i], "-c4") && i + 1 < argc)
            cmdfile[4] = argv[++i];
        else if (!strcmp(argv[i], "-a") && i + 1 < argc)
            antinfo = argv[++i];
        else if (!strcmp(argv[i], "-i") && i + 1 < argc)
            rcvinfo = argv[++i];
        else if (!strcmp(argv[i], "-l") && i + 1 < argc)
            local = argv[++i];
        else if (!strcmp(argv[i], "-x") && i + 1 < argc)
            proxy = argv[++i];
        else if (!strcmp(argv[i], "-b") && i + 1 < argc)
            opts[7] = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-fl") && i + 1 < argc)
            logfile = argv[++i];
        else if (!strcmp(argv[i], "-t") && i + 1 < argc)
            trlevel = atoi(argv[++i]);
        else if (*argv[i] == '-')
            printhelp();
    }
    if (n <= 0) n = 1; /* stdout */

    for (i = 0; i < n; i++) {
        if (fmts[i + 1] <= 0) continue;
        if (fmts[i + 1] != STRFMT_RTCM3) {
            fprintf(stderr, "unsupported output format\n");
            return -1;
        }
        if (fmts[0] < 0) {
            fprintf(stderr, "specify input format\n");
            return -1;
        }
        if (!(conv[i] = strconvnew(fmts[0], fmts[i + 1], msg, sta, sta != 0, opt))) {
            fprintf(stderr, "stream conversion error\n");
            return -1;
        }
        strcpy(buff, antinfo);
        for (p = strtok(buff, ","), j = 0; p && j < 3; p = strtok(NULL, ",")) ant[j++] = p;
        strcpy(conv[i]->out.sta.antdes, ant[0]);
        strcpy(conv[i]->out.sta.antsno, ant[1]);
        conv[i]->out.sta.antsetup = atoi(ant[2]);
        strcpy(buff, rcvinfo);
        for (p = strtok(buff, ","), j = 0; p && j < 3; p = strtok(NULL, ",")) rcv[j++] = p;
        strcpy(conv[i]->out.sta.rectype, rcv[0]);
        strcpy(conv[i]->out.sta.recver, rcv[1]);
        strcpy(conv[i]->out.sta.recsno, rcv[2]);
        matcpy(conv[i]->out.sta.pos, stapos, 3, 1);
        matcpy(conv[i]->out.sta.del, stadel, 3, 1);
    }
    signal(SIGTERM, sigfunc);
    signal(SIGINT, sigfunc);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    strsvrinit(&strsvr, n);

    if (trlevel > 0) {
        /* trace*() resolve their target through the global runtime context;
           without it traceopen() no-ops and no trace file is written (#79) */
        ctx = mrtk_ctx_create();
        g_mrtk_ctx = ctx;
        traceopen(NULL, *logfile ? logfile : TRFILE);
        tracelevel(NULL, trlevel);
    }
    fprintf(stderr, "stream server start\n");

    strsetdir(local);
    strsetproxy(proxy);

    for (i = 0; i < MAXSTR; i++) {
        if (*cmdfile[i]) readcmd(cmdfile[i], cmds[i], 0);
        if (*cmdfile[i]) readcmd(cmdfile[i], cmds_periodic[i], 2);
    }
    /* start stream server */
    if (!strsvrstart(&strsvr, opts, types, paths, logs, conv, cmds, cmds_periodic, stapos)) {
        fprintf(stderr, "stream server start error\n");
        for (i = 0; i < n; i++) {
            strconvfree(conv[i]);
        }
        if (trlevel > 0) {
            traceclose(NULL);
            g_mrtk_ctx = NULL;
            mrtk_ctx_destroy(ctx);
        }
        return -1;
    }
    for (intrflg = 0; !intrflg;) {
        /* get stream server status */
        strsvrstat(&strsvr, stat, log_stat, byte, bps, strmsg);

        /* show stream server status */
        for (i = 0, p = buff; i < MAXSTR; i++) p += sprintf(p, "%c", ss[stat[i] + 1]);

        fprintf(stderr, "%s [%s] %10" PRIu64 " B %7d bps %s\n", time_str(utc2gpst(timeget()), 0), buff, byte[0], bps[0],
                strmsg);

        sleepms(dispint);
    }
    for (i = 0; i < MAXSTR; i++) {
        if (*cmdfile[i]) readcmd(cmdfile[i], cmds[i], 1);
    }
    /* stop stream server */
    strsvrstop(&strsvr, cmds);

    for (i = 0; i < n; i++) {
        strconvfree(conv[i]);
    }
    if (trlevel > 0) {
        traceclose(NULL);
        g_mrtk_ctx = NULL;
        mrtk_ctx_destroy(ctx);
    }
    fprintf(stderr, "stream server stop\n");
    return 0;
}
