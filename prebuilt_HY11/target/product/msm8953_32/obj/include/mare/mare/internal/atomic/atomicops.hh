// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <atomic>

#include <mare/internal/util/debug.hh>

#if !defined(_MSC_VER)
  #define MARE_SIZEOF_MWORD __SIZEOF_SIZE_T__
#else
  #if _M_X64
    #define MARE_SIZEOF_MWORD 8
  #else
    #define MARE_SIZEOF_MWORD 4
  #endif
#endif

#define MARE_SIZEOF_DMWORD (MARE_SIZEOF_MWORD * 2)

namespace mare {
namespace internal {

template<size_t N, typename T>
class atomicops {
private:
  static inline void load_unchecked(T const* src, T* dest,
                                    std::memory_order m);
  static inline void store_unchecked(T* dest, T const* desired,
                                     std::memory_order m);
  static inline bool cmpxchg_unchecked(T* dest, T* expected,
                                       T const* desired, bool weak,
                                       std::memory_order msuccess,
                                       std::memory_order mfailure);

  static inline T
  fetch_add_unchecked(T* dest, const T& incr, std::memory_order m)
  {
    T old = T();
    std::memory_order m_load = (m == std::memory_order_acq_rel ?
                                std::memory_order_acquire : m);
    bool done = false;
    while (!done) {
      load_unchecked(dest, &old, m_load);
      T desired = old + incr;
      done = cmpxchg_unchecked(dest, &old, &desired, true, m, m);
    }
    return old;
  }

  static inline T
  fetch_sub_unchecked(T* dest, const T& incr, std::memory_order m)
  {
    T old = T();
    std::memory_order m_load = (m == std::memory_order_acq_rel ?
                                std::memory_order_acquire : m);
    bool done = false;
    while (!done) {
      load_unchecked(dest, &old, m_load);
      T desired = old - incr;
      done = cmpxchg_unchecked(dest, &old, &desired, true, m, m);
    }
    return old;
  }

  static inline T
  fetch_and_unchecked(T* dest, const T& incr, std::memory_order m)
  {
    T old = T();
    std::memory_order m_load = (m == std::memory_order_acq_rel ?
                                std::memory_order_acquire : m);
    bool done = false;
    while (!done) {
      load_unchecked(dest, &old, m_load);
      T desired = old & incr;
      done = cmpxchg_unchecked(dest, &old, &desired, true, m, m);
    }
    return old;
  }

  static inline T
  fetch_or_unchecked(T* dest, const T& incr, std::memory_order m)
  {
    T old = T();
    std::memory_order m_load = (m == std::memory_order_acq_rel ?
                                std::memory_order_acquire : m);
    bool done = false;
    while (!done) {
      load_unchecked(dest, &old, m_load);
      T desired = old | incr;
      done = cmpxchg_unchecked(dest, &old, &desired, true, m, m);
    }
    return old;
  }

  static inline T
  fetch_xor_unchecked(T* dest, const T& incr, std::memory_order m)
  {
    T old = T();
    std::memory_order m_load = (m == std::memory_order_acq_rel ?
                                std::memory_order_acquire : m);
    bool done = false;
    while (!done) {
      load_unchecked(dest, &old, m_load);
      T desired = old ^ incr;
      done = cmpxchg_unchecked(dest, &old, &desired, true, m, m);
    }
    return old;
  }

public:
  static inline void load(
      T const* src, T* dest,
      std::memory_order m = std::memory_order_seq_cst) {
    MARE_INTERNAL_ASSERT(m == std::memory_order_consume ||
                         m == std::memory_order_acquire ||
                         m == std::memory_order_seq_cst ||
                         m == std::memory_order_relaxed,
                         "Supplied memory order is undefined: %u", m);
    load_unchecked(src, dest, m);
  }

  static inline void store(
      T* dest, T const* desired,
      std::memory_order m = std::memory_order_seq_cst) {

    MARE_INTERNAL_ASSERT(m == std::memory_order_release ||
                         m == std::memory_order_seq_cst ||
                         m == std::memory_order_relaxed,
                         "Supplied memory order is undefined: %u", m);
    store_unchecked(dest, desired, m);
  }

  static inline bool cmpxchg(
      T* dest, T* expected, T const* desired, bool weak,
      std::memory_order msuccess, std::memory_order mfailure) {

    MARE_INTERNAL_ASSERT(msuccess <= std::memory_order_seq_cst &&
                         msuccess >= std::memory_order_relaxed,
                         "Supplied success memory order is undefined: %u",
                         msuccess);
    MARE_INTERNAL_ASSERT(mfailure != std::memory_order_release &&
                         mfailure != std::memory_order_acq_rel &&
                         mfailure >= std::memory_order_relaxed &&
                         mfailure <= msuccess,
                         "Supplied failure memory order is not allowed: %u",
                         mfailure);
    return cmpxchg_unchecked(dest, expected, desired, weak,
                             msuccess, mfailure);
  }

  static inline bool cmpxchg(
      T* dest, T* expected, T const* desired, bool weak,
      std::memory_order order = std::memory_order_seq_cst) {
    if (order == std::memory_order_acq_rel) {
      return cmpxchg(dest, expected, desired, weak, order,
                     std::memory_order_release);
    }
    if (order == std::memory_order_release) {
      return cmpxchg(dest, expected, desired, weak, order,
                     std::memory_order_relaxed);
    }
    return cmpxchg(dest, expected, desired, weak, order, order);
  }

  static inline T
  fetch_add(T* dest, const T& incr,
            std::memory_order m = std::memory_order_seq_cst)
  {
    MARE_INTERNAL_ASSERT(m == std::memory_order_seq_cst ||
                         m == std::memory_order_acq_rel ||
                         m == std::memory_order_relaxed,
                         "Invalid memory order %u", m);
    return fetch_add_unchecked(dest, incr, m);
  }
  static inline T
  fetch_sub(T* dest, const T& incr,
            std::memory_order m = std::memory_order_seq_cst)
  {
    MARE_INTERNAL_ASSERT(m == std::memory_order_seq_cst ||
                         m == std::memory_order_acq_rel ||
                         m == std::memory_order_relaxed,
                         "Invalid memory order %u", m);
    return fetch_sub_unchecked(dest, incr, m);
  }
  static inline T
  fetch_and(T* dest, const T& incr,
            std::memory_order m = std::memory_order_seq_cst)
  {
    MARE_INTERNAL_ASSERT(m == std::memory_order_seq_cst ||
                         m == std::memory_order_acq_rel ||
                         m == std::memory_order_relaxed,
                         "Invalid memory order %u", m);
    return fetch_and_unchecked(dest, incr, m);
  }
  static inline T
  fetch_or(T* dest, const T& incr,
           std::memory_order m = std::memory_order_seq_cst)
  {
    MARE_INTERNAL_ASSERT(m == std::memory_order_seq_cst ||
                         m == std::memory_order_acq_rel ||
                         m == std::memory_order_relaxed,
                         "Invalid memory order %u", m);
    return fetch_or_unchecked(dest, incr, m);
  }
  static inline T
  fetch_xor(T* dest, const T& incr,
            std::memory_order m = std::memory_order_seq_cst)
  {
    MARE_INTERNAL_ASSERT(m == std::memory_order_seq_cst ||
                         m == std::memory_order_acq_rel ||
                         m == std::memory_order_relaxed,
                         "Invalid memory order %u", m);
    return fetch_xor_unchecked(dest, incr, m);
  }
};

static inline void mare_atomic_thread_fence(std::memory_order m);

};

};

#if defined(i386) || defined(__i386) || defined(__i386__) ||defined(__x86_64__)
#include <mare/internal/atomic/atomicops-x86.hh>
#elif defined(__ARM_ARCH_7A__)
#include <mare/internal/atomic/atomicops-armv7.hh>
#elif defined(__aarch64__)
#include <mare/internal/atomic/atomicops-armv8.hh>
#elif defined(__GNUC__)
#include <mare/internal/atomic/atomicops-gcc.hh>
#elif defined(_M_IX86) || defined(_M_X64)
#include <mare/internal/atomic/atomicops-msvc.hh>
#else
#error No implementation for atomic operations available for this architecture.
#endif
