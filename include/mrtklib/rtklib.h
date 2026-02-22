/*------------------------------------------------------------------------------
* rtklib.h : RTKLIB compatibility wrapper
*
* Copyright (C) 2024-2025 Japan Aerospace Exploration Agency. All Rights Reserved.
* Copyright (C) 2007-2021 by T.TAKASU, All rights reserved.
*
* This header is now a pure wrapper that includes all MRTKLIB modular headers.
* Legacy code can continue to #include "rtklib.h" and get the same types,
* constants, and function declarations as before.
*
* history : 2007/01/13 1.0  rtklib ver.1.0.0
*           ...
*           2025/02/22 2.0  reduced to pure include wrapper (Phase 1 complete)
*-----------------------------------------------------------------------------*/
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
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <pthread.h>
#include <sys/select.h>
#endif

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
