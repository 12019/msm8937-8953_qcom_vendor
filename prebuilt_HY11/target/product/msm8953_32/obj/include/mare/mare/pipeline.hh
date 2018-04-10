// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/pipelinedata.hh>

#ifdef MARE_CAPI_COMPILING_CC
#include <lib/capi.hh>
#endif
#include <mare/internal/patterns/pipeline/pipeline.hh>
#include <mare/internal/patterns/pipeline/pipelineutility.hh>

namespace mare {

namespace internal {
namespace test {
  class pipeline_tester;
  class pipeline_capi_internal_tester;
};
};

class serial_stage;
class parallel_stage;
class iteration_lag;
class iteration_rate;

enum param_list_type {
  mare_iteration_lag,
  mare_iteration_rate,
  mare_sliding_window_size
};

namespace pattern {

template<typename ...UserData>
class pipeline {

protected:

  mare::internal::mare_shared_ptr<mare::internal::pipeline_skeleton<UserData...>> _skeleton;

  static constexpr size_t userdata_size = sizeof...(UserData);
  static_assert(userdata_size <= 1,
                "MARE pipeline can only have, at most, one type of context data.");

  template<typename... Args>
  void add_cpu_stage(Args&&... args) {
    using check_params =
      typename mare::internal::pipeline_utility::check_add_cpu_stage_params<context&, Args...>;
    static_assert(check_params::value, "add_cpu_stage has illegal parameters");

    int const stage_index = check_params::stage_index;
    int const body_index  = check_params::body_index;
    int const lag_index   = check_params::result::iteration_lag_index;
    int const rate_index  = check_params::result::iteration_rate_index;
    int const sws_index   = check_params::result::sliding_window_size_index;
    bool const has_iteration_lag =
      check_params::result::iteration_lag_num == 0 ? false : true;
    using StageType       = typename check_params::StageType;

    auto input_tuple      = std::forward_as_tuple(args...);
    mare::iteration_lag       lag(0);
    mare::iteration_rate      rate(1, 1);
    mare::sliding_window_size sws(0);
    auto default_tuple    = std::forward_as_tuple(lag, rate, sws);

    using lag_mux =
      typename mare::internal::pipeline_utility::mux_param_value<mare::iteration_lag,
                                                                 decltype(input_tuple),
                                                                 decltype(default_tuple),
                                                                 lag_index,
                                                                 param_list_type::mare_iteration_lag>;
    using rate_mux =
      typename mare::internal::pipeline_utility::mux_param_value<mare::iteration_rate,
                                                                 decltype(input_tuple),
                                                                 decltype(default_tuple),
                                                                 rate_index,
                                                                 param_list_type::mare_iteration_rate>;
    using sws_mux =
      typename mare::internal::pipeline_utility::mux_param_value<mare::sliding_window_size,
                                                                 decltype(input_tuple),
                                                                 decltype(default_tuple),
                                                                 sws_index,
                                                                 param_list_type::mare_sliding_window_size>;
    using Body =
      typename mare::internal::pipeline_utility::get_cpu_body_type<decltype(input_tuple), body_index>::type;

    auto skeleton = c_ptr(_skeleton);
    skeleton->template add_cpu_stage<StageType, Body>(
      std::get<stage_index>(input_tuple),
      std::forward<Body>(mare::internal::pipeline_utility::get_tuple_element_helper<body_index,
                         decltype(input_tuple)>::get(input_tuple)),
      has_iteration_lag,
      lag_mux::get(input_tuple, default_tuple),
      rate_mux::get(input_tuple, default_tuple),
      sws_mux::get(input_tuple, default_tuple));
  }

public:

  using context = pipeline_context<UserData...>;

  pipeline() :
    _skeleton(new mare::internal::pipeline_skeleton<UserData...>()) {
  }

  virtual ~pipeline() {}

  pipeline(pipeline const& other) :
   _skeleton(
     new mare::internal::pipeline_skeleton<UserData...>(*c_ptr(other._skeleton))) {
  }

  pipeline(pipeline && other) :_skeleton(other._skeleton) {
    other._skeleton = nullptr;
  }

  pipeline& operator=(pipeline const& other) {
    _skeleton =
      mare::internal::mare_shared_ptr<mare::internal::pipeline_skeleton<UserData...>>(
        new mare::internal::pipeline_skeleton<UserData...>(*c_ptr(other._skeleton)));

    return *this;
  }

  pipeline& operator=(pipeline&& other) {
    _skeleton = other._skeleton;
    other._skeleton = nullptr;
    return *this;
  }

  template<typename... Args>
  void add_stage(Args&&... args)  {
    add_cpu_stage(std::forward<Args>(args)...);
  }

  void run(UserData*... context_data,
           size_t num_iterations) const {

    auto skeleton = c_ptr(_skeleton);

    if (skeleton->is_empty())
      return;

    skeleton->freeze();

    mare::internal::mare_shared_ptr<mare::internal::pipeline_instance<UserData...>> pinst(
      new mare::internal::pipeline_instance<UserData...>(_skeleton));

    auto pipelineinst = c_ptr(pinst);
    pipelineinst->launch(context_data...,
                         num_iterations,
                         skeleton->get_launch_type());

    try {
      pipelineinst->wait_for();
    } catch (...)  { }
  }

  mare::task_ptr<void(UserData*..., size_t)>
  create_task() const {

    auto skeleton = c_ptr(_skeleton);

    MARE_API_ASSERT(skeleton->is_empty() == false, "The pipeline has no stages.");

    skeleton->freeze();
    mare::internal::mare_shared_ptr<mare::internal::pipeline_instance<UserData...>> pinst(
      new mare::internal::pipeline_instance<UserData...>(_skeleton));

    auto launch_type = skeleton->get_launch_type();

    auto ptask = mare::create_task([pinst, launch_type](UserData*... context_data,
                                                        size_t num_iterations) {
      auto pipelineinst = c_ptr(pinst);
      pipelineinst->launch(context_data..., num_iterations, launch_type);
      pipelineinst->finish_after();
    });

    return ptask;
  }

  mare::task_ptr<>
  create_task(UserData*... context_data,
              size_t num_iterations) const {

    auto skeleton = c_ptr(_skeleton);
    MARE_API_ASSERT(skeleton->is_empty() == false, "The pipeline has no stages.");

    skeleton->freeze();

    mare::internal::mare_shared_ptr<mare::internal::pipeline_instance<UserData...>> pinst(
      new mare::internal::pipeline_instance<UserData...>(_skeleton));

    auto datatp = std::make_tuple(context_data..., num_iterations);

    auto launch_type = skeleton->get_launch_type();

    using launch_helper =
      typename mare::internal::pipeline_utility::apply_launch<userdata_size + 1>;

    auto ptask = mare::create_task([pinst, datatp, launch_type]() {

      auto pipelineinst = c_ptr(pinst);
      launch_helper::apply(pipelineinst,
                           launch_type,
                           datatp);
      pipelineinst->finish_after();
    });

    return ptask;
  }

  bool is_valid() {
    auto skeleton = c_ptr(_skeleton);
    return skeleton->is_valid();
  }

  void disable_sliding_window() {
    auto skeleton = c_ptr(_skeleton);
    skeleton->set_launch_type(mare::internal::without_sliding_window);
  }

  void enable_sliding_window() {
    auto skeleton = c_ptr(_skeleton);
    skeleton->set_launch_type(mare::internal::with_sliding_window);
  }

  friend mare::internal::test::pipeline_tester;
  friend mare::internal::test::pipeline_capi_internal_tester;

#ifdef MARE_CAPI_COMPILING_CC
  friend int ::mare_pattern_pipeline_run(::mare_pattern_pipeline_ptr p, void* data, unsigned int iterations);
#endif

};

};

mare::task_ptr<>
create_task(const mare::pattern::pipeline<>& p,
            size_t num_iterations);

mare::task_ptr<void(size_t)>
create_task(const mare::pattern::pipeline<>& p);

void launch(const mare::pattern::pipeline<>& p,
            size_t num_iterations);

template<typename UserData>
mare::task_ptr<>
create_task(const mare::pattern::pipeline<UserData>& p,
            UserData* context_data,
            size_t num_iterations) {
  return p.create_task(context_data, num_iterations);
}

template<typename UserData>
mare::task_ptr<void(UserData*, size_t)>
create_task(const mare::pattern::pipeline<UserData>& p) {
  return p.create_task();
}

template<typename UserData>
void launch(const mare::pattern::pipeline<UserData>& p,
            UserData* context_data,
            size_t num_iterations) {
  p.run(context_data, num_iterations);
}
};

#ifdef MARE_HAVE_GPU
namespace mare {
namespace beta {
namespace pattern {

template<typename ...UserData>
class pipeline : public mare::pattern::pipeline<UserData...> {

  static constexpr size_t userdata_size = sizeof...(UserData);
  static_assert(userdata_size <= 1,
                "MARE pipeline can only have, at most, one type of context data.");
  using parent_type = typename mare::pattern::pipeline<UserData...>;

  template<typename... Args>
  void add_gpu_stage(Args&&... args) {
    using check_gpu_kernel = typename mare::internal::pipeline_utility::check_gpu_kernel<Args...>;
    static_assert(check_gpu_kernel::has_gpu_kernel,
      "gpu stage should have a gpu kernel");

    auto input_tuple      = std::forward_as_tuple(std::forward<Args>(args)...);

    using check_params =
      typename mare::internal::pipeline_utility::check_add_gpu_stage_params<
        context&, decltype(input_tuple), Args...>;
    static_assert(check_params::value, "add_gpu_stage has illegal parameters");

    int const stage_index = check_params::stage_index;
    int const lag_index   = check_params::result::iteration_lag_index;
    int const rate_index  = check_params::result::iteration_rate_index;
    int const sws_index   = check_params::result::sliding_window_size_index;
    bool const has_iteration_lag =
      check_params::result::iteration_lag_num == 0 ? false : true;
    using StageType       = typename check_params::StageType;

    mare::iteration_lag       lag(0);
    mare::iteration_rate      rate(1, 1);
    mare::sliding_window_size sws(0);
    auto default_tuple    = std::forward_as_tuple(lag, rate, sws);

    using lag_mux =
      typename mare::internal::pipeline_utility::mux_param_value<mare::iteration_lag,
                                                                 decltype(input_tuple),
                                                                 decltype(default_tuple),
                                                                 lag_index,
                                                                 param_list_type::mare_iteration_lag>;
    using rate_mux =
      typename mare::internal::pipeline_utility::mux_param_value<mare::iteration_rate,
                                                                 decltype(input_tuple),
                                                                 decltype(default_tuple),
                                                                 rate_index,
                                                                 param_list_type::mare_iteration_rate>;
    using sws_mux =
      typename mare::internal::pipeline_utility::mux_param_value<mare::sliding_window_size,
                                                                 decltype(input_tuple),
                                                                 decltype(default_tuple),
                                                                 sws_index,
                                                                 param_list_type::mare_sliding_window_size>;

    const auto bbody_index  = check_params::bbody_index;
    using BeforeBody        = typename check_params::BeforeBody;

    const auto abody_index  = check_params::abody_index;
    using AfterBody         = typename check_params::AfterBody;

    const auto gkbody_index = check_params::gkbody_index;
    using GKBody            = typename check_params::GKBody;

    static_assert(gkbody_index != -1, "gpu stage should have a gpu kernel");

    c_ptr(parent_type::_skeleton)->template add_gpu_stage<StageType, BeforeBody, GKBody, AfterBody>(
      std::get<stage_index>(input_tuple),
      std::forward<BeforeBody>(mare::internal::pipeline_utility::get_tuple_element_helper<bbody_index,
                           decltype(input_tuple)>::get(input_tuple)),
      std::forward<GKBody>(mare::internal::pipeline_utility::get_tuple_element_helper<gkbody_index,
                           decltype(input_tuple)>::get(input_tuple)),
      std::forward<AfterBody>(mare::internal::pipeline_utility::get_tuple_element_helper<abody_index,
                           decltype(input_tuple)>::get(input_tuple)),
      has_iteration_lag,
      lag_mux::get(input_tuple, default_tuple),
      rate_mux::get(input_tuple, default_tuple),
      sws_mux::get(input_tuple, default_tuple));
  }

public:

  using context = typename parent_type::context;

  pipeline() : parent_type() {}

  virtual ~pipeline() {}

  pipeline(pipeline const& other) : parent_type(other) {}

  pipeline(pipeline && other) :
    parent_type(std::forward<parent_type>(other)) {}

  pipeline& operator=(pipeline const& other) {
    parent_type::operator=(other);
    return *this;
  }

  pipeline& operator=(pipeline&& other) {
    parent_type::operator=(std::forward<parent_type>(other));
    return *this;
  }

  template<typename... Args>
  typename std::enable_if<
             !internal::pipeline_utility::check_gpu_kernel<
               Args...
             >::has_gpu_kernel, void
            >::type
  add_stage(Args&&... args)  {
    parent_type::add_cpu_stage(std::forward<Args>(args)...);
  }

  template<typename... Args>
  typename std::enable_if<
             internal::pipeline_utility::check_gpu_kernel<
               Args...
             >::has_gpu_kernel, void
            >::type
  add_stage(Args&&... args)  {
    add_gpu_stage(std::forward<Args>(args)...);
  }

  friend mare::internal::test::pipeline_tester;

};

};
};
};
#endif
