// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--



#ifndef QBLAS_TYPES_H
#define QBLAS_TYPES_H

#ifdef __cplusplus
    #include <complex>
    #include <cstdint>
#else
    #include <complex.h>
    #include <stdint.h>
#endif

#include <qblas_architecture.h>


#if defined(QBLAS_STRICT_BLAS_SPEC) || defined(__hexagon__)

typedef int32_t qblas_int;
typedef int32_t qblas_long;

typedef uint32_t qblas_uint;
typedef uint32_t qblas_ulong;

#else
    #if defined(QBLAS_ARM_32) || defined(QBLAS_X86)

    typedef int32_t qblas_int;
    typedef int32_t qblas_long;

    typedef uint32_t qblas_uint;
    typedef uint32_t qblas_ulong;

    #elif defined(QBLAS_ARM_64) || defined(QBLAS_X86_64)

    typedef int32_t qblas_int;
    typedef int64_t qblas_long;

    typedef uint32_t qblas_uint;
    typedef uint64_t qblas_ulong;

    #endif
#endif

#if defined(QBLAS_ARM_32) || defined(QBLAS_X86) || defined(__hexagon__)

    typedef uint32_t qblas_ptr_size;

#elif defined(QBLAS_ARM_64) || defined(QBLAS_X86_64)

    typedef uint64_t qblas_ptr_size;

#endif

typedef int16_t qblas_int16;
typedef uint16_t qblas_uint16;
typedef int8_t  qblas_int8;
typedef uint8_t qblas_uint8;

#ifdef __cplusplus
    typedef std::complex<float> qblas_single_complex;
    typedef std::complex<double> qblas_double_complex;
#else
    typedef float complex qblas_single_complex;
    typedef double complex qblas_double_complex;
#endif

#if defined(__cplusplus) && defined(__hexagon__)
    #undef I
#endif

#endif // QBLAS_TYPES_H
