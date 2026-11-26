#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include "mrtklib/mrtk_antenna.h"
#include "mrtklib/mrtk_const.h"

int main(void) {
    pcvs_t pcvs = {0};
    int i, slot = freq_num2ant_idx(SYS_GPS, 1);

    assert(readpcv("antenna_grid.atx", &pcvs));
    assert(pcvs.n == 2);
    assert(pcvs.pcv[0].sat == satid2no("G01"));
    assert(pcvs.pcv[1].sat == 0);

    for (i = 0; i < 19; i++) {
        double zenith = fmin(i * 5.0, 80.0);

        /* Satellite output uses 1-degree steps. */
        assert(fabs(pcvs.pcv[0].var[slot][i] - i * 0.01) < 1E-12);

        /* Receiver output uses 5-degree steps and holds the 80-degree value. */
        assert(fabs(pcvs.pcv[1].var[slot][i] - zenith * 0.01) < 1E-12);
    }
    free(pcvs.pcv);
    return 0;
}
