// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <atomic>

#include <mare/internal/atomic/atomicops.hh>
#include <mare/internal/util/macros.hh>

MARE_GCC_IGNORE_BEGIN("-Weffc++");

namespace mare {

namespace internal {

template <size_t N, typename T>
struct alignedatomic_n {
private:
#ifdef _MSC_VER
  static_assert(sizeof(N) <= 16, "Alignment is hard coded for MSVC");

  MARE_ALIGNED (T, _v, 16);
#else
  MARE_ALIGNED (T, _v, N);
#endif
public:
  alignedatomic_n() : _v() {};
  MARE_DEFAULT_METHOD(~alignedatomic_n());

  explicit alignedatomic_n(T v) : _v(v) {}

  explicit operator T() const { return load(); }

  T load(std::memory_order m = std::memory_order_seq_cst) const {
    T tmp;
    internal::atomicops<N,T>::load(&_v, &tmp, m);
    return tmp;
  }

  T loadNA() const {
    return _v;
  }

  bool compare_exchange_strong(
      T& e, T n,
      std::memory_order const& m = std::memory_order_seq_cst) {
    return internal::atomicops<N,T>::cmpxchg(&_v, &e, &n, false, m, m);
  }

  bool compare_exchange_weak(
      T& e, T n,
      std::memory_order const& m = std::memory_order_seq_cst) {
    return internal::atomicops<N,T>::cmpxchg(&_v, &e, &n, true, m, m);
  }

  void store(T const& n,
             std::memory_order const& m = std::memory_order_seq_cst) {
    internal::atomicops<N,T>::store(&_v, &n, m);
  }

  void storeNA(T const& n) {
    _v = n;
  }
};

#if 0

template <typename T>
struct alignedatomic_n<16,T> : public std::atomic<T> {
  MARE_DEFAULT_METHOD(alignedatomic_n());
  MARE_DEFAULT_METHOD(~alignedatomic_n());

  T loadNA() const {
    return this->load(std::memory_order_relaxed);
  }

  void storeNA(T const& other) {
    this->store(other, std::memory_order_relaxed);
  }
} __attribute__ ((aligned(16)));
#endif

template <typename T>
struct alignedatomic : public alignedatomic_n<sizeof(T),T> {
  MARE_DEFAULT_METHOD(alignedatomic());
  MARE_DEFAULT_METHOD(~alignedatomic());
};

};

};

MARE_GCC_IGNORE_END("-Weffc++");
