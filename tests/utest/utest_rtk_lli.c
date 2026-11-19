/* RTK cycle-slip state under LLI bits above LLI_HALFC (#160, upstream RTKLIB
 * #673). Includes the implementation to reach the private detslp_ll(). No
 * captured receiver data used.
 *
 * ssat[].slip[f] packs three fields: 0x03 the slip / half-cycle flags of the
 * epoch, 0xC0 the rover's previous LLI and 0x30 the base's (getbitu() counts
 * from the MSB). Each case checks the whole byte after both receivers were
 * processed, so a stray bit in any field fails.
 *
 * Raw decoders set LLI_HALFS (0x80, u-blox RXM-RAWX), LLI_HALFA (0x40,
 * NovAtel) and 0x04 (RT17, encrypted code). RINEX and BINEX input carry none
 * of them. */
#include "../../src/pos/mrtk_rtkpos.c"

static int failed = 0;

#define FWD 1.0
#define BWD (-1.0)

typedef struct {
    uint8_t rover;    /* rover LLI of the epoch */
    uint8_t base;     /* base LLI of the epoch */
    double tt;        /* FWD or BWD */
    uint8_t expected; /* ssat[].slip[0] after the epoch */
} epoch_t;

/* Runs one satellite on one frequency through the epochs as udbias() does:
 * clear the flags, rover, base, then relpos() saves the phase time. */
static void run(const char* name, const epoch_t* ep, int n) {
    rtk_t* rtk = calloc(1, sizeof(*rtk));
    obsd_t obs[2] = {{0}};
    gtime_t t = gpst2time(2445, 50000.0);
    int i, r;

    if (!rtk) {
        printf("FAIL %s: calloc\n", name);
        failed++;
        return;
    }
    rtk->opt.nf = 1;
    for (r = 0; r < 2; r++) {
        obs[r].sat = 1;
        obs[r].rcv = (uint8_t)(r + 1);
        obs[r].L[0] = 1.0e8;
    }
    for (i = 0; i < n; i++) {
        rtk->tt = ep[i].tt;
        t = timeadd(t, ep[i].tt);
        obs[0].time = obs[1].time = t;
        obs[0].LLI[0] = ep[i].rover;
        obs[1].LLI[0] = ep[i].base;

        rtk->ssat[0].slip[0] &= 0xFC;
        detslp_ll(rtk, obs, 0, 1);
        detslp_ll(rtk, obs, 1, 2);
        rtk->ssat[0].pt[0][0] = rtk->ssat[0].pt[1][0] = t;

        if (rtk->ssat[0].slip[0] != ep[i].expected) {
            printf("FAIL %s: epoch %d rover=%02x base=%02x %s slip=%02x expected=%02x\n", name, i, ep[i].rover,
                   ep[i].base, ep[i].tt > 0.0 ? "fwd" : "bwd", rtk->ssat[0].slip[0], ep[i].expected);
            failed++;
        }
    }
    free(rtk);
}

#define RUN(name, ep) run(name, ep, (int)(sizeof(ep) / sizeof((ep)[0])))

int main(void) {
    /* Constant half-cycle-subtracted flag, as u-blox reports it on a tracked
     * signal: used to read back as previous LLI 2 and flag a half-cycle slip
     * on every epoch after the first. */
    static const epoch_t rover_halfs[] = {
        {0x80, 0x00, FWD, 0x00}, {0x80, 0x00, FWD, 0x00}, {0x80, 0x00, FWD, 0x00}, {0x80, 0x00, FWD, 0x00}};
    /* The base's flag used to land in the rover's previous-LLI field. */
    static const epoch_t base_halfs[] = {
        {0x00, 0x80, FWD, 0x00}, {0x00, 0x80, FWD, 0x00}, {0x00, 0x80, FWD, 0x00}, {0x00, 0x80, FWD, 0x00}};
    static const epoch_t both_halfs[] = {{0x80, 0x80, FWD, 0x00}, {0x80, 0x80, FWD, 0x00}, {0x80, 0x80, FWD, 0x00}};

    /* Slip and half-cycle transitions under a constant 0x80 are still seen. */
    static const epoch_t rover_slip[] = {{0x80, 0x00, FWD, 0x00}, {0x81, 0x00, FWD, 0x41}, {0x80, 0x00, FWD, 0x00}};
    static const epoch_t rover_half[] = {
        {0x80, 0x00, FWD, 0x00}, {0x82, 0x00, FWD, 0x83}, {0x82, 0x00, FWD, 0x82}, {0x80, 0x00, FWD, 0x01}};
    static const epoch_t base_slip_half[] = {
        {0x00, 0x80, FWD, 0x00}, {0x00, 0x81, FWD, 0x11}, {0x00, 0x82, FWD, 0x23}, {0x00, 0x80, FWD, 0x01}};

    /* Half-cycle-added flag: stored as previous LLI 1 in the forward epoch, it
     * was a slip for the next epoch processed backward. */
    static const epoch_t rover_halfa_bwd[] = {{0x40, 0x00, FWD, 0x00}, {0x40, 0x00, BWD, 0x00}};
    static const epoch_t base_halfa_bwd[] = {{0x00, 0x40, FWD, 0x00}, {0x00, 0x40, BWD, 0x00}};
    /* A real slip of the forward epoch is still a slip going backward. */
    static const epoch_t rover_slip_bwd[] = {{0x41, 0x00, FWD, 0x41}, {0x40, 0x00, BWD, 0x01}};
    static const epoch_t base_slip_bwd[] = {{0x00, 0x41, FWD, 0x11}, {0x00, 0x40, BWD, 0x01}};

    /* BOC tracking and the remaining bits never caused a slip, but stayed in
     * the byte for good. */
    static const epoch_t boctrk[] = {
        {0x04, 0x04, FWD, 0x00}, {0x05, 0x04, FWD, 0x41}, {0x04, 0x04, FWD, 0x00}, {0xFC, 0xFC, FWD, 0x00}};

    /* No upper bits: unchanged behaviour. */
    static const epoch_t plain[] = {{0x00, 0x00, FWD, 0x00}, {0x01, 0x00, FWD, 0x41}, {0x00, 0x02, FWD, 0x23},
                                    {0x02, 0x02, FWD, 0xA3}, {0x02, 0x00, FWD, 0x83}, {0x00, 0x00, FWD, 0x01},
                                    {0x00, 0x00, FWD, 0x00}};

    RUN("rover LLI_HALFS constant", rover_halfs);
    RUN("base LLI_HALFS constant", base_halfs);
    RUN("rover and base LLI_HALFS constant", both_halfs);
    RUN("rover slip under LLI_HALFS", rover_slip);
    RUN("rover half-cycle transition under LLI_HALFS", rover_half);
    RUN("base slip and half-cycle transition under LLI_HALFS", base_slip_half);
    RUN("rover LLI_HALFA forward then backward", rover_halfa_bwd);
    RUN("base LLI_HALFA forward then backward", base_halfa_bwd);
    RUN("rover slip forward then backward", rover_slip_bwd);
    RUN("base slip forward then backward", base_slip_bwd);
    RUN("LLI_BOCTRK and unused bits", boctrk);
    RUN("no upper bits", plain);

    if (failed) {
        printf("%d check(s) failed\n", failed);
        return 1;
    }
    printf("utest_rtk_lli: all checks passed\n");
    return 0;
}
