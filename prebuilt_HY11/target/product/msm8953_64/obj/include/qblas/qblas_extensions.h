// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--



#ifndef QBLAS_EXTENSIONS_H
#define QBLAS_EXTENSIONS_H

#include <qblas_cblas_common.h>
#include <qblas_extensions_common.h>
#include <qblas_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/// biased = false -> output  = image * filter
/// biased = true  -> output += image *filter
void convolution_mm(const QBLAS_IMAGE_FORMAT inputFormat, const QBLAS_IMAGE_FORMAT outputFormat, const bool biased, const float* src, const qblas_int srcWidth, const qblas_int srcHeight,
                    const qblas_int srcNumChannels, const float* filters, const qblas_int numFilters, const qblas_int filterSize, const qblas_int padding, const qblas_int stride,
                    float* output, const qblas_int outputWidth, const qblas_int outputHeight);


void convolution_mm2(const QBLAS_IMAGE_FORMAT inputFormat, const QBLAS_IMAGE_FORMAT outputFormat, const bool biased, const float* src, const qblas_int srcWidth, const qblas_int srcHeight,
                    const qblas_int srcNumChannels, const float* filters, const qblas_int numFilters, const qblas_int filterX, const qblas_int filterY, const qblas_int padLeft, const qblas_int padTop,
                    const qblas_int strideX, const qblas_int strideY, float* output, const qblas_int outputWidth, const qblas_int outputHeight);


void i16convolution_mm(const QBLAS_IMAGE_FORMAT inputFormat, const QBLAS_IMAGE_FORMAT outputFormat, const bool biased, const qblas_int16* src, const qblas_int srcWidth, const qblas_int srcHeight,
                    const qblas_int srcNumChannels, const qblas_int16* filters, const qblas_int numFilters, const qblas_int filterSize, const qblas_int padding, const qblas_int stride,
                    qblas_int16* output, const qblas_int outputWidth, const qblas_int outputHeight);


void i16convolution_mm2(const QBLAS_IMAGE_FORMAT inputFormat, const QBLAS_IMAGE_FORMAT outputFormat, const bool biased, const qblas_int16* src, const qblas_int srcWidth, const qblas_int srcHeight,
                    const qblas_int srcNumChannels, const qblas_int16* filters, const qblas_int numFilters, const qblas_int filterX, const qblas_int filterY, const qblas_int padLeft, const qblas_int padTop,
                    const qblas_int strideX, const qblas_int strideY, qblas_int16* output, const qblas_int outputWidth, const qblas_int outputHeight);

void i32i16convolution_mm(const QBLAS_IMAGE_FORMAT inputFormat, const QBLAS_IMAGE_FORMAT outputFormat, const bool biased, const qblas_int16* src, const qblas_int srcWidth, const qblas_int srcHeight,
                    const qblas_int srcNumChannels, const qblas_int16* filters, const qblas_int numFilters, const qblas_int filterSize, const qblas_int padding, const qblas_int stride,
                    qblas_int* output, const qblas_int outputWidth, const qblas_int outputHeight);


void i32i16convolution_mm2(const QBLAS_IMAGE_FORMAT inputFormat, const QBLAS_IMAGE_FORMAT outputFormat, const bool biased, const qblas_int16* src, const qblas_int srcWidth, const qblas_int srcHeight,
                    const qblas_int srcNumChannels, const qblas_int16* filters, const qblas_int numFilters, const qblas_int filterX, const qblas_int filterY, const qblas_int padLeft, const qblas_int padTop,
                    const qblas_int strideX, const qblas_int strideY, qblas_int* output, const qblas_int outputWidth, const qblas_int outputHeight);

void i32i16convolution_mm_sat_dsp(const QBLAS_IMAGE_FORMAT inputFormat, const QBLAS_IMAGE_FORMAT outputFormat, const bool biased, const qblas_int16* src,
                                  const qblas_int srcWidth, const qblas_int srcHeight, const qblas_int srcNumChannels, const qblas_int16* filters, const qblas_int numFilters,
                                  const qblas_int filterSize, const qblas_int padding, const qblas_int stride, qblas_int* output, const qblas_int outputWidth, const qblas_int outputHeight);


void i32i16convolution_mm_sr8(const QBLAS_IMAGE_FORMAT inputFormat, const QBLAS_IMAGE_FORMAT outputFormat, const bool biased, const qblas_int16* src, const qblas_int srcWidth, const qblas_int srcHeight,
                    const qblas_int srcNumChannels, const qblas_int16* filters, const qblas_int numFilters, const qblas_int filterSize, const qblas_int padding, const qblas_int stride,
                    qblas_int* output, const qblas_int outputWidth, const qblas_int outputHeight);

void i32i16convolution_mm2_sr8(const QBLAS_IMAGE_FORMAT inputFormat, const QBLAS_IMAGE_FORMAT outputFormat, const bool biased, const qblas_int16* src,
                    const qblas_int srcWidth, const qblas_int srcHeight, const qblas_int srcNumChannels,
                    const qblas_int16* filters, const qblas_int numFilters, const qblas_int filterX, const qblas_int filterY, const qblas_int padLeft, const qblas_int padTop,
                    const qblas_int strideX, const qblas_int strideY, qblas_int* output, const qblas_int outputWidth, const qblas_int outputHeight);


/// GEMM
void i16gemm(const char *TRANSA, const char *TRANSB, const qblas_long *M, const qblas_long *N, const qblas_long *K, const qblas_int16 *ALPHA,
           const qblas_int16 *A, const qblas_long *LDA, const qblas_int16 *B, const qblas_long *LDB,
           const qblas_int16 *BETA, qblas_int16 *C, const qblas_long *LDC);

void i32i16gemm(const char *TRANSA, const char *TRANSB, const qblas_long *M, const qblas_long *N, const qblas_long *K, const qblas_int16 *ALPHA,
                    const qblas_int16 *A, const qblas_long *LDA, const qblas_int16 *B, const qblas_long *LDB,
                    const qblas_int16 *BETA, qblas_int *C, const qblas_long *LDC);

void i32i16gemm_sr8(const char *TRANSA, const char *TRANSB, const qblas_long *M, const qblas_long *N, const qblas_long *K, const qblas_int16 *ALPHA,
                    const qblas_int16 *A, const qblas_long *LDA, const qblas_int16 *B, const qblas_long *LDB,
                    const qblas_int16 *BETA, qblas_int *C, const qblas_long *LDC);

void i32i16gemm_sat_dsp(const char *TRANSA, const char *TRANSB, const qblas_long *M, const qblas_long *N, const qblas_long *K, const qblas_int16 *ALPHA,
                    const qblas_int16 *A, const qblas_long *LDA, const qblas_int16 *B, const qblas_long *LDB,
                    const qblas_int16 *BETA, qblas_int *C, const qblas_long *LDC);

/// CBLAS GEMM
void cblas_i16gemm(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANSA, const CBLAS_TRANSPOSE TRANSB, const qblas_long M, const qblas_long N, const qblas_long K, const qblas_int16 ALPHA,
           const qblas_int16 *A, const qblas_long LDA, const qblas_int16 *B, const qblas_long LDB,
           const qblas_int16 BETA, qblas_int16 *C, const qblas_long LDC);

void cblas_i32i16gemm(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANSA, const CBLAS_TRANSPOSE TRANSB, const qblas_long M, const qblas_long N, const qblas_long K, const qblas_int16 ALPHA,
           const qblas_int16 *A, const qblas_long LDA, const qblas_int16 *B, const qblas_long LDB,
           const qblas_int16 BETA, qblas_int *C, const qblas_long LDC);

void cblas_i32i16gemm_sr8(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANSA, const CBLAS_TRANSPOSE TRANSB, const qblas_long M, const qblas_long N, const qblas_long K, const qblas_int16 ALPHA,
           const qblas_int16 *A, const qblas_long LDA, const qblas_int16 *B, const qblas_long LDB,
           const qblas_int16 BETA, qblas_int *C, const qblas_long LDC);

void cblas_i32i16gemm_sat_dsp(const CBLAS_ORDER ORDER, const CBLAS_TRANSPOSE TRANSA, const CBLAS_TRANSPOSE TRANSB, const qblas_long M, const qblas_long N, const qblas_long K, const qblas_int16 ALPHA,
           const qblas_int16 *A, const qblas_long LDA, const qblas_int16 *B, const qblas_long LDB,
           const qblas_int16 BETA, qblas_int *C, const qblas_long LDC);


#ifdef __cplusplus
}
#endif

#endif // QBLAS_EXTENSIONS_H
