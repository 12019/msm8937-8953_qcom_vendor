// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#define MARE_UNUSED(x) ((void)x)

#ifdef _MSC_VER
#define MARE_VS2015 1900
#define MARE_VS2013 1800
#define MARE_VS2012 1700
#endif

#ifndef MARE_INLINE
#ifdef _MSC_VER
#define MARE_INLINE __inline
#else
#define MARE_INLINE inline
#endif
#endif

#ifndef MARE_CONSTEXPR
#ifdef _MSC_VER
#define MARE_CONSTEXPR
#else
#define MARE_CONSTEXPR constexpr
#endif
#endif

#ifndef MARE_CONSTEXPR_CONST
#ifdef _MSC_VER
#define MARE_CONSTEXPR_CONST const
#else
#define MARE_CONSTEXPR_CONST constexpr
#endif
#endif

#ifdef _MSC_VER

#define MARE_ALIGN(size) __declspec(align(size))
#define MARE_ALIGNED(type, name, size) \
  __declspec(align(size)) type name

#else

#define MARE_ALIGN(size) __attribute__ ((aligned(size)))
#define MARE_ALIGNED(type, name, size) \
  type name __attribute__ ((aligned(size)))

#endif

#ifdef _MSC_VER
#define MARE_GCC_ATTRIBUTE(args)
#else
#define MARE_GCC_ATTRIBUTE(args) __attribute__(args)
#endif

#ifdef __GNUC__
#define MARE_DEPRECATED(decl) decl __attribute__((deprecated))
#elif defined(_MSC_VER)
#define MARE_DEPRECATED(decl) __declspec(deprecated) decl
#else
#warning No MARE_DEPRECATED implementation for this compiler
#define MARE_DEPRECATED(decl) decl
#endif

#if defined(_MSC_VER) && (_MSC_VER < MARE_VS2015)
#define MARE_NOEXCEPT
#else
#define MARE_NOEXCEPT noexcept
#endif

#if defined(_MSC_VER) && (_MSC_VER < MARE_VS2015)

#define MARE_DELETE_METHOD(...) __VA_ARGS__
#else

#define MARE_DELETE_METHOD(...) __VA_ARGS__ = delete
#endif

#if defined(_MSC_VER) && (_MSC_VER < MARE_VS2015)
#define MARE_DEFAULT_METHOD(...) __VA_ARGS__ { }
#else
#define MARE_DEFAULT_METHOD(...) __VA_ARGS__ = default
#endif
