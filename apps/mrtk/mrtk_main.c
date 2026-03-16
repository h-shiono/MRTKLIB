/*------------------------------------------------------------------------------
 * mrtk_main.c : unified CLI entry point for MRTKLIB
 *
 * Copyright (C) 2025-2026, MRTKLIB Contributors, All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * description : Single-binary entry point that dispatches subcommands to the
 *               individual MRTKLIB applications (BusyBox/Git pattern).
 *
 * references  : [1] MRTKLIB project — https://github.com/h-shiono/MRTKLIB
 *
 *-----------------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "rtklib.h"

/* external subcommand entry points ------------------------------------------*/
extern int mrtk_post(int argc, char** argv);
extern int mrtk_run(int argc, char** argv);
extern int mrtk_relay(int argc, char** argv);
extern int mrtk_convert(int argc, char** argv);
extern int mrtk_ssr2obs(int argc, char** argv);
extern int mrtk_ssr2osr(int argc, char** argv);
extern int mrtk_bias(int argc, char** argv);
extern int mrtk_dump(int argc, char** argv);
extern int mrtk_cssr2rtcm3(int argc, char** argv);
extern int mrtk_l6extract(int argc, char** argv);

/* showmsg / settspan / settime — shared callback stubs required by mrtklib --*/
extern int showmsg(const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    vfprintf(stderr, format, arg);
    va_end(arg);
    fprintf(stderr, *format ? "\r" : "\n");
    return 0;
}
extern void settspan(gtime_t ts, gtime_t te) {
    (void)ts;
    (void)te;
}
extern void settime(gtime_t time) { (void)time; }

/* subcommand table ----------------------------------------------------------*/
typedef struct {
    const char* name;
    int (*func)(int, char**);
} subcmd_t;

static const subcmd_t subcmds[] = {
    {"run", mrtk_run},         {"post", mrtk_post},       {"relay", mrtk_relay}, {"convert", mrtk_convert},
    {"ssr2obs", mrtk_ssr2obs}, {"ssr2osr", mrtk_ssr2osr}, {"bias", mrtk_bias},   {"dump", mrtk_dump},
    {"cssr2rtcm3", mrtk_cssr2rtcm3},
    {"l6extract", mrtk_l6extract},
};
#define NSUBCMD (int)(sizeof(subcmds) / sizeof(subcmds[0]))

/* help text -----------------------------------------------------------------*/
static void print_help(void) {
    fprintf(stderr,
            "mrtk: MRTKLIB unified CLI (%s ver.%s)\n"
            "\n"
            "Usage: mrtk [COMMAND] [OPTIONS]\n"
            "\n"
            "Core Commands:\n"
            "  run         Run real-time positioning pipeline (rtkrcv)\n"
            "  post        Run post-processing positioning (rnx2rtkp)\n"
            "\n"
            "Data & Streaming:\n"
            "  relay       Relay and split data streams (str2str)\n"
            "  convert     Convert receiver raw data to RINEX (convbin)\n"
            "\n"
            "Format Translation:\n"
            "  ssr2obs     Convert SSR corrections to pseudo-observations\n"
            "  ssr2osr     Convert SSR corrections to OSR\n"
            "  cssr2rtcm3  Convert CLAS CSSR to RTCM3 MSM4 (real-time VRS)\n"
            "\n"
            "Utilities:\n"
            "  bias        Estimate receiver fractional biases\n"
            "  dump        Dump stream data to human-readable format\n"
            "  l6extract   Extract L6 frames from SBF/UBX to per-PRN files\n",
            MRTKLIB_SOFTNAME, MRTKLIB_VERSION_STRING);
}

/* main ----------------------------------------------------------------------*/
int main(int argc, char** argv) {
    int i;

    if (argc < 2) {
        print_help();
        return 1;
    }
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        print_help();
        return 0;
    }
    if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) {
        fprintf(stderr, "mrtk (%s ver.%s)\n", MRTKLIB_SOFTNAME, MRTKLIB_VERSION_STRING);
        return 0;
    }

    for (i = 0; i < NSUBCMD; i++) {
        if (!strcmp(argv[1], subcmds[i].name)) {
            return subcmds[i].func(argc - 1, argv + 1);
        }
    }

    fprintf(stderr, "mrtk: unknown command '%s'\n\n", argv[1]);
    print_help();
    return 1;
}
