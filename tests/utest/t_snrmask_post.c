/*------------------------------------------------------------------------------
 * t_snrmask_post.c : postpos() with the SNR mask on and no C/N0 (#477)
 *
 * The RTKLIB sample RINEX files (07590920.05o rover, 30400920.05o base) carry
 * no S observables. Before #477 the mask treated their C/N0 as 0 dBHz and
 * rejected every satellite: no solution, no warning. Checks:
 *   - rover mask on, 4 sessions (tu=600): solutions, rover warning once
 *   - a second postpos() call warns again (latch reset per call)
 *   - mask off: solutions, no warning
 *   - base mask on (dgps): solutions, base warning once, no rover warning
 *
 * usage: t_snrmask_post <rinex dir> (run in a scratch working directory)
 *-----------------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "mrtklib/mrtklib.h"
#include "mrtklib/rtklib.h"

static int fails = 0;

#define CHECK(cond, msg)                 \
    do {                                 \
        if (!(cond)) {                   \
            printf("FAIL: %s\n", (msg)); \
            fails++;                     \
        }                                \
    } while (0)

/* callbacks the library expects from the application (as in mrtk_main.c) */
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

static const char* ROVER_WARN = "SNR mask is enabled for the rover but its observations have no C/N0";
static const char* BASE_WARN = "SNR mask is enabled for the base but its observations have no C/N0";

/* count occurrences of str in a file */
static int count_in_file(const char* path, const char* str) {
    static char buf[1 << 20];
    FILE* fp;
    size_t n;
    int cnt = 0;
    const char* p;

    if (!(fp = fopen(path, "rb"))) {
        return -1;
    }
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    for (p = buf; (p = strstr(p, str)) != NULL; p += strlen(str)) {
        cnt++;
    }
    return cnt;
}

/* count solution lines in a .pos file */
static int count_sol(const char* path) {
    char line[1024];
    FILE* fp;
    int cnt = 0;

    if (!(fp = fopen(path, "r"))) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] != '%' && line[0] != '\n') {
            cnt++;
        }
    }
    fclose(fp);
    return cnt;
}

/* run postpos() with stderr captured to errfile; return number of solutions */
static int run(const char* name, mrtk_ctx_t* ctx, const prcopt_t* popt, double tu, char** infile, int n,
               const char* errfile) {
    double ep0[] = {2005, 4, 2, 0, 0, 0}, ep1[] = {2005, 4, 2, 0, 39, 30};
    solopt_t sopt = solopt_default;
    filopt_t fopt;
    char out[256];

    memset(&fopt, 0, sizeof(fopt));
    snprintf(out, sizeof(out), "snrmask_%s.pos", name);
    remove(out);
    fflush(stderr);
    if (!freopen(errfile, "w", stderr)) {
        printf("FAIL: cannot redirect stderr to %s\n", errfile);
        fails++;
        return 0;
    }
    if (postpos(ctx, epoch2time(ep0), epoch2time(ep1), 0.0, tu, popt, &sopt, &fopt, infile, n, out, "", "") != 0) {
        printf("FAIL: %s: postpos() returned an error\n", name);
        fails++;
    }
    fflush(stderr);
    return count_sol(out);
}

int main(int argc, char** argv) {
    char rov[1024], bas[1024], nav[1024], *infile[3], msg[256];
    prcopt_t popt = prcopt_default;
    mrtk_ctx_t* ctx;
    int f, k, nsol;

    if (argc < 2) {
        printf("usage: t_snrmask_post <rinex dir>\n");
        return 2;
    }
    snprintf(rov, sizeof(rov), "%s/07590920.05o", argv[1]);
    snprintf(bas, sizeof(bas), "%s/30400920.05o", argv[1]);
    snprintf(nav, sizeof(nav), "%s/07590920.05n", argv[1]);

    ctx = mrtk_ctx_create();
    g_mrtk_ctx = ctx;
    g_mrtk_legacy_ctx = mrtk_context_new();

    popt.mode = PMODE_SINGLE;
    popt.navsys = SYS_GPS;
    popt.elmin = 15.0 * D2R;
    popt.refpos = POSOPT_RINEX;
    for (f = 0; f < NFREQ; f++) {
        for (k = 0; k < 9; k++) {
            popt.snrmask.mask[f][k] = 30.0;
        }
    }

    /* rover mask on, 4 sessions of 600 s: one warning per postpos() call */
    popt.snrmask.ena[0] = 1;
    infile[0] = rov;
    infile[1] = nav;
    nsol = run("sessions", ctx, &popt, 600.0, infile, 2, "snrmask_sessions.err");
    snprintf(msg, sizeof(msg), "rover, 4 sessions: %d solutions (want > 0)", nsol);
    CHECK(nsol > 0, msg);
    CHECK(count_in_file("snrmask_sessions.err", ROVER_WARN) == 1, "rover, 4 sessions: rover warning once");

    /* second postpos() call warns again */
    nsol = run("again", ctx, &popt, 0.0, infile, 2, "snrmask_again.err");
    CHECK(nsol > 0, "second call: solutions");
    CHECK(count_in_file("snrmask_again.err", ROVER_WARN) == 1, "second call: rover warning again");

    /* mask off: no warning */
    popt.snrmask.ena[0] = 0;
    nsol = run("off", ctx, &popt, 0.0, infile, 2, "snrmask_off.err");
    CHECK(nsol > 0, "mask off: solutions");
    CHECK(count_in_file("snrmask_off.err", ROVER_WARN) == 0, "mask off: no rover warning");
    CHECK(count_in_file("snrmask_off.err", BASE_WARN) == 0, "mask off: no base warning");

    /* base mask on, dgps: base warning only */
    popt.mode = PMODE_DGPS;
    popt.snrmask.ena[1] = 1;
    infile[1] = bas;
    infile[2] = nav;
    nsol = run("base", ctx, &popt, 0.0, infile, 3, "snrmask_base.err");
    CHECK(nsol > 0, "base: solutions");
    CHECK(count_in_file("snrmask_base.err", BASE_WARN) == 1, "base: base warning once");
    CHECK(count_in_file("snrmask_base.err", ROVER_WARN) == 0, "base: no rover warning");

    mrtk_context_free(g_mrtk_legacy_ctx);
    g_mrtk_legacy_ctx = NULL;
    g_mrtk_ctx = NULL;
    mrtk_ctx_destroy(ctx);

    if (fails) {
        printf("t_snrmask_post: %d failure(s)\n", fails);
        return 1;
    }
    printf("t_snrmask_post: OK\n");
    return 0;
}
