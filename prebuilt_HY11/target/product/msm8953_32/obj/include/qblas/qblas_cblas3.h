// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--




#ifndef QBLAS_CBLAS3_H
#define QBLAS_CBLAS3_H


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

/// GEMM
void cblas_sgemm(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANSA, const CBLAS_TRANSPOSE TRANSB, const qblas_long M, const qblas_long N, const qblas_long K, const float ALPHA, const float *A,
           const qblas_long LDA, const float *B, const qblas_long LDB, const float BETA, float *C, const qblas_long LDC);

void cblas_dgemm(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANSA, const CBLAS_TRANSPOSE TRANSB, const qblas_long M, const qblas_long N, const qblas_long K, const double ALPHA, const double *A,
           const qblas_long LDA, const double *B, const qblas_long LDB, const double BETA, double *C, const qblas_long LDC);

void cblas_cgemm(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANSA, const CBLAS_TRANSPOSE TRANSB, const qblas_long M, const qblas_long N, const qblas_long K, const QBLAS_SINGLE_COMPLEX *ALPHA,
           const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA, const QBLAS_SINGLE_COMPLEX *B, const qblas_long LDB,
           const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *C, const qblas_long LDC);

void cblas_zgemm(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANSA, const CBLAS_TRANSPOSE TRANSB, const qblas_long M, const qblas_long N, const qblas_long K, const QBLAS_DOUBLE_COMPLEX *ALPHA,
           const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA, const QBLAS_DOUBLE_COMPLEX *B, const qblas_long LDB,
           const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *C, const qblas_long LDC);


/// HEMM
void cblas_chemm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const qblas_long M, const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A,
           const qblas_long LDA, const QBLAS_SINGLE_COMPLEX *B, const qblas_long LDB, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *C,
           const qblas_long LDC);

void cblas_zhemm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const qblas_long M, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A,
           const qblas_long LDA, const QBLAS_DOUBLE_COMPLEX *B, const qblas_long LDB, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *C,
           const qblas_long LDC);

/// HERK
void cblas_cherk(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const float ALPHA, const QBLAS_SINGLE_COMPLEX *A,
                    const qblas_long LDA, const float BETA, QBLAS_SINGLE_COMPLEX *C, const qblas_long LDC);

void cblas_zherk(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const double ALPHA, const QBLAS_DOUBLE_COMPLEX *A,
                    const qblas_long LDA, const double BETA, QBLAS_DOUBLE_COMPLEX *C, const qblas_long LDC);

/// HER2K
void cblas_cher2k(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA,
                     const QBLAS_SINGLE_COMPLEX *B, const qblas_long LDB, const float BETA, QBLAS_SINGLE_COMPLEX *C, const qblas_long LDC);

void cblas_zher2k(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA,
                     const QBLAS_DOUBLE_COMPLEX *B, const qblas_long LDB, const double BETA, QBLAS_DOUBLE_COMPLEX *C, const qblas_long LDC);


/// SYMM
void cblas_ssymm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const qblas_long M, const qblas_long N, const float ALPHA, const float *A,
                    const qblas_long LDA, const float *B, const qblas_long LDB, const float BETA, float *C, const qblas_long LDC);

void cblas_dsymm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const qblas_long M, const qblas_long N, const double ALPHA, const double *A,
                    const qblas_long LDA, const double *B, const qblas_long LDB, const double BETA, double *C, const qblas_long LDC);

void cblas_csymm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const qblas_long M, const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A,
           const qblas_long LDA, const QBLAS_SINGLE_COMPLEX *B, const qblas_long LDB, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *C,
           const qblas_long LDC);

void cblas_zsymm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const qblas_long M, const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A,
           const qblas_long LDA, const QBLAS_DOUBLE_COMPLEX *B, const qblas_long LDB, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *C,
           const qblas_long LDC);

/// SYRK
void cblas_ssyrk(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const float ALPHA, const float *A,
                    const qblas_long LDA, const float BETA, float *C, const qblas_long LDC);

void cblas_dsyrk(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const double ALPHA, const double *A,
           const qblas_long LDA, const double BETA, double *C, const qblas_long LDC);

void cblas_csyrk(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A,
           const qblas_long LDA, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *C, const qblas_long LDC);

void cblas_zsyrk(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A,
           const qblas_long LDA, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *C, const qblas_long LDC);

/// SYR2K
void cblas_ssyr2k(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const float ALPHA, const float *A, const qblas_long LDA,
                     const float *B, const qblas_long LDB, const float BETA, float *C, const qblas_long LDC);

void cblas_dsyr2k(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const double ALPHA, const double *A, const qblas_long LDA,
                     const double *B, const qblas_long LDB, const double BETA, double *C, const qblas_long LDC);

void cblas_csyr2k(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA,
                     const QBLAS_SINGLE_COMPLEX *B, const qblas_long LDB, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *C, const qblas_long LDC);

void cblas_zsyr2k(const CBLAS_ORDER ORDER, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANS, const qblas_long N, const qblas_long K, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA,
                     const QBLAS_DOUBLE_COMPLEX *B, const qblas_long LDB, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *C, const qblas_long LDC);


/// TRMM
void cblas_strmm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANSA, const CBLAS_DIAG DIAG, const qblas_long M, const qblas_long N,
                    const float ALPHA, const float *A, const qblas_long LDA, float *B, const qblas_long LDB);

void cblas_dtrmm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANSA, const CBLAS_DIAG DIAG, const qblas_long M, const qblas_long N,
                    const double ALPHA, const double *A, const qblas_long LDA, double *B, const qblas_long LDB);

void cblas_ctrmm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANSA, const CBLAS_DIAG DIAG, const qblas_long M, const qblas_long N,
                    const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA, QBLAS_SINGLE_COMPLEX *B, const qblas_long LDB);

void cblas_ztrmm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANSA, const CBLAS_DIAG DIAG, const qblas_long M, const qblas_long N,
                    const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA, QBLAS_DOUBLE_COMPLEX *B, const qblas_long LDB);


/// TRSM
void cblas_strsm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANSA, const CBLAS_DIAG DIAG, const qblas_long M, const qblas_long N,
                    const float ALPHA, const float *A, const qblas_long LDA, float *B, const qblas_long LDB);

void cblas_dtrsm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANSA, const CBLAS_DIAG DIAG, const qblas_long M, const qblas_long N,
                    const double ALPHA, const double *A, const qblas_long LDA, double *B, const qblas_long LDB);

void cblas_ctrsm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANSA, const CBLAS_DIAG DIAG, const qblas_long M, const qblas_long N,
                    const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *A, const qblas_long LDA, QBLAS_SINGLE_COMPLEX *B, const qblas_long LDB);

void cblas_ztrsm(const CBLAS_ORDER ORDER, const CBLAS_SIDE SIDE, const CBLAS_UPLO UPLO, const CBLAS_TRANSPOSE TRANSA, const CBLAS_DIAG DIAG, const qblas_long M, const qblas_long N,
                    const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *A, const qblas_long LDA, QBLAS_DOUBLE_COMPLEX *B, const qblas_long LDB);

#ifdef __cplusplus
}
#endif

#endif // QBLAS_CBLAS3_H
