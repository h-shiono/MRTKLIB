/*------------------------------------------------------------------------------
 * rtklib.h : RTKLIB compatibility wrapper (includes all MRTKLIB headers)
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#ifndef RTKLIB_H
#define RTKLIB_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/select.h>

/* MRTKLIB modular headers */
#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_const.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_mat.h"
#include "mrtklib/mrtk_coords.h"
#include "mrtklib/mrtk_atmos.h"
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_peph.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_bits.h"
#include "mrtklib/mrtk_sys.h"
#include "mrtklib/mrtk_astro.h"
#include "mrtklib/mrtk_antenna.h"
#include "mrtklib/mrtk_station.h"
#include "mrtklib/mrtk_rinex.h"
#include "mrtklib/mrtk_tides.h"
#include "mrtklib/mrtk_geoid.h"
#include "mrtklib/mrtk_sbas.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_ionex.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_sol.h"
#include "mrtklib/mrtk_bias_sinex.h"
#include "mrtklib/mrtk_fcb.h"
#include "mrtklib/mrtk_spp.h"
#include "mrtklib/mrtk_madoca_local_corr.h"
#include "mrtklib/mrtk_madoca_local_comb.h"
#include "mrtklib/mrtk_madoca.h"
#include "mrtklib/mrtk_lambda.h"
#include "mrtklib/mrtk_rtkpos.h"
#include "mrtklib/mrtk_ppp_ar.h"
#include "mrtklib/mrtk_ppp.h"
#include "mrtklib/mrtk_postpos.h"
#include "mrtklib/mrtk_options.h"
#include "mrtklib/mrtk_rcvraw.h"
#include "mrtklib/mrtk_stream.h"
#include "mrtklib/mrtk_rtksvr.h"
#include "mrtklib/mrtk_trace.h"

#endif /* RTKLIB_H */
