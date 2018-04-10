// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/patterns/policy_adaptive.hh>

namespace mare {

namespace internal {

MARE_MSC_IGNORE_BEGIN(4702)
template<typename T, typename Fn, Policy P>
struct worker_wrapper;

template<typename T, typename Fn>
struct worker_wrapper<T, Fn, Policy::REDUCE>
{
  static bool
  work_on(typename ws_node<T>::node_type* node, Fn&& fn)
  {
    MARE_INTERNAL_ASSERT(node != nullptr, "Unexpectedly work on null pointer");
    MARE_INTERNAL_ASSERT(node->is_unclaimed
        (mare::mem_order_relaxed) == false, "Invalid node state %zu",
        node->get_progress(mare::mem_order_relaxed));

    typename ws_node<T>::size_type first = node->get_first();
    typename ws_node<T>::size_type last  = node->get_last();

    if (first > last) {
      node->set_completed();
      return true;
    }

    auto blk_size = node->get_tree()->get_blk_size();
    auto i = first;
    typename ws_node<T>::size_type right_bound;

    if (first == node->get_tree()->range_start()) {
      right_bound = i + blk_size;
      right_bound = right_bound > last ? last + 1 : right_bound;
      node->set_value(fn(i, right_bound, node->peek_value()));
      i = right_bound;
      if (right_bound == last + 1)
        return true;
    }

    do {

      right_bound = i + blk_size;
      right_bound = right_bound > last ? last + 1 : right_bound;

      auto j = i;
      node->set_value(fn(j, right_bound, node->peek_value()));

      auto prev = node->inc_progress(blk_size, mare::mem_order_relaxed);

      if (i != prev) {
        MARE_INTERNAL_ASSERT
          (node->is_stolen(blk_size,mare::mem_order_relaxed),
           "Invalid node state %zu",
           node->get_progress(mare::mem_order_relaxed));
        return false;
      }

      i = prev + blk_size;

    } while(i <= last);

    node->set_completed();
    return true;
  }
};

template<typename Fn>
struct worker_wrapper<void, Fn, Policy::MAP>
{
  static bool
  work_on(typename ws_node<void>::node_type* node, Fn&& fn)
  {
    MARE_INTERNAL_ASSERT(node != nullptr, "Unexpectedly work on null pointer");
    MARE_INTERNAL_ASSERT(node->is_unclaimed
        (mare::mem_order_relaxed) == false, "Invalid node state %zu",
        node->get_progress(mare::mem_order_relaxed));

    typename ws_node<void>::size_type first = node->get_first();
    typename ws_node<void>::size_type last  = node->get_last();

    if (first > last) {
      node->set_completed();
      return true;
    }

    auto blk_size = node->get_tree()->get_blk_size();
    auto stride   = node->get_tree()->get_stride();

    auto i = first;
    typename ws_node<void>::size_type right_bound;

    if (first == node->get_tree()->range_start()) {
      right_bound = i + blk_size - 1;
      right_bound = right_bound > last ? last : right_bound;
      while (i <= right_bound) {
        fn(i);
        i += stride;
      }
      if (right_bound == last)
        return true;
    }

    do {

      right_bound = i + blk_size - 1;
      right_bound = right_bound > last ? last : right_bound;

      auto j = i;
      while (j <= right_bound) {
        fn(j);
        j += stride;
      }

      auto prev = node->inc_progress(blk_size, mare::mem_order_relaxed);

      if (i != prev) {
        MARE_INTERNAL_ASSERT(
            node->is_stolen(blk_size,mare::mem_order_relaxed),
            "Invalid node state %zu",
            node->get_progress(mare::mem_order_relaxed));
        return false;
      }

      i = prev + blk_size;

    } while(i <= last);

    node->set_completed();
    return true;
  }
};

template<typename T, typename Fn>
struct worker_wrapper<T, Fn, Policy::SCAN>
{
  static bool
  work_on(typename ws_node<T>::node_type* node, Fn&& fn)
  {
    MARE_INTERNAL_ASSERT(node != nullptr, "Unexpectedly work on null pointer");
    MARE_INTERNAL_ASSERT(node->is_unclaimed(mare::mem_order_relaxed) == false,
        "Invalid node state %zu",
        node->get_progress(mare::mem_order_relaxed));

    typename ws_node<T>::size_type first = node->get_first();
    typename ws_node<T>::size_type last  = node->get_last();

    if (first > last) {
      node->set_completed();
      return true;
    }

    auto blk_size = node->get_tree()->get_blk_size();
    typename ws_node<void>::size_type rb;

    auto i = first + 1;
    auto lb = first;
    auto range_start = node->get_tree()->range_start();

    while (i <= last) {

      rb = lb + blk_size;
      rb = rb >= last ? last + 1 : rb;

      for (auto j = i; j < rb; ++j) {
        fn(j-1, j);
      }

      if (lb != range_start) {

        auto prev = node->inc_progress(blk_size, mare::mem_order_relaxed);

        if (lb != prev) {
          MARE_INTERNAL_ASSERT(
              node->is_stolen(blk_size, mare::mem_order_relaxed),
              "Invalid node state %zu, %zu-%zu, lb: %zu, prev: %zu",
              node->get_progress(mare::mem_order_relaxed), first, last, lb, prev);
          return false;
        }
      }

      i = rb;
      lb = i;
    }

    node->set_completed();
    return true;
  }
};
MARE_MSC_IGNORE_END(4702)

};
};

