// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/pfor_each.hh>
#include <mare/taskfactory.hh>
#include <mare/tuner.hh>

#include <mare/internal/patterns/ptransform.hh>

namespace mare {
namespace pattern {

template <typename Fn> class ptransformer;

template <typename Fn>
ptransformer<Fn> create_ptransform(Fn&& fn);

template <typename Fn>
class ptransformer {
public:
  template <typename InputIterator>
  void run(InputIterator first,
           InputIterator last,
           const mare::pattern::tuner& t = mare::pattern::tuner()) {
    internal::ptransform(nullptr, first, last, std::forward<Fn>(_fn), t);
  }

  template <typename InputIterator>
  void operator()(InputIterator first,
                  InputIterator last,
                  const mare::pattern::tuner& t = mare::pattern::tuner()) {
    run(first, last, t);
  }

  template <typename InputIterator, typename OutputIterator>
  void run(InputIterator first, InputIterator last, OutputIterator d_first,
           const mare::pattern::tuner& t = mare::pattern::tuner()) {
    internal::ptransform(nullptr, first, last, d_first, std::forward<Fn>(_fn), t);
  }

  template <typename InputIterator, typename OutputIterator>
  void operator()(InputIterator first, InputIterator last,
                  OutputIterator d_first,
                  const mare::pattern::tuner& t = mare::pattern::tuner()) {
    run(first, last, d_first, t);
  }

  template <typename InputIterator1, typename InputIterator2,
            typename OutputIterator>
  void run(InputIterator1 first1,
           InputIterator1 last1,
           InputIterator2 first2,
           OutputIterator d_first,
           const mare::pattern::tuner& t = mare::pattern::tuner()) {
    internal::ptransform(nullptr, first1, last1, first2, d_first, std::forward<Fn>(_fn), t);
  }

  template <typename InputIterator1, typename InputIterator2,
            typename OutputIterator>
  void operator()(InputIterator1 first1,
                  InputIterator1 last1,
                  InputIterator2 first2,
                  OutputIterator d_first,
                  const mare::pattern::tuner& t = mare::pattern::tuner()) {
    run(first1, last1, first2, d_first, t);
  }

private:
  Fn _fn;
  explicit ptransformer(Fn&& fn) : _fn(fn) {}
  friend ptransformer create_ptransform<Fn>(Fn&& fn);
  template<typename F, typename ...Args>
  friend mare::task_ptr<void()> mare::create_task(const ptransformer<F>& ptf, Args&&...args);
};

template <typename Fn>
ptransformer<Fn>
create_ptransform(Fn&& fn) {
  using traits = internal::function_traits<Fn>;

  static_assert(traits::arity::value == 1 || traits::arity::value == 2,
      "ptransform takes a function accepting either one or two arguments");

  return ptransformer<Fn>(std::forward<Fn>(fn));
}

};

template <typename InputIterator, typename OutputIterator, typename UnaryFn>

typename std::enable_if<!std::is_same<mare::pattern::tuner, typename std::remove_reference<UnaryFn>
                        ::type>::value, void>::type
ptransform(InputIterator first,
           InputIterator last,
           OutputIterator d_first,
           UnaryFn&& fn,
           const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  internal::ptransform(nullptr, first, last, d_first, std::forward<UnaryFn>(fn), tuner);
}

template <typename InputIterator, typename OutputIterator, typename UnaryFn>
mare::task_ptr<void()>
ptransform_async(InputIterator first, InputIterator last,
                 OutputIterator d_first, UnaryFn&& fn,
                 const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  return
    mare::internal::ptransform_async(std::forward<UnaryFn>(fn), first, last, d_first, tuner);
}

template <typename InputIterator, typename OutputIterator, typename BinaryFn>

typename std::enable_if<!std::is_same<mare::pattern::tuner, typename std::remove_reference<BinaryFn>
                        ::type>::value, void>::type
ptransform(InputIterator first1,
           InputIterator last1,
           InputIterator first2,
           OutputIterator d_first,
           BinaryFn&& fn,
           const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  internal::ptransform(nullptr, first1, last1, first2, d_first, std::forward<BinaryFn>(fn), tuner);
}

template <typename InputIterator, typename OutputIterator, typename BinaryFn>
mare::task_ptr<void()>
ptransform_async(InputIterator first1, InputIterator last1,
                 InputIterator first2,
                 OutputIterator d_first, BinaryFn&& fn,
                 const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  return
    mare::internal::ptransform_async(std::forward<BinaryFn>(fn), first1, last1, first2, d_first, tuner);
}

template <typename InputIterator, typename UnaryFn>
void ptransform(InputIterator first, InputIterator last, UnaryFn&& fn,
                const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  internal::ptransform(nullptr, first, last, std::forward<UnaryFn>(fn), tuner);
}

template <typename InputIterator, typename UnaryFn>
mare::task_ptr<void()>
ptransform_async(InputIterator first, InputIterator last, UnaryFn&& fn,
                 const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  return mare::internal::ptransform_async(std::forward<UnaryFn>(fn), first, last, tuner);
}

template <typename Fn, typename ...Args>
mare::task_ptr<void()>
create_task(const mare::pattern::ptransformer<Fn>& ptf, Args&&...args) {
  return mare::internal::ptransform_async(ptf._fn, args...);
}

template <typename Fn, typename ...Args>
mare::task_ptr<void()>
launch(const mare::pattern::ptransformer<Fn>& ptf, Args&&...args) {
  auto t = mare::create_task(ptf, args...);
  t->launch();
  return t;
}

};
