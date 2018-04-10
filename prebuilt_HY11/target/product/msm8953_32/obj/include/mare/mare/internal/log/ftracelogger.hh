// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <array>
#include <atomic>
#include <map>

#include <mare/internal/util/debug.hh>
#include <mare/internal/log/loggerbase.hh>
#include <mare/internal/log/objectid.hh>

namespace mare {
namespace internal {
namespace log {

class ftrace_logger : public logger_base {

public:

#ifndef MARE_USE_FTRACE_LOGGER
  typedef std::false_type enabled;
#else
  typedef std::true_type enabled;
#endif

  static void init();

  static void shutdown();

  static void dump() { }

  static void paused();

  static void resumed();

  template<typename EVENT>
  static void log(EVENT&& e, event_context&) {
    print(std::forward<EVENT>(e));
  }

private:

  static void ftrace_write(const char *str);
  static void ftrace_write(const char *str, size_t len);

  static void print(events::group_canceled&& e);

  static void print(events::group_created&& e);

  static void print(events::group_destroyed&& e);

  static void print(events::group_reffed&& e);

  static void print(events::group_unreffed&& e);

  static void print(events::group_wait_for_ended&& e);

  static void print(events::task_after&& e);

  static void print(events::task_cleanup&& e);

  static void print(events::task_created&& e);

  static void print(events::task_destroyed&& e);

  static void print(events::task_finished&& e);

  static void print(events::task_executes&& e);

  static void print(events::task_sent_to_runtime&& e);

  static void print(events::task_reffed&& e);

  static void print(events::task_unreffed&& e);

  static void print(events::task_wait&& e);

  template<typename UNKNOWNEVENT>
  static void print(UNKNOWNEVENT&&) { }

  MARE_DELETE_METHOD(ftrace_logger());
  MARE_DELETE_METHOD(ftrace_logger(ftrace_logger const&));
  MARE_DELETE_METHOD(ftrace_logger(ftrace_logger&&));
  MARE_DELETE_METHOD(ftrace_logger&
                     operator=(ftrace_logger const&));
  MARE_DELETE_METHOD(ftrace_logger& operator=(ftrace_logger&&));

};

};
};
};
