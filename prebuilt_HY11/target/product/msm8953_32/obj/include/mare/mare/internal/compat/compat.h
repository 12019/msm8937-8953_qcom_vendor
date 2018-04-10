// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MARE_INLINE
#ifdef _MSC_VER
#define MARE_INLINE __inline
#else
#define MARE_INLINE inline
#endif
#endif

#ifdef _MSC_VER

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <errno.h>

static MARE_INLINE void __sync_synchronize (void)
{
  MemoryBarrier();
}
#endif

#ifdef _MSC_VER

typedef SSIZE_T ssize_t;
typedef int pid_t;
#else
#include <inttypes.h>
#endif

#if defined(__ANDROID__) || defined(__linux__)
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

#ifndef _MSC_VER
#include <string.h>
#endif

#if defined(__ANDROID__)
#include <mare/internal/compat/compilercompat.h>

MARE_GCC_IGNORE_BEGIN("-Wshadow")
#include <sys/stat.h>
MARE_GCC_IGNORE_END("-Wshadow")
#elif defined(__linux__)
#include <sched.h>
#else

#endif

#include <mare/internal/util/macros.hh>
#include <mare/internal/util/popcount.h>

uint64_t mare_get_time_now();

#ifdef _MSC_VER
__declspec(noreturn) void mare_exit(int);
#else
void mare_exit(int) MARE_GCC_ATTRIBUTE ((noreturn));
#endif

void
mare_android_logprintf(int level, const char *format, ...)
  MARE_GCC_ATTRIBUTE ((format (printf, 2, 3)));

#ifdef __cplusplus

#ifdef min
#ifdef _MSC_VER
static_assert(false, "Cannot use std::min when there is an existing min "
              "#define, on VS2012 you should define NOMINMAX when "
              "including Windows.h");
#else
#error Cannot use std::min when there is an existing min #define
#endif
#endif
#ifdef max
#ifdef _MSC_VER
static_assert(false, "Cannot use std::max when there is an existing min "
              "#define, on VS2012 you should define NOMINMAX "
              "when including Windows.h");
#else
#error Cannot use std::max when there is an existing max #define
#endif
#endif
#endif

#ifdef _MSC_VER
#define mare_constexpr_static_assert(cond, format, ...) \
  do { if (!cond) MARE_FATAL(format, ## __VA_ARGS__); } while (0)
#else
#define mare_constexpr_static_assert static_assert
#endif

#ifdef _MSC_VER

#ifdef __cplusplus
static inline void usleep(size_t ms) { Sleep(static_cast<DWORD>(ms) / 1000); }
static inline void sleep(size_t s) { Sleep(static_cast<DWORD>(s) * 1000); }
#else
static void usleep(size_t ms) { Sleep((DWORD)ms / 1000); }
static void sleep(size_t s) { Sleep((DWORD)s * 1000); }
#endif
#else
#include <unistd.h>
#endif

#ifdef _MSC_VER

typedef DWORD pthread_key_t;

static MARE_INLINE int pthread_key_create (pthread_key_t *key, void (*destructor)(void*))
{
  MARE_UNUSED(destructor);

  *key = TlsAlloc();
  if (*key == TLS_OUT_OF_INDEXES)
    return 1;
  else
    return 0;
}

static MARE_INLINE int pthread_key_delete (pthread_key_t key)
{

  return !TlsFree(key);
}

static MARE_INLINE void* pthread_getspecific(pthread_key_t key)
{
  return TlsGetValue(key);
}

static MARE_INLINE
int pthread_setspecific(pthread_key_t key, const void *value)
{

  return !TlsSetValue(key, (LPVOID)value);
}
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>
#ifdef _MSC_VER
static inline std::string
mare_strerror(int errnum_)
{
  char buf[1024] = "";
  strerror_s(buf, errnum_);
  return buf;
}
#elif defined(__APPLE__) || defined(__ANDROID__) ||                     \
  (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! defined _GNU_SOURCE
#include <cstring>
static inline std::string
mare_strerror(int errnum_)
{
  char buf[1024] = "";
  ::strerror_r(errnum_, buf, sizeof buf);
  return buf;
}
#else
#include <cstring>
static inline std::string
mare_strerror(int errnum_)
{
  char buf[1024] = "";
  return ::strerror_r(errnum_, buf, sizeof buf);
}
#endif
#endif

#ifdef __cplusplus
extern "C"
#endif
int mare_string_copy(char* dst, const char* src, int max);
