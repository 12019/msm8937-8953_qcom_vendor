// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--




#ifndef QBLAS_CBLAS2_H
#define QBLAS_CBLAS2_H

#include <qblas_cblas_common.h>
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
void cblas_sgbmv(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANS, const qblas_long M, const qblas_long N, const qblas_long KL, const qblas_long KU, const float ALPHA, const float *A, const qblas_long LDA,
                    const float *X, const qblas_long INCX, const float BETA, float *Y, const qblas_long INCY);

void cblas_dgbmv(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANS, const qblas_long M, const qblas_long N, const qblas_long KL, const qblas_long KU, const double ALPHA, const double *A, const qblas_long LDA,
                    const double *X, const qblas_long INCX, const double BETA, double *Y, const qblas_long INCY);

void cblas_cgbmv(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANS, const qblas_long M, const qblas_long N, const qblas_long KL, const qblas_long KU, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA,
                    const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY);

void cblas_zgbmv(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANS, const qblas_long M, const qblas_long N, const qblas_long KL, const qblas_long KU, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA,
                    const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY);

/// GEMV
void cblas_sgemv(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANS, const qblas_long M, const qblas_long N, const float ALPHA, const float *A, const qblas_long LDA,
                    const float *X, const qblas_long INCX, const float BETA, float *Y, const qblas_long INCY);

void cblas_dgemv(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANS, const qblas_long M, const qblas_long N, const double ALPHA, const double *A, const qblas_long LDA,
                    const double *X, const qblas_long INCX, const double BETA, double *Y, const qblas_long INCY);

void cblas_cgemv(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANS, const qblas_long M, const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA,
                    const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY);

void cblas_zgemv(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANS, const qblas_long M, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA,
                    const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY);

/// GER
void cblas_sger(const CBLAS_ORDER ORDER, const qblas_long M, const qblas_long N, const float ALPHA, const float *X, const qblas_long INCX, const float *Y, const qblas_long INCY, float *A, const qblas_long LDA);

void cblas_dger(const CBLAS_ORDER ORDER, const qblas_long M, const qblas_long N, const double ALPHA, const double *X, const qblas_long INCX, const double *Y, const qblas_long INCY, double *A, const qblas_long LDA);

/// GERC
void cblas_cgerc(const CBLAS_ORDER ORDER, const qblas_long M, const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, const QBLAS_SINGLE_COMPLEX *Y,
           const qblas_long INCY, QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA);

void cblas_zgerc(const CBLAS_ORDER ORDER, const qblas_long M, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, const QBLAS_DOUBLE_COMPLEX *Y,
           const qblas_long INCY, QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA);


/// GERU
void cblas_cgeru(const CBLAS_ORDER ORDER, const qblas_long M, const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX,
                    const QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY, QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA);

void cblas_zgeru(const CBLAS_ORDER ORDER, const qblas_long M, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX,
                    const QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY, QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA);

/// HBMV
void cblas_chbmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const qblas_long K, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A,
                    const qblas_long LDA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY);

void cblas_zhbmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const qblas_long K, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A,
                    const qblas_long LDA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY);

/// HEMV
void cblas_chemv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA,
                    const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY);

void cblas_zhemv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA,
                    const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY);

/// HER2
void cblas_cher2(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX,
                    const QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY, QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA);

void cblas_zher2(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX,
                    const QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY, QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA);


/// HER
void cblas_cher(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const float ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA);

void cblas_zher(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const double ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA);


/// HPMV
void cblas_chpmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *AP, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX,
           const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY);

void cblas_zhpmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *AP, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX,
           const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY);

/// HPR2
void cblas_chpr2(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, const QBLAS_SINGLE_COMPLEX *Y,
           const qblas_long INCY, QBLAS_SINGLE_COMPLEX *AP);

void cblas_zhpr2(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, const QBLAS_DOUBLE_COMPLEX *Y,
           const qblas_long INCY, QBLAS_DOUBLE_COMPLEX *AP);

/// HPR
void cblas_chpr(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const float ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, QBLAS_SINGLE_COMPLEX *AP);

void cblas_zhpr(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const double ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, QBLAS_DOUBLE_COMPLEX *AP);

/// SBMV
void cblas_ssbmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const qblas_long K, const float ALPHA, const float *A,
                    const qblas_long LDA, const float *X, const qblas_long INCX, const float BETA, float *Y, const qblas_long INCY);

void cblas_dsbmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const qblas_long K, const double ALPHA, const double *A,
                    const qblas_long LDA, const double *X, const qblas_long INCX, const double BETA, double *Y, const qblas_long INCY);

/// SPMV
void cblas_sspmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const float ALPHA, const float *AP, const float *X, const qblas_long INCX, const float BETA, float *Y, const qblas_long INCY);

void cblas_dspmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const double ALPHA, const double *AP, const double *X, const qblas_long INCX, const double BETA, double *Y, const qblas_long INCY);


/// SPR2
void cblas_sspr2(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const float ALPHA, const float *X, const qblas_long INCX, const float *Y, const qblas_long INCY, float *AP);

void cblas_dspr2(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const double ALPHA, const double *X, const qblas_long INCX, const double *Y, const qblas_long INCY, double *AP);


/// SPR
void cblas_sspr(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const float ALPHA, const float *X, const qblas_long INCX, float *AP);

void cblas_dspr(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const double ALPHA, const double *X, const qblas_long INCX, double *AP);


/// SYMV
void cblas_ssymv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const float ALPHA, const float *A, const qblas_long LDA,
                    const float *X, const qblas_long INCX, const float BETA, float *Y, const qblas_long INCY);

void cblas_dsymv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const double ALPHA, const double *A, const qblas_long LDA,
                    const double *X, const qblas_long INCX, const double BETA, double *Y, const qblas_long INCY);


/// SYR2
void cblas_ssyr2(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const float ALPHA, const float *X, const qblas_long INCX,
                    const float *Y, const qblas_long INCY, float *A, const qblas_long LDA);

void cblas_dsyr2(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const double ALPHA, const double *X, const qblas_long INCX,
                    const double *Y, const qblas_long INCY, double *A, const qblas_long LDA);


/// SYR
void cblas_ssyr(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const float ALPHA, const float *X, const qblas_long INCX, float *A, const qblas_long LDA);

void cblas_dsyr(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const qblas_long N, const double ALPHA, const double *X, const qblas_long INCX, double *A, const qblas_long LDA);


/// TBMV
void cblas_stbmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N,
                    const qblas_long K, const float *A, const qblas_long LDA, float *X, const qblas_long INCX);

void cblas_dtbmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N,
                    const qblas_long K, const double *A, const qblas_long LDA, double *X, const qblas_long INCX);

void cblas_ctbmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N,
                    const qblas_long K, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA, QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

void cblas_ztbmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N,
                    const qblas_long K, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);


/// TBSV
void cblas_stbsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N,
                    const qblas_long K, const float *A, const qblas_long LDA, float *X, const qblas_long INCX);

void cblas_dtbsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N,
                    const qblas_long K, const double *A, const qblas_long LDA, double *X, const qblas_long INCX);

void cblas_ctbsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N,
                    const qblas_long K, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA, QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);


void cblas_ztbsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N,
                    const qblas_long K, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);

/// TPMV
void cblas_stpmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const float *AP, float *X, const qblas_long INCX);

void cblas_dtpmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const double *AP, double *X, const qblas_long INCX);

void cblas_ctpmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const QBLAS_SINGLE_COMPLEX *AP, QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

void cblas_ztpmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *AP, QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);


/// TPSV
void cblas_stpsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const float *AP, float *X, const qblas_long INCX);

void cblas_dtpsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const double *AP, double *X, const qblas_long INCX);

void cblas_ctpsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const QBLAS_SINGLE_COMPLEX *AP, QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

void cblas_ztpsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *AP, QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);

/// TRMV
void cblas_strmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const float *A, const qblas_long LDA, float *X, const qblas_long INCX);

void cblas_dtrmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const double *A, const qblas_long LDA, double *X, const qblas_long INCX);

void cblas_ctrmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA, QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

void cblas_ztrmv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);

/// TRSV
void cblas_strsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const float *A, const qblas_long LDA, float *X, const qblas_long INCX);

void cblas_dtrsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const double *A, const qblas_long LDA, double *X, const qblas_long INCX);

void cblas_ctrsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA, QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

void cblas_ztrsv(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const CBLAS_DIAG DIAG, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);

#ifdef __cplusplus
}
#endif

#endif // QBLAS_CBLAS2_H
