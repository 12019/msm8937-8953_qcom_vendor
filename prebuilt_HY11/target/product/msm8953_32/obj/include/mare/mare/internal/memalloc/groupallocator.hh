// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <atomic>
#include <string>

#include <mare/internal/memalloc/linearpool.hh>
#include <mare/internal/util/ffs.hh>

namespace mare {

namespace internal {

namespace allocatorinternal {

template <size_t LeafSize, size_t MeetSize>
class group_allocator {

  static constexpr size_t s_leaf_pool_size = 32;
public:

  group_allocator():
    _leaf_pool() {
  }

  ~group_allocator(){
  }
  MARE_DELETE_METHOD(group_allocator(group_allocator const&));
  MARE_DELETE_METHOD(group_allocator& operator=(group_allocator const&));
  MARE_DELETE_METHOD(group_allocator(group_allocator const&&));
  MARE_DELETE_METHOD(group_allocator& operator=(group_allocator const&&));

  char* allocate_leaf(size_t leaf_id) {
    return _leaf_pool.allocate_object(leaf_id);
  }

  std::string to_string() {
    std::string str = strprintf("Group allocator leaf pool: %s",
                                _leaf_pool.to_string().c_str());
    return str;
  }

  static size_t get_leaf_pool_size() {
    return s_leaf_pool_size;
  }
private:

  fixed_size_linear_pool<LeafSize, s_leaf_pool_size> _leaf_pool;
};

};

};

};
