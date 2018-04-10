// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--



#ifndef QBLAS_BLAS1_H
#define QBLAS_BLAS1_H

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
qblas_long isamax(const qblas_long *N, const float *X, const qblas_long *INCX);

qblas_long idamax(const qblas_long *N, const double *X, const qblas_long *INCX);

qblas_long icamax(const qblas_long *N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

qblas_long izamax(const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);


/// AMIN
qblas_long isamin(const qblas_long *N, const float *X, const qblas_long *INCX);

qblas_long idamin(const qblas_long *N, const double *X, const qblas_long *INCX);

qblas_long icamin(const qblas_long *N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

qblas_long izamin(const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);



/// ASUM
float sasum(const qblas_long *N, const float *X, const qblas_long *INCX);

double dasum(const qblas_long *N, const double *X, const qblas_long *INCX);

float scasum(const qblas_long *N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

double dzasum(const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);


/// AXPY
void saxpy(const qblas_long *N, const float *ALPHA, const float *X, const qblas_long *INCX, float *Y, const qblas_long *INCY);

void daxpy(const qblas_long *N, const double *ALPHA, const double *X, const qblas_long *INCX, double *Y, const qblas_long *INCY);

void caxpy(const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zaxpy(const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);


/// AXPBY
void saxpby(const qblas_long *N, const float *ALPHA, const float *X, const qblas_long *INCX, const float *BETA, float *Y, const qblas_long *INCY);

void daxpby(const qblas_long *N, const double *ALPHA, const double *X, const qblas_long *INCX, const double *BETA, double *Y, const qblas_long *INCY);

void caxpby(const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_SINGLE_COMPLEX *BETA, QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zaxpby(const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_DOUBLE_COMPLEX *BETA, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);


/// CABS1
float scabs1(const QBLAS_SINGLE_COMPLEX *Z);

double dcabs1(const QBLAS_DOUBLE_COMPLEX *Z);



/// COPY
void scopy(const qblas_long *N, const float *X, const qblas_long *INCX, float *Y, const qblas_long *INCY);

void dcopy(const qblas_long *N, const double *X, const qblas_long *INCX, double *Y, const qblas_long *INCY);

void ccopy(const qblas_long *N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zcopy(const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);


/// DOT
float sdot(const qblas_long *N, const float *X, const qblas_long *INCX, const float *Y, const qblas_long *INCY);

double ddot(const qblas_long *N, const double *X, const qblas_long *INCX, const double *Y, const qblas_long *INCY);


/// DOTC
void cdotc(QBLAS_SINGLE_COMPLEX *result, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zdotc(QBLAS_DOUBLE_COMPLEX *result, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);


/// DOTU
void cdotu(QBLAS_SINGLE_COMPLEX *result, const qblas_long *N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zdotu(QBLAS_DOUBLE_COMPLEX *result, const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, const QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);


/// NRM2
float snrm2(const qblas_long *N, const float *X, const qblas_long *INCX);

double dnrm2(const qblas_long *N, const double *X, const qblas_long *INCX);

float scnrm2(const qblas_long *N, const QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

double dznrm2(const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);


/// ROT
void srot(const qblas_long *N, float *X, const qblas_long *INCX, float *Y, const qblas_long *INCY, const float *C, const float *S);

void drot(const qblas_long *N, double *X, const qblas_long *INCX, double *Y, const qblas_long *INCY, const double *C, const double *S);

void csrot(const qblas_long *N, QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY, const float *C, const float *S);

void zdrot(const qblas_long *N, QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY, const double *C, const double *S);


/// ROTG
void srotg(float *A, float *B, float *C, float *S);

void drotg(double *A, double *B, double *C, double *S);

void crotg(QBLAS_SINGLE_COMPLEX *A, const QBLAS_SINGLE_COMPLEX *B, float *C, QBLAS_SINGLE_COMPLEX *S);

void zrotg(QBLAS_DOUBLE_COMPLEX *A, const QBLAS_DOUBLE_COMPLEX *B, double *C, QBLAS_DOUBLE_COMPLEX *S);

/// ROTM
void srotm(const qblas_long *N, float *X, const qblas_long *INCX, float *Y, const qblas_long *INCY, const float *PARAM);

void drotm(const qblas_long *N, double *X, const qblas_long *INCX, double *Y, const qblas_long *INCY, const double *PARAM);

/// ROTMG
void srotmg(float *D1, float *D2, float *X1, const float *Y1, float *PARAM);

void drotmg(double *D1, double *D2, double *X1, const double *Y1, double *PARAM);

/// SCAL

void sscal(const qblas_long *N, const float *ALPHA, float *X, const qblas_long *INCX);

void dscal(const qblas_long *N, const double *ALPHA, double *X, const qblas_long *INCX);

void cscal(const qblas_long *N, const QBLAS_SINGLE_COMPLEX *ALPHA, QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

void zscal(const qblas_long *N, const QBLAS_DOUBLE_COMPLEX *ALPHA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);

void csscal(const qblas_long *N, const float *ALPHA, QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX);

void zdscal(const qblas_long *N, const double *ALPHA, QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX);


/// SDOT
float sdsdot(const qblas_long *N, const float *B, const float *X, const qblas_long *INCX, const float *Y, const qblas_long *INCY);

double dsdot(const qblas_long *N, const float *X, const qblas_long *INCX, const float *Y, const qblas_long *INCY);



/// SWAP
void sswap(const qblas_long *N, float *X, const qblas_long *INCX, float *Y, const qblas_long *INCY);

void dswap(const qblas_long *N, double *X, const qblas_long *INCX, double *Y, const qblas_long *INCY);

void cswap(const qblas_long *N, QBLAS_SINGLE_COMPLEX *X, const qblas_long *INCX, QBLAS_SINGLE_COMPLEX *Y, const qblas_long *INCY);

void zswap(const qblas_long *N, QBLAS_DOUBLE_COMPLEX *X, const qblas_long *INCX, QBLAS_DOUBLE_COMPLEX *Y, const qblas_long *INCY);

#ifdef __cplusplus
}
#endif

#endif // QBLAS_BLAS1_H
