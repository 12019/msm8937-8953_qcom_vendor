// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#if defined(MARE_HAVE_QTI_HEXAGON)

#include <string>
#include <tuple>
#include <typeinfo>
#include <type_traits>

#include <mare/exceptions.hh>
#include <mare/taskptr.hh>

#include <mare/internal/task/hexagontraits.hh>
#include <mare/internal/task/task.hh>
#include <mare/internal/util/debug.hh>
#include <mare/internal/util/demangler.hh>
#include <mare/internal/util/strprintf.hh>

MARE_GCC_IGNORE_BEGIN("-Weffc++");

namespace mare {

template<typename ...Stuff> class task_ptr;

namespace internal {

class group;
class task_bundle_dispatch;

class hexagontask_return_layer: public task {

public:

  virtual ~hexagontask_return_layer(){};

protected:

  explicit hexagontask_return_layer(state_snapshot initial_state, group* g, ::mare::internal::legacy::task_attrs a)
    : task(initial_state, g, a) {
  }

  explicit hexagontask_return_layer(group* g, ::mare::internal::legacy::task_attrs a)
    : task(g, a) {
  }

  std::string describe_return_value() {
    return demangler::demangle<void>();
  };

  template<typename Fn>
  void execute_and_store_retval(Fn& fn, const void *) {

    int status = fn();

    if (status != 0) {
      throw mare::hexagon_exception();
    }
  }

  template<typename Fn, typename ...TaskArgs>
  void execute_and_store_retval(Fn& fn, const void *requestor, TaskArgs&&...args) {

    MARE_INTERNAL_ASSERT(requestor != nullptr, "The requestor should be not null");

    using tuple_args_type = std::tuple<TaskArgs...>;

    using args_buffers_acquire_set_type = buffer_acquire_set< num_buffer_ptrs_in_tuple<tuple_args_type>::value>;

    tuple_args_type args_in_tuple(args...);

    static_assert(std::tuple_size<std::tuple<TaskArgs...>>::value > 0, "Argument tuple should contain at least 1 element");

    args_buffers_acquire_set_type bas;

    parse_and_add_buffers_to_acquire_set<args_buffers_acquire_set_type,
                                         tuple_args_type,
                                         0,
                                         Fn,
                                         0,
                                         false> parsed_params(bas, args_in_tuple);

    bas.blocking_acquire_buffers(requestor, mare::internal::executor_device::hexagon);

    translate_TP_args_to_hexagon<Fn,
                                 0,
                                 tuple_args_type,
                                 0,
                                 args_buffers_acquire_set_type,
                                 0 == sizeof...(TaskArgs)> hexagon_args_container(args_in_tuple, bas);

    auto & hexagon_args = hexagon_args_container._htp_till_index;

    int status = apply(fn, hexagon_args);

    bas.release_buffers(requestor);

    if (status != 0) {
      throw mare::hexagon_exception();
    }
  }

private:

};

template<typename ...TaskArgs>
class hexagontask_arg_layer : public hexagontask_return_layer {

  using parent = hexagontask_return_layer;

  using arity = std::integral_constant<std::uint32_t, sizeof...(TaskArgs)>;

  using storage_arg_types = std::tuple<TaskArgs...>;

  template<std::uint32_t ... numbers>
  struct integer_sequence {

    static constexpr size_t get_size() {
      return sizeof...(numbers);
    }
  };

  template<std::uint32_t Top, std::uint32_t ...Sequence>
  struct create_integer_sequence : create_integer_sequence<Top-1, Top-1, Sequence...> {
  };

  template<std::uint32_t ...Sequence>
  struct create_integer_sequence<0, Sequence...> {
    using type = integer_sequence<Sequence...>;
  };

  template<typename HexagonKernel, uint32_t ...Number>
  void expand_args(HexagonKernel& hk, const void* requestor, integer_sequence<Number...>) {

    parent::template execute_and_store_retval(hk.get_fn(), requestor, std::move(std::get<Number>(_args))...);

  }

  storage_arg_types _args;

protected:

  std::string describe_arguments() {
    std::string description = "(";
    description += demangler::demangle<TaskArgs...>();
    description +=")";
    return description;
  };

  using state_snapshot = typename parent::state_snapshot;

  hexagontask_arg_layer(state_snapshot initial_state, group* g,
    ::mare::internal::legacy::task_attrs a, TaskArgs&& ...args)
    : parent(initial_state, g, a)
      , _args(std::forward<TaskArgs>(args)...)
  {
  }

  template<typename HexagonKernel>
  void prepare_args(HexagonKernel& hk, void const* requestor) {
    expand_args<HexagonKernel>(hk, requestor, typename create_integer_sequence<arity::value>::type());
  }

public:

  virtual ~hexagontask_arg_layer() {

  };

};

template<typename HexagonKernel, typename ...TaskArgs>
class hexagontask : public hexagontask_arg_layer<TaskArgs...> {

  HexagonKernel _kernel;
  using parent = hexagontask_arg_layer<TaskArgs...>;

protected:

  using state_snapshot = typename parent::state_snapshot;

public:

  hexagontask(state_snapshot initial_state, group* g, ::mare::internal::legacy::task_attrs a,
      HexagonKernel&& kernel, TaskArgs&& ...args)
    : parent(initial_state, g, a, std::forward<TaskArgs>(args)...)
    , _kernel(kernel) {
  }

  virtual ~hexagontask() {};

  virtual std::string describe_body() {
    return strprintf("%s%s %" MARE_FMT_TID, parent::describe_return_value().c_str(),
                     parent::describe_arguments().c_str(),
                     do_get_source());
  };

private:

  virtual uintptr_t do_get_source() const {
    return reinterpret_cast<uintptr_t>(const_cast<HexagonKernel&>(_kernel).get_fn());
  }

  virtual bool do_execute(task_bundle_dispatch* tbd) {
    MARE_UNUSED(tbd);
    parent::prepare_args(_kernel, this);
    return true;
  }

  MARE_DELETE_METHOD(hexagontask(hexagontask const&));
  MARE_DELETE_METHOD(hexagontask(hexagontask&&));
  MARE_DELETE_METHOD(hexagontask& operator=(hexagontask const&));
  MARE_DELETE_METHOD(hexagontask& operator=(hexagontask&&));
};

MARE_GCC_IGNORE_END("-Weffc++");

};
};

#endif
