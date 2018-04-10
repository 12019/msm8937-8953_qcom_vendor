// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/task/cputaskfactory.hh>
#include <mare/internal/task/gputaskfactory.hh>
#include <mare/internal/task/group.hh>
#include <mare/internal/task/hexagontaskfactory.hh>
#include <mare/internal/util/macros.hh>

#if !defined(MARE_NO_TASK_ALLOCATOR)
#include <mare/internal/memalloc/concurrentbumppool.hh>
#include <mare/internal/memalloc/taskallocator.hh>
#endif

namespace mare {

template<typename Fn> class cpu_kernel;
template<typename Fn> class hexagon_kernel;

namespace internal {

MARE_GCC_IGNORE_BEGIN("-Weffc++");

template<typename F, class Enable = void>
struct is_taskable : std::integral_constant<bool, false> {
};

template<typename F>
struct is_taskable<F, typename std::enable_if<!std::is_same<typename function_traits<F>::kind,
                                                            user_code_types::invalid>::value>::type>
                  : std::integral_constant<bool, true> {
};

template<typename F>
struct is_taskable<::mare::cpu_kernel<F>, void> : std::integral_constant<bool, true> {
};

template<typename F>
struct is_taskable<::mare::hexagon_kernel<F>, void> : std::integral_constant<bool, true> {
};

template<typename ...KernelArgs>
struct is_taskable<::mare::gpu_kernel<KernelArgs...>, void> : std::integral_constant<bool, true> {
};

template<typename Code, class Enable = void>
struct task_factory;

template<typename Code>
struct task_factory<Code, typename std::enable_if<is_taskable<typename std::remove_reference<Code>::type>::value>::type>
  : public task_factory_dispatch<typename std::remove_reference<Code>::type> {

private:

  using parent = task_factory_dispatch<typename std::remove_reference<Code>::type>;

public:

  using collapsed_task_type = typename parent::collapsed_task_type;
  using non_collapsed_task_type = typename parent::non_collapsed_task_type;

  using collapsed_raw_task_type = typename parent::collapsed_raw_task_type;
  using non_collapsed_raw_task_type = typename parent::non_collapsed_raw_task_type;

  template<typename UserCode, typename... Args>
  static collapsed_raw_task_type* create_collapsed_task(UserCode&& code, Args&&...args) {
    return parent::create_collapsed_task(std::forward<UserCode>(code), std::forward<Args>(args)...);
  };

  template<typename UserCode, typename... Args>
  static non_collapsed_raw_task_type* create_non_collapsed_task(UserCode&& code, Args&&...args) {
    return parent::create_non_collapsed_task(std::forward<UserCode>(code), std::forward<Args>(args)...);
  };

  template<typename UserCode, typename... Args>
  static void launch(bool notify, group* g, UserCode&& code, Args&&...args) {
    g->inc_task_counter();
    bool success = parent::launch(notify, g, std::forward<UserCode>(code), std::forward<Args>(args)...);

    if (!success)
      g->dec_task_counter();
  }
};

MARE_GCC_IGNORE_END("-Weffc++");
};
};
