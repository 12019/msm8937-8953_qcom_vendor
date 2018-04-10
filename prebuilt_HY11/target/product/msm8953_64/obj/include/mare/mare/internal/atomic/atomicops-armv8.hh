// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once
#include <cstdint>
#include <climits>

#ifdef __thumb__
#define MARE_ARM_IF_THEN(c) "it " # c
#else
#define MARE_ARM_IF_THEN(c)
#endif

namespace mare {

namespace internal {

#define MARE_HAS_ATOMIC_DMWORD 1
#if MARE_SIZEOF_MWORD == 8
  typedef unsigned __int128 mare_dmword_t;
#else
  typedef uint64_t mare_dmword_t;
#endif

static inline void armv8a_inner_realm_barrier()
{

  asm volatile ("dmb ish":::"memory");
}

static inline bool
armv8a_cmpxchg_weak_4(uint32_t* ptr, uint32_t exp, uint32_t des,
                      uint32_t* old)
{
  uint32_t tmp;
  uint32_t failed_write;

  asm volatile ("// armv8a_cmpxchg_weak_4 \n\t"
                "   mov  %w[fw], #1 \n\t"
                "1: # loop \n"
                "        ldxr %w[tmp], [%[ptr]] \n\t"
                "        cmp  %w[tmp], %w[exp] \n\t"
                "        bne 2f\n"
                "        stxr %w[fw], %w[des], [%[ptr]] \n"
                "        cbnz %w[fw], 1b\n\t"
                "2: # out \n"
                : [tmp] "=&r" (tmp),
                  [fw] "=&r" (failed_write),
                  "=Qo" (*ptr)
                : [ptr] "r" (ptr),
                  [exp] "r" (exp),
                  [des] "r" (des)
                : "cc", "memory");

  if (failed_write) {
    *old = tmp;
    return false;
  }
  return true;
}

static inline bool
armv8a_cmpxchg_strong_4(uint32_t* ptr, uint32_t exp, uint32_t des,
                        uint32_t* old)
{
  uint32_t tmp = 0;
  uint32_t failed_write;
  asm volatile ("// armv8a_cmpxchg_strong_4 \n"
                "1: # loop \n"
                "        ldxr %w[tmp], [%[ptr]] \n"
                "        subs %w[fw], %w[tmp], %w[exp] \n"
                "        bne  2f \n"
                "        stxr %w[fw], %w[des], [%[ptr]] \n"
                "        cbnz  %w[fw], 1b\n\t"
                "2: # out \n"
                : [tmp] "=&r" (tmp),
                  [fw] "=&r" (failed_write),
                  "=Qo" (*ptr)
                : [ptr] "r" (ptr),
                  [exp] "r" (exp),
                  [des] "r" (des)
                : "cc", "memory");

  if (failed_write) {
    *old = tmp;
    return false;
  }
  return true;
}

static inline bool
armv8a_cmpxchg_4(uint32_t* ptr, uint32_t* expected,
                uint32_t const* desired,
                bool weak,
                std::memory_order ms,
                std::memory_order)
{
  uint32_t exp = *expected;
  uint32_t des = *desired;
  uint32_t old = 0;
  bool done;

  if (weak) {
    done = armv8a_cmpxchg_weak_4(ptr, exp, des, &old);
  } else {
    done = armv8a_cmpxchg_strong_4(ptr, exp, des, &old);
  }

  if (ms == std::memory_order_seq_cst ||
      ms == std::memory_order_acquire ||
      ms == std::memory_order_acq_rel)
    armv8a_inner_realm_barrier();

  if (!done)
    *expected = old;
  return done;
}

static inline bool
armv8a_cmpxchg_weak_8(uint64_t* ptr, uint64_t exp, uint64_t des,
                      uint64_t* old)
{
  uint64_t tmp;
  uint32_t failed_write;

  MARE_CLANG_IGNORE_BEGIN("-Wasm-operand-widths");
  asm volatile ("//  armv8a_cmpxchg_weak_8 \n\t"
                "1: # loop \n\t"
                "        ldxr %[tmp], [%[ptr]] \n\t"
                "        cmp  %[tmp], %[exp] \n\t"
                "        bne  2f\n\t"
                "        stxr %w[fw], %[des], [%[ptr]] \n\t"
                "        cbnz %[fw], 1b\n\t"
                "2: # out \n\t"
                : [tmp] "=&r" (tmp),
                  [fw] "=&r" (failed_write),
                  "=Qo" (*ptr)
                : [ptr] "r" (ptr),
                  [exp] "r" (exp),
                  [des] "r" (des)
                : "cc", "memory");
  MARE_CLANG_IGNORE_END("-Wasm-operand-widths");

  if (failed_write) {
    *old = tmp;
    return false;
  }
  return true;
}

static inline bool
armv8a_cmpxchg_strong_8(uint64_t* ptr, uint64_t exp, uint64_t des,
                        uint64_t* old)
{
  uint64_t tmp;
  size_t failed_write;

  MARE_CLANG_IGNORE_BEGIN("-Wasm-operand-widths");

  asm volatile ("// armv8a_cmpxchg_strong_8 \n"

                "        mov %[fw], #1 \n"
                "1: #loop \n"
                "       ldxr %[tmp], [%[ptr]] \n"
                "       cmp  %[tmp], %[exp] \n"
                "       bne  2f \n"
                "       stxr %w[fw], %[des], [%[ptr]] \n"
                "       cbnz  %w[fw], 1b \n"
                "2: # out \n"
                : [tmp] "=&r" (tmp),
                  [fw] "=&r" (failed_write),
                  "=Qo" (*ptr)
                : [ptr] "r" (ptr),
                  [exp] "r" (exp),
                  [des] "r" (des)
                : "cc", "memory");

  MARE_CLANG_IGNORE_END("-Wasm-operand-widths");

  if (failed_write) {
    *old = tmp;
    return false;
  }
  return true;
}

static inline bool
armv8a_cmpxchg_8(uint64_t* ptr, uint64_t* expected,
                 uint64_t const* desired,
                 bool weak,
                 std::memory_order ms,
                 std::memory_order)
{
  uint64_t exp = *expected;
  uint64_t des = *desired;
  uint64_t old = exp;
  bool done;

  if (weak) {
    done = armv8a_cmpxchg_weak_8(ptr, exp, des, &old);
  } else {
    done = armv8a_cmpxchg_strong_8(ptr, exp, des, &old);
  }

  if (ms == std::memory_order_seq_cst ||
      ms == std::memory_order_acquire ||
      ms == std::memory_order_acq_rel)
    armv8a_inner_realm_barrier();

  if (!done)
    *expected = old;
  return done;
}

static inline bool
armv8a_cmpxchg_weak_16(mare_dmword_t* ptr, mare_dmword_t exp, mare_dmword_t des,
                       mare_dmword_t* old)
{
  mare_dmword_t tmp;
  uint32_t failed_write;

  MARE_CLANG_IGNORE_BEGIN("-Wasm-operand-widths");
  asm volatile ("//  armv8a_cmpxchg_weak_16 \n\t"
                "1: #loop \n"
                "ldxp %[tmp], %H[tmp], [%[ptr]] \n\t"
                "cmp  %[tmp], %[exp] \n\t"
                "ccmp %H[tmp], %H[exp], #0, eq \n\t"
                "bne  2f\n\t"
                "stxp %w[fw], %[des], %H[des], [%[ptr]] \n\t"
                "cbnz %w[fw], 1b\n\t"
                "2:\n\t"
                : [tmp] "=&r" (tmp),
                  [fw] "=&r" (failed_write),
                  "=Qo" (*ptr)
                : [ptr] "r" (ptr),
                  [exp] "r" (exp),
                  [des] "r" (des)
                : "cc", "memory");
  MARE_CLANG_IGNORE_END("-Wasm-operand-widths");

  if (failed_write) {
    *old = tmp;
    return false;
  }
  return true;
}

static inline bool
armv8a_cmpxchg_strong_16(mare_dmword_t* ptr, mare_dmword_t exp, mare_dmword_t des,
                        mare_dmword_t* old)
{
  mare_dmword_t tmp;
  size_t failed_write;

  MARE_CLANG_IGNORE_BEGIN("-Wasm-operand-widths");

  asm volatile ("// armv8a_cmpxchg_strong_16 \n"

                "        mov   %[fw], #1 \n"
                "1: #loop \n"
                "        ldxp %[tmp], %H[tmp], [%[ptr]] \n"
                "        cmp  %[tmp], %[exp] \n"
                "        ccmp %H[tmp], %H[exp], #0, eq \n\t"
                "        bne  2f \n"
                "        stxp %w[fw], %[des], %H[des], [%[ptr]] \n"
                "        cmp  %w[fw], #0 \n"
                "        bne  1b \n"
                "2: # exit \n"
                : [tmp] "=&r" (tmp),
                  [fw] "=&r" (failed_write),
                  "=Qo" (*ptr)
                : [ptr] "r" (ptr),
                  [exp] "r" (exp),
                  [des] "r" (des)
                : "cc", "memory");

  MARE_CLANG_IGNORE_END("-Wasm-operand-widths");

  if (failed_write) {
    *old = tmp;
    return false;
  }
  return true;
}

static inline bool
armv8a_cmpxchg_16(mare_dmword_t* ptr, mare_dmword_t* expected,
                 mare_dmword_t const* desired,
                 bool weak,
                 std::memory_order ms,
                 std::memory_order)
{
  mare_dmword_t exp = *expected;
  mare_dmword_t des = *desired;
  mare_dmword_t old = exp;
  bool done;

  if (weak) {
    done = armv8a_cmpxchg_weak_16(ptr, exp, des, &old);
  } else {
    done = armv8a_cmpxchg_strong_16(ptr, exp, des, &old);
  }

  if (ms == std::memory_order_seq_cst ||
      ms == std::memory_order_acquire ||
      ms == std::memory_order_acq_rel)
    armv8a_inner_realm_barrier();

  if (!done)
    *expected = old;
  return done;
}

template<size_t N, typename T>
inline bool
atomicops<N,T>::cmpxchg_unchecked(T* dest, T* expected,
                                  T const* desired, bool weak,
                                  std::memory_order msuccess,
                                  std::memory_order mfailure)
{

  static_assert(N == 4 || N == 8 || (N == 16 && MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8),
                "atomicops only supports 4, 8 or 16 byte");

  if (msuccess == std::memory_order_seq_cst ||
      msuccess == std::memory_order_release)
    armv8a_inner_realm_barrier();

  switch (N) {
  case 4:
    {
      uint32_t* ptr = reinterpret_cast<uint32_t*>(dest);
      uint32_t* exp = reinterpret_cast<uint32_t*>(expected);
      uint32_t const* des = reinterpret_cast<uint32_t const*>(desired);

      return armv8a_cmpxchg_4(ptr, exp, des, weak, msuccess, mfailure);
    }
    break;
  case 8:
    {
      uint64_t* ptr = reinterpret_cast<uint64_t*>(dest);
      uint64_t* exp = reinterpret_cast<uint64_t*>(expected);
      uint64_t const* des = reinterpret_cast<uint64_t const *>(desired);

      return armv8a_cmpxchg_8(ptr, exp, des, weak, msuccess, mfailure);
    }
    break;
  case 16:
    {
      mare_dmword_t* ptr = reinterpret_cast<mare_dmword_t*>(dest);
      mare_dmword_t* exp = reinterpret_cast<mare_dmword_t*>(expected);
      mare_dmword_t const* des = reinterpret_cast<mare_dmword_t const*>(desired);

      return armv8a_cmpxchg_16(ptr, exp, des, weak, msuccess, mfailure);
    }
    break;
  default:
    MARE_UNREACHABLE("Unimplemented size");
  }
}

static inline void
armv8a_load_4(uint32_t const*ptr, uint32_t *ret, std::memory_order m)
{
  uint32_t tmp;

  if (m == std::memory_order_seq_cst ||
      m == std::memory_order_acquire ||
      m == std::memory_order_consume)
    armv8a_inner_realm_barrier();

  asm volatile ("ldr %[tmp], [%[src]]"
                : [tmp] "=&r"(tmp)
                : [src] "r" (ptr));

  if (m == std::memory_order_seq_cst)
    armv8a_inner_realm_barrier();

  *ret = tmp;
}

static inline void
armv8a_load_8(uint64_t const* ptr, uint64_t* ret, std::memory_order m)
{
  uint64_t tmp;

  if (m == std::memory_order_seq_cst ||
      m == std::memory_order_acquire ||
      m == std::memory_order_consume)
    armv8a_inner_realm_barrier();

  asm volatile ("ldr %[tmp], [%[src]]"
                : [tmp] "=&r"(tmp)
                : [src] "r" (ptr));

  if (m == std::memory_order_seq_cst)
    armv8a_inner_realm_barrier();

  *ret = tmp;
}

static inline void
armv8a_load_16(mare_dmword_t const* ptr, mare_dmword_t* ret, std::memory_order m)
{
  mare_dmword_t tmp;

  if (m == std::memory_order_seq_cst ||
      m == std::memory_order_acquire ||
      m == std::memory_order_consume)
    armv8a_inner_realm_barrier();

  asm volatile ("ldp %[tmp], %H[tmp], [%[src]]"
                : [tmp] "=&r"(tmp)
                : [src] "r" (ptr));

  if (m == std::memory_order_seq_cst)
    armv8a_inner_realm_barrier();

  *ret = tmp;
}

template<size_t N, typename T>
inline void
atomicops<N,T>::load_unchecked(T const* src, T* dest, std::memory_order m)
{
  static_assert(N == 4 || N == 8 || (N == 16 && MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8),
                "atomicops only supports 4, 8 or 16 byte");

  switch (N) {
  case 4:
    {
      uint32_t const*ptr = reinterpret_cast<uint32_t const*>(src);
      uint32_t *ret = reinterpret_cast<uint32_t*>(dest);

      armv8a_load_4(ptr, ret, m);
    }
    break;
  case 8:
    {
      uint64_t const*ptr = reinterpret_cast<uint64_t const*>(src);
      uint64_t *ret = reinterpret_cast<uint64_t*>(dest);
      armv8a_load_8(ptr, ret, m);
    }
    break;
  case 16:
    {
      mare_dmword_t const* ptr = reinterpret_cast<mare_dmword_t const*>(src);
      mare_dmword_t * ret = reinterpret_cast<mare_dmword_t*>(dest);
      armv8a_load_16(ptr, ret, m);
    }
    break;
  default:
    MARE_UNREACHABLE("Unimplemented size");
  }
}

static inline void
armv8a_store_4(uint32_t* ptr, uint32_t const* val, std::memory_order m)
{
  uint32_t v = *val;

  if (m == std::memory_order_seq_cst || m == std::memory_order_release)
    armv8a_inner_realm_barrier();

  *ptr = v;

  if (m == std::memory_order_seq_cst)
    armv8a_inner_realm_barrier();

}

static inline void
armv8a_store_8(uint64_t* ptr, uint64_t const* val, std::memory_order m)
{
  uint64_t v = *val;

  if (m == std::memory_order_seq_cst || m == std::memory_order_release)
    armv8a_inner_realm_barrier();

  *ptr = v;

  if (m == std::memory_order_seq_cst) {
    armv8a_inner_realm_barrier();
  }
}

static inline void
armv8a_store_16(mare_dmword_t* ptr, mare_dmword_t const* val, std::memory_order m)
{
  mare_dmword_t tmp;
  mare_dmword_t v = *val;
  uint32_t failed_write;

  if (m == std::memory_order_seq_cst || m == std::memory_order_release)
    armv8a_inner_realm_barrier();

  MARE_CLANG_IGNORE_BEGIN("-Wasm-operand-widths");

  asm volatile ("1: # loop \n"
                "        ldxp %[tmp], %H[tmp], [%[dst]] \n"
                "        stxp %w[fw], %[v], %H[v], [%[dst]] \n"
                "        cbnz    %[fw], 1b\n\t"
                : [tmp] "=&r"(tmp),
                  [fw] "=&r" (failed_write),
                  "=Qo" (*ptr)
                : [dst] "r" (ptr),
                  [v] "r" (v)
                : "cc");
  MARE_CLANG_IGNORE_END("-Wasm-operand-widths");

  if (m == std::memory_order_seq_cst) {
    armv8a_inner_realm_barrier();
  }
}

template<size_t N, typename T>
inline void
atomicops<N,T>::store_unchecked(T* dest, T const* desired, std::memory_order m)
{
  static_assert(N == 4 || N == 8|| (N == 16 && MARE_HAS_ATOMIC_DMWORD && MARE_SIZEOF_MWORD == 8),
                "atomicops only supports 4, 8 or 16 byte");

  switch (N) {
  case 4:
    {
      uint32_t* ptr = reinterpret_cast<uint32_t*>(dest);
      uint32_t const* val = reinterpret_cast<uint32_t const*>(desired);

      armv8a_store_4(ptr, val, m);
    }
    break;
  case 8:
    {
      uint64_t* ptr = reinterpret_cast<uint64_t*>(dest);
      uint64_t const* val = reinterpret_cast<uint64_t const*>(desired);

      armv8a_store_8(ptr, val, m);
    }
    break;
  case 16:
    {
      mare_dmword_t * ptr = reinterpret_cast<mare_dmword_t*>(dest);
      mare_dmword_t const* val = reinterpret_cast<mare_dmword_t const*>(desired);

      armv8a_store_16(ptr, val, m);
    }
    break;
  default:
    MARE_UNREACHABLE("Unimplemented size");
  }

}

static inline void
mare_atomic_thread_fence(std::memory_order m)
{
  if (m != std::memory_order_relaxed)
    armv8a_inner_realm_barrier();
}

};
};
