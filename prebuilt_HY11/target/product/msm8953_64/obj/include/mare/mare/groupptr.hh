// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <string>

#include <mare/group.hh>

#include <mare/internal/util/debug.hh>
#include <mare/internal/util/mareptrs.hh>
#include <mare/internal/task/group.cc.hh>

namespace mare {

class group_ptr;

group_ptr create_group();
group_ptr create_group(std::string const &name);

namespace internal {

group* c_ptr(::mare::group_ptr& g);
group* c_ptr(::mare::group_ptr const& g);

namespace testing {
  class group_tester;
};
};

class group_ptr {

  friend ::mare::internal::group*
         ::mare::internal::c_ptr(::mare::group_ptr & g);
  friend ::mare::internal::group*
         ::mare::internal::c_ptr(::mare::group_ptr const& g);

  friend group_ptr create_group(std::string const &);
  friend group_ptr create_group(const char *);
  friend group_ptr create_group();
  friend group_ptr intersect(group_ptr const& a, group_ptr const& b);
  friend class ::mare::internal::testing::group_tester;
  friend class ::mare::group;

public:

  group_ptr() :
    _shared_ptr(nullptr) {
  }

   group_ptr(std::nullptr_t) :
    _shared_ptr(nullptr) {
  }

  group_ptr(group_ptr const& other) :
    _shared_ptr(other._shared_ptr) {
  }

  group_ptr(group_ptr&& other) :
    _shared_ptr(std::move(other._shared_ptr)) {
  }

  group_ptr& operator=(group_ptr const& other) {
    _shared_ptr = other._shared_ptr;
    return *this;
  }

  group_ptr& operator=(std::nullptr_t) {
    _shared_ptr = nullptr;
    return *this;
  }

  group_ptr& operator=(group_ptr&& other) {
    _shared_ptr = (std::move(other._shared_ptr));
    return *this;
  }

  void swap(group_ptr& other) {
    std::swap(_shared_ptr, other._shared_ptr);
  }

  group* operator->() const {
    MARE_INTERNAL_ASSERT(_shared_ptr != nullptr, "Invalid group ptr");
    auto g = get_raw_ptr();
    MARE_INTERNAL_ASSERT(g != nullptr, "Invalid null pointer.");
    return g->get_facade<group>();
  }

  group* get() const {
    auto t = get_raw_ptr();
    if (t == nullptr)
      return nullptr;
    return t->get_facade<group>();
  }

  void reset() {
    _shared_ptr.reset();
  }

  explicit operator bool() const {
    return _shared_ptr != nullptr;
  }

  size_t use_count() const {
    return _shared_ptr.use_count();
  }

  bool unique() const {
    return _shared_ptr.use_count() == 1;
  }

  ~group_ptr() {}

protected:

  explicit group_ptr(internal::group* g) :
    _shared_ptr(g) {
  }

  group_ptr(internal::group* g,
            internal::group_shared_ptr::ref_policy policy) :
    _shared_ptr(g, policy) {
  }

  explicit group_ptr(internal::group_shared_ptr&& g) :
    _shared_ptr(g.reset_but_not_unref(),
                internal::group_shared_ptr::ref_policy::no_initial_ref) {
  }

  internal::group* get_raw_ptr() const {
    return ::mare::internal::c_ptr(_shared_ptr);
  }

private:

  internal::group_shared_ptr _shared_ptr;

  static_assert(sizeof(group) == sizeof(internal::group::self_ptr), "Invalid group size.");
};

inline
bool operator==(group_ptr const& g, std::nullptr_t)  {
  return !g;
}

inline
bool operator==(std::nullptr_t, group_ptr const& g)  {
  return !g;
}

inline
bool operator!=(group_ptr const& g, std::nullptr_t)  {
  return static_cast<bool>(g);
}

inline
bool operator!=(std::nullptr_t, group_ptr const& g)  {
  return static_cast<bool>(g);
}

inline
bool operator==(group_ptr const& a, group_ptr const& b)  {
  return mare::internal::c_ptr(a) == mare::internal::c_ptr(b);
}

inline
bool operator!=(group_ptr const& a, group_ptr const& b)  {
  return mare::internal::c_ptr(a) != mare::internal::c_ptr(b);
}

inline
group_ptr create_group(const char* name)
{
  return group_ptr(::mare::internal::group_factory::create_leaf_group(name));
}

inline
group_ptr create_group(std::string const& name)
{
  return group_ptr(::mare::internal::group_factory::create_leaf_group(name));
}

inline
group_ptr create_group()
{
  return group_ptr(::mare::internal::group_factory::create_leaf_group());
}

group_ptr intersect(group_ptr const& a, group_ptr const& b);

inline
group_ptr operator&(group_ptr const& a, group_ptr const& b)
{
  return ::mare::intersect(a,b);
}

  void finish_after(group* g);

void finish_after(group_ptr const& g);

namespace internal {

inline
::mare::internal::group* c_ptr(::mare::group_ptr& g) {
  return g.get_raw_ptr();
}

inline
::mare::internal::group* c_ptr(::mare::group_ptr const& g) {
  return g.get_raw_ptr();
}

template<typename Code>
void launch(mare::group_ptr& gptr, std::list<Code>& kernels)
{
  auto sz = kernels.size();
  if (sz == 0)
    return;

  auto g = c_ptr(gptr);
  MARE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group.");

  using decayed_type = typename std::decay<Code>::type;

  static_assert(!::mare::internal::is_mare_task20_ptr<decayed_type>::value, "can only launch multiple kernels, not tasks");
  using launch_policy = mare::internal::group_launch::launch_code<Code>;

  for (auto k = kernels.begin(), e = kernels.end(); k != e; k++) {
    launch_policy::launch_impl(false, g, *k, std::false_type());
  }
  mare::internal::notify_all(sz);
}

};

};
