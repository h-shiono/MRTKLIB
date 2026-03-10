/*------------------------------------------------------------------------------
 * mrtk_toml.h : TOML configuration file loader for MRTKLIB
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
#ifndef MRTK_TOML_H
#define MRTK_TOML_H

#include "mrtklib/mrtk_options.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load options from a TOML configuration file.
 * @param[in]  file  Path to the TOML file.
 * @param[out] opts  Options table (sysopts[]-compatible, terminated by empty name).
 * @return 1 on success, 0 on error.
 *
 * Reads a TOML file and maps semantic section.key paths to legacy option names
 * in the opts table. Unknown TOML keys are silently skipped.
 */
extern int loadopts_toml(const char *file, opt_t *opts);

/**
 * @brief Save options to a TOML configuration file.
 * @param[in] file     Path to the output TOML file.
 * @param[in] comment  Header comment (NULL for none).
 * @param[in] opts     Options table (terminated by empty name).
 * @return 1 on success, 0 on error.
 */
extern int saveopts_toml(const char *file, const char *comment,
                         const opt_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_TOML_H */
