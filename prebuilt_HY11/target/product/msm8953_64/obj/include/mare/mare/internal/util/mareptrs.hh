// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <atomic>
#include <memory>
#include <utility>

#include<mare/internal/log/log.hh>
#include<mare/internal/util/debug.hh>

namespace mare {
namespace internal {
namespace testing {

  class MarePtrSuite;

};

template<typename Object>
class mare_shared_ptr;

template <typename Object>
void explicit_unref(Object* obj);

template <typename Object>
void explicit_ref(Object* obj);

typedef uint32_t ref_counter_type;

namespace mareptrs {

template<typename Object>
struct always_delete {
  static void release(Object* obj) {
    delete obj;
  }
};

struct default_logger {

  static void ref(void* o, ref_counter_type count) {
    log::fire_event(log::events::object_reffed(o, count));
  }

  static void unref(void* o, ref_counter_type count) {
    log::fire_event(log::events::object_reffed(o, count));
  }
};

enum class ref_policy {
  do_initial_ref,
  no_initial_ref
};

};

template<typename Object,
         class Logger = mareptrs::default_logger,
         class ReleaseManager = mareptrs::always_delete<Object>
         >
class ref_counted_object {
public:
  using size_type =  ref_counter_type;

  size_type use_count() const {
    return _num_refs.load();
  }

protected:

  explicit ref_counted_object(size_type initial_value)
    :_num_refs(initial_value) {
  }

  constexpr ref_counted_object()
    :_num_refs(0) {
  }

MARE_GCC_IGNORE_BEGIN("-Weffc++");

  ~ref_counted_object(){}

MARE_GCC_IGNORE_END("-Weffc++");

private:

  void unref() {

    auto new_num_refs = --_num_refs;
    Logger::unref(static_cast<Object*>(this), new_num_refs);

    if (new_num_refs == 0)
      ReleaseManager::release(static_cast<Object*>(this));
  }

  void ref() {
    auto new_num_refs = ++_num_refs;
    Logger::ref(static_cast<Object*>(this), new_num_refs);
  }

  friend class mare_shared_ptr<Object>;
  template<typename Descendant>
  friend void explicit_unref(Descendant* obj);
  template<typename Descendant>
  friend void explicit_ref(Descendant* obj);

  std::atomic<size_type> _num_refs;
};

template <typename Object>
void explicit_unref(Object* obj) {
  obj->unref();
}

template <typename Object>
void explicit_ref(Object* obj) {
  obj->ref();
}

template<typename Object>
bool operator==(mare_shared_ptr<Object> const&, mare_shared_ptr<Object> const&);

template<typename Object>
bool operator!=(mare_shared_ptr<Object> const&, mare_shared_ptr<Object> const&);

template<typename Object>
class mare_shared_ptr {
public:

  typedef Object type;
  typedef Object* pointer;
  typedef mareptrs::ref_policy ref_policy;

  constexpr mare_shared_ptr()
    :_target(nullptr) {
  }

  constexpr  mare_shared_ptr(std::nullptr_t)
    :_target(nullptr) {
  }

  mare_shared_ptr(mare_shared_ptr const& other)
    :_target(other._target) {
    if (_target != nullptr)
      _target->ref();
  }

  mare_shared_ptr(mare_shared_ptr&& other)
    : _target(other._target) {

    other._target = nullptr;
  }

  ~mare_shared_ptr() {
    reset(nullptr);
  }

  mare_shared_ptr& operator=(mare_shared_ptr const& other) {
    if (other._target == _target)
      return *this;

    reset(other._target);
    return *this;
  }

  mare_shared_ptr& operator=(std::nullptr_t) {
    if (_target  == nullptr)
      return *this;

    reset(nullptr);
    return *this;
  }

  mare_shared_ptr& operator=(mare_shared_ptr&& other) {
    if (other._target == _target) {
      return *this;
    }
    unref();

    _target = other._target;
    other._target = nullptr;

    return *this;
  }

  void swap(mare_shared_ptr& other) {
    std::swap(_target, other._target);
  }

  Object* get_raw_ptr() const {
    return _target;
  }

  Object* reset_but_not_unref() {
    pointer t = _target;
    _target = nullptr;
    return t;
  }

  void reset() {
    reset(nullptr);
  }

  void reset(pointer ref) {
    unref();
    acquire(ref);
  }

  explicit operator bool() const  {
    return _target != nullptr;
  }

  Object& operator*() const {
    return *_target;
  }

  Object* operator->() const {
    return _target;
  }

  size_t use_count() const {
    if (_target != nullptr)
      return _target->use_count();
    return 0;
  }

  bool is_unique() const {
    return use_count() == 1;
  }

  explicit mare_shared_ptr(pointer ref) :
    _target(ref) {
    if (_target != nullptr)
      _target->ref();
  }

  mare_shared_ptr(pointer ref,  ref_policy policy)
    :_target(ref) {
    if ((_target != nullptr) && policy == ref_policy::do_initial_ref)
      _target->ref();
  }

private:

  void acquire(pointer other){
    _target = other;
    if (_target != nullptr)
      _target->ref();
  }

  void unref() {
    if (_target != nullptr)
      _target->unref();
  }

  friend bool operator==<>(mare_shared_ptr<Object> const&,
                           mare_shared_ptr<Object> const&);
  friend bool operator!=<>(mare_shared_ptr<Object> const&,
                           mare_shared_ptr<Object> const&);

  pointer _target;
};

template<typename Object>
bool operator==(mare_shared_ptr<Object> const& a_ptr,
                mare_shared_ptr<Object> const& b_ptr) {
  return a_ptr.get_raw_ptr() == b_ptr.get_raw_ptr();
}

template<typename Object>
bool operator==(mare_shared_ptr<Object> const& ptr, std::nullptr_t) {
  return !ptr;
}

template<typename Object>
bool operator==(std::nullptr_t, mare_shared_ptr<Object> const& ptr) {
  return !ptr;
}

template<typename Object>
bool operator!=(mare_shared_ptr<Object> const& a_ptr,
                mare_shared_ptr<Object> const& b_ptr) {
  return a_ptr.get_raw_ptr()!=b_ptr.get_raw_ptr();
}

template<typename Object>
bool operator!=(mare_shared_ptr<Object> const& ptr, std::nullptr_t) {
  return static_cast<bool>(ptr);
}

template<typename Object>
bool operator!=(std::nullptr_t, mare_shared_ptr<Object> const& ptr) {
  return static_cast<bool>(ptr);
}

template <typename Object>
Object* c_ptr(mare_shared_ptr<Object>& t) {
  return static_cast<Object*>(t.get_raw_ptr());
}

template <typename Object>
Object* c_ptr(mare_shared_ptr<Object>const& t) {
  return static_cast<Object*>(t.get_raw_ptr());
}

template<typename Object>
Object* c_ptr(Object* p) {
  return p;
}

MARE_GCC_IGNORE_BEGIN("-Weffc++");

template<typename Object>
class mare_buffer_ptr : public std::shared_ptr<Object>
{

MARE_GCC_IGNORE_END("-Weffc++");

private:
  typedef typename Object::Type ElementType;
public:
  constexpr mare_buffer_ptr() :  std::shared_ptr<Object>() { }

  template< class ManagedObject >
  explicit mare_buffer_ptr( ManagedObject* ptr ) : std::shared_ptr<Object>(ptr) { }

  template< class ManagedObject, class Deleter >
  mare_buffer_ptr( ManagedObject* ptr, Deleter d ) : std::shared_ptr<Object>(ptr, d) { }

  template< class ManagedObject, class Deleter, class Alloc >
  mare_buffer_ptr( ManagedObject* ptr, Deleter d, Alloc alloc ) :
    std::shared_ptr<Object>(ptr, d, alloc) { }

  constexpr explicit mare_buffer_ptr( std::nullptr_t ptr) : std::shared_ptr<Object>(ptr) { }

  template< class Deleter >
  mare_buffer_ptr( std::nullptr_t ptr, Deleter d ) :
    std::shared_ptr<Object>( ptr, d) { }

  template< class Deleter, class Alloc >
  mare_buffer_ptr( std::nullptr_t ptr, Deleter d, Alloc alloc ) :
    std::shared_ptr<Object>( ptr, d, alloc) { }

  template< class ManagedObject >
  mare_buffer_ptr( const std::shared_ptr<ManagedObject>& r, Object *ptr ) :
    std::shared_ptr<Object>(r, ptr) { }

  explicit mare_buffer_ptr( const std::shared_ptr<Object>& r ) : std::shared_ptr<Object>(r) { }

  template< class ManagedObject >
  explicit mare_buffer_ptr( const std::shared_ptr<ManagedObject>& r ) : std::shared_ptr<Object>(r) { }

  explicit mare_buffer_ptr( std::shared_ptr<Object>&& r ) : std::shared_ptr<Object>(r) { }

  template< class ManagedObject >
  explicit mare_buffer_ptr( std::shared_ptr<ManagedObject>&& r ) : std::shared_ptr<Object>(r) { }

  template< class ManagedObject >
  explicit mare_buffer_ptr( const std::weak_ptr<ManagedObject>& r ) :
    std::shared_ptr<Object>(r) { }

  template< class ManagedObject >
  explicit mare_buffer_ptr( std::auto_ptr<ManagedObject>&& r ) : std::shared_ptr<Object>(r) { }

  ElementType& operator [] (size_t index)
  {
    Object& mare_buffer = std::shared_ptr<Object>::operator * ();
    return mare_buffer[index];
  }

  const ElementType& operator [] (size_t index) const
  {
    Object& mare_buffer = std::shared_ptr<Object>::operator * ();
    return mare_buffer[index];
  }

  Object& operator * () const { return std::shared_ptr<Object>::operator * (); }

  Object* operator -> () const { return std::shared_ptr<Object>::operator -> (); }
};

};
};
