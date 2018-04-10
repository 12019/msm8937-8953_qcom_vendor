// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <atomic>

#include <mare/internal/atomic/alignedatomic.hh>
#include <mare/internal/compat/compilercompat.h>
#include <mare/internal/memalloc/alignedmalloc.hh>
#include <mare/internal/util/debug.hh>
#include <mare/internal/util/macros.hh>
#include <mare/internal/util/memorder.hh>

#define MARE_BLFQ_FORCE_SEQ_CST 0

#if MARE_BLFQ_FORCE_SEQ_CST
#define MARE_BLFQ_MO(x) mare::mem_order_seq_cst
#else
#define MARE_BLFQ_MO(x) x
#endif

namespace mare{

namespace internal{

namespace blfq{

struct node
{
#if MARE_HAS_ATOMIC_DMWORD
  typedef mare_dmword_t node_t;
  node_t _safe    : 1;
  node_t _empty   : 1;
  #if MARE_SIZEOF_MWORD == 8
    node_t _idx     : 62;
    node_t _val     : 64;
  #elif MARE_SIZEOF_MWORD == 4
    node_t _idx     : 30;
    node_t _val     : 32;
  #else
    #error "unknown MARE_SIZEOF_MWORD"
  #endif
#else
  #error "The Bounded Lock-Free Queue requires Double Machine Word atomics"
#endif

  node(node_t safe, node_t empty, node_t idx, node_t val) :
    _safe(safe),
    _empty(empty),
    _idx(idx),
    _val(val) {}

  node() : _safe(), _empty(), _idx(), _val() {}
};

class bounded_lfqueue{

private:

  static constexpr size_t s_starving_limit = 100;

  inline void close_node(){

    #if MARE_SIZEOF_MWORD == 8
      _tail.fetch_or(size_t(1) << 63, MARE_BLFQ_MO(mare::mem_order_relaxed));
    #elif MARE_SIZEOF_MWORD == 4
      _tail.fetch_or(size_t(1) << 31, MARE_BLFQ_MO(mare::mem_order_relaxed));
    #endif
  }

  inline size_t extract_index_from_tail(size_t tail){

    return((tail << 1) >> 1);
  }

  inline bool is_node_closed(size_t tail){

    #if MARE_SIZEOF_MWORD == 8
      return((tail & (size_t(1) << 63)) != 0);
    #elif MARE_SIZEOF_MWORD == 4
      return((tail & (size_t(1) << 31)) != 0);
    #endif
  }

  void fix_state(){
    while(true){
      auto t = _tail.load(MARE_BLFQ_MO(mare::mem_order_relaxed));
      auto h = _head.load(MARE_BLFQ_MO(mare::mem_order_relaxed));

      if(_tail != t)

        continue;

      if(h <= t){
        break;
      }

      if(_tail.compare_exchange_strong(t,h, MARE_BLFQ_MO(mare::mem_order_relaxed))){
        break;
      }
    }
  }

public:

  explicit bounded_lfqueue(size_t logsz):
    _head(0),
    _tail(0),
    _logsz(logsz),
    _max_array_size(size_t(1) << logsz),
    _array( nullptr){

    #if MARE_SIZEOF_MWORD == 8
      MARE_INTERNAL_ASSERT(logsz <= 62, "On 64 bit systems the largest size of the blfq array allowed is 2^62");
    #elif MARE_SIZEOF_MWORD == 4
      MARE_INTERNAL_ASSERT(logsz <= 30, "On 32 bit systems the largest size of the blfq array allowed is 2^30");
    #endif

    auto sz = sizeof(*_array) * (_max_array_size);
    auto mem = mare_aligned_malloc(MARE_SIZEOF_DMWORD, sz);
    if (mem == nullptr)
      throw std::bad_alloc();

    memset(mem, 0, sz);
    _array = static_cast<alignedatomic<node>*>(mem);

    for (size_t i = 0; i < _max_array_size; i++) {
      node a;
      a._safe = 1;
      a._empty = 1;
      a._idx  = i;
      a._val  = 0;
      _array[i].store(a);
    }
    mare_atomic_thread_fence(MARE_BLFQ_MO(mare::mem_order_seq_cst));
  }

  ~bounded_lfqueue(){
    mare_aligned_free(_array);
  }

  MARE_DELETE_METHOD(bounded_lfqueue());

  MARE_DELETE_METHOD(bounded_lfqueue(bounded_lfqueue const&));
  MARE_DELETE_METHOD(bounded_lfqueue(bounded_lfqueue &&));
  MARE_DELETE_METHOD(bounded_lfqueue& operator=(bounded_lfqueue const&));
  MARE_DELETE_METHOD(bounded_lfqueue& operator=(bounded_lfqueue &&));

  size_t take(size_t * result){
    size_t num_iter = 0;
    while(true){

      auto h = _head.fetch_add(1,MARE_BLFQ_MO(mare::mem_order_relaxed));
      while(true){

        auto current_node = _array[h & (_max_array_size - 1)].load(MARE_BLFQ_MO(mare::mem_order_relaxed));

        if(current_node._idx > h){
          break;
        }

        if(current_node._empty == 0){
          if(current_node._idx == h){
            if(_array[h & (_max_array_size - 1)].compare_exchange_strong(current_node,
              node(current_node._safe, 1, (h + _max_array_size), 0), MARE_BLFQ_MO(mare::mem_order_relaxed) ) ){

              *result = current_node._val;

              auto t = extract_index_from_tail(_tail.load(MARE_BLFQ_MO(mare::mem_order_relaxed)));

              MARE_INTERNAL_ASSERT(t > h, "Error. head has to be strictly less than tail as the pop was successful.");

              return (t - h);
            }
          }
          else{

            if(_array[h & (_max_array_size - 1)].compare_exchange_strong(current_node,
               node(0, current_node._empty, current_node._idx, current_node._val),
                    MARE_BLFQ_MO(mare::mem_order_relaxed))){
              break;
            }
          }
        }

        else{

          auto t = extract_index_from_tail(_tail.load(MARE_BLFQ_MO(mare::mem_order_relaxed)));
          if(t >= (h+1)){

            num_iter++;
          }
          else{

            num_iter = s_starving_limit;
          }

          if(num_iter >= s_starving_limit){
            if(_array[h & (_max_array_size - 1)].compare_exchange_strong(current_node,
               node(current_node._safe, current_node._empty, (h + _max_array_size), 0),
               MARE_BLFQ_MO(mare::mem_order_relaxed))){
              break;
            }
          }
          continue;
        }
      }

      auto t = extract_index_from_tail(_tail.load(MARE_BLFQ_MO(mare::mem_order_relaxed)));
      if(t <= (h + 1)){

        fix_state();
        return 0;
      }
    }
    return 0;
  }

  size_t put(size_t value){
    while(true){

      size_t t = 0;
      size_t h = 0;

      size_t num_retries = 0;
#if MARE_SIZEOF_MWORD == 8
      t = _tail.fetch_add(1, MARE_BLFQ_MO(mare::mem_order_relaxed));

      if(is_node_closed(t)){
        return 0;
      }

      MARE_INTERNAL_ASSERT(((t & (size_t(1) << 62)) == 0), "tail should be less than 2^62");

#else

      while(true){
        t = _tail.load(MARE_BLFQ_MO(mare::mem_order_relaxed));

        if(is_node_closed(t)){
          return 0;
        }

        if((t+1) < (size_t(1) << 30)){

          if(_tail.compare_exchange_strong(t, t+1, MARE_BLFQ_MO(mare::mem_order_relaxed))){

            break;
          }
        }
        else{

          close_node();
          return 0;
        }
      }
#endif

      auto current_node = _array[t & (_max_array_size - 1)].load(MARE_BLFQ_MO(mare::mem_order_relaxed));

      h = _head.load(MARE_BLFQ_MO(mare::mem_order_relaxed));

      if(current_node._empty != 0){
        if((current_node._idx <= t) && ((current_node._safe != 0) || (h <= t))){
          if(_array[t & (_max_array_size - 1)].compare_exchange_strong(
            current_node, node(1,0,t,value), MARE_BLFQ_MO(mare::mem_order_seq_cst))){

            if(h <= t){
              return ((t + 1) - h);
            }
            else{

              return 1;
            }
          }
          else{

            h = _head.load(MARE_BLFQ_MO(mare::mem_order_relaxed));

            num_retries++;
          }
        }
      }

      if((((t - h) >= _max_array_size) && (t > h)) || (num_retries > s_starving_limit)){
        close_node();
        return 0;
      }
    }
  }

  size_t get_size() const{
    auto t = _tail.load(MARE_BLFQ_MO(mare::mem_order_relaxed));
    auto h = _head.load(MARE_BLFQ_MO(mare::mem_order_relaxed));
    return (t - h);
  }

  size_t get_log_array_size() const{ return _logsz;}

  size_t get_max_array_size() const{ return _max_array_size;}

private:

  std::atomic<size_t> _head;

  std::atomic<size_t> _tail;

  size_t const _logsz;

  size_t const _max_array_size;

  alignedatomic<node>*  _array;

};

template<typename T, bool FITS_IN_SIZE_T>
class blfq_size_t
{

};

template<typename T>
class blfq_size_t<T,false>
{
static_assert(sizeof(T*) <= sizeof(size_t), "Error. Address has size greater than sizeof(size_t)");

public:
  explicit blfq_size_t(size_t log_size = 12) : _blfq(log_size) { }

  MARE_DELETE_METHOD(blfq_size_t(blfq_size_t const&));
  MARE_DELETE_METHOD(blfq_size_t(blfq_size_t &&));
  MARE_DELETE_METHOD(blfq_size_t& operator=(blfq_size_t const&));
  MARE_DELETE_METHOD(blfq_size_t& operator=(blfq_size_t &&));

  size_t push(T const& value)
  {

    T * ptr = new T(value);
    size_t res = _blfq.put(reinterpret_cast<size_t>(ptr));
    if (res == 0)
      delete ptr;

    return res;
  }

MARE_GCC_IGNORE_BEGIN("-Wstrict-aliasing")
  size_t pop(T& result) {
    T *ptr;
    size_t res = _blfq.take(reinterpret_cast<size_t*>(&ptr));
    if (res != 0){
      result = *ptr;

      delete ptr;
    }
    return res;
  }
MARE_GCC_IGNORE_END("-Wstrict-aliasing")

  size_t size() const{ return _blfq.get_size(); }

  size_t get_log_array_size() const{ return _blfq.get_log_array_size(); }

  size_t get_max_array_size() const{ return _blfq.get_max_array_size(); }

private:
  bounded_lfqueue _blfq;

};

template <typename T>
struct blfq_convert
{
  static_assert(sizeof(T) <= sizeof(size_t), "Error. sizeof(T) > sizeof(size_t)");

MARE_GCC_IGNORE_BEGIN("-Wstrict-aliasing")
  static size_t cast_from(T v) {
    size_t u;
    *(reinterpret_cast<T*>(&u)) = v;
    return u;
  }

  static T cast_to(size_t v) {
    return(*(reinterpret_cast<T*>(&v)));
  }
MARE_GCC_IGNORE_END("-Wstrict-aliasing")
};

template <typename T>
struct blfq_convert<T*>
{
  static_assert(sizeof(T*) <= sizeof(size_t), "Error. sizeof(T*) > sizeof(size_t)");

  static size_t cast_from(T* v) { return reinterpret_cast<size_t>(v); }
  static T* cast_to(size_t v) { return reinterpret_cast<T*>(v); }
};

template<typename T>
class blfq_size_t<T, true>
{
public:
  typedef T value_type;
  blfq_size_t<T, true>(size_t log_size = 12) : _blfq(log_size) {}

  MARE_DELETE_METHOD(blfq_size_t(blfq_size_t const&));
  MARE_DELETE_METHOD(blfq_size_t(blfq_size_t &&));
  MARE_DELETE_METHOD(blfq_size_t& operator=(blfq_size_t const&));
  MARE_DELETE_METHOD(blfq_size_t& operator=(blfq_size_t &&));

  size_t push(value_type const& value) {
    return (_blfq.put(blfq_convert<T>::cast_from(value)));
  }

  size_t pop(value_type& result) {
    size_t tmp;
    size_t res = _blfq.take(&tmp);
    if (res != 0){
      result = blfq_convert<T>::cast_to(tmp);
    }
    return res;
  }

  size_t size() const{ return _blfq.get_size();}

  size_t get_log_array_size() const{ return _blfq.get_log_array_size(); }

  size_t get_max_array_size() const{ return _blfq.get_max_array_size(); }

private:
  bounded_lfqueue _blfq;

};

};

};

};
