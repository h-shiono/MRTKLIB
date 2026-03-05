/*------------------------------------------------------------------------------
 * mrtk_options.h : option string processing functions
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
 * @file mrtk_options.h
 * @brief MRTKLIB Options Module — Configuration option parsing and system
 *        option management.
 *
 * This header provides the opt_t type for option table entries, functions
 * for loading/saving/converting options, and the system options interface
 * (sysopts, getsysopts, setsysopts, resetsysopts).
 * Extracted from the legacy options.c / rtklib.h with zero algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_options.c.
 */
#ifndef MRTK_OPTIONS_H
#define MRTK_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_opt.h"

/*============================================================================
 * Option Table Entry Type
 *===========================================================================*/

/**
 * @brief Option type for configuration file parsing.
 */
typedef struct {
    const char *name;   /**< option name */
    int format;         /**< option format (0:int,1:double,2:string,3:enum) */
    void *var;          /**< pointer to option variable */
    const char *comment; /**< option comment/enum labels/unit */
} opt_t;

/*============================================================================
 * Global Variables
 *===========================================================================*/

/**
 * @brief System options table.
 * @note Terminated with an entry where name[0]=='\\0'.
 */
extern opt_t sysopts[];

/*============================================================================
 * Option Functions
 *===========================================================================*/

/**
 * @brief Search option record.
 * @param[in] name  Option name
 * @param[in] opts  Options table (terminated with name="")
 * @return Option record (NULL: not found)
 */
opt_t *searchopt(const char *name, const opt_t *opts);

/**
 * @brief Convert string to option value.
 * @param[out] opt  Option
 * @param[in]  str  Option value string
 * @return Status (1:ok, 0:error)
 */
int str2opt(opt_t *opt, const char *str);

/**
 * @brief Convert option value to string.
 * @param[in]  opt  Option
 * @param[out] str  Option value string
 * @return Length of output string
 */
int opt2str(const opt_t *opt, char *str);

/**
 * @brief Convert option to string (keyword=value # comment).
 * @param[in]  opt   Option
 * @param[out] buff  Option string
 * @return Length of output string
 */
int opt2buf(const opt_t *opt, char *buff);

/**
 * @brief Load options from file.
 * @param[in]    file  Options file
 * @param[in,out] opts  Options table (terminated with name="")
 * @return Status (1:ok, 0:error)
 */
int loadopts(const char *file, opt_t *opts);

/**
 * @brief Save options to file.
 * @param[in] file     Options file
 * @param[in] mode     Write mode ("w":overwrite, "a":append)
 * @param[in] comment  Header comment (NULL: no comment)
 * @param[in] opts     Options table (terminated with name="")
 * @return Status (1:ok, 0:error)
 */
int saveopts(const char *file, const char *mode, const char *comment,
             const opt_t *opts);

/**
 * @brief Reset system options to default.
 */
void resetsysopts(void);

/**
 * @brief Get system options.
 * @param[out] popt  Processing options (NULL: no output)
 * @param[out] sopt  Solution options   (NULL: no output)
 * @param[out] fopt  File options       (NULL: no output)
 */
void getsysopts(prcopt_t *popt, solopt_t *sopt, filopt_t *fopt);

/**
 * @brief Set system options.
 * @param[in] prcopt  Processing options (NULL: default)
 * @param[in] solopt  Solution options   (NULL: default)
 * @param[in] filopt  File options       (NULL: default)
 */
void setsysopts(const prcopt_t *prcopt, const solopt_t *solopt,
                const filopt_t *filopt);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_OPTIONS_H */
