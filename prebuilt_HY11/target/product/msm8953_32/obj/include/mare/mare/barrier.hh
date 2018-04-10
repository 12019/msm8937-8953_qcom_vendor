// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/synchronization/barrier.hh>

namespace mare{

typedef internal::sense_barrier barrier;

};

#ifdef ONLY_FOR_DOXYGEN

namespace mare {

class barrier{
public:

  explicit barrier(size_t total);

  barrier(barrier const&) = delete;
  barrier(barrier&&) = delete;
  barrier& operator=(barrier const&) = delete;
  barrier& operator=(barrier&&) = delete;

  void wait();
};

};

#endif
