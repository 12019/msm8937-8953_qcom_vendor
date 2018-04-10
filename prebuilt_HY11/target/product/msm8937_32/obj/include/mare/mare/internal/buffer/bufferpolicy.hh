// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <algorithm>
#include <array>
#include <tuple>
#include <type_traits>

#include <mare/internal/buffer/arena.hh>
#include <mare/internal/buffer/bufferstate.hh>
#include <mare/internal/compat/compat.h>
#include <mare/internal/compat/compilercompat.h>
#include <mare/internal/task/task.hh>
#include <mare/internal/util/debug.hh>

namespace mare {
namespace internal {

class bufferpolicy {

public:

  enum action_t {
    acquire_r,
    acquire_w,
    acquire_rw,
    release
  };

  virtual ~bufferpolicy(){}

  virtual arena* unsafe_get_or_create_cpu_arena_with_valid_data(bufferstate* bufstate,
                                                                size_t size_in_bytes) = 0;

  virtual arena* unsafe_create_arena_with_cpu_initial_data(bufferstate* bufstate,
                                                           void* p,
                                                           size_t size_in_bytes) = 0;

  virtual arena* unsafe_create_arena_with_memregion_initial_data(bufferstate* bufstate,
                                                                 internal_memregion* int_mr,
                                                                 size_t size_in_bytes) = 0;

  enum class acquire_scope {

    tentative,

    confirm,

    full
  };

  struct conflict_info {
    bool        _no_conflict_found;
    arena*      _acquired_arena;
    void const* _conflicting_requestor;

    std::string to_string() const {
      return mare::internal::strprintf("{%s, arena=%p, conflicting_requestor=%p}",
                                       (_no_conflict_found ? "no_conflict" : "conflict"),
                                       _acquired_arena,
                                       _conflicting_requestor);
    }
  };

  virtual conflict_info request_acquire_action(bufferstate* bufstate,
                                               void const* requestor,
                                               executor_device ed,
                                               action_t ac,
                                               acquire_scope as,
                                               buffer_as_texture_info tex_info) = 0;

  virtual void release_action(bufferstate* bufstate,
                              void const* requestor) = 0;

  virtual void remove_matching_arena(bufferstate* bufstate,
                                     executor_device ed) = 0;

};

bufferpolicy* get_current_bufferpolicy();

MARE_GCC_IGNORE_BEGIN("-Weffc++");

template <typename T>
struct is_api20_buffer_ptr;

template<typename T>
struct is_api20_buffer_ptr: public std::false_type {};

template <typename T>
struct is_api20_buffer_ptr< ::mare::buffer_ptr<T> > : public std::true_type {};

template <typename T>
struct is_api20_buffer_ptr< ::mare::in<::mare::buffer_ptr<T>> > : public std::true_type {};

template <typename T>
struct is_api20_buffer_ptr< ::mare::out<::mare::buffer_ptr<T>> > : public std::true_type {};

template <typename T>
struct is_api20_buffer_ptr< ::mare::inout<::mare::buffer_ptr<T>> > : public std::true_type {};

template<typename BufPtr>
struct is_const_buffer_ptr : public std::false_type {};

template<typename T>
struct is_const_buffer_ptr< ::mare::buffer_ptr<const T> > : public std::true_type {};

MARE_GCC_IGNORE_END("-Weffc++");

template<typename Tuple, size_t index>
struct num_buffer_ptrs_in_tuple_helper {
  enum { value =  (is_api20_buffer_ptr<typename std::tuple_element<index-1, Tuple>::type>::value ? 1 : 0) +
                  num_buffer_ptrs_in_tuple_helper<Tuple, index-1>::value };
};

template<typename Tuple>
struct num_buffer_ptrs_in_tuple_helper<Tuple, 0> {
  enum { value = 0 };
};

template<typename Tuple>
struct num_buffer_ptrs_in_tuple {
  enum { value = num_buffer_ptrs_in_tuple_helper<Tuple, std::tuple_size<Tuple>::value>::value };
};

template<typename BuffersArrayType>
struct checked_addition_of_buffer_entry;

template<typename BufferInfo>
struct checked_addition_of_buffer_entry<std::vector<BufferInfo>> {
  static void add(std::vector<BufferInfo>& arr_buffers,
                  size_t&                  num_args_added,
                  BufferInfo const&        bi)
  {
    MARE_INTERNAL_ASSERT(arr_buffers.size() == num_args_added, "new entry expected to be added only at end");
    arr_buffers.push_back(bi);
    num_args_added = arr_buffers.size();
  }
};

template<typename BufferInfo, size_t MaxNumArgs>
struct checked_addition_of_buffer_entry<std::array<BufferInfo, MaxNumArgs>> {
  static void add(std::array<BufferInfo, MaxNumArgs>& arr_buffers,
                  size_t&                             num_args_added,
                  BufferInfo const&                   bi)
  {
    MARE_INTERNAL_ASSERT(num_args_added < MaxNumArgs,
                         "buffer_acquire_set: adding beyond expected max size: _num_args_added=%zu must be < MaxNumArgs=%zu",
                         num_args_added,
                         MaxNumArgs);

    arr_buffers[num_args_added++] = bi;
  }
};

template<typename ArenasArrayType>
struct resize_arenas_array {
  static void resize(size_t           arr_buffers_size,
                     ArenasArrayType& acquired_arenas)

  {
    MARE_INTERNAL_ASSERT(arr_buffers_size == acquired_arenas.size(),
                         "Fixed-size acquired_arenas (size=%zu) must have same size as final arr_buffers.size()=%zu",
                         acquired_arenas.size(),
                         arr_buffers_size);
    for(auto& a : acquired_arenas)
      a = nullptr;
  }
};

template<>
struct resize_arenas_array<std::vector<arena*>> {
  static void resize(size_t               arr_buffers_size,
                     std::vector<arena*>& acquired_arenas)
  {
    acquired_arenas.resize(arr_buffers_size);
    for(auto& a : acquired_arenas)
      a = nullptr;
  }
};

template<size_t MaxNumArgs, bool FixedSize = true>
class buffer_acquire_set {
  using action_t = mare::internal::bufferpolicy::action_t;

  static constexpr size_t s_flag_tentative_acquire = 0x1;

  struct buffer_info {
    bufferstate*           _bufstate_raw_ptr;
    action_t               _acquire_action;
    bool                   _syncs_on_task_finish;
    buffer_as_texture_info _tex_info;

    buffer_info() :
      _bufstate_raw_ptr(nullptr),
      _acquire_action(action_t::acquire_r),
      _syncs_on_task_finish(true),
      _tex_info()
    {}

    buffer_info(bufferstate*            bufstate_raw_ptr,
                action_t                acquire_action,
                bool                    syncs_on_task_finish,
                buffer_as_texture_info  tex_info = buffer_as_texture_info()) :
      _bufstate_raw_ptr(bufstate_raw_ptr),
      _acquire_action(acquire_action),
      _syncs_on_task_finish(syncs_on_task_finish),
      _tex_info(tex_info)
    {}
  };

  using buffers_array = typename std::conditional<FixedSize,
                                                  std::array<buffer_info, MaxNumArgs>,
                                                  std::vector<buffer_info>>::type;

  using arenas_array  = typename std::conditional<FixedSize,
                                                  std::array<arena*, MaxNumArgs>,
                                                  std::vector<arena*>>::type;

  buffers_array _arr_buffers;

  arenas_array _acquired_arenas;

  size_t _num_args_added;

  bool _acquire_status;

public:
  buffer_acquire_set() :
    _arr_buffers(),
    _acquired_arenas(),
    _num_args_added(0),
    _acquire_status(false)
  {}

  template<typename BufferPtr>
  void add(BufferPtr& b,
           action_t acquire_action,
           buffer_as_texture_info tex_info = buffer_as_texture_info())
  {
    static_assert( is_api20_buffer_ptr<BufferPtr>::value, "must be a mare::buffer_ptr" );
    if(b == nullptr) {
      return;
    }

    auto bufstate = buffer_accessor::get_bufstate(reinterpret_cast<buffer_ptr_base&>(b));
    auto bufstate_raw_ptr = ::mare::internal::c_ptr(bufstate);
    MARE_INTERNAL_ASSERT(bufstate_raw_ptr != nullptr, "Non-null buffer_ptr contains a null bufferstate");

    checked_addition_of_buffer_entry<buffers_array>::add(_arr_buffers,
                                                         _num_args_added,
                                                         buffer_info{ bufstate_raw_ptr,
                                                                      acquire_action,
                                                                      b.syncs_on_task_finish(),
                                                                      tex_info });
  }

  inline
  bool get_acquire_status() const { return _acquire_status; }

  void acquire_buffers(void const* requestor,
                       mare::internal::executor_device ed,
                       const bool setup_task_deps_on_conflict = false)
  {
    MARE_INTERNAL_ASSERT(get_acquire_status() == false, "acquire_buffers(): buffers are already acquired");

    std::sort(_arr_buffers.begin(),
              _arr_buffers.begin()+_num_args_added,
              [](buffer_info const& v1, buffer_info const& v2)
              { return v1._bufstate_raw_ptr < v2._bufstate_raw_ptr; });

    resize_arenas_array<arenas_array>::resize(_arr_buffers.size(), _acquired_arenas);

    auto bp = get_current_bufferpolicy();

    for(size_t pass = 1; pass <= 2; pass++) {
      size_t index = 0;
      while(index < _num_args_added) {
        auto bs       = _arr_buffers[index]._bufstate_raw_ptr;
        auto ac       = _arr_buffers[index]._acquire_action;
        bool sotf     = _arr_buffers[index]._syncs_on_task_finish;
        auto tex_info = _arr_buffers[index]._tex_info;

        while(index+1 < _num_args_added && bs == _arr_buffers[index+1]._bufstate_raw_ptr) {

          _acquired_arenas[index] = nullptr;

          auto next_ac   = _arr_buffers[index+1]._acquire_action;
          bool next_sotf = _arr_buffers[index+1]._syncs_on_task_finish;
          if(ac != next_ac) {

            ac = action_t::acquire_rw;

          }
          sotf |= next_sotf;
          index++;
        }

        _arr_buffers[index]._syncs_on_task_finish = sotf;

        bool try_buffer_acquire = false;
        bufferpolicy::conflict_info conflict;
        do {
          conflict = bp->request_acquire_action(bs,
                                                requestor,
                                                (tex_info.get_used_as_texture() ? executor_device::gputexture : ed),
                                                ac,
                                                (pass == 1 ? bufferpolicy::acquire_scope::tentative :
                                                             bufferpolicy::acquire_scope::confirm),
                                                tex_info);
          if(conflict._no_conflict_found == false) {

            MARE_INTERNAL_ASSERT(pass == 1, "buffer conflicts are expected to be found only in pass 1");

            if(!setup_task_deps_on_conflict) {
              release_buffers(requestor);
              return;
            }

            while(conflict._no_conflict_found == false && conflict._conflicting_requestor == nullptr) {
              conflict = bp->request_acquire_action(bs,
                                                    requestor,
                                                    (tex_info.get_used_as_texture() ? executor_device::gputexture : ed),
                                                    ac,
                                                    bufferpolicy::acquire_scope::tentative,
                                                    tex_info);
            }

            if(conflict._no_conflict_found == false) {

              MARE_INTERNAL_ASSERT(conflict._conflicting_requestor != nullptr,
                                   "Should have remained in while loop!");

              auto conflicting_task = static_cast<task*>(const_cast<void*>(conflict._conflicting_requestor));
              MARE_INTERNAL_ASSERT(conflicting_task != nullptr,
                                   "Conflict was confirmed, but conflicting requestor was not identified");
              auto current_task = static_cast<task*>(const_cast<void*>(requestor));
              MARE_INTERNAL_ASSERT(current_task != nullptr,
                                   "Requestor task not specified");
              MARE_INTERNAL_ASSERT(conflicting_task != current_task,
                                   "Buffer already held by the same task");

              if (conflicting_task->add_dynamic_control_dependency(current_task)) {

                release_buffers(requestor);
                return;
              } else {

                try_buffer_acquire = true;
              }
            }

          }
        } while (try_buffer_acquire);

        if(pass == 1) {
          MARE_INTERNAL_ASSERT(conflict._acquired_arena == nullptr,
                               "Pass 1 should not have acquired_arena on confirmed acquire of bufstate=%p",
                               bs);
          _acquired_arenas[index] = reinterpret_cast<arena*>(s_flag_tentative_acquire);
        }
        else if(pass == 2) {

          MARE_INTERNAL_ASSERT(conflict._acquired_arena != nullptr,
                               "Pass 2 should have found non-null acquired_arena on confirmed acquire of bufstate=%p",
                               bs);
          _acquired_arenas[index] = conflict._acquired_arena;
        }
        else {
          MARE_UNREACHABLE("There should only be Pass 1 and Pass 2. Found pass=%zu", pass);
        }

        index++;
      }
    }

    _acquire_status = true;
    log::fire_event(log::events::buffer_set_acquired());
  }

  void blocking_acquire_buffers(void const* requestor,
                                mare::internal::executor_device ed)
  {
    size_t spin_count = 10;
    while(get_acquire_status() == false) {
      if(spin_count > 0)
        spin_count--;
      else
        usleep(1);

      acquire_buffers(requestor, ed);
    }
  }

  void release_buffers(void const* requestor) {
    auto bp = get_current_bufferpolicy();

    for(size_t i=0; i<_num_args_added; i++) {
      if(_acquired_arenas[i] == nullptr)
        continue;

      auto bufstate_raw_ptr     = _arr_buffers[i]._bufstate_raw_ptr;
      bool syncs_on_task_finish = _arr_buffers[i]._syncs_on_task_finish;

      bp->release_action(bufstate_raw_ptr,
                         requestor);
      if(get_acquire_status() == true) {

        if(syncs_on_task_finish == true) {
          MARE_DLOG("Syncing mare::buffer_ptr: bufstate_raw_ptr = %p on release by requestor=%p",
                    bufstate_raw_ptr,
                    requestor);
          bufstate_raw_ptr->lock();
          bufstate_raw_ptr->sync_on_task_finish();
          bufstate_raw_ptr->unlock();
        }
        MARE_DLOG("bufstate=%p %s",
                  bufstate_raw_ptr,
                  bufstate_raw_ptr->to_string().c_str());

      }

      _acquired_arenas[i] = nullptr;
    }
    _acquire_status = false;
  }

  template<typename BufferPtr>
  arena* find_acquired_arena(BufferPtr& b) {
    MARE_INTERNAL_ASSERT(get_acquire_status() == true,
                         "buffer_acquire_set::find_acquired_arena(): Attempting to find"
                         "arena when buffers not acquired");

    static_assert( is_api20_buffer_ptr<BufferPtr>::value, "must be a mare::buffer_ptr" );
    if(b == nullptr) {
      return nullptr;
    }

    auto bufstate = buffer_accessor::get_bufstate(
                                            reinterpret_cast<mare::internal::buffer_ptr_base&>(b));
    auto bufstate_raw_ptr = ::mare::internal::c_ptr(bufstate);
    for(size_t i=0; i<_num_args_added; i++) {
      if(_arr_buffers[i]._bufstate_raw_ptr == bufstate_raw_ptr && _acquired_arenas[i] != nullptr)
        return _acquired_arenas[i];
    }
    return nullptr;
  }
};

};
};
