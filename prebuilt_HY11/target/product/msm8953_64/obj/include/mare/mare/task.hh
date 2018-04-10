// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <string>
#include <typeinfo>
#include <utility>

#include <mare/internal/task/cputask.hh>
#include <mare/internal/util/debug.hh>

MARE_GCC_IGNORE_BEGIN("-Weffc++");

namespace mare {

template<typename ...Stuff> class task;
template<typename ...Stuff> class task_ptr;

namespace internal {

::mare::internal::task* c_ptr(::mare::task_ptr<>& t);
::mare::internal::task* c_ptr(::mare::task_ptr<> const& t);
::mare::internal::task* c_ptr(::mare::task<>*);

template<typename R, typename... As>
cputask_arg_layer<R(As...)>* c_ptr2(::mare::task<R(As...)>& t);

};

template<>
class task<> {

  friend ::mare::internal::task* ::mare::internal::c_ptr(::mare::task<>* t);

protected:

  using internal_raw_task_ptr = ::mare::internal::task*;

public:

  using size_type = ::mare::internal::task::size_type;

  void launch() {
    MARE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
    get_raw_ptr()->launch(nullptr, nullptr);
  }

  void wait_for() {
    MARE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
    get_raw_ptr()->wait();
  }

  void finish_after() {
    auto p = get_raw_ptr();
    MARE_INTERNAL_ASSERT(p != nullptr, "Unexpected null task<>.");
    auto t = ::mare::internal::current_task();
    MARE_INTERNAL_ASSERT(t != nullptr,
        "finish_after must be called from within a task");
    MARE_INTERNAL_ASSERT(!t->is_pfor(),
        "finish_after cannot be called from within a pfor_each");
    t->finish_after(p);
  }

  task_ptr<>& then(task_ptr<>& succ) {
    MARE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task.");

    auto succ_ptr = ::mare::internal::c_ptr(succ);
    MARE_API_ASSERT(succ_ptr != nullptr, "null task_ptr<>");

    get_raw_ptr()->add_control_dependency(succ_ptr);
    return succ;
  }

  task<>* then(task<>* succ) {
    MARE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task.");

    auto succ_ptr = ::mare::internal::c_ptr(succ);
    MARE_API_ASSERT(succ_ptr != nullptr, "null task_ptr<>");

    get_raw_ptr()->add_control_dependency(succ_ptr);
    return succ;
  }

  void cancel() {
    MARE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
    get_raw_ptr()->cancel();
  }

  bool canceled() const {
    MARE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
    return get_raw_ptr()->is_canceled();
  }

  bool is_bound() const {
    MARE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
    return get_raw_ptr()->is_bound();
  }

  std::string to_string() const {
    MARE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task<>.");
    return get_raw_ptr()->to_string();
  }

protected:

  internal_raw_task_ptr get_raw_ptr() const {
    return _ptr;
  }

  internal_raw_task_ptr _ptr;

  MARE_DELETE_METHOD(task());
  MARE_DELETE_METHOD(task(task const&));
  MARE_DELETE_METHOD(task(task&&));
  MARE_DELETE_METHOD(task& operator=(task const&));
  MARE_DELETE_METHOD(task& operator=(task&&));

};

template<>
class task<void> : public task<> {

protected:

  ~task() { }

  MARE_DELETE_METHOD(task());
  MARE_DELETE_METHOD(task(task const&));
  MARE_DELETE_METHOD(task(task&&));
  MARE_DELETE_METHOD(task& operator=(task const&));
  MARE_DELETE_METHOD(task& operator=(task&&));

};

template<typename ReturnType>
class task<ReturnType> : public task<> {

public:

  using return_type = ReturnType;

  return_type const& copy_value() {
    auto t = get_typed_raw_ptr();
    MARE_INTERNAL_ASSERT(t != nullptr, "Unexpected null task<ReturnType>");
    t->wait();
    return t->get_retval();
  }

  return_type&& move_value() {
    auto t = get_typed_raw_ptr();
    MARE_INTERNAL_ASSERT(t != nullptr, "Unexpected null task<ReturnType>");
    t->wait();
    return t->move_retval();
  }

private:

  using raw_ptr = mare::internal::cputask_return_layer<return_type>*;

  raw_ptr get_typed_raw_ptr() const {
    MARE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null task*");
    return static_cast<raw_ptr>(get_raw_ptr());
  }

  ~task() { }

  MARE_DELETE_METHOD(task());
  MARE_DELETE_METHOD(task(task const&));
  MARE_DELETE_METHOD(task(task&&));
  MARE_DELETE_METHOD(task& operator=(task const&));
  MARE_DELETE_METHOD(task& operator=(task&&));
};

template<typename ReturnType, typename ...Args>
class task<ReturnType(Args...)> : public task<ReturnType> {

public:

  using size_type = task<>::size_type;

  using return_type = ReturnType;

  using args_tuple = std::tuple<Args...>;

  static MARE_CONSTEXPR_CONST size_type arity = sizeof...(Args);

  template <size_type ArgIndex>
  struct argument {
    static_assert(ArgIndex < sizeof...(Args), "Out of range argument indexing.");
    using type = typename std::tuple_element<ArgIndex, args_tuple>::type;
  };

  template<typename... Arguments>
  void bind_all(Arguments&&... args) {
    static_assert(sizeof...(Arguments) == sizeof...(Args),
                  "Invalide number of arguments");

    auto t = get_typed_raw_ptr();
    t->bind_all(std::forward<Arguments>(args)...);
  }

  template<typename... Arguments>
  void launch(Arguments&&... args) {

    using should_bind = typename std::conditional<
                          sizeof...(Arguments) != 0,
                          std::true_type,
                          std::false_type>::type;

    launch_impl(should_bind(), std::forward<Arguments>(args)...);
  }

private:

  using raw_ptr = mare::internal::cputask_arg_layer<return_type(Args...)>*;

  raw_ptr get_typed_raw_ptr() const {
    MARE_API_ASSERT(task<>::get_raw_ptr(),  "Unexpected null ptr");
    return static_cast<raw_ptr>(task<>::get_raw_ptr());
  }

  template<typename... Arguments>
  void launch_impl(std::true_type, Arguments&&... args) {
    bind_all(std::forward<Arguments>(args)...);
    task<>::launch();
  }

  template<typename... Arguments>
  void launch_impl(std::false_type, Arguments&&...) {
    task<>::launch();
  }

  ~task() { }

  MARE_DELETE_METHOD(task());
  MARE_DELETE_METHOD(task(task const&));
  MARE_DELETE_METHOD(task(task&&));
  MARE_DELETE_METHOD(task& operator=(task const&));
  MARE_DELETE_METHOD(task& operator=(task&&));

  template<typename R, typename... As>
  friend
  ::mare::internal::cputask_arg_layer<R(As...)>* c_ptr2(::mare::task<R(As...)>& t);
};

template<typename Task>
mare::internal::by_data_dep_t<Task&&>
bind_as_data_dependency(Task&& t ) {
  return mare::internal::by_data_dep_t<Task&&>(std::forward<Task>(t));
}

template<typename BlockingFunction, typename CancelFunction>
void blocking(BlockingFunction&& bf, CancelFunction&& cf);

template<typename ReturnType, typename ...Args>
MARE_CONSTEXPR_CONST typename task<ReturnType(Args...)>::size_type
task<ReturnType(Args...)>::arity;

template<typename Task>
mare::internal::by_value_t<Task&&> bind_by_value(Task&& t){
  return mare::internal::by_value_t<Task&&>(std::forward<Task>(t));
}

inline void abort_on_cancel() {
  auto t = internal::current_task();
  MARE_API_THROW(t, "Error: you must call abort_on_cancel from within a task.");
  t->abort_on_cancel();
}

void abort_task();

namespace internal {

inline
::mare::internal::task* c_ptr(::mare::task<>* t) {
  return t->get_raw_ptr();
}

template<typename R, typename... As>
::mare::internal::cputask_arg_layer<R(As...)>* c_ptr2(::mare::task<R(As...)>& t) {
  return t.get_typed_raw_ptr();
}

};

MARE_GCC_IGNORE_END("-Weffc++");

};
