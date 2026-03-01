/*------------------------------------------------------------------------------
 * mrtk_mat.h : matrix and vector type definitions and functions
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
 * @file mrtk_mat.h
 * @brief MRTKLIB Matrix Module — Matrix/vector operations and estimation.
 *
 * This header provides matrix and vector allocation, arithmetic, linear
 * algebra (LU decomposition, matrix inversion, linear solve), and
 * estimation algorithms (least squares, Kalman filter, smoother).
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from rtkcmn.c with zero algorithmic changes.
 */
#ifndef MRTK_MAT_H
#define MRTK_MAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include <stdio.h>

/*============================================================================
 * Fatal Callback
 *===========================================================================*/

/** @brief Fatal callback function type for allocation failures. */
typedef void fatalfunc_t(const char *);

/**
 * @brief Add fatal callback function for mat(), zeros(), imat().
 * @param[in] func  Callback function
 */
void add_fatal(fatalfunc_t *func);

/*============================================================================
 * Matrix Allocation
 *===========================================================================*/

/**
 * @brief Allocate memory for a double matrix.
 * @param[in] n  Number of rows
 * @param[in] m  Number of columns
 * @return Matrix pointer (NULL if n<=0 or m<=0)
 */
double *mat(int n, int m);

/**
 * @brief Allocate memory for an integer matrix.
 * @param[in] n  Number of rows
 * @param[in] m  Number of columns
 * @return Matrix pointer (NULL if n<=0 or m<=0)
 */
int *imat(int n, int m);

/**
 * @brief Allocate a new zero matrix.
 * @param[in] n  Number of rows
 * @param[in] m  Number of columns
 * @return Matrix pointer (NULL if n<=0 or m<=0)
 */
double *zeros(int n, int m);

/**
 * @brief Generate a new identity matrix.
 * @param[in] n  Number of rows and columns
 * @return Matrix pointer (NULL if n<=0)
 */
double *eye(int n);

/*============================================================================
 * Vector Operations
 *===========================================================================*/

/**
 * @brief Inner product of vectors.
 * @param[in] a  Vector a (n x 1)
 * @param[in] b  Vector b (n x 1)
 * @param[in] n  Size of vectors
 * @return a' * b
 */
double dot(const double *a, const double *b, int n);

/**
 * @brief Euclidean norm of vector.
 * @param[in] a  Vector a (n x 1)
 * @param[in] n  Size of vector
 * @return || a ||
 */
double norm(const double *a, int n);

/**
 * @brief Cross product of 3D vectors.
 * @param[in]  a  Vector a (3 x 1)
 * @param[in]  b  Vector b (3 x 1)
 * @param[out] c  Cross product a x b (3 x 1)
 */
void cross3(const double *a, const double *b, double *c);

/**
 * @brief Normalize 3D vector.
 * @param[in]  a  Vector a (3 x 1)
 * @param[out] b  Normalized vector (3 x 1), || b || = 1
 * @return Status (1:ok, 0:error)
 */
int normv3(const double *a, double *b);

/*============================================================================
 * Matrix Operations
 *===========================================================================*/

/**
 * @brief Copy matrix.
 * @param[out] A  Destination matrix (n x m)
 * @param[in]  B  Source matrix (n x m)
 * @param[in]  n  Number of rows
 * @param[in]  m  Number of columns
 */
void matcpy(double *A, const double *B, int n, int m);

/**
 * @brief Multiply matrix by matrix (C = alpha*A*B + beta*C).
 * @param[in]    tr     Transpose flags ("N":normal, "T":transpose)
 * @param[in]    n,k,m  Size of (transposed) matrix A, B
 * @param[in]    alpha  Alpha coefficient
 * @param[in]    A,B    (Transposed) matrix A (n x m), B (m x k)
 * @param[in]    beta   Beta coefficient
 * @param[in,out] C     Matrix C (n x k)
 */
void matmul(const char *tr, int n, int k, int m, double alpha,
            const double *A, const double *B, double beta, double *C);

/**
 * @brief Inverse of matrix (A = A^-1).
 * @param[in,out] A  Matrix (n x n)
 * @param[in]     n  Size of matrix
 * @return Status (0:ok, <0:error)
 */
int matinv(double *A, int n);

/**
 * @brief Solve linear equation (X = A\Y or X = A'\Y).
 * @param[in]  tr   Transpose flag ("N":normal, "T":transpose)
 * @param[in]  A    Input matrix A (n x n)
 * @param[in]  Y    Input matrix Y (n x m)
 * @param[in]  n,m  Size of matrix A, Y
 * @param[out] X    Solution X = A\Y or X = A'\Y (n x m)
 * @return Status (0:ok, <0:error)
 * @note Matrix stored by column-major order (Fortran convention).
 *       X can be same as Y.
 */
int solve(const char *tr, const double *A, const double *Y, int n,
          int m, double *X);

/*============================================================================
 * Estimation Algorithms
 *===========================================================================*/

/**
 * @brief Least square estimation by solving normal equation.
 *
 * Solves x = (A*A')^-1 * A * y.
 *
 * @param[in]  A  Transpose of (weighted) design matrix (n x m)
 * @param[in]  y  (Weighted) measurements (m x 1)
 * @param[in]  n  Number of parameters
 * @param[in]  m  Number of measurements (n <= m)
 * @param[out] x  Estimated parameters (n x 1)
 * @param[out] Q  Estimated parameters covariance matrix (n x n)
 * @return Status (0:ok, <0:error)
 * @note For weighted least square, replace A and y by A*w and w*y (w=W^(1/2)).
 *       Matrix stored by column-major order (Fortran convention).
 */
int lsq(const double *A, const double *y, int n, int m, double *x,
        double *Q);

/**
 * @brief Kalman filter state update.
 *
 * K=P*H*(H'*P*H+R)^-1, xp=x+K*v, Pp=(I-K*H')*P
 *
 * @param[in,out] x  States vector (n x 1)
 * @param[in,out] P  Covariance matrix of states (n x n)
 * @param[in]     H  Transpose of design matrix (n x m)
 * @param[in]     v  Innovation (measurement - model) (m x 1)
 * @param[in]     R  Covariance matrix of measurement error (m x m)
 * @param[in]     n  Number of states
 * @param[in]     m  Number of measurements
 * @return Status (0:ok, <0:error)
 * @note Matrix stored by column-major order (Fortran convention).
 *       If state x[i]==0.0, does not update state x[i]/P[i+i*n].
 */
int filter(double *x, double *P, const double *H, const double *v,
           const double *R, int n, int m);

/**
 * @brief Combine forward and backward filters by fixed-interval smoother.
 *
 * xs=Qs*(Qf^-1*xf+Qb^-1*xb), Qs=(Qf^-1+Qb^-1)^-1
 *
 * @param[in]  xf  Forward solutions (n x 1)
 * @param[in]  Qf  Forward solutions covariance matrix (n x n)
 * @param[in]  xb  Backward solutions (n x 1)
 * @param[in]  Qb  Backward solutions covariance matrix (n x n)
 * @param[in]  n   Number of solutions
 * @param[out] xs  Smoothed solutions (n x 1)
 * @param[out] Qs  Smoothed solutions covariance matrix (n x n)
 * @return Status (0:ok, <0:error)
 * @note Matrix stored by column-major order (Fortran convention).
 */
int smoother(const double *xf, const double *Qf, const double *xb,
             const double *Qb, int n, double *xs, double *Qs);

/*============================================================================
 * Matrix Output
 *===========================================================================*/

/**
 * @brief Print matrix to file pointer.
 * @param[in] A     Matrix A (n x m)
 * @param[in] n,m   Number of rows and columns
 * @param[in] p,q   Total columns, columns under decimal point
 * @param[in] fp    Output file pointer
 * @note Matrix stored by column-major order (Fortran convention).
 */
void matfprint(const double *A, int n, int m, int p, int q, FILE *fp);

/**
 * @brief Print matrix to stdout.
 * @param[in] A    Matrix A (n x m)
 * @param[in] n,m  Number of rows and columns
 * @param[in] p,q  Total columns, columns under decimal point
 */
void matprint(const double *A, int n, int m, int p, int q);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_MAT_H */
