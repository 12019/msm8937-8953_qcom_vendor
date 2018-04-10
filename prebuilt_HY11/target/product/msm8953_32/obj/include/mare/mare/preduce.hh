// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/taskfactory.hh>
#include <mare/tuner.hh>

#include <mare/internal/patterns/preduce.hh>

namespace mare {
namespace pattern {

template <typename Reduce, typename Join> class preducer;

template <typename Reduce, typename Join>
preducer<Reduce, Join> create_preduce(Reduce&& r, Join&& j);

template <typename Reduce, typename Join>
class preducer {
public:
  template <typename T, typename InputIterator>
  T run(InputIterator first,
        InputIterator last,
        const T& identity,
        const mare::pattern::tuner& t = mare::pattern::tuner()) {
    return internal::preduce_internal
              (nullptr, first, last, identity,
               std::forward<Reduce>(_reduce),
               std::forward<Join>(_join),
               t);
  }

  template <typename T, typename InputIterator>
  T operator()(InputIterator first, InputIterator last, const T& identity,
      const mare::pattern::tuner& t = mare::pattern::tuner()) {
    return run(first, last, identity, t);
  }

private:
  Reduce _reduce;
  Join   _join;
  preducer(Reduce&& r, Join&& j) : _reduce(r), _join(j) {}
  friend preducer create_preduce<Reduce, Join>(Reduce&& r, Join&& j);
  template<typename T, typename R, typename J, typename ...Args>
  friend mare::task_ptr<T> mare::create_task(const preducer<R, J>& p, Args&&...args);
};

template <typename Reduce, typename Join>
preducer<Reduce, Join> create_preduce(Reduce&& r, Join&& j) {
  return preducer<Reduce, Join>
    (std::forward<Reduce>(r), std::forward<Join>(j));
}

};

template<typename T, class InputIterator, typename Reduce, typename Join>
T preduce(InputIterator first,
          InputIterator last,
          const T& identity,
          Reduce&& reduce,
          Join&& join,
          mare::pattern::tuner tuner = mare::pattern::tuner())
{

  if (!tuner.is_chunk_set()) {
    auto chunk_size = mare::internal::static_chunk_size<size_t>
                    (static_cast<size_t>(last - first),
                     mare::internal::num_execution_contexts());
    tuner.set_chunk_size(chunk_size);
  }

  return internal::preduce(nullptr, first, last, identity,
                 std::forward<Reduce>(reduce),
                 std::forward<Join>(join), tuner);
}

template<typename T, class InputIterator, typename Reduce, typename Join>
mare::task_ptr<T> preduce_async(InputIterator first,
          InputIterator last,
          const T& identity,
          Reduce&& reduce,
          Join&& join,
          mare::pattern::tuner tuner = mare::pattern::tuner())
{
  if (!tuner.is_chunk_set()) {
    tuner.set_chunk_size(mare::internal::static_chunk_size<size_t>(
                         static_cast<size_t>(last - first),
                         mare::internal::num_execution_contexts())
                      );
  }
  return mare::internal::preduce_async(
      std::forward<Reduce>(reduce),
      std::forward<Join>(join),
      first,
      last,
      identity,
      tuner);
}

template<typename T, typename Container, typename Join>
T preduce(Container& c, const T& identity, Join&& join,
          mare::pattern::tuner tuner = mare::pattern::tuner())
{
  if (!tuner.is_chunk_set()){
    tuner.set_chunk_size(mare::internal::static_chunk_size<size_t>
                         (c.size(), mare::internal::num_execution_contexts()));
  }

  return internal::preduce
    (nullptr, c, identity, std::forward<Join>(join), tuner);
}

template<typename T, typename Container, typename Join>
mare::task_ptr<T> preduce_async
        ( Container& c,
          const T& identity,
          Join&& join,
          mare::pattern::tuner tuner = mare::pattern::tuner()
        )
{
  if (!tuner.is_chunk_set()){
    tuner.set_chunk_size(mare::internal::static_chunk_size<size_t>
                       (c.size(), mare::internal::num_execution_contexts()));
  }

  return mare::internal::preduce_async(std::forward<Join>(join), c, identity, tuner);
}

template<typename T, typename Iterator, typename Join>
T preduce(Iterator first, Iterator last, const T& identity, Join&& join,
          mare::pattern::tuner tuner = mare::pattern::tuner())
{
  if (!tuner.is_chunk_set()) {
    tuner.set_chunk_size(mare::internal::static_chunk_size<size_t>(
                         static_cast<size_t>(last - first),
                         mare::internal::num_execution_contexts())
                        );
  }

  return internal::preduce
    (nullptr, first, last, identity, std::forward<Join>(join), tuner);
}

template<typename T, typename Iterator, typename Join>
mare::task_ptr<T> preduce_async
  (Iterator first, Iterator last, const T& identity, Join&& join,
   mare::pattern::tuner tuner = mare::pattern::tuner())
{
  if (!tuner.is_chunk_set()) {
    tuner.set_chunk_size(mare::internal::static_chunk_size<size_t>(
                         static_cast<size_t>(last - first),
                         mare::internal::num_execution_contexts())
                        );
  }

  return mare::internal::preduce_async(std::forward<Join>(join), first, last, identity, tuner);
}

template<typename T, typename Reduce, typename Join, typename ...Args>
mare::task_ptr<T> create_task(const mare::pattern::preducer<Reduce, Join>& pr, Args&&...args)
{
  return mare::internal::preduce_async(pr._reduce, pr._join, args...);
}

template<typename T, typename Reduce, typename Join, typename ...Args>
mare::task_ptr<T> launch(const mare::pattern::preducer<Reduce, Join>& pr, Args&&...args)
{
  auto t = mare::create_task<T>(pr, args...);
  t->launch();
  return t;
}

};
