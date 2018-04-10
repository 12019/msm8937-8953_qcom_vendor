// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#if !defined(MARE_NO_TASK_ALLOCATOR)

#include <mare/internal/memalloc/threadlocalallocator.hh>

namespace mare {
namespace internal {

class task;

namespace task_allocator {

  void init(size_t threads);

  void shutdown();

  char* allocate(size_t object_size);

  char* allocate_default(size_t object_size);

  void deallocate(task* object);

};
};
};

#endif
