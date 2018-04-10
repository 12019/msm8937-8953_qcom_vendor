// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/compat/compilercompat.h>
#include <mare/internal/util/exceptions.hh>
#include <mare/internal/util/memorder.hh>

#ifdef __ANDROID__

MARE_GCC_IGNORE_BEGIN("-Wshadow")
MARE_GCC_IGNORE_BEGIN("-Wold-style-cast")
#endif
#include <atomic>
#include <cstring>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include <mare/internal/compat/compat.h>

#ifdef __ANDROID__
MARE_GCC_IGNORE_END("-Wold-style-cast")
MARE_GCC_IGNORE_END("-Wshadow")
#endif

#include <mare/internal/util/macros.hh>
#include <mare/internal/util/tls.hh>

#ifndef _MSC_VER

extern "C" void mare_breakpoint() __attribute__((noinline));
#else
MARE_INLINE void mare_breakpoint() {}
#endif

namespace mare {

namespace internal {

uintptr_t get_thread_id();

using ::mare_exit;
using ::mare_android_logprintf;

extern std::atomic<unsigned int> mare_log_counter;

};

};

#ifdef _MSC_VER

#include <string>
inline const std::string mare_c99_vsformat (const char* _format)
{
  bool last_pct = false;
  std::string fmtcopy (_format);
  for (auto& i : fmtcopy) {
    if (i == '%') {

      last_pct = !last_pct;
    } else {
      if (last_pct) {
        if (i == 'z') {

          i = 'I';
          last_pct = false;
        } else if (isdigit(i)) {

        } else {

          last_pct = false;
        }
      }
    }
  }
  return fmtcopy;
}
#endif

#ifdef __ANDROID__

#ifdef __aarch64__
#define MARE_FMT_TID "lx"
#else
#define MARE_FMT_TID "x"
#endif
#define  MARE_DLOG(format, ...)                                               \
  ::mare::internal::mare_android_logprintf (ANDROID_LOG_DEBUG,                \
      "\033[32mD %8x t%" MARE_FMT_TID " %s:%d " format "\033[0m",             \
      ::mare::internal::mare_log_counter.fetch_add(1,::mare::mem_order_relaxed), \
      ::mare::internal::get_thread_id(), __FILE__, __LINE__ , ## __VA_ARGS__)
#define  MARE_DLOGN(format, ...)                                              \
  ::mare::internal::mare_android_logprintf (ANDROID_LOG_DEBUG,                \
      "\033[32mD %8x t%" MARE_FMT_TID " %s:%d " format "\033[0m",             \
      ::mare::internal::mare_log_counter.fetch_add(1,::mare::mem_order_relaxed),\
      ::mare::internal::get_thread_id(), __FILE__, __LINE__ , ## __VA_ARGS__)
#define  MARE_ILOG(format, ...)                                               \
  ::mare::internal::mare_android_logprintf (ANDROID_LOG_INFO,                 \
      "\033[33mI %8x t%" MARE_FMT_TID " %s:%d " format "\033[0m",             \
      ::mare::internal::mare_log_counter.fetch_add(1,::mare::mem_order_relaxed),\
      ::mare::internal::get_thread_id(), __FILE__, __LINE__ , ## __VA_ARGS__)
#define  MARE_WLOG(format, ...)                                               \
  ::mare::internal::mare_android_logprintf (ANDROID_LOG_WARN,                 \
      "\033[35mW %8x t%" MARE_FMT_TID " %s:%d " format "\033[0m",             \
      ::mare::internal::mare_log_counter.fetch_add(1,::mare::mem_order_relaxed),\
      ::mare::internal::get_thread_id(), __FILE__, __LINE__ , ## __VA_ARGS__)
#define  MARE_EXIT_FATAL(format, ...)                                         \
  ::mare::internal::mare_android_logprintf (ANDROID_LOG_ERROR,                \
      "\033[31mFATAL %8x t%" MARE_FMT_TID " %s:%d %s() " format,              \
      ::mare::internal::mare_log_counter.fetch_add(1,::mare::mem_order_relaxed),\
      ::mare::internal::get_thread_id(), __FILE__, __LINE__, __FUNCTION__ ,   \
      ## __VA_ARGS__),                                                        \
  ::mare::internal::mare_android_logprintf (ANDROID_LOG_ERROR,                \
      "t%" MARE_FMT_TID " %s:%d **********",                                  \
      ::mare::internal::get_thread_id(), __FILE__, __LINE__),                 \
  ::mare::internal::mare_android_logprintf (ANDROID_LOG_ERROR,                \
      "t%" MARE_FMT_TID " %s:%d - Terminating with exit(1)",                  \
      ::mare::internal::get_thread_id(), __FILE__, __LINE__),                 \
  ::mare::internal::mare_android_logprintf (ANDROID_LOG_ERROR,                \
      "t%" MARE_FMT_TID " %s:%d **********\033[0m",                           \
      ::mare::internal::get_thread_id(), __FILE__, __LINE__),                 \
  mare_exit(1)
#define  MARE_ALOG(format, ...)                                               \
  ::mare::internal::mare_android_logprintf (ANDROID_LOG_INFO,                 \
      "\033[36mA %8x t%" MARE_FMT_TID " %s:%d " format "\033[0m",             \
      ::mare::internal::mare_log_counter.fetch_add(1,::mare::mem_order_relaxed),\
      ::mare::internal::get_thread_id(), __FILE__, __LINE__ , ## __VA_ARGS__)
#define  MARE_LLOG(format, ...)                                               \
  ::mare::internal::mare_android_logprintf (ANDROID_LOG_INFO, format "\n",    \
                                            ## __VA_ARGS__)
#else

#if defined(PRIxPTR)
#define MARE_FMT_TID PRIxPTR
#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(__ARM_ARCH_7A__)
#define MARE_FMT_TID "x"
#else
#define MARE_FMT_TID "lx"
#endif
#ifdef _MSC_VER

#define MARE_LOG_FPRINTF(stream, fmt, ...) fprintf(stream, mare_c99_vsformat(fmt).c_str(), ## __VA_ARGS__)
#else
#define MARE_LOG_FPRINTF(stream, fmt, ...) fprintf(stream, fmt, ## __VA_ARGS__)
#endif
#define  MARE_DLOG(format, ...) MARE_LOG_FPRINTF(stderr,"\033[32mD %8x t%" MARE_FMT_TID " %s:%d " format "\033[0m\n", mare::internal::mare_log_counter.fetch_add(1,mare::mem_order_relaxed), mare::internal::get_thread_id(), __FILE__, __LINE__ , ## __VA_ARGS__)
#define  MARE_DLOGN(format, ...) MARE_LOG_FPRINTF(stderr,"\033[32mD %8x t%" MARE_FMT_TID " %s:%d " format "\033[0m", mare::internal::mare_log_counter.fetch_add(1,mare::mem_order_relaxed), mare::internal::get_thread_id(), __FILE__, __LINE__ , ## __VA_ARGS__)
#define  MARE_ILOG(format, ...) MARE_LOG_FPRINTF(stderr,"\033[33mI %8x t%" MARE_FMT_TID " %s:%d " format "\033[0m\n", mare::internal::mare_log_counter.fetch_add(1,mare::mem_order_relaxed), mare::internal::get_thread_id(), __FILE__, __LINE__ , ## __VA_ARGS__)
#define  MARE_WLOG(format, ...) MARE_LOG_FPRINTF(stderr,"\033[35mW %8x t%" MARE_FMT_TID " %s:%d " format "\033[0m\n", mare::internal::mare_log_counter.fetch_add(1,mare::mem_order_relaxed), mare::internal::get_thread_id(), __FILE__, __LINE__ , ## __VA_ARGS__)
#define  MARE_EXIT_FATAL(format, ...) MARE_LOG_FPRINTF(stderr,"\033[31mFATAL %8x t%" MARE_FMT_TID " %s:%d %s() " format "\n", mare::internal::mare_log_counter.fetch_add(1,mare::mem_order_relaxed), mare::internal::get_thread_id(), __FILE__, __LINE__, __FUNCTION__ , ## __VA_ARGS__), MARE_LOG_FPRINTF(stderr,"t%" MARE_FMT_TID " %s:%d - Terminating with exit(1)\033[0m\n", mare::internal::get_thread_id(), __FILE__, __LINE__), mare_exit(1)
#define  MARE_ALOG(format, ...) MARE_LOG_FPRINTF(stderr,"\033[36mA %8x t%" MARE_FMT_TID " %s:%d " format "\033[0m\n", mare::internal::mare_log_counter.fetch_add(1,mare::mem_order_relaxed), mare::internal::get_thread_id(), __FILE__, __LINE__ , ## __VA_ARGS__)
#define  MARE_LLOG(format, ...) MARE_LOG_FPRINTF(stderr, format "\n", ## __VA_ARGS__)
#endif

#define MARE_UNIMPLEMENTED(format, ...)  MARE_EXIT_FATAL("Unimplemented code in function '%s' at %s:%d " format, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__)
#define MARE_UNREACHABLE(format, ...)  MARE_EXIT_FATAL("Unreachable code triggered in function '%s' at %s:%d " format, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__)

#ifndef MARE_THROW_ON_API_ASSERT
#define MARE_FATAL(format, ...) MARE_EXIT_FATAL(format, ## __VA_ARGS__)
#else
#define MARE_FATAL(format, ...) do {                                    \
    throw ::mare::internal::fatal_exception(::mare::internal::          \
                              strprintf(format, ## __VA_ARGS__),        \
                              __FILE__, __LINE__, __FUNCTION__);        \
    } while(false)
#endif

#ifdef  MARE_THROW_ON_API_ASSERT
#define MARE_API_ASSERT(cond, format, ...) do {                         \
    if(!(cond)) { throw                                                 \
        ::mare::internal::api_assert_exception(::mare::internal::       \
                              strprintf(format, ## __VA_ARGS__),        \
                              __FILE__, __LINE__, __FUNCTION__);        \
    } } while(false)
#else
#define MARE_API_ASSERT_COND(cond) #cond
#define MARE_API_ASSERT(cond, format, ...) do { if(!(cond)) { \
      MARE_EXIT_FATAL("API assert [%s] - " format, MARE_API_ASSERT_COND(cond), ## __VA_ARGS__); } \
  } while(false)
#endif

#ifdef MARE_DEBUG
#define MARE_CHECK_INTERNAL
#endif

#ifdef MARE_DISABLE_CHECK_INTERNAL
#undef MARE_CHECK_INTERNAL
#endif

#ifdef MARE_ENABLE_CHECK_INTERNAL
#ifndef MARE_CHECK_INTERNAL
#define MARE_CHECK_INTERNAL
#endif
#endif

#ifdef MARE_CHECK_INTERNAL
#ifndef MARE_THROW_ON_API_ASSERT
#define MARE_INTERNAL_ASSERT_COND(cond) #cond
#define MARE_INTERNAL_ASSERT(cond, format, ...) do { if(!(cond)) {      \
      MARE_EXIT_FATAL("Internal assert failed [%s] - " format, MARE_INTERNAL_ASSERT_COND(cond), ## __VA_ARGS__); } \
  } while(false)
#else
#define MARE_INTERNAL_ASSERT(cond, format, ...) do {                    \
    if(!(cond)) { throw                                                 \
        ::mare::internal::assert_exception(::mare::internal::           \
                              strprintf(format, ## __VA_ARGS__),        \
                              __FILE__, __LINE__, __FUNCTION__);        \
    } } while(false)
#endif
#else
#define MARE_INTERNAL_ASSERT(cond, format, ...) do {                    \
      if(false) { if(cond){} if(format){} } } while(false)
#endif

#ifndef MARE_DEBUG

#ifndef MARE_NOLOG
# define MARE_NOLOG
#endif
#endif

#ifdef MARE_NOLOG
# undef  MARE_DLOG
# undef  MARE_DLOGN
# undef  MARE_ILOG
# undef  MARE_WLOG
# define  MARE_DLOG(format, ...) do {} while (false)
# define  MARE_DLOGN(format, ...) do {} while (false)
# define  MARE_ILOG(format, ...) do {} while (false)
# define  MARE_WLOG(format, ...) do {} while (false)
#endif
