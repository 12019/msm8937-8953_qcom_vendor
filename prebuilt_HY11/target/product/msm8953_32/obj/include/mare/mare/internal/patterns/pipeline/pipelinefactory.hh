// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/taskfactory.hh>

#include <mare/internal/util/macros.hh>
#include <mare/internal/patterns/pipeline/pipelinebuffers.hh>

namespace mare {
namespace internal {

template <typename F>
class stage_function_unary {

public:
  using f_type      =  typename function_traits<F>::type_in_task;
  using return_type =  typename function_traits<F>::return_type;
  using arg0_type   =  typename function_traits<F>::template arg_type<0>;
  using arg1_type   =  void;

  explicit stage_function_unary(F&& f)
    : _f(f) { }

  stage_function_unary(stage_function_unary const& other)
    : _f(other._f) { }

  virtual ~stage_function_unary() { };

  MARE_DELETE_METHOD(
    stage_function_unary(stage_function_unary&& other));
  MARE_DELETE_METHOD(
    stage_function_unary& operator=(stage_function_unary const& other));
  MARE_DELETE_METHOD(
    stage_function_unary& operator=(stage_function_unary&& other));

public:
  f_type _f;
};

template <typename F>
class stage_function_binary {

public:
  using f_type      =  typename function_traits<F>::type_in_task;
  using return_type =  typename function_traits<F>::return_type;
  using arg0_type   =  typename function_traits<F>::template arg_type<0>;
  using arg1_type   =  typename function_traits<F>::template arg_type<1>;

  explicit stage_function_binary(F&& f)
    : _f(f) { }

  stage_function_binary(stage_function_binary const& other)
    : _f(other._f) { }

  virtual ~stage_function_binary() { };

  MARE_DELETE_METHOD(
    stage_function_binary(stage_function_binary&& other));
  MARE_DELETE_METHOD(
    stage_function_binary& operator=(stage_function_binary const& other));
  MARE_DELETE_METHOD(
    stage_function_binary& operator=(stage_function_binary&& other));

public:
  f_type _f;
};

template <typename F, size_t Arity, typename RT>
class cpu_stage_function_arity_dispatch;

template <typename F, typename RT>
class cpu_stage_function_arity_dispatch<F, 1, RT> :
  public stage_function_unary<F>{

public:
  using context_type       = typename stage_function_unary<F>::arg0_type;
  using context_type_noref = typename std::remove_reference<context_type>::type;
  using return_type        = typename stage_function_unary<F>::return_type;
  using arg1_type          = void;

  static_assert(std::is_default_constructible<return_type>::value,
    "The return type of a stage should be default constructible.");

  static_assert(std::is_copy_assignable<return_type>::value,
    "The return type of a stage should be copy-assignable.");

  explicit cpu_stage_function_arity_dispatch(F&& f) :
    stage_function_unary<F>(std::forward<F>(f)) { }

  cpu_stage_function_arity_dispatch(
    cpu_stage_function_arity_dispatch const& other) :
    stage_function_unary<F>(other) {
  }

  template<typename IBT, typename OBT, typename >
  void apply_function(size_t,
                      size_t,
                      IBT*,
                      size_t out_idx,
                      OBT* outbuf,
                      void* context,
                      pipeline_launch_type) {

    MARE_INTERNAL_ASSERT(outbuf != nullptr,
      "The input buffer for the pipeline stage should not be allocated.");

    size_t oid   = outbuf->get_buffer_index(out_idx);
    (*outbuf)[oid] = this->_f(*static_cast<context_type_noref*>(context));
  }

  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch&& other));
  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch&
    operator=(cpu_stage_function_arity_dispatch const& other));
  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch&
    operator=(cpu_stage_function_arity_dispatch&& other));
};

template <typename F>
class cpu_stage_function_arity_dispatch<F, 1, void> :
  public stage_function_unary<F>{

public:

  using context_type       =  typename stage_function_unary<F>::arg0_type;
  using context_type_noref =  typename std::remove_reference<context_type>::type;
  using return_type        =  typename stage_function_unary<F>::return_type;
  using arg1_type          =  void;

  explicit cpu_stage_function_arity_dispatch(F&& f) :
    stage_function_unary<F>(std::forward<F>(f)) { }

  cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch const& other) :
    stage_function_unary<F>(other) {
  }

  template<typename IBT, typename OBT, typename >
  void apply_function(size_t,
                      size_t,
                      IBT* inbuf,
                      size_t,
                      OBT*,
                      void* context,
                      pipeline_launch_type) {

    MARE_INTERNAL_ASSERT(inbuf == nullptr,
      "The input buffer for the first pipeline stage should not be allocated.");

    this->_f(*static_cast<context_type_noref*>(context));
  }

  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch&& other));
  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch&
    operator=(cpu_stage_function_arity_dispatch const& other));
  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch&
    operator=(cpu_stage_function_arity_dispatch&& other));
};

template <typename F, typename RT>
class cpu_stage_function_arity_dispatch<F, 2, RT> :
  public stage_function_binary<F> {

public:
  using context_type       = typename stage_function_binary<F>::arg0_type;
  using context_type_noref = typename std::remove_reference<context_type>::type;
  using arg1_type          = typename stage_function_binary<F>::arg1_type;
  using arg1_type_noref    = typename std::remove_reference<arg1_type>::type;
  using return_type        =  typename stage_function_binary<F>::return_type;

  static_assert(std::is_default_constructible<return_type>::value,
    "The return type of a stage should be default constructible.");

  static_assert(std::is_copy_assignable<return_type>::value ||
                std::is_move_assignable<return_type>::value,
    "The return type of a stage should be either copy-assignable or move-assignable.");

  explicit cpu_stage_function_arity_dispatch(F&& f) :
    stage_function_binary<F>(std::forward<F>(f)) {
  }

  cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch const& other) :
    stage_function_binary<F>(other) {
  }

  template<typename IBT, typename OBT, typename InputType>
  void apply_function(size_t in_first_idx,
                      size_t in_size,
                      IBT* inbuf,
                      size_t out_idx,
                      OBT* outbuf,
                      void* context,
                      pipeline_launch_type launch_type) {

    MARE_INTERNAL_ASSERT(in_size > 0,
      "The size for the input for the stage should be great than zero.");
    MARE_INTERNAL_ASSERT(inbuf != nullptr,
      "The input buffer for the pipeline stage should be allocated.");

    size_t iid = inbuf->get_buffer_index(in_first_idx);

    stage_input<InputType> input(
      static_cast<stagebuffer*>(inbuf),
      iid, in_size, in_first_idx, launch_type);

    if(outbuf) {
      size_t oid = outbuf->get_buffer_index(out_idx);
      (*outbuf)[oid] = this->_f(*static_cast<context_type_noref*>(context), input);
    } else {
      this->_f(*static_cast<context_type_noref*>(context), input);
    }
  }

  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch&& other));
  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch&
    operator=(cpu_stage_function_arity_dispatch const& other));
  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch&
    operator=(cpu_stage_function_arity_dispatch&& other));
};

template <typename F>
class cpu_stage_function_arity_dispatch<F, 2, void> :
  public stage_function_binary<F> {

public:

  using context_type       = typename stage_function_binary<F>::arg0_type;
  using context_type_noref = typename std::remove_reference<context_type>::type;
  using arg1_type          = typename stage_function_binary<F>::arg1_type;
  using arg1_type_noref    = typename std::remove_reference<arg1_type>::type;
  using return_type        = typename stage_function_binary<F>::return_type;

  static_assert(
    std::is_base_of<stage_input_base, arg1_type_noref>::value,
    "The 2nd param for a stage function should be of type stage_input<xxx>."
  );

  explicit cpu_stage_function_arity_dispatch(F&& f) :
    stage_function_binary<F>(std::forward<F>(f)) {
  }

  cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch const& other) :
    stage_function_binary<F>(other) {
  }

  template<typename IBT, typename OBT, typename InputType>
  void apply_function(size_t in_first_idx,
                      size_t in_size,
                      IBT* inbuf,
                      size_t,
                      OBT* outbuf,
                      void* context,
                      pipeline_launch_type launch_type) {

    MARE_INTERNAL_ASSERT(outbuf == nullptr,
      "Last stage does not have the output buffer.");
    MARE_INTERNAL_ASSERT(inbuf != nullptr,
      "Input buffer should not be nullptr. ");

    size_t iid = inbuf->get_buffer_index(in_first_idx);

    stage_input<InputType> input(
      static_cast<stagebuffer*>(inbuf),
      iid, in_size, in_first_idx, launch_type);

    this->_f(*static_cast<context_type_noref*>(context), input);
  }

  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch(cpu_stage_function_arity_dispatch&& other));
  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch&
    operator=(cpu_stage_function_arity_dispatch const& other));
  MARE_DELETE_METHOD(
    cpu_stage_function_arity_dispatch&
    operator=(cpu_stage_function_arity_dispatch&& other));
};

template <typename F>
class cpu_stage_function:
  public cpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value,
                                  typename function_traits<F>::return_type >{
public:

  using f_type       = typename function_traits<F>::type_in_task;
  using return_type  = typename function_traits<F>::return_type;
  using context_type = typename
    cpu_stage_function_arity_dispatch<F,
                                      function_traits<F>::arity::value,
                                      typename function_traits<F>::return_type>::context_type;

  using arg1_type    =  typename
    cpu_stage_function_arity_dispatch<F,
                                      function_traits<F>::arity::value,
                                      typename function_traits<F>::return_type>::arg1_type;

  explicit cpu_stage_function(F&& f) :
    cpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, return_type>(
     std::forward<F>(f)) { }

  cpu_stage_function(cpu_stage_function<F> const& other):
    cpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, return_type>(
     other) { }

  virtual ~cpu_stage_function()
  { }

  MARE_DELETE_METHOD(cpu_stage_function(cpu_stage_function<F>&& other));
  MARE_DELETE_METHOD(
    cpu_stage_function<F>& operator=(cpu_stage_function<F> const& other));
  MARE_DELETE_METHOD(
    cpu_stage_function<F>& operator=(cpu_stage_function<F>&& other));
};

#ifdef MARE_HAVE_GPU

template <typename F, size_t Arity, typename RT>
class gpu_stage_function_arity_dispatch;

template <typename F, typename RT>
class gpu_stage_function_arity_dispatch<F, 1, RT> :
  public stage_function_unary<F>{

public:

  typedef typename stage_function_unary<F>::arg0_type   context_type;
  typedef typename stage_function_unary<F>::return_type return_type;
  typedef void                                          arg1_type;

  explicit gpu_stage_function_arity_dispatch(F&& f) :
    stage_function_unary<F>(std::forward<F>(f)) { }

  gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch const& other) :
    stage_function_unary<F>(other) {
  }

  MARE_DELETE_METHOD(
    gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch && other));
  MARE_DELETE_METHOD(
    gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch const& other));
  MARE_DELETE_METHOD(
    gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch&& other));
};

template <typename F>
class gpu_stage_function_arity_dispatch<F, 1, void> :
  public stage_function_unary<F>{

public:

  typedef typename stage_function_unary<F>::arg0_type   context_type;
  typedef typename stage_function_unary<F>::return_type return_type;
  typedef void                                 arg1_type;

  explicit gpu_stage_function_arity_dispatch(F&& f) :
    stage_function_unary<F>(std::forward<F>(f)) { }

  gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch const& other) :
    stage_function_unary<F>(other) {
  }

  MARE_DELETE_METHOD(
    gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch && other));
  MARE_DELETE_METHOD(
    gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch const& other));
  MARE_DELETE_METHOD(
    gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch&& other));
};

template <typename F, typename RT>
class gpu_stage_function_arity_dispatch<F, 2, RT> :
  public stage_function_binary<F> {

public:

  typedef typename stage_function_binary<F>::arg0_type      context_type;
  using context_type_noref = typename std::remove_reference<context_type>::type;
  typedef typename stage_function_binary<F>::return_type    return_type;
  typedef typename stage_function_binary<F>::arg1_type      arg1_type;

  explicit gpu_stage_function_arity_dispatch(F&& f) :
    stage_function_binary<F>(std::forward<F>(f)) {
  }

  gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch const& other) :
    stage_function_binary<F>(other) {
  }

  template<typename InputType, typename IBT>
  RT apply_before_sync(
    size_t in_first_idx,
    size_t in_size,
    IBT* inbuf,
    void* context,
    pipeline_launch_type& launch_type) {

    size_t iid = inbuf->get_buffer_index(in_first_idx);

    stage_input<InputType> input(
      static_cast<stagebuffer*>(inbuf),
      iid, in_size, in_first_idx, launch_type);

    return this->_f(*static_cast<context_type_noref*>(context), input);
  }

  template<typename GKTuple, typename OBT>
  void apply_after_sync(
      size_t out_idx,
      OBT* outbuf,
      void* context,
      GKTuple& gk_tuple)  {
    if(outbuf) {
      size_t oid = outbuf->get_buffer_index(out_idx);
      (*outbuf)[oid] = this->_f(*static_cast<context_type_noref*>(context), gk_tuple);
    } else {
      this->_f(*static_cast<context_type_noref*>(context), gk_tuple);
    }
  }

  MARE_DELETE_METHOD(gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch && other));
  MARE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch const& other));
  MARE_DELETE_METHOD(
    gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch&& other));
};

template <typename F>
class gpu_stage_function_arity_dispatch<F, 2, void> :
  public stage_function_binary<F> {

public:

  typedef typename stage_function_binary<F>::arg0_type      context_type;
  typedef typename stage_function_binary<F>::return_type    return_type;
  typedef typename stage_function_binary<F>::arg1_type      arg1_type;

  explicit gpu_stage_function_arity_dispatch(F&& f) :
    stage_function_binary<F>(std::forward<F>(f)) {
  }

  gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch const& other) :
    stage_function_binary<F>(other) {
  }

  MARE_DELETE_METHOD(gpu_stage_function_arity_dispatch(gpu_stage_function_arity_dispatch && other));
  MARE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch const& other));
  MARE_DELETE_METHOD(gpu_stage_function_arity_dispatch& operator=(gpu_stage_function_arity_dispatch&& other));
};

template <typename F>
class gpu_stage_sync_function:
    public gpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value,
                                             typename function_traits<F>::return_type >{
public:

  typedef typename function_traits<F>::type_in_task   f_type;
  typedef typename function_traits<F>::return_type      return_type;
  typedef typename gpu_stage_function_arity_dispatch<F,
                                                     function_traits<F>::arity::value,
    typename function_traits<F>::return_type>::context_type context_type;

  typedef typename gpu_stage_function_arity_dispatch<F,
                                                     function_traits<F>::arity::value,
    typename function_traits<F>::return_type>::arg1_type arg1_type;

  explicit gpu_stage_sync_function(F&& f) :
    gpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, return_type>(
     std::forward<F>(f)) { }

  gpu_stage_sync_function(gpu_stage_sync_function<F> const& other):
    gpu_stage_function_arity_dispatch<F, function_traits<F>::arity::value, return_type>(
     other) { }

  virtual ~gpu_stage_sync_function()
  { }

  MARE_DELETE_METHOD(gpu_stage_sync_function(gpu_stage_sync_function<F> && other));
  MARE_DELETE_METHOD(gpu_stage_sync_function<F>& operator=(gpu_stage_sync_function<F> const& other));
  MARE_DELETE_METHOD(gpu_stage_sync_function<F>& operator=(gpu_stage_sync_function<F> && other));
};

template <>
class gpu_stage_sync_function<void*> {
public:

  typedef void f_type;
  typedef void return_type;
  typedef void context_type;
  typedef void arg1_type;

  explicit gpu_stage_sync_function(void*) {}

  gpu_stage_sync_function(gpu_stage_sync_function<void*> const& ) { }

  virtual ~gpu_stage_sync_function()
  { }

  template<typename GKTuple, typename OBT>
  void apply_after_sync(
      size_t,
      OBT*,
      void*,
      GKTuple&)  {}

  MARE_DELETE_METHOD(gpu_stage_sync_function(gpu_stage_sync_function<void*> && other));
  MARE_DELETE_METHOD(gpu_stage_sync_function<void>& operator=(gpu_stage_sync_function<void*> const& other));
  MARE_DELETE_METHOD(gpu_stage_sync_function<void>& operator=(gpu_stage_sync_function<void*> && other));
};

#endif

};
};
