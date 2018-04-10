// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <atomic>
#include <mare/internal/util/debug.hh>

#define MARE_HAS_ATOMIC_DMWORD 1

namespace mare {
namespace internal {

typedef ULONGLONG mare_dmword_t;

static inline bool
msvc_cmpxchg_4(uint32_t* ptr, uint32_t* exp, uint32_t des)
{
  uint32_t old;

  old = InterlockedCompareExchange(ptr, des, *exp);

  if ((*exp) == old) {
    return true;
  } else {

    *exp = old;
    return false;
  }
}

static inline bool
msvc_cmpxchg_8(uint64_t* ptr, uint64_t* exp, uint64_t des)
{
  LONGLONG old;
  LONGLONG llexp = *exp;
  LONGLONG lldes = des;
  LONGLONG *llptr = reinterpret_cast<LONGLONG *>(ptr);

  old = InterlockedCompareExchange64(llptr, lldes, llexp);

  if (llexp == old) {
    return true;
  } else {

    *exp = old;
    return false;
  }
}

#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
static inline bool
msvc_cmpxchg_16(LONGLONG* ptr, LONGLONG* exp, const LONGLONG *des)
{

  __declspec(align(16)) LONGLONG cnr[2] = { exp[0], exp[1] };

  LONGLONG des_hi = des[1];
  LONGLONG des_lo = des[0];
  uint8_t ret;

  MARE_API_ASSERT((reinterpret_cast<uintptr_t>(ptr) & 0xfull) == 0,
                  "ptr must be 16 byte aligned");

  ret = _InterlockedCompareExchange128(ptr, des_hi, des_lo, cnr);
  if (ret)
    return true;

  exp[0] = cnr[0];
  exp[1] = cnr[1];
  return false;
}
#endif

template<size_t N, typename T>
inline bool
atomicops<N,T>::cmpxchg_unchecked(T* dest, T* expected,
                                  T const* desired, bool weak,
                                  std::memory_order, std::memory_order)
{
  MARE_UNUSED(weak);

  static_assert(N == 4 || N == 8 ||
                (N == 16 && MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8),
                "unsupported size for atomicops");

  switch (N) {
  case 4:
    {
      uint32_t *ptr = reinterpret_cast<uint32_t *>(dest);
      uint32_t *exp = reinterpret_cast<uint32_t *>(expected);
      uint32_t des = *reinterpret_cast<const uint32_t *>(desired);

      return msvc_cmpxchg_4(ptr, exp, des);
    }
    break;
  case 8:
    {
      uint64_t * ptr = reinterpret_cast<uint64_t *>(dest);
      uint64_t *exp = reinterpret_cast<uint64_t *>(expected);
      uint64_t des = *reinterpret_cast<const uint64_t *>(desired);

      return msvc_cmpxchg_8(ptr, exp, des);

    }
    break;
#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
  case 16:
    {
      LONGLONG * ptr = reinterpret_cast<LONGLONG *>(dest);
      LONGLONG* exp = reinterpret_cast<LONGLONG*>(expected);
      const LONGLONG* des = reinterpret_cast<LONGLONG const*>(desired);

      return msvc_cmpxchg_16(ptr, exp, des);
    }
    break;
#endif
  }
  MARE_UNREACHABLE("Unexpected size encountered");
}

template<size_t N, typename T>
inline void
atomicops<N,T>::load_unchecked(T const* src, T* dest, std::memory_order)
{
  static_assert(N == 4 || N == 8 ||
                (N == 16 && MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8),
                "unsupported size for atomicops");

  switch (N) {
  case 4:
  case 8:
    MemoryBarrier();
    *dest = *src;
    break;
#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
  case 16:
    {
      LONGLONG* ptr =
        const_cast<LONGLONG*>(reinterpret_cast<LONGLONG const*>(src));
      LONGLONG* dst = reinterpret_cast<LONGLONG*>(dest);

      __declspec(align(16)) LONGLONG cnr[2] = {0 ,0};

      MARE_API_ASSERT((reinterpret_cast<uintptr_t>(ptr) & 0xfull) == 0,
                      "ptr must be 16 byte aligned");

      _InterlockedCompareExchange128(ptr, 0, 0, cnr);
      dst[0] = cnr[0];
      dst[1] = cnr[1];
    }
    break;
#endif
  default:
    MARE_UNREACHABLE("Unimplemented size");
    break;
  }
}

template<size_t N, typename T>
inline void
atomicops<N,T>::store_unchecked(T* dest, T const* desired, std::memory_order)
{
  static_assert(N == 4 || N == 8 ||
                (N == 16 && MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8),
                "unsupported size for atomicops");
  switch (N) {
  case 4:
  case 8:
    *dest = *desired;
    MemoryBarrier();
    break;
#if MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8
  case 16:
    {
      LONGLONG const* des = reinterpret_cast<LONGLONG const*>(desired);
      LONGLONG *ptr = reinterpret_cast<LONGLONG *>(dest);

      for (;;) {
        LONGLONG exp[2] = { ptr[0], ptr[1] };
        if (msvc_cmpxchg_16(ptr, exp, des))
          return;
      }
    }
    break;
#endif
  default:
    MARE_UNREACHABLE("Unimplemented size");
    break;
  }
}

static inline void
mare_atomic_thread_fence(std::memory_order m)
{
  std::atomic_thread_fence(m);
}

};
};
