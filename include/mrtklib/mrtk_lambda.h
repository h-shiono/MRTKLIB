/*------------------------------------------------------------------------------
 * mrtk_lambda.h : integer ambiguity resolution (LAMBDA) functions
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
 * @file mrtk_lambda.h
 * @brief MRTKLIB Integer Ambiguity Resolution Module — LAMBDA method.
 *
 * This header provides functions for integer least-squares estimation using
 * the LAMBDA (Least-squares AMBiguity Decorrelation Adjustment) method and
 * the modified LAMBDA (MLAMBDA) search algorithm.
 *
 * @note Functions declared here are implemented in mrtk_lambda.c.
 *
 * References:
 *   [1] P.J.G.Teunissen, The least-square ambiguity decorrelation adjustment:
 *       a method for fast GPS ambiguity estimation, J.Geodesy, Vol.70, 65-82,
 *       1995
 *   [2] X.-W.Chang, X.Yang, T.Zhou, MLAMBDA: A modified LAMBDA method for
 *       integer least-squares estimation, J.Geodesy, Vol.79, 552-565, 2005
 */
#ifndef MRTK_LAMBDA_H
#define MRTK_LAMBDA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"

/**
 * @brief Integer least-square estimation (LAMBDA/MLAMBDA).
 *
 * Reduction is performed by LAMBDA (ref.[1]), and search by MLAMBDA (ref.[2]).
 *
 * @param[in]  n  Number of float parameters
 * @param[in]  m  Number of fixed solutions
 * @param[in]  a  Float parameters (n x 1)
 * @param[in]  Q  Covariance matrix of float parameters (n x n)
 * @param[out] F  Fixed solutions (n x m)
 * @param[out] s  Sum of squared residuals of fixed solutions (1 x m)
 * @return Status (0: ok, other: error)
 * @note Matrix stored by column-major order (Fortran convention).
 */
int lambda(int n, int m, const double *a, const double *Q, double *F,
           double *s);

/**
 * @brief LAMBDA reduction for integer least-squares.
 *
 * Performs decorrelation reduction on the covariance matrix using the
 * LAMBDA method (ref.[1]).
 *
 * @param[in]  n  Number of float parameters
 * @param[in]  Q  Covariance matrix of float parameters (n x n)
 * @param[out] Z  LAMBDA reduction matrix (n x n)
 * @return Status (0: ok, other: error)
 */
int lambda_reduction(int n, const double *Q, double *Z);

/**
 * @brief MLAMBDA search for integer least-squares.
 *
 * Performs the modified LAMBDA search algorithm (ref.[2]) for integer
 * least-squares estimation.
 *
 * @param[in]  n  Number of float parameters
 * @param[in]  m  Number of fixed solutions
 * @param[in]  a  Float parameters (n x 1)
 * @param[in]  Q  Covariance matrix of float parameters (n x n)
 * @param[out] F  Fixed solutions (n x m)
 * @param[out] s  Sum of squared residuals of fixed solutions (1 x m)
 * @return Status (0: ok, other: error)
 */
int lambda_search(int n, int m, const double *a, const double *Q, double *F,
                  double *s);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_LAMBDA_H */
