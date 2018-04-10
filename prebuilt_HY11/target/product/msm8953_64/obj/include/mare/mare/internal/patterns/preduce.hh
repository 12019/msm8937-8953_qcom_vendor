// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/patterns/common.hh>

namespace mare {

namespace internal{

template<typename T, typename Fn>
T top_down_merge(typename ws_node<T>::node_type* root,
                 const T& identity,
                 Fn&& fn)
{
  if (root == nullptr)
    return identity;

  return fn(root->peek_value(),
            fn(top_down_merge(root->get_left(), identity, std::forward<Fn>(fn)),
               top_down_merge(root->get_right(), identity, std::forward<Fn>(fn))));
}

template<typename T, class InputIterator,
         typename Reduce, typename Join, bool IsIntegral>
struct preduce_dispatcher;

template<typename T, class InputIterator, typename Reduce, typename Join>
struct preduce_dispatcher<T, InputIterator, Reduce, Join, true>
{
  static T dispatch(group_shared_ptr group,
                    InputIterator first,
                    InputIterator last,
                    const T& identity,
                    Reduce&& reduce,
                    Join&& join,
                    const mare::pattern::tuner& t)
  {
    return preduce_dynamic(group, first, last, identity, std::forward<Reduce>(reduce),
                           std::forward<Join>(join), t);
  }
};

template<typename T, class InputIterator, typename Reduce, typename Join>
struct preduce_dispatcher<T, InputIterator, Reduce, Join, false>
{
  static T dispatch(group_shared_ptr group,
                    InputIterator first,
                    InputIterator last,
                    const T& identity,
                    Reduce&&,
                    Join&& join,
                    const mare::pattern::tuner& t)
  {
    auto fn = [first, join](size_t i, size_t j, T init){
      for (size_t k = i; k < j; ++k){
        init = join(init, *(first + k));
      }
      return init;
    };

    return preduce_dynamic(group, size_t(0), size_t(last - first), identity,
                           std::forward<decltype(fn)>(fn), std::forward<Join>(join), t);
  }
};

template<typename T, class InputIterator, typename Reduce, typename Join>
T preduce_dynamic(group_shared_ptr group,
                  InputIterator first, InputIterator last,
                  const T& identity, Reduce&& reduce, Join&& join,
                  const mare::pattern::tuner& tuner) {

  if (first >= last)
    return identity;

  MARE_API_ASSERT(first >= 0, "No negative index");

  typedef mare::internal::legacy::body_wrapper<Reduce> reduce_body_type;
  typedef mare::internal::legacy::body_wrapper<Join>   join_body_type;
  reduce_body_type reduce_wrapper(std::forward<Reduce>(reduce));

  auto body_attrs = reduce_wrapper.get_attrs();
  body_attrs = legacy::add_attr(body_attrs, mare::internal::attr::inlined);

  MARE_INTERNAL_ASSERT(reduce_body_type::get_arity() == 3,
                       "Invalid number of arguments in Reduce function");
  MARE_INTERNAL_ASSERT(join_body_type::get_arity() == 2,
                       "Invalid number of arguments in Join function");

  const size_t blk_size = tuner.get_chunk_size();

  auto g_internal = legacy::create_group();
  auto g = g_internal;
  if (group)
    g = legacy::intersect(g, group);

  typedef adaptive_steal_strategy<T, size_t, Reduce, Reduce, Policy::REDUCE> strategy_type;
  mare_shared_ptr<strategy_type> strategy_ptr(new strategy_type(
                                g, first, last - 1, identity,
                                std::forward<Reduce>(reduce),
                                body_attrs,
                                blk_size,
                                tuner));

  execute_master_task<T, strategy_type, Policy::REDUCE>(strategy_ptr);

  legacy::spin_wait_for(g);

  return
    top_down_merge(strategy_ptr->get_root(), identity, std::forward<Join>(join));
}

template<typename T, class InputIterator, typename Reduce, typename Join>
T preduce_static(group_shared_ptr group,
                 InputIterator first, InputIterator last,
                 const T& identity, Reduce&& reduce, Join&& join,
                 const mare::pattern::tuner& tuner) {
  if (first >= last)
    return identity;

  auto g_internal = legacy::create_group();
  auto g = g_internal;
  if (group != nullptr)
    g = legacy::intersect(g, group);

  typedef typename internal::distance_helper<InputIterator>::_result_type working_type;

  const working_type GRANULARITY_MULTIPLIER = 4;

  const working_type dist = internal::distance(first, last);
  auto num_chunks = GRANULARITY_MULTIPLIER * static_cast<working_type>(tuner.get_doc());
  working_type blksz = std::max(working_type(1), dist / num_chunks);
  num_chunks = dist % blksz == 0 ? dist / blksz : dist / blksz + 1;
  size_t n = size_t(num_chunks);
  MARE_INTERNAL_ASSERT(blksz > 0, "block size should be greater than zero");

  std::vector<T> temp_vec(n);

  std::atomic<size_t> work_id(0);

  auto chunk_body = [&temp_vec, first, last, identity, blksz, n, &work_id, reduce] () mutable {
    while(1) {
      auto prev = work_id.fetch_add(1, mare::mem_order_relaxed);

      if (prev < n) {
        InputIterator lb = first + prev * blksz;
        working_type safeofs = std::min(working_type(internal::distance(lb, last)), blksz);
        InputIterator rb = lb + safeofs;
        auto init = identity;
        temp_vec[prev] = reduce(lb, rb, init);
      } else {
        break;
      }
    }
  };

  for (size_t i = 0; i < tuner.get_doc() - 1; ++i) {
    legacy::launch(g, chunk_body);
  }
  chunk_body();

  legacy::spin_wait_for(g);

  T result = identity;
  for (size_t i = 0; i < n; ++i) {
    result = join(result, temp_vec[i]);
  }

  return result;
}

template<typename T, class InputIterator, typename Reduce, typename Join>
T preduce_internal(group_shared_ptr group,
                   InputIterator first, InputIterator last,
                   const T& identity, Reduce&& reduce, Join&& join,
                   const mare::pattern::tuner& t)
{
  if (t.is_serial()) {
    T result = identity;
    return reduce(first, last, result);
  }

  else if (t.is_static()) {
    return preduce_static<T, InputIterator, Reduce, Join>
            (group, first, last, identity, std::forward<Reduce>(reduce), std::forward<Join>(join), t);
  }

  else {

    return preduce_dispatcher
           <T, InputIterator, Reduce, Join,
            std::is_integral<InputIterator>::value>::
            dispatch(group, first, last, identity,
                     std::forward<Reduce>(reduce), std::forward<Join>(join),
                     t);
  }
}
template<typename T, class InputIterator, typename Reduce, typename Join>
T preduce(group_shared_ptr group,
          InputIterator first,
          InputIterator last,
          const T& identity,
          Reduce&& reduce,
          Join&& join,
          const mare::pattern::tuner& t)
{
  return internal::preduce_internal
          (group, first, last, identity,
           std::forward<Reduce>(reduce),
           std::forward<Join>(join), t);
}

template<typename T, typename Container, typename Join>
T preduce(group_shared_ptr group,
          Container& c, const T& identity, Join&& join,
          const mare::pattern::tuner& t)
{

  auto fn = [c, join](size_t i, size_t j, T init){
    for(size_t k = i; k < j; ++k){
      init = join(init, c[k]);
    }
    return init;
  };

  return preduce(group, size_t(0), c.size(), identity,
                 std::forward<decltype(fn)>(fn), std::forward<Join>(join), t);
}

template<typename T, typename Join>
T preduce
  (group_shared_ptr group, T* first, T* last, const T& identity, Join&& join,
   const mare::pattern::tuner& t)
{
  auto fn = [first, join](size_t i, size_t j, T init){
    for(size_t k = i; k < j; ++k){
      init = join(init, *(first + k));
    }
    return init;
  };

  return preduce(group, size_t(0), size_t(last - first), identity,
                 std::forward<decltype(fn)>(fn), std::forward<Join>(join), t);
}

template<typename T, class Iterator, typename Join>
T preduce(group_shared_ptr group,
          Iterator first,
          Iterator last,
          const T& identity,
          Join&& join,
          const mare::pattern::tuner& t)
{
  auto fn = [first, join](size_t i, size_t j, T init){
    for (size_t k = i; k < j; ++k){
      init = join(init, *(first + k));
    }
    return init;
  };

  return preduce(group, size_t(0), size_t(last - first), identity,
                 std::forward<decltype(fn)>(fn), std::forward<Join>(join), t);
}

template<typename T, class InputIterator, typename Reduce, typename Join>
mare::task_ptr<T> preduce_async(Reduce&& reduce,
          Join&& join,
          InputIterator first,
          InputIterator last,
          const T& identity,
          const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  auto g = legacy::create_group();
  auto t = mare::create_task([g, first, last, identity, reduce, join, tuner]{
      auto v = internal::preduce(g, first, last, identity, reduce, join, tuner);
      legacy::finish_after(g);
      return v;
  });
  auto gptr = internal::c_ptr(g);
  gptr->set_representative_task(internal::c_ptr(t));
  return t;
}

template<typename T, typename Container, typename Join>
mare::task_ptr<T> preduce_async(
          Join&& join,
          Container& c,
          const T& identity,
          const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  auto g = legacy::create_group();
  auto t = mare::create_task([g, c, identity, join, tuner]{
      auto v = internal::preduce(g, c, identity, join, tuner);
      legacy::finish_after(g);
      return v;
  });
  auto gptr = internal::c_ptr(g);
  gptr->set_representative_task(internal::c_ptr(t));
  return t;
}

template<typename T, typename Join>
mare::task_ptr<T> preduce_async(
          Join&& join,
          T* first,
          T* last,
          const T& identity,
          const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  auto g = legacy::create_group();
  auto t = mare::create_task([g, first, last, identity, join, tuner]{
      auto v = internal::preduce(g, first, last, identity, std::forward<Join>(join), tuner);
      legacy::finish_after(g);
      return v;
  });
  auto gptr = internal::c_ptr(g);
  gptr->set_representative_task(internal::c_ptr(t));
  return t;
}

template<typename T, typename Iterator, typename Join>
mare::task_ptr<T> preduce_async(
          Join&& join,
          Iterator first,
          Iterator last,
          const T& identity,
          const mare::pattern::tuner& tuner = mare::pattern::tuner())
{
  auto g = legacy::create_group();
  auto t = mare::create_task([g, first, last, identity, join, tuner]{
      auto v = internal::preduce(g, first, last, identity, std::forward<Join>(join), tuner);
      legacy::finish_after(g);
      return v;
  });
  auto gptr = internal::c_ptr(g);
  gptr->set_representative_task(internal::c_ptr(t));
  return t;
}

};
};
