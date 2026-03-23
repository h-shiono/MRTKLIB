/*------------------------------------------------------------------------------
 * mrtk_rcv_septentrio.c : Septentrio receiver raw data decoder
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2015- Mitsubishi Electric Corp.
 * Copyright (C) 2014 Geospatial Information Authority of Japan
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_madoca.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_rcvraw.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_sys.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_trace.h"

/* Local constants (duplicated from rtklib.h to avoid dependency) */
#define PI 3.1415926535897932  /* pi */
#define D2R (PI / 180.0)       /* deg to rad */
#define R2D (180.0 / PI)       /* rad to deg */
#define CLIGHT 299792458.0     /* speed of light (m/s) */
#define SC2RAD 3.1415926535898 /* semi-circle to radian (IS-GPS) */

#define SYS_NONE 0x00
#define SYS_GPS 0x01
#define SYS_SBS 0x02
#define SYS_GLO 0x04
#define SYS_GAL 0x08
#define SYS_QZS 0x10
#define SYS_CMP 0x20
#define SYS_IRN 0x40

#define TSYS_GPS 0
#define TSYS_UTC 1
#define TSYS_GLO 2
#define TSYS_GAL 3
#define TSYS_QZS 4
#define TSYS_CMP 5
#define TSYS_IRN 6

#define FREQ1 1.57542E9       /* L1/E1/B1C  frequency (Hz) */
#define FREQ2 1.22760E9       /* L2         frequency (Hz) */
#define FREQ5 1.17645E9       /* L5/E5a/B2a frequency (Hz) */
#define FREQ6 1.27875E9       /* E6/L6      frequency (Hz) */
#define FREQ7 1.20714E9       /* E5b/B2I    frequency (Hz) */
#define FREQ8 1.191795E9      /* E5ab/B2ab  frequency (Hz) */
#define FREQ9 2.492028E9      /* S          frequency (Hz) */
#define FREQ1_GLO 1.60200E9   /* GLONASS G1 base frequency (Hz) */
#define DFRQ1_GLO 0.56250E6   /* GLONASS G1 bias frequency (Hz/n) */
#define FREQ2_GLO 1.24600E9   /* GLONASS G2 base frequency (Hz) */
#define DFRQ2_GLO 0.43750E6   /* GLONASS G2 bias frequency (Hz/n) */
#define FREQ3_GLO 1.202025E9  /* GLONASS G3 frequency (Hz) */
#define FREQ1a_GLO 1.600995E9 /* GLONASS G1a frequency (Hz) */
#define FREQ2a_GLO 1.248060E9 /* GLONASS G2a frequency (Hz) */
#define FREQ1_CMP 1.561098E9  /* BDS B1I    frequency (Hz) */

#define MINPRNGPS 1
#define MAXPRNGPS 32
#define MINPRNSBS 120
#define MAXPRNSBS 158

/* Power-of-2 constants */
#define P2_5 0.03125
#define P2_6 0.015625
#define P2_11 4.882812500000000E-04
#define P2_15 3.051757812500000E-05
#define P2_17 7.629394531250000E-06
#define P2_19 1.907348632812500E-06
#define P2_20 9.536743164062500E-07
#define P2_21 4.768371582031250E-07
#define P2_23 1.192092895507810E-07
#define P2_24 5.960464477539063E-08
#define P2_27 7.450580596923828E-09
#define P2_29 1.862645149230957E-09
#define P2_30 9.313225746154785E-10
#define P2_31 4.656612873077393E-10
#define P2_32 2.328306436538696E-10
#define P2_33 1.164153218269348E-10
#define P2_34 5.820766091346740E-11
#define P2_35 2.910383045673370E-11
#define P2_38 3.637978807091710E-12
#define P2_40 9.094947017729280E-13
#define P2_43 1.136868377216160E-13
#define P2_46 1.421085471520200E-14
#define P2_50 8.881784197001252E-16
#define P2_55 2.775557561562891E-17
#define P2_59 1.734723475976810E-18

/* Forward declarations for functions in rtklib (resolved at link time) */

#define SBF_SYNC1 0x24 /* SBF block header 1 */
#define SBF_SYNC2 0x40 /* SBF block header 2 */
#define SBF_MAXSIG 40  /* SBF max signal number */

#define SBF_MEASEPOCH 4027  /* SBF GNSS measurements */
#define SBF_MEASEXTRA 4000  /* SBF GNSS measurements extra info */
#define SBF_GPSRAWCA 4017   /* SBF GPS C/A subframe */
#define SBF_GLORAWCA 4026   /* SBF GLONASS L1CA or L2CA navigation string */
#define SBF_GALRAWFNAV 4022 /* SBF Galileo F/NAV navigation page */
#define SBF_GALRAWINAV 4023 /* SBF Galileo I/NAV navigation page */
#define SBF_GEORAWL1 4020   /* SBF SBAS L1 navigation frame */
#define SBF_BDSRAW 4047     /* SBF BDS navigation page */
#define SBF_QZSRAWL1CA 4066 /* SBF QZSS C/A subframe */
#define SBF_QZSRAWL6 4069   /* SBF QZSS L6 message */
#define SBF_QZSRAWL6D 4270  /* SBF QZSS L6D message (mosaic-G5) */
#define SBF_NAVICRAW 4093   /* SBF NavIC/IRNSS subframe */

/* Decoded navigation blocks */
#define SBF_GPSNAV   5891   /* SBF GPS decoded ephemeris */
#define SBF_GLONAV   4004   /* SBF GLONASS decoded ephemeris */
#define SBF_GALNAV   4002   /* SBF Galileo decoded ephemeris */
#define SBF_BDSNAV   4081   /* SBF BDS decoded ephemeris */
#define SBF_QZSSNAV  4095   /* SBF QZSS decoded ephemeris */

/* Position blocks */
#define SBF_PVTGEOD  4007   /* SBF PVTGeodetic */

/* get fields (little-endian) ------------------------------------------------*/
#define U1(p) (*((uint8_t*)(p)))
#define I1(p) (*((int8_t*)(p)))
static uint16_t U2(uint8_t* p) {
    uint16_t a;
    memcpy(&a, p, 2);
    return a;
}
static uint32_t U4(uint8_t* p) {
    uint32_t a;
    memcpy(&a, p, 4);
    return a;
}
static int32_t I4(uint8_t* p) {
    int32_t a;
    memcpy(&a, p, 4);
    return a;
}
static int16_t I2(uint8_t* p) {
    int16_t a;
    memcpy(&a, p, 2);
    return a;
}
static float R4(const uint8_t* p) {
    float r;
    memcpy(&r, p, 4);
    return r;
}
static double R8(const uint8_t* p) {
    double r;
    memcpy(&r, p, 8);
    return r;
}

/* convert N-bit week number to full week ------------------------------------*/
static uint16_t adjust_WN10(uint16_t ref_WN, uint16_t WN) {
    int16_t offset = (ref_WN % 1024) - WN;
    if (offset >  512) offset -= 1024;
    if (offset < -511) offset += 1024;
    return ref_WN + offset;
}
static uint16_t adjust_WN12(uint16_t ref_WN, uint16_t WN) {
    int16_t offset = (ref_WN % 4096) - WN;
    if (offset >  2048) offset -= 4096;
    if (offset < -2047) offset += 4096;
    return ref_WN + offset;
}
static uint16_t adjust_WN14(uint16_t ref_WN, uint16_t WN) {
    int16_t offset = (ref_WN % 8192) - WN;
    if (offset >  4096) offset -= 8192;
    if (offset < -4095) offset += 8192;
    return ref_WN + offset;
}

/* svid to satellite number ([1] 4.1.9) --------------------------------------*/
static int svid2sat(int svid) {
    if (svid <= 37) {
        return satno(SYS_GPS, svid);
    }
    if (svid <= 61) {
        return satno(SYS_GLO, svid - 37);
    }
    if (svid <= 62) {
        return 0; /* glonass unknown slot */
    }
    if (svid <= 68) {
        return satno(SYS_GLO, svid - 38);
    }
    if (svid <= 70) {
        return 0;
    }
    if (svid <= 106) {
        return satno(SYS_GAL, svid - 70);
    }
    if (svid <= 119) {
        return 0;
    }
    if (svid <= 140) {
        return satno(SYS_SBS, svid);
    }
    if (svid <= 180) {
        return satno(SYS_CMP, svid - 140);
    }
    if (svid <= 187) {
        return satno(SYS_QZS, svid - 180 + 192);
    }
    if (svid <= 190) {
        return 0;
    }
    if (svid <= 197) {
        return satno(SYS_IRN, svid - 190);
    }
    if (svid <= 215) {
        return satno(SYS_SBS, svid - 57);
    }
    if (svid <= 222) {
        return satno(SYS_IRN, svid - 208);
    }
    if (svid <= 245) {
        return satno(SYS_CMP, svid - 182);
    }
    return 0; /* error */
}
/* signal number table ([1] 4.1.10) ------------------------------------------*/
static uint8_t sig_tbl[SBF_MAXSIG + 1][2] = {
    /* system, obs-code */
    {SYS_GPS, CODE_L1C}, /*  0: GPS L1C/A */
    {SYS_GPS, CODE_L1W}, /*  1: GPS L1P */
    {SYS_GPS, CODE_L2W}, /*  2: GPS L2P */
    {SYS_GPS, CODE_L2L}, /*  3: GPS L2C */
    {SYS_GPS, CODE_L5Q}, /*  4: GPS L5 */
    {SYS_GPS, CODE_L1L}, /*  5: GPS L1C */
    {SYS_QZS, CODE_L1C}, /*  6: QZS L1C/A */
    {SYS_QZS, CODE_L2L}, /*  7: QZS L2C */
    {SYS_GLO, CODE_L1C}, /*  8: GLO L1C/A */
    {SYS_GLO, CODE_L1P}, /*  9: GLO L1P */
    {SYS_GLO, CODE_L2P}, /* 10: GLO L2P */
    {SYS_GLO, CODE_L2C}, /* 11: GLO L2C/A */
    {SYS_GLO, CODE_L3Q}, /* 12: GLO L3 */
    {SYS_CMP, CODE_L1P}, /* 13: BDS B1C */
    {SYS_CMP, CODE_L5P}, /* 14: BDS B2a */
    {SYS_IRN, CODE_L5A}, /* 15: IRN L5 */
    {0, 0},              /* 16: reserved */
    {SYS_GAL, CODE_L1C}, /* 17: GAL E1(L1BC) */
    {0, 0},              /* 18: reserved */
    {SYS_GAL, CODE_L6C}, /* 19: GAL E6(E6BC) */
    {SYS_GAL, CODE_L5Q}, /* 20: GAL E5a */
    {SYS_GAL, CODE_L7Q}, /* 21: GAL E5b */
    {SYS_GAL, CODE_L8Q}, /* 22: GAL E5 AltBoc */
    {0, 0},              /* 23: LBand */
    {SYS_SBS, CODE_L1C}, /* 24: SBS L1C/A */
    {SYS_SBS, CODE_L5I}, /* 25: SBS L5 */
    {SYS_QZS, CODE_L5Q}, /* 26: QZS L5 */
    {SYS_QZS, CODE_L6L}, /* 27: QZS L6 */
    {SYS_CMP, CODE_L2I}, /* 28: BDS B1I */
    {SYS_CMP, CODE_L7I}, /* 29: BDS B2I */
    {SYS_CMP, CODE_L6I}, /* 30: BDS B3I */
    {0, 0},              /* 31: reserved */
    {SYS_QZS, CODE_L1L}, /* 32: QZS L1C */
    {SYS_QZS, CODE_L1Z}, /* 33: QZS L1S */
    {SYS_CMP, CODE_L7D}, /* 34: BDS B2b */
    {0, 0},              /* 35: reserved */
    {SYS_IRN, CODE_L9A}, /* 36: IRN S */
    {0, 0},              /* 37: reserved */
    {SYS_QZS, CODE_L1E}, /* 38: QZS L1E(L1CB) */
    {SYS_QZS, CODE_L5Z}  /* 39: QZS L5S */
};
/* signal number to freq-index and code --------------------------------------*/
static int sig2idx(int sat, int sig, const char* opt, uint8_t* code) {
    int idx, sys = satsys(sat, NULL), nex = NEXOBS;

    if (sig < 0 || sig > SBF_MAXSIG || sig_tbl[sig][0] != sys) {
        return -1;
    }
    *code = sig_tbl[sig][1];
    idx = code2freq_idx(sys, *code);

    /* resolve code priority in a freq-index */
    if (sys == SYS_GPS) {
        if (strstr(opt, "-GL1W") && idx == 0) {
            return (*code == CODE_L1W) ? 0 : -1;
        }
        if (strstr(opt, "-GL1L") && idx == 0) {
            return (*code == CODE_L1L) ? 0 : -1;
        }
        if (strstr(opt, "-GL2L") && idx == 1) {
            return (*code == CODE_L2L) ? 1 : -1;
        }
        if (*code == CODE_L1W) {
            return (nex < 1) ? -1 : NFREQ;
        }
        if (*code == CODE_L2L) {
            return (nex < 2) ? -1 : NFREQ + 1;
        }
        if (*code == CODE_L1L) {
            return (nex < 3) ? -1 : NFREQ + 2;
        }
    } else if (sys == SYS_GLO) {
        if (strstr(opt, "-RL1P") && idx == 0) {
            return (*code == CODE_L1P) ? 0 : -1;
        }
        if (strstr(opt, "-RL2C") && idx == 1) {
            return (*code == CODE_L2C) ? 1 : -1;
        }
        if (*code == CODE_L1P) {
            return (nex < 1) ? -1 : NFREQ;
        }
        if (*code == CODE_L2C) {
            return (nex < 2) ? -1 : NFREQ + 1;
        }
    } else if (sys == SYS_QZS) {
        if (strstr(opt, "-JL1L") && idx == 0) {
            return (*code == CODE_L1L) ? 0 : -1;
        }
        if (strstr(opt, "-JL1Z") && idx == 0) {
            return (*code == CODE_L1Z) ? 0 : -1;
        }
        if (*code == CODE_L1L) {
            return (nex < 1) ? -1 : NFREQ;
        }
        if (*code == CODE_L1Z) {
            return (nex < 2) ? -1 : NFREQ + 1;
        }
    } else if (sys == SYS_CMP) {
        if (strstr(opt, "-CL1P") && idx == 0) {
            return (*code == CODE_L1P) ? 0 : -1;
        }
        if (*code == CODE_L1P) {
            return (nex < 1) ? -1 : NFREQ;
        }
    }
    return (idx < NFREQ) ? idx : -1;
}
/* initialize obs data fields ------------------------------------------------*/
static void init_obsd(gtime_t time, int sat, obsd_t* data) {
    int i;

    data->time = time;
    data->sat = (uint8_t)sat;

    for (i = 0; i < NFREQ + NEXOBS; i++) {
        data->L[i] = data->P[i] = 0.0;
        data->D[i] = 0.0f;
        data->SNR[i] = (uint16_t)0;
        data->LLI[i] = (uint8_t)0;
        data->code[i] = CODE_NONE;
    }
}
/* decode SBF GNSS measurements ----------------------------------------------*/
static int decode_measepoch(raw_t* raw) {
    uint8_t *p = raw->buff + 14, code;
    double P1, P2, L1, L2, D1, D2, S1, S2, freq1, freq2;
    int i, j, idx, n, n1, n2, len1, len2, sig, ant, svid, info, sat, sys, lock, fcn, LLI;
    int ant_sel = 0; /* antenna selection (0:main) */

    if (strstr(raw->opt, "-AUX1")) {
        ant_sel = 1;
    } else if (strstr(raw->opt, "-AUX2")) {
        ant_sel = 2;
    }

    if (raw->len < 20) {
        trace(NULL, 2, "sbf measepoch length error: len=%d\n", raw->len);
        return -1;
    }
    n1 = U1(p);
    len1 = U1(p + 1);
    len2 = U1(p + 2);

    if (U1(p + 3) & 0x80) {
        trace(NULL, 2, "sbf measepoch scrambled\n");
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype + strlen(raw->msgtype), " nsat=%d", n1);
    }
    for (i = n = 0, p += 6; i < n1 && n < MAXOBS && p + 20 <= raw->buff + raw->len; i++) {
        svid = U1(p + 2);
        ant = U1(p + 1) >> 5;
        sig = U1(p + 1) & 0x1f;
        info = U1(p + 18);
        n2 = U1(p + 19);
        fcn = 0;
        if (sig == 31) {
            sig = (info >> 3) + 32;
        } else if (sig >= 8 && sig <= 11) {
            fcn = (info >> 3) - 8;
        }

        if (ant != ant_sel) {
            trace(NULL, 3, "sbf measepoch ant error: svid=%d ant=%d\n", svid, ant);
            p += len1 + len2 * n2;
            continue;
        }
        if (!(sat = svid2sat(svid))) {
            trace(NULL, 3, "sbf measepoch svid error: svid=%d\n", svid);
            p += len1 + len2 * n2;
            continue;
        }
        init_obsd(raw->time, sat, raw->obs.data + n);
        P1 = D1 = 0.0;
        sys = satsys(sat, NULL);

        /* extract Type-1 pseudorange and Doppler (needed as base for Type-2
         * delta values) regardless of whether this signal is accepted */
        if ((U1(p + 3) & 0x1f) != 0 || U4(p + 4) != 0) {
            P1 = (U1(p + 3) & 0x0f) * 4294967.296 + U4(p + 4) * 0.001;
        }
        if (I4(p + 8) != (-2147483647 - 1)) {
            D1 = I4(p + 8) * 0.0001;
        }

        if ((idx = sig2idx(sat, sig, raw->opt, &code)) < 0) {
            /* Type-1 signal not in obsdef — still process Type-2 sub-blocks
             * which may contain accepted signals (e.g. GAL E7Q rejected but
             * E1C/E5Q accepted in Type-2). P1/D1 are retained as base values. */
            trace(NULL, 3, "sbf measepoch sig skip (type1): sat=%d sig=%d\n", sat, sig);
        } else {
            freq1 = code2freq(sys, code, fcn);
            if (P1 != 0.0) {
                raw->obs.data[n].P[idx] = P1;
            }
            if (D1 != 0.0) {
                raw->obs.data[n].D[idx] = (float)D1;
            }
            lock = U2(p + 16);
            if (P1 != 0.0 && freq1 > 0.0 && lock != 65535 && (I1(p + 14) != -128 || U2(p + 12) != 0)) {
                L1 = I1(p + 14) * 65.536 + U2(p + 12) * 0.001;
                raw->obs.data[n].L[idx] = P1 * freq1 / CLIGHT + L1;
                LLI = 0;
                if (lock < raw->lockt[sat - 1][idx]) {
                    LLI += 1;
                }
                if (info & (1 << 2)) {
                    LLI += 2;
                }
                raw->obs.data[n].LLI[idx] = (uint8_t)LLI;
                raw->lockt[sat - 1][idx] = lock;
            }
            if (U1(p + 15) != 255) {
                S1 = U1(p + 15) * 0.25 + ((sig == 1 || sig == 2) ? 0.0 : 10.0);
                raw->obs.data[n].SNR[idx] = (uint16_t)(S1 / SNR_UNIT + 0.5);
            }
            raw->obs.data[n].code[idx] = code;
        }

        for (j = 0, p += len1; j < n2 && p + 12 <= raw->buff + raw->len; j++, p += len2) {
            sig = U1(p) & 0x1f;
            ant = U1(p) >> 5;
            info = U1(p + 5);
            if (sig == 31) {
                sig = (info >> 3) + 32;
            }

            if (ant != ant_sel) {
                trace(NULL, 3, "sbf measepoch ant error: sat=%d ant=%d\n", sat, ant);
                continue;
            }
            if ((idx = sig2idx(sat, sig, raw->opt, &code)) < 0) {
                trace(NULL, 3, "sbf measepoch sig error: sat=%d sig=%d\n", sat, sig);
                continue;
            }
            P2 = 0.0;
            freq2 = code2freq(sys, code, fcn);

            if (P1 != 0.0 && (getbits(p + 3, 5, 3) != -4 || U2(p + 6) != 0)) {
                P2 = P1 + getbits(p + 3, 5, 3) * 65.536 + U2(p + 6) * 0.001;
                raw->obs.data[n].P[idx] = P2;
            }
            if (P2 != 0.0 && freq2 > 0.0 && (I1(p + 4) != -128 || U2(p + 8) != 0)) {
                L2 = I1(p + 4) * 65.536 + U2(p + 8) * 0.001;
                raw->obs.data[n].L[idx] = P2 * freq2 / CLIGHT + L2;
            }
            if (D1 != 0.0 && freq1 > 0.0 && freq2 > 0.0 && (getbits(p + 3, 0, 5) != -16 || U2(p + 10) != 0)) {
                D2 = getbits(p + 3, 0, 5) * 6.5536 + U2(p + 10) * 0.0001;
                raw->obs.data[n].D[idx] = (float)(D1 * freq2 / freq1) + D2;
            }
            lock = U1(p + 1);
            if (lock != 255) {
                LLI = 0;
                if (lock < raw->lockt[sat - 1][idx]) {
                    LLI += 1;
                }
                if (info & (1 << 2)) {
                    LLI += 2;
                }
                raw->obs.data[n].LLI[idx] = (uint8_t)LLI;
                raw->lockt[sat - 1][idx] = lock;
            }
            if (U1(p + 2) != 255) {
                S2 = U1(p + 2) * 0.25 + ((sig == 1 || sig == 2) ? 0.0 : 10.0);
                raw->obs.data[n].SNR[idx] = (uint16_t)(S2 / SNR_UNIT + 0.5);
            }
            raw->obs.data[n].code[idx] = code;
        }
        /* only advance obs counter if at least one signal was accepted */
        {
            int has_obs = 0, k;
            for (k = 0; k < NFREQ + NEXOBS; k++) {
                if (raw->obs.data[n].code[k] != 0) { has_obs = 1; break; }
            }
            if (has_obs) n++;
        }
    }
    raw->obs.n = n;
    return 1;
}
/* decode SBF GNSS measurements extra info -----------------------------------*/
static int decode_measextra(raw_t* raw) {
    /* not yet supported */
    return 0;
}
/* decode ephemeris ----------------------------------------------------------*/
static int decode_eph(raw_t* raw, int sat) {
    eph_t eph = {0};

    if (!decode_frame(raw->subfrm[sat - 1], &eph, NULL, NULL, NULL)) {
        return 0;
    }

    if (!strstr(raw->opt, "-EPHALL")) {
        if (eph.iode == raw->nav.eph[sat - 1].iode && eph.iodc == raw->nav.eph[sat - 1].iodc &&
            timediff(eph.toe, raw->nav.eph[sat - 1].toe) == 0.0 &&
            timediff(eph.toc, raw->nav.eph[sat - 1].toc) == 0.0) {
            return 0;
        }
    }
    eph.sat = sat;
    raw->nav.eph[sat - 1] = eph;
    raw->ephsat = sat;
    raw->ephset = 0;
    return 2;
}
/* UTC 8-bit week -> full week -----------------------------------------------*/
static void adj_utcweek(gtime_t time, double* utc) {
    int week;

    time2gpst(time, &week);
    utc[3] += week / 256 * 256;
    if (utc[3] < week - 127) {
        utc[3] += 256.0;
    } else if (utc[3] > week + 127) {
        utc[3] -= 256.0;
    }
    utc[5] += utc[3] / 256 * 256;
    if (utc[5] < utc[3] - 127) {
        utc[5] += 256.0;
    } else if (utc[5] > utc[3] + 127) {
        utc[5] -= 256.0;
    }
}
/* decode ION/UTC parameters -------------------------------------------------*/
static int decode_ionutc(raw_t* raw, int sat) {
    double ion[8], utc[8];
    int sys = satsys(sat, NULL);

    if (!decode_frame(raw->subfrm[sat - 1], NULL, NULL, ion, utc)) {
        return 0;
    }

    adj_utcweek(raw->time, utc);
    if (sys == SYS_QZS) {
        matcpy(raw->nav.ion_qzs, ion, 8, 1);
        matcpy(raw->nav.utc_qzs, utc, 8, 1);
    } else {
        matcpy(raw->nav.ion_gps, ion, 8, 1);
        matcpy(raw->nav.utc_gps, utc, 8, 1);
    }
    return 9;
}
/* decode SBF raw C/A subframe -----------------------------------------------*/
static int decode_rawca(raw_t* raw, int sys) {
    uint8_t *p = raw->buff + 14, buff[30];
    int i, svid, sat, prn, id, ret;

    if (raw->len < 60) {
        trace(NULL, 2, "sbf rawca length error: sys=%d len=%d\n", sys, raw->len);
        return -1;
    }
    svid = U1(p);
    if (!(sat = svid2sat(svid)) || satsys(sat, &prn) != sys) {
        trace(NULL, 2, "sbf rawca svid error: sys=%d svid=%d\n", sys, svid);
        return -1;
    }
    if (!U1(p + 1)) {
        trace(NULL, 3, "sbf rawca parity/crc error: sys=%d prn=%d\n", sys, prn);
        return 0;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype + strlen(raw->msgtype), " prn=%d", prn);
    }
    for (i = 0, p += 6; i < 10; i++, p += 4) { /* 24 x 10 bits w/o parity */
        setbitu(buff, 24 * i, 24, U4(p) >> 6);
    }
    id = getbitu(buff, 43, 3);

    if (id < 1 || id > 5) {
        trace(NULL, 2, "sbf rawca subframe id error: sys=%d prn=%d id=%d\n", sys, prn, id);
        return -1;
    }
    memcpy(raw->subfrm[sat - 1] + (id - 1) * 30, buff, 30);

    if (id == 3) {
        return decode_eph(raw, sat);
    }
    if (id == 4 || id == 5) {
        ret = decode_ionutc(raw, sat);
        memset(raw->subfrm[sat - 1] + id * 30, 0, 30);
        return ret;
    }
    return 0;
}
/* decode SBF GPS C/A subframe -----------------------------------------------*/
static int decode_gpsrawca(raw_t* raw) { return decode_rawca(raw, SYS_GPS); }
/* decode SBF GLONASS L1CA or L2CA navigation string -------------------------*/
static int decode_glorawca(raw_t* raw) {
    geph_t geph = {0};
    gtime_t* time;
    double utc[8] = {0};
    uint8_t *p = raw->buff + 14, buff[12];
    int i, svid, sat, prn, m;

    if (raw->len < 32) {
        trace(NULL, 2, "sbf glorawca length error: len=%d\n", raw->len);
        return -1;
    }
    svid = U1(p);
    if (!(sat = svid2sat(svid)) || satsys(sat, &prn) != SYS_GLO) {
        trace(NULL, 3, "sbf glorawca svid error: svid=%d\n", svid);
        return (svid == 62) ? 0 : -1; /* svid=62: slot unknown */
    }
    if (!U1(p + 1)) {
        trace(NULL, 3, "sbf glorawca parity/crc error: prn=%d\n", prn);
        return 0;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype + strlen(raw->msgtype), " prn=%d", prn);
    }
    for (i = 0; i < 3; i++) {
        setbitu(buff, 32 * i, 32, U4(p + 6 + 4 * i)); /* 85 bits */
    }
    m = getbitu(buff, 1, 4);
    if (m < 1 || m > 15) {
        trace(NULL, 2, "sbf glorawca string number error: prn=%d m=%d\n", prn, m);
        return -1;
    }
    time = (gtime_t*)(raw->subfrm[sat - 1] + 150);
    if (fabs(timediff(raw->time, *time)) > 30.0) {
        memset(raw->subfrm[sat - 1], 0, 40);
        memcpy(time, &raw->time, sizeof(gtime_t));
    }
    memcpy(raw->subfrm[sat - 1] + (m - 1) * 10, buff, 10);
    if (m != 4) {
        return 0;
    }

    geph.tof = raw->time;
    if (!decode_glostr(raw->subfrm[sat - 1], &geph, utc)) {
        return 0;
    }

    matcpy(raw->nav.utc_glo, utc, 8, 1);

    if (geph.sat != sat) {
        trace(NULL, 2, "sbf glorawca satellite error: sat=%d %d\n", sat, geph.sat);
        return -1;
    }
    geph.frq = (int)U1(p + 4) - 8;

    if (!strstr(raw->opt, "-EPHALL")) {
        if (geph.iode == raw->nav.geph[prn - 1].iode && geph.svh == raw->nav.geph[prn - 1].svh &&
            timediff(geph.toe, raw->nav.geph[prn - 1].toe) == 0.0) {
            return 0;
        }
    }
    raw->nav.geph[prn - 1] = geph;
    raw->ephsat = sat;
    raw->ephset = 0;
    return 2;
}
/* decode SBF Galileo F/NAV navigation page ----------------------------------*/
static int decode_galrawfnav(raw_t* raw) {
    eph_t eph = {0};
    double ion[4] = {0}, utc[8] = {0};
    uint8_t *p = raw->buff + 14, buff[32], crc_buff[27];
    int i, svid, src, sat, prn, type;

    if (strstr(raw->opt, "-GALINAV")) {
        return 0;
    }

    if (raw->len < 52) {
        trace(NULL, 2, "sbf galrawfnav length error: len=%d\n", raw->len);
        return -1;
    }
    svid = U1(p);
    src = U1(p + 3) & 0x1f;

    if (!(sat = svid2sat(svid)) || satsys(sat, &prn) != SYS_GAL) {
        trace(NULL, 2, "sbf galrawfnav svid error: svid=%d src=%d\n", svid, src);
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype + strlen(raw->msgtype), " prn=%d src=%d", prn, src);
    }
    if (src != 20 && src != 22) { /* E5a or E5 AltBOC */
        trace(NULL, 2, "sbf galrawfnav source error: prn=%d src=%d\n", prn, src);
        return -1;
    }
    for (i = 0; i < 8; i++) {
        setbitu(buff, 32 * i, 32, U4(p + 6 + 4 * i)); /* 244 bits page */
    }
    for (i = 0; i < 27; i++) { /* data(214) + CRC(24) */
        if (i == 0) {
            crc_buff[i] = getbitu(buff, 0, 6);
        } else {
            crc_buff[i] = getbitu(buff, 8 * i - 2, 8);
        }
    }
    trace(NULL, 4, "sbf galrawfnav: prn=%d src=%d crc=%06x %06x\n", prn, src, rtk_crc24q(crc_buff, 27),
          getbitu(buff, 214, 24));

    if (!U1(p + 1)) {
        trace(NULL, 3, "sbf galrawfnav parity/crc error: prn=%d src=%d\n", prn, src);
        return 0;
    }
    type = getbitu(buff, 0, 6); /* page type */

    if (type == 63) {
        return 0; /* dummy page */
    }
    if (type < 1 || type > 6) {
        trace(NULL, 2, "sbf galrawfnav page type error: prn=%d type=%d\n", prn, type);
        return -1;
    }
    /* save 244 bits page (31 bytes * 6 page) */
    memcpy(raw->subfrm[sat - 1] + 128 + (type - 1) * 31, buff, 31);

    if (type != 4) {
        return 0;
    }
    if (!decode_gal_fnav(raw->subfrm[sat - 1] + 128, &eph, ion, utc)) {
        return 0;
    }

    if (eph.sat != sat) {
        trace(NULL, 2, "sbf galrawfnav satellite error: sat=%d %d\n", sat, eph.sat);
        return -1;
    }
    eph.code |= (1 << 1); /* data source: E5a */

    adj_utcweek(raw->time, utc);
    matcpy(raw->nav.ion_gal, ion, 4, 1);
    matcpy(raw->nav.utc_gal, utc, 8, 1);

    if (!strstr(raw->opt, "-EPHALL")) {
        if (eph.iode == raw->nav.eph[sat - 1 + MAXSAT].iode &&
            timediff(eph.toe, raw->nav.eph[sat - 1 + MAXSAT].toe) == 0.0 &&
            timediff(eph.toc, raw->nav.eph[sat - 1 + MAXSAT].toc) == 0.0) {
            return 0;
        }
    }
    raw->nav.eph[sat - 1 + MAXSAT] = eph;
    raw->ephsat = sat;
    raw->ephset = 1; /* 1:F/NAV */
    return 2;
}
/* decode SBF Galileo I/NAV navigation page ----------------------------------*/
static int decode_galrawinav(raw_t* raw) {
    eph_t eph = {0};
    double ion[4] = {0}, utc[8] = {0};
    uint8_t *p = raw->buff + 14, buff[32], crc_buff[25], type, part1, part2, page1, page2;
    int i, j, svid, src, sat, prn;

    if (strstr(raw->opt, "-GALFNAV")) {
        return 0;
    }

    if (raw->len < 52) {
        trace(NULL, 2, "sbf galrawinav length error: len=%d\n", raw->len);
        return -1;
    }
    svid = U1(p);
    src = U1(p + 3) & 0x1f;

    if (!(sat = svid2sat(svid)) || satsys(sat, &prn) != SYS_GAL) {
        trace(NULL, 2, "sbf galrawinav svid error: svid=%d src=%d\n", svid, src);
        return -1;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype + strlen(raw->msgtype), " prn=%d src=%d", prn, src);
    }
    if (src != 17 && src != 21 && src != 22) { /* E1, E5b or E5 AltBOC */
        trace(NULL, 2, "sbf galrawinav source error: prn=%d src=%d\n", prn, src);
        return -1;
    }
    for (i = 0; i < 8; i++) {
        setbitu(buff, 32 * i, 32, U4(p + 6 + i * 4)); /* 114(even) + 120(odd) bits */
    }
    for (i = 0; i < 25; i++) { /* data(196) + CRC(24) */
        if (i == 0) {
            crc_buff[i] = getbitu(buff, 0, 4);
        } else {
            crc_buff[i] = getbitu(buff, 8 * i - 4, 8);
        }
    }
    trace(NULL, 4, "sbf galrawinav: prn=%d src=%d crc=%06x %06x\n", prn, src, rtk_crc24q(crc_buff, 25),
          getbitu(buff, 196, 24));

    if (!U1(p + 1)) {
        trace(NULL, 3, "sbf galrawinav parity/crc error: prn=%d src=%d\n", prn, src);
        return 0;
    }
    part1 = getbitu(buff, 0, 1);
    page1 = getbitu(buff, 1, 1);
    part2 = getbitu(buff, 114, 1);
    page2 = getbitu(buff, 115, 1);

    if (part1 != 0 || part2 != 1) {
        trace(NULL, 3, "sbf galrawinav part error: prn=%d even/odd=%d %d\n", prn, part1, part2);
        return -1;
    }
    if (page1 == 1 || page2 == 1) {
        return 0; /* alert page */
    }

    type = getbitu(buff, 2, 6); /* word type */

    if (type > 6) {
        return 0;
    }

    /* save 128 (112:even+16:odd) bits word (16 bytes * 7 word) */
    for (i = 0, j = 2; i < 14; i++, j += 8) {
        raw->subfrm[sat - 1][type * 16 + i] = getbitu(buff, j, 8);
    }
    for (i = 14, j = 116; i < 16; i++, j += 8) {
        raw->subfrm[sat - 1][type * 16 + i] = getbitu(buff, j, 8);
    }
    if (type != 5) {
        return 0;
    }
    if (!decode_gal_inav(raw->subfrm[sat - 1], &eph, ion, utc)) {
        return 0;
    }

    if (eph.sat != sat) {
        trace(NULL, 2, "sbf galrawinav satellite error: sat=%d %d\n", sat, eph.sat);
        return -1;
    }
    eph.code |= (src == 17) ? (1 << 0) : (1 << 2); /* data source: E1 or E5b */

    adj_utcweek(raw->time, utc);
    matcpy(raw->nav.ion_gal, ion, 4, 1);
    matcpy(raw->nav.utc_gal, utc, 8, 1);

    if (!strstr(raw->opt, "-EPHALL")) {
        if (eph.iode == raw->nav.eph[sat - 1].iode && timediff(eph.toe, raw->nav.eph[sat - 1].toe) == 0.0 &&
            timediff(eph.toc, raw->nav.eph[sat - 1].toc) == 0.0) {
            return 0;
        }
    }
    raw->nav.eph[sat - 1] = eph;
    raw->ephsat = sat;
    raw->ephset = 0; /* 0:I/NAV */
    return 2;
}
/* decode SBF SBAS L1 navigation frame ---------------------------------------*/
static int decode_georawl1(raw_t* raw) {
    uint8_t *p = raw->buff + 14, buff[32];
    int i, svid, sat, prn;

    if (raw->len < 52) {
        trace(NULL, 2, "sbf georawl1 length error: len=%d\n", raw->len);
        return -1;
    }
    svid = U1(p);
    if (!(sat = svid2sat(svid)) || satsys(sat, &prn) != SYS_SBS) {
        trace(NULL, 2, "sbf georawl1 svid error: svid=%d\n", svid);
        return -1;
    }
    if (!U1(p + 1)) {
        trace(NULL, 3, "sbf georawl1 parity/crc error: prn=%d err=%d\n", prn, U1(p + 2));
        return 0;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype + strlen(raw->msgtype), " prn=%d", prn);
    }
    raw->sbsmsg.tow = (int)time2gpst(raw->time, &raw->sbsmsg.week);
    raw->sbsmsg.prn = prn;

    for (i = 0; i < 8; i++) {
        setbitu(buff, 32 * i, 32, U4(p + 6 + 4 * i));
    }
    memcpy(raw->sbsmsg.msg, buff, 29); /* 226 bits w/o CRC */
    raw->sbsmsg.msg[28] &= 0xC0;
    return 3;
}
/* decode SBF BDS navigation frame -------------------------------------------*/
static int decode_bdsraw(raw_t* raw) {
    eph_t eph = {0};
    double ion[8], utc[8];
    uint8_t *p = raw->buff + 14, buff[40];
    int i, id, svid, sat, prn, pgn;

    if (raw->len < 52) {
        trace(NULL, 2, "sbf bdsraw length error: len=%d\n", raw->len);
        return -1;
    }
    svid = U1(p);
    if (!(sat = svid2sat(svid)) || satsys(sat, &prn) != SYS_CMP) {
        trace(NULL, 2, "sbf bdsraw svid error: svid=%d\n", svid);
        return -1;
    }
    if (!U1(p + 1)) {
        trace(NULL, 3, "sbf bdsraw parity/crc error: prn=%d\n", prn);
        return 0;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype + strlen(raw->msgtype), " prn=%d", prn);
    }
    for (i = 0, p += 6; i < 10; i++, p += 4) {
        setbitu(buff, 32 * i, 32, U4(p));
    }
    id = getbitu(buff, 15, 3); /* subframe ID */
    if (id < 1 || id > 5) {
        trace(NULL, 2, "sbf bdsraw id error: prn=%d id=%d\n", prn, id);
        return -1;
    }
    if (prn >= 6 && prn <= 58) { /* IGSO/MEO */
        memcpy(raw->subfrm[sat - 1] + (id - 1) * 38, buff, 38);

        if (id == 3) {
            if (!decode_bds_d1(raw->subfrm[sat - 1], &eph, NULL, NULL)) {
                return 0;
            }
        } else if (id == 5) {
            if (!decode_bds_d1(raw->subfrm[sat - 1], NULL, ion, utc)) {
                return 0;
            }
            matcpy(raw->nav.ion_cmp, ion, 8, 1);
            matcpy(raw->nav.utc_cmp, utc, 8, 1);
            return 9;
        } else {
            return 0;
        }
    } else {                        /* GEO */
        pgn = getbitu(buff, 42, 4); /* page number */

        if (id == 1 && pgn >= 1 && pgn <= 10) {
            memcpy(raw->subfrm[sat - 1] + (pgn - 1) * 38, buff, 38);
            if (pgn != 10) {
                return 0;
            }
            if (!decode_bds_d2(raw->subfrm[sat - 1], &eph, NULL)) {
                return 0;
            }
        } else if (id == 1 && pgn == 102) {
            memcpy(raw->subfrm[sat - 1] + 10 * 38, buff, 38);
            if (!decode_bds_d2(raw->subfrm[sat - 1], NULL, utc)) {
                return 0;
            }
            matcpy(raw->nav.utc_cmp, utc, 8, 1);
            return 9;
        } else {
            return 0;
        }
    }
    if (!strstr(raw->opt, "-EPHALL")) {
        if (timediff(eph.toe, raw->nav.eph[sat - 1].toe) == 0.0) {
            return 0;
        }
    }
    eph.sat = sat;
    raw->nav.eph[sat - 1] = eph;
    raw->ephsat = sat;
    raw->ephset = 0;
    return 2;
}
/* decode SBF QZS C/A subframe -----------------------------------------------*/
static int decode_qzsrawl1ca(raw_t* raw) { return decode_rawca(raw, SYS_QZS); }
/* decode SBF QZS L6 message -------------------------------------------------*/
static int decode_qzsrawl6(raw_t* raw, rtcm_t* rtcm) {
    gtime_t time;
    double tow;
    int prn, sat, week, i, j, ret;
    unsigned char svid, parity, rscnt, source, rxchannel;
    uint8_t* p = (raw->buff) + 8; /* jump to TOW location */
    unsigned char buff[256];

    if (raw->len < 272) {
        trace(NULL, 2, "SBF decode_qzsrawl6 frame length error: len=%d\n", raw->len);
        return -1;
    }

    /* Get time information */
    tow = U4(p);      /* TOW in ms */
    week = U2(p + 4); /* number of weeks */
    time = gpst2time(week, tow * 0.001);

    svid = U1(p + 6);
    parity = U1(p + 7);
    rscnt = U1(p + 8);
    source = U1(p + 9);
    rxchannel = U1(p + 11);

    prn = svid - 180 + MINPRNQZS - 1;
    sat = satno(SYS_QZS, prn);

    if (sat == 0) {
        trace(NULL, 2, "SBF decode_qzssl6 SVID error: SVID=%d,prn=%d\n", svid, prn);
        return -1;
    }
    trace(NULL, 3, "SBF QZSRawL6 : %s, prn=%d,SVID=%d, Parity=%d, RSCnt=%d, Source=%d, rxchannel=%d\n",
          time_str(time, 0), prn, svid, parity, rscnt, source, rxchannel);

    if (parity == 0) {
        trace(NULL, 2, "SBF decode_qzsrawl6 RS decode error\n");
        return -1;
    }

    j = 0;
    for (i = 0; i < 63; i++) {
        buff[j] = ((U4(p + 12 + i * 4) >> 24) & 0x000000FF);     /* take first byte  */
        buff[1 + j] = ((U4(p + 12 + i * 4) >> 16) & 0x000000FF); /* take second byte */
        buff[2 + j] = ((U4(p + 12 + i * 4) >> 8) & 0x000000FF);  /* take third byte  */
        buff[3 + j] = ((U4(p + 12 + i * 4)) & 0x000000FF);       /* take fourth byte */
        j = j + 4;
    }

    memcpy(rtcm->buff, buff, 250);
    ret = decode_qzss_l6emsg(rtcm);

    return ret;
}
/* decode SBF QZSRawL6D block (CLAS L6D) ------------------------------------
 * Extracts the 250-byte L6D data part from the SBF QZSRawL6D (4270) block
 * and copies it into rtcm->buff WITHOUT passing through the MADOCA L6E
 * decoder.  Returns 10 to signal "L6D frame ready" to the caller.
 *---------------------------------------------------------------------------*/
static int decode_qzsrawl6d(raw_t* raw, rtcm_t* rtcm) {
    double tow;
    int prn, sat, week, i, j;
    unsigned char svid, parity, rscnt, source, rxchannel;
    uint8_t* p = (raw->buff) + 8; /* jump to TOW location */
    unsigned char buff[256];

    if (raw->len < 272) {
        trace(NULL, 2, "SBF decode_qzsrawl6d frame length error: len=%d\n",
              raw->len);
        return -1;
    }

    /* time */
    tow = U4(p);
    week = U2(p + 4);
    raw->time = gpst2time(week, tow * 0.001);

    svid      = U1(p + 6);
    parity    = U1(p + 7);
    rscnt     = U1(p + 8);
    source    = U1(p + 9);
    rxchannel = U1(p + 11);

    prn = svid - 180 + MINPRNQZS - 1;
    sat = satno(SYS_QZS, prn);

    if (sat == 0) {
        trace(NULL, 2, "SBF decode_qzsrawl6d SVID error: SVID=%d prn=%d\n",
              svid, prn);
        return -1;
    }
    trace(NULL, 3, "SBF QZSRawL6D: %s prn=%d SVID=%d Parity=%d Source=%d\n",
          time_str(raw->time, 0), prn, svid, parity, source);

    if (parity == 0) {
        trace(NULL, 2, "SBF decode_qzsrawl6d RS decode error\n");
        return -1;
    }

    /* extract 252 bytes (63 x 4B words) → use first 250 bytes */
    for (i = 0, j = 0; i < 63; i++, j += 4) {
        buff[j]     = (U4(p + 12 + i * 4) >> 24) & 0xFF;
        buff[j + 1] = (U4(p + 12 + i * 4) >> 16) & 0xFF;
        buff[j + 2] = (U4(p + 12 + i * 4) >>  8) & 0xFF;
        buff[j + 3] =  U4(p + 12 + i * 4)        & 0xFF;
    }

    memcpy(rtcm->buff, buff, 250);

    /* store satellite number for caller (e.g., cssr2rtcm3 PRN filtering) */
    raw->ephsat = sat;

    /* return 10 = L6D frame ready (skip MADOCA decoder) */
    return 10;
}
/* decode SBF NavIC/IRNSS subframe -------------------------------------------*/
static int decode_navicraw(raw_t* raw) {
    eph_t eph = {0};
    double ion[8], utc[9];
    uint8_t *p = raw->buff + 14, buff[40];
    int i, id, svid, sat, prn, ret = 0;

    if (raw->len < 52) {
        trace(NULL, 2, "sbf navicraw length error: len=%d\n", raw->len);
        return -1;
    }
    svid = U1(p);
    if (!(sat = svid2sat(svid)) || satsys(sat, &prn) != SYS_IRN) {
        trace(NULL, 2, "sbf navicraw svid error: svid=%d\n", svid);
        return -1;
    }
    if (!U1(p + 1)) {
        trace(NULL, 3, "sbf navicraw parity/crc error: prn=%d err=%d\n", prn, U1(p + 2));
        return 0;
    }
    if (raw->outtype) {
        sprintf(raw->msgtype + strlen(raw->msgtype), " prn=%d", prn);
    }
    for (i = 0, p += 6; i < 10; i++, p += 4) {
        setbitu(buff, 32 * i, 32, U4(p));
    }
    id = getbitu(buff, 27, 2); /* subframe ID (0-3) */

    memcpy(raw->subfrm[sat - 1] + id * 37, buff, 37);

    if (id == 1) { /* subframe 2 */
        if (!decode_irn_nav(raw->subfrm[sat - 1], &eph, NULL, NULL)) {
            return 0;
        }

        if (!strstr(raw->opt, "-EPHALL")) {
            if (eph.iode == raw->nav.eph[sat - 1].iode && timediff(eph.toe, raw->nav.eph[sat - 1].toe) == 0.0) {
                return 0;
            }
        }
        eph.sat = sat;
        raw->nav.eph[sat - 1] = eph;
        raw->ephsat = sat;
        raw->ephset = 0;
        return 2;
    } else if (id == 2 || id == 3) { /* subframe 3 or 4 */
        if (decode_irn_nav(raw->subfrm[sat - 1], NULL, ion, NULL)) {
            matcpy(raw->nav.ion_irn, ion, 8, 1);
            ret = 9;
        }
        if (decode_irn_nav(raw->subfrm[sat - 1], NULL, NULL, utc)) {
            adj_utcweek(raw->time, utc);
            matcpy(raw->nav.utc_irn, utc, 9, 1);
            ret = 9;
        }
        memset(raw->subfrm[sat - 1] + id * 37, 0, 37);
        return ret;
    }
    return 0;
}
/* decode SBF GPS decoded navigation (GPSNav 5891) ---------------------------*/
static int decode_gpsnav(raw_t* raw) {
    eph_t eph = {0};
    uint8_t prn, sat;
    uint16_t week;
    uint32_t tocs;

    trace(NULL, 4, "SBF decode_gpsnav: len=%d\n", raw->len);

    if (raw->len < 140) {
        trace(NULL, 2, "SBF decode_gpsnav frame length error: len=%d\n",
              raw->len);
        return -1;
    }
    prn = U1(raw->buff + 14);
    sat = satno(SYS_GPS, prn);
    if (sat == 0 || !(prn >= 1 && prn <= 37)) {
        trace(NULL, 2, "SBF decode_gpsnav prn error: prn=%d\n", prn);
        return -1;
    }
    week = U2(raw->buff + 12);
    eph.code = U1(raw->buff + 18);
    eph.sva = U1(raw->buff + 19);
    eph.svh = U1(raw->buff + 20);
    eph.flag = U1(raw->buff + 21);
    eph.iodc = U2(raw->buff + 22);
    eph.iode = U1(raw->buff + 24);
    eph.fit = U1(raw->buff + 26) ? 6 : 4;
    eph.tgd[0] = R4(raw->buff + 28);
    tocs = U4(raw->buff + 32);
    eph.f2 = R4(raw->buff + 36);
    eph.f1 = R4(raw->buff + 40);
    eph.f0 = R4(raw->buff + 44);
    eph.crs = R4(raw->buff + 48);
    eph.deln = R4(raw->buff + 52) * PI;
    eph.M0 = R8(raw->buff + 56) * PI;
    eph.cuc = R4(raw->buff + 64);
    eph.e = R8(raw->buff + 68);
    eph.cus = R4(raw->buff + 76);
    eph.A = pow(R8(raw->buff + 80), 2);
    eph.toes = U4(raw->buff + 88);
    eph.cic = R4(raw->buff + 92);
    eph.OMG0 = R8(raw->buff + 96) * PI;
    eph.cis = R4(raw->buff + 104);
    eph.i0 = R8(raw->buff + 108) * PI;
    eph.crc = R4(raw->buff + 116);
    eph.omg = R8(raw->buff + 120) * PI;
    eph.OMGd = R4(raw->buff + 128) * PI;
    eph.idot = R4(raw->buff + 132) * PI;
    eph.week = adjgpsweek(week);
    eph.toe = gpst2time(adjgpsweek(adjust_WN10(week, U2(raw->buff + 138))),
                        eph.toes);
    eph.toc = gpst2time(adjgpsweek(adjust_WN10(week, U2(raw->buff + 136))),
                        tocs);
    eph.ttr = raw->time;

    if (!strstr(raw->opt, "-EPHALL")) {
        if (eph.iode == raw->nav.eph[sat - 1].iode &&
            eph.iodc == raw->nav.eph[sat - 1].iodc)
            return 0;
    }
    eph.sat = sat;
    raw->nav.eph[sat - 1] = eph;
    raw->ephsat = sat;
    raw->ephset = 0;
    return 2;
}
/* decode SBF GLONASS decoded navigation (GLONav 4004) -----------------------*/
static int decode_glonav(raw_t* raw) {
    geph_t eph = {0};
    uint8_t* p = raw->buff + 8;
    int prn, sat;
    uint16_t week, week_toes;
    uint32_t toes;

    trace(NULL, 4, "SBF decode_glonav: len=%d\n", raw->len);

    if (raw->len < 96) {
        trace(NULL, 2, "SBF decode_glonav frame length error: len=%d\n",
              raw->len);
        return -1;
    }
    prn = U1(p + 6) - 37;
    sat = satno(SYS_GLO, prn);
    if (sat == 0 || !(prn >= 1 && prn <= 24)) {
        trace(NULL, 2, "SBF decode_glonav prn error: prn=%d\n", prn);
        return -1;
    }
    week = U2(p + 4);
    eph.frq = U1(p + 7) - 8;
    eph.pos[0] = R8(p + 8) * 1000.0;
    eph.pos[1] = R8(p + 16) * 1000.0;
    eph.pos[2] = R8(p + 24) * 1000.0;
    eph.vel[0] = R4(p + 32) * 1000.0;
    eph.vel[1] = R4(p + 36) * 1000.0;
    eph.vel[2] = R4(p + 40) * 1000.0;
    eph.acc[0] = R4(p + 44) * 1000.0;
    eph.acc[1] = R4(p + 48) * 1000.0;
    eph.acc[2] = R4(p + 52) * 1000.0;
    eph.gamn = R4(p + 56);
    eph.taun = R4(p + 60);
    eph.dtaun = R4(p + 64);
    toes = U4(p + 68);
    week_toes = adjust_WN10(week, U2(p + 72));
    eph.age = U1(p + 76);
    eph.svh = U1(p + 77) >> 2;
    eph.iode = U2(p + 78);
    eph.sva = U2(p + 86);
    eph.tof = gpst2time(week, U4(p) * 0.001);
    eph.toe = gpst2time(week_toes, toes);

    if (!strstr(raw->opt, "-EPHALL")) {
        if (eph.iode == raw->nav.geph[prn - 1].iode) return 0;
    }
    eph.sat = sat;
    raw->nav.geph[prn - 1] = eph;
    raw->ephsat = sat;
    raw->ephset = 0;
    raw->nav.glo_fcn[prn - 1] = eph.frq + 8;
    return 2;
}
/* decode SBF Galileo decoded navigation (GALNav 4002) -----------------------*/
static int decode_galnav(raw_t* raw) {
    eph_t eph = {0};
    uint8_t* p = raw->buff + 8;
    int prn, sat, code;
    uint16_t week_toe, week_toc, health;
    uint32_t tocs;
    uint8_t sva;

    trace(NULL, 4, "SBF decode_galnav: len=%d\n", raw->len);

    if (raw->len < 149) {
        trace(NULL, 2, "SBF decode_galnav frame length error: len=%d\n",
              raw->len);
        return -1;
    }
    prn = U1(p + 6) - 70;
    sat = satno(SYS_GAL, prn);
    if (sat == 0 || !(prn >= 1 && prn <= 36)) {
        trace(NULL, 2, "SBF decode_galnav prn error: prn=%d\n", prn);
        return -1;
    }
    eph.week = U2(p + 4);
    code = U1(p + 7);
    if (code != 2 && code != 16) {
        trace(NULL, 2, "SBF decode_galnav code error: prn=%d code=%d\n", prn,
              code);
        return -1;
    }
    eph.code = code == 2 ? (1 << 0) | (1 << 2) | (1 << 9)
                         : (1 << 1) | (1 << 8);
    eph.A = pow(R8(p + 8), 2);
    eph.M0 = R8(p + 16) * PI;
    eph.e = R8(p + 24);
    eph.i0 = R8(p + 32) * PI;
    eph.omg = R8(p + 40) * PI;
    eph.OMG0 = R8(p + 48) * PI;
    eph.OMGd = R4(p + 56) * PI;
    eph.idot = R4(p + 60) * PI;
    eph.deln = R4(p + 64) * PI;
    eph.cuc = R4(p + 68);
    eph.cus = R4(p + 72);
    eph.crc = R4(p + 76);
    eph.crs = R4(p + 80);
    eph.cic = R4(p + 84);
    eph.cis = R4(p + 88);
    eph.toes = U4(p + 92);
    tocs = U4(p + 96);
    eph.f2 = R4(p + 100);
    eph.f1 = R4(p + 104);
    eph.f0 = R8(p + 108);
    week_toe = adjust_WN12(eph.week, U2(p + 116));
    week_toc = adjust_WN12(eph.week, U2(p + 118));
    eph.iode = U2(p + 120);
    eph.iodc = 0;
    health = U2(p + 122);
    {
        int svh = 0;
        if (health & 0x001) svh |= (health >> 1) & 7;
        if (health & 0x010) svh |= ((health >> 5) & 7) << 6;
        if (health & 0x100) svh |= ((health >> 9) & 7) << 3;
        eph.svh = svh;
    }
    sva = U1(p + (code == 2 ? 126 : 125));
    eph.sva = sva == 255 ? 0 : sva;
    if (R4(p + 128) != -2.e10f) eph.tgd[0] = R4(p + 128);
    if (R4(p + 132) != -2.e10f) eph.tgd[1] = R4(p + 132);
    eph.fit = 0;
    eph.toe = gpst2time(week_toe, eph.toes);
    eph.toc = gpst2time(week_toc, tocs);
    eph.ttr = gpst2time(eph.week, U4(p) * 0.001);

    if (!strstr(raw->opt, "-EPHALL")) {
        if (eph.iode == raw->nav.eph[sat - 1].iode) return 0;
    }
    eph.sat = sat;
    raw->nav.eph[sat - 1] = eph;
    raw->ephsat = sat;
    raw->ephset = 0;
    return 2;
}
/* decode SBF BDS decoded navigation (BDSNav 4081) ---------------------------*/
static int decode_bdsnav(raw_t* raw) {
    eph_t eph = {0};
    int prn, sat;
    uint16_t week, week_toc, week_toe;
    uint32_t tocs;

    trace(NULL, 4, "SBF decode_bdsnav: len=%d\n", raw->len);

    if (raw->len < 140) {
        trace(NULL, 2, "SBF decode_bdsnav frame length error: len=%d\n",
              raw->len);
        return -1;
    }
    prn = U1(raw->buff + 14) - 140;
    sat = satno(SYS_CMP, prn);
    if (sat == 0 || !(prn >= 1 && prn <= 63)) {
        trace(NULL, 2, "SBF decode_bdsnav prn error: prn=%d\n", prn);
        return -1;
    }
    eph.code = 0;
    eph.sva = U1(raw->buff + 18);
    eph.svh = U1(raw->buff + 19);
    eph.iodc = U1(raw->buff + 20);
    eph.iode = U1(raw->buff + 21);
    eph.tgd[0] = R4(raw->buff + 24);
    if (R4(raw->buff + 28) != -2.e10f) eph.tgd[1] = R4(raw->buff + 28);
    tocs = U4(raw->buff + 32);
    eph.f2 = R4(raw->buff + 36);
    eph.f1 = R4(raw->buff + 40);
    eph.f0 = R4(raw->buff + 44);
    eph.crs = R4(raw->buff + 48);
    eph.deln = R4(raw->buff + 52) * PI;
    eph.M0 = R8(raw->buff + 56) * PI;
    eph.cuc = R4(raw->buff + 64);
    eph.e = R8(raw->buff + 68);
    eph.cus = R4(raw->buff + 76);
    eph.A = pow(R8(raw->buff + 80), 2);
    eph.toes = U4(raw->buff + 88);
    eph.cic = R4(raw->buff + 92);
    eph.OMG0 = R8(raw->buff + 96) * PI;
    eph.cis = R4(raw->buff + 104);
    eph.i0 = R8(raw->buff + 108) * PI;
    eph.crc = R4(raw->buff + 116);
    eph.omg = R8(raw->buff + 120) * PI;
    eph.OMGd = R4(raw->buff + 128) * PI;
    eph.idot = R4(raw->buff + 132) * PI;
    week = U2(raw->buff + 12) - 1356; /* GPS week -> approx BDS week */
    week_toc = adjust_WN14(week, U2(raw->buff + 136));
    week_toe = adjust_WN14(week, U2(raw->buff + 138));
    eph.week = week_toe;
    eph.fit = 0;
    eph.toe = bdt2gpst(bdt2time(week_toe, eph.toes));
    eph.toc = bdt2gpst(bdt2time(week_toc, tocs));
    eph.ttr = raw->time;

    if (!strstr(raw->opt, "-EPHALL")) {
        if (eph.iode == raw->nav.eph[sat - 1].iode) return 0;
    }
    eph.sat = sat;
    raw->nav.eph[sat - 1] = eph;
    raw->ephsat = sat;
    raw->ephset = 0;
    return 2;
}
/* decode SBF QZSS decoded navigation (QZSNav 4095) -------------------------*/
static int decode_qzssnav(raw_t* raw) {
    eph_t eph = {0};
    int svid, sat;
    uint16_t week;
    double toc;

    trace(NULL, 4, "SBF decode_qzssnav: len=%d\n", raw->len);

    if (raw->len < 140) {
        trace(NULL, 2, "SBF decode_qzssnav frame length error: len=%d\n",
              raw->len);
        return -1;
    }
    svid = U1(raw->buff + 14);
    sat = svid2sat(svid);
    if (sat == 0) {
        trace(NULL, 2, "SBF decode_qzssnav svid error: svid=%d\n", svid);
        return -1;
    }
    week = U2(raw->buff + 12);
    eph.code = U1(raw->buff + 18);
    eph.sva = U1(raw->buff + 19);
    eph.svh = U1(raw->buff + 20);
    eph.iodc = U2(raw->buff + 22);
    eph.iode = U1(raw->buff + 24);
    eph.fit = U1(raw->buff + 26) ? 4 : 2;
    if (R4(raw->buff + 28) != -2.e10f) eph.tgd[0] = R4(raw->buff + 28);
    toc = U4(raw->buff + 32);
    eph.f2 = R4(raw->buff + 36);
    eph.f1 = R4(raw->buff + 40);
    eph.f0 = R4(raw->buff + 44);
    eph.crs = R4(raw->buff + 48);
    eph.deln = R4(raw->buff + 52) * PI;
    eph.M0 = R8(raw->buff + 56) * PI;
    eph.cuc = R4(raw->buff + 64);
    eph.e = R8(raw->buff + 68);
    eph.cus = R4(raw->buff + 76);
    eph.A = pow(R8(raw->buff + 80), 2);
    eph.toes = U4(raw->buff + 88);
    eph.cic = R4(raw->buff + 92);
    eph.OMG0 = R8(raw->buff + 96) * PI;
    eph.cis = R4(raw->buff + 104);
    eph.i0 = R8(raw->buff + 108) * PI;
    eph.crc = R4(raw->buff + 116);
    eph.omg = R8(raw->buff + 120) * PI;
    eph.OMGd = R4(raw->buff + 128) * PI;
    eph.idot = R4(raw->buff + 132) * PI;
    eph.week = adjust_WN10(week, U2(raw->buff + 136));
    eph.toe = gpst2time(adjust_WN10(week, U2(raw->buff + 138)), eph.toes);
    eph.toc = gpst2time(eph.week, toc);
    eph.ttr = raw->time;

    if (!strstr(raw->opt, "-EPHALL")) {
        if (eph.iode == raw->nav.eph[sat - 1].iode) return 0;
    }
    eph.sat = sat;
    raw->nav.eph[sat - 1] = eph;
    raw->ephsat = sat;
    raw->ephset = 0;
    return 2;
}
/* decode SBF PVTGeodetic (4007) ---------------------------------------------*/
static int decode_pvtgeodetic(raw_t* raw) {
    uint8_t* p = raw->buff + 8;
    uint8_t mode, error;
    double lat, lon, hgt, pos[3];

    if (raw->len < 44) return 0;

    mode = U1(p + 6);
    error = U1(p + 7);

    /* bits 0-3: PVT type; 0 = no fix */
    if ((mode & 0x0F) == 0 || error != 0) return 0;

    lat = R8(p + 8);  /* rad */
    lon = R8(p + 16); /* rad */
    hgt = R8(p + 24); /* m, ellipsoidal */

    /* Do-Not-Use check (-2e10) */
    if (lat < -2.0e10 || lon < -2.0e10 || hgt < -2.0e10) return 0;

    pos[0] = lat;
    pos[1] = lon;
    pos[2] = hgt;
    pos2ecef(pos, raw->sta.pos);

    trace(NULL, 3, "SBF PVTGeodetic: lat=%.8f lon=%.8f hgt=%.3f mode=%d\n",
          lat / D2R, lon / D2R, hgt, mode & 0x0F);

    return 5; /* receiver position */
}
/* decode SBF block ----------------------------------------------------------*/
static int decode_sbf(raw_t* raw, rtcm_t* rtcm) {
    uint8_t* p = raw->buff;
    uint32_t week, tow;
    char tstr[32];
    int type = U2(p + 4) & 0x1fff;

    if (rtk_crc16(p + 4, raw->len - 4) != U2(p + 2)) {
        trace(NULL, 2, "sbf crc error: type=%d len=%d\n", type, raw->len);
        return -1;
    }
    if (raw->len < 14) {
        trace(NULL, 2, "sbf length error: type=%d len=%d\n", type, raw->len);
        return -1;
    }
    tow = U4(p + 8);
    week = U2(p + 12);
    if (tow == 4294967295u || week == 65535u) {
        trace(NULL, 2, "sbf tow/week error: type=%d len=%d\n", type, raw->len);
        return -1;
    }
    raw->time = gpst2time(week, tow * 0.001);

    if (raw->outtype) {
        time2str(raw->time, tstr, 2);
        sprintf(raw->msgtype, "SBF %4d (%4d): %s", type, raw->len, tstr);
    }
    switch (type) {
        case SBF_MEASEPOCH:
            return decode_measepoch(raw);
        case SBF_MEASEXTRA:
            return decode_measextra(raw);
        case SBF_GPSRAWCA:
            return decode_gpsrawca(raw);
        case SBF_GLORAWCA:
            return decode_glorawca(raw);
        case SBF_GALRAWFNAV:
            return decode_galrawfnav(raw);
        case SBF_GALRAWINAV:
            return decode_galrawinav(raw);
        case SBF_GEORAWL1:
            return decode_georawl1(raw);
        case SBF_BDSRAW:
            return decode_bdsraw(raw);
        case SBF_QZSRAWL1CA:
            return decode_qzsrawl1ca(raw);
        case SBF_QZSRAWL6:
            return decode_qzsrawl6(raw, rtcm);
        case SBF_QZSRAWL6D:
            return decode_qzsrawl6d(raw, rtcm);
        case SBF_NAVICRAW:
            return decode_navicraw(raw);
        /* decoded navigation blocks */
        case SBF_GPSNAV:
            return decode_gpsnav(raw);
        case SBF_GLONAV:
            return decode_glonav(raw);
        case SBF_GALNAV:
            return decode_galnav(raw);
        case SBF_BDSNAV:
            return decode_bdsnav(raw);
        case SBF_QZSSNAV:
            return decode_qzssnav(raw);
        /* receiver position */
        case SBF_PVTGEOD:
            return decode_pvtgeodetic(raw);
    }
    trace(NULL, 3, "sbf unsupported message: type=%d\n", type);
    return 0;
}
/* synchronize SBF block header ----------------------------------------------*/
static int sync_sbf(uint8_t* buff, uint8_t data) {
    buff[0] = buff[1];
    buff[1] = data;
    return buff[0] == SBF_SYNC1 && buff[1] == SBF_SYNC2;
}
/* input SBF raw data from stream ----------------------------------------------
 * fetch next SBF raw data and input a mesasge from stream
 * args   : raw_t *raw       IO  receiver raw data control struct
 *          rtcm_t *rtcm     IO  rtcm control struct
 *          uint_t data      I   stream data (1 byte)
 * return : status (-1: error message, 0: no message, 1: input observation data,
 *                  2: input ephemeris, 3: input sbas message,
 *                  9: input ion/utc parameter)
 *
 * notes  : supported SBF block (block ID):
 *
 *           MEASEPOCH(4027), GPSRAWCA(4017), GLORAWCA(4026), GALRAWFNAV(4022),
 *           GALRAWINAV(4023), GEORAWL1(4020), BDSRAW(4047), QZSRAWL1CA(4066),
 *           NAVICRAW(4093)
 *
 *          to specify input options for sbf, set raw->opt to the following
 *          option strings separated by spaces.
 *
 *          -EPHALL : input all ephemerides
 *          -AUX1   : select antenna Aux1  (default: main)
 *          -AUX2   : select antenna Aux2  (default: main)
 *          -GL1W   : select 1W for GPS L1 (default: 1C)
 *          -GL1L   : select 1L for GPS L1 (default: 1C)
 *          -GL2L   : select 2L for GPS L2 (default: 2W)
 *          -RL1P   : select 1P for GLO G1 (default: 1C)
 *          -RL2C   : select 2C for GLO G2 (default: 2P)
 *          -JL1L   : select 1L for QZS L1 (default: 1C)
 *          -JL1Z   : select 1Z for QZS L1 (default: 1C)
 *          -CL1P   : select 1P for BDS B1 (default: 2I)
 *          -GALINAV: select I/NAV for Galileo ephemeris (default: all)
 *          -GALFNAV: select F/NAV for Galileo ephemeris (default: all)
 *-----------------------------------------------------------------------------*/
extern int input_sbf(raw_t* raw, rtcm_t* rtcm, uint8_t data) {
    trace(NULL, 5, "input_sbf: data=%02x\n", data);

    if (raw->nbyte == 0) {
        if (sync_sbf(raw->buff, data)) {
            raw->nbyte = 2;
        }
        return 0;
    }
    raw->buff[raw->nbyte++] = data;
    if (raw->nbyte < 8) {
        return 0;
    }
    raw->len = U2(raw->buff + 6);

    if (raw->len <= 8 || raw->len > MAXRAWLEN) {
        trace(NULL, 2, "sbf length error: len=%d\n", raw->len);
        raw->nbyte = 0;
        return -1;
    }
    if (raw->nbyte < raw->len) {
        return 0;
    }
    raw->nbyte = 0;

    /* decode SBF block */
    return decode_sbf(raw, rtcm);
}
/* input SBF raw data from file ------------------------------------------------
 * fetch next SBF raw data and input a message from file
 * args   : raw_t *raw       IO  receiver raw data control struct
 *          rtcm_t *rtcm     IO  rtcm control struct
 *          FILE  *fp        I   file pointer
 * return : status(-2: end of file, -1...9: same as above)
 *-----------------------------------------------------------------------------*/
extern int input_sbff(raw_t* raw, rtcm_t* rtcm, FILE* fp) {
    int i, data;

    trace(NULL, 4, "input_sbff:\n");

    if (raw->nbyte == 0) {
        for (i = 0;; i++) {
            if ((data = fgetc(fp)) == EOF) {
                return -2;
            }
            if (sync_sbf(raw->buff, (uint8_t)data)) {
                break;
            }
            if (i >= 4096) {
                return 0;
            }
        }
    }
    if (fread(raw->buff + 2, 1, 6, fp) < 6) {
        return -2;
    }
    raw->nbyte = 8;
    raw->len = U2(raw->buff + 6);

    if (raw->len <= 8 || raw->len > MAXRAWLEN) {
        trace(NULL, 2, "sbf length error: len=%d\n", raw->len);
        raw->nbyte = 0;
        return -1;
    }
    if (fread(raw->buff + 8, raw->len - 8, 1, fp) < 1) {
        return -2;
    }
    raw->nbyte = 0;

    /* decode SBF block */
    return decode_sbf(raw, rtcm);
}
