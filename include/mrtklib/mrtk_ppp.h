/**
 * @file mrtk_ppp.h
 * @brief MRTKLIB Precise Point Positioning (PPP) Module.
 *
 * This header provides the PPP positioning functions including the main
 * PPP filter (pppos), number-of-states query (pppnx), and solution
 * status output (pppoutstat).
 *
 * @note Functions declared here are implemented in mrtk_ppp.c.
 */
#ifndef MRTK_PPP_H
#define MRTK_PPP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_rtkpos.h"

/**
 * @brief Precise point positioning.
 * @param[in,out] rtk  RTK control/result struct
 * @param[in]     obs  Observation data
 * @param[in]     n    Number of observation data
 * @param[in]     nav  Navigation data
 */
void pppos(rtk_t *rtk, const obsd_t *obs, int n, const nav_t *nav);

/**
 * @brief Number of estimated states for PPP.
 * @param[in] opt  Processing options
 * @return Number of states
 */
int pppnx(const prcopt_t *opt);

/**
 * @brief Write PPP solution status to buffer.
 * @param[in,out] rtk   RTK control/result struct
 * @param[out]    buff  Output buffer
 * @return Number of bytes written
 */
int pppoutstat(rtk_t *rtk, char *buff);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_PPP_H */
