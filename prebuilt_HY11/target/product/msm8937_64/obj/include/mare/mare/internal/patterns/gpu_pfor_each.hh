// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef MARE_HAVE_OPENCL

#include <mare/range.hh>

namespace mare {

namespace internal{

template <typename BWAG>
void pfor_each_gpu(group_shared_ptr group, size_t first, size_t last, BWAG bwag) {

  if (first >= last)
    return;

  auto g_internal = legacy::create_group("pfor_each_gpu");
  auto g = g_internal;
  if (group != nullptr)
    g = legacy::intersect(g, group);

  auto g_ptr = c_ptr(group);
  if (g_ptr && g_ptr->is_canceled())
    return;

  mare::range<1> r(first, last);
  mare::range<1> l;
  pfor_each_gpu(g, r, l, bwag);
}

template <size_t Dims, typename BWAG>
void pfor_each_gpu(group_shared_ptr, const mare::range<Dims>& r,
                   const mare::range<Dims>& l, BWAG bwag)
{
  typedef typename BWAG::kernel_parameters k_params;
  typedef typename BWAG::kernel_arguments k_args;

  auto d_ptr = c_ptr((*internal::s_dev_ptrs)[0]);
  auto k_ptr = c_ptr(bwag.get_gpu_kernel());
  clkernel* kernel = k_ptr->get_cl_kernel();

  char dummy;
  void* requestor = &dummy;

  buffer_acquire_set< num_buffer_ptrs_in_tuple<k_args>::value > bas;

  gpu_kernel_dispatch::
    prepare_args<k_params, k_args>((*s_dev_ptrs)[0],
                                   k_ptr,
                                   std::forward<k_args>(
                                     bwag.get_cl_kernel_args()),
                                   bas,
                                   requestor);

  clevent event = d_ptr->launch_kernel(kernel, r, l);
  event.wait();

  bas.release_buffers(requestor);

#ifdef MARE_HAVE_OPENCL_PROFILING
    MARE_ALOG("cl::CommandQueue::enqueueNDRangeKernel() time = %0.6f us", event.get_time_ns() / 1000.0);
#endif
}

};
};

#endif
