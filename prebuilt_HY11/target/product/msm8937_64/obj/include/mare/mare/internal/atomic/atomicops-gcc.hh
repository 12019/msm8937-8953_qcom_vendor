// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#if __LP64__
#define MARE_HAS_ATOMIC_DMWORD __GLIBCXX_USE_INT128
#else
#define MARE_HAS_ATOMIC_DMWORD 1
#endif

#if __GNUC__ == 4 && __GNUC_MINOR__ < 7
#define MARE_USE_GCC_SYNC_ATOMICS
#if defined(__ARM_ARCH_5TE__) && __ARM_ARCH_5TE__
#include "atomicops-armv5te.hh"
#endif
#endif

namespace mare {
namespace internal {

#if __GLIBCXX_USE_INT128
typedef unsigned __int128 mare_dmword_t;
#else
typedef uint64_t mare_dmword_t;
#endif

template<size_t N, typename T>
inline void
atomicops<N,T>::load_unchecked(T const* src, T* dest, std::memory_order m)
{
#ifndef MARE_USE_GCC_SYNC_ATOMICS
  __atomic_load(src, dest, m);
#else
  MARE_UNUSED(m);
  switch (N) {
  case 4:
    {
      uint32_t* ptr = const_cast<uint32_t*>(
          reinterpret_cast<uint32_t const*>(src));
      uint32_t* des = reinterpret_cast<uint32_t*>(dest);

      *des = __sync_val_compare_and_swap(ptr, 0, 0);
    }
    break;
  case 8:
    {
      uint64_t* ptr = const_cast<uint64_t*>(
          reinterpret_cast<uint64_t const*>(src));
      uint64_t* des = reinterpret_cast<uint64_t*>(dest);
      *des = __sync_val_compare_and_swap(ptr, 0, 0);
    }
    break;
#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
  case 16:
    {
      unsigned __int128* ptr =  const_cast<unsigned __int128*>(
          reinterpret_cast<unsigned __int128>(src));
      unsigned __int128* des = reinterpret_cast<unsigned __int128*>(dest);
      *des = __sync_val_compare_and_swap(ptr, 0, 0);
    }
    break;
#endif
  default:
    MARE_UNREACHABLE("unsupported size for atomicops");
    break;
  }
#endif

  MARE_UNUSED(src);
}

template<typename T>
static inline void
gcc_sync_atomics_store(T* ptr, T newval)
{
      for (;;) {
        auto oldval = *ptr;
        if (__sync_bool_compare_and_swap(ptr, oldval, newval))
          return;
        asm volatile ("" ::: "memory");
      }
}

template<size_t N, typename T>
inline void
atomicops<N,T>::store_unchecked(T* dest, T const* desired, std::memory_order m)
{
#ifndef MARE_USE_GCC_SYNC_ATOMICS
  __atomic_store(dest, desired, m);
#else
  MARE_UNUSED(m);
  switch (N) {
  case 4:
    {
      uint32_t* ptr = reinterpret_cast<uint32_t*>(dest);
      uint32_t newval = *reinterpret_cast<uint32_t const*>(desired);
      gcc_sync_atomics_store(ptr, newval);
    }
    break;
  case 8:
    {
      uint64_t* ptr = reinterpret_cast<uint64_t*>(dest);
      uint64_t newval = *reinterpret_cast<uint64_t const*>(desired);
      gcc_sync_atomics_store(ptr, newval);
    }
    break;
#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
  case 16:
    {
      unsigned __int128* ptr = reinterpret_cast<unsigned __int128*>(dest);
      unsigned __int128 newval =
        *reinterpret_cast<unsigned __int128 const*>(desired);
      gcc_sync_atomics_store(ptr, oldval, newval);
    }
    break;
#endif
  default:
    MARE_UNREACHABLE("unsupported size for atomicops");
    break;
  }
#endif
  MARE_UNUSED(dest);
}

template<size_t N, typename T>
inline bool
atomicops<N,T>::cmpxchg_unchecked(T* dest, T* expected,
                             T const* desired, bool weak,
                             std::memory_order msuccess,
                             std::memory_order mfailure)
{
  bool ret;

#ifndef MARE_USE_GCC_SYNC_ATOMICS
  ret = __atomic_compare_exchange(dest, expected, desired, weak,
                                  msuccess, mfailure);
#else
  switch (N) {
  case 4:
    {
      uint32_t* ptr = reinterpret_cast<uint32_t*>(dest);
      uint32_t* exp = reinterpret_cast<uint32_t*>(expected);
      uint32_t newval = *reinterpret_cast<uint32_t const*>(desired);
      uint32_t oldval = *exp;

      *exp = __sync_val_compare_and_swap(ptr, oldval, newval);
      ret = (*exp == oldval);
    }
    break;
  case 8:
    {
      uint64_t* ptr = reinterpret_cast<uint64_t*>(dest);
      uint64_t* exp = reinterpret_cast<uint64_t*>(expected);
      uint64_t newval = *reinterpret_cast<uint64_t const*>(desired);
      uint64_t oldval = *exp;

      *exp = __sync_val_compare_and_swap(ptr, oldval, newval);
      ret = (*exp == oldval);
    }
    break;
#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
  case 16:
    {
      unsigned __int128* ptr = reinterpret_cast<unsigned __int128*>(dest);
      unsigned __int128* exp = reinterpret_cast<unsigned __int128*>(expected);
      unsigned __int128 newval =
        *reinterpret_cast<unsigned __int128 const*>(desired);
      unsigned __int128 oldval = *exp;

      *exp = __sync_val_compare_and_swap(ptr, oldval, newval);
      ret = (*exp == oldval);
    }
    break;
#endif
  default:
    MARE_UNREACHABLE("unsupported size for atomicops");
    break;
  }
  MARE_UNUSED(weak);
  MARE_UNUSED(msuccess);
  MARE_UNUSED(mfailure);
#endif

  MARE_UNUSED(dest);
  MARE_UNUSED(expected);
  return ret;
}

static inline void
mare_atomic_thread_fence(std::memory_order m)
{
#if !__clang__ && __GNUC__ == 4 && __GNUC_MINOR__ < 7
  __sync_synchronize();
  MARE_UNUSED(m);
#else
  __atomic_thread_fence(m);
#endif
}

};
};
