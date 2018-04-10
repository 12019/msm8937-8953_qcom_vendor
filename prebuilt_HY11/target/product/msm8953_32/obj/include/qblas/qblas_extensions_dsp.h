// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#ifndef _QBLAS_EXTENSIONS_DSP_H
#define _QBLAS_EXTENSIONS_DSP_H
#include "AEEStdDef.h"
#ifndef __QAIC_HEADER
#define __QAIC_HEADER(ff) ff
#endif //__QAIC_HEADER

#ifndef __QAIC_HEADER_EXPORT
#define __QAIC_HEADER_EXPORT
#endif // __QAIC_HEADER_EXPORT

#ifndef __QAIC_HEADER_ATTRIBUTE
#define __QAIC_HEADER_ATTRIBUTE
#endif // __QAIC_HEADER_ATTRIBUTE

#ifndef __QAIC_IMPL
#define __QAIC_IMPL(ff) ff
#endif //__QAIC_IMPL

#ifndef __QAIC_IMPL_EXPORT
#define __QAIC_IMPL_EXPORT
#endif // __QAIC_IMPL_EXPORT

#ifndef __QAIC_IMPL_ATTRIBUTE
#define __QAIC_IMPL_ATTRIBUTE
#endif // __QAIC_IMPL_ATTRIBUTE
#ifdef __cplusplus
extern "C" {
#endif
/// Fixed point extensions
__QAIC_HEADER_EXPORT AEEResult __QAIC_HEADER(AMF_hexagon_i32i16gemm_sat)(char TRANSA, char TRANSB, int M, int N, int K, short ALPHA, const short* A, int ALen, int LDA, const short* B, int BLen, int LDB, short BETA, const int* CIn, int CInLen, int* COut, int COutLen, int LDC) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT AEEResult __QAIC_HEADER(AMF_hexagon_cblas_i32i16gemm_sat)(int ORDER, int TRANSA, int TRANSB, int M, int N, int K, short ALPHA, const short* A, int ALen, int LDA, const short* B, int BLen, int LDB, short BETA, const int* CIn, int CInLen, int* COut, int COutLen, int LDC) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT AEEResult __QAIC_HEADER(AMF_hexagon_i32i16convolution_mm_sat)(int inputFormat, int outputFormat, boolean biased, const short* src, int srcLen, int srcWidth, int srcHeight, int srcNumChannels, const short* filters, int filtersLen, int numFilters, int filterSize, int padding, int stride, const int* inOutput, int inOutputLen, int* output, int outputLen, int outputWidth, int outputHeight) __QAIC_HEADER_ATTRIBUTE;
#ifdef __cplusplus
}
#endif
#endif //_QBLAS_EXTENSIONS_DSP_H
