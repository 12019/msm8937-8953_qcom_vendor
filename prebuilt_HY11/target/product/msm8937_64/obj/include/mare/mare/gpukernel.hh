// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/task/gpukernel.hh>

namespace mare {

template<typename T>
class local;

namespace beta {

class cl_t {};

class gl_t {};

cl_t const cl {};

gl_t const gl {};

};

#ifdef MARE_HAVE_GPU
MARE_GCC_IGNORE_BEGIN("-Weffc++");

template<typename ...Args>
class gpu_kernel : private internal::gpu_kernel_implementation<Args...>
{
  using parent = internal::gpu_kernel_implementation<Args...>;

  enum { num_kernel_args = sizeof...(Args) };

public:
#ifdef MARE_HAVE_OPENCL

  gpu_kernel(std::string const& cl_kernel_str,
             std::string const& cl_kernel_name,
             std::string const& cl_build_options = "")
    : parent(cl_kernel_str, cl_kernel_name, cl_build_options)
  {}

  gpu_kernel(beta::cl_t const&,
             std::string const& cl_kernel_str,
             std::string const& cl_kernel_name,
             std::string const& cl_build_options = "")
    : parent(cl_kernel_str, cl_kernel_name, cl_build_options)
  {}
#endif

#ifdef MARE_HAVE_GLES

  gpu_kernel(beta::gl_t const&,
             std::string const& gl_kernel_str)
    : parent(gl_kernel_str)
  {}
#endif

#ifdef MARE_HAVE_OPENCL

  gpu_kernel(void const*        cl_kernel_bin,
             size_t             cl_kernel_len,
             std::string const& cl_kernel_name,
             std::string const& cl_build_options = "")
    : parent(cl_kernel_bin, cl_kernel_len, cl_kernel_name, cl_build_options)
  {}

  gpu_kernel(beta::cl_t const&,
             void const*        cl_kernel_bin,
             size_t             cl_kernel_len,
             std::string const& cl_kernel_name,
             std::string const& cl_build_options = "")
    : parent(cl_kernel_bin, cl_kernel_len, cl_kernel_name, cl_build_options)
  {}
#endif

  MARE_DEFAULT_METHOD(gpu_kernel(gpu_kernel const&));
  MARE_DEFAULT_METHOD(gpu_kernel& operator=(gpu_kernel const&));
  MARE_DEFAULT_METHOD(gpu_kernel(gpu_kernel&&));
  MARE_DEFAULT_METHOD(gpu_kernel& operator=(gpu_kernel&&));

  ~gpu_kernel() {}

private:

  template<typename Code, class Enable>
  friend
  struct mare::internal::task_factory_dispatch;
};
MARE_GCC_IGNORE_END("-Weffc++");
#endif

#ifdef MARE_HAVE_OPENCL

template<typename ...Args>
gpu_kernel<Args...> create_gpu_kernel(std::string const& cl_kernel_str,
                                      std::string const& cl_kernel_name,
                                      std::string const& cl_build_options = "")
{
  return gpu_kernel<Args...>(cl_kernel_str, cl_kernel_name, cl_build_options);
}
#endif

namespace beta {

#ifdef MARE_HAVE_OPENCL

template<typename ...Args>
gpu_kernel<Args...> create_gpu_kernel(beta::cl_t const&,
                                      std::string const& cl_kernel_str,
                                      std::string const& cl_kernel_name,
                                      std::string const& cl_build_options = "")
{
  return gpu_kernel<Args...>(beta::cl, cl_kernel_str, cl_kernel_name, cl_build_options);
}
#endif

#ifdef MARE_HAVE_GLES

template<typename ...Args>
gpu_kernel<Args...> create_gpu_kernel(beta::gl_t const&,
                                      std::string const& gl_kernel_str)
{
  return gpu_kernel<Args...>(beta::gl, gl_kernel_str);
}
#endif

};

#ifdef MARE_HAVE_OPENCL

template<typename ...Args>
gpu_kernel<Args...> create_gpu_kernel(void const*        cl_kernel_bin,
                                      size_t             cl_kernel_len,
                                      std::string const& cl_kernel_name,
                                      std::string const& cl_build_options = "")
{
  return gpu_kernel<Args...>(cl_kernel_bin,
                             cl_kernel_len,
                             cl_kernel_name,
                             cl_build_options);
}
#endif

namespace beta {

#ifdef MARE_HAVE_OPENCL

template<typename ...Args>
gpu_kernel<Args...> create_gpu_kernel(beta::cl_t const&,
                                      void const*        cl_kernel_bin,
                                      size_t             cl_kernel_len,
                                      std::string const& cl_kernel_name,
                                      std::string const& cl_build_options = "")
{
  return gpu_kernel<Args...>(beta::cl,
                             cl_kernel_bin,
                             cl_kernel_len,
                             cl_kernel_name,
                             cl_build_options);
}
#endif

#ifdef MARE_HAVE_GLES
template<typename ...Args>
gpu_kernel<Args...> create_gpu_kernel(beta::gl_t const&,
                                      void const* gl_kernel_bin,
                                      size_t      gl_kernel_len)
{
  MARE_UNIMPLEMENTED("MARE does not support creation of precompiled OpenGL ES kernels.");
  return gpu_kernel<Args...>(beta::gl, gl_kernel_bin, gl_kernel_len);
}
#endif

};

};
