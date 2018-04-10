// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/task/task.hh>

namespace mare {
namespace internal {

MARE_GCC_IGNORE_BEGIN("-Weffc++");

#define MARE_UNARY_TASKPTR_OP(op)                                                         \
 template<typename T>                                                                     \
 inline                                                                                   \
 ::mare::task_ptr<typename ::mare::task_ptr<T>::return_type>                              \
  operator op(const ::mare::task_ptr<T>& t) {                                             \
    using op1_type = typename ::mare::task_ptr<T>::return_type;                           \
    using op1_decayed_type = typename std::decay<op1_type>::type;                         \
    static_assert(::mare::internal::is_mare_task20_ptr<op1_decayed_type>::value == false, \
                  "Cannot perform " #op " operation on non-collapsed tasks.");            \
    auto c = ::mare::create_task([](op1_type op1) {                                       \
        return op op1;                                                                    \
      });                                                                                 \
    c->bind_all(t);                                                                       \
    c->launch();                                                                          \
    return c;                                                                             \
  }

#define MARE_BINARY_TASKPTR_OP_2PTRS(op)                                                             \
template<typename T1, typename T2>                                                                   \
inline                                                                                               \
::mare::task_ptr<decltype(std::declval<typename ::mare::task_ptr<T1>::return_type>()                 \
                                 op                                                                  \
                                 std::declval<typename ::mare::task_ptr<T2>::return_type>())>        \
operator op(const ::mare::task_ptr<T1>& t1,                                                          \
            const ::mare::task_ptr<T2>& t2) {                                                        \
  using op1_type = typename ::mare::task_ptr<T1>::return_type;                                       \
  using op2_type = typename ::mare::task_ptr<T2>::return_type;                                       \
  using op1_decayed_type = typename std::decay<op1_type>::type;                                      \
  using op2_decayed_type = typename std::decay<op2_type>::type;                                      \
  static_assert(::mare::internal::is_mare_task20_ptr<op1_decayed_type>::value == false,              \
                "Cannot perform " #op " operation on non-collapsed tasks.");                         \
  static_assert(::mare::internal::is_mare_task20_ptr<op2_decayed_type>::value == false,              \
                "Cannot perform " #op " operation on non-collapsed tasks.");                         \
  auto c = ::mare::create_task([](op1_type op1, op2_type op2) {                                      \
      return op1 op op2;                                                                             \
  });                                                                                                \
  c->bind_all(t1, t2);                                                                               \
  c->launch();                                                                                       \
  return c;                                                                                          \
}

#define MARE_BINARY_TASKPTR_OP_PTR_VALUE(op)                                                         \
template<typename T1, typename T2>                                                                   \
inline typename                                                                                      \
std::enable_if<                                                                                      \
  ::mare::internal::is_mare_task20_ptr<typename std::decay<T2>::type>::value == false,               \
  ::mare::task_ptr<decltype(std::declval<typename ::mare::task_ptr<T1>::return_type>()               \
                          op                                                                         \
                          std::declval<T2>())>>::type                                                \
operator op(const ::mare::task_ptr<T1>& t1, T2&& op2) {                                              \
  using op1_type = typename ::mare::task_ptr<T1>::return_type;                                       \
  using op1_decayed_type = typename std::decay<op1_type>::type;                                      \
  static_assert(::mare::internal::is_mare_task20_ptr<op1_decayed_type>::value == false,              \
                "Cannot perform " #op " operation on non-collapsed tasks.");                         \
  auto c = ::mare::create_task([op2](op1_type op1) {                                                 \
      return op1 op op2;                                                                             \
  });                                                                                                \
  c->bind_all(t1);                                                                                   \
  c->launch();                                                                                       \
  return c;                                                                                          \
}

#define MARE_BINARY_TASKPTR_OP_VALUE_PTR(op)                                                         \
template<typename T1, typename T2>                                                                   \
inline typename                                                                                      \
std::enable_if<                                                                                      \
  ::mare::internal::is_mare_task20_ptr<typename std::decay<T1>::type>::value == false,               \
  ::mare::task_ptr<decltype(std::declval<T1>()                                                       \
                          op                                                                         \
                          std::declval<typename ::mare::task_ptr<T2>::return_type>())>>::type        \
operator op(T1&& op1, const ::mare::task_ptr<T2>& t2) {                                              \
  using op2_type = typename ::mare::task_ptr<T2>::return_type;                                       \
  using op2_decayed_type = typename std::decay<op2_type>::type;                                      \
  static_assert(::mare::internal::is_mare_task20_ptr<op2_decayed_type>::value == false,              \
                "Cannot perform " #op " operation on non-collapsed tasks.");                         \
  auto c = ::mare::create_task([op1](op2_type op2) {                                                 \
      return op1 op op2;                                                                             \
  });                                                                                                \
  c->bind_all(t2);                                                                                   \
  c->launch();                                                                                       \
  return c;                                                                                          \
}

#define MARE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(op)                                   \
template<typename ReturnType>                                                           \
  template<typename OpType>                                                             \
task_ptr<ReturnType>&                                                                   \
task_ptr<ReturnType>::                                                                  \
operator op##=(OpType&& opd) {                                                          \
  using decayed_optype = typename std::decay<OpType>::type;                             \
  static MARE_CONSTEXPR_CONST bool is_optype_taskptr =                                  \
    mare::internal::is_mare_task20_ptr<decayed_optype>::value;                          \
  using op1_type = ReturnType;                                                          \
  using op2_type =                                                                      \
    typename std::conditional<                                                          \
      is_optype_taskptr,                                                                \
      typename ::mare::internal::is_mare_task20_ptr<decayed_optype>::type,              \
      OpType>::type;                                                                    \
                                                                                        \
  using op1_decayed_type = typename std::decay<op1_type>::type;                         \
  using op2_decayed_type = typename std::decay<op2_type>::type;                         \
                                                                                        \
  static_assert(::mare::internal::is_mare_task20_ptr<op1_decayed_type>::value == false, \
                "Cannot perform " #op " operation on non-collapsed tasks.");            \
  static_assert(is_optype_taskptr == false ||                                           \
                ::mare::internal::is_mare_task20_ptr<op2_decayed_type>::value == false, \
                "Cannot perform " #op " operation on non-collapsed tasks.");            \
  auto t = ::mare::create_task([](op1_type op1,                                         \
                                  op2_type op2) {                                       \
    return static_cast<op1_type>(op1 op op2);                                           \
  });                                                                                   \
  t->bind_all(*this, opd);                                                              \
  parent::operator=(std::move(t));                                                      \
  mare::internal::task* t_ptr = get_raw_ptr();                                          \
  t_ptr->launch(nullptr, nullptr);                                                       \
  return *this;                                                                         \
}

MARE_GCC_IGNORE_END("-Weffc++");
};
};
