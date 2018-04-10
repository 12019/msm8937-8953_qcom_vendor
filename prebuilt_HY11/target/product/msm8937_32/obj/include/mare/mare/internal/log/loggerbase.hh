// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/log/events.hh>
#include <mare/internal/log/objectid.hh>

#include <mare/internal/util/debug.hh>

namespace mare {
namespace internal {
namespace log {

class logger_base {
public:
  virtual ~logger_base(){}

  enum class logger_id : int8_t {
    unknown = -1,
    corelogger = 0,
    schedulerlogger = 1,
    userlogger = 2
   };
};

};
};
};

