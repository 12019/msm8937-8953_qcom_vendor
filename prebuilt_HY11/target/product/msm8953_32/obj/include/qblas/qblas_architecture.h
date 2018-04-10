// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--




#ifndef QBLAS_ARCHITECTURE_H
#define QBLAS_ARCHITECTURE_H

// ARM
// GCC
#ifdef __arm__
#define QBLAS_ARM_32
#endif

// MSVC
#ifdef _M_ARM
#define QBLAS_ARM_32
#endif

// ARM64
// GCC
#ifdef __aarch64__
#define QBLAS_ARM_64
#endif

// x86
// GCC
#ifdef __i386__
#define QBLAS_X86
#endif

// MSVC
#ifdef _M_IX86
#define QBLAS_X86
#endif

// x86_64
// GCC
#ifdef __x86_64__
#define QBLAS_X86_64
#endif

// MSVC
#ifdef _M_AMD64
#define QBLAS_X86_64
#endif


#endif // QBLAS_ARCHITECTURE_H
