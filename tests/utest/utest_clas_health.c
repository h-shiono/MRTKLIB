/* CLAS OSR satellite / signal health gate (#285), the health half of claslib
 * satsigexclude(). Includes the implementation to reach the private
 * clas_osr_healthexclude(). No captured receiver data used.
 *
 * QZS slots are laid out L1 / L5 / L2 as in MRTKLIB (claslib uses L1 / L2 /
 * L5), so a mask taken from the slot index instead of the band fails here. */
#include "../../src/clas/mrtk_clas_osr.c"

static int failed = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failed++;                                              \
        }                                                          \
    } while (0)

enum { NSLOT = NFREQ + NEXOBS };

static obsd_t qzs_obs(void) {
    obsd_t obs = {0};
    int j;
    obs.sat = satno(SYS_QZS, 194);
    obs.code[0] = CODE_L1C;
    obs.code[1] = CODE_L5Q;
    obs.code[2] = CODE_L2L;
#if NFREQ + NEXOBS > 3
    obs.code[3] = CODE_L1X;
#endif
    for (j = 0; j < NSLOT; j++) {
        obs.L[j] = 1.0e8 + j;
        obs.P[j] = 2.0e7 + j;
        obs.D[j] = (float)(100 + j);
        obs.SNR[j] = (uint16_t)(40000 + j);
    }
    return obs;
}

/* Slots reported zeroed by the last gate() call. */
static uint32_t masked;

static int gate(obsd_t* obs, int svh) { return clas_osr_healthexclude(obs, svh, &masked); }

/* Returns 1 when slot j still has its original L and P. */
static int kept(const obsd_t* obs, int j) { return obs->L[j] == 1.0e8 + j && obs->P[j] == 2.0e7 + j; }

/* Checks that exactly the slots of the given band mask (bit j = slot j) were
 * zeroed and reported, and that D / SNR were never touched. */
static void check_slots(const obsd_t* obs, unsigned zeroed) {
    int j;
    CHECK(masked == zeroed);
    for (j = 0; j < NSLOT; j++) {
        if (zeroed & (1u << j)) {
            CHECK(obs->L[j] == 0.0 && obs->P[j] == 0.0);
        } else {
            CHECK(kept(obs, j));
        }
        CHECK(obs->D[j] == (float)(100 + j) && obs->SNR[j] == (uint16_t)(40000 + j));
    }
}

int main(void) {
    obsd_t obs;
    unsigned l1 = 0x1u;
    int i;
    /* QZS health values that must not change anything: L1C/B (QZS-2/3/4 live),
     * L1C/A (QZS-1R/6 live) and L1C, alone and together */
    const int noop[] = {0x01, 0x10, 0x02, 0x13};

#if NFREQ + NEXOBS > 3
    l1 |= 0x8u; /* L1X in slot 3 */
#endif

    /* non-QZS: any non-zero health excludes the satellite */
    obs = qzs_obs();
    obs.sat = satno(SYS_GPS, 5);
    CHECK(gate(&obs, 0) == 0);
    check_slots(&obs, 0);
    CHECK(gate(&obs, 0x01) == 1);
    obs.sat = satno(SYS_GAL, 11);
    CHECK(gate(&obs, 0x01) == 1);
    CHECK(gate(&obs, 0x100) == 1);

    /* QZS: never excluded as a whole */
    for (i = 0; i < (int)(sizeof(noop) / sizeof(noop[0])); i++) {
        obs = qzs_obs();
        CHECK(gate(&obs, noop[i]) == 0);
        check_slots(&obs, 0);
    }
    obs = qzs_obs();
    CHECK(gate(&obs, 0x04) == 0); /* L5 -> slot 1 */
    check_slots(&obs, 0x2u);
    obs = qzs_obs();
    CHECK(gate(&obs, 0x08) == 0); /* L2C -> slot 2 */
    check_slots(&obs, 0x4u);
    obs = qzs_obs();
    CHECK(gate(&obs, 0x20) == 0); /* L1 -> every L1-band slot */
    check_slots(&obs, l1);
    obs = qzs_obs();
    CHECK(gate(&obs, 0x3F) == 0);
    check_slots(&obs, l1 | 0x2u | 0x4u);

    printf("CLAS health gate failures: %d\n", failed);
    return failed ? 1 : 0;
}
