// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/cpukernel.hh>
#include <mare/gpukernel.hh>
#include <mare/groupptr.hh>
#include <mare/hexagonkernel.hh>
#include <mare/taskptr.hh>

#include <mare/internal/task/operators.hh>
#include <mare/internal/util/templatemagic.hh>

namespace mare {

struct do_not_collapse_t {};

const do_not_collapse_t do_not_collapse {};

template<typename Fn>
using collapsed_task_type = typename ::mare::internal::task_factory<Fn>::collapsed_task_type;

template<typename Fn>
using non_collapsed_task_type =typename ::mare::internal::task_factory<Fn>::non_collapsed_task_type;

template<typename ReturnType, typename... Args>
::mare::task_ptr<ReturnType> create_value_task(Args&&... args) {
  auto t = ::mare::internal::cputask_factory::create_value_task<ReturnType>(std::forward<Args>(args)...);
  return ::mare::task_ptr<ReturnType>(t, ::mare::internal::task_shared_ptr::ref_policy::no_initial_ref);
}

template<typename Code, typename... Args>
collapsed_task_type<Code> create_task(Code&& code, Args&&... args)
{
  using factory = ::mare::internal::task_factory<Code>;
  auto raw_task_ptr = factory::create_collapsed_task(std::forward<Code>(code),
                                                     std::forward<Args>(args)...);
  return collapsed_task_type<Code>(raw_task_ptr, ::mare::internal::task_shared_ptr::ref_policy::no_initial_ref);
}

namespace internal {
template<typename... Args>
void create_task_impl(::mare::internal::task*, std::tuple<>&&, Args&&...)
{

}

template<typename Code1, typename... Codes, typename... Args>
void create_task_impl(::mare::internal::group* g, ::mare::internal::task* t,
    std::tuple<Code1, Codes...>&& codes, Args&&... args)
{
  using factory1 = ::mare::internal::task_factory<Code1>;
  auto raw_task_ptr1 = factory1::create_collapsed_task(std::forward<Code1>(std::get<0>(codes)),
                                                     std::forward<Args>(args)...);
  t->add_alternative(raw_task_ptr1);
  raw_task_ptr1->join_group(g, false);
  create_task_impl(t, ::mare::internal::tuple_rest(codes), std::forward<Args>(args)...);
}
};

namespace beta {

template<typename Code1, typename... Codes, typename... Args>
collapsed_task_type<Code1> create_task(std::tuple<Code1, Codes...>&& codes, Args&&... args)
{
  using factory1 = ::mare::internal::task_factory<Code1>;

  auto raw_task_ptr1 = factory1::create_collapsed_task(std::forward<Code1>(std::get<0>(codes)),
                                                     std::forward<Args>(args)...);

  auto g = mare::create_group();
  auto g_ptr = ::mare::internal::c_ptr(g);

  ::mare::internal::create_task_impl(g_ptr, raw_task_ptr1, ::mare::internal::tuple_rest(codes), std::forward<Args>(args)...);

  auto t = collapsed_task_type<Code1>(raw_task_ptr1, ::mare::internal::task_shared_ptr::ref_policy::no_initial_ref);

  g_ptr->set_representative_task(raw_task_ptr1);

  return t;
}
};

template<typename Code, typename... Args>
non_collapsed_task_type<Code> create_task(do_not_collapse_t, Code&& code, Args&&... args)
{
  using factory = ::mare::internal::task_factory<Code>;
  auto raw_task_ptr = factory::create_non_collapsed_task(std::forward<Code>(code),
                                                         std::forward<Args>(args)...);
  return non_collapsed_task_type<Code>(raw_task_ptr, ::mare::internal::task_shared_ptr::ref_policy::no_initial_ref);
}

template<typename Code, typename...Args>
collapsed_task_type<Code> launch(Code&& code, Args&& ...args)
{
  using factory = ::mare::internal::task_factory<Code>;
  auto raw_task_ptr = factory::create_collapsed_task(std::forward<Code>(code),
                                                     std::forward<Args>(args)...);
  raw_task_ptr->launch(nullptr, nullptr);
  return collapsed_task_type<Code>(raw_task_ptr, ::mare::internal::task_shared_ptr::ref_policy::no_initial_ref);
}

template<typename Code, typename...Args>
non_collapsed_task_type<Code> launch(do_not_collapse_t, Code&& code, Args&& ...args)
{
  using factory = ::mare::internal::task_factory<Code>;
  auto raw_task_ptr = factory::create_non_collapsed_task(std::forward<Code>(code),
                                                         std::forward<Args>(args)...);
  raw_task_ptr->launch(nullptr, nullptr);
  return non_collapsed_task_type<Code>(raw_task_ptr, ::mare::internal::task_shared_ptr::ref_policy::no_initial_ref);
}

MARE_GCC_IGNORE_BEGIN("-Weffc++");

MARE_UNARY_TASKPTR_OP(+)
MARE_UNARY_TASKPTR_OP(-)
MARE_UNARY_TASKPTR_OP(~)

MARE_BINARY_TASKPTR_OP_2PTRS(+)
MARE_BINARY_TASKPTR_OP_2PTRS(-)
MARE_BINARY_TASKPTR_OP_2PTRS(*)
MARE_BINARY_TASKPTR_OP_2PTRS(/)
MARE_BINARY_TASKPTR_OP_2PTRS(%)
MARE_BINARY_TASKPTR_OP_2PTRS(&)
MARE_BINARY_TASKPTR_OP_2PTRS(|)
MARE_BINARY_TASKPTR_OP_2PTRS(^)

MARE_BINARY_TASKPTR_OP_PTR_VALUE(+)
MARE_BINARY_TASKPTR_OP_PTR_VALUE(-)
MARE_BINARY_TASKPTR_OP_PTR_VALUE(*)
MARE_BINARY_TASKPTR_OP_PTR_VALUE(/)
MARE_BINARY_TASKPTR_OP_PTR_VALUE(%)
MARE_BINARY_TASKPTR_OP_PTR_VALUE(&)
MARE_BINARY_TASKPTR_OP_PTR_VALUE(|)
MARE_BINARY_TASKPTR_OP_PTR_VALUE(^)

MARE_BINARY_TASKPTR_OP_VALUE_PTR(+)
MARE_BINARY_TASKPTR_OP_VALUE_PTR(-)
MARE_BINARY_TASKPTR_OP_VALUE_PTR(*)
MARE_BINARY_TASKPTR_OP_VALUE_PTR(/)
MARE_BINARY_TASKPTR_OP_VALUE_PTR(%)
MARE_BINARY_TASKPTR_OP_VALUE_PTR(&)
MARE_BINARY_TASKPTR_OP_VALUE_PTR(|)
MARE_BINARY_TASKPTR_OP_VALUE_PTR(^)

MARE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(+)
MARE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(-)
MARE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(*)
MARE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(/)
MARE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(%)
MARE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(&)
MARE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(|)
MARE_TASKPTR_COMPOUND_ASSIGNMENT_OPERATOR(^)

MARE_GCC_IGNORE_END("-Weffc++");

};
