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

static int capture_contains(const char* needle) {
    char buf[8192];
    size_t n;
    FILE* fp = fopen(CAPTURE, "r");
    int found = 0;
    if (!fp) {
        return 0;
    }
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    found = strstr(buf, needle) != NULL;
    return found;
}

int main(void) {
    prcopt_t prcopt;
    solopt_t solopt;
    filopt_t filopt;
    FILE* fp;

    /* fixture: two deprecated aliases that must apply, one deprecated key
     * shadowed by its new location (new must win), and one typo (unknown). */
    fp = fopen(FIXTURE, "w");
    if (!fp) {
        printf("FAIL: cannot write fixture\n");
        return 1;
    }
    fprintf(fp,
            "[receiver]\n"
            "phase_shift = \"table\"\n"     /* alias -> pos2-phasshft -> phasshft=1 */
            "max_age = 99.0\n"              /* deprecated, but shadowed below */
            "iono_correction = \"maybe\"\n" /* deprecated + invalid enum value */
            "[server]\n"
            "regularly = 600\n" /* alias -> regularly=600 */
            "[adaptive_filter]\n"
            "enabled = true\n" /* alias (#287 nesting) -> pos2-prnadpt -> prnadpt=1 */
            "[positioning.relative]\n"
            "max_age = 5.0\n" /* new location wins over [receiver].max_age */
            "[positioning]\n"
            "elevaton_mask = 15.0\n"); /* typo -> unknown key */
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
    /* New location wins over the deprecated one. */
    expect(prcopt.maxtdiff == 5.0, "[positioning.relative].max_age wins over [receiver].max_age");

    /* Warnings emitted. */
    expect(capture_contains("[receiver].phase_shift is deprecated; move it to"),
           "deprecation warning for [receiver].phase_shift");
    expect(capture_contains("[server].regularly is deprecated; move it to"),
           "deprecation warning for [server].regularly");
    expect(capture_contains("[adaptive_filter].enabled is deprecated; move it to"),
           "deprecation warning for [adaptive_filter].enabled");
    expect(capture_contains("[receiver].max_age is deprecated and ignored"),
           "shadowed-alias warning for [receiver].max_age");
    expect(capture_contains("unknown key [positioning].elevaton_mask"), "unknown-key warning for typo");
    expect(capture_contains("invalid value for receiver.iono_correction"),
           "invalid-value warning for a deprecated key with a bad value");

    remove(FIXTURE);
    remove(CAPTURE);
    return fail;
}
