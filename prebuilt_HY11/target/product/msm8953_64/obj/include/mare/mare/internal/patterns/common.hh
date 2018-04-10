// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/runtime.hh>

#include <mare/internal/legacy/group.hh>
#include <mare/internal/legacy/task.hh>
#include <mare/internal/patterns/policy_adaptive_stealer.hh>
#include <mare/internal/util/distance.hh>

namespace mare {

namespace internal {

template<class T>
T static_chunk_size(T count, const size_t nctx)
{

  T const GRANULARITY_MULTIPLIER = 4;
  return std::max(T(1), count / (GRANULARITY_MULTIPLIER *
                  static_cast<T>(nctx)));
}

template <typename NullaryFn>
void launch_or_exec(bool not_last, internal::group_shared_ptr& g, NullaryFn&& fn)
{
  if (not_last) {
    auto attrs = legacy::create_task_attrs(attr::pfor);
    legacy::launch(g, legacy::with_attrs(attrs, fn));
  } else {
    auto attrs = legacy::create_task_attrs(attr::pfor, attr::inlined);
    auto master = legacy::create_task(legacy::with_attrs(attrs, fn));
    c_ptr(master)->execute_inline_task(c_ptr(g));
  }
}

template<typename T, typename AdaptiveStrategy, Policy P>
void execute_master_task(mare_shared_ptr<AdaptiveStrategy> strategy_ptr)
{
  auto group_ptr = strategy_ptr->get_group();
  auto master_attrs = legacy::create_task_attrs(attr::pfor, attr::inlined);
  auto helper_attrs = legacy::create_task_attrs(attr::pfor);

  auto master = legacy::create_task(legacy::with_attrs(master_attrs, [strategy_ptr] ()
          mutable {
            stealer_wrapper<T, AdaptiveStrategy, P>
              ::stealer_task_body(strategy_ptr, 0); }
        ));

  group* g = c_ptr(group_ptr);
  size_t max_tasks = strategy_ptr->get_max_tasks();

  for (size_t i = 1; i < max_tasks; ++i) {
    legacy::launch(group_ptr, legacy::with_attrs(helper_attrs,
                              [strategy_ptr, i]() mutable
                              { stealer_wrapper<T, AdaptiveStrategy, P>
                                ::stealer_task_body(strategy_ptr, i); }
                              ), false);
  }

  mare::internal::notify_all(max_tasks - 1);
  c_ptr(master)->execute_inline_task(g);
}

};
};
