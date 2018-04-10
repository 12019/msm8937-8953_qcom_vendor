// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef __GNUC__
#define MARE_GCC_PRAGMA(x) _Pragma(#x)
#define MARE_GCC_PRAGMA_DIAG(x) MARE_GCC_PRAGMA(GCC diagnostic x)
#define MARE_GCC_IGNORE_BEGIN(x) MARE_GCC_PRAGMA_DIAG(push)             \
  MARE_GCC_PRAGMA_DIAG(ignored x)
#define MARE_GCC_IGNORE_END(x) MARE_GCC_PRAGMA_DIAG(pop)
#ifdef __clang__
#define MARE_CLANG_PRAGMA_DIAG(x) MARE_GCC_PRAGMA(clang diagnostic x)
#define MARE_CLANG_IGNORE_BEGIN(x) MARE_CLANG_PRAGMA_DIAG(push)         \
  MARE_CLANG_PRAGMA_DIAG(ignored x)
#define MARE_CLANG_IGNORE_END(x) MARE_CLANG_PRAGMA_DIAG(pop)
#else
#define MARE_CLANG_IGNORE_BEGIN(x)
#define MARE_CLANG_IGNORE_END(x)
#endif
#else
#define MARE_GCC_IGNORE_BEGIN(x)
#define MARE_GCC_IGNORE_END(x)
#define MARE_CLANG_IGNORE_BEGIN(x)
#define MARE_CLANG_IGNORE_END(x)
#endif

#ifdef _MSC_VER
#define MARE_MSC_PRAGMA(x) __pragma(x)
#define MARE_MSC_PRAGMA_WARNING(x) MARE_MSC_PRAGMA(warning(x))
#define MARE_MSC_IGNORE_BEGIN(x) MARE_MSC_PRAGMA_WARNING(push)          \
  MARE_MSC_PRAGMA_WARNING(disable : x)
#define MARE_MSC_IGNORE_END(x) MARE_MSC_PRAGMA_WARNING(pop)
#else
#define MARE_MSC_IGNORE_BEGIN(x)
#define MARE_MSC_IGNORE_END(x)
#endif

#if defined(__clang__)

  #define MARE_PUBLIC __attribute__ ((visibility ("default")))
  #define MARE_PUBLIC_START  MARE_GCC_PRAGMA(GCC visibility push(default))
  #define MARE_PUBLIC_STOP   MARE_GCC_PRAGMA(GCC visibility pop)
  #define MARE_PRIVATE __attribute__ ((visibility ("hidden")))
  #define MARE_PRIVATE_START MARE_GCC_PRAGMA(GCC visibility push(hidden))
  #define MARE_PRIVATE_STOP  MARE_GCC_PRAGMA(GCC visibility pop)
#elif defined(__GNUC__)
  #define MARE_PUBLIC  __attribute__ ((visibility ("default")))
  #define MARE_PUBLIC_START  MARE_GCC_PRAGMA(GCC visibility push(default))
  #define MARE_PUBLIC_STOP   MARE_GCC_PRAGMA(GCC visibility pop)
  #define MARE_PRIVATE __attribute__ ((visibility ("hidden")))
  #define MARE_PRIVATE_START MARE_GCC_PRAGMA(GCC visibility push(hidden))
  #define MARE_PRIVATE_STOP  MARE_GCC_PRAGMA(GCC visibility pop)
#elif defined(_MSC_VER)
  #ifdef MARE_DLL_EXPORT

    #define MARE_PUBLIC __declspec(dllexport)
  #else

    #define MARE_PUBLIC __declspec(dllimport)
  #endif
  #define MARE_PRIVATE
#else
  #error "Unknown compiler. Please use GCC, Clang, or Visual Studio!"
#endif
