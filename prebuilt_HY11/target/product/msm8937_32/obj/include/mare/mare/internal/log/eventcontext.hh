// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <thread>
#include <mare/internal/util/debug.hh>
#include <mare/internal/log/common.hh>

namespace mare {
namespace internal {
namespace log {

class event_context {

public:

  typedef size_t counter_type;
  typedef uint64_t time_type;

  event_context() :
    _count(0),
    _thread(),
    _when(0)
  {}

  std::thread::id get_this_thread_id() {
    if (_thread == s_null_thread_id)
      _thread = std::this_thread::get_id();
    return _thread;
  }

  time_type get_time() {
    if (_when == 0)
      _when = mare_get_time_now();
    return _when;
  }

  counter_type get_count() {
    if (_count == 0)
      _count = s_global_counter.fetch_add(1, mare::mem_order_relaxed);

    return _count;
  }

private:

  counter_type _count;
  std::thread::id _thread;
  time_type _when;

  static std::thread::id s_null_thread_id;
  static std::atomic<counter_type> s_global_counter;

  MARE_DELETE_METHOD(event_context(event_context const&));
  MARE_DELETE_METHOD(event_context(event_context&&));
  MARE_DELETE_METHOD(event_context& operator=(event_context const&));
  MARE_DELETE_METHOD(event_context& operator=(event_context&&));
};

};
};
};
