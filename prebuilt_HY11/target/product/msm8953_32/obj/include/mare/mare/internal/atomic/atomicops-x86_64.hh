// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once
#include <cstdint>
#include <climits>
#include <mare/internal/util/debug.hh>

namespace mare {

namespace internal {

template<size_t N, typename T>
struct atomicops {
  static inline bool cmpxchg(T* dest, T* expected, T const* desired, bool weak,
                             std::memory_order const& msuccess,
                             std::memory_order const& mfailure);
  static inline void load(T const* src, T* dest, std::memory_order m);
  static inline void store(T* dest, T const* desired, std::memory_order m);
};

template<typename T>
struct atomicops<4,T> {
  static inline uint32_t* U32(T* p) {
    return reinterpret_cast<uint32_t*>(p);
  }
  static inline uint32_t const* U32(T const* p) {
    return reinterpret_cast<uint32_t const*>(p);
  }
  static inline bool cmpxchg(T* dest, T* expected, T const* desired, bool weak,
                             std::memory_order msuccess,
                             std::memory_order mfailure) {
    MARE_INTERNAL_ASSERT(msuccess==std::memory_order_seq_cst,
                         "Unimplemented memory_order");
    MARE_INTERNAL_ASSERT(mfailure==std::memory_order_seq_cst,
                         "Unimplemented memory_order");
    MARE_UNUSED(msuccess);
    MARE_UNUSED(mfailure);
    MARE_UNUSED(weak);
    unsigned char changed;
    __asm__ __volatile__ ("lock; cmpxchg %[desired],%[dest];\n\t"
                          "sete %[changed];"
                          : [changed] "=q" (changed), [dest] "+m" (*U32(dest)),
                            "=a" (*U32(expected))
                          : "a" (*U32(expected)), [desired] "r" (*U32(desired))
                          : "cc", "memory");
    return changed;
  }
  static inline void load(T const* src, T* dest, std::memory_order m) {
    switch(m) {
    case std::memory_order_relaxed:
      *dest = *src;
      break;
    case std::memory_order_consume:
    case std::memory_order_acquire:
    case std::memory_order_release:
    case std::memory_order_acq_rel:
    case std::memory_order_seq_cst:
      asm volatile("mfence" ::: "memory");
      *dest = *src;
      break;
    default:
      MARE_UNREACHABLE("Unimplemented memory_order");
    }
  }
  static inline void store(T* dest, T const* desired, std::memory_order m) {
    switch(m) {
    case std::memory_order_relaxed:
      *dest = *desired;
      break;
    case std::memory_order_consume:
    case std::memory_order_acquire:
    case std::memory_order_release:
    case std::memory_order_acq_rel:
    case std::memory_order_seq_cst:
      *dest = *desired;
      asm volatile("mfence" ::: "memory");
      break;
    default:
      MARE_UNREACHABLE("Unimplemented memory_order");
    }
  }
};

template<typename T>
struct atomicops<8,T> {
  static inline uint64_t* U64(T* p) {
    return reinterpret_cast<uint64_t*>(p);
  }
  static inline uint64_t const* U64(T const* p) {
    return reinterpret_cast<uint64_t const*>(p);
  }
  static inline bool cmpxchg(T* dest, T* expected, T const* desired, bool weak,
                             std::memory_order const& msuccess,
                             std::memory_order const& mfailure) {
    MARE_INTERNAL_ASSERT(msuccess==std::memory_order_seq_cst,
                         "Unimplemented memory_order");
    MARE_INTERNAL_ASSERT(mfailure==std::memory_order_seq_cst,
                         "Unimplemented memory_order");
    MARE_UNUSED(msuccess);
    MARE_UNUSED(mfailure);
    MARE_UNUSED(weak);
    unsigned char changed;
    __asm__ __volatile__ ("lock; cmpxchgq %[desired],%[dest];\n\t"
                          "sete %[changed];"
                          : [changed] "=q" (changed), [dest] "+m" (*U64(dest)),
                            "=a" (*U64(expected))
                          : "a" (*U64(expected)), [desired] "r" (*U64(desired))
                          : "cc", "memory");
    return changed;
  }
  static inline void load(T const* src, T* dest, std::memory_order const& m) {
    switch(m) {
    case std::memory_order_relaxed:
      *dest = *src;
      break;
    case std::memory_order_consume:
    case std::memory_order_acquire:
    case std::memory_order_release:
    case std::memory_order_acq_rel:
    case std::memory_order_seq_cst:
      asm volatile("mfence" ::: "memory");
      *dest = *src;
      break;
    default:
      MARE_UNREACHABLE("Unimplemented memory_order");
    }
  }
  static inline void store(T* dest, T const* desired,
                           std::memory_order const& m) {
    switch(m) {
    case std::memory_order_relaxed:
      *dest = *desired;
      break;
    case std::memory_order_consume:
    case std::memory_order_acquire:
    case std::memory_order_release:
    case std::memory_order_acq_rel:
    case std::memory_order_seq_cst:
      *dest = *desired;
      asm volatile("mfence" ::: "memory");
      break;
    default:
      MARE_UNREACHABLE("Unimplemented memory_order");
    }
  }
};

};

};
