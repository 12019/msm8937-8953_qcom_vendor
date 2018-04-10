// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/patterns/cpu_pfor_each.hh>
#include <mare/internal/patterns/gpu_pfor_each.hh>
#include <mare/internal/patterns/utility.hh>

namespace mare {

namespace internal {

template<class InputIterator, typename UnaryFn, typename T>
struct pfor_each_dispatcher;

template<typename UnaryFn>
struct pfor_each_dispatcher<size_t, UnaryFn, std::true_type>
{

  static void dispatch(group_shared_ptr group, size_t first, size_t last,
                       UnaryFn&& fn, const size_t,
                       const mare::pattern::tuner&) {
    pfor_each_gpu(group, first, last, std::forward<UnaryFn>(fn));
  }

  template<size_t Dims>
  static void dispatch(group_shared_ptr group, const mare::range<Dims>& r,
                       UnaryFn&& fn, const mare::pattern::tuner&) {
    for(auto i : r.stride())
      MARE_API_ASSERT( (i== 1), "GPU ranges must be unit stride");

    mare::range<Dims> l;
    pfor_each_gpu(group, r, l, std::forward<UnaryFn>(fn));
  }
};

template<typename UnaryFn, typename IterType>
void func_impl(const size_t& idx, IterType first, UnaryFn&& fn, std::true_type)
{
  fn(static_cast<IterType>(idx) + first);
}

template<typename UnaryFn, class IterType>
void func_impl(const size_t& idx, IterType first, UnaryFn&& fn, std::false_type)
{
  fn(idx + first);
}

template<class InputIterator, typename UnaryFn>
struct pfor_each_dispatcher<InputIterator, UnaryFn, std::false_type>
{
  static void dispatch(group_shared_ptr group, InputIterator first,
                       InputIterator last, UnaryFn&& fn, const size_t stride,
                       const mare::pattern::tuner& t)
  {
    if (t.is_serial()) {
      for (InputIterator idx = first; idx < last; idx += stride) {
        fn(idx);
      }
    }

    else if (t.is_static()) {
      pfor_each_static
        (group, first, last, std::forward<UnaryFn>(fn), stride, t);
    }

    else{
      auto fn_transform = [fn, first](size_t i) mutable {

        func_impl(i, first,
                  std::forward<UnaryFn>(fn),
                  std::is_integral<InputIterator>());
      };

      pfor_each_dynamic(group, size_t(0), size_t(last - first),
                        std::forward<decltype(fn_transform)>(fn_transform),
                        stride, t);
    }
  }
};

template<typename UnaryFn>
struct pfor_each_dispatcher<size_t, UnaryFn, std::false_type>
{

  static void dispatch(group_shared_ptr group, size_t first, size_t last,
                       UnaryFn&& fn, const size_t stride,
                       const mare::pattern::tuner& t) {

    if (t.is_serial()) {
      for (size_t idx = first; idx < last; idx += stride) {
        fn(idx);
      }
    }

    else if (t.is_static()) {
      pfor_each_static
        (group, first, last, std::forward<UnaryFn>(fn), stride, t);
    }

    else {
      pfor_each_dynamic
        (group, first, last, std::forward<UnaryFn>(fn), stride, t);
    }
  }

  template<size_t Dims>
  static void dispatch(group_shared_ptr group,
                       const mare::range<Dims>& r,
                       UnaryFn&& fn,
                       const mare::pattern::tuner& t) {
   pfor_each_range<Dims, UnaryFn>::pfor_each_range_impl(group, r, std::forward<UnaryFn>(fn), t);
  }
};

template <class InputIterator, typename UnaryFn>
void
pfor_each_internal(group_shared_ptr group, InputIterator first, InputIterator last,
                   UnaryFn&& fn, const size_t stride,
                   const mare::pattern::tuner& t = mare::pattern::tuner())
{

  internal::pfor_each_dispatcher<InputIterator, UnaryFn,
    typename std::is_base_of<legacy::body_with_attrs_base_gpu, UnaryFn>::type>::
    dispatch(group, first, last, std::forward<UnaryFn>(fn), stride, t);
}

template<size_t Dims, typename UnaryFn>
void pfor_each_internal(group_shared_ptr group, const mare::range<Dims>& r,
                        UnaryFn&& fn, const mare::pattern::tuner& t)
{

  internal::pfor_each_dispatcher<size_t, UnaryFn,
    typename std::is_base_of<legacy::body_with_attrs_base_gpu, UnaryFn>::type>::
    dispatch(group, r, std::forward<UnaryFn>(fn), t);
}

template <class InputIterator, typename UnaryFn>
void
pfor_each(group_shared_ptr group, InputIterator first, InputIterator last,
          UnaryFn&& fn, const size_t stride = 1,
          const mare::pattern::tuner& t = mare::pattern::tuner())
{
  internal::pfor_each_internal
    (group, first, last, std::forward<UnaryFn>(fn), stride, t);
}

template <class InputIterator, typename UnaryFn>
void pfor_each(int, InputIterator, InputIterator, UnaryFn) = delete;

template<size_t Dims, typename UnaryFn>
void pfor_each(group_shared_ptr group, const mare::range<Dims>& r,
               UnaryFn&& fn, const mare::pattern::tuner& t)
{
  internal::pfor_each_internal(group, r, std::forward<UnaryFn>(fn), t);
}

template <class InputIterator, typename UnaryFn>
mare::task_ptr<void()>
pfor_each_async(UnaryFn fn,
                InputIterator first,
                InputIterator last,
                const size_t stride = 1,
                const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  auto g = legacy::create_group();
  auto t = mare::create_task([g, first, last, fn, stride, tuner]{
      internal::pfor_each(g, first, last, fn, stride, tuner);
      legacy::finish_after(g);
  });
  auto gptr = internal::c_ptr(g);
  gptr->set_representative_task(internal::c_ptr(t));
  return t;
}

template <size_t Dims, typename UnaryFn>
mare::task_ptr<void()>
pfor_each_async(UnaryFn fn,
                const mare::range<Dims>& r,
                const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  auto g = legacy::create_group();
  auto t = mare::create_task([g, r, fn, tuner]{
      internal::pfor_each(g, r, fn, tuner);
      legacy::finish_after(g);
  });
  auto gptr = internal::c_ptr(g);
  gptr->set_representative_task(internal::c_ptr(t));
  return t;
}

#if defined(MARE_HAVE_GPU) && defined(MARE_HAVE_QTI_HEXAGON)

inline int stride_round_up(const int src_idx, const int stride)
{
  return (src_idx / stride + 1) * stride;
}

MARE_CONSTEXPR size_t invalid_pos     = std::numeric_limits<size_t>::max();

MARE_CONSTEXPR size_t num_dsp_threads = 1;

template <size_t pos, typename T, typename Tuple, typename... Args>
struct gpu_exec {
  static void kernel_launch(mare::group_ptr& g, Tuple& klist,
                            const size_t& gpu_num_iters,
                            mare::buffer_ptr<T>& gpu_buffer, Args&&... args)
  {
    MARE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group!");

    auto gpu_kernel = std::get<pos>(klist);
    auto gpu_range  = mare::range<1>(gpu_num_iters);
    g->launch(gpu_kernel, gpu_range, pattern::utility::arg_transform(std::forward<Args>(args), gpu_buffer)...);
  }
};

template <typename T, typename Tuple, typename... Args>
struct gpu_exec<invalid_pos, T, Tuple, Args...> {
  static void kernel_launch(mare::group_ptr&, Tuple&, const size_t&, mare::buffer_ptr<T>&, Args&&...)
  {}
};

template <size_t pos, typename T, typename Tuple, typename... Args>
struct hexagon_exec {
  static void kernel_launch(mare::group_ptr& g, Tuple& klist,
                            const size_t& stride, const size_t& dsp_num_iters,
                            mare::buffer_ptr<T>* dsp_buffer,
                            const size_t& dsp_begin, const size_t& dsp_end,
                            size_t* dsp_begin_list, size_t* dsp_end_list,
                            const mare::pattern::tuner& tuner,
                            Args&&... args)
  {
    MARE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group!");

    auto hex_kernel = std::get<pos>(klist);

    if (!tuner.is_serial()) {
      *dsp_begin_list = dsp_begin;
      *dsp_end_list = dsp_end;
      g->launch(hex_kernel, dsp_begin, stride, dsp_end, pattern::utility::arg_transform(std::forward<Args>(args), *dsp_buffer)...);
    } else {
      size_t k = 0;
      size_t step_sz = static_cast<size_t>(dsp_num_iters / num_dsp_threads);

      size_t lb = dsp_begin;
      while (lb < dsp_end) {
        auto rb = lb + step_sz;
        rb = dsp_end - rb < step_sz ? dsp_end : rb;
        *(dsp_begin_list + k) = lb;
        *(dsp_end_list + k) = rb;
        g->launch(hex_kernel, lb, stride, rb, pattern::utility::arg_transform(std::forward<Args>(args), *(dsp_buffer + k))...);
        lb = rb % stride == 0 ? rb : stride_round_up(rb, stride);
        k++;
      }
    }
  }
};

template <typename T, typename Tuple, typename... Args>
struct hexagon_exec<invalid_pos, T, Tuple, Args...> {
  static void kernel_launch(mare::group_ptr&, Tuple&, const size_t&, const size_t&,
                            mare::buffer_ptr<T>*, const size_t, const size_t,
                            size_t*, size_t*, const mare::pattern::tuner&, Args&&...)
  {}
};

template <size_t pos, typename T, typename KernelTuple, typename... Args>
struct cpu_exec {
  static void kernel_launch(KernelTuple& klist,
                            mare::buffer_ptr<T>& cpu_buffer,
                            const size_t& stride, const size_t& cpu_begin, const size_t& cpu_end,
                            const mare::pattern::tuner& tuner, Args&&... args)
  {
    auto ck = std::get<pos>(klist);
    auto transformed_arg_list = std::forward_as_tuple(pattern::utility::arg_transform(std::forward<Args>(args), cpu_buffer)...);

    auto cpu_kernel = [ck, stride, &transformed_arg_list] (size_t idx) {
      pattern::utility::fn_call_wrapper(ck, idx, stride, idx + 1, transformed_arg_list);
    };
    pfor_each_dynamic(nullptr, cpu_begin, cpu_end, cpu_kernel, stride, tuner);
  }
};

template <typename T, typename KernelTuple, typename... Args>
struct cpu_exec<invalid_pos, T, KernelTuple, Args...> {
  static void kernel_launch(KernelTuple&, mare::buffer_ptr<T>&,
                            const size_t&, const size_t&, const size_t&,
                            const mare::pattern::tuner&, Args&&...)
  {}
};

template <size_t pos, size_t Dims, typename Tuple, typename... Args>
struct serial_exec {
  static void kernel_launch(const mare::range<Dims>& r, Tuple& klist, Args&&... args) {
    auto ck = std::get<pos>(klist);
    ck(r.begin(0), r.stride()[0], r.end(0), std::forward<Args>(args)...);
  }
};

template <size_t Dims, typename Tuple, typename... Args>
struct serial_exec<invalid_pos, Dims, Tuple, Args...> {
  static void kernel_launch(const mare::range<Dims>&, Tuple&, Args&&...) {
    MARE_ALOG("Serialized execution requires a valid CPU kernel in this version of MARE.");
  }
};

template <size_t Dims, typename KernelFirst, typename... KernelRest, typename... Args>
void pfor_each(const mare::range<Dims>& r,
               std::tuple<KernelFirst, KernelRest...>& klist,
               const mare::pattern::tuner& tuner,
               Args&&... args)
{
  using kernel_type = std::tuple<KernelFirst, KernelRest...>;

  size_t klist_size = std::tuple_size<kernel_type>::value;
  MARE_API_ASSERT(klist_size > 0 && klist_size <= 3,
                  "Improper number of kernels provided!");

  if (tuner.is_serial()) {
    const size_t ck_idx = mare::internal::pattern::utility::cpu_kernel_pos<kernel_type>::pos;
    serial_exec<ck_idx, Dims, kernel_type, Args...>::kernel_launch(r, klist, std::forward<Args>(args)...);
    return ;
  }

  using idx_type = size_t;
  using pct_type = float;

  const idx_type cpu_load = tuner.get_cpu_load();
  const idx_type gpu_load = tuner.get_gpu_load();
  const idx_type dsp_load = tuner.get_hexagon_load();

  MARE_API_ASSERT(cpu_load + gpu_load + dsp_load == 100,
                  "Incorrect load setting across devices!");

  idx_type first = r.begin(0);
  idx_type last  = r.end(0);

  idx_type stride = r.stride()[0];

  MARE_CONSTEXPR size_t ck_idx = pattern::utility::cpu_kernel_pos<kernel_type>::pos;
  MARE_CONSTEXPR size_t gk_idx = pattern::utility::gpu_kernel_pos<kernel_type>::pos;
  MARE_CONSTEXPR size_t hk_idx = pattern::utility::hexagon_kernel_pos<kernel_type>::pos;

  MARE_API_ASSERT(!(cpu_load > 0 && ck_idx == invalid_pos),
                  "CPU: kernel and tuner load mismatch!");
  MARE_API_ASSERT(!(gpu_load > 0 && gk_idx == invalid_pos),
                  "GPU: kernel and tuner load mismatch!");
  MARE_API_ASSERT(!(dsp_load > 0 && hk_idx == invalid_pos),
                  "Hexagon: Kernel and tuner load mismatch!");

  auto arg_list = std::make_tuple(std::forward<Args>(args)...);
  MARE_CONSTEXPR size_t num_buffer = pattern::utility::get_num_buffer<decltype(arg_list)>::value;
  auto num_output_buf = pattern::utility::num_output_buffer_in_tuple<decltype(arg_list)>::value;

  MARE_INTERNAL_ASSERT(num_output_buf < 2,
                       "Multiple output buffers are not supported.");

  pattern::utility::buffer_size_check(arg_list);

  MARE_CONSTEXPR int out_idx = pattern::utility::extract_output_buf<
                                  sizeof...(Args), decltype(arg_list)>::value;
  if (out_idx == invalid_pos)
    return ;

  auto output = std::get<out_idx>(arg_list);
  using buf_type = typename pattern::utility::buffer_data_type<decltype(output)>::data_type;

  buffer_acquire_set<num_buffer> bas;
  pattern::utility::process_buffer_in_args<sizeof...(Args), num_buffer, decltype(arg_list)>::add_buffer(bas, arg_list);

  size_t uid = 0;
  bas.acquire_buffers(&uid, executor_device::hexagon);

  idx_type size = output.size();
  idx_type nd_offset = static_cast<idx_type>(size / (last - first));

  pct_type gpu_fraction = pct_type(gpu_load) / 100.00;
  pct_type dsp_fraction = pct_type(dsp_load) / 100.00;

  auto gpu_num_iters = idx_type((last - first) * gpu_fraction);
  auto dsp_num_iters = idx_type((last - first) * dsp_fraction);

  auto gpu_begin = first;
  auto gpu_end = gpu_begin + gpu_num_iters;

  auto dsp_begin = gpu_end % stride == 0 ? gpu_end : stride_round_up(gpu_end, stride);
  auto dsp_end = dsp_begin + dsp_num_iters;

  auto cpu_begin = dsp_end % stride == 0 ? dsp_end : stride_round_up(dsp_end, stride);
  auto cpu_end = last;
  auto cpu_num_iters = cpu_end - cpu_begin;

  mare::buffer_ptr<buf_type> gpu_buffer;
  mare::buffer_ptr<buf_type> dsp_buffer[num_dsp_threads];
  mare::buffer_ptr<buf_type> cpu_buffer;

  idx_type dsp_begin_list[num_dsp_threads] = {0};
  idx_type dsp_end_list[num_dsp_threads] = {0};

  auto g = mare::create_group();

  if (gpu_num_iters > 0) {
    gpu_buffer = mare::create_buffer<buf_type>(size, true, {mare::gpu});
    gpu_exec<gk_idx, buf_type, kernel_type, Args...>::
      kernel_launch(g, klist, gpu_num_iters, gpu_buffer, std::forward<Args>(args)...);
  }

  if (dsp_num_iters > 0){

    for (size_t i = 0; i < num_dsp_threads; ++i) {
      dsp_buffer[i] = mare::create_buffer<buf_type>(size, true, {mare::hexagon});
    }

    hexagon_exec<hk_idx, buf_type, kernel_type, Args...>::
      kernel_launch(g, klist, stride, dsp_num_iters, dsp_buffer, dsp_begin, dsp_end,
                    dsp_begin_list, dsp_end_list, tuner, std::forward<Args>(args)...);
  }

  if (cpu_num_iters > 0) {
    cpu_buffer = mare::create_buffer<buf_type>(size, true, {mare::cpu});
    cpu_exec<ck_idx, buf_type, kernel_type, Args...>::
      kernel_launch(klist, cpu_buffer, stride, cpu_begin, cpu_end, tuner, std::forward<Args>(args)...);
  }

  g->wait_for();

  auto* acquired_arena = bas.find_acquired_arena(output);
  auto* output_arena = static_cast<ion_arena*>(acquired_arena);
  MARE_INTERNAL_ASSERT(output_arena != nullptr, "Invalid arena pointer!");
  auto* optr = static_cast<buf_type*>(output_arena->get_ptr());
  MARE_INTERNAL_ASSERT(optr != nullptr, "Unexpected null pointer!");

  size_t num_bytes = 0;

  if (gpu_num_iters > 0) {

    auto* gpu_buffer_ptr = static_cast<buf_type*>(gpu_buffer.get_host_accessible_data());
    num_bytes = gpu_num_iters * nd_offset * sizeof(buf_type);
    memcpy(optr, gpu_buffer_ptr, num_bytes);
  }

  if (dsp_num_iters > 0) {

    size_t iter_cnt = tuner.is_serial() ? num_dsp_threads : 1;

    for (size_t i = 0; i < iter_cnt; ++i) {
      auto* dsp_buffer_ptr = static_cast<buf_type*>(dsp_buffer[i].get_host_accessible_data());
      auto offset = dsp_begin_list[i] * nd_offset;
      num_bytes = (dsp_end_list[i] - dsp_begin_list[i]) * nd_offset * sizeof(buf_type);

      memcpy(optr + offset, dsp_buffer_ptr + offset, num_bytes);
    }
  }

  if (cpu_num_iters > 0) {
    auto* cpu_buffer_ptr = static_cast<buf_type*>(cpu_buffer.get_host_accessible_data());
    auto offset = cpu_begin * nd_offset;
    num_bytes = cpu_num_iters * nd_offset * sizeof(buf_type);
    memcpy(optr + offset, cpu_buffer_ptr + offset, num_bytes);
  }

  bas.release_buffers(&uid);

}
#endif

};
};
