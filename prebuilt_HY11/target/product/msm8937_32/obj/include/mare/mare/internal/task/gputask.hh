// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef MARE_HAVE_GPU

#include <mare/buffer.hh>
#include <mare/internal/legacy/gpukernel.hh>
#include <mare/internal/buffer/bufferpolicy.hh>
#include <mare/internal/device/cldevice.hh>
#include <mare/internal/device/clevent.hh>
#include <mare/internal/task/task.hh>

namespace mare {

template <typename T>
struct buffer_attrs {
  typedef T type;
};

template<typename T>
class local {};

namespace internal {

class task_bundle_dispatch;

class gputask_base : public task {
protected:
  using dyn_sized_bas = buffer_acquire_set<0, false>;

  bool _executed;

  bool _does_bundle_dispatch;

  bool _first_execution;

  virtual void release_arguments() = 0;
public:
  explicit gputask_base(legacy::task_attrs a) :
    task(nullptr, a)
    , _executed(false)
    , _does_bundle_dispatch(false)
    , _first_execution(true)
  {}

  void configure_for_bundle_dispatch() {
    _does_bundle_dispatch = true;
  }

  virtual void update_buffer_acquire_set(dyn_sized_bas& bas) = 0;

  bool has_executed() const { return _executed; }

  virtual executor_device get_executor_device() const = 0;

  virtual bool do_execute(task_bundle_dispatch* tbd = nullptr) = 0;

  void on_finish() {
    if(!_does_bundle_dispatch)
      release_arguments();
  }
};

namespace gpu_kernel_dispatch {

template<typename T>
struct cl_arg_setter
{
  template<typename Kernel>
  static void set(Kernel* kernel,
                  size_t index,
                  T& arg) {
    MARE_DLOG("Setting kernel argument[%zu]: value type", index);
    kernel->set_arg(index, arg);
  }
};

struct cl_arg_local_alloc_setter
{
  template<typename Kernel>
  static void set(Kernel* kernel,
                  size_t index,
                  size_t num_bytes_to_local_alloc) {
    MARE_DLOG("Setting kernel argument[%zu]: local_alloc %zu bytes", index, num_bytes_to_local_alloc);
    kernel->set_arg_local_alloc(index, num_bytes_to_local_alloc);
  }
};

struct input_output_param{};
struct input_param{};
struct output_param{};
struct const_buffer{};
struct non_const_buffer{};

struct cl_arg_api20_buffer_setter
{
#ifdef MARE_HAVE_OPENCL
  template<typename BufferPtr, typename BufferAcquireSet>
  static void set(::mare::internal::legacy::device_ptr const& device,
                  internal::clkernel* kernel,
                  size_t i,
                  BufferPtr& b,
                  input_param,
                  BufferAcquireSet& bas)
  {
    MARE_DLOG("Setting kernel argument[%zu]: mare::buffer_ptr in", i);

    MARE_API_ASSERT(b != nullptr,
                    "Need non-null buffer_ptr to execute with gpu kernel");

    MARE_UNUSED(device);

    auto acquired_arena = bas.find_acquired_arena(b);

    auto cl_a = static_cast<mare::internal::cl_arena*>(acquired_arena);
    MARE_INTERNAL_ASSERT(cl_a != nullptr,
                         "A cl_arena or a derived arena must be present after the request_acquire_action() succeeds");
    cl_a->acquire(executor_device::gpucl);
    kernel->set_arg(i, cl_a->get_ocl_buffer());
  }
#endif

#ifdef MARE_HAVE_GLES
  template<typename BufferPtr, typename BufferAcquireSet>
  static void set(::mare::internal::legacy::device_ptr const& device,
                  internal::glkernel* kernel,
                  size_t i,
                  BufferPtr& b,
                  input_param,
                  BufferAcquireSet& bas)
  {
    MARE_DLOG("Setting kernel argument[%zu]: mare::buffer_ptr in", i);

    MARE_API_ASSERT(b != nullptr,
                    "Need non-null buffer_ptr to execute with gpu kernel");

    MARE_UNUSED(device);

    auto acquired_arena = bas.find_acquired_arena(b);

    auto gl_a = static_cast<mare::internal::gl_arena*>(acquired_arena);
    MARE_INTERNAL_ASSERT(gl_a != nullptr,
                         "A gl_arena or a derived arena must be present after the request_acquire_action() succeeds");
    gl_a->acquire(executor_device::gpugl);
    kernel->set_arg(i, gl_a->get_id());
  }
#endif

#ifdef MARE_HAVE_OPENCL
  template<typename BufferPtr, typename BufferAcquireSet>
  static void set(::mare::internal::legacy::device_ptr const& device,
                  internal::clkernel* kernel,
                  size_t i,
                  BufferPtr& b,
                  output_param,
                  BufferAcquireSet& bas)
  {
    MARE_DLOG("Setting kernel argument[%zu]: mare::buffer_ptr out", i);

    MARE_UNUSED(device);

    auto acquired_arena = bas.find_acquired_arena(b);

    auto cl_a = static_cast<mare::internal::cl_arena*>(acquired_arena);
    MARE_INTERNAL_ASSERT(cl_a != nullptr,
                         "A cl_arena or a derived arena must be present after the request_acquire_action() succeeds");
    cl_a->acquire(executor_device::gpucl);
    kernel->set_arg(i, cl_a->get_ocl_buffer());
  }
#endif

#ifdef MARE_HAVE_GLES
  template<typename BufferPtr, typename BufferAcquireSet>
  static void set(::mare::internal::legacy::device_ptr const& device,
                  internal::glkernel* kernel,
                  size_t i,
                  BufferPtr& b,
                  output_param,
                  BufferAcquireSet& bas)
  {
    MARE_DLOG("Setting kernel argument[%zu]: mare::buffer_ptr out", i);

    MARE_UNUSED(device);

    auto acquired_arena = bas.find_acquired_arena(b);

    auto gl_a = static_cast<mare::internal::gl_arena*>(acquired_arena);
    MARE_INTERNAL_ASSERT(gl_a != nullptr,
                         "A gl_arena or a derived arena must be present after the request_acquire_action() succeeds");
    gl_a->acquire(executor_device::gpugl);
    kernel->set_arg(i, gl_a->get_id());
  }
#endif

#ifdef MARE_HAVE_OPENCL
  template<typename BufferPtr, typename BufferAcquireSet>
  static void set(::mare::internal::legacy::device_ptr const& device,
                  internal::clkernel* kernel,
                  size_t i,
                  BufferPtr& b,
                  input_output_param,
                  BufferAcquireSet& bas)
  {
    MARE_DLOG("Setting kernel argument[%zu]: mare::buffer_ptr inout", i);

    MARE_UNUSED(device);

    auto acquired_arena = bas.find_acquired_arena(b);

    auto cl_a = static_cast<mare::internal::cl_arena*>(acquired_arena);
    MARE_INTERNAL_ASSERT(cl_a != nullptr,
                         "A cl_arena or a derived arena must be present after the request_acquire_action() succeeds");
    cl_a->acquire(executor_device::gpucl);
    kernel->set_arg(i, cl_a->get_ocl_buffer());
  }
#endif

#ifdef MARE_HAVE_GLES
  template<typename BufferPtr, typename BufferAcquireSet>
  static void set(::mare::internal::legacy::device_ptr const& device,
                  internal::glkernel* kernel,
                  size_t i,
                  BufferPtr& b,
                  input_output_param,
                  BufferAcquireSet& bas)
  {
    MARE_DLOG("Setting kernel argument[%zu]: mare::buffer_ptr inout", i);

    MARE_UNUSED(device);

    auto acquired_arena = bas.find_acquired_arena(b);

    auto gl_a = static_cast<mare::internal::gl_arena*>(acquired_arena);
    MARE_INTERNAL_ASSERT(gl_a != nullptr,
                         "A gl_arena or a derived arena must be present after the request_acquire_action() succeeds");
    gl_a->acquire(executor_device::gpugl);
    kernel->set_arg(i, gl_a->get_id());
  }
#endif

};

struct cl_arg_texture_setter {
#ifdef MARE_HAVE_OPENCL
  static void set(::mare::internal::legacy::device_ptr const&,
                  internal::clkernel* kernel,
                  size_t i,
                  mare::graphics::internal::base_texture_cl* tex_cl)
  {
    MARE_INTERNAL_ASSERT(tex_cl != nullptr, "Null texture_cl");
    MARE_DLOG("Setting kernel argument[%zu]: texture type. Parameter: in_out",
              i);
    kernel->set_arg(i, tex_cl);
  }
#endif

#if defined(MARE_HAVE_OPENCL) && defined(MARE_HAVE_GLES)
  static void set(::mare::internal::legacy::device_ptr const&,
                  internal::glkernel* ,
                  size_t ,
                  mare::graphics::internal::base_texture_cl* )
  {
    MARE_UNIMPLEMENTED("Textures not implemented in GL compute path");
  }
#endif

};

struct cl_arg_sampler_setter {
#ifdef MARE_HAVE_OPENCL
  template<typename SamplerPtr>
  static void set(::mare::internal::legacy::device_ptr const&,
                  internal::clkernel* kernel,
                  size_t i,
                  SamplerPtr& sampler_ptr)
  {
    MARE_DLOG("Setting kernel argument[%zu]: sampler type. Parameter: in_out",
              i);
    kernel->set_arg(i, sampler_ptr);
  }
#endif

#ifdef MARE_HAVE_GLES
  template<typename SamplerPtr>
  static void set(::mare::internal::legacy::device_ptr const&,
                  internal::glkernel* ,
                  size_t ,
                  SamplerPtr& )
  {
    MARE_UNIMPLEMENTED("Samplers not implemented in GL compute path");
  }
#endif

};

MARE_GCC_IGNORE_BEGIN("-Weffc++");

template<typename T>
struct is_local_alloc: public std::false_type
{
  typedef void data_type;
};

template<typename U>
struct is_local_alloc<mare::local<U>>: public std::true_type
{
  typedef U data_type;
};

template<typename T>
struct is_texture_ptr : public std::false_type {};

#ifdef MARE_HAVE_OPENCL
template <mare::graphics::image_format ImageFormat, int Dims>
struct is_texture_ptr<mare::graphics::texture_ptr<ImageFormat, Dims>> : public std::true_type {};
#endif

template<typename T>
struct is_sampler_ptr : public std::false_type {};

#ifdef MARE_HAVE_OPENCL
template <mare::graphics::addressing_mode addrMode, mare::graphics::filter_mode filterMode>
struct is_sampler_ptr<mare::graphics::sampler_ptr<addrMode, filterMode>> : public std::true_type {};
#endif

MARE_GCC_IGNORE_END("-Weffc++");

template<typename T> struct kernel_arg_ptr_traits;

template<typename T>
struct kernel_arg_ptr_traits< ::mare::buffer_ptr<T> > {
  using naked_type = typename std::remove_cv<T>::type;
  using is_const   = typename std::is_const<T>;
};

template<typename T> struct kernel_param_ptr_traits;

template<typename T>
struct kernel_param_ptr_traits<mare::buffer_ptr<T>> {
  using naked_type = typename std::remove_cv<T>::type;
  using is_const  = std::is_const<T>;
  using direction = typename std::conditional<is_const::value, input_param, input_output_param>::type;
};

template<typename T>
struct kernel_param_ptr_traits<mare::in<mare::buffer_ptr<T>>> {
  using naked_type = typename std::remove_cv<T>::type;
  using is_const  = std::is_const<T>;
  using direction = input_param;
};

template<typename T>
struct kernel_param_ptr_traits<mare::out<mare::buffer_ptr<T>>> {
  using naked_type = typename std::remove_cv<T>::type;
  using is_const  = std::is_const<T>;
  using direction = output_param;
};

template<typename T>
struct kernel_param_ptr_traits<mare::inout<mare::buffer_ptr<T>>> {
  using naked_type = typename std::remove_cv<T>::type;
  using is_const  = std::is_const<T>;
  using direction = input_output_param;
};

#ifdef MARE_HAVE_OPENCL

template<typename Param, typename Arg, typename Kernel>
void parse_normal_arg(::mare::internal::legacy::device_ptr const&,
                      Kernel* kernel,
                      size_t index,
                      Arg& arg,
                      bool do_not_dispatch)
{

  static_assert(!is_api20_buffer_ptr<Param>::value, "Expected buffer_ptr argument");
  static_assert(!is_texture_ptr<Param>::value, "Expected texture_ptr or buffer_ptr argument");
  if(do_not_dispatch == false) {
    cl_arg_setter<Arg>::set(kernel, index, arg);
  }
}
#endif

template<typename Param, typename Arg, typename Kernel>
void parse_local_alloc_arg(::mare::internal::legacy::device_ptr const&,
                           Kernel* kernel,
                           size_t index,
                           Arg& arg,
                           bool do_not_dispatch)
{
  static_assert(is_local_alloc<Param>::value, "Expected a mare::local parameter");
  size_t sizeof_T = sizeof(typename is_local_alloc<Param>::data_type);

  size_t num_bytes_to_local_alloc = sizeof_T * arg;
  if(do_not_dispatch == false) {
    cl_arg_local_alloc_setter::set(kernel, index, num_bytes_to_local_alloc);
  }
}

template<typename Param, typename Arg, typename BufferAcquireSet, typename Kernel>
void parse_api20_buffer_ptr_arg_dispatch(std::true_type,
                                         ::mare::internal::legacy::device_ptr const& device,
                                         Kernel* kernel,
                                         size_t index,
                                         Arg& arg,
                                         BufferAcquireSet& bas,
                                         bool do_not_dispatch)
{
  static_assert(mare::internal::is_api20_buffer_ptr<Param>::value, "Param must be a mare::buffer_ptr");

  typedef kernel_param_ptr_traits<Param> param_traits;
  using direction = typename param_traits::direction;

  typedef kernel_arg_ptr_traits<Arg> arg_traits;

  static_assert(std::is_same<typename param_traits::naked_type, typename arg_traits::naked_type>::value == true,
                "Incompatible mare::buffer_ptr data types");

  static_assert(param_traits::is_const::value == false || std::is_same<direction, input_param>::value == true,
                "buffer_ptr of const data type may only be declared as input to kernel");

  static_assert(arg_traits::is_const::value == false || std::is_same<direction, input_param>::value == true,
                "A buffer_ptr<const T> argument may only be passed as a kernel input");

  if(do_not_dispatch == true) {

    auto acquire_action = (std::is_same<direction, input_param>::value == true ? bufferpolicy::acquire_r :
                           (std::is_same<direction, output_param>::value == true ? bufferpolicy::acquire_w :
                                                                                   bufferpolicy::acquire_rw));
    bas.add(arg, acquire_action);
  }
  else {

    cl_arg_api20_buffer_setter::set(device,
                                    kernel,
                                    index,
                                    arg,
                                    direction(),
                                    bas);
  }
}

#ifdef MARE_HAVE_OPENCL

template<typename Param, typename Arg, typename BufferAcquireSet, typename Kernel>
void parse_api20_buffer_ptr_arg_dispatch(std::false_type,
                                         ::mare::internal::legacy::device_ptr const& device,
                                         Kernel* kernel,
                                         size_t index,
                                         Arg& arg,
                                         BufferAcquireSet& bas,
                                         bool do_not_dispatch)
{
  static_assert(is_texture_ptr<Param>::value, "Param must be a mare::texture_ptr");

  if(do_not_dispatch == true) {

    auto acquire_action = bufferpolicy::acquire_rw;

    auto& bb = reinterpret_cast<mare::internal::buffer_ptr_base&>(arg);
    auto buf_as_tex_info = buffer_accessor::get_buffer_as_texture_info(bb);

    using tpinfo = mare::graphics::internal::texture_ptr_info<Param>;
    MARE_API_ASSERT(buf_as_tex_info.get_used_as_texture(),
                    "buffer_ptr is not setup to be treated as texture");
    MARE_INTERNAL_ASSERT(buf_as_tex_info.get_dims() > 0 && buf_as_tex_info.get_dims() <= 3,
                         "buffer_ptr treated as texture of invalid dimension");
    MARE_API_ASSERT(buf_as_tex_info.get_dims() == tpinfo::dims,
                    "buffer_ptr is setup to be interpreted as %d dimensional, but kernel requires %d dimensions",
                    buf_as_tex_info.get_dims(),
                    tpinfo::dims);

    auto bufstate = c_ptr(buffer_accessor::get_bufstate(bb));
    MARE_INTERNAL_ASSERT(bufstate != nullptr, "Null bufstate");
    auto preexisting_tex_a = static_cast<texture_arena*>(bufstate->get(TEXTURE_ARENA));
    if(preexisting_tex_a == nullptr || !preexisting_tex_a->has_format()) {

      MARE_DLOG("Allocating new texture to save in texture_arena of bufstate=%p", bufstate);
      bb.allocate_host_accessible_data();
      bool has_ion_ptr = false;
#ifdef MARE_HAVE_QTI_HEXAGON
      auto preexisting_ion_a = static_cast<ion_arena*>(bufstate->get(ION_ARENA));
      has_ion_ptr = (preexisting_ion_a != nullptr &&
                     arg.get_host_accessible_data() == preexisting_ion_a->get_ptr());
#endif
      auto tex_cl = new mare::graphics::internal::texture_cl
                                     <tpinfo::img_format, tpinfo::dims>
                                     (mare::graphics::internal::image_size_converter<tpinfo::dims, 3>::
                                                                    convert(buf_as_tex_info.get_img_size()),
                                      false,
                                      arg.get_host_accessible_data(),
                                      has_ion_ptr,
                                      false);
      MARE_INTERNAL_ASSERT(tex_cl != nullptr,
                           "internal texture_cl creation failed for bufferstate=%p",
                           ::mare::internal::c_ptr(buffer_accessor::get_bufstate(bb)));
      MARE_DLOG("Created tex_cl=%p for bufstate=%p",
                tex_cl,
                ::mare::internal::c_ptr(buffer_accessor::get_bufstate(bb)));
      buf_as_tex_info.access_tex_cl() = tex_cl;
    }

    bas.add(arg, acquire_action, buf_as_tex_info);
  }
  else {

    auto acquired_arena = bas.find_acquired_arena(arg);

    auto t_a = static_cast<texture_arena*>(acquired_arena);
    MARE_INTERNAL_ASSERT(t_a != nullptr,
                         "A texture arena must be present after request_acquire_action() succeeds");

    t_a->acquire(executor_device::gputexture);

    auto tex_cl = t_a->get_texture();

    cl_arg_texture_setter::set(device,
                               kernel,
                               index,
                               tex_cl);
  }
}
#endif

template<typename Param, typename Arg, typename BufferAcquireSet, typename Kernel>
void parse_api20_buffer_ptr_arg(::mare::internal::legacy::device_ptr const& device,
                                Kernel* kernel,
                                size_t index,
                                Arg& arg,
                                BufferAcquireSet& bas,
                                bool do_not_dispatch)
{
  static_assert(mare::internal::is_api20_buffer_ptr<Param>::value || is_texture_ptr<Param>::value,
                "Unexpected mare::buffer_ptr argument");

  parse_api20_buffer_ptr_arg_dispatch<Param, Arg, BufferAcquireSet>
                                     (mare::internal::is_api20_buffer_ptr<Param>(),
                                      device,
                                      kernel,
                                      index,
                                      arg,
                                      bas,
                                      do_not_dispatch);

}

#ifdef MARE_HAVE_OPENCL

template<typename Param, typename Arg, typename Kernel>
void parse_texture_ptr_arg(::mare::internal::legacy::device_ptr const& device,
                          Kernel* kernel,
                          size_t index,
                          Arg& arg,
                          bool do_not_dispatch)
{

  static_assert(is_texture_ptr<Param>::value, "Unexpected texture_ptr argument");

  static_assert(std::is_same<Param, Arg>::value,
                "Incompatible texture_ptr types");

  if(do_not_dispatch == false) {
    cl_arg_texture_setter::set(device,
                               kernel,
                               index,
                               internal::c_ptr(arg));
  }
}
#endif

#ifdef MARE_HAVE_OPENCL

template<typename Param, typename Arg, typename Kernel>
void parse_sampler_ptr_arg(::mare::internal::legacy::device_ptr const& device,
                          Kernel* kernel,
                          size_t index,
                          Arg& arg,
                          bool do_not_dispatch)
{

  static_assert(is_sampler_ptr<Param>::value, "Expected sampler_ptr argument");

  static_assert(std::is_same<Param, Arg>::value,
                "Incompatible sampler_ptr types");

  if(do_not_dispatch == false) {
    cl_arg_sampler_setter::set(device,
                               kernel,
                               index,
                               arg);
  }
}
#endif

enum class dispatch_type
{
  normal,
  local_alloc,
  api20_buffer,
  texture,
  sampler
};

template<bool is_arg_a_local_alloc,
         bool is_arg_an_api20_buffer_ptr,
         bool is_arg_a_texture_ptr,
         bool is_arg_a_sampler_ptr>
struct translate_flags_to_dispatch_type;

template<>
struct translate_flags_to_dispatch_type<false, false, false, false>
{
  static auto const value = dispatch_type::normal;
};

template<>
struct translate_flags_to_dispatch_type<true, false, false, false>
{
  static auto const value = dispatch_type::local_alloc;
};

template<>
struct translate_flags_to_dispatch_type<false, true, false, false>
{
  static auto const value = dispatch_type::api20_buffer;
};

template<>
struct translate_flags_to_dispatch_type<false, false, true, false>
{
  static auto const value = dispatch_type::texture;
};

template<>
struct translate_flags_to_dispatch_type<false, false, false, true>
{
  static auto const value = dispatch_type::sampler;
};

#ifdef MARE_HAVE_OPENCL

template<typename Param, typename Arg, dispatch_type DispatchType, typename BufferAcquireSet, typename Kernel>
typename std::enable_if<DispatchType == dispatch_type::normal, void>::type
parse_arg_dispatch(::mare::internal::legacy::device_ptr const& device,
                        Kernel* kernel,
                        size_t index,
                        Arg& arg,
                        BufferAcquireSet&,
                        bool do_not_dispatch)
{
  parse_normal_arg<Param, Arg>(device, kernel, index, arg, do_not_dispatch);
}
#endif

#ifdef MARE_HAVE_OPENCL
template<typename Param, typename Arg, dispatch_type DispatchType, typename BufferAcquireSet, typename Kernel>
typename std::enable_if<DispatchType == dispatch_type::local_alloc, void>::type
parse_arg_dispatch(::mare::internal::legacy::device_ptr const& device,
                        Kernel* kernel,
                        size_t index,
                        Arg& arg,
                        BufferAcquireSet&,
                        bool do_not_dispatch)
{
  parse_local_alloc_arg<Param, Arg>(device, kernel, index, arg, do_not_dispatch);
}
#endif

template<typename Param, typename Arg, dispatch_type DispatchType, typename BufferAcquireSet, typename Kernel>
typename std::enable_if<DispatchType == dispatch_type::api20_buffer, void>::type
parse_arg_dispatch(::mare::internal::legacy::device_ptr const& device,
                        Kernel* kernel,
                        size_t index,
                        Arg& arg,
                        BufferAcquireSet& bas,
                        bool do_not_dispatch)
{
  parse_api20_buffer_ptr_arg<Param, Arg>(device,
                                         kernel,
                                         index,
                                         arg,
                                         bas,
                                         do_not_dispatch);
}

#ifdef MARE_HAVE_OPENCL
template<typename Param, typename Arg, dispatch_type DispatchType, typename BufferAcquireSet, typename Kernel>
typename std::enable_if<DispatchType == dispatch_type::texture, void>::type
parse_arg_dispatch(::mare::internal::legacy::device_ptr const& device,
                        Kernel* kernel,
                        size_t index,
                        Arg& arg,
                        BufferAcquireSet&,
                        bool do_not_dispatch)
{
  parse_texture_ptr_arg<Param, Arg>(device, kernel, index, arg, do_not_dispatch);
}
#endif

#ifdef MARE_HAVE_OPENCL
template<typename Param, typename Arg, dispatch_type DispatchType, typename BufferAcquireSet, typename Kernel>
typename std::enable_if<DispatchType == dispatch_type::sampler, void>::type
parse_arg_dispatch(::mare::internal::legacy::device_ptr const& device,
                        Kernel* kernel,
                        size_t index,
                        Arg& arg,
                        BufferAcquireSet&,
                        bool do_not_dispatch)
{
  parse_sampler_ptr_arg<Param, Arg>(device, kernel, index, arg, do_not_dispatch);
}
#endif

template<typename Params, typename Args, size_t index = 0, typename BufferAcquireSet, typename Kernel>
typename std::enable_if<index == std::tuple_size<Args>::value, void>::type
prepare_args_pass(::mare::internal::legacy::device_ptr const&,
                  Kernel*,
                  Args&&,
                  BufferAcquireSet&,
                  bool)
{

}

template<typename Params, typename Args, size_t index = 0, typename BufferAcquireSet, typename Kernel>
typename std::enable_if<index < std::tuple_size<Args>::value, void>::type
prepare_args_pass(::mare::internal::legacy::device_ptr const& device,
                  Kernel* kernel,
                  Args&& args,
                  BufferAcquireSet& bas,
                  bool do_not_dispatch)
{

  static_assert(std::tuple_size<Args>::value == std::tuple_size<Params>::value,
                "Number of parameters is different to number of arguments");

  typedef typename std::tuple_element<index, Params>::type param_type;
  typedef typename std::tuple_element<index, Args>::type arg_type;

  typedef typename is_local_alloc<param_type>::type is_param_a_local_alloc;
  typedef typename mare::internal::is_api20_buffer_ptr<arg_type>::type is_arg_an_api20_buffer_ptr;
  typedef typename is_texture_ptr<arg_type>::type is_arg_a_texture_ptr;
  typedef typename is_sampler_ptr<arg_type>::type is_arg_a_sampler_ptr;

  parse_arg_dispatch<param_type,
                     arg_type,
                     translate_flags_to_dispatch_type<is_param_a_local_alloc::value,
                                                      is_arg_an_api20_buffer_ptr::value,
                                                      is_arg_a_texture_ptr::value,
                                                      is_arg_a_sampler_ptr::value>::value>
                    (device,
                     kernel,
                     index,
                     std::get<index>(args),
                     bas,
                     do_not_dispatch);

  prepare_args_pass<Params, Args, index + 1>
                   (device,
                    kernel,
                    std::forward<Args>(args),
                    bas,
                    do_not_dispatch);
}

template<typename Params, typename Args, typename BufferAcquireSet, typename Kernel>
bool
prepare_args(::mare::internal::legacy::device_ptr const& device,
             Kernel* k_ptr,
             Args&& args,
             BufferAcquireSet& bas,
             void const* requestor,
             bool add_buffers = true,
             bool acquire_buffers = true,
             bool dispatch_args = true)
{
  MARE_INTERNAL_ASSERT(k_ptr != nullptr, "null kernel");

  if (add_buffers) {

    if(0) {}
#ifdef MARE_HAVE_OPENCL
    else if(k_ptr->is_cl())
      prepare_args_pass<Params, Args>
                       (device,
                        k_ptr->get_cl_kernel(),
                        std::forward<Args>(args),
                        bas,
                        true);
#endif
#ifdef MARE_HAVE_GLES
    else if(k_ptr->is_gl())
      prepare_args_pass<Params, Args>
                       (device,
                        k_ptr->get_gl_kernel(),
                        std::forward<Args>(args),
                        bas,
                        true);
#endif
    else
      MARE_FATAL("Unsupported type of GPU kernel");
  }

  if(acquire_buffers) {
    mare::internal::executor_device ed = mare::internal::executor_device::unspecified;
#ifdef MARE_HAVE_OPENCL
    if(k_ptr->is_cl())
      ed = mare::internal::executor_device::gpucl;
#endif
#ifdef MARE_HAVE_GLES
    if(k_ptr->is_gl())
      ed = mare::internal::executor_device::gpugl;
#endif

    bas.acquire_buffers(requestor,
                        ed,
                        true);
    if (bas.get_acquire_status() == false) {
      return false;
    }
  }

  if(dispatch_args) {

    if(0) {}
#ifdef MARE_HAVE_OPENCL
    else if(k_ptr->is_cl())
      prepare_args_pass<Params, Args>
                       (device,
                        k_ptr->get_cl_kernel(),
                        std::forward<Args>(args),
                        bas,
                        false);
#endif
#ifdef MARE_HAVE_GLES
    else if(k_ptr->is_gl())
      prepare_args_pass<Params, Args>
                       (device,
                        k_ptr->get_gl_kernel(),
                        std::forward<Args>(args),
                        bas,
                        false);
#endif
    else
      MARE_FATAL("Unsupported type of GPU kernel");
  }

  return true;
}

};

#ifdef MARE_HAVE_OPENCL
void CL_CALLBACK task_bundle_completion_callback(cl_event event, cl_int, void* user_data);
#endif

class task_bundle_dispatch {

  std::vector<gputask_base*> _tasks;

  unsigned int _depth;

  using dyn_sized_bas = buffer_acquire_set<0, false>;
  dyn_sized_bas _bas;

  void add(task* t, bool first = false) {
    MARE_DLOG("task_bundle_dispatch() %p: adding gputask %p", this, t);
    MARE_INTERNAL_ASSERT(t != nullptr, "Cannot add a null task to task_bundle_dispatch");
    auto gt = static_cast<gputask_base*>(t);
    MARE_INTERNAL_ASSERT(gt != nullptr, "task_bundle_dispatch: received a non-GPU task");
    if (!first) {
      _tasks.push_back(gt);
    } else {
      MARE_INTERNAL_ASSERT(_tasks[0] == nullptr,
          "first element must be written exactly once");
      _tasks[0] = gt;
    }
    gt->configure_for_bundle_dispatch();
    gt->update_buffer_acquire_set(_bas);
  }

  void add_root(task* t) {
    add(t, true);
  }

public:

  task_bundle_dispatch() :
    _tasks(),
    _depth(0),
    _bas()
  {
    _tasks.reserve(8);
    _tasks.push_back(nullptr);

  }

  void add_non_root(task* t) {
    add(t, false);
  }

  void execute_all(task* root_task) {
    MARE_INTERNAL_ASSERT(_tasks.size() > 0,
                        "task_bundle_dispatch should be called on bundles of"
                        "at least two tasks (including the root_task)");
    MARE_INTERNAL_ASSERT(_depth == 0,
                        "Can call execute_all only at scheduling depth of zero");

    add_root(root_task);

    MARE_DLOG("task_bundle_dispatch %p: execute_all()", this);

    MARE_INTERNAL_ASSERT(root_task != nullptr, "root task is null");
    void const* requestor = root_task;

    auto gpu_root_task = static_cast<gputask_base*>(root_task);
    MARE_INTERNAL_ASSERT(gpu_root_task != nullptr, "Current limitation: root task must be a gputask.");
    auto ed = gpu_root_task->get_executor_device();

    _bas.blocking_acquire_buffers(requestor, ed);
    for(auto& gt : _tasks) {
      MARE_INTERNAL_ASSERT(gt != nullptr, "Null gputask found during bundle dispatch");
      MARE_INTERNAL_ASSERT(gt->get_executor_device() == ed, "Mismatch in executor device type in bundle");
      gt->execute_sync(this);
    }
  }

  std::vector<gputask_base*> const& get_tasks() { return _tasks; }

  void increment_depth() { _depth++; }

  void decrement_depth() { _depth--; }

  unsigned int get_depth() const { return _depth; }

  bool contains_many() const { return _tasks.size() > 1; }

  dyn_sized_bas& get_buffer_acquire_set() { return _bas; }
};

#ifdef MARE_HAVE_OPENCL
void CL_CALLBACK completion_callback(cl_event event, cl_int, void* user_data);
#endif

template<typename TupledParams,
         size_t   index = std::tuple_size<TupledParams>::value,
         typename ...ExtractedKernelArgs>
struct kernel_ptr_for_tupled_params
{
  using type = typename kernel_ptr_for_tupled_params<TupledParams,
                                                     index-1,
                                                     typename std::tuple_element<index-1, TupledParams>::type,
                                                     ExtractedKernelArgs...>::type;
};

template<typename TupledParams, typename ...ExtractedKernelArgs>
struct kernel_ptr_for_tupled_params<TupledParams, 0, ExtractedKernelArgs...>
{
  using type = ::mare::internal::legacy::kernel_ptr<ExtractedKernelArgs...>;
};

template<size_t Dims, typename Fn, typename Params, typename Args>
class gputask : public gputask_base {

private:
  Args _kernel_args;
#ifdef MARE_HAVE_OPENCL
  clevent _cl_completion_event;
#endif
  typename kernel_ptr_for_tupled_params<Params>::type _kernel_ptr;
#ifdef MARE_HAVE_OPENCL
  cldevice* _d_ptr;
#endif
  ::mare::range<Dims> _global_range;
  ::mare::range<Dims> _local_range;
#ifdef MARE_HAVE_GLES
  GLsync _gl_fence;
#endif
  ::mare::internal::legacy::device_ptr const& _device;
  Fn& _fn;
  buffer_acquire_set< num_buffer_ptrs_in_tuple<Args>::value > _buffer_acquire_set;

  MARE_DELETE_METHOD(gputask(gputask const&));
  MARE_DELETE_METHOD(gputask(gputask&&));
  MARE_DELETE_METHOD(gputask& operator=(gputask const&));
  MARE_DELETE_METHOD(gputask& operator=(gputask&&));

  virtual executor_device get_executor_device() const;
  virtual bool do_execute(task_bundle_dispatch* tbd = nullptr);

public:
  template <typename Kernel>
  gputask(::mare::internal::legacy::device_ptr const& device,
          const ::mare::range<Dims>& r,
          const ::mare::range<Dims>& l,
          Fn& f,
          legacy::task_attrs a,
          Kernel kernel,
          Args& kernel_args) :
    gputask_base(a)
    , _kernel_args(kernel_args)
#ifdef MARE_HAVE_OPENCL
    , _cl_completion_event()
#endif
    , _kernel_ptr(kernel)
#ifdef MARE_HAVE_OPENCL
    , _d_ptr( internal::c_ptr(device) )
#endif
    , _global_range(r)
    , _local_range(l)
#ifdef MARE_HAVE_GLES
    , _gl_fence()
#endif
    , _device(device)
    , _fn(f)
    , _buffer_acquire_set()
  {}

  virtual void update_buffer_acquire_set(dyn_sized_bas& bas);

  virtual void release_arguments();
};

template<size_t Dims, typename Fn, typename Params, typename Args>
void
gputask<Dims, Fn, Params, Args>::update_buffer_acquire_set(dyn_sized_bas& bas)
{
  auto k_ptr = internal::c_ptr(_kernel_ptr);
  MARE_INTERNAL_ASSERT((k_ptr != nullptr), "null kernel");

#ifdef MARE_HAVE_OPENCL
  clkernel* kernel = nullptr;
#else
  glkernel* kernel = nullptr;
#endif

  gpu_kernel_dispatch::prepare_args_pass<Params, Args>
                                        (_device,
                                         kernel,
                                         std::forward<Args>(_kernel_args),
                                         bas,
                                         true);
}

template<size_t Dims, typename Fn, typename Params, typename Args>
executor_device
gputask<Dims, Fn, Params, Args>::get_executor_device() const
{
  auto k_ptr = internal::c_ptr(_kernel_ptr);
  MARE_INTERNAL_ASSERT((k_ptr != nullptr), "null kernel");
#ifdef MARE_HAVE_OPENCL
  if(k_ptr->is_cl())
    return executor_device::gpucl;
#endif
#ifdef MARE_HAVE_GLES
  if(k_ptr->is_gl())
    return executor_device::gpugl;
#endif

  MARE_UNREACHABLE("Invalid kernel");
  return executor_device::unspecified;
}

#ifdef MARE_HAVE_OPENCL
MARE_GCC_IGNORE_BEGIN("-Wshadow");
template<typename Device, typename GPUTask, typename CLKernel, typename Range, typename CLEvent>
void launch_cl_kernel(bool is_bundle_dispatch,
                      task_bundle_dispatch* tbd,
                      Device* d_ptr,
                      GPUTask* gputask,
                      CLKernel* cl_kernel,
                      Range& global_range,
                      Range& local_range,
                      CLEvent& cl_completion_event)
{
  cl_completion_event = d_ptr->launch_kernel(cl_kernel, global_range, local_range);

  if(!is_bundle_dispatch || (gputask == tbd->get_tasks().back())) {
    auto cc =  is_bundle_dispatch ? &task_bundle_completion_callback
                                  : &completion_callback;
    auto userdata = is_bundle_dispatch ? static_cast<void*>(tbd)
                                       : static_cast<void*>(gputask);

    cl_completion_event.get_impl().setCallback(CL_COMPLETE,
                                               cc,
                                               userdata);
  }
}
MARE_GCC_IGNORE_END("-Wshadow");
#endif

#ifdef MARE_HAVE_GLES
template<typename GPUTask, typename GLKernel, typename Range, typename GLFence>
void launch_gl_kernel(bool is_bundle_dispatch,
                      GPUTask* gputask,
                      GLKernel* gl_kernel,
                      Range& global_range,
                      Range& local_range,
                      GLFence& gl_fence)
{
  MARE_UNUSED(is_bundle_dispatch);
  MARE_INTERNAL_ASSERT(!is_bundle_dispatch, "GL dispatch does not as yet support bundle dispatch");

  gl_fence = gl_kernel->launch(global_range, local_range);

  GLenum error = glClientWaitSync(gl_fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
  if(GL_TIMEOUT_EXPIRED == error) {
    MARE_DLOG("GL_TIMEOUT_EXPIRED");
  }
  else if((GL_ALREADY_SIGNALED == error) || (GL_CONDITION_SATISFIED == error)) {
    MARE_DLOG("--- GL stub task ---");
    gputask->on_finish();
    gputask->finish(false, nullptr, true, true);
  }
  else if(GL_WAIT_FAILED == error) {
    MARE_DLOG("GL_WAIT_FAILED");
  }
}
#endif

template<size_t Dims, typename Fn, typename Params, typename Args>
bool
gputask<Dims, Fn, Params, Args>::do_execute(task_bundle_dispatch* tbd)
{
  auto k_ptr = internal::c_ptr(_kernel_ptr);
  MARE_INTERNAL_ASSERT((k_ptr != nullptr), "null kernel");

  MARE_INTERNAL_ASSERT(get_snapshot().is_running(),
                       "Can't execute task %p: %s",
                       this, to_string().c_str());

  auto is_bundle_dispatch = tbd != nullptr;

#ifdef MARE_HAVE_OPENCL

#ifdef MARE_HAVE_GLES
  if (is_bundle_dispatch) {
    MARE_DLOG("enqueuing %s task %p for bundle %p", (k_ptr->is_cl() ? "OpenCL" :
                                                      k_ptr->is_gl() ? "OpenGLES" : "INVALID"), this, tbd);
  } else {
    MARE_DLOG("enqueuing %s task %p", (k_ptr->is_cl() ? "OpenCL" :
                                        k_ptr->is_gl() ? "OpenGLES" : "INVALID"), this);
  }
#else
  MARE_INTERNAL_ASSERT(k_ptr->is_cl(), "Only OpenCL configured");
  if (is_bundle_dispatch) {
    MARE_DLOG("enqueuing %s task %p for bundle %p", "OpenCL", this, tbd);
  } else {
    MARE_DLOG("enqueuing %s task %p", "OpenCL", this);
  }
#endif

#else
  MARE_INTERNAL_ASSERT(k_ptr->is_gl(), "Only OpenGLES configured");
  if (is_bundle_dispatch) {
    MARE_DLOG("enqueuing %s task %p for bundle %p", "OpenGLES", this, tbd);
  } else {
    MARE_DLOG("enqueuing %s task %p", "OpenGLES", this);
  }
#endif

#ifdef MARE_HAVE_OPENCL
  MARE_INTERNAL_ASSERT((_d_ptr != nullptr), "null device");
  if(k_ptr->is_cl())
    MARE_DLOG("cl kernel: %p", k_ptr->get_cl_kernel());
#endif
#ifdef MARE_HAVE_GLES
  if(k_ptr->is_gl())
    MARE_DLOG("gl kernel: %p", k_ptr->get_gl_kernel());
#endif

  std::lock_guard<std::mutex> lg(k_ptr->access_dispatch_mutex());

  bool conflict = false;
  if (!is_bundle_dispatch) {
    conflict =
        !gpu_kernel_dispatch::prepare_args<Params, Args>(
                _device,
                k_ptr,
                std::forward<Args>(_kernel_args),
                _buffer_acquire_set,
                this,
                _first_execution,
                true);
  } else {
    gpu_kernel_dispatch::prepare_args<Params, Args>(
        _device,
        k_ptr,
        std::forward<Args>(_kernel_args),
        tbd->get_buffer_acquire_set(),
        this,
        false,
        false);
  }

  if (conflict) {
    _first_execution = false;
    return false;
  }

  if(0) {}
#ifdef MARE_HAVE_OPENCL
  else if(k_ptr->is_cl())
    launch_cl_kernel(is_bundle_dispatch,
                     tbd,
                     _d_ptr,
                     this,
                     k_ptr->get_cl_kernel(),
                     _global_range,
                     _local_range,\
                     _cl_completion_event);
#endif
#ifdef MARE_HAVE_GLES
  else if(k_ptr->is_gl())
    launch_gl_kernel(is_bundle_dispatch,
                     this,
                     k_ptr->get_gl_kernel(),
                     _global_range,
                     _local_range,
                     _gl_fence);
#endif
  else
    MARE_FATAL("Unsupported type of GPU kernel");

  auto guard = get_lock_guard();
  _executed = true;

  return true;
}

template<size_t Dims, typename Fn, typename Params, typename Args>
void
gputask<Dims, Fn, Params, Args>::release_arguments()
{
    MARE_DLOG("release_arguments");
    _buffer_acquire_set.release_buffers(this);
}

};

};
#endif
