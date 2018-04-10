// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <list>
#include <string>

#include <mare/internal/util/macros.hh>

namespace mare {

template<typename ...Stuff> class task_ptr;
template<typename ...Stuff> class task;

class group;
class group_ptr;

namespace internal {

class group;

::mare::internal::group* c_ptr(::mare::group* g);

template<typename Code>
void launch(::mare::group_ptr& gptr, std::list<Code>& kernels);

};

class group {

  friend ::mare::internal::group*
         ::mare::internal::c_ptr(::mare::group* g);

  template<typename Kernel>
  friend void ::mare::internal::launch(::mare::group_ptr& gptr, std::list<Kernel>& kernels);

  using internal_raw_group_ptr = ::mare::internal::group*;

public:

#ifdef ONLY_FOR_DOXYGEN

#error The compiler should not see these methods

  template<typename FullType, typename FirstArg, typename...RestArgs>
  void launch(mare::task_ptr<FullType> const& task,
              FirstArg&& first_arg,
              RestArgs&& ...rest_args);

  template<typename TaskType, typename FirstArg, typename...RestArgs>
  void launch(mare::task<TaskType>* task,
              FirstArg&& first_arg,
              RestArgs&& ...rest_args);

  void launch(mare::task_ptr<> const& task);

  void launch(mare::task<>* task);

#endif

  template<typename Code, typename...Args>
  void launch(Code&& code, Args&& ...args);

  void finish_after();

  void wait_for();

  void cancel();

  void add(task_ptr<> const& task);

  void add(task<>* task);

  group_ptr intersect(group_ptr const& other);
  group_ptr intersect(group* other);

  bool canceled() const;

  std::string to_string() const;

  std::string get_name() const;

protected:

  ~group(){}

private:

  internal_raw_group_ptr get_raw_ptr() const { return _ptr; }
  internal_raw_group_ptr _ptr;

  MARE_DELETE_METHOD(group());
  MARE_DELETE_METHOD(group(group const&));
  MARE_DELETE_METHOD(group(group&&));
  MARE_DELETE_METHOD(group& operator=(group const&));
  MARE_DELETE_METHOD(group& operator=(group&&));
};

};
