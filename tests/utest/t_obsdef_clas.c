/*------------------------------------------------------------------------------
 * unit test : observation slot set-up for a run, mrtk_apply_obsdef() (#535)
 *
 * The CLAS PPP-RTK / VRS-RTK engines address observation slots, and the
 * claslib layout puts GPS / QZSS L5 and Galileo E5a in slot 2. This test
 * guards that, for correction = qzs-clas:
 *   - the layout is applied over the signals list (list order does not matter);
 *   - nf follows the highest slot 0-2 the listed bands occupy, so an L1 + L5
 *     list needs no placeholder signal, E5b (slot 4) does not raise it, and an
 *     nf given apart from the list is kept;
 *   - gps_frequency / qzs_frequency left unset (auto) resolve from the list:
 *     l1+l5 without L2, l1 for L1 only, unchanged when L2 is listed; an
 *     explicit value is kept;
 * and that loading options never touches the tables, while without the CLAS
 * flag the list order is kept as before.
 *-----------------------------------------------------------------------------*/
#include <stdio.h>

#include "mrtklib/mrtk_toml.h"
#include "mrtklib/rtklib.h"

#define FIXTURE "t_obsdef_clas_fixture.toml"

static int fails = 0;

#define CHECK(cond, msg)                 \
    do {                                 \
        if (!(cond)) {                   \
            printf("FAIL: %s\n", (msg)); \
            fails++;                     \
        } else {                         \
            printf("ok  : %s\n", (msg)); \
        }                                \
    } while (0)

static int slot(int sys, int idx) { return freq_idx2freq_num(sys, idx); }

/* load a TOML body; correction resolved to qzs-clas as resolve_correction() would */
static prcopt_t load(const char* body) {
    prcopt_t prcopt;
    solopt_t solopt;
    filopt_t filopt;
    FILE* fp = fopen(FIXTURE, "w");

    if (fp) {
        fprintf(fp, "[positioning]\nmode = \"ppp-rtk\"\n%s", body);
        fclose(fp);
    }
    resetsysopts();
    loadopts_toml(FIXTURE, sysopts);
    getsysopts(&prcopt, &solopt, &filopt);
    remove(FIXTURE);
    prcopt.correction = CORR_QZS_CLAS;
    return prcopt;
}

int main(void) {
    prcopt_t opt;
    const int flags = MRTK_OBSDEF_PPPSIG | MRTK_OBSDEF_CLAS;

    /* L1 + L5 receiver (DATAGNSS MA-10P, #585) */
    opt = load("signals = [\"G1C\", \"G5Q\", \"E1X\", \"E5Q\", \"J1C\", \"J5Q\"]\n");
    CHECK(opt.sigcfg_set && opt.nf == 2, "L1+L5 list: nf 2 from the signal count");
    mrtk_apply_obsdef(&opt, flags);
    CHECK(slot(SYS_GPS, 1) == 2 && slot(SYS_GPS, 2) == 5, "L1+L5 list: GPS L5 in slot 2 (claslib layout)");
    CHECK(slot(SYS_QZS, 2) == 5 && slot(SYS_GAL, 2) == 5, "L1+L5 list: QZS L5 / GAL E5a in slot 2");
    CHECK(opt.nf == 3, "L1+L5 list: nf 3 without a placeholder");
    CHECK(opt.posopt[10] == 3 && opt.posopt[12] == 3, "L1+L5 list: gps/qzs_frequency auto -> l1+l5");

    /* bundled CLAS list: L2 listed, E5b at slot 4 */
    opt = load("signals = [\"G1C\", \"G2W\", \"E1C\", \"E5Q\", \"E7Q\", \"J1C\", \"J5Q\", \"J2X\"]\n");
    mrtk_apply_obsdef(&opt, flags);
    CHECK(opt.nf == 3, "bundled list: nf 3 (E5b in slot 4 ignored)");
    CHECK(opt.posopt[10] == 0 && opt.posopt[12] == 0, "bundled list: L2 listed, frequency pair left at the default");

    /* F9P list: L1 + L2, Galileo E1 + E5b */
    opt = load("signals = [\"G1C\", \"G2L\", \"E1C\", \"E7Q\", \"J1C\", \"J2L\"]\n");
    mrtk_apply_obsdef(&opt, flags);
    CHECK(opt.nf == 2, "F9P list: nf 2");

    /* single frequency */
    opt = load("signals = [\"G1C\", \"E1C\", \"J1C\"]\n");
    mrtk_apply_obsdef(&opt, flags);
    CHECK(opt.nf == 1 && opt.posopt[10] == 1 && opt.posopt[12] == 1, "L1 list: nf 1, frequency pair l1");

    /* explicit values are kept */
    opt = load(
        "signals = [\"G1C\", \"G5Q\", \"J1C\", \"J5Q\"]\n"
        "[positioning.corrections]\ngps_frequency = \"l1+l2\"\nqzs_frequency = \"auto\"\n");
    CHECK(opt.posopt[10] == 2 && opt.posopt[12] == 0, "gps_frequency l1+l2 and qzs_frequency auto parse");
    opt.nf = 1; /* as mrtk post -f 1 */
    mrtk_apply_obsdef(&opt, flags);
    CHECK(opt.posopt[10] == 2 && opt.posopt[12] == 3, "explicit gps_frequency kept, auto qzs_frequency resolved");
    CHECK(opt.nf == 1, "nf given apart from the signals list kept");

    /* loading options does not touch the tables */
    opt = load("signals = [\"G1C\", \"G5Q\", \"E1X\", \"E5Q\", \"J1C\", \"J5Q\"]\n");
    mrtk_apply_obsdef(&opt, flags);
    opt = load("signals = [\"G1C\", \"G2W\", \"E1C\", \"E7Q\", \"J1C\", \"J2X\"]\n");
    CHECK(slot(SYS_GPS, 2) == 5 && slot(SYS_GAL, 2) == 5, "getsysopts leaves the running tables alone");

    /* without the CLAS flag: list order, nf and frequency pair untouched */
    opt = load("signals = [\"G1C\", \"G5Q\", \"E1X\", \"E5Q\", \"J1C\", \"J5Q\"]\n");
    mrtk_apply_obsdef(&opt, MRTK_OBSDEF_PPPSIG);
    CHECK(slot(SYS_GPS, 1) == 5 && slot(SYS_GAL, 1) == 5, "no CLAS flag: list order (L5 / E5a in slot 1)");
    CHECK(opt.nf == 2 && opt.posopt[10] == 0, "no CLAS flag: nf and frequency pair unchanged");

    /* CLAS run followed by a run with another correction and no signals list */
    opt = load("signals = [\"G1C\", \"G5Q\", \"E1X\", \"E5Q\", \"J1C\", \"J5Q\"]\n");
    mrtk_apply_obsdef(&opt, flags);
    opt = load("");
    opt.correction = CORR_IGS;
    mrtk_apply_obsdef(&opt, flags);
    CHECK(slot(SYS_GAL, 1) == 5 && slot(SYS_GAL, 2) == 7, "CLAS then igs: default tables restored");

    if (fails) {
        printf("%d check(s) FAILED\n", fails);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
