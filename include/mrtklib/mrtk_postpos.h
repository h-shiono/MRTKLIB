/*------------------------------------------------------------------------------
 * mrtk_postpos.h : post-processing positioning functions
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
/**
 * @file mrtk_postpos.h
 * @brief MRTKLIB Post-Processing Positioning Module — Post-processing
 *        positioning orchestrator.
 *
 * This header provides the post-processing positioning function that
 * orchestrates forward/backward filtering and combined solutions.
 * Extracted from the legacy postpos.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_postpos.c.
 */
#ifndef MRTK_POSTPOS_H
#define MRTK_POSTPOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_context.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_opt.h"

/**
 * @brief Post-processing positioning.
 * @param[in] ctx     Runtime context (NULL = use global)
 * @param[in] ts      Processing start time (ts.time==0: no limit)
 * @param[in] te      Processing end time   (te.time==0: no limit)
 * @param[in] ti      Processing interval  (s) (0:all)
 * @param[in] tu      Processing unit time (s) (0:all)
 * @param[in] popt    Processing options
 * @param[in] sopt    Solution options
 * @param[in] fopt    File options
 * @param[in] infile  Input files
 * @param[in] n       Number of input files
 * @param[in] outfile Output file ("":stdout)
 * @param[in] rov     Rover id list (separated by " ")
 * @param[in] base    Base station id list (separated by " ")
 * @return Status (0:ok, 0>:error, 1:aborted)
 */
int postpos(mrtk_ctx_t *ctx, gtime_t ts, gtime_t te, double ti, double tu,
            const prcopt_t *popt, const solopt_t *sopt,
            const filopt_t *fopt, char **infile, int n, char *outfile,
            const char *rov, const char *base);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_POSTPOS_H */
