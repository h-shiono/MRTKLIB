/*------------------------------------------------------------------------------
 * t_toml.c : unit test for the TOML loader deprecation aliases and unknown-key
 *            detection (#286).
 *
 * Uses explicit failure returns (not assert) so the checks run even in
 * NDEBUG builds where assert() is a no-op (see #277).
 *-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "mrtklib/mrtk_options.h"
#include "mrtklib/mrtk_toml.h"
#include "mrtklib/rtklib.h"

#define FIXTURE "t_toml_fixture.toml"
#define CAPTURE "t_toml_stderr.txt"

static int fail = 0;

static void expect(int cond, const char* what) {
    printf("%s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) {
        fail = 1;
    }
}

static int capture_count(const char* needle) {
    char buf[8192];
    size_t n;
    FILE* fp = fopen(CAPTURE, "r");
    const char* p;
    int count = 0;
    if (!fp) {
        return 0;
    }
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    for (p = buf; (p = strstr(p, needle)) != NULL; p += strlen(needle)) {
        count++;
    }
    return count;
}

static int capture_contains(const char* needle) { return capture_count(needle) > 0; }

int main(void) {
    prcopt_t prcopt;
    solopt_t solopt;
    filopt_t filopt;
    FILE* fp;

    /* fixture: deprecated aliases that must apply, one deprecated key shadowed
     * by its new location (new must win), one typo (unknown), and the special
     * string-list / integer keys whose types changed in the redesign. */
    fp = fopen(FIXTURE, "w");
    if (!fp) {
        printf("FAIL: cannot write fixture\n");
        return 1;
    }
    fprintf(fp,
            "[receiver]\n"
            "phase_shift = \"table\"\n"     /* alias -> pos2-phasshft -> phasshft=1 */
            "max_age = 99.0\n"              /* deprecated, but shadowed below */
            "uncorr_bias = true\n"          /* alias -> pos2-uncorrbias -> unbias=1 */
            "iono_correction = \"maybe\"\n" /* deprecated + invalid enum value */
            "[server]\n"
            "regularly = 600\n" /* alias -> regularly=600 */
            "[adaptive_filter]\n"
            "enabled = true\n" /* alias (#287 nesting) -> pos2-prnadpt -> prnadpt=1 */
            "[positioning.relative]\n"
            "max_age = 5.0\n" /* new location wins over [receiver].max_age */
            "[positioning]\n"
            "elevaton_mask = 15.0\n" /* typo -> unknown key */
            "systems = [\"GPS\", \"GAL\"]\n"
            "excluded_sats = [\"G05\"]\n"
            "[positioning.corrections]\n"
            "snr_fixed = 45\n");
    fclose(fp);

    /* Capture loader warnings. stdout stays on the console for PASS/FAIL. */
    if (!freopen(CAPTURE, "w", stderr)) {
        printf("FAIL: cannot redirect stderr\n");
        return 1;
    }

    resetsysopts();
    loadopts_toml(FIXTURE, sysopts);
    getsysopts(&prcopt, &solopt, &filopt);
    fflush(stderr);

    /* Aliases applied their values. */
    expect(prcopt.phasshft == 1, "[receiver].phase_shift alias sets phasshft=1");
    expect(prcopt.regularly == 600, "[server].regularly alias sets regularly=600");
    expect(prcopt.prnadpt == 1, "[adaptive_filter].enabled alias sets prnadpt=1");
    expect(prcopt.unbias == 1, "[receiver].uncorr_bias alias sets unbias=1");
    /* New location wins over the deprecated one. */
    expect(prcopt.maxtdiff == 5.0, "[positioning.relative].max_age wins over [receiver].max_age");

    /* Keys whose types changed in the redesign parse into the right options. */
    expect(prcopt.navsys == (SYS_GPS | SYS_GAL), "systems string list sets the navsys mask");
    expect(satid2no("G05") > 0 && prcopt.exsats[satid2no("G05") - 1] == 1,
           "excluded_sats string list marks the satellite excluded");
    expect(prcopt.posopt[11] == 45, "snr_fixed integer parses into posopt[11]");

    /* Warnings emitted. */
    expect(capture_contains("[receiver].phase_shift is deprecated; move it to"),
           "deprecation warning for [receiver].phase_shift");
    expect(capture_contains("[server].regularly is deprecated; move it to"),
           "deprecation warning for [server].regularly");
    expect(capture_contains("[adaptive_filter].enabled is deprecated; move it to"),
           "deprecation warning for [adaptive_filter].enabled");
    expect(capture_contains("[receiver].max_age is deprecated and ignored"),
           "shadowed-alias warning for [receiver].max_age");
    expect(capture_contains("[receiver].uncorr_bias is deprecated; move it to"),
           "deprecation warning for [receiver].uncorr_bias");
    expect(capture_contains("unknown key [positioning].elevaton_mask"), "unknown-key warning for typo");
    expect(capture_contains("invalid value for receiver.iono_correction"),
           "invalid-value warning for a deprecated key with a bad value");
    /* The specially-handled string-list keys must not trip the unknown-key sweep. */
    expect(capture_count("unknown key [positioning].systems") == 0 &&
               capture_count("unknown key [positioning].excluded_sats") == 0,
           "systems / excluded_sats are not flagged as unknown keys");

    /* rtkrcv loads the same file twice per logical load (rcvopts, then
     * sysopts); the unknown-key sweep must not run again for an unchanged
     * file, so the typo warning stays single. */
    loadopts_toml(FIXTURE, sysopts);
    fflush(stderr);
    expect(capture_count("unknown key [positioning].elevaton_mask") == 1,
           "unknown-key sweep runs once for an unchanged file loaded twice");

    remove(FIXTURE);
    remove(CAPTURE);
    return fail;
}
