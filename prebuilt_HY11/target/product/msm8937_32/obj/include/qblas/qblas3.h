// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--



#ifndef QBLAS_BLAS3_H
#define QBLAS_BLAS3_H

#include <qblas_types.h>

#ifdef __cplusplus

typedef qblas_single_complex QBLAS_SINGLE_COMPLEX;
typedef qblas_double_complex QBLAS_DOUBLE_COMPLEX;

#else

typedef void QBLAS_SINGLE_COMPLEX;
typedef void QBLAS_DOUBLE_COMPLEX;

#endif

#ifdef __cplusplus
extern "C" {
#endif

/// GEMM
void sgemm(const char *TRANSA, const char *TRANSB, const qblas_long *M, const qblas_long *N, const qblas_long *K, const float *ALPHA, const float *A,
           const qblas_long *LDA, const float *B, const qblas_long *LDB, const float *BETA, float *C, const qblas_long *LDC);

void dgemm(const char *TRANSA, const char *TRANSB, const qblas_long *M, const qblas_long *N, const qblas_long *K, const double *ALPHA, const double *A,
           const qblas_long *LDA, const double *B, const qblas_long *LDB, const double *BETA, double *C, const qblas_long *LDC);

void cgemm(const char *TRANSA, const char *TRANSB, const qblas_long *M, const qblas_long *N, const qblas_long *K, const QBLAS_SINGLE_COMPLEX *ALPHA,
           const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA, const QBLAS_SINGLE_COMPLEX *B, const qblas_long *LDB,
           const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *C, const qblas_long *LDC);

void zgemm(const char *TRANSA, const char *TRANSB, const qblas_long *M, const qblas_long *N, const qblas_long *K, const QBLAS_DOUBLE_COMPLEX *ALPHA,
           const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA, const QBLAS_DOUBLE_COMPLEX *B, const qblas_long *LDB,
           const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *C, const qblas_long *LDC);


/// HEMM
void chemm(const char *SIDE, const char *UPLO, const qblas_long *M, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A,
           const qblas_long *LDA, const QBLAS_SINGLE_COMPLEX *B, const qblas_long *LDB, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *C,
           const qblas_long *LDC);

void zhemm(const char *SIDE, const char *UPLO, const qblas_long *M, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A,
           const qblas_long *LDA, const QBLAS_DOUBLE_COMPLEX *B, const qblas_long *LDB, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *C,
           const qblas_long *LDC);

/// HERK
void cherk(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const float *ALPHA, const QBLAS_SINGLE_COMPLEX *A,
                    const qblas_long *LDA, const float *BETA, QBLAS_SINGLE_COMPLEX *C, const qblas_long *LDC);

void zherk(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const double *ALPHA, const QBLAS_DOUBLE_COMPLEX *A,
                    const qblas_long *LDA, const double *BETA, QBLAS_DOUBLE_COMPLEX *C, const qblas_long *LDC);

/// HER2K
void cher2k(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA,
                     const QBLAS_SINGLE_COMPLEX *B, const qblas_long *LDB, const float *BETA, QBLAS_SINGLE_COMPLEX *C, const qblas_long *LDC);

void zher2k(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA,
                     const QBLAS_DOUBLE_COMPLEX *B, const qblas_long *LDB, const double *BETA, QBLAS_DOUBLE_COMPLEX *C, const qblas_long *LDC);


/// SYMM
void ssymm(const char *SIDE, const char *UPLO, const qblas_long *M, const qblas_long *N, const float *ALPHA, const float *A,
                    const qblas_long *LDA, const float *B, const qblas_long *LDB, const float *BETA, float *C, const qblas_long *LDC);

void dsymm(const char *SIDE, const char *UPLO, const qblas_long *M, const qblas_long *N, const double *ALPHA, const double *A,
                    const qblas_long *LDA, const double *B, const qblas_long *LDB, const double *BETA, double *C, const qblas_long *LDC);

void csymm(const char *SIDE, const char *UPLO, const qblas_long *M, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A,
           const qblas_long *LDA, const QBLAS_SINGLE_COMPLEX *B, const qblas_long *LDB, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *C,
           const qblas_long *LDC);

void zsymm(const char *SIDE, const char *UPLO, const qblas_long *M, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A,
           const qblas_long *LDA, const QBLAS_DOUBLE_COMPLEX *B, const qblas_long *LDB, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *C,
           const qblas_long *LDC);

/// SYRK
void ssyrk(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const float *ALPHA, const float *A,
                    const qblas_long *LDA, const float *BETA, float *C, const qblas_long *LDC);

void dsyrk(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const double *ALPHA, const double *A,
           const qblas_long *LDA, const double *BETA, double *C, const qblas_long *LDC);

void csyrk(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A,
           const qblas_long *LDA, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *C, const qblas_long *LDC);

void zsyrk(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A,
           const qblas_long *LDA, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *C, const qblas_long *LDC);

/// SYR2K
void ssyr2k(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const float *ALPHA, const float *A, const qblas_long *LDA,
                     const float *B, const qblas_long *LDB, const float *BETA, float *C, const qblas_long *LDC);

void dsyr2k(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const double *ALPHA, const double *A, const qblas_long *LDA,
                     const double *B, const qblas_long *LDB, const double *BETA, double *C, const qblas_long *LDC);

void csyr2k(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA,
                     const QBLAS_SINGLE_COMPLEX *B, const qblas_long *LDB, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *C, const qblas_long *LDC);

void zsyr2k(const char *UPLO, const char *TRANS, const qblas_long *N, const qblas_long *K, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA,
                     const QBLAS_DOUBLE_COMPLEX *B, const qblas_long *LDB, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *C, const qblas_long *LDC);


/// TRMM
void strmm(const char *SIDE, const char *UPLO, const char *TRANSA, const char *DIAG, const qblas_long *M, const qblas_long *N,
                    const float *ALPHA, const float *A, const qblas_long *LDA, float *B, const qblas_long *LDB);

void dtrmm(const char *SIDE, const char *UPLO, const char *TRANSA, const char *DIAG, const qblas_long *M, const qblas_long *N,
                    const double *ALPHA, const double *A, const qblas_long *LDA, double *B, const qblas_long *LDB);

void ctrmm(const char *SIDE, const char *UPLO, const char *TRANSA, const char *DIAG, const qblas_long *M, const qblas_long *N,
                    const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA, QBLAS_SINGLE_COMPLEX *B, const qblas_long *LDB);

void ztrmm(const char *SIDE, const char *UPLO, const char *TRANSA, const char *DIAG, const qblas_long *M, const qblas_long *N,
                    const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA, QBLAS_DOUBLE_COMPLEX *B, const qblas_long *LDB);


/// TRSM
void strsm(const char *SIDE, const char *UPLO, const char *TRANSA, const char *DIAG, const qblas_long *M, const qblas_long *N,
                    const float *ALPHA, const float *A, const qblas_long *LDA, float *B, const qblas_long *LDB);

void dtrsm(const char *SIDE, const char *UPLO, const char *TRANSA, const char *DIAG, const qblas_long *M, const qblas_long *N,
                    const double *ALPHA, const double *A, const qblas_long *LDA, double *B, const qblas_long *LDB);

void ctrsm(const char *SIDE, const char *UPLO, const char *TRANSA, const char *DIAG, const qblas_long *M, const qblas_long *N,
                    const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA, QBLAS_SINGLE_COMPLEX *B, const qblas_long *LDB);

void ztrsm(const char *SIDE, const char *UPLO, const char *TRANSA, const char *DIAG, const qblas_long *M, const qblas_long *N,
                    const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA, QBLAS_DOUBLE_COMPLEX *B, const qblas_long *LDB);

#ifdef __cplusplus
}
#endif

#endif // QBLAS_BLAS3_H
