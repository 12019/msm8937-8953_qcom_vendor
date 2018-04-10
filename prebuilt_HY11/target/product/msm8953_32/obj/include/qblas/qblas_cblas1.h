// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--



#ifndef QBLAS_CBLAS1_H
#define QBLAS_CBLAS1_H

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

/// AMAX
qblas_long cblas_isamax(const qblas_long N, const float *X, const qblas_long INCX);

qblas_long cblas_idamax(const qblas_long N, const double *X, const qblas_long INCX);

qblas_long cblas_icamax(const qblas_long N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

qblas_long cblas_izamax(const qblas_long N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);


/// AMIN
qblas_long cblas_isamin(const qblas_long N, const float *X, const qblas_long INCX);

qblas_long cblas_idamin(const qblas_long N, const double *X, const qblas_long INCX);

qblas_long cblas_icamin(const qblas_long N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

qblas_long cblas_izamin(const qblas_long N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);



/// ASUM
float cblas_sasum(const qblas_long N, const float *X, const qblas_long INCX);

double cblas_dasum(const qblas_long N, const double *X, const qblas_long INCX);

float cblas_scasum(const qblas_long N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

double cblas_dzasum(const qblas_long N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);


/// AXPY
void cblas_saxpy(const qblas_long N, const float ALPHA, const float *X, const qblas_long INCX, float *Y, const qblas_long INCY);

void cblas_daxpy(const qblas_long N, const double ALPHA, const double *X, const qblas_long INCX, double *Y, const qblas_long INCY);

void cblas_caxpy(const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY);

void cblas_zaxpy(const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY);


/// AXPBY
void cblas_saxpby(const qblas_long N, const float ALPHA, const float *X, const qblas_long INCX, const float BETA, float *Y, const qblas_long INCY);

void cblas_daxpby(const qblas_long N, const double ALPHA, const double *X, const qblas_long INCX, const double BETA, double *Y, const qblas_long INCY);

void cblas_caxpby(const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY);

void cblas_zaxpby(const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY);


/// CABS1
float cblas_scabs1(const QBLAS_SINGLE_COMPLEX *Z);

double cblas_dcabs1(const QBLAS_DOUBLE_COMPLEX *Z);



/// COPY
void cblas_scopy(const qblas_long N, const float *X, const qblas_long INCX, float *Y, const qblas_long INCY);

void cblas_dcopy(const qblas_long N, const double *X, const qblas_long INCX, double *Y, const qblas_long INCY);

void cblas_ccopy(const qblas_long N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY);

void cblas_zcopy(const qblas_long N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY);


/// DOT
float cblas_sdot(const qblas_long N, const float *X, const qblas_long INCX, const float *Y, const qblas_long INCY);

double cblas_ddot(const qblas_long N, const double *X, const qblas_long INCX, const double *Y, const qblas_long INCY);


/// DOTC
void cblas_cdotc_sub(const qblas_long N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, const QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY, QBLAS_SINGLE_COMPLEX *result);

void cblas_zdotc_sub(const qblas_long N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, const QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY, QBLAS_DOUBLE_COMPLEX *result);


/// DOTU
void cblas_cdotu_sub(const qblas_long N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, const QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY, QBLAS_SINGLE_COMPLEX *result);

void cblas_zdotu_sub(const qblas_long N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, const QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY, QBLAS_DOUBLE_COMPLEX *result);


/// NRM2
float cblas_snrm2(const qblas_long N, const float *X, const qblas_long INCX);

double cblas_dnrm2(const qblas_long N, const double *X, const qblas_long INCX);

float cblas_scnrm2(const qblas_long N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

double cblas_dznrm2(const qblas_long N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);


/// ROT
void cblas_srot(const qblas_long N, float *X, const qblas_long INCX, float *Y, const qblas_long INCY, const float C, const float S);

void cblas_drot(const qblas_long N, double *X, const qblas_long INCX, double *Y, const qblas_long INCY, const double C, const double S);

void cblas_csrot(const qblas_long N, QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY, const float C, const float S);

void cblas_zdrot(const qblas_long N, QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY, const double C, const double S);


/// ROTG
void cblas_srotg(float *A, float *B, float *C, float *S);

void cblas_drotg(double *A, double *B, double *C, double *S);

void cblas_crotg(QBLAS_SINGLE_COMPLEX *A, const QBLAS_SINGLE_COMPLEX *B, float *C, QBLAS_SINGLE_COMPLEX *S);

void cblas_zrotg(QBLAS_DOUBLE_COMPLEX *A, const QBLAS_DOUBLE_COMPLEX *B, double *C, QBLAS_DOUBLE_COMPLEX *S);

/// ROTM
void cblas_srotm(const qblas_long N, float *X, const qblas_long INCX, float *Y, const qblas_long INCY, const float *PARAM);

void cblas_drotm(const qblas_long N, double *X, const qblas_long INCX, double *Y, const qblas_long INCY, const double *PARAM);

/// ROTMG
void cblas_srotmg(float *D1, float *D2, float *X1, const float Y1, float *PARAM);

void cblas_drotmg(double *D1, double *D2, double *X1, const double Y1, double *PARAM);

/// SCAL

void cblas_sscal(const qblas_long N, const float ALPHA, float *X, const qblas_long INCX);

void cblas_dscal(const qblas_long N, const double ALPHA, double *X, const qblas_long INCX);

void cblas_cscal(const qblas_long N, const QBLAS_SINGLE_COMPLEX *ALPHA, QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

void cblas_zscal(const qblas_long N, const QBLAS_DOUBLE_COMPLEX *ALPHA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);

void cblas_csscal(const qblas_long N, const float ALPHA, QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX);

void cblas_zdscal(const qblas_long N, const double ALPHA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX);


/// SDOT
float cblas_sdsdot(const qblas_long N, const float B, const float *X, const qblas_long INCX, const float *Y, const qblas_long INCY);

double cblas_dsdot(const qblas_long N, const float *X, const qblas_long INCX, const float *Y, const qblas_long INCY);



/// SWAP
void cblas_sswap(const qblas_long N, float *X, const qblas_long INCX, float *Y, const qblas_long INCY);

void cblas_dswap(const qblas_long N, double *X, const qblas_long INCX, double *Y, const qblas_long INCY);

void cblas_cswap(const qblas_long N, QBLAS_SINGLE_COMPLEX *X, const qblas_long INCX, QBLAS_SINGLE_COMPLEX *Y, const qblas_long INCY);

void cblas_zswap(const qblas_long N, QBLAS_DOUBLE_COMPLEX *X, const qblas_long INCX, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long INCY);



#ifdef __cplusplus
}
#endif


#endif // QBLAS_CBLAS1_H
