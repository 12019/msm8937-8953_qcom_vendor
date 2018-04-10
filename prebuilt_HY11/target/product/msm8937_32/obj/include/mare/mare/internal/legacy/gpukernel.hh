// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef MARE_HAVE_GPU

#include <mare/internal/legacy/attrobjs.hh>
#include <mare/internal/legacy/types.hh>

#include <mare/internal/device/clkernel.hh>
#include <mare/internal/device/glkernel.hh>

namespace mare {
namespace internal {

template<typename GPUKernelType>
struct gpukernel_deleter {
  static void release(GPUKernelType* k) {
#ifdef MARE_HAVE_OPENCL
    delete k->get_cl_kernel();
#endif
#ifdef MARE_HAVE_GLES
    delete k->get_gl_kernel();
#endif
    delete k;
  }
};

namespace legacy {

MARE_GCC_IGNORE_BEGIN("-Weffc++");

template<typename...Params>
class gpukernel : public mare::internal::ref_counted_object<gpukernel<Params...>,
                                                            mare::internal::mareptrs::default_logger,
                                                            mare::internal::gpukernel_deleter<gpukernel<Params...>>>,
                  public internal::legacy::gpu_kernel_base
{

MARE_GCC_IGNORE_END("-Weffc++");

private:
#ifdef MARE_HAVE_OPENCL
  mare::internal::clkernel* _cl_kernel;
#endif

#ifdef MARE_HAVE_GLES
  mare::internal::glkernel* _gl_kernel;
#endif

  MARE_DELETE_METHOD(gpukernel(gpukernel const&));
  MARE_DELETE_METHOD(gpukernel(gpukernel&&));
  MARE_DELETE_METHOD(gpukernel& operator=(gpukernel const&));
  MARE_DELETE_METHOD(gpukernel& operator=(gpukernel&&));

public:
  typedef std::tuple<Params...> parameters;

#ifdef MARE_HAVE_OPENCL

  gpukernel(const std::string& kernel_str,
            const std::string& kernel_name,
            const std::string& build_options) :
   _cl_kernel(nullptr)
#ifdef MARE_HAVE_GLES
   , _gl_kernel(nullptr)
#endif
  {

    MARE_INTERNAL_ASSERT(!mare::internal::s_dev_ptrs->empty(),
                         "No GPU devices found on the platform");
    _cl_kernel = new mare::internal::clkernel((*mare::internal::s_dev_ptrs)[0],
                                               kernel_str,
                                               kernel_name,
                                               build_options);
    MARE_DLOG("kernel_name: %s, %p, %s", kernel_name.c_str(), _cl_kernel,
               build_options.c_str());
  }

  gpukernel(void const* kernel_bin,
            size_t kernel_len,
            const std::string& kernel_name,
            const std::string& build_options) :
    _cl_kernel(nullptr)
#ifdef MARE_HAVE_GLES
    , _gl_kernel(nullptr)
#endif
  {

    MARE_INTERNAL_ASSERT(!mare::internal::s_dev_ptrs->empty(),
                         "No GPU devices found on the platform");
    _cl_kernel = new mare::internal::clkernel((*mare::internal::s_dev_ptrs)[0],
                                              kernel_bin,
                                              kernel_len,
                                              kernel_name,
                                              build_options);
    MARE_DLOG("kernel_name: %s, %p, %s", kernel_name.c_str(), _cl_kernel, build_options.c_str());
  }

#ifdef MARE_HAVE_GLES
  bool is_cl() const
  {
    MARE_INTERNAL_ASSERT(_cl_kernel == nullptr || _gl_kernel == nullptr, "Kernel must only one of CL or GLES");
    return _cl_kernel != nullptr;
  }
#else
  bool is_cl() const { return true; }
#endif

  mare::internal::clkernel* get_cl_kernel() { return _cl_kernel; }
#endif

#ifdef MARE_HAVE_GLES
  mare::internal::glkernel* get_gl_kernel() { return _gl_kernel; }

#ifdef MARE_HAVE_OPENCL
  bool is_gl() const
  {
    MARE_INTERNAL_ASSERT(_cl_kernel == nullptr || _gl_kernel == nullptr, "Kernel must only one of CL or GLES");
    return _gl_kernel != nullptr;
  }
#else
  bool is_gl() const { return true; }
#endif

  explicit gpukernel(const std::string& kernel_str) :
#ifdef MARE_HAVE_OPENCL
    _cl_kernel(nullptr),
#endif
    _gl_kernel(nullptr)
  {
    _gl_kernel = new mare::internal::glkernel(kernel_str);
  }

#endif

  inline std::mutex& access_dispatch_mutex() {
    MARE_INTERNAL_ASSERT(false
#ifdef MARE_HAVE_OPENCL
                         || is_cl()
#endif
#ifdef MARE_HAVE_GLES
                         || is_gl()
#endif
                         , "Invalid GPU kernel type: must be cl or gl");

#ifdef MARE_HAVE_OPENCL
#ifdef MARE_HAVE_GLES
    if(is_cl())
#endif
      return get_cl_kernel()->access_dispatch_mutex();
#endif

#ifdef MARE_HAVE_GLES
    return get_gl_kernel()->access_dispatch_mutex();
#endif
  }
};

#ifdef MARE_HAVE_OPENCL
template<typename...Params>
kernel_ptr<Params...>
create_kernel(const std::string& kernel_str, const std::string& kernel_name,
              const std::string build_options=std::string(""))
{
  auto k_ptr = new gpukernel<Params...>(kernel_str, kernel_name, build_options);
  MARE_DLOG("Creating mare::gpukernel: %s", kernel_name.c_str());
  return kernel_ptr<Params...>(k_ptr, kernel_ptr<Params...>::ref_policy::do_initial_ref);
}

template<typename...Params>
kernel_ptr<Params...>
create_kernel(const std::string& kernel_str, const std::string& kernel_name,
              size_t& opt_local_size,
              const std::string build_options=std::string(""))
{
  auto k_ptr = new gpukernel<Params...>(kernel_str, kernel_name, build_options);
  opt_local_size = k_ptr->get_cl_kernel()->get_optimal_local_size();
  MARE_DLOG("Creating mare::gpukernel: %s", kernel_name.c_str());
  return kernel_ptr<Params...>(k_ptr, kernel_ptr<Params...>::ref_policy::do_initial_ref);
}
#endif

#ifdef MARE_HAVE_GLES

template<typename...Params>
kernel_ptr<Params...>
create_kernel(const std::string& gl_kernel_str)
{
  auto k_ptr = new gpukernel<Params...>(gl_kernel_str);
  MARE_DLOG("Creating mare::glkernel");
  return kernel_ptr<Params...>(k_ptr, kernel_ptr<Params...>::ref_policy::do_initial_ref);
}
#endif

#ifdef MARE_HAVE_OPENCL

template<typename...Params>
kernel_ptr<Params...>
create_kernel(void const* kernel_bin,
              size_t kernel_len,
              const std::string& kernel_name,
              const std::string build_options = std::string(""))
{
  auto k_ptr = new gpukernel<Params...>(kernel_bin, kernel_len, kernel_name, build_options);
  MARE_DLOG("Creating mare::gpukernel: %s", kernel_name.c_str());
  return kernel_ptr<Params...>(k_ptr, kernel_ptr<Params...>::ref_policy::do_initial_ref);
}

template<typename...Params>
kernel_ptr<Params...>
create_kernel(void const* kernel_bin,
              size_t kernel_len,
              size_t& opt_local_size,
              const std::string& kernel_name,
              const std::string build_options = std::string(""))
{
  auto k_ptr = new gpukernel<Params...>(kernel_bin, kernel_len, kernel_name, build_options);
  opt_local_size = k_ptr->get_cl_kernel()->get_optimal_local_size();
  MARE_DLOG("Creating mare::gpukernel: %s", kernel_name.c_str());
  return kernel_ptr<Params...>(k_ptr, kernel_ptr<Params...>::ref_policy::do_initial_ref);
}
#endif
};
};
};

#endif
