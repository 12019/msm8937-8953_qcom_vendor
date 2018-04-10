// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/runtime.hh>

namespace mare {

namespace runtime {

extern void init_implementation();

inline void init()
{
  init_implementation();
}

void shutdown();

typedef void(*callback_t)();

callback_t set_thread_created_callback(callback_t fptr);

callback_t set_thread_destroyed_callback(callback_t fptr);

callback_t thread_created_callback();

callback_t thread_destroyed_callback();

size_t busyness_level();

};
};
