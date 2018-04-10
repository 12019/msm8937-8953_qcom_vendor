// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef MARE_HAVE_GPU

#include <mare/range.hh>

#include <mare/internal/legacy/gpukernel.hh>
#include <mare/internal/legacy/group.hh>
#include <mare/internal/legacy/task.hh>

namespace mare {
namespace internal {

template<size_t Dims, typename Body>
inline ::mare::internal::task* api20_create_ndrange_task(const mare::range<Dims>& global_range,
                                                         const mare::range<Dims>& local_range,
                                                         Body&& body)
{
  typedef ::mare::internal::legacy::body_wrapper<Body> wrapper;
#ifdef MARE_HAVE_OPENCL

  MARE_INTERNAL_ASSERT(!::mare::internal::s_dev_ptrs->empty(),
                       "No OpenCL GPU devices found on the platform");
  ::mare::internal::cldevice *d_ptr = ::mare::internal::c_ptr(::mare::internal::s_dev_ptrs->at(0));
  MARE_INTERNAL_ASSERT(d_ptr != nullptr, "null device_ptr");
  mare::internal::legacy::device_ptr device = (*::mare::internal::s_dev_ptrs)[0];
#else

  mare::internal::legacy::device_ptr device;
#endif

  for(auto i : global_range.stride())
    MARE_API_ASSERT( (i== 1), "GPU global ranges must be unit stride");
  for(auto i : local_range.stride())
    MARE_API_ASSERT( (i== 1), "GPU local ranges must be unit stride");
  ::mare::internal::task* t = wrapper::create_task(device,
                                                   global_range,
                                                   local_range,
                                                   std::forward<Body>(body));

  return t;
}

template<size_t Dims, typename Body>
::mare::internal::task* api20_create_ndrange_task(const mare::range<Dims>& global_range,
                                                         Body&& body)
{
  mare::range<Dims> local_range;
  return api20_create_ndrange_task(global_range, local_range,
                                   std::forward<Body>(body));
}

template<typename ...Args>
class gpu_kernel_implementation
{

  ::mare::internal::legacy::kernel_ptr<Args...>  _kernel;

  template<typename Code, class Enable>
  friend
  struct ::mare::internal::task_factory_dispatch;

protected:
  gpu_kernel_implementation(std::string const& kernel_str,
                          std::string const& kernel_name,
                          std::string const& build_options) :
    _kernel(::mare::internal::legacy::create_kernel<Args...>(kernel_str, kernel_name, build_options))
  {}

  explicit gpu_kernel_implementation(std::string const& gl_kernel_str) :
    _kernel(::mare::internal::legacy::create_kernel<Args...>(gl_kernel_str))
  {}

  gpu_kernel_implementation(void const* kernel_bin,
                            size_t kernel_len,
                            std::string const& kernel_name,
                            std::string const& build_options) :
    _kernel(::mare::internal::legacy::create_kernel<Args...>(kernel_bin,
                                                             kernel_len,
                                                             kernel_name,
                                                             build_options))
  {}

  MARE_DEFAULT_METHOD(gpu_kernel_implementation(gpu_kernel_implementation const&));
  MARE_DEFAULT_METHOD(gpu_kernel_implementation& operator=(gpu_kernel_implementation const&));
  MARE_DEFAULT_METHOD(gpu_kernel_implementation(gpu_kernel_implementation&&));
  MARE_DEFAULT_METHOD(gpu_kernel_implementation& operator=(gpu_kernel_implementation&&));

  template<typename TaskParam1, typename ...TaskParams>
  mare::internal::task*
  create_task_helper_with_global_range(TaskParam1 const& global_range,
                                       TaskParams&&... task_params) const
  {
    auto attrs = mare::internal::legacy::create_task_attrs(mare::internal::legacy::attr::gpu);
    auto gpu_task = api20_create_ndrange_task(global_range,
                                              mare::internal::legacy::with_attrs(attrs,
                                                               _kernel,
                                                               std::forward<TaskParams>(task_params)...));
    return gpu_task;
  }

  template<typename TaskParam1, typename TaskParam2, typename ...TaskParams>
  mare::internal::task*
  create_task_helper_with_global_and_local_ranges(TaskParam1 const& global_range,
                                                  TaskParam2 const& local_range,
                                                  TaskParams&&... task_params) const
  {
    auto attrs = mare::internal::legacy::create_task_attrs(mare::internal::legacy::attr::gpu);
    auto gpu_task = api20_create_ndrange_task(global_range,
                                              local_range,
                                              mare::internal::legacy::with_attrs(attrs,
                                                               _kernel,
                                                               std::forward<TaskParams>(task_params)...));
    return gpu_task;
  }

  ~gpu_kernel_implementation()
  {}
};

};
};

#endif
