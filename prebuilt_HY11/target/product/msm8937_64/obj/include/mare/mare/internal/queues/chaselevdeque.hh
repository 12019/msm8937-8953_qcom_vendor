// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <atomic>
#include <mutex>

#include <mare/internal/atomic/atomicops.hh>
#include <mare/internal/util/debug.hh>
#include <mare/internal/util/macros.hh>
#include <mare/internal/util/memorder.hh>

#define MARE_CLD_SERIALIZE 0

#define MARE_CLD_FORCE_SEQ_CST 0

#if MARE_CLD_SERIALIZE
#define MARE_CLD_LOCK std::lock_guard<std::mutex> lock(_serializing_mutex)
#else
#define MARE_CLD_LOCK do { } while(0)
#endif

#if MARE_CLD_FORCE_SEQ_CST
#define MARE_CLD_MO(x) mare::mem_order_seq_cst
#else
#define MARE_CLD_MO(x) x
#endif

template <typename T>
class chase_lev_array {
public:
  explicit chase_lev_array(unsigned logsz = 12) :
    _logsz(logsz),
    _arr(new std::atomic<T>[size_t(1) << logsz]),
    _next(nullptr){}

  ~chase_lev_array() { delete[] _arr; }

  explicit chase_lev_array(T&) { }

  size_t size() const {
    return size_t(1) << _logsz;
  }

  T get(size_t i,
        mare::mem_order mo = MARE_CLD_MO(mare::mem_order_relaxed)) {
    return _arr[i & (size() - 1)].load(mo);
  }

  void put(size_t i, T x,
           mare::mem_order mo = MARE_CLD_MO(mare::mem_order_relaxed)) {
    _arr[i & (size() - 1)].store(x, mo);
  }

  chase_lev_array<T> *resize(size_t bot, size_t top) {
    auto a = new chase_lev_array<T>(_logsz + 1);

    MARE_INTERNAL_ASSERT(top <= bot, "oops");

    for (size_t i = top; i < bot; ++i)
      a->put(i, get(i));
    return a;
  }

  void set_next(chase_lev_array<T>* next){_next = next;}
  chase_lev_array<T>* get_next(){ return _next;}
private:
  unsigned _logsz;
  std::atomic<T>* _arr;

  chase_lev_array<T>* _next;

  chase_lev_array<T> & operator=(const chase_lev_array<T>&);
  chase_lev_array<T>(const chase_lev_array<T>&);
};

template <typename T, bool Lepop = true>
class chase_lev_deque {
public:

  static constexpr size_t s_abort = ~size_t(0);

  explicit chase_lev_deque(unsigned logsz = 12):
    _top(1),
    _bottom(1),
    _cl_array(new chase_lev_array<T>(logsz)),
    _serializing_mutex(),
    _original_head(_cl_array){}

  virtual ~chase_lev_deque() {

    MARE_INTERNAL_ASSERT(_original_head != nullptr, "Error. Head is nullptr");
    while(_original_head != nullptr){
      auto cur = _original_head;
      _original_head = cur->get_next();
      delete cur;
    }
  }

  MARE_DELETE_METHOD(chase_lev_deque(chase_lev_deque const&));
  MARE_DELETE_METHOD(chase_lev_deque(chase_lev_deque &&));
  MARE_DELETE_METHOD(chase_lev_deque& operator=(chase_lev_deque const&));
  MARE_DELETE_METHOD(chase_lev_deque& operator=(chase_lev_deque &&));

  size_t take(T& x) {
    MARE_CLD_LOCK;

    size_t b = _bottom.load(MARE_CLD_MO(mare::mem_order_relaxed));

    if (b == 0)
      return 0;

    b = _bottom.load(MARE_CLD_MO(mare::mem_order_relaxed));
    auto a = _cl_array.load(MARE_CLD_MO(mare::mem_order_relaxed));

    --b;

    _bottom.store(b, MARE_CLD_MO(mare::mem_order_relaxed));
    mare::internal::mare_atomic_thread_fence(
        MARE_CLD_MO(mare::mem_order_seq_cst));

    size_t t = _top.load(MARE_CLD_MO(mare::mem_order_relaxed));

    if (b < t) {

      if (Lepop)
        _bottom.store(b + 1, MARE_CLD_MO(mare::mem_order_relaxed));
      else
        _bottom.store(t, MARE_CLD_MO(mare::mem_order_relaxed));
      return 0;
    }

    auto current_x = x;
    x = a->get(b);
    if (b > t) {

      return b - t;
    }

    size_t sz = 1;

    if (!_top.compare_exchange_strong(
            t, t + 1,
            MARE_CLD_MO(mare::mem_order_seq_cst),
            MARE_CLD_MO(mare::mem_order_relaxed))) {

      x = current_x;
      sz = 0;
    }

    if (Lepop)
      _bottom.store(b + 1, MARE_CLD_MO(mare::mem_order_relaxed));
    else
      _bottom.store(t + 1, MARE_CLD_MO(mare::mem_order_relaxed));

    return sz;
  }

  size_t push(T x) {
    MARE_CLD_LOCK;
    size_t b = _bottom.load(MARE_CLD_MO(mare::mem_order_relaxed));
    size_t t = _top.load(MARE_CLD_MO(mare::mem_order_acquire));
    auto a = _cl_array.load(MARE_CLD_MO(mare::mem_order_relaxed));

    if (b >= t + a->size()) {

      auto old_a = a;
      a = a->resize(b, t);
      _cl_array.store(a, MARE_CLD_MO(mare::mem_order_relaxed));

      old_a->set_next(a);
    }
    a->put(b, x);
    mare::internal::mare_atomic_thread_fence(
        MARE_CLD_MO(mare::mem_order_release));
    _bottom.store(b + 1, MARE_CLD_MO(mare::mem_order_relaxed));
    return b - t;
  }

  size_t steal(T& x) {
    MARE_CLD_LOCK;
    size_t t = _top.load(MARE_CLD_MO(mare::mem_order_acquire));
    mare::internal::mare_atomic_thread_fence(
        MARE_CLD_MO(mare::mem_order_seq_cst));
    size_t b = _bottom.load(MARE_CLD_MO(mare::mem_order_acquire));

    if (t >= b)
      return 0;

    auto current_x = x;
    auto a = _cl_array.load(MARE_CLD_MO(mare::mem_order_relaxed));
    x = a->get(t);
    if (!_top.compare_exchange_strong(
            t, t + 1,
            MARE_CLD_MO(mare::mem_order_seq_cst),
            MARE_CLD_MO(mare::mem_order_relaxed))) {

      x = current_x;
      return s_abort;
    }
    return b - t;
  }

  size_t unsafe_size(mare::mem_order order = mare::mem_order_relaxed) {
    auto t = _top.load(MARE_CLD_MO(order));
    auto b = _bottom.load(MARE_CLD_MO(order));

    return b - t;
  }

private:

  std::atomic<size_t> _top;
  std::atomic<size_t> _bottom;
  std::atomic<chase_lev_array<T>*> _cl_array;
  std::mutex _serializing_mutex;

  chase_lev_array<T>* _original_head;
};
