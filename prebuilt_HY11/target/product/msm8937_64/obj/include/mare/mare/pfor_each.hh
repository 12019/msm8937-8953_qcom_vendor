// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/taskfactory.hh>
#include <mare/tuner.hh>

#include <mare/internal/patterns/pfor_each.hh>
#include <mare/internal/util/templatemagic.hh>

namespace mare {

namespace pattern {

template <typename T1, typename T2> class pfor;

template <typename UnaryFn>
pfor<UnaryFn, void> create_pfor_each(UnaryFn&& fn);

};

#if defined(MARE_HAVE_GPU) && defined(MARE_HAVE_QTI_HEXAGON)
namespace beta {

namespace pattern {

template<typename KernelTuple, typename ArgTuple>
mare::pattern::pfor<KernelTuple, ArgTuple>
create_pfor_each_helper(KernelTuple&& ktpl, ArgTuple&& atpl);

};

};
#endif

namespace pattern {

#if defined(MARE_HAVE_GPU) && defined(MARE_HAVE_QTI_HEXAGON)
template <typename T1, typename T2>
class pfor {
public:
  template <size_t Dims, size_t... Indices>
  void run_helper(const mare::range<Dims>& r,
                  T1& klist,
                  const mare::pattern::tuner& t,
                  mare::internal::integer_list_gen<Indices...>)
  {
    mare::internal::pfor_each(r, klist, t, std::get<Indices-1>(std::forward<T2>(_atpl))...);
  }

  template <size_t Dims>
  void run(const mare::range<Dims>& r,
           const mare::pattern::tuner& t = mare::pattern::tuner().set_cpu_load(100)) {
    run_helper(r, _ktpl, t, typename mare::internal::integer_list<std::tuple_size<decltype(_atpl)>::value>::type());
  }

  template <size_t Dims>
  void operator()(const mare::range<Dims>& r,
                  const mare::pattern::tuner& t = mare::pattern::tuner().set_cpu_load(100)) {
    run<Dims>(r, t);
  }

#ifndef _MSC_VER

private:
  template<typename KT, typename AT>
  friend pfor<KT, AT> mare::beta::pattern::create_pfor_each_helper(KT&& ktpl, AT&& atpl);
#else
public:
#endif

  T1 _ktpl;
  T2 _atpl;

  pfor(T1&& ktpl, T2&& atpl) : _ktpl(ktpl), _atpl(atpl) {}
};
#endif

template <typename T1>
class pfor<T1, void> {
public:
  template <typename InputIterator>
  void run(InputIterator first,
           InputIterator last,
           const mare::pattern::tuner& t = mare::pattern::tuner()) {
    mare::internal::pfor_each_internal
      (nullptr, first, last, std::forward<T1>(_fn), 1, t);
  }

  template <typename InputIterator>
  void operator()(InputIterator first,
                  InputIterator last,
                  const mare::pattern::tuner& t = mare::pattern::tuner()) {
    run(first, last, t);
  }

  template <typename InputIterator>
  void run(InputIterator first,
           const size_t stride,
           InputIterator last,
           const mare::pattern::tuner& t = mare::pattern::tuner()) {
    mare::internal::pfor_each_internal
      (nullptr, first, last, std::forward<T1>(_fn), stride, t);
  }

  template <typename InputIterator>
  void operator()(InputIterator first,
                  const size_t stride,
                  InputIterator last,
                  const mare::pattern::tuner& t = mare::pattern::tuner()) {
    run(first, stride, last, t);
  }

  template <size_t Dims>
  void run(const mare::range<Dims>& r,
           const mare::pattern::tuner& t = mare::pattern::tuner()) {
    mare::internal::pfor_each_internal
      (nullptr, r, std::forward<T1>(_fn), 1, t);
  }

  template <size_t Dims>
  void operator()(const mare::range<Dims>& r,
                  const mare::pattern::tuner& t = mare::pattern::tuner()) {
    run<Dims>(r, t);
  }

#ifndef _MSC_VER

private:
  friend pfor create_pfor_each<T1>(T1&& fn);

  template<typename Fn, typename... Args>
  friend mare::task_ptr<void()> mare::create_task(const pfor<Fn, void>& pf, Args&&...args);

#else
public:
#endif
  T1 _fn;
  explicit pfor(T1&& fn) : _fn(fn) {}

};

template <typename UnaryFn>
pfor<UnaryFn, void>
create_pfor_each(UnaryFn&& fn) {

  return pfor<UnaryFn, void>(std::forward<UnaryFn>(fn));
}

};

#if defined(MARE_HAVE_GPU) && defined(MARE_HAVE_QTI_HEXAGON)

namespace beta {

namespace pattern {

template<typename KernelTuple, typename ArgTuple>
mare::pattern::pfor<KernelTuple, ArgTuple>
create_pfor_each_helper(KernelTuple&& ktpl, ArgTuple&& atpl)
{
  return mare::pattern::pfor<KernelTuple, ArgTuple>
    (std::forward<KernelTuple>(ktpl), std::forward<ArgTuple>(atpl));
}

template<typename KernelTuple, typename... Args>
auto create_pfor_each(KernelTuple&& ktpl, Args&&... args)
  ->decltype(create_pfor_each_helper(std::forward<KernelTuple>(ktpl),
                                     std::forward<decltype(std::forward_as_tuple(args...))>(std::forward_as_tuple(args...)))) {

  auto arg_tuple = std::forward_as_tuple(args...);

  return create_pfor_each_helper(std::forward<KernelTuple>(ktpl),
                                 std::forward<decltype(arg_tuple)>(arg_tuple));
}

};

};
#endif

template <class InputIterator, typename UnaryFn>
mare::task_ptr<void()>
pfor_each_async(InputIterator first, InputIterator last,
                UnaryFn fn,
                const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  return mare::internal::pfor_each_async(fn, first, last, 1, tuner);
}

template <class InputIterator, typename UnaryFn>
mare::task_ptr<void()>
pfor_each_async(InputIterator first, const size_t stride,
                InputIterator last, UnaryFn fn,
                const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  return mare::internal::pfor_each_async(fn, first, last, stride, tuner);
}

template <size_t Dims, typename UnaryFn>
mare::task_ptr<void()>
pfor_each_async(const mare::range<Dims>& r,
                UnaryFn fn,
                const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  return mare::internal::pfor_each_async(fn, r, 1, tuner);
}

template <size_t Dims, typename UnaryFn>
mare::task_ptr<void()>
pfor_each_async(const mare::range<Dims>& r,
                const size_t stride, UnaryFn fn,
                const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  return mare::internal::pfor_each_async(fn, r, stride, tuner);
}

template<typename Fn, typename... Args>
mare::task_ptr<void()> create_task(const mare::pattern::pfor<Fn, void>& pf, Args&&...args)
{
  return mare::internal::pfor_each_async(pf._fn, args...);
}

template<typename Fn, typename... Args>
mare::task_ptr<void()> launch(const mare::pattern::pfor<Fn, void>& pf, Args&&...args)
{
  auto t = mare::create_task(pf, args...);
  t->launch();
  return t;
}

template <class InputIterator, typename UnaryFn>
void pfor_each(InputIterator first, InputIterator last,
               UnaryFn&& fn,
               const mare::pattern::tuner& t = mare::pattern::tuner())
{
  internal::pfor_each
    (nullptr, first, last, std::forward<UnaryFn>(fn), 1, t);
}

template <class InputIterator, typename UnaryFn>
void pfor_each(InputIterator first, const size_t stride,
               InputIterator last, UnaryFn&& fn,
               const mare::pattern::tuner& t = mare::pattern::tuner())
{
  internal::pfor_each
    (nullptr, first, last, std::forward<UnaryFn>(fn), stride, t);
}

template <size_t Dims, typename UnaryFn>
void pfor_each(const mare::range<Dims>& r, UnaryFn&& fn,
               const mare::pattern::tuner& t = mare::pattern::tuner())
{
  internal::pfor_each(nullptr, r, std::forward<UnaryFn>(fn), t);
}

};
