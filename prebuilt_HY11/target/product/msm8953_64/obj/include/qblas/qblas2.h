// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--



#ifndef QBLAS_BLAS2_H
#define QBLAS_BLAS2_H

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

/// GBMV
void sgbmv(const char *TRANS, const qblas_long *M, const qblas_long *N, const qblas_long *KL, const qblas_long *KU, const float *ALPHA, const float *A, const qblas_long *LDA,
                    const float *X, const qblas_long *INCX, const float *BETA, float *Y, const qblas_long *INCY);

void dgbmv(const char *TRANS, const qblas_long *M, const qblas_long *N, const qblas_long *KL, const qblas_long *KU, const double *ALPHA, const double *A, const qblas_long *LDA,
                    const double *X, const qblas_long *INCX, const double *BETA, double *Y, const qblas_long *INCY);

void cgbmv(const char *TRANS, const qblas_long *M, const qblas_long *N, const qblas_long *KL, const qblas_long *KU, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA,
                    const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zgbmv(const char *TRANS, const qblas_long *M, const qblas_long *N, const qblas_long *KL, const qblas_long *KU, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA,
                    const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);

/// GEMV
void sgemv(const char *TRANS, const qblas_long *M, const qblas_long *N, const float *ALPHA, const float *A, const qblas_long *LDA,
                    const float *X, const qblas_long *INCX, const float *BETA, float *Y, const qblas_long *INCY);

void dgemv(const char *TRANS, const qblas_long *M, const qblas_long *N, const double *ALPHA, const double *A, const qblas_long *LDA,
                    const double *X, const qblas_long *INCX, const double *BETA, double *Y, const qblas_long *INCY);

void cgemv(const char *TRANS, const qblas_long *M, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA,
                    const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zgemv(const char *TRANS, const qblas_long *M, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA,
                    const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);

/// GER
void sger(const qblas_long *M, const qblas_long *N, const float *ALPHA, const float *X, const qblas_long *INCX, const float *Y, const qblas_long *INCY, float *A, const qblas_long *LDA);

void dger(const qblas_long *M, const qblas_long *N, const double *ALPHA, const double *X, const qblas_long *INCX, const double *Y, const qblas_long *INCY, double *A, const qblas_long *LDA);

/// GERC
void cgerc(const qblas_long *M, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_SINGLE_COMPLEX *Y,
           const qblas_long *INCY, QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA);

void zgerc(const qblas_long *M, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_DOUBLE_COMPLEX *Y,
           const qblas_long *INCY, QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA);


/// GERU
void cgeru(const qblas_long *M, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX,
                    const QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY, QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA);

void zgeru(const qblas_long *M, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX,
                    const QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY, QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA);

/// HBMV
void chbmv(const char *UPLO, const qblas_long *N, const qblas_long *K, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A,
                    const qblas_long *LDA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zhbmv(const char *UPLO, const qblas_long *N, const qblas_long *K, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A,
                    const qblas_long *LDA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);

/// HEMV
void chemv(const char *UPLO, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA,
                    const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zhemv(const char *UPLO, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA,
                    const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);

/// HER2
void cher2(const char *UPLO, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX,
                    const QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY, QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA);

void zher2(const char *UPLO, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX,
                    const QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY, QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA);


/// HER
void cher(const char *UPLO, const qblas_long *N, const float *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA);

void zher(const char *UPLO, const qblas_long *N, const double *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA);


/// HPMV
void chpmv(const char *UPLO, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *AP, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX,
           const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zhpmv(const char *UPLO, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *AP, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX,
           const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);

/// HPR2
void chpr2(const char *UPLO, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_SINGLE_COMPLEX *Y,
           const qblas_long *INCY, QBLAS_SINGLE_COMPLEX *AP);

void zhpr2(const char *UPLO, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_DOUBLE_COMPLEX *Y,
           const qblas_long *INCY, QBLAS_DOUBLE_COMPLEX *AP);

/// HPR
void chpr(const char *UPLO, const qblas_long *N, const float *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, QBLAS_SINGLE_COMPLEX *AP);

void zhpr(const char *UPLO, const qblas_long *N, const double *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, QBLAS_DOUBLE_COMPLEX *AP);

/// SBMV
void ssbmv(const char *UPLO, const qblas_long *N, const qblas_long *K, const float *ALPHA, const float *A,
                    const qblas_long *LDA, const float *X, const qblas_long *INCX, const float *BETA, float *Y, const qblas_long *INCY);

void dsbmv(const char *UPLO, const qblas_long *N, const qblas_long *K, const double *ALPHA, const double *A,
                    const qblas_long *LDA, const double *X, const qblas_long *INCX, const double *BETA, double *Y, const qblas_long *INCY);

/// SPMV
void sspmv(const char *UPLO, const qblas_long *N, const float *ALPHA, const float *AP, const float *X, const qblas_long *INCX, const float *BETA, float *Y, const qblas_long *INCY);

void dspmv(const char *UPLO, const qblas_long *N, const double *ALPHA, const double *AP, const double *X, const qblas_long *INCX, const double *BETA, double *Y, const qblas_long *INCY);


/// SPR2
void sspr2(const char *UPLO, const qblas_long *N, const float *ALPHA, const float *X, const qblas_long *INCX, const float *Y, const qblas_long *INCY, float *AP);

void dspr2(const char *UPLO, const qblas_long *N, const double *ALPHA, const double *X, const qblas_long *INCX, const double *Y, const qblas_long *INCY, double *AP);


/// SPR
void sspr(const char *UPLO, const qblas_long *N, const float *ALPHA, const float *X, const qblas_long *INCX, float *AP);

void dspr(const char *UPLO, const qblas_long *N, const double *ALPHA, const double *X, const qblas_long *INCX, double *AP);


/// SYMV
void ssymv(const char *UPLO, const qblas_long *N, const float *ALPHA, const float *A, const qblas_long *LDA,
                    const float *X, const qblas_long *INCX, const float *BETA, float *Y, const qblas_long *INCY);

void dsymv(const char *UPLO, const qblas_long *N, const double *ALPHA, const double *A, const qblas_long *LDA,
                    const double *X, const qblas_long *INCX, const double *BETA, double *Y, const qblas_long *INCY);


/// SYR2
void ssyr2(const char *UPLO, const qblas_long *N, const float *ALPHA, const float *X, const qblas_long *INCX,
                    const float *Y, const qblas_long *INCY, float *A, const qblas_long *LDA);

void dsyr2(const char *UPLO, const qblas_long *N, const double *ALPHA, const double *X, const qblas_long *INCX,
                    const double *Y, const qblas_long *INCY, double *A, const qblas_long *LDA);


/// SYR
void ssyr(const char *UPLO, const qblas_long *N, const float *ALPHA, const float *X, const qblas_long *INCX, float *A, const qblas_long *LDA);

void dsyr(const char *UPLO, const qblas_long *N, const double *ALPHA, const double *X, const qblas_long *INCX, double *A, const qblas_long *LDA);


/// TBMV
void stbmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N,
                    const qblas_long *K, const float *A, const qblas_long *LDA, float *X, const qblas_long *INCX);

void dtbmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N,
                    const qblas_long *K, const double *A, const qblas_long *LDA, double *X, const qblas_long *INCX);

void ctbmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N,
                    const qblas_long *K, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA, QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

void ztbmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N,
                    const qblas_long *K, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);


/// TBSV
void stbsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N,
                    const qblas_long *K, const float *A, const qblas_long *LDA, float *X, const qblas_long *INCX);

void dtbsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N,
                    const qblas_long *K, const double *A, const qblas_long *LDA, double *X, const qblas_long *INCX);

void ctbsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N,
                    const qblas_long *K, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA, QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);


void ztbsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N,
                    const qblas_long *K, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);

/// TPMV
void stpmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const float *AP, float *X, const qblas_long *INCX);

void dtpmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const double *AP, double *X, const qblas_long *INCX);

void ctpmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *AP, QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

void ztpmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *AP, QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);


/// TPSV
void stpsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const float *AP, float *X, const qblas_long *INCX);

void dtpsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const double *AP, double *X, const qblas_long *INCX);

void ctpsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *AP, QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

void ztpsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *AP, QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);

/// TRMV
void strmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const float *A, const qblas_long *LDA, float *X, const qblas_long *INCX);

void dtrmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const double *A, const qblas_long *LDA, double *X, const qblas_long *INCX);

void ctrmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA, QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

void ztrmv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);

/// TRSV
void strsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const float *A, const qblas_long *LDA, float *X, const qblas_long *INCX);

void dtrsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const double *A, const qblas_long *LDA, double *X, const qblas_long *INCX);

void ctrsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *A, const qblas_long *LDA, QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

void ztrsv(const char *UPLO, const char *TRANS, const char *DIAG, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long *LDA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);

#ifdef __cplusplus
}
#endif

#endif // QBLAS_BLAS2_H
