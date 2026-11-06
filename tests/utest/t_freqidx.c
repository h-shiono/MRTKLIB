/*------------------------------------------------------------------------------
 * unit test : code2freq_idx() obs-slot contract for RINEX conversion (#71)
 *
 * #71 unifies the converter (convrnx) onto code2freq_idx(), the obsdef-based
 * mapping already used by every receiver decoder, the RINEX parser, RTCM3 and
 * CLAS. This test pins the resulting (sys,code) -> frequency-index contract so
 * the converter's column ordering and signal preservation cannot regress.
 *
 * Two properties matter for the converter:
 *   - ordering: indices follow the obsdef table order, NOT the legacy fixed
 *     per-band order (e.g. Galileo E5a precedes E5b, QZSS L5 precedes L2);
 *   - preservation: a band the receiver can decode survives conversion only if
 *     it owns an obsdef slot. Bands with no slot return -1 and are dropped by
 *     setopt_obstype() — a documented converter known-limitation. GLONASS G3
 *     (CDMA, GLONASS-K2 only) is the canonical case: obsdef_GLO has no G3 row
 *     (adding one perturbs GLONASS positioning, so it is intentionally absent).
 *
 * Uses explicit checks (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <stdio.h>

#include "mrtklib/rtklib.h"

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

/* frequency index for a (system, RINEX obs-code string) pair */
static int idx(int sys, const char* obs) { return code2freq_idx(sys, obs2code(obs)); }
static int pri(int sys, const char* obs, const char* opt) { return getcodepri(sys, obs2code(obs), opt); }

int main(void) {
    reset_obsdef(); /* pristine multi-band defaults */

    /* GPS: L1=0, L2=1, L5=2 (unchanged by #71) */
    CHECK(idx(SYS_GPS, "1C") == 0 && idx(SYS_GPS, "2W") == 1 && idx(SYS_GPS, "5Q") == 2, "GPS L1/L2/L5 -> 0/1/2");

    /* Galileo: E1=0, E5a=1, E5b=2, E6=3 — obsdef order, E5a BEFORE E5b */
    CHECK(idx(SYS_GAL, "1C") == 0, "GAL E1 -> 0");
    CHECK(idx(SYS_GAL, "5Q") == 1, "GAL E5a -> 1 (was 2 under fixed band order)");
    CHECK(idx(SYS_GAL, "7Q") == 2, "GAL E5b -> 2 (was 1 under fixed band order)");
    CHECK(idx(SYS_GAL, "6C") == 3, "GAL E6 -> 3");

    /* QZSS: L1=0, L5=1, L2=2, L6=3 — obsdef order, L5 BEFORE L2 */
    CHECK(idx(SYS_QZS, "1C") == 0, "QZS L1 -> 0");
    CHECK(idx(SYS_QZS, "5Q") == 1, "QZS L5 -> 1 (was 2 under fixed band order)");
    CHECK(idx(SYS_QZS, "2L") == 2, "QZS L2 -> 2 (was 1 under fixed band order)");
    CHECK(idx(SYS_QZS, "6L") == 3, "QZS L6 -> 3");
    CHECK(pri(SYS_QZS, "1L", "") > pri(SYS_QZS, "1X", "") && pri(SYS_QZS, "1X", "") > pri(SYS_QZS, "1S", "") &&
              pri(SYS_QZS, "1S", "") > pri(SYS_QZS, "1C", ""),
          "QZS L1 default priority keeps MRTK order L>X>S>C");
    mrtk_use_claslib_qzs_l1_priority(1);
    CHECK(pri(SYS_QZS, "1C", "") > pri(SYS_QZS, "1S", "") && pri(SYS_QZS, "1S", "") > pri(SYS_QZS, "1L", "") &&
              pri(SYS_QZS, "1L", "") > pri(SYS_QZS, "1X", "") && pri(SYS_QZS, "1X", "") > pri(SYS_QZS, "1Z", ""),
          "QZS L1 claslib override follows C>S>L>X>Z");
    CHECK(pri(SYS_QZS, "1X", "-JL1X") == 15 && pri(SYS_QZS, "1C", "-JL1X") == 0,
          "QZS L1 priority option -JL1X overrides claslib order");
    mrtk_use_claslib_qzs_l1_priority(0);
    CHECK(pri(SYS_QZS, "1L", "") > pri(SYS_QZS, "1C", ""), "QZS L1 priority restored after claslib override off");

    /* BDS: B1I=0, B3=1, B2b=2, B1C=3, B2a=4 — obsdef order */
    CHECK(idx(SYS_CMP, "2I") == 0, "BDS B1I -> 0");
    CHECK(idx(SYS_CMP, "6I") == 1, "BDS B3 -> 1");
    CHECK(idx(SYS_CMP, "7I") == 2, "BDS B2b -> 2");
    CHECK(idx(SYS_CMP, "1P") == 3, "BDS B1C -> 3");
    CHECK(idx(SYS_CMP, "5P") == 4, "BDS B2a -> 4");

    /* GLONASS: G1=0, G2=1; G3 has no obsdef slot -> -1 (known limitation:
     * converter does not emit GLONASS G3, see #71. Adding a G3 slot to
     * obsdef_GLO perturbs GLONASS positioning, so it is intentionally absent.) */
    CHECK(idx(SYS_GLO, "1C") == 0, "GLO G1 -> 0");
    CHECK(idx(SYS_GLO, "2C") == 1, "GLO G2 -> 1");
    CHECK(idx(SYS_GLO, "3Q") == -1, "GLO G3 -> -1 (no obsdef slot; dropped by convrnx, known limitation)");

    if (fails) {
        printf("\n%d check(s) FAILED\n", fails);
        return 1;
    }
    printf("\nall code2freq_idx #71 checks passed\n");
    return 0;
}
