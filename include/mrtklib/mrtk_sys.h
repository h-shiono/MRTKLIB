/*------------------------------------------------------------------------------
 * mrtk_sys.h : system utility functions
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
/**
 * @file mrtk_sys.h
 * @brief MRTKLIB System Module — OS/filesystem utility functions.
 *
 * This header provides system-level utility functions for command
 * execution, file path expansion, directory creation, path keyword
 * replacement, string-to-number conversion, and file decompression,
 * extracted from rtkcmn.c with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_sys.c.
 */
#ifndef MRTK_SYS_H
#define MRTK_SYS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_time.h"

/*============================================================================
 * String Conversion Functions
 *===========================================================================*/

/**
 * @brief Convert substring in string to number.
 * @param[in] s  String ("... nnn.nnn ...")
 * @param[in] i  Substring position
 * @param[in] n  Substring width
 * @return Converted number (0.0 on error)
 */
double str2num(const char *s, int i, int n);

/*============================================================================
 * Command / Path Functions
 *===========================================================================*/

/**
 * @brief Execute command line by operating system shell.
 * @param[in] cmd  Command line string
 * @return Execution status (0:ok, >0:error)
 */
int execcmd(const char *cmd);

/**
 * @brief Expand file path with wild-card (*) in file.
 * @param[in]  path   File path to expand (case insensitive)
 * @param[out] paths  Expanded file paths
 * @param[in]  nmax   Max number of expanded file paths
 * @return Number of expanded file paths
 * @note The order of expanded files is alphabetical order
 */
int expath(const char *path, char *paths[], int nmax);

/**
 * @brief Create directory if not exists (recursively).
 * @param[in] path  File path to be saved
 */
void createdir(const char *path);

/**
 * @brief Replace keywords in file path with date, time, rover and base
 *        station id.
 * @param[in]  path   File path with keywords (see notes)
 * @param[out] rpath  File path with keywords replaced
 * @param[in]  time   Time (GPST) (time.time==0: not replaced)
 * @param[in]  rov    Rover id string ("": not replaced)
 * @param[in]  base   Base station id string ("": not replaced)
 * @return Status (1:keywords replaced, 0:no valid keyword, -1:no valid time)
 * @note Keywords: %Y(yyyy), %y(yy), %m(mm), %d(dd), %h(hh), %M(mm),
 *       %S(ss), %n(ddd), %W(wwww), %D(d), %H(h), %ha(hh), %hb(hh),
 *       %hc(hh), %t(mm), %r(rover), %b(base)
 */
int reppath(const char *path, char *rpath, gtime_t time, const char *rov,
            const char *base);

/**
 * @brief Replace keywords in file path and generate multiple paths.
 * @param[in]  path   File path with keywords
 * @param[out] rpath  File paths with keywords replaced
 * @param[in]  nmax   Max number of output file paths
 * @param[in]  ts     Time start (GPST)
 * @param[in]  te     Time end (GPST)
 * @param[in]  rov    Rover id string ("": not replaced)
 * @param[in]  base   Base station id string ("": not replaced)
 * @return Number of replaced file paths
 */
int reppaths(const char *path, char *rpath[], int nmax, gtime_t ts,
             gtime_t te, const char *rov, const char *base);

/*============================================================================
 * File Compression Functions
 *===========================================================================*/

/**
 * @brief Uncompress file (gzip/zip/tar/Hatanaka).
 * @param[in]  file     Input file
 * @param[out] uncfile  Uncompressed file path
 * @return Status (-1:error, 0:not compressed file, 1:uncompress completed)
 * @note Creates uncompressed file in temporary directory.
 *       gzip, tar and crx2rnx commands must be installed.
 */
int rtk_uncompress(const char *file, char *uncfile);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_SYS_H */
