// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <cstdint>
#include <climits>

#define MARE_HAS_ATOMIC_DMWORD 1

namespace mare {

namespace internal {

#ifdef __x86_64__
#if defined(__clang__) && __SIZEOF_INT128__ < 16
typedef __uint128_t mare_dmword_t;
#else
typedef unsigned __int128 mare_dmword_t;
#endif
#else
typedef uint64_t mare_dmword_t;
#endif

static inline void x86_mfence()
{
  asm volatile("## x86_mfence \n\t"
               "mfence" ::: "memory");
}

static inline bool
x86_cmpxchg_4(uint32_t* ptr, uint32_t* exp, uint32_t des)
{
  uint8_t changed;

  asm volatile ("## x86_cmpxchg_4 \n\t"
                "lock; cmpxchgl %[des],%[ptr]\n\t"
                "sete %[changed];"
                : [changed] "=q" (changed),
                  [ptr] "+m" (*ptr),
                    "=a" (*exp)
                : "a" (*exp),
                  [des] "r" (des)
                : "cc", "memory");
  return (changed != 0);
}

static inline bool
x86_cmpxchg_8(uint64_t* ptr, uint64_t* exp, uint64_t des)
{
  uint8_t changed;

#ifdef __x86_64__
  asm volatile ("## x86_cmpxchg_8 \n\t"
                "lock; cmpxchgq %[des],%[ptr]\n\t"
                "sete %[changed];"
                : [changed] "=q" (changed),
                  [ptr] "+m" (*ptr),
                    "=a" (*exp)
                : "a" (*exp),
                  [des] "r" (des)
                : "cc", "memory");
#else
  uint64_t prev = *exp;
  uint32_t des_lo = des;
  uint32_t des_hi = des >> 32;

  asm volatile ("## x86_cmpxchg_8 \n\t"
                "lock; cmpxchg8b %[ptr]\n\t"
                "sete %[changed];"
                : [changed] "=q" (changed),
                  [ptr] "+m" (*ptr),
                  "+A" (prev)
                : "b" (des_lo),
                  "c" (des_hi)
                : "cc", "memory");
  if (!changed)
    *exp = prev;
#endif

  return (changed != 0);
}

#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
static inline bool
x86_cmpxchg_16(mare_dmword_t* ptr, mare_dmword_t* exp,
               mare_dmword_t des)
{
  uint8_t changed;
  uint64_t des_lo = des;
  uint64_t des_hi = des >> 64;

  mare_dmword_t prev = *exp;

  uint64_t prev_lo = prev;
  uint64_t prev_hi = prev >> 64;

  asm volatile ("## x86_cmpxchg_16\n\t"
                "lock; cmpxchg16b %[ptr]\n\t"
                "sete %[changed];"
                : [changed] "=q" (changed),
                  [ptr] "+m" (*ptr),
                  "+a" (prev_lo),
                  "+d" (prev_hi)
                : "b" (des_lo),
                  "c" (des_hi)
                : "cc", "memory");
  if (changed)
    return true;
  prev = prev_hi;
  prev <<= 64;
  prev |= prev_lo;
  *exp = prev;
  return false;
}

#endif

template<size_t N, typename T>
inline bool
atomicops<N,T>::cmpxchg_unchecked(T* dest, T* expected,
                             T const* desired, bool weak,
                             std::memory_order msuccess,
                             std::memory_order mfailure)
{
  bool changed;

  static_assert(N == 4 || N == 8 ||
                (N == 16 && MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8),
                "unsupported size for atomicops");

  MARE_UNUSED(weak);
  MARE_UNUSED(msuccess);
  MARE_UNUSED(mfailure);

  switch (N) {
  case 4:
    {
      uint32_t* ptr = reinterpret_cast<uint32_t*>(dest);
      uint32_t* exp = reinterpret_cast<uint32_t*>(expected);
      uint32_t des = *reinterpret_cast<uint32_t const*>(desired);

      changed = x86_cmpxchg_4(ptr, exp, des);
    }
    break;
  case 8:
    {
      uint64_t* ptr = reinterpret_cast<uint64_t*>(dest);
      uint64_t* exp = reinterpret_cast<uint64_t*>(expected);
      uint64_t des = *reinterpret_cast<uint64_t const*>(desired);

      changed = x86_cmpxchg_8(ptr, exp, des);
    }
    break;
#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
  case 16:
    {
      mare_dmword_t* ptr = reinterpret_cast<mare_dmword_t*>(dest);
      mare_dmword_t* exp = reinterpret_cast<mare_dmword_t*>(expected);
      mare_dmword_t des =
        *reinterpret_cast <mare_dmword_t const*>(desired);

      changed = x86_cmpxchg_16(ptr, exp, des);
    }
    break;
#endif

  default:
    MARE_UNREACHABLE("Unimplemented memory size");
  }
  return changed;
}

template<size_t N, typename T>
inline void
atomicops<N,T>::load_unchecked(T const* src, T* dest, std::memory_order m)
{

  static_assert(N == 4 || N == 8 ||
                (N == 16 && MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8),
                "unsupported size for atomicops");

  switch (N) {
  case 4:
  case 8:

    if (m == std::memory_order_consume ||
        m == std::memory_order_acquire ||
        m == std::memory_order_seq_cst)
      x86_mfence();

    *dest = *src;
    break;
#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
  case 16:
    {
      mare_dmword_t* ptr =
        const_cast<mare_dmword_t*>(
            reinterpret_cast<mare_dmword_t const*>(src));
      mare_dmword_t* exp = reinterpret_cast<mare_dmword_t*>(dest);

      asm volatile ("## load_unchecked_16");
      x86_cmpxchg_16(ptr, exp, *exp);
    }
    break;
#endif
  default:
    MARE_UNREACHABLE("unsupported size for atomicops");
    break;
  }
}

template<size_t N, typename T>
inline void
atomicops<N,T>::store_unchecked(T* dest, T const* desired, std::memory_order m)
{
  static_assert(N == 4 || N == 8 ||
                (N == 16 && MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8),
                "unsupported size for atomicops");

  switch (N) {
  case 4:
  case 8:
    *dest = *desired;

    if (m == std::memory_order_release ||
        m == std::memory_order_seq_cst)
      x86_mfence();
    break;
#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
  case 16:
    {
      mare_dmword_t des =
        *reinterpret_cast<mare_dmword_t const*>(desired);
      mare_dmword_t* ptr = reinterpret_cast<mare_dmword_t*>(dest);

      for (;;) {
        mare_dmword_t exp = *ptr;
        asm volatile ("## store_unchecked_16 inner loop");
        if (x86_cmpxchg_16(ptr, &exp, des))
          return;
        asm volatile ("" ::: "memory");
      }
    }
    break;
#endif
  default:
    MARE_UNREACHABLE("unsupported size for atomicops");
    break;
  }
}

static inline void
mare_atomic_thread_fence(std::memory_order m)
{
  if (m == std::memory_order_seq_cst)
    x86_mfence();
}

};

};
