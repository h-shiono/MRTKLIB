/*------------------------------------------------------------------------------
 * mrtk_mat.c : matrix and vector functions
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
 * @file mrtk_mat.c
 * @brief MRTKLIB Matrix Module — Matrix/vector operations and estimation.
 *
 * This file contains all matrix/vector and estimation functions extracted
 * from rtkcmn.c with zero algorithmic changes (pure cut-and-paste).
 */
#include "mrtklib/mrtk_mat.h"

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Static Data
 *===========================================================================*/

static fatalfunc_t *fatalfunc=NULL; /* fatal callback function */

/*============================================================================
 * Internal Helpers
 *===========================================================================*/

/* fatal error ---------------------------------------------------------------*/
static void fatalerr(const char *format, ...)
{
    char msg[1024];
    va_list ap;
    va_start(ap,format); vsprintf(msg,format,ap); va_end(ap);
    if (fatalfunc) {
        fatalfunc(msg);
    } else {
        fprintf(stderr, "%s", msg);
    }
    exit(-9);
}

/*============================================================================
 * Fatal Callback
 *===========================================================================*/

/* add fatal callback function -------------------------------------------------
* add fatal callback function for mat(),zeros(),imat()
* args   : fatalfunc_t *func I  callback function
* return : none
* notes  : if malloc() failed in return : none
*-----------------------------------------------------------------------------*/
extern void add_fatal(fatalfunc_t *func)
{
    fatalfunc=func;
}

/*============================================================================
 * Matrix Allocation
 *===========================================================================*/

/* new matrix ------------------------------------------------------------------
* allocate memory of matrix
* args   : int    n,m       I   number of rows and columns of matrix
* return : matrix pointer (if n<=0 or m<=0, return NULL)
*-----------------------------------------------------------------------------*/
extern double *mat(int n, int m)
{
    double *p;

    if (n <= 0 || m <= 0) {
        return NULL;
    }
    if (!(p=(double *)malloc(sizeof(double)*n*m))) {
        fatalerr("matrix memory allocation error: n=%d,m=%d\n",n,m);
    }
    return p;
}
/* new integer matrix ----------------------------------------------------------
* allocate memory of integer matrix
* args   : int    n,m       I   number of rows and columns of matrix
* return : matrix pointer (if n<=0 or m<=0, return NULL)
*-----------------------------------------------------------------------------*/
extern int *imat(int n, int m)
{
    int *p;

    if (n <= 0 || m <= 0) {
        return NULL;
    }
    if (!(p=(int *)malloc(sizeof(int)*n*m))) {
        fatalerr("integer matrix memory allocation error: n=%d,m=%d\n",n,m);
    }
    return p;
}
/* zero matrix -----------------------------------------------------------------
* generate new zero matrix
* args   : int    n,m       I   number of rows and columns of matrix
* return : matrix pointer (if n<=0 or m<=0, return NULL)
*-----------------------------------------------------------------------------*/
extern double *zeros(int n, int m)
{
    double *p;

#if NOCALLOC
    if ((p=mat(n,m))) for (n=n*m-1;n>=0;n--) p[n]=0.0;
#else
    if (n <= 0 || m <= 0) {
        return NULL;
    }
    if (!(p=(double *)calloc(sizeof(double),n*m))) {
        fatalerr("matrix memory allocation error: n=%d,m=%d\n",n,m);
    }
#endif
    return p;
}
/* identity matrix -------------------------------------------------------------
* generate new identity matrix
* args   : int    n         I   number of rows and columns of matrix
* return : matrix pointer (if n<=0, return NULL)
*-----------------------------------------------------------------------------*/
extern double *eye(int n)
{
    double *p;
    int i;

    if ((p = zeros(n, n))) {
        for (i = 0; i < n; i++) {
            p[i + i * n] = 1.0;
        }
    }
    return p;
}

/*============================================================================
 * Vector Operations
 *===========================================================================*/

/* inner product ---------------------------------------------------------------
* inner product of vectors
* args   : double *a,*b     I   vector a,b (n x 1)
*          int    n         I   size of vector a,b
* return : a'*b
*-----------------------------------------------------------------------------*/
extern double dot(const double *a, const double *b, int n)
{
    double c=0.0;

    while (--n >= 0) {
        c += a[n] * b[n];
    }
    return c;
}
/* euclid norm -----------------------------------------------------------------
* euclid norm of vector
* args   : double *a        I   vector a (n x 1)
*          int    n         I   size of vector a
* return : || a ||
*-----------------------------------------------------------------------------*/
extern double norm(const double *a, int n)
{
    return sqrt(dot(a,a,n));
}
/* outer product of 3d vectors -------------------------------------------------
* outer product of 3d vectors
* args   : double *a,*b     I   vector a,b (3 x 1)
*          double *c        O   outer product (a x b) (3 x 1)
* return : none
*-----------------------------------------------------------------------------*/
extern void cross3(const double *a, const double *b, double *c)
{
    c[0]=a[1]*b[2]-a[2]*b[1];
    c[1]=a[2]*b[0]-a[0]*b[2];
    c[2]=a[0]*b[1]-a[1]*b[0];
}
/* normalize 3d vector ---------------------------------------------------------
* normalize 3d vector
* args   : double *a        I   vector a (3 x 1)
*          double *b        O   normlized vector (3 x 1) || b || = 1
* return : status (1:ok,0:error)
*-----------------------------------------------------------------------------*/
extern int normv3(const double *a, double *b)
{
    double r;
    if ((r = norm(a, 3)) <= 0.0) {
        return 0;
    }
    b[0]=a[0]/r;
    b[1]=a[1]/r;
    b[2]=a[2]/r;
    return 1;
}

/*============================================================================
 * Matrix Operations
 *===========================================================================*/

/* copy matrix -----------------------------------------------------------------
* copy matrix
* args   : double *A        O   destination matrix A (n x m)
*          double *B        I   source matrix B (n x m)
*          int    n,m       I   number of rows and columns of matrix
* return : none
*-----------------------------------------------------------------------------*/
extern void matcpy(double *A, const double *B, int n, int m)
{
    memcpy(A,B,sizeof(double)*n*m);
}
/* matrix routines -----------------------------------------------------------*/

#ifdef LAPACK /* with LAPACK/BLAS or MKL */

/* LAPACK/BLAS Fortran interface declarations */
extern void dgemm_(const char *transa, const char *transb, const int *m,
                   const int *n, const int *k, const double *alpha,
                   const double *A, const int *lda, const double *B,
                   const int *ldb, const double *beta, double *C,
                   const int *ldc);
extern void dgetrf_(const int *m, const int *n, double *A, const int *lda,
                    int *ipiv, int *info);
extern void dgetri_(const int *n, double *A, const int *lda, const int *ipiv,
                    double *work, const int *lwork, int *info);
extern void dgetrs_(const char *trans, const int *n, const int *nrhs,
                    const double *A, const int *lda, const int *ipiv,
                    double *B, const int *ldb, int *info);

/* multiply matrix (wrapper of blas dgemm) -------------------------------------
* multiply matrix by matrix (C=alpha*A*B+beta*C)
* args   : char   *tr       I  transpose flags ("N":normal,"T":transpose)
*          int    n,k,m     I  size of (transposed) matrix A,B
*          double alpha     I  alpha
*          double *A,*B     I  (transposed) matrix A (n x m), B (m x k)
*          double beta      I  beta
*          double *C        IO matrix C (n x k)
* return : none
*-----------------------------------------------------------------------------*/
extern void matmul(const char *tr, int n, int k, int m, double alpha,
                   const double *A, const double *B, double beta, double *C)
{
    int lda,ldb;

    if (n <= 0 || k <= 0 || m <= 0) {
        return;
    }
    lda=tr[0]=='T'?m:n; ldb=tr[1]=='T'?k:m;

    dgemm_((char *)tr,(char *)tr+1,&n,&k,&m,&alpha,(double *)A,&lda,(double *)B,
           &ldb,&beta,C,&n);
}
/* inverse of matrix -----------------------------------------------------------
* inverse of matrix (A=A^-1)
* args   : double *A        IO  matrix (n x n)
*          int    n         I   size of matrix A
* return : status (0:ok,0>:error)
*-----------------------------------------------------------------------------*/
extern int matinv(double *A, int n)
{
    double *work;
    int info,lwork=n*16,*ipiv=imat(n,1);

    work=mat(lwork,1);
    dgetrf_(&n,&n,A,&n,ipiv,&info);
    if (!info) {
        dgetri_(&n, A, &n, ipiv, work, &lwork, &info);
    }
    free(ipiv); free(work);
    return info;
}
/* solve linear equation -------------------------------------------------------
* solve linear equation (X=A\Y or X=A'\Y)
* args   : char   *tr       I   transpose flag ("N":normal,"T":transpose)
*          double *A        I   input matrix A (n x n)
*          double *Y        I   input matrix Y (n x m)
*          int    n,m       I   size of matrix A,Y
*          double *X        O   X=A\Y or X=A'\Y (n x m)
* return : status (0:ok,0>:error)
* notes  : matirix stored by column-major order (fortran convention)
*          X can be same as Y
*-----------------------------------------------------------------------------*/
extern int solve(const char *tr, const double *A, const double *Y, int n,
                 int m, double *X)
{
    double *B=mat(n,n);
    int info,*ipiv=imat(n,1);

    matcpy(B,A,n,n);
    matcpy(X,Y,n,m);
    dgetrf_(&n,&n,B,&n,ipiv,&info);
    if (!info) {
        dgetrs_((char*)tr, &n, &m, B, &n, ipiv, X, &n, &info);
    }
    free(ipiv); free(B);
    return info;
}

#else /* without LAPACK/BLAS or MKL */

/* multiply matrix -----------------------------------------------------------*/
extern void matmul(const char *tr, int n, int k, int m, double alpha,
                   const double *A, const double *B, double beta, double *C)
{
    double d;
    int i, j, x, f;

    if (tr[0] == 'N') {
        f = tr[1] == 'N' ? 1 : 2;
    } else {
        f = tr[1] == 'N' ? 3 : 4;
    }

    for (i=0;i<n;i++) for (j=0;j<k;j++) {
        d=0.0;
        switch (f) {
            case 1: for (x=0;x<m;x++) d+=A[i+x*n]*B[x+j*m]; break;
            case 2: for (x=0;x<m;x++) d+=A[i+x*n]*B[j+x*k]; break;
            case 3: for (x=0;x<m;x++) d+=A[x+i*m]*B[x+j*m]; break;
            case 4: for (x=0;x<m;x++) d+=A[x+i*m]*B[j+x*k]; break;
        }
        if (beta==0.0) C[i+j*n]=alpha*d; else C[i+j*n]=alpha*d+beta*C[i+j*n];
    }
}
/* LU decomposition ----------------------------------------------------------*/
static int ludcmp(double *A, int n, int *indx, double *d)
{
    double big,s,tmp,*vv=mat(n,1);
    int i,imax=0,j,k;

    *d=1.0;
    for (i=0;i<n;i++) {
        big=0.0; for (j=0;j<n;j++) if ((tmp=fabs(A[i+j*n]))>big) big=tmp;
        if (big>0.0) vv[i]=1.0/big; else {free(vv); return -1;}
    }
    for (j=0;j<n;j++) {
        for (i=0;i<j;i++) {
            s=A[i+j*n]; for (k=0;k<i;k++) s-=A[i+k*n]*A[k+j*n]; A[i+j*n]=s;
        }
        big=0.0;
        for (i=j;i<n;i++) {
            s=A[i+j*n]; for (k=0;k<j;k++) s-=A[i+k*n]*A[k+j*n]; A[i+j*n]=s;
            if ((tmp=vv[i]*fabs(s))>=big) {big=tmp; imax=i;}
        }
        if (j!=imax) {
            for (k=0;k<n;k++) {
                tmp=A[imax+k*n]; A[imax+k*n]=A[j+k*n]; A[j+k*n]=tmp;
            }
            *d=-(*d); vv[imax]=vv[j];
        }
        indx[j]=imax;
        if (A[j+j*n]==0.0) {free(vv); return -1;}
        if (j!=n-1) {
            tmp=1.0/A[j+j*n]; for (i=j+1;i<n;i++) A[i+j*n]*=tmp;
        }
    }
    free(vv);
    return 0;
}
/* LU back-substitution ------------------------------------------------------*/
static void lubksb(const double *A, int n, const int *indx, double *b)
{
    double s;
    int i,ii=-1,ip,j;

    for (i=0;i<n;i++) {
        ip=indx[i]; s=b[ip]; b[ip]=b[i];
        if (ii>=0) for (j=ii;j<i;j++) s-=A[i+j*n]*b[j]; else if (s) ii=i;
        b[i]=s;
    }
    for (i=n-1;i>=0;i--) {
        s=b[i]; for (j=i+1;j<n;j++) s-=A[i+j*n]*b[j]; b[i]=s/A[i+i*n];
    }
}
/* inverse of matrix ---------------------------------------------------------*/
extern int matinv(double *A, int n)
{
    double d,*B;
    int i,j,*indx;

    indx=imat(n,1); B=mat(n,n); matcpy(B,A,n,n);
    if (ludcmp(B,n,indx,&d)) {free(indx); free(B); return -1;}
    for (j=0;j<n;j++) {
        for (i=0;i<n;i++) A[i+j*n]=0.0;
        A[j+j*n]=1.0;
        lubksb(B,n,indx,A+j*n);
    }
    free(indx); free(B);
    return 0;
}
/* solve linear equation -----------------------------------------------------*/
extern int solve(const char *tr, const double *A, const double *Y, int n,
                 int m, double *X)
{
    double *B=mat(n,n);
    int info;

    matcpy(B,A,n,n);
    if (!(info=matinv(B,n))) matmul(tr[0]=='N'?"NN":"TN",n,m,n,1.0,B,Y,0.0,X);
    free(B);
    return info;
}
#endif
/* end of matrix routines ----------------------------------------------------*/

/*============================================================================
 * Estimation Algorithms
 *===========================================================================*/

/* least square estimation -----------------------------------------------------
* least square estimation by solving normal equation (x=(A*A')^-1*A*y)
* args   : double *A        I   transpose of (weighted) design matrix (n x m)
*          double *y        I   (weighted) measurements (m x 1)
*          int    n,m       I   number of parameters and measurements (n<=m)
*          double *x        O   estmated parameters (n x 1)
*          double *Q        O   esimated parameters covariance matrix (n x n)
* return : status (0:ok,0>:error)
* notes  : for weighted least square, replace A and y by A*w and w*y (w=W^(1/2))
*          matirix stored by column-major order (fortran convention)
*-----------------------------------------------------------------------------*/
extern int lsq(const double *A, const double *y, int n, int m, double *x,
               double *Q)
{
    double *Ay;
    int info;

    if (m < n) {
        return -1;
    }
    Ay=mat(n,1);
    matmul("NN",n,1,m,1.0,A,y,0.0,Ay); /* Ay=A*y */
    matmul("NT",n,n,m,1.0,A,A,0.0,Q);  /* Q=A*A' */
    if (!(info = matinv(Q, n))) {
        matmul("NN", n, 1, n, 1.0, Q, Ay, 0.0, x); /* x=Q^-1*Ay */
    }
    free(Ay);
    return info;
}
/* kalman filter ---------------------------------------------------------------
* kalman filter state update as follows:
*
*   K=P*H*(H'*P*H+R)^-1, xp=x+K*v, Pp=(I-K*H')*P
*
* args   : double *x        I   states vector (n x 1)
*          double *P        I   covariance matrix of states (n x n)
*          double *H        I   transpose of design matrix (n x m)
*          double *v        I   innovation (measurement - model) (m x 1)
*          double *R        I   covariance matrix of measurement error (m x m)
*          int    n,m       I   number of states and measurements
*          double *xp       O   states vector after update (n x 1)
*          double *Pp       O   covariance matrix of states after update (n x n)
* return : status (0:ok,<0:error)
* notes  : matirix stored by column-major order (fortran convention)
*          if state x[i]==0.0, not updates state x[i]/P[i+i*n]
*-----------------------------------------------------------------------------*/
static int filter_(const double *x, const double *P, const double *H,
                   const double *v, const double *R, int n, int m,
                   double *xp, double *Pp)
{
    double *F=mat(n,m),*Q=mat(m,m),*K=mat(n,m),*I=eye(n);
    int info;

    matcpy(Q,R,m,m);
    matcpy(xp,x,n,1);
    matmul("NN",n,m,n,1.0,P,H,0.0,F);       /* Q=H'*P*H+R */
    matmul("TN",m,m,n,1.0,H,F,1.0,Q);
    if (!(info=matinv(Q,m))) {
        matmul("NN",n,m,m,1.0,F,Q,0.0,K);   /* K=P*H*Q^-1 */
        matmul("NN",n,1,m,1.0,K,v,1.0,xp);  /* xp=x+K*v */
        matmul("NT",n,n,m,-1.0,K,H,1.0,I);  /* Pp=(I-K*H')*P */
        matmul("NN",n,n,n,1.0,I,P,0.0,Pp);
    }
    free(F); free(Q); free(K); free(I);
    return info;
}
extern int filter(double *x, double *P, const double *H, const double *v,
                  const double *R, int n, int m)
{
    double *x_,*xp_,*P_,*Pp_,*H_;
    int i,j,k,info,*ix;

    ix = imat(n, 1);
    for (i = k = 0; i < n; i++) {
        if (x[i] != 0.0 && P[i + i * n] > 0.0) {
            ix[k++] = i;
        }
    }
    x_=mat(k,1); xp_=mat(k,1); P_=mat(k,k); Pp_=mat(k,k); H_=mat(k,m);
    for (i=0;i<k;i++) {
        x_[i]=x[ix[i]];
        for (j = 0; j < k; j++) {
            P_[i + j * k] = P[ix[i] + ix[j] * n];
        }
        for (j = 0; j < m; j++) {
            H_[i + j * k] = H[ix[i] + j * n];
        }
    }
    info=filter_(x_,P_,H_,v,R,k,m,xp_,Pp_);
    for (i=0;i<k;i++) {
        x[ix[i]]=xp_[i];
        for (j = 0; j < k; j++) {
            P[ix[i] + ix[j] * n] = Pp_[i + j * k];
        }
    }
    free(ix); free(x_); free(xp_); free(P_); free(Pp_); free(H_);
    return info;
}
/* smoother --------------------------------------------------------------------
* combine forward and backward filters by fixed-interval smoother as follows:
*
*   xs=Qs*(Qf^-1*xf+Qb^-1*xb), Qs=(Qf^-1+Qb^-1)^-1)
*
* args   : double *xf       I   forward solutions (n x 1)
* args   : double *Qf       I   forward solutions covariance matrix (n x n)
*          double *xb       I   backward solutions (n x 1)
*          double *Qb       I   backward solutions covariance matrix (n x n)
*          int    n         I   number of solutions
*          double *xs       O   smoothed solutions (n x 1)
*          double *Qs       O   smoothed solutions covariance matrix (n x n)
* return : status (0:ok,0>:error)
* notes  : see reference [4] 5.2
*          matirix stored by column-major order (fortran convention)
*-----------------------------------------------------------------------------*/
extern int smoother(const double *xf, const double *Qf, const double *xb,
                    const double *Qb, int n, double *xs, double *Qs)
{
    double *invQf=mat(n,n),*invQb=mat(n,n),*xx=mat(n,1);
    int i,info=-1;

    matcpy(invQf,Qf,n,n);
    matcpy(invQb,Qb,n,n);
    if (!matinv(invQf,n)&&!matinv(invQb,n)) {
        for (i = 0; i < n * n; i++) {
            Qs[i] = invQf[i] + invQb[i];
        }
        if (!(info=matinv(Qs,n))) {
            matmul("NN",n,1,n,1.0,invQf,xf,0.0,xx);
            matmul("NN",n,1,n,1.0,invQb,xb,1.0,xx);
            matmul("NN",n,1,n,1.0,Qs,xx,0.0,xs);
        }
    }
    free(invQf); free(invQb); free(xx);
    return info;
}

/*============================================================================
 * Matrix Output
 *===========================================================================*/

/* print matrix ----------------------------------------------------------------
* print matrix to stdout
* args   : double *A        I   matrix A (n x m)
*          int    n,m       I   number of rows and columns of A
*          int    p,q       I   total columns, columns under decimal point
*         (FILE  *fp        I   output file pointer)
* return : none
* notes  : matirix stored by column-major order (fortran convention)
*-----------------------------------------------------------------------------*/
extern void matfprint(const double A[], int n, int m, int p, int q, FILE *fp)
{
    int i,j;

    for (i=0;i<n;i++) {
        for (j = 0; j < m; j++) {
            fprintf(fp, " %*.*f", p, q, A[i + j * n]);
        }
        fprintf(fp,"\n");
    }
}
extern void matprint(const double A[], int n, int m, int p, int q)
{
    matfprint(A,n,m,p,q,stdout);
}
