/**
 * @file mrtk_madoca_local_comb.h
 * @brief MRTKLIB MADOCA Local Combination Module — local correction data
 *        combination functions.
 *
 * This header provides functions for combining local tropospheric and
 * ionospheric correction data (grid interpolation, station selection,
 * and RTCM3 output), extracted from lclcmbcmn.c with zero algorithmic
 * changes.
 *
 * @note Functions declared here are implemented in mrtk_madoca_local_comb.c.
 */
#ifndef MRTK_MADOCA_LOCAL_COMB_H
#define MRTK_MADOCA_LOCAL_COMB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_rtcm.h"
#include "mrtklib/mrtk_madoca_local_corr.h"

/* forward-declare stream_t (defined in rtklib.h, not yet migrated) */
struct stream_tag;

/*============================================================================
 * Local Correction Data Combination Functions
 *===========================================================================*/

/**
 * @brief Initialize grid/station settings from a specified file.
 * @param[in]     setfile  Grid/station setting file path
 * @param[in,out] lclblk   Local block information struct
 * @param[in]     btype    Block type (BTYPE_GRID or BTYPE_STA)
 * @return Status (0: error, 1: success)
 */
int initgridsta(const char *setfile, lclblock_t *lclblk, int btype);

/**
 * @brief Interpolate tropospheric grid data.
 * @param[in]     gt     Time
 * @param[in]     stat   Station data
 * @param[in,out] lclblk Local block information
 */
void grid_intp_trop(const gtime_t gt, const stat_t *stat,
                    lclblock_t *lclblk);

/**
 * @brief Select and assign tropospheric data blocks based on station positions.
 * @param[in]     time   Time
 * @param[in]     stat   Station data
 * @param[in,out] lslblk Local block information struct
 */
void sta_sel_trop(const gtime_t time, const stat_t *stat,
                  lclblock_t *lslblk);

/**
 * @brief Interpolate ionospheric grid data.
 * @param[in]     gt     Time
 * @param[in]     stat   Station data
 * @param[in,out] lclblk Local block information struct
 * @param[in]     maxres Maximum residual
 * @param[out]    nrej   Number of rejections
 * @param[out]    ncnt   Number of counts
 */
void grid_intp_iono(const gtime_t gt, const stat_t *stat,
                    lclblock_t *lclblk, double maxres, int *nrej, int *ncnt);

/**
 * @brief Select and assign ionospheric data blocks based on station positions.
 * @param[in]     gt     Time
 * @param[in]     stat   Station data
 * @param[in,out] lslblk Local block information struct
 */
void sta_sel_iono(const gtime_t gt, const stat_t *stat,
                  lclblock_t *lslblk);

/**
 * @brief Generate and output RTCM3 local combination correction messages.
 * @param[in,out] rtcm   RTCM control struct
 * @param[in]     btype  Block type (BTYPE_GRID or BTYPE_STA)
 * @param[in]     ostr   Output stream
 */
void output_lclcmb(rtcm_t *rtcm, int btype, struct stream_tag *ostr);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_MADOCA_LOCAL_COMB_H */
