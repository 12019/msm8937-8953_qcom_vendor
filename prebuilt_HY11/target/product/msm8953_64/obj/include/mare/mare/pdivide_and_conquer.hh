// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/taskfactory.hh>
#include <mare/tuner.hh>

#include <mare/internal/patterns/pdivide_and_conquer.hh>

namespace mare {

namespace pattern {
  template<typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
  class pdivide_and_conquerer;
};

template <typename Problem, typename Solution, typename Fn1,
         typename Fn2, typename Fn3, typename Fn4>
Solution pdivide_and_conquer(Problem p, Fn1&& is_base,
    Fn2&& base, Fn3&& split, Fn4&& merge,
    const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  MARE_UNUSED(tuner);
  auto t = mare::internal::pdivide_and_conquer<Problem, Solution>(
      nullptr,
      std::forward<Problem>(p),
      std::forward<Fn1>(is_base),
      std::forward<Fn2>(base),
      std::forward<Fn3>(split),
      std::forward<Fn4>(merge));
  return t->move_value();
}

template <typename Problem, typename Solution, typename Fn1,
         typename Fn2, typename Fn3, typename Fn4>
mare::task_ptr<Solution> pdivide_and_conquer_async(Problem p, Fn1&& is_base,
    Fn2&& base, Fn3&& split, Fn4&& merge,
    const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  MARE_UNUSED(tuner);
  return mare::internal::pdivide_and_conquer_async<Problem, Solution>(
      std::forward<Fn1>(is_base),
      std::forward<Fn2>(base),
      std::forward<Fn3>(split),
      std::forward<Fn4>(merge),
      p);
}

template <typename Problem, typename Fn1, typename Fn2,
         typename Fn3, typename Fn4>
void pdivide_and_conquer(Problem p, Fn1&& is_base,
    Fn2&& base, Fn3&& split, Fn4&& merge,
    const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  MARE_UNUSED(tuner);
  mare::internal::pdivide_and_conquer(
      nullptr,
      p,
      std::forward<Fn1>(is_base),
      std::forward<Fn2>(base),
      std::forward<Fn3>(split),
      std::forward<Fn4>(merge));
}

template <typename Problem, typename Fn1, typename Fn2,
          typename Fn3, typename Fn4>
mare::task_ptr<> pdivide_and_conquer_async(Problem p, Fn1&& is_base,
    Fn2&& base, Fn3&& split, Fn4&& merge,
    const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  MARE_UNUSED(tuner);
  return mare::internal::pdivide_and_conquer_async<Problem>(
      std::forward<Fn1>(is_base),
      std::forward<Fn2>(base),
      std::forward<Fn3>(split),
      std::forward<Fn4>(merge),
      p);
}

template <typename Problem, typename Fn1, typename Fn2, typename Fn3>
void pdivide_and_conquer(Problem p, Fn1&& is_base, Fn2&& base, Fn3&& split,
    const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  MARE_UNUSED(tuner);
  mare::internal::pdivide_and_conquer(nullptr, p, std::forward<Fn1>(is_base),
      std::forward<Fn2>(base), std::forward<Fn3>(split));
}

template <typename Problem, typename Fn1, typename Fn2, typename Fn3>
mare::task_ptr<>
pdivide_and_conquer_async(Problem p, Fn1&& is_base, Fn2&& base, Fn3&& split,
    const mare::pattern::tuner& tuner = mare::pattern::tuner()) {
  MARE_UNUSED(tuner);
  return mare::internal::pdivide_and_conquer_async<Problem>(
        std::forward<Fn1>(is_base),
        std::forward<Fn2>(base),
        std::forward<Fn3>(split),
        p);
}

template <typename Problem, typename Solution, typename Fn1,
         typename Fn2, typename Fn3, typename Fn4>
mare::task_ptr<Solution>
create_task(const mare::pattern::pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc, Problem p,
    const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  MARE_UNUSED(tuner);
  return mare::internal::pdivide_and_conquer_async<Problem, Solution>(
      pdnc._is_base_fn,
      pdnc._base_fn,
      pdnc._split_fn,
      pdnc._merge_fn,
      p);
}

template <typename Problem, typename Solution, typename Fn1,
         typename Fn2, typename Fn3, typename Fn4>
mare::task_ptr<Solution>
launch(const mare::pattern::pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc, Problem p,
    const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  auto t = create_task<Problem, Solution>(pdnc, p, tuner);
  t->launch();
  return t;
}

template <typename Problem, typename Fn1,
         typename Fn2, typename Fn3, typename Fn4>
mare::task_ptr<>
create_task(const mare::pattern::pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc, Problem p,
    const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  MARE_UNUSED(tuner);
  return mare::internal::pdivide_and_conquer_async(
      pdnc._is_base_fn,
      pdnc._base_fn,
      pdnc._split_fn,
      pdnc._merge_fn,
      p);
}

template <typename Problem, typename Fn1,
         typename Fn2, typename Fn3, typename Fn4>
mare::task_ptr<>
launch(const mare::pattern::pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc, Problem p,
    const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  auto t = create_task<Problem>(pdnc, p, tuner);
  t->launch();
  return t;
}

namespace pattern {

template<typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
pdivide_and_conquerer<IsBaseFn, BaseFn, SplitFn, MergeFn>
create_pdivide_and_conquer(IsBaseFn&& isbase, BaseFn&& base,
                            SplitFn&& split, MergeFn&& merge);

template<typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
class pdivide_and_conquerer {
  using _Problem = typename internal::function_traits<BaseFn>::template arg_type<0>;
  using _Solution = typename internal::function_traits<BaseFn>::return_type;

public:

  _Solution run(_Problem& p, const mare::pattern::tuner& pt = mare::pattern::tuner()) {

    MARE_UNUSED(pt);
    return mare::pdivide_and_conquer<_Problem, _Solution>(
        p,
        std::forward<IsBaseFn>(_is_base_fn),
        std::forward<BaseFn>(_base_fn),
        std::forward<SplitFn>(_split_fn),
        std::forward<MergeFn>(_merge_fn));
  }

  _Solution operator()(_Problem& p, const mare::pattern::tuner& pt = mare::pattern::tuner()) {
    return run(p, pt);
  }

private:
  IsBaseFn _is_base_fn;
  BaseFn   _base_fn;
  SplitFn  _split_fn;
  MergeFn  _merge_fn;

  pdivide_and_conquerer(IsBaseFn&& isbase, BaseFn&& base, SplitFn&& split, MergeFn&& merge)
  : _is_base_fn(isbase), _base_fn(base), _split_fn(split), _merge_fn(merge) {}

  friend pdivide_and_conquerer
  create_pdivide_and_conquer<IsBaseFn, BaseFn, SplitFn, MergeFn>
  (IsBaseFn&& isbase, BaseFn&& base, SplitFn&& split, MergeFn&& merge);

  template<typename Problem, typename Solution, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
  friend mare::task_ptr<Solution> mare::create_task(
      const pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc, Problem p, const mare::pattern::tuner& t);

  template<typename Problem, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
  friend mare::task_ptr<> mare::create_task(
      const pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>& pdnc, Problem p, const mare::pattern::tuner& t);
};

template<typename IsBaseFn, typename BaseFn, typename SplitFn, typename MergeFn>
pdivide_and_conquerer<IsBaseFn, BaseFn, SplitFn, MergeFn>
create_pdivide_and_conquer(IsBaseFn&& isbase, BaseFn&& base, SplitFn&& split, MergeFn&& merge)
{
  using is_base_fn_traits = internal::function_traits<IsBaseFn>;
  using base_fn_traits = internal::function_traits<BaseFn>;

  static_assert(is_base_fn_traits::arity::value == 1,
      "is_base function takes exactly one argument");

  static_assert(std::is_same<typename is_base_fn_traits::return_type, bool>::value,
      "is_base function must return bool");

  static_assert(std::is_same<typename is_base_fn_traits::template arg_type<0>,
      typename base_fn_traits::template arg_type<0>>::value,
      "arguments to is_base and base functions must be of the same type");

  return pdivide_and_conquerer<IsBaseFn, BaseFn, SplitFn, MergeFn>
    (std::forward<IsBaseFn>(isbase), std::forward<BaseFn>(base),
     std::forward<SplitFn>(split), std::forward<MergeFn>(merge));
}

};
};
