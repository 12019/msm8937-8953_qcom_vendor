// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <array>
#include <mutex>
#include <set>
#include <string>

#include <mare/devicetypes.hh>

#include <mare/internal/buffer/arena.hh>
#include <mare/internal/buffer/executordevice.hh>
#include <mare/internal/buffer/memregion.hh>
#include <mare/internal/compat/compilercompat.h>
#include <mare/internal/util/debug.hh>
#include <mare/internal/util/macros.hh>
#include <mare/internal/util/mareptrs.hh>
#include <mare/internal/util/strprintf.hh>

namespace mare {
namespace internal {

inline
std::string to_string(executor_device ed) {
  return std::string(ed == executor_device::unspecified ? "USP" :
                     (ed == executor_device::cpu ? "CPU" :
                      (ed == executor_device::gpucl ? "GPUCL" :
                       (ed == executor_device::gpugl ? "GPUGL" :
                        (ed == executor_device::gputexture ? "GPUTEXTURE" : "HEX")))));
}

struct buffer_acquire_info {
public:
  enum access_t {
    unspecified, read, write, readwrite
  };

  static inline
  std::string to_string(access_t access) {
    return std::string(access == unspecified ? "U" :
                       (access == read ? "R" :
                        (access == write ? "W" : "RW")));
  }

  void*           _acquired_by;

  executor_device _ed;

  access_t        _access;

  bool            _tentative_acquire;

  arena*          _acquired_arena;

  buffer_acquire_info() :
    _acquired_by(nullptr),
    _ed(executor_device::unspecified),
    _access(unspecified),
    _tentative_acquire(false),
    _acquired_arena(nullptr)
  {}

  explicit buffer_acquire_info(void const*     acquired_by) :
    _acquired_by(const_cast<void*>(acquired_by)),
    _ed(executor_device::unspecified),
    _access(unspecified),
    _tentative_acquire(false),
    _acquired_arena(nullptr)
  {}

  buffer_acquire_info(void const*     acquired_by,
                      executor_device ed,
                      access_t        access,
                      bool            tentative_acquire,
                      arena*          acquired_arena) :
    _acquired_by(const_cast<void*>(acquired_by)),
    _ed(ed),
    _access(access),
    _tentative_acquire(tentative_acquire),
    _acquired_arena(acquired_arena)
  {}

  std::string to_string() const {
    return ::mare::internal::strprintf("{%p, %s, %s, %s, %p}",
                                       _acquired_by,
                                       ::mare::internal::to_string(_ed).c_str(),
                                       to_string(_access).c_str(),
                                       (_tentative_acquire ? "T" : "F"),
                                       static_cast<void*>(_acquired_arena));
  }
};

bool operator<(buffer_acquire_info const& bai1, buffer_acquire_info const& bai2);

inline
bool operator<(buffer_acquire_info const& bai1, buffer_acquire_info const& bai2)
{
  return bai1._acquired_by < bai2._acquired_by;
}

MARE_GCC_IGNORE_BEGIN("-Weffc++")
class bufferstate : public ::mare::internal::ref_counted_object<bufferstate> {
MARE_GCC_IGNORE_END("-Weffc++")

private:
  using existing_arenas_t = std::array<arena*, NUM_ARENA_TYPES>;
  using arena_set_t       = std::array<bool,   NUM_ARENA_TYPES>;

  size_t const _size_in_bytes;

  bool const _conservative_host_sync;

  std::mutex _mutex;

  device_set _device_hints;

  existing_arenas_t _existing_arenas;

  arena_set_t _valid_data_arenas;

  std::set<buffer_acquire_info> _acquire_set;

private:

  friend class buffer_ptr_base;

  bufferstate(size_t size_in_bytes, bool conservative_host_sync, device_set const& device_hints) :
    _size_in_bytes(size_in_bytes),
    _conservative_host_sync(conservative_host_sync),
    _mutex(),
    _device_hints(device_hints),
    _existing_arenas(),
    _valid_data_arenas(),
    _acquire_set()
  {
    for(auto& a : _existing_arenas)
      a = nullptr;
    for(auto& va: _valid_data_arenas)
      va = false;
  }

  MARE_DELETE_METHOD(bufferstate());

  MARE_DELETE_METHOD(bufferstate(bufferstate const&));
  MARE_DELETE_METHOD(bufferstate& operator=(bufferstate const&));

  MARE_DELETE_METHOD(bufferstate(bufferstate&&));
  MARE_DELETE_METHOD(bufferstate& operator=(bufferstate&&));

public:

  ~bufferstate() {
    for(auto& a: _existing_arenas) {
      if(a != nullptr) {
        delete a;
        a = nullptr;
      }
    }

    for(auto& va: _valid_data_arenas)
      va = false;
  }

  size_t get_size_in_bytes() const { return _size_in_bytes; }

  bool does_conservative_host_sync() const { return _conservative_host_sync; }

  void lock() {
    _mutex.lock();
  }

  void unlock() {
    _mutex.unlock();
  }

  device_set const& get_device_hints() const {
    return _device_hints;
  }

  inline
  arena* get(arena_t arena_type) const {
    MARE_INTERNAL_ASSERT(arena_type >= 0 && arena_type < NUM_ARENA_TYPES, "Invalid arena_type %zu", arena_type);
    return _existing_arenas[arena_type];
  }

  inline
  bool has(arena_t arena_type) const {
    return get(arena_type) != nullptr;
  }

  std::set<buffer_acquire_info> const& get_acquire_set() const {
    return _acquire_set;
  }

  void add_device_hint(device_type d) { _device_hints.add(d); }

  void remove_device_hint(device_type d) { _device_hints.remove(d); }

  inline
  bool buffer_holds_valid_data() const {
    for(auto va : _valid_data_arenas)
      if(va == true)
        return true;
    return false;
  }

  size_t num_valid_data_arenas() const {
    size_t count = 0;
    for(auto va : _valid_data_arenas)
      if(va == true)
        count++;
    return count;
  }

  arena* first_valid_data_arena() const {
    for(size_t i=0; i<_existing_arenas.size(); i++) {
      if(_existing_arenas[i] != nullptr && _valid_data_arenas[i] == true)
        return _existing_arenas[i];
    }
    return nullptr;
  }

  bool is_valid_data_arena(arena* a) const {
    MARE_INTERNAL_ASSERT(a != nullptr, "arena=%p has not been created", a);

    auto arena_type = a->get_type();
    MARE_INTERNAL_ASSERT(arena_type != NO_ARENA, "Unusable arena type");
    MARE_INTERNAL_ASSERT(_existing_arenas[arena_type] == a,
                         "arena=%p does not exist in bufferstate=%p",
                         a,
                         this);

    MARE_INTERNAL_ASSERT(_valid_data_arenas[arena_type] == false || a->get_alloc_type() != UNALLOCATED,
                         "arena=%p is marked as having valid data in bufstate=%p but is UNALLOCATED",
                         a,
                         this);

    return _valid_data_arenas[arena_type];
  }

  bool is_valid_data_arena(arena_t arena_type) const {
    auto a = get(arena_type);
    if(a == nullptr)
      return false;

    MARE_INTERNAL_ASSERT(_valid_data_arenas[arena_type] == false || a->get_alloc_type() != UNALLOCATED,
                         "arena=%p is marked as having valid data in bufstate=%p but is UNALLOCATED",
                         a,
                         this);

    if(a->get_alloc_type() == UNALLOCATED)
      return false;

    return _valid_data_arenas[arena_type];
  }

  void add_arena(arena* a, bool has_valid_data) {
    MARE_INTERNAL_ASSERT(a != nullptr,
                         "Arena object needs to have been created a-priori by the bufferpolicy.");
    auto arena_type = a->get_type();
    MARE_INTERNAL_ASSERT(arena_type != NO_ARENA,
                         "new arena does not have a usable arena type");
    MARE_INTERNAL_ASSERT(_existing_arenas[arena_type] == nullptr,
                         "When adding arena=%p, arena=%p already present in this buffer "
                         "for arena_type=%d bufferstate=%p",
                         a,
                         _existing_arenas[arena_type],
                         static_cast<int>(arena_type),
                         this);
    _existing_arenas[arena_type] = a;

    MARE_INTERNAL_ASSERT(has_valid_data == false || num_valid_data_arenas() == 0,
                         "New arena brings valid data, but there are already arenas allocated with valid data.\n"
                         "Disable this assertion if the buffer policy can guarantee that the new arena's data is"
                         "already consistent.");
    if(has_valid_data)
      _valid_data_arenas[arena_type] = true;
  }

  void designate_as_unique_valid_data_arena(arena* unique_a) {
    MARE_INTERNAL_ASSERT(unique_a != nullptr,
                         "Arena object needs to have been created a-priori by the bufferpolicy.");
    auto arena_type = unique_a->get_type();
    MARE_INTERNAL_ASSERT(arena_type != NO_ARENA, "Unusable arena type");
    MARE_INTERNAL_ASSERT(_existing_arenas[arena_type] == unique_a,
                         "Arena to set as having valid data is not allocated in this buffer: unique_a=%p, bufferstate=%p\n",
                         unique_a,
                         this);
    if(_valid_data_arenas[arena_type] == false && buffer_holds_valid_data() == true) {

      auto one_valid_arena = pick_optimal_valid_copy_from_arena(unique_a);
      MARE_INTERNAL_ASSERT(one_valid_arena != nullptr,
                           "Valid copy-from arena unexpectedly not found");
      if(one_valid_arena != nullptr) {
        copy_valid_data(one_valid_arena, unique_a);
      }
      else {
        MARE_FATAL("designate_as_unique_valid_data_arena(): Expected non-null value for one_valid_arena.");
      }
    }
    _valid_data_arenas[arena_type] = true;

    MARE_INTERNAL_ASSERT(_existing_arenas.size() == _valid_data_arenas.size(),
                         "_existing_arenas and _valid_data_arenas unexpectedly differ in size");

    for(size_t i=0; i<_existing_arenas.size(); i++) {
      if(i != arena_type && _valid_data_arenas[i] == true) {
        invalidate_arena(_existing_arenas[i]);
      }
    }

    MARE_INTERNAL_ASSERT(_valid_data_arenas[arena_type] == true && num_valid_data_arenas() == 1,
                         "designate_as_unique_valid_data_arena() must result in exactly one valid arena");
  }

  void invalidate_arena(arena* a) {
    MARE_INTERNAL_ASSERT(a != nullptr, "Arena is null");
    auto arena_type = a->get_type();
    if(_valid_data_arenas[arena_type] == true){
      a->invalidate();
      _valid_data_arenas[arena_type] = false;
    }
  }

  void invalidate_all() {
    for(auto a: _existing_arenas){
      if(a != nullptr){
        invalidate_arena(a);
      }
    }
  }

  void remove_arena(arena* a, bool delete_arena = false) {
    invalidate_arena(a);
    auto arena_type = a->get_type();
    MARE_INTERNAL_ASSERT(arena_type != NO_ARENA, "Unusable arena type");
    MARE_INTERNAL_ASSERT(_existing_arenas[arena_type] == a,
                         "Removing arena=%p which is not present in bufferstate=%p",
                         a,
                         this);
    _existing_arenas[arena_type] = nullptr;
    if(delete_arena) {
      for(auto other_arena : _existing_arenas) {
        if(other_arena != nullptr && other_arena->get_bound_to_arena() == a) {
          MARE_FATAL("Attempting to delete arena=%p, but another arena=%p is bound to it in bufstate=%s",
                     a,
                     other_arena,
                     to_string().c_str());
        }
      }
      delete a;
    }
  }

  arena* pick_optimal_valid_copy_from_arena(arena* to_arena) const {
    MARE_INTERNAL_ASSERT(to_arena != nullptr,
                         "to_arena has not been created.");
    auto to_arena_type = to_arena->get_type();
    MARE_INTERNAL_ASSERT(to_arena_type != NO_ARENA,
                         "Unusable arena type of to_arena=%p",
                         to_arena);
    MARE_INTERNAL_ASSERT(_existing_arenas[to_arena_type] == to_arena,
                         "to_arena=%p is not part of this bufferstate=%p",
                         to_arena,
                         this);
    if(_valid_data_arenas[to_arena_type] == true)
      return to_arena;

    auto to_arena_alloc_type = to_arena->get_alloc_type();
    MARE_UNUSED(to_arena_alloc_type);

    return first_valid_data_arena();
  }

  void copy_data(arena* from_arena, arena* to_arena) {
    MARE_INTERNAL_ASSERT(from_arena != nullptr && to_arena != nullptr,
                         "from_arena=%p and to_arena=%p must both be created before copy",
                         from_arena,
                         to_arena);
    auto from_arena_type = from_arena->get_type();
    MARE_INTERNAL_ASSERT(from_arena_type != NO_ARENA,
                         "Unusable arena type of from_arena=%p",
                         from_arena);
    MARE_INTERNAL_ASSERT(_existing_arenas[from_arena_type] == from_arena,
                         "from_arena=%p is not part of this bufferstate=%p",
                         from_arena,
                         this);
    auto to_arena_type = to_arena->get_type();
    MARE_INTERNAL_ASSERT(to_arena_type != NO_ARENA,
                         "Unusable arena type of to_arena=%p",
                         to_arena);
    MARE_INTERNAL_ASSERT(_existing_arenas[to_arena_type] == to_arena,
                         "to_arena=%p is not part of this bufferstate=%p",
                         to_arena,
                         this);
    copy_arenas(from_arena, to_arena);
  }

  void copy_valid_data(arena* valid_from_arena, arena* to_arena) {
    MARE_INTERNAL_ASSERT(valid_from_arena != nullptr,
                         "valid_from_arena is not yet created");
    auto from_arena_type = valid_from_arena->get_type();
    MARE_INTERNAL_ASSERT(from_arena_type != NO_ARENA,
                         "Unusuable arena type of valid_from_arena=%p",
                         valid_from_arena);
    MARE_INTERNAL_ASSERT(_existing_arenas[from_arena_type] == valid_from_arena,
                         "valid_from_arena=%p is not part of this bufferstate=%p",
                         valid_from_arena,
                         this);
    MARE_INTERNAL_ASSERT(_valid_data_arenas[from_arena_type] == true,
                         "source arena=%p needs to have valid data",
                         valid_from_arena);
    copy_data(valid_from_arena, to_arena);
    auto to_arena_type = to_arena->get_type();
    MARE_INTERNAL_ASSERT(to_arena_type != NO_ARENA,
                         "Unusable arena type of to_arena=%p",
                         to_arena);
    _valid_data_arenas[to_arena_type] = true;
  }

  void invalidate_non_mainmem_arenas() {

    auto mma = get(MAINMEM_ARENA);
    if(mma != nullptr &&
       mma->get_alloc_type() != UNALLOCATED &&
       !is_valid_data_arena(mma))
    {
      auto one_valid_arena = pick_optimal_valid_copy_from_arena(mma);
      if(one_valid_arena != nullptr)
        copy_valid_data(one_valid_arena, mma);
    }

    for(auto a: _existing_arenas) {
      if(a != nullptr && a != mma)
        invalidate_arena(a);
    }
  }

  void sync_on_task_finish() {
    if(does_conservative_host_sync() == true) {

      conservative_sync_on_task_finish();
    }
    else {

      relaxed_sync_on_task_finish();
    }
  }

  void conservative_sync_on_task_finish() {
    auto mma = get(MAINMEM_ARENA);
    MARE_INTERNAL_ASSERT(mma != nullptr && mma->get_alloc_type() != UNALLOCATED,
                         "mainmem_arena must always be allocated in conservative access mode");
    designate_as_unique_valid_data_arena(mma);
    mma->acquire(executor_device::cpu);
  }

  void relaxed_sync_on_task_finish() {
    auto mma = get(MAINMEM_ARENA);
    if(mma != nullptr &&
       mma->get_alloc_type() != UNALLOCATED &&
       !is_valid_data_arena(mma))
    {
      auto one_valid_arena = pick_optimal_valid_copy_from_arena(mma);
      MARE_INTERNAL_ASSERT(one_valid_arena != nullptr,
                           "On task finish at least one arena must have valid data");
      copy_valid_data(one_valid_arena, mma);
      mma->acquire(executor_device::cpu);
    }
  }

  void host_code_sync() {
    auto mma = get(MAINMEM_ARENA);
    MARE_INTERNAL_ASSERT(mma != nullptr,
                         "Main memory arena should be already created by the time bufferstate::host_code_sync() is called");

    auto mma_was_initially_valid = is_valid_data_arena(MAINMEM_ARENA);

    if(mma_was_initially_valid == false) {
      auto one_valid_arena = pick_optimal_valid_copy_from_arena(mma);
      if(one_valid_arena != nullptr) {
        copy_valid_data(one_valid_arena, mma);
      }
    }
    if(mma_was_initially_valid == true || does_conservative_host_sync() == true) {

      for(auto a: _existing_arenas){
        if(a != nullptr && a != mma){
          invalidate_arena(a);
        }
      }
      designate_as_unique_valid_data_arena(mma);
    }
  }

  struct conflict_info {
    bool        _no_conflict_found;
    void const* _conflicting_requestor;
  };

  conflict_info add_acquire_requestor(void const*                   requestor,
                                      executor_device               ed,
                                      buffer_acquire_info::access_t access,
                                      bool                          tentative_acquire)
  {
    MARE_INTERNAL_ASSERT(ed != executor_device::unspecified,
                         "executor device doing acquire is unspecified");
    MARE_INTERNAL_ASSERT(access != buffer_acquire_info::unspecified,
                         "Access type is unspecified during acquire");

    MARE_INTERNAL_ASSERT(_acquire_set.find( buffer_acquire_info(requestor) ) == _acquire_set.end(),
                         "buffer_acquire_info already present for requestor=%p in bufstate=%p",
                         requestor,
                         this);

    if(access == buffer_acquire_info::read) {
      for(auto const& acreq : _acquire_set) {
        if(acreq._access != buffer_acquire_info::read) {
          auto confirmed_conflicting_requestor = (acreq._tentative_acquire ? nullptr : acreq._acquired_by);
          return {false, confirmed_conflicting_requestor};
        }
      }
    }
    else {
      if(_acquire_set.size() > 0) {
        auto first_acreq = *(_acquire_set.begin());
        auto confirmed_conflicting_requestor = (first_acreq._tentative_acquire ? nullptr : first_acreq._acquired_by);
        return {false, confirmed_conflicting_requestor};
      }
    }

    _acquire_set.insert( buffer_acquire_info(requestor, ed, access, tentative_acquire, nullptr) );

    switch(ed){
    default:
    case executor_device::unspecified: MARE_UNREACHABLE("Unspecified executor_device"); break;
    case executor_device::cpu: _device_hints.add(device_type::cpu); break;
    case executor_device::gpucl:
    case executor_device::gpugl: _device_hints.add(device_type::gpu); break;
    case executor_device::gputexture: break;
    case executor_device::hexagon: _device_hints.add(device_type::hexagon); break;
    }

    return {true, nullptr};
  }

  void confirm_tentative_acquire_requestor(void const* requestor) {
    buffer_acquire_info lookup_acqinfo(requestor);
    auto it = _acquire_set.find(lookup_acqinfo);
    MARE_INTERNAL_ASSERT(it != _acquire_set.end(),
                         "requestor=%p not found in bufstate=%p",
                         requestor,
                         this);
    auto entry = *it;
    MARE_INTERNAL_ASSERT(entry._tentative_acquire == true,
                         "Entry for requestor=%p was not tentative in bufstate=%p",
                         requestor,
                         this);
    MARE_INTERNAL_ASSERT(entry._acquired_arena == nullptr,
                         "requestor=%p is tentative, but somehow has non-null _acquired_arena=%p",
                         requestor,
                         entry._acquired_arena);
    entry._tentative_acquire = false;
    _acquire_set.erase(entry);
    _acquire_set.insert(entry);
  }

  void update_acquire_info_with_arena(void const* requestor, arena* acquired_arena) {
    MARE_INTERNAL_ASSERT(acquired_arena != nullptr, "Error. acquired_arena is null");
    buffer_acquire_info lookup_acqinfo(requestor);
    auto it = _acquire_set.find(lookup_acqinfo);
    MARE_INTERNAL_ASSERT(it != _acquire_set.end(),
                         "buffer_acquire_info not found: %s",
                         lookup_acqinfo.to_string().c_str());
    auto entry = *it;
    entry._acquired_arena = acquired_arena;
    _acquire_set.erase(entry);
    _acquire_set.insert(entry);
  }

  void remove_acquire_requestor(void const* requestor) {
    buffer_acquire_info lookup_acqinfo(requestor);
    auto it = _acquire_set.find(lookup_acqinfo);
    MARE_INTERNAL_ASSERT(it != _acquire_set.end(),
                         "buffer_acquire_info not found: %s",
                          lookup_acqinfo.to_string().c_str());
    if(it->_tentative_acquire) {
      MARE_INTERNAL_ASSERT(it->_acquired_arena == nullptr,
                           "A tentative requestor=%p was found with non-null arena=%p in bufstate=%p",
                           requestor,
                           it->_acquired_arena,
                           this);
    }
    else {
      MARE_INTERNAL_ASSERT(it->_acquired_arena != nullptr,
                           "Acquired arena is nullptr for confirmed requestor=%p in bufstate=%p",
                           requestor,
                           this);
      it->_acquired_arena->release(it->_ed);
    }
    _acquire_set.erase(*it);
  }

  std::string to_string() const {
    std::string s;
    s += "[";
    for(size_t i=0; i<_existing_arenas.size(); i++) {
      auto a = _existing_arenas[i];
      auto bound_to_arena = (a == nullptr ? nullptr : a->get_bound_to_arena());

      size_t index_of_bound_to_arena = _existing_arenas.size();
      if(bound_to_arena != nullptr) {
        for(size_t j=0; j<_existing_arenas.size(); j++) {
          if(_existing_arenas[j] == bound_to_arena) {
            index_of_bound_to_arena = j;
            break;
          }
        }
      }

      s += ::mare::internal::strprintf("%zu=(%p %s %s) ",
                                       i,
                                       _existing_arenas[i],
                                       _valid_data_arenas[i] ? "V" : "I",
                                       (bound_to_arena == nullptr ? "UB" :
                                            ::mare::internal::strprintf("B%zu", index_of_bound_to_arena).c_str()));
    }
    s += "]";
    return s;
  }

};

};
};
