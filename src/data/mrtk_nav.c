/*------------------------------------------------------------------------------
 * mrtk_nav.c : navigation data functions
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
/**
 * @file mrtk_nav.c
 * @brief MRTKLIB Navigation Module — Implementation.
 *
 * All functions and static data in this file are cut-and-paste extractions
 * from the legacy rtkcmn.c with zero algorithmic changes.
 */

#include "mrtklib/mrtk_nav.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_trace.h"

/*============================================================================
 * Forward Declarations (resolved at link time)
 *===========================================================================*/

/*============================================================================
 * Private Constants
 *===========================================================================*/

/* SYS_* constants are defined in mrtk_const.h (included above) */

#define SQR(x) ((x) * (x))
#define MAX_VAR_EPH SQR(300.0) /* max variance eph to reject satellite (m^2) */

/*============================================================================
 * Public Functions
 *===========================================================================*/

/* satellite system+prn/slot number to satellite number ------------------------
 * convert satellite system+prn/slot number to satellite number
 * args   : int    sys       I   satellite system (SYS_GPS,SYS_GLO,...)
 *          int    prn       I   satellite prn/slot number
 * return : satellite number (0:error)
 *-----------------------------------------------------------------------------*/
extern int satno(int sys, int prn) {
    if (prn <= 0) {
        return 0;
    }
    switch (sys) {
        case SYS_GPS:
            if (prn < MINPRNGPS || MAXPRNGPS < prn) {
                return 0;
            }
            return prn - MINPRNGPS + 1;
        case SYS_GLO:
            if (prn < MINPRNGLO || MAXPRNGLO < prn) {
                return 0;
            }
            return NSATGPS + prn - MINPRNGLO + 1;
        case SYS_GAL:
            if (prn < MINPRNGAL || MAXPRNGAL < prn) {
                return 0;
            }
            return NSATGPS + NSATGLO + prn - MINPRNGAL + 1;
        case SYS_QZS:
            if (prn < MINPRNQZS || MAXPRNQZS < prn) {
                return 0;
            }
            return NSATGPS + NSATGLO + NSATGAL + prn - MINPRNQZS + 1;
        case SYS_CMP:
            if (prn < MINPRNCMP || MAXPRNCMP < prn) {
                return 0;
            }
            return NSATGPS + NSATGLO + NSATGAL + NSATQZS + prn - MINPRNCMP + 1;
        case SYS_IRN:
            if (prn < MINPRNIRN || MAXPRNIRN < prn) {
                return 0;
            }
            return NSATGPS + NSATGLO + NSATGAL + NSATQZS + NSATCMP + prn - MINPRNIRN + 1;
        case SYS_LEO:
            if (prn < MINPRNLEO || MAXPRNLEO < prn) {
                return 0;
            }
            return NSATGPS + NSATGLO + NSATGAL + NSATQZS + NSATCMP + NSATIRN + prn - MINPRNLEO + 1;
        case SYS_SBS:
            if (prn < MINPRNSBS || MAXPRNSBS < prn) {
                return 0;
            }
            return NSATGPS + NSATGLO + NSATGAL + NSATQZS + NSATCMP + NSATIRN + NSATLEO + prn - MINPRNSBS + 1;
    }
    return 0;
}
/* satellite number to satellite system ----------------------------------------
 * convert satellite number to satellite system
 * args   : int    sat       I   satellite number (1-MAXSAT)
 *          int    *prn      IO  satellite prn/slot number (NULL: no output)
 * return : satellite system (SYS_GPS,SYS_GLO,...)
 *-----------------------------------------------------------------------------*/
extern int satsys(int sat, int* prn) {
    int sys = SYS_NONE;
    if (sat <= 0 || MAXSAT < sat) {
        sat = 0;
    } else if (sat <= NSATGPS) {
        sys = SYS_GPS;
        sat += MINPRNGPS - 1;
    } else if ((sat -= NSATGPS) <= NSATGLO) {
        sys = SYS_GLO;
        sat += MINPRNGLO - 1;
    } else if ((sat -= NSATGLO) <= NSATGAL) {
        sys = SYS_GAL;
        sat += MINPRNGAL - 1;
    } else if ((sat -= NSATGAL) <= NSATQZS) {
        sys = SYS_QZS;
        sat += MINPRNQZS - 1;
    } else if ((sat -= NSATQZS) <= NSATCMP) {
        sys = SYS_CMP;
        sat += MINPRNCMP - 1;
    } else if ((sat -= NSATCMP) <= NSATIRN) {
        sys = SYS_IRN;
        sat += MINPRNIRN - 1;
    } else if ((sat -= NSATIRN) <= NSATLEO) {
        sys = SYS_LEO;
        sat += MINPRNLEO - 1;
    } else if ((sat -= NSATLEO) <= NSATSBS) {
        sys = SYS_SBS;
        sat += MINPRNSBS - 1;
    } else {
        sat = 0;
    }
    if (prn) {
        *prn = sat;
    }
    return sys;
}
/* convert satellite number to satellite system (BDS2-aware) -------------------
 * convert satellite number to satellite system, distinguishing BDS-2 from BDS-3
 * args   : int    sat       I   satellite number (1-MAXSAT)
 *          int    *prn      IO  satellite prn/slot number (NULL: no output)
 * return : satellite system (SYS_GPS,SYS_GLO,...,SYS_BD2,SYS_CMP)
 *-----------------------------------------------------------------------------*/
extern int satsys_bd2(int sat, int* prn) {
    int sys = SYS_NONE, _prn;

    sys = satsys(sat, &_prn);
#ifdef ENACMP
    if (sys == SYS_CMP && (_prn) < MINPRNBDS3) {
        sys = SYS_BD2;
    }
#endif

    if (prn) {
        *prn = _prn;
    }
    return sys;
}
/* observation definition type for frequency mapping ---------------------------*/
typedef struct {
    int freq_num;   /* frequency number [1,2,5,...] */
    double freq_hz; /* frequency [Hz] */
} obsdef_t;

static obsdef_t obsdef_GPS[MAXFREQ] = {
    /* freq-index:freq */
    {1, FREQ1},  /* 0:L1 (MRTK_BAND_GPS_L1) */
    {2, FREQ2},  /* 1:L2 (MRTK_BAND_GPS_L2) */
    {5, FREQ5},  /* 2:L5 (MRTK_BAND_GPS_L5) */
    {0, 0.0},    /* 3 */
    {0, 0.0},    /* 4 */
    {0, 0.0},    /* 5 */
    {0, 0.0},    /* 6 */
};
static obsdef_t obsdef_GLO[MAXFREQ] = {
    /* freq-index:freq */
    {1, FREQ1_GLO}, /* 0:G1 (MRTK_BAND_GLO_G1) */
    {2, FREQ2_GLO}, /* 1:G2 (MRTK_BAND_GLO_G2) */
    {0, 0.0},       /* 2 */
    {0, 0.0},       /* 3 */
    {0, 0.0},       /* 4 */
    {0, 0.0},       /* 5 */
    {0, 0.0},       /* 6 */
};
static obsdef_t obsdef_GAL[MAXFREQ] = {
    /* freq-index:freq */
    {1, FREQ1}, /* 0:E1  (MRTK_BAND_GAL_E1)  */
    {5, FREQ5}, /* 1:E5a (MRTK_BAND_GAL_E5a) */
    {7, FREQ7}, /* 2:E5b (MRTK_BAND_GAL_E5b) */
    {6, FREQ6}, /* 3:E6  (MRTK_BAND_GAL_E6)  */
    {8, FREQ8}, /* 4:E5ab(MRTK_BAND_GAL_E5ab)*/
    {0, 0.0},   /* 5 */
    {0, 0.0},   /* 6 */
};
static obsdef_t obsdef_QZS[MAXFREQ] = {
    /* freq-index:freq */
    {1, FREQ1}, /* 0:L1 (MRTK_BAND_QZS_L1) */
    {5, FREQ5}, /* 1:L5 (MRTK_BAND_QZS_L5) */
    {2, FREQ2}, /* 2:L2 (MRTK_BAND_QZS_L2) */
    {6, FREQ6}, /* 3:L6 (MRTK_BAND_QZS_L6) */
    {0, 0.0},   /* 4 */
    {0, 0.0},   /* 5 */
    {0, 0.0},   /* 6 */
};
static obsdef_t obsdef_SBS[MAXFREQ] = {
    /* freq-index:freq */
    {1, FREQ1}, /* 0:L1 (MRTK_BAND_SBS_L1) */
    {5, FREQ5}, /* 1:L5 (MRTK_BAND_SBS_L5) */
    {0, 0.0},   /* 2 */
    {0, 0.0},   /* 3 */
    {0, 0.0},   /* 4 */
    {0, 0.0},   /* 5 */
    {0, 0.0},   /* 6 */
};
static obsdef_t obsdef_BDS[MAXFREQ] = {
    /* freq-index:freq */
    {2, FREQ1_CMP}, /* 0:B1I    (MRTK_BAND_BDS_B1I)  */
    {6, FREQ3_CMP}, /* 1:B3I    (MRTK_BAND_BDS_B3)   */
    {7, FREQ2_CMP}, /* 2:B2I/B2b(MRTK_BAND_BDS_B2b)  */
    {1, FREQ1},     /* 3:B1C    (MRTK_BAND_BDS_B1C)  */
    {5, FREQ5},     /* 4:B2a    (MRTK_BAND_BDS_B2a)  */
    {8, FREQ8},     /* 5:B2     (MRTK_BAND_BDS_B2ab) */
    {0, 0.0},       /* 6 */
};
static obsdef_t obsdef_BD2[MAXFREQ] = {
    /* freq-index:freq */
    {2, FREQ1_CMP}, /* 0:B1I (MRTK_BAND_BDS_B1I) */
    {6, FREQ3_CMP}, /* 1:B3I (MRTK_BAND_BDS_B3)  */
    {7, FREQ2_CMP}, /* 2:B2I (MRTK_BAND_BDS_B2b) */
    {0, 0.0},       /* 3 */
    {0, 0.0},       /* 4 */
    {0, 0.0},       /* 5 */
    {0, 0.0},       /* 6 */
};
static obsdef_t obsdef_IRN[MAXFREQ] = {
    /* freq-index:freq */
    {5, FREQ5}, /* 0:L5 (MRTK_BAND_IRN_L5) */
    {9, FREQ9}, /* 1:S  (MRTK_BAND_IRN_S)  */
    {0, 0.0},   /* 2 */
    {0, 0.0},   /* 3 */
    {0, 0.0},   /* 4 */
    {0, 0.0},   /* 5 */
    {0, 0.0},   /* 6 */
};
/* helper: select obsdef table by system -------------------------------------*/
static obsdef_t* get_obsdef(int sys) {
    switch (sys) {
        case SYS_GPS:
            return obsdef_GPS;
        case SYS_GLO:
            return obsdef_GLO;
        case SYS_GAL:
            return obsdef_GAL;
        case SYS_QZS:
            return obsdef_QZS;
        case SYS_SBS:
            return obsdef_SBS;
        case SYS_CMP:
            return obsdef_BDS;
        case SYS_BD2:
            return obsdef_BD2;
        case SYS_IRN:
            return obsdef_IRN;
        default:
            return NULL;
    }
}
/* set observation definition by frequency number array -----------------------
 * rearrange obsdef table so that freq_nums[0..MAXFREQ-1] appear at indices
 * 0,1,2,... — used to apply pos2-sig* signal selection options.
 * args   : int    sys         I   satellite system (SYS_GPS,SYS_CMP,...)
 *          const int *freq_nums I  array of frequency numbers (1 x MAXFREQ)
 * return : none
 *-----------------------------------------------------------------------------*/
extern void set_obsdef(int sys, const int* freq_nums) {
    obsdef_t obsdef_tmp[MAXFREQ] = {{0}};
    obsdef_t* obsdef;
    obsdef_t obsdef0 = {0};
    int i, j;

    if (!(obsdef = get_obsdef(sys))) {
        return;
    }

    for (i = 0; i < MAXFREQ; i++) {
        obsdef_tmp[i] = obsdef[i];
        obsdef[i] = obsdef0;
    }
    for (i = 0; i < MAXFREQ; i++) {
        for (j = 0; j < MAXFREQ; j++) {
            if (obsdef_tmp[j].freq_num == freq_nums[i]) {
                break;
            }
        }
        if (j == MAXFREQ) {
            continue;
        }
        obsdef[i].freq_num = obsdef_tmp[j].freq_num;
        obsdef[i].freq_hz = obsdef_tmp[j].freq_hz;
    }
}
/* apply PPP signal selection options to obsdef tables -----------------------
 * rearrange obsdef tables based on pppsig[] options for madocalib PPP engine.
 * args   : const int *pppsig  I   signal selection [0]GPS,[1]QZS,[2]GAL,
 *                                 [3]BDS2,[4]BDS3 (from prcopt_t.pppsig)
 * return : none
 *-----------------------------------------------------------------------------*/
extern void apply_pppsig(const int* pppsig) {
    static const int fn_l1l2[MAXFREQ] = {1, 2, 0, 0, 0, 0, 0};
    static const int fn_l1l5[MAXFREQ] = {1, 5, 0, 0, 0, 0, 0};
    static const int fn_l1l2l5[MAXFREQ] = {1, 2, 5, 0, 0, 0, 0};
    static const int fn_l1l5l2[MAXFREQ] = {1, 5, 2, 0, 0, 0, 0};

    static const int fn_e1e5a[MAXFREQ] = {1, 5, 0, 0, 0, 0, 0};
    static const int fn_e1e5b[MAXFREQ] = {1, 7, 0, 0, 0, 0, 0};
    static const int fn_e1e6[MAXFREQ] = {1, 6, 0, 0, 0, 0, 0};
    static const int fn_e1e5ae5be6[MAXFREQ] = {1, 5, 7, 6, 0, 0, 0};
    static const int fn_e1e5ae6e5b[MAXFREQ] = {1, 5, 6, 7, 0, 0, 0};

    static const int fn_b1b3[MAXFREQ] = {2, 6, 0, 0, 0, 0, 0};
    static const int fn_b1b2i[MAXFREQ] = {2, 7, 0, 0, 0, 0, 0};
    static const int fn_b1b2a[MAXFREQ] = {2, 5, 0, 0, 0, 0, 0};
    static const int fn_b1b3b2i[MAXFREQ] = {2, 6, 7, 0, 0, 0, 0};
    static const int fn_b1b3b2a[MAXFREQ] = {2, 6, 5, 0, 0, 0, 0};

    /* GPS signal selection */
    switch (pppsig[0]) {
        case 0:
            set_obsdef(SYS_GPS, fn_l1l2);
            break;
        case 1:
            set_obsdef(SYS_GPS, fn_l1l5);
            break;
        case 2:
            set_obsdef(SYS_GPS, fn_l1l2l5);
            break;
    }
    /* QZS signal selection */
    switch (pppsig[1]) {
        case 0:
            set_obsdef(SYS_QZS, fn_l1l5);
            break;
        case 1:
            set_obsdef(SYS_QZS, fn_l1l2);
            break;
        case 2:
            set_obsdef(SYS_QZS, fn_l1l5l2);
            break;
    }
    /* GAL signal selection */
    switch (pppsig[2]) {
        case 0:
            set_obsdef(SYS_GAL, fn_e1e5a);
            break;
        case 1:
            set_obsdef(SYS_GAL, fn_e1e5b);
            break;
        case 2:
            set_obsdef(SYS_GAL, fn_e1e6);
            break;
        case 3:
            set_obsdef(SYS_GAL, fn_e1e5ae5be6);
            break;
        case 4:
            set_obsdef(SYS_GAL, fn_e1e5ae6e5b);
            break;
    }
    /* BDS-2 signal selection */
    switch (pppsig[3]) {
        case 0:
            set_obsdef(SYS_BD2, fn_b1b3);
            break;
        case 1:
            set_obsdef(SYS_BD2, fn_b1b2i);
            break;
        case 2:
            set_obsdef(SYS_BD2, fn_b1b3b2i);
            break;
    }
    /* BDS-3 signal selection */
    switch (pppsig[4]) {
        case 0:
            set_obsdef(SYS_CMP, fn_b1b3);
            break;
        case 1:
            set_obsdef(SYS_CMP, fn_b1b2a);
            break;
        case 2:
            set_obsdef(SYS_CMP, fn_b1b3b2a);
            break;
    }
}
/* convert frequency index to frequency number --------------------------------
 * args   : int    sys        I   satellite system
 *          int    freq_idx   I   frequency index (0,1,2,...)
 * return : frequency number (1,2,5,6,7,...) or 0 on error
 *-----------------------------------------------------------------------------*/
extern int freq_idx2freq_num(int sys, int freq_idx) {
    obsdef_t* obsdef = get_obsdef(sys);
    if (!obsdef || freq_idx < 0 || MAXFREQ <= freq_idx) {
        return 0;
    }
    return obsdef[freq_idx].freq_num;
}
/* convert frequency number to frequency index --------------------------------
 * args   : int    sys        I   satellite system
 *          int    freq_num   I   frequency number (1,2,5,6,7,...)
 * return : frequency index (0,1,2,...) or -1 on error
 *-----------------------------------------------------------------------------*/
extern int freq_num2freq_idx(int sys, int freq_num) {
    obsdef_t* obsdef = get_obsdef(sys);
    int i;
    if (!obsdef || freq_num <= 0) {
        return -1;
    }
    for (i = 0; i < MAXFREQ; i++) {
        if (obsdef[i].freq_num == freq_num) {
            return i;
        }
    }
    return -1;
}
/* convert frequency number to frequency (Hz) ---------------------------------
 * args   : int    sys        I   satellite system
 *          int    freq_num   I   frequency number (1,2,5,6,7,...)
 *          int    fcn        I   GLONASS frequency channel number (-7..6)
 * return : carrier frequency (Hz) or 0.0 on error
 *-----------------------------------------------------------------------------*/
extern double freq_num2freq(int sys, int freq_num, int fcn) {
    const double dfrq_glo[] = {DFRQ1_GLO, DFRQ2_GLO, 0.0};
    obsdef_t* obsdef = get_obsdef(sys);
    double freq = 0.0, dfrq = 0.0;
    int i;
    if (!obsdef || freq_num <= 0) {
        return 0.0;
    }

    if (sys == SYS_GLO) {
        if (fcn < -7 || fcn > 6) {
            return 0.0;
        }
        if (freq_num == 1 || freq_num == 2) {
            dfrq = dfrq_glo[freq_num - 1];
        }
    }
    for (i = 0; i < MAXFREQ; i++) {
        if (obsdef[i].freq_num == freq_num) {
            freq = obsdef[i].freq_hz + dfrq * fcn;
            break;
        }
    }
    return freq;
}
/* convert frequency number to antenna PCV index ------------------------------
 * args   : int    sys        I   satellite system
 *          int    freq_num   I   frequency number (1,2,5,6,7,...)
 * return : antenna PCV index (0..NFREQPCV-1)
 *-----------------------------------------------------------------------------*/
extern int freq_num2ant_idx(int sys, int freq_num) {
    int idx = NFREQPCV - 1;

    if ((sys & (SYS_GPS | SYS_GAL | SYS_QZS | SYS_SBS | SYS_CMP | SYS_BD2)) && (freq_num == 1)) {
        idx = 0;
    } else if ((sys & (SYS_GPS | SYS_QZS)) && (freq_num == 2)) {
        idx = 1;
    } else if ((sys & (SYS_GPS | SYS_GAL | SYS_QZS | SYS_SBS | SYS_CMP | SYS_BD2)) && (freq_num == 5)) {
        idx = 2;
    } else if ((sys & SYS_GLO) && (freq_num == 1 || freq_num == 4)) {
        idx = 3;
    } else if ((sys & (SYS_CMP | SYS_BD2)) && (freq_num == 2)) {
        idx = 4;
    } else if ((sys & (SYS_GAL | SYS_QZS)) && (freq_num == 6)) {
        idx = 5;
    } else if ((sys & (SYS_CMP | SYS_BD2)) && (freq_num == 6)) {
        idx = 6;
    } else if ((sys & SYS_GLO) && (freq_num == 2 || freq_num == 6)) {
        idx = 7;
    } else if ((sys & (SYS_GAL | SYS_CMP | SYS_BD2)) && (freq_num == 7)) {
        idx = 8;
    } else if ((sys & SYS_GLO) && (freq_num == 3)) {
        idx = 9;
    } else if ((sys & (SYS_GAL | SYS_CMP | SYS_BD2)) && (freq_num == 8)) {
        idx = 10;
    }
    return idx;
}
/* convert frequency index to antenna PCV index --------------------------------
 * args   : int    sys        I   satellite system
 *          int    freq_idx   I   frequency index (0,1,2,...)
 * return : antenna PCV index (0..NFREQPCV-1)
 *-----------------------------------------------------------------------------*/
extern int freq_idx2ant_idx(int sys, int freq_idx) {
    int fn = freq_idx2freq_num(sys, freq_idx);
    return freq_num2ant_idx(sys, fn);
}
/* satellite id to satellite number --------------------------------------------
 * convert satellite id to satellite number
 * args   : char   *id       I   satellite id (nn,Gnn,Rnn,Enn,Jnn,Cnn,Inn or Snn)
 * return : satellite number (0: error)
 * notes  : 120-142 and 193-199 are also recognized as sbas and qzss
 *-----------------------------------------------------------------------------*/
extern int satid2no(const char* id) {
    int sys, prn;
    char code;

    if (sscanf(id, "%d", &prn) == 1) {
        if (MINPRNGPS <= prn && prn <= MAXPRNGPS) {
            sys = SYS_GPS;
        } else if (MINPRNSBS <= prn && prn <= MAXPRNSBS) {
            sys = SYS_SBS;
        } else if (MINPRNQZS <= prn && prn <= MAXPRNQZS) {
            sys = SYS_QZS;
        } else {
            return 0;
        }
        return satno(sys, prn);
    }
    if (sscanf(id, "%c%d", &code, &prn) < 2) {
        return 0;
    }

    switch (code) {
        case 'G':
            sys = SYS_GPS;
            prn += MINPRNGPS - 1;
            break;
        case 'R':
            sys = SYS_GLO;
            prn += MINPRNGLO - 1;
            break;
        case 'E':
            sys = SYS_GAL;
            prn += MINPRNGAL - 1;
            break;
        case 'J':
            sys = SYS_QZS;
            prn += MINPRNQZS - 1;
            break;
        case 'C':
            sys = SYS_CMP;
            prn += MINPRNCMP - 1;
            break;
        case 'I':
            sys = SYS_IRN;
            prn += MINPRNIRN - 1;
            break;
        case 'L':
            sys = SYS_LEO;
            prn += MINPRNLEO - 1;
            break;
        case 'S':
            sys = SYS_SBS;
            prn += 100;
            break;
        default:
            return 0;
    }
    return satno(sys, prn);
}
/* satellite number to satellite id --------------------------------------------
 * convert satellite number to satellite id
 * args   : int    sat       I   satellite number
 *          char   *id       O   satellite id (Gnn,Rnn,Enn,Jnn,Cnn,Inn or nnn)
 * return : none
 *-----------------------------------------------------------------------------*/
extern void satno2id(int sat, char* id) {
    int prn;
    switch (satsys(sat, &prn)) {
        case SYS_GPS:
            sprintf(id, "G%02d", prn - MINPRNGPS + 1);
            return;
        case SYS_GLO:
            sprintf(id, "R%02d", prn - MINPRNGLO + 1);
            return;
        case SYS_GAL:
            sprintf(id, "E%02d", prn - MINPRNGAL + 1);
            return;
        case SYS_QZS:
            sprintf(id, "J%02d", prn - MINPRNQZS + 1);
            return;
        case SYS_CMP:
            sprintf(id, "C%02d", prn - MINPRNCMP + 1);
            return;
        case SYS_IRN:
            sprintf(id, "I%02d", prn - MINPRNIRN + 1);
            return;
        case SYS_LEO:
            sprintf(id, "L%02d", prn - MINPRNLEO + 1);
            return;
        case SYS_SBS:
            sprintf(id, "%03d", prn);
            return;
    }
    strcpy(id, "");
}
/* compare ephemeris ---------------------------------------------------------*/
static int cmpeph(const void* p1, const void* p2) {
    eph_t *q1 = (eph_t*)p1, *q2 = (eph_t*)p2;
    return q1->ttr.time != q2->ttr.time
               ? (int)(q1->ttr.time - q2->ttr.time)
               : (q1->toe.time != q2->toe.time ? (int)(q1->toe.time - q2->toe.time) : q1->sat - q2->sat);
}
/* sort and unique ephemeris -------------------------------------------------*/
static void uniqeph(nav_t* nav) {
    eph_t* nav_eph;
    int i, j;

    trace(NULL, 3, "uniqeph: n=%d\n", nav->n);

    if (nav->n <= 0) {
        return;
    }

    qsort(nav->eph, nav->n, sizeof(eph_t), cmpeph);

    for (i = 1, j = 0; i < nav->n; i++) {
        if (nav->eph[i].sat != nav->eph[j].sat || nav->eph[i].iode != nav->eph[j].iode ||
            nav->eph[i].type != nav->eph[j].type) {
            nav->eph[++j] = nav->eph[i];
        }
    }
    nav->n = j + 1;

    if (!(nav_eph = (eph_t*)realloc(nav->eph, sizeof(eph_t) * nav->n))) {
        trace(NULL, 1, "uniqeph malloc error n=%d\n", nav->n);
        free(nav->eph);
        nav->eph = NULL;
        nav->n = nav->nmax = 0;
        return;
    }
    nav->eph = nav_eph;
    nav->nmax = nav->n;

    trace(NULL, 4, "uniqeph: n=%d\n", nav->n);
}
/* compare glonass ephemeris -------------------------------------------------*/
static int cmpgeph(const void* p1, const void* p2) {
    geph_t *q1 = (geph_t*)p1, *q2 = (geph_t*)p2;
    return q1->tof.time != q2->tof.time
               ? (int)(q1->tof.time - q2->tof.time)
               : (q1->toe.time != q2->toe.time ? (int)(q1->toe.time - q2->toe.time) : q1->sat - q2->sat);
}
/* sort and unique glonass ephemeris -----------------------------------------*/
static void uniqgeph(nav_t* nav) {
    geph_t* nav_geph;
    int i, j;

    trace(NULL, 3, "uniqgeph: ng=%d\n", nav->ng);

    if (nav->ng <= 0) {
        return;
    }

    qsort(nav->geph, nav->ng, sizeof(geph_t), cmpgeph);

    for (i = j = 0; i < nav->ng; i++) {
        if (nav->geph[i].sat != nav->geph[j].sat || nav->geph[i].toe.time != nav->geph[j].toe.time ||
            nav->geph[i].svh != nav->geph[j].svh) {
            nav->geph[++j] = nav->geph[i];
        }
    }
    nav->ng = j + 1;

    if (!(nav_geph = (geph_t*)realloc(nav->geph, sizeof(geph_t) * nav->ng))) {
        trace(NULL, 1, "uniqgeph malloc error ng=%d\n", nav->ng);
        free(nav->geph);
        nav->geph = NULL;
        nav->ng = nav->ngmax = 0;
        return;
    }
    nav->geph = nav_geph;
    nav->ngmax = nav->ng;

    trace(NULL, 4, "uniqgeph: ng=%d\n", nav->ng);
}
/* compare sbas ephemeris ----------------------------------------------------*/
static int cmpseph(const void* p1, const void* p2) {
    seph_t *q1 = (seph_t*)p1, *q2 = (seph_t*)p2;
    return q1->tof.time != q2->tof.time
               ? (int)(q1->tof.time - q2->tof.time)
               : (q1->t0.time != q2->t0.time ? (int)(q1->t0.time - q2->t0.time) : q1->sat - q2->sat);
}
/* sort and unique sbas ephemeris --------------------------------------------*/
static void uniqseph(nav_t* nav) {
    seph_t* nav_seph;
    int i, j;

    trace(NULL, 3, "uniqseph: ns=%d\n", nav->ns);

    if (nav->ns <= 0) {
        return;
    }

    qsort(nav->seph, nav->ns, sizeof(seph_t), cmpseph);

    for (i = j = 0; i < nav->ns; i++) {
        if (nav->seph[i].sat != nav->seph[j].sat || nav->seph[i].t0.time != nav->seph[j].t0.time) {
            nav->seph[++j] = nav->seph[i];
        }
    }
    nav->ns = j + 1;

    if (!(nav_seph = (seph_t*)realloc(nav->seph, sizeof(seph_t) * nav->ns))) {
        trace(NULL, 1, "uniqseph malloc error ns=%d\n", nav->ns);
        free(nav->seph);
        nav->seph = NULL;
        nav->ns = nav->nsmax = 0;
        return;
    }
    nav->seph = nav_seph;
    nav->nsmax = nav->ns;

    trace(NULL, 4, "uniqseph: ns=%d\n", nav->ns);
}
/* unique ephemerides ----------------------------------------------------------
 * unique ephemerides in navigation data
 * args   : nav_t *nav    IO     navigation data
 * return : number of epochs
 *-----------------------------------------------------------------------------*/
extern void uniqnav(nav_t* nav) {
    trace(NULL, 3, "uniqnav: neph=%d ngeph=%d nseph=%d\n", nav->n, nav->ng, nav->ns);

    /* unique ephemeris */
    uniqeph(nav);
    uniqgeph(nav);
    uniqseph(nav);
}
/* read/save navigation data ---------------------------------------------------
 * save or load navigation data
 * args   : char    file  I      file path
 *          nav_t   nav   O/I    navigation data
 * return : status (1:ok,0:no file)
 *-----------------------------------------------------------------------------*/
extern int readnav(const char* file, nav_t* nav) {
    FILE* fp;
    eph_t eph0 = {0};
    geph_t geph0 = {0};
    char buff[4096], *p;
    long toe_time, tof_time, toc_time, ttr_time;
    int i, sat, prn;

    trace(NULL, 3, "loadnav: file=%s\n", file);

    if (!(fp = fopen(file, "r"))) {
        return 0;
    }

    while (fgets(buff, sizeof(buff), fp)) {
        if (!strncmp(buff, "IONUTC", 6)) {
            for (i = 0; i < 8; i++) {
                nav->ion_gps[i] = 0.0;
            }
            for (i = 0; i < 8; i++) {
                nav->utc_gps[i] = 0.0;
            }
            sscanf(buff, "IONUTC,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", &nav->ion_gps[0],
                   &nav->ion_gps[1], &nav->ion_gps[2], &nav->ion_gps[3], &nav->ion_gps[4], &nav->ion_gps[5],
                   &nav->ion_gps[6], &nav->ion_gps[7], &nav->utc_gps[0], &nav->utc_gps[1], &nav->utc_gps[2],
                   &nav->utc_gps[3], &nav->utc_gps[4]);
            continue;
        }
        if ((p = strchr(buff, ','))) {
            *p = '\0';
        } else {
            continue;
        }
        if (!(sat = satid2no(buff))) {
            continue;
        }
        if (satsys(sat, &prn) == SYS_GLO) {
            nav->geph[prn - 1] = geph0;
            nav->geph[prn - 1].sat = sat;
            toe_time = tof_time = 0;
            sscanf(p + 1,
                   "%d,%d,%d,%d,%d,%ld,%ld,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,"
                   "%lf,%lf,%lf,%lf",
                   &nav->geph[prn - 1].iode, &nav->geph[prn - 1].frq, &nav->geph[prn - 1].svh, &nav->geph[prn - 1].sva,
                   &nav->geph[prn - 1].age, &toe_time, &tof_time, &nav->geph[prn - 1].pos[0],
                   &nav->geph[prn - 1].pos[1], &nav->geph[prn - 1].pos[2], &nav->geph[prn - 1].vel[0],
                   &nav->geph[prn - 1].vel[1], &nav->geph[prn - 1].vel[2], &nav->geph[prn - 1].acc[0],
                   &nav->geph[prn - 1].acc[1], &nav->geph[prn - 1].acc[2], &nav->geph[prn - 1].taun,
                   &nav->geph[prn - 1].gamn, &nav->geph[prn - 1].dtaun);
            nav->geph[prn - 1].toe.time = toe_time;
            nav->geph[prn - 1].tof.time = tof_time;
        } else {
            nav->eph[sat - 1] = eph0;
            nav->eph[sat - 1].sat = sat;
            toe_time = toc_time = ttr_time = 0;
            sscanf(p + 1,
                   "%d,%d,%d,%d,%ld,%ld,%ld,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,"
                   "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%d,%d",
                   &nav->eph[sat - 1].iode, &nav->eph[sat - 1].iodc, &nav->eph[sat - 1].sva, &nav->eph[sat - 1].svh,
                   &toe_time, &toc_time, &ttr_time, &nav->eph[sat - 1].A, &nav->eph[sat - 1].e, &nav->eph[sat - 1].i0,
                   &nav->eph[sat - 1].OMG0, &nav->eph[sat - 1].omg, &nav->eph[sat - 1].M0, &nav->eph[sat - 1].deln,
                   &nav->eph[sat - 1].OMGd, &nav->eph[sat - 1].idot, &nav->eph[sat - 1].crc, &nav->eph[sat - 1].crs,
                   &nav->eph[sat - 1].cuc, &nav->eph[sat - 1].cus, &nav->eph[sat - 1].cic, &nav->eph[sat - 1].cis,
                   &nav->eph[sat - 1].toes, &nav->eph[sat - 1].fit, &nav->eph[sat - 1].f0, &nav->eph[sat - 1].f1,
                   &nav->eph[sat - 1].f2, &nav->eph[sat - 1].tgd[0], &nav->eph[sat - 1].code, &nav->eph[sat - 1].flag);
            nav->eph[sat - 1].toe.time = toe_time;
            nav->eph[sat - 1].toc.time = toc_time;
            nav->eph[sat - 1].ttr.time = ttr_time;
        }
    }
    fclose(fp);
    return 1;
}
extern int savenav(const char* file, const nav_t* nav) {
    FILE* fp;
    int i;
    char id[32];

    trace(NULL, 3, "savenav: file=%s\n", file);

    if (!(fp = fopen(file, "w"))) {
        return 0;
    }

    for (i = 0; i < MAXSAT; i++) {
        if (nav->eph[i].ttr.time == 0) {
            continue;
        }
        satno2id(nav->eph[i].sat, id);
        fprintf(fp,
                "%s,%d,%d,%d,%d,%d,%d,%d,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,"
                "%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,"
                "%.14E,%.14E,%.14E,%.14E,%.14E,%d,%d\n",
                id, nav->eph[i].iode, nav->eph[i].iodc, nav->eph[i].sva, nav->eph[i].svh, (int)nav->eph[i].toe.time,
                (int)nav->eph[i].toc.time, (int)nav->eph[i].ttr.time, nav->eph[i].A, nav->eph[i].e, nav->eph[i].i0,
                nav->eph[i].OMG0, nav->eph[i].omg, nav->eph[i].M0, nav->eph[i].deln, nav->eph[i].OMGd, nav->eph[i].idot,
                nav->eph[i].crc, nav->eph[i].crs, nav->eph[i].cuc, nav->eph[i].cus, nav->eph[i].cic, nav->eph[i].cis,
                nav->eph[i].toes, nav->eph[i].fit, nav->eph[i].f0, nav->eph[i].f1, nav->eph[i].f2, nav->eph[i].tgd[0],
                nav->eph[i].code, nav->eph[i].flag);
    }
    for (i = 0; i < MAXPRNGLO; i++) {
        if (nav->geph[i].tof.time == 0) {
            continue;
        }
        satno2id(nav->geph[i].sat, id);
        fprintf(fp,
                "%s,%d,%d,%d,%d,%d,%d,%d,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,"
                "%.14E,%.14E,%.14E,%.14E,%.14E,%.14E\n",
                id, nav->geph[i].iode, nav->geph[i].frq, nav->geph[i].svh, nav->geph[i].sva, nav->geph[i].age,
                (int)nav->geph[i].toe.time, (int)nav->geph[i].tof.time, nav->geph[i].pos[0], nav->geph[i].pos[1],
                nav->geph[i].pos[2], nav->geph[i].vel[0], nav->geph[i].vel[1], nav->geph[i].vel[2], nav->geph[i].acc[0],
                nav->geph[i].acc[1], nav->geph[i].acc[2], nav->geph[i].taun, nav->geph[i].gamn, nav->geph[i].dtaun);
    }
    fprintf(fp,
            "IONUTC,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,%.14E,"
            "%.14E,%.14E,%.14E,%.0f",
            nav->ion_gps[0], nav->ion_gps[1], nav->ion_gps[2], nav->ion_gps[3], nav->ion_gps[4], nav->ion_gps[5],
            nav->ion_gps[6], nav->ion_gps[7], nav->utc_gps[0], nav->utc_gps[1], nav->utc_gps[2], nav->utc_gps[3],
            nav->utc_gps[4]);

    fclose(fp);
    return 1;
}
/* free navigation data ---------------------------------------------------------
 * free memory for navigation data
 * args   : nav_t *nav    IO     navigation data
 *          int   opt     I      option (or of followings)
 *                               (0x01: gps/qzs ephmeris, 0x02: glonass ephemeris,
 *                                0x04: sbas ephemeris,   0x08: precise ephemeris,
 *                                0x10: precise clock     0x20: almanac,
 *                                0x40: tec data          0x80: pppiono data)
 * return : none
 *-----------------------------------------------------------------------------*/
extern void freenav(nav_t* nav, int opt) {
    if (opt & 0x01) {
        free(nav->eph);
        nav->eph = NULL;
        nav->n = nav->nmax = 0;
    }
    if (opt & 0x02) {
        free(nav->geph);
        nav->geph = NULL;
        nav->ng = nav->ngmax = 0;
    }
    if (opt & 0x04) {
        free(nav->seph);
        nav->seph = NULL;
        nav->ns = nav->nsmax = 0;
    }
    if (opt & 0x08) {
        free(nav->peph);
        nav->peph = NULL;
        nav->ne = nav->nemax = 0;
    }
    if (opt & 0x10) {
        free(nav->pclk);
        nav->pclk = NULL;
        nav->nc = nav->ncmax = 0;
    }
    if (opt & 0x20) {
        free(nav->alm);
        nav->alm = NULL;
        nav->na = nav->namax = 0;
    }
    if (opt & 0x40) {
        free(nav->tec);
        nav->tec = NULL;
        nav->nt = nav->ntmax = 0;
    }
    if (opt & 0x80) {
        free(nav->pppiono);
        nav->pppiono = NULL;
    }
}
/* test excluded satellite ------------------------------------------------------
 * test excluded satellite
 * args   : int    sat       I   satellite number
 *          double var       I   variance of ephemeris (m^2)
 *          int    svh       I   sv health flag
 *          prcopt_t *opt    I   processing options (NULL: not used)
 * return : status (1:excluded,0:not excluded)
 *-----------------------------------------------------------------------------*/
int satexclude(int sat, double var, int svh, const struct prcopt_t* opt) {
    int sys = satsys(sat, NULL);

    if (svh < 0) {
        return 1; /* ephemeris unavailable */
    }

    if (opt) {
        if (opt->exsats[sat - 1] == 1) {
            return 1; /* excluded satellite */
        }
        if (opt->exsats[sat - 1] == 2) {
            return 0; /* included satellite */
        }
        if (!(sys & opt->navsys)) {
            return 1; /* unselected sat sys */
        }
    }
    if (sys == SYS_QZS) {
        svh &= 0xFE; /* mask QZSS LEX health */
    }
    if (sys == SYS_CMP) {
        svh &= ~0x1D; /* mask BeiDou health by reserved bit */
    }
    if (sys == SYS_GLO) {
        /* GLONASS ICD health: bit0=L1 bad, bit3=sat malfunction,
           bits1-2=01 (binary) means L2 bad — all three cases excluded */
        if ((svh & 9) != 0 || (svh & 6) == 4) {
            trace(NULL, 3, "unhealthy satellite: sat=%3d svh=%02X\n", sat, svh);
            return 1;
        }
    } else if (svh) {
        trace(NULL, 3, "unhealthy satellite: sat=%3d svh=%02X\n", sat, svh);
        return 1;
    }
    if (var > MAX_VAR_EPH) {
        trace(NULL, 3, "invalid ura satellite: sat=%3d ura=%.2f\n", sat, sqrt(var));
        return 1;
    }
    return 0;
}
