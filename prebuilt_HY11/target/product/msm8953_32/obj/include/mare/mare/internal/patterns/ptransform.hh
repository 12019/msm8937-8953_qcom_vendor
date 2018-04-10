// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/patterns/common.hh>

namespace mare {

namespace internal {

template <typename InputIterator, typename OutputIterator, typename UnaryFn>
void ptransform_static(group_shared_ptr group,
                       InputIterator first,
                       InputIterator last,
                       OutputIterator d_first,
                       UnaryFn fn,
                       const mare::pattern::tuner& tuner) {

  if (first >= last)
    return;

  auto g_internal = legacy::create_group("ptransform/4");
  auto g = g_internal;
  if (group != nullptr)
    g = legacy::intersect(g, group);

  size_t const dist = std::distance(first,last);
  auto blksz = internal::static_chunk_size(dist, tuner.get_doc());
  MARE_INTERNAL_ASSERT(blksz > 0, "block size should be greater than zero");
  auto lb = first;
  auto d_lb = d_first;
  while (lb < last) {
    auto safeofs = std::min(size_t(std::distance(lb,last)), blksz);
    InputIterator rb = lb + safeofs;

    launch_or_exec(rb != last, g, [=] {
        InputIterator it = lb;
        OutputIterator d_it = d_lb;
        while (it < rb)
          *d_it++ = fn(*it++);
      });
    lb = rb;
    d_lb += safeofs;
  }

  legacy::spin_wait_for(g_internal);
}

template <typename InputIterator, typename OutputIterator, typename BinaryFn>
void ptransform_static(group_shared_ptr group,
                       InputIterator first1,
                       InputIterator last1,
                       InputIterator first2,
                       OutputIterator d_first,
                       BinaryFn fn,
                       const mare::pattern::tuner& tuner) {
  if (first1 >= last1)
    return;
  auto g_internal = legacy::create_group("ptransform/5");
  auto g = g_internal;
  if (group != nullptr)
    g = legacy::intersect(g, group);

  size_t const dist = std::distance(first1,last1);
  auto blksz = internal::static_chunk_size(dist, tuner.get_doc());
  MARE_INTERNAL_ASSERT(blksz > 0, "block size should be greater than zero");
  auto lb = first1;
  auto lb2 = first2;
  auto d_lb = d_first;
  while (lb < last1) {
    auto safeofs = std::min(size_t(std::distance(lb,last1)), blksz);
    InputIterator rb = lb + safeofs;

    launch_or_exec(rb != last1, g, [=] {
        InputIterator it = lb;
        InputIterator it2 = lb2;
        OutputIterator d_it = d_lb;
        while (it < rb)
          *d_it++ = fn(*it++, *it2++);
      });
    lb = rb;
    lb2 += safeofs;
    d_lb += safeofs;
  }

  legacy::spin_wait_for(g_internal);
}

template<typename InputIterator, typename OutputIterator, typename UnaryFn>
void ptransform_dynamic(group_shared_ptr group,
                        InputIterator first,
                        InputIterator last,
                        OutputIterator d_first,
                        UnaryFn fn,
                        const mare::pattern::tuner& tuner)
{
  auto new_fn = [first, d_first, fn] (size_t i) {
    *(d_first + i) = fn(*(first + i));
  };

  internal::pfor_each_dynamic(group, 0, std::distance(first, last), new_fn, 1, tuner);
}

template<typename InputIterator, typename OutputIterator, typename BinaryFn>
void ptransform_dynamic(group_shared_ptr group,
                        InputIterator first1,
                        InputIterator last1,
                        InputIterator first2,
                        OutputIterator d_first,
                        BinaryFn fn,
                        const mare::pattern::tuner& tuner)
{
  auto new_fn = [first1, first2, d_first, fn] (size_t i) {
    *(d_first + i) = fn(*(first1 + i), *(first2 + i));
  };

  internal::pfor_each_dynamic(group, 0, std::distance(first1, last1), new_fn, 1, tuner);
}

template <typename InputIterator, typename OutputIterator, typename UnaryFn>
void ptransform(group_shared_ptr group,
                InputIterator first, InputIterator last,
                OutputIterator d_first, UnaryFn&& fn,
                const mare::pattern::tuner& tuner) {
  if (tuner.is_serial()) {
    InputIterator  it = first;
    OutputIterator d_it = d_first;
    while (it != last) {
      *(d_it++) = fn(*it++);
    }

    return ;
  }

  if (tuner.is_static()) {
    internal::ptransform_static(group, first, last, d_first, fn, tuner);
    return ;
  }

  internal::ptransform_dynamic(group, first, last, d_first, fn, tuner);
}

template <typename InputIterator, typename OutputIterator, typename UnaryFn>
void ptransform(int, InputIterator, InputIterator, OutputIterator,
                UnaryFn&&, const mare::pattern::tuner&) = delete;

template <typename InputIterator, typename OutputIterator, typename BinaryFn>
void ptransform(group_shared_ptr group,
                InputIterator first1, InputIterator last1,
                InputIterator first2, OutputIterator d_first,
                BinaryFn&& fn, const mare::pattern::tuner& tuner) {
  if (tuner.is_serial()) {
    InputIterator  it1 = first1;
    InputIterator  it2 = first2;
    OutputIterator d_it = d_first;
    while (it1 != last1) {
      *(d_it++) = fn(*it1++, *it2++);
    }

    return ;
  }

  if (tuner.is_static()) {
    internal::ptransform_static(group, first1, last1, first2, d_first, fn, tuner);
    return ;
  }

  internal::ptransform_dynamic(group, first1, last1, first2, d_first, fn, tuner);
}

template <typename InputIterator, typename OutputIterator, typename BinaryFn>
void ptransform(int, InputIterator, InputIterator, InputIterator,
                OutputIterator, BinaryFn&&, const mare::pattern::tuner&) = delete;

template <typename InputIterator, typename UnaryFn>
void ptransform(group_shared_ptr group,
                InputIterator first, InputIterator last,
                UnaryFn fn, const mare::pattern::tuner& tuner) {
  if (tuner.is_serial()) {
    for (InputIterator it = first; it != last; ++it) {
      fn(*it);
    }

    return ;
  }

  if (tuner.is_static()) {
    internal::pfor_each_static(group, first, last,
      [fn] (InputIterator it) { fn(*it); }, 1, tuner);
    return ;
  }

  internal::pfor_each_dynamic(group, 0, last - first,
    [first, fn] (size_t i) { fn(*(first + i)); }, 1, tuner);
}

template <typename InputIterator, typename UnaryFn>
void ptransform(int, InputIterator, InputIterator, UnaryFn&&,
                const mare::pattern::tuner&) = delete;

template <typename InputIterator, typename OutputIterator, typename UnaryFn>
mare::task_ptr<void()>
ptransform_async(UnaryFn&& fn, InputIterator first, InputIterator last,
                OutputIterator d_first,
                const mare::pattern::tuner& tuner = mare::pattern::tuner()) {

  auto g = legacy::create_group();
  auto t = mare::create_task([g, first, last, d_first, fn, tuner]{
      internal::ptransform(g, first, last, d_first, fn, tuner);
      legacy::finish_after(g);
  });
  auto gptr = internal::c_ptr(g);
  gptr->set_representative_task(internal::c_ptr(t));
  return t;
}

template <typename InputIterator, typename OutputIterator, typename BinaryFn>
mare::task_ptr<void()>
ptransform_async(BinaryFn&& fn,
                InputIterator first1, InputIterator last1,
                InputIterator first2,
                OutputIterator d_first,
                const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  auto g = legacy::create_group();
  auto t = mare::create_task([g, first1, last1, first2, d_first, fn, tuner]{
      internal::ptransform(g, first1, last1, first2, d_first, fn, tuner);
      legacy::finish_after(g);
  });
  auto gptr = internal::c_ptr(g);
  gptr->set_representative_task(internal::c_ptr(t));
  return t;
}

template <typename InputIterator, typename UnaryFn>
mare::task_ptr<void()>
ptransform_async(UnaryFn&& fn, InputIterator first, InputIterator last,
    const mare::pattern::tuner& tuner) {
  auto g = legacy::create_group();
  auto t = mare::create_task([g, first, last, fn, tuner]{
      internal::ptransform(g, first, last, fn, tuner);
      legacy::finish_after(g);
  });
  auto gptr = internal::c_ptr(g);
  gptr->set_representative_task(internal::c_ptr(t));
  return t;
}

};
};
