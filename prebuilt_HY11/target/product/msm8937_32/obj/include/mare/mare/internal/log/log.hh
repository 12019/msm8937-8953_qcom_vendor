// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#if defined(__ANDROID__) || defined(__linux__)
#include <mare/internal/log/ftracelogger.hh>
#endif

#include <mare/internal/log/corelogger.hh>
#include <mare/internal/log/eventcounterlogger.hh>
#include <mare/internal/log/infrastructure.hh>
#include <mare/internal/log/pforlogger.hh>
#include <mare/internal/log/schedulerlogger.hh>
#include <mare/internal/log/userhandlers.hh>
#include <mare/internal/log/userlogapi.hh>

namespace mare {
namespace internal {
namespace log {

#if defined(__ANDROID__) || defined(__linux__)
typedef infrastructure<event_counter_logger, ftrace_logger, pfor_logger, schedulerlogger> loggers;
#else
typedef infrastructure<event_counter_logger, pfor_logger, schedulerlogger> loggers;
#endif

typedef loggers::enabled enabled;
typedef loggers::group_id group_id;
typedef loggers::task_id task_id;

void init();
void shutdown();

void pause();
void resume();
void dump();

template<typename Event>
inline void fire_event (Event&& e) {
  loggers::event_fired(std::forward<Event>(e));
  user_handlers::event_fired(std::forward<Event>(e));
}

};
};
};
