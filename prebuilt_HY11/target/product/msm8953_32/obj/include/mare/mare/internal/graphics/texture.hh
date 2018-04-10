// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef MARE_HAVE_OPENCL

#include <set>
#include <unordered_set>

#include <mare/memregion.hh>
#include <mare/texturetype.hh>

#include <mare/internal/buffer/memregionaccessor.hh>
#include <mare/internal/device/cldevice.hh>
#include <mare/internal/device/gpuopencl.hh>
#include <mare/internal/graphics/texture_init.hh>

#ifdef MARE_HAVE_QTI_HEXAGON
#include <rpcmem.h>
#endif

namespace mare {
namespace graphics {
namespace internal {

MARE_GCC_IGNORE_BEGIN("-Weffc++");

struct cl_RGBAsnorm_int8 {
  static constexpr image_format mare_image_format = image_format::RGBAsnorm_int8;
  enum { order = CL_RGBA };
  enum { type  = CL_SNORM_INT8 };
};

struct cl_RGBAunorm_int8 {
  static constexpr image_format mare_image_format = image_format::RGBAunorm_int8;
  enum { order = CL_RGBA };
  enum { type  = CL_UNORM_INT8 };
};

struct cl_RGBAsigned_int8 {
  static constexpr image_format mare_image_format = image_format::RGBAsigned_int8;
  enum { order = CL_RGBA };
  enum { type  = CL_SIGNED_INT8 };
};

struct cl_RGBAunsigned_int8 {
  static constexpr image_format mare_image_format = image_format::RGBAunsigned_int8;
  enum { order = CL_RGBA };
  enum { type  = CL_UNSIGNED_INT8 };
};

struct cl_RGBAunorm_int16 {
  static constexpr image_format mare_image_format = image_format::RGBAunorm_int16;
  enum { order = CL_RGBA };
  enum { type  = CL_UNORM_INT16 };
};

struct cl_RGBA_float {
  static constexpr image_format mare_image_format = image_format::RGBA_float;
  enum { order = CL_RGBA };
  enum { type  = CL_FLOAT };
};

struct cl_RGBA_half {
  static constexpr image_format mare_image_format = image_format::RGBA_half;
  enum { order = CL_RGBA };
  enum { type  = CL_HALF_FLOAT };
};

struct cl_ARGBsnorm_int8 {
  static constexpr image_format mare_image_format = image_format::ARGBsnorm_int8;
  enum { order = CL_ARGB };
  enum { type  = CL_SNORM_INT8 };
};

struct cl_ARGBunorm_int8 {
  static constexpr image_format mare_image_format = image_format::ARGBunorm_int8;
  enum { order = CL_ARGB };
  enum { type  = CL_UNORM_INT8 };
};

struct cl_ARGBsigned_int8 {
  static constexpr image_format mare_image_format = image_format::ARGBsigned_int8;
  enum { order = CL_ARGB };
  enum { type  = CL_SIGNED_INT8 };
};

struct cl_ARGBunsigned_int8 {
  static constexpr image_format mare_image_format = image_format::ARGBunsigned_int8;
  enum { order = CL_ARGB };
  enum { type  = CL_UNSIGNED_INT8 };
};

struct cl_BGRAsnorm_int8 {
  static constexpr image_format mare_image_format = image_format::BGRAsnorm_int8;
  enum { order = CL_BGRA };
  enum { type  = CL_SNORM_INT8 };
};

struct cl_BGRAunorm_int8 {
  static constexpr image_format mare_image_format = image_format::BGRAunorm_int8;
  enum { order = CL_BGRA };
  enum { type  = CL_UNORM_INT8 };
};

struct cl_BGRAsigned_int8 {
  static constexpr image_format mare_image_format = image_format::BGRAsigned_int8;
  enum { order = CL_BGRA };
  enum { type  = CL_SIGNED_INT8 };
};

struct cl_BGRAunsigned_int8 {
  static constexpr image_format mare_image_format = image_format::BGRAunsigned_int8;
  enum { order = CL_BGRA };
  enum { type  = CL_UNSIGNED_INT8 };
};

struct cl_INTENSITYsnorm_int8 {
  static constexpr image_format mare_image_format = image_format::INTENSITYsnorm_int8;
  enum { order = CL_INTENSITY };
  enum { type  = CL_SNORM_INT8 };
};

struct cl_INTENSITYunorm_int8 {
  static constexpr image_format mare_image_format = image_format::INTENSITYunorm_int8;
  enum { order = CL_INTENSITY };
  enum { type  = CL_UNORM_INT8 };
};

struct cl_INTENSITYsnorm_int16 {
  static constexpr image_format mare_image_format = image_format::INTENSITYsnorm_int16;
  enum { order = CL_INTENSITY };
  enum { type  = CL_SNORM_INT16 };
};

struct cl_INTENSITYunorm_int16 {
  static constexpr image_format mare_image_format = image_format::INTENSITYunorm_int16;
  enum { order = CL_INTENSITY };
  enum { type  = CL_UNORM_INT16 };
};

struct cl_INTENSITY_float {
  static constexpr image_format mare_image_format = image_format::INTENSITY_float;
  enum { order = CL_INTENSITY };
  enum { type  = CL_FLOAT };
};

struct cl_LUMINANCEsnorm_int8 {
  static constexpr image_format mare_image_format = image_format::LUMINANCEsnorm_int8;
  enum { order = CL_LUMINANCE };
  enum { type  = CL_SNORM_INT8 };
};

struct cl_LUMINANCEunorm_int8 {
  static constexpr image_format mare_image_format = image_format::LUMINANCEunorm_int8;
  enum { order = CL_LUMINANCE };
  enum { type  = CL_UNORM_INT8 };
};

struct cl_LUMINANCEsnorm_int16 {
  static constexpr image_format mare_image_format = image_format::LUMINANCEsnorm_int16;
  enum { order = CL_LUMINANCE };
  enum { type  = CL_SNORM_INT16 };
};

struct cl_LUMINANCEunorm_int16 {
  static constexpr image_format mare_image_format = image_format::LUMINANCEunorm_int16;
  enum { order = CL_LUMINANCE };
  enum { type  = CL_UNORM_INT16 };
};

struct cl_LUMINANCE_float {
  static constexpr image_format mare_image_format = image_format::LUMINANCE_float;
  enum { order = CL_LUMINANCE };
  enum { type  = CL_FLOAT };
};

MARE_GCC_IGNORE_END("-Weffc++");

MARE_GCC_IGNORE_BEGIN("-Weffc++");

template<cl_filter_mode mode>
struct cl_filter_val
{ enum { cl_val = mode }; };

template<filter_mode mode>
struct translate_filter_mode
{};

template<>
struct translate_filter_mode<filter_mode::FILTER_NEAREST> : cl_filter_val<CL_FILTER_NEAREST> {};

template<>
struct translate_filter_mode<filter_mode::FILTER_LINEAR> : cl_filter_val<CL_FILTER_LINEAR> {};

template<cl_addressing_mode mode>
struct cl_addr_val
{ enum { cl_val = mode }; };

template<addressing_mode mode>
struct translate_addressing_mode
{};

template<>
struct translate_addressing_mode<addressing_mode::ADDRESS_NONE> : cl_addr_val<CL_ADDRESS_NONE> {};

template<>
struct translate_addressing_mode<addressing_mode::ADDRESS_CLAMP_TO_EDGE> : cl_addr_val<CL_ADDRESS_CLAMP_TO_EDGE> {};

template<>
struct translate_addressing_mode<addressing_mode::ADDRESS_CLAMP> : cl_addr_val<CL_ADDRESS_CLAMP> {};

template<>
struct translate_addressing_mode<addressing_mode::ADDRESS_REPEAT> : cl_addr_val<CL_ADDRESS_REPEAT> {};

MARE_GCC_IGNORE_END("-Weffc++");

MARE_GCC_IGNORE_BEGIN("-Weffc++");

template<typename T>
struct type_wrapper
{
   typedef T type;
};

template<image_format img_format>
struct translate_image_format_to_struct
{};

template<>
struct translate_image_format_to_struct<image_format::RGBAsnorm_int8> :
          type_wrapper<cl_RGBAsnorm_int8> {};

template<>
struct translate_image_format_to_struct<image_format::RGBAunorm_int8> :
           type_wrapper<cl_RGBAunorm_int8> {};

template<>
struct translate_image_format_to_struct<image_format::RGBAsigned_int8> :
           type_wrapper<cl_RGBAsigned_int8> {};

template<>
struct translate_image_format_to_struct<image_format::RGBAunsigned_int8> :
           type_wrapper<cl_RGBAunsigned_int8> {};

template<>
struct translate_image_format_to_struct<image_format::RGBAunorm_int16> :
           type_wrapper<cl_RGBAunorm_int16> {};

template<>
struct translate_image_format_to_struct<image_format::RGBA_float> :
           type_wrapper<cl_RGBA_float> {};

template<>
struct translate_image_format_to_struct<image_format::RGBA_half> :
           type_wrapper<cl_RGBA_half> {};

template<>
struct translate_image_format_to_struct<image_format::ARGBsnorm_int8> :
           type_wrapper<cl_ARGBsnorm_int8> {};

template<>
struct translate_image_format_to_struct<image_format::ARGBunorm_int8> :
           type_wrapper<cl_ARGBunorm_int8> {};

template<>
struct translate_image_format_to_struct<image_format::ARGBsigned_int8> :
           type_wrapper<cl_ARGBsigned_int8> {};

template<>
struct translate_image_format_to_struct<image_format::ARGBunsigned_int8> :
           type_wrapper<cl_ARGBunsigned_int8> {};

template<>
struct translate_image_format_to_struct<image_format::BGRAsnorm_int8> :
           type_wrapper<cl_BGRAsnorm_int8> {};

template<>
struct translate_image_format_to_struct<image_format::BGRAunorm_int8> :
           type_wrapper<cl_BGRAunorm_int8> {};

template<>
struct translate_image_format_to_struct<image_format::BGRAsigned_int8> :
           type_wrapper<cl_BGRAsigned_int8> {};

template<>
struct translate_image_format_to_struct<image_format::BGRAunsigned_int8> :
           type_wrapper<cl_BGRAunsigned_int8> {};

template<>
struct translate_image_format_to_struct<image_format::INTENSITYsnorm_int8> :
           type_wrapper<cl_INTENSITYsnorm_int8> {};

template<>
struct translate_image_format_to_struct<image_format::INTENSITYunorm_int8> :
           type_wrapper<cl_INTENSITYunorm_int8> {};

template<>
struct translate_image_format_to_struct<image_format::INTENSITYsnorm_int16> :
           type_wrapper<cl_INTENSITYsnorm_int16> {};

template<>
struct translate_image_format_to_struct<image_format::INTENSITYunorm_int16> :
           type_wrapper<cl_INTENSITYunorm_int16> {};

template<>
struct translate_image_format_to_struct<image_format::INTENSITY_float> :
           type_wrapper<cl_INTENSITY_float> {};

template<>
struct translate_image_format_to_struct<image_format::LUMINANCEsnorm_int8> :
           type_wrapper<cl_LUMINANCEsnorm_int8> {};

template<>
struct translate_image_format_to_struct<image_format::LUMINANCEunorm_int8> :
           type_wrapper<cl_LUMINANCEunorm_int8> {};

template<>
struct translate_image_format_to_struct<image_format::LUMINANCEsnorm_int16> :
           type_wrapper<cl_LUMINANCEsnorm_int16> {};

template<>
struct translate_image_format_to_struct<image_format::LUMINANCEunorm_int16> :
           type_wrapper<cl_LUMINANCEunorm_int16> {};

template<>
struct translate_image_format_to_struct<image_format::LUMINANCE_float> :
           type_wrapper<cl_LUMINANCE_float> {};

MARE_GCC_IGNORE_END("-Weffc++");

MARE_GCC_IGNORE_BEGIN("-Weffc++");

#define MARE_USE_OPENCL_C_APIS_FOR_TEXTURE

class base_texture_cl
{
public:
  ::mare::internal::legacy::device_ptr        _device_ptr;
  cl::CommandQueue& _cl_cmd_q;

  void*             _host_ptr;
#ifdef MARE_USE_OPENCL_C_APIS_FOR_TEXTURE
  cl_mem            _img;
#else
  cl::Image         _img;
#endif
  void*             _mapped_ptr;
  image_size<3>     _base_image_size;

  explicit base_texture_cl(void* host_ptr, image_size<3> base_image_size) :
   _device_ptr(::mare::internal::s_dev_ptrs->at(0)),
   _cl_cmd_q(::mare::internal::access_cldevice::get_cl_cmd_queue(
                  _device_ptr)),
   _host_ptr(host_ptr),
   _img(),
   _mapped_ptr(nullptr),
   _base_image_size(base_image_size)
  {}

  ~base_texture_cl()
  {
    MARE_DLOG("DESTRUCTING texture %p", _img);
#ifdef MARE_USE_OPENCL_C_APIS_FOR_TEXTURE
    cl_int status = ::clReleaseMemObject(_img);
    if(status != CL_SUCCESS)
      MARE_FATAL("Failed to release texture %p", _img);
#endif
  }

  void unmap()
  {
#ifdef MARE_USE_OPENCL_C_APIS_FOR_TEXTURE
    cl_int status;
    cl_event evt;
    status = ::clEnqueueUnmapMemObject(_cl_cmd_q(),
                                        _img,
                                        _mapped_ptr,
                                        0,
                                        nullptr,
                                        &evt);
    clWaitForEvents(1, &evt);

    MARE_DLOG("texture_cl(): unmap clerror=%d\n", status);
    cl::detail::errHandler(status, "texture unmap failed");
#endif

    MARE_INTERNAL_ASSERT(_mapped_ptr != nullptr, "map not called before unmap");
  }

  void* map()
  {
    try {
      cl::size_t<3> origin;
      origin[0] = 0;
      origin[1] = 0;
      origin[2] = 0;

      cl::size_t<3> region;
      region[0] = _base_image_size._width;
      region[1] = _base_image_size._height;
      region[2] = _base_image_size._depth;

      size_t row_pitch, slice_pitch;

#ifdef MARE_USE_OPENCL_C_APIS_FOR_TEXTURE
      cl_int error;
      MARE_DLOG("cmd_q=%p", _cl_cmd_q());
      _mapped_ptr = ::clEnqueueMapImage(_cl_cmd_q(),
                                        _img,
                                        CL_TRUE,
                                        CL_MAP_READ | CL_MAP_WRITE,
                                        origin,
                                        region,
                                        &row_pitch,
                                        &slice_pitch,
                                        0,
                                        nullptr,
                                        nullptr,
                                        &error);

      MARE_DLOG("texture_cl(): map clerror=%d\n", error);
      cl::detail::errHandler(error, "texture map failed");
#else
      _mapped_ptr = _cl_cmd_q.enqueueMapImage(_img,
                                              CL_TRUE,

                                              CL_MAP_READ | CL_MAP_WRITE,
                                              origin,
                                              region,
                                              &row_pitch,
                                              &slice_pitch,
                                              nullptr,
                                              nullptr,
                                              nullptr);
#endif
      MARE_INTERNAL_ASSERT(_host_ptr == nullptr || _host_ptr == _mapped_ptr,
                           "_host_ptr and _mapped_ptr mismatch");
    }
    catch(cl::Error& err) {
      MARE_FATAL("cl::CommandQueue::enqueueMapImage()->%s",
                 ::mare::internal::get_cl_error_string(err.err()));
    }
    catch(...) {
      MARE_FATAL("Unknown error in cl::CommandQueue::enqueueMapImage()");
    }

    return _mapped_ptr;
  }
};

template<image_format img_format, int dims>
struct texture_cl;

template<image_format img_format>
struct texture_cl<img_format, 1> :
   public base_texture_cl, ::mare::internal::ref_counted_object<texture_cl<img_format, 1>>
{

  image_size<1>     _image_size;
  using image_cl_desc = typename translate_image_format_to_struct<img_format>::type;

public:
  texture_cl(image_size<1> const& is, bool read_only, void* host_ptr, bool is_ion = false, bool is_cached = false) :
    base_texture_cl(host_ptr, {is._width, 1, 1}),
    _image_size(is)
  {
    MARE_API_ASSERT(host_ptr != nullptr,
      "Currently host_ptr == nullptr not supported. Please pre-allocate.");
    _host_ptr = host_ptr;
    try
    {
#ifdef MARE_USE_OPENCL_C_APIS_FOR_TEXTURE
#ifdef ANDROID
MARE_UNUSED(is_cached);
      if (!is_ion)
      {
        cl_image_format format;
        format.image_channel_order     = image_cl_desc::order;
        format.image_channel_data_type = image_cl_desc::type;

        cl_image_desc image_desc = {CL_MEM_OBJECT_IMAGE1D,
                                    is._width,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    nullptr};

        cl_int error;

        cl::Context& context =
            ::mare::internal::access_cldevice::get_cl_context(
                              _device_ptr);

        MARE_GCC_IGNORE_BEGIN("-Wdeprecated-declarations");
        _img = ::clCreateImage(
                                 context(),
                                 ((_host_ptr == nullptr ? CL_MEM_ALLOC_HOST_PTR :
                                                          CL_MEM_USE_HOST_PTR) |
                                  (read_only ? CL_MEM_READ_ONLY :
                                               CL_MEM_READ_WRITE)),
                                 &format,
                                 &image_desc,
                                 host_ptr,
                                 &error);
        MARE_GCC_IGNORE_END("-Wdeprecated-declarations");

        MARE_DLOG("texture_cl(): clerror=%d\n", error);
        cl::detail::errHandler(error, "texture creation failed");
      }
      else
      {
        MARE_FATAL("mare doesn't support ion 1d texture yet.");
      }
#else
      MARE_UNUSED(is);
      MARE_UNUSED(read_only);
      MARE_UNUSED(is_ion);
      MARE_UNUSED(is_cached);
#endif
#endif
    }
    catch(cl::Error& err) {
      MARE_FATAL("Could not construct cl 1D image in texture_cl()->%s for format: %d",
                 ::mare::internal::get_cl_error_string(err.err()),
                 static_cast<int>(img_format));
    }
  }

  ~texture_cl()
  {
  }
};

template<image_format img_format>
struct texture_cl<img_format, 2> :
   public base_texture_cl, ::mare::internal::ref_counted_object<texture_cl<img_format, 2>>
{

  image_size<2>     _image_size;
  using image_cl_desc =
          typename translate_image_format_to_struct<img_format>::type;

public:
  texture_cl(image_size<2> const& is, bool read_only, void* host_ptr, bool is_ion, bool is_cached) :
    base_texture_cl(host_ptr, {is._width, is._height, 1}),
    _image_size(is)
  {
    MARE_API_ASSERT(host_ptr != nullptr,
      "Currently host_ptr == nullptr not supported. Please pre-allocate.");
    _host_ptr = host_ptr;
    try
    {
#ifdef MARE_USE_OPENCL_C_APIS_FOR_TEXTURE
      cl_image_format format;
      format.image_channel_order     = image_cl_desc::order;
      format.image_channel_data_type = image_cl_desc::type;

      cl_int error;

      cl::Context& context =
          ::mare::internal::access_cldevice::get_cl_context(
                            _device_ptr);

      MARE_GCC_IGNORE_BEGIN("-Wdeprecated-declarations");
      if (!is_ion)
      {
        _img = ::clCreateImage2D(context(),
                                 ((_host_ptr == nullptr ? CL_MEM_ALLOC_HOST_PTR :
                                                          CL_MEM_USE_HOST_PTR) |
                                  (read_only ? CL_MEM_READ_ONLY :
                                               CL_MEM_READ_WRITE)),
                                 &format,
                                 is._width,
                                 is._height,
                                 0,
                                 host_ptr,
                                 &error);
      }
      else
      {
#ifdef MARE_HAVE_QTI_HEXAGON

        cl_mem_ion_host_ptr ionmem_host_ptr;
        ionmem_host_ptr.ext_host_ptr.allocation_type = CL_MEM_ION_HOST_PTR_QCOM;

        ionmem_host_ptr.ext_host_ptr.host_cache_policy = is_cached ? CL_MEM_HOST_WRITEBACK_QCOM : CL_MEM_HOST_UNCACHED_QCOM;
        ionmem_host_ptr.ion_hostptr = host_ptr;
        ionmem_host_ptr.ion_filedesc = rpcmem_to_fd(ionmem_host_ptr.ion_hostptr);

        size_t image_row_pitch;
        cl::Device& device = ::mare::internal::access_cldevice::get_cl_device(_device_ptr);
        clGetDeviceImageInfoQCOM(device(),
                                 is._width,
                                 is._height,
                                 &format,
                                 CL_IMAGE_ROW_PITCH,
                                 sizeof(image_row_pitch),
                                 &image_row_pitch,
                                 nullptr);
        MARE_DLOG("getdeviceinfo_qcom done. image row pitch = %zu", image_row_pitch);

        _img =  ::clCreateImage2D(context(),
                                  CL_MEM_USE_HOST_PTR | CL_MEM_EXT_HOST_PTR_QCOM | CL_MEM_READ_WRITE,
                                  &format,
                                  is._width,
                                  is._height,

                                  image_row_pitch,
                                  &ionmem_host_ptr,
                                  &error);
#else
        MARE_UNUSED(is_cached);
        MARE_FATAL("ion texture is not supported in non-hexagon platform!");
#endif
      }
      MARE_GCC_IGNORE_END("-Wdeprecated-declarations");

      MARE_DLOG("texture_cl(): clerror=%d\n", error);
      cl::detail::errHandler(error, "texture creation failed");
#else
      _img = cl::Image2D(::mare::internal::access_cldevice::get_cl_context(
                            _device_ptr),
                         ((_host_ptr == nullptr ? CL_MEM_ALLOC_HOST_PTR :
                                                  CL_MEM_USE_HOST_PTR) |
                          (read_only ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE)),
                         cl::img_format(image_cl_desc::order,
                                         image_cl_desc::type),
                         is._width,
                         is._height,
                         size_t(0),
                         _host_ptr,
                         nullptr);
#endif
    }
    catch(cl::Error& err) {
      MARE_FATAL("Could not construct cl::Image2D in texture_cl()->%s for format: %d",
                 ::mare::internal::get_cl_error_string(err.err()),
                 static_cast<int>(img_format));
    }
  }

  ~texture_cl()
  {
  }
};

template<image_format img_format>
struct texture_cl<img_format, 3> :
   public base_texture_cl, ::mare::internal::ref_counted_object<texture_cl<img_format, 3>>
{

  image_size<3>     _image_size;
  using image_cl_desc =
          typename translate_image_format_to_struct<img_format>::type;

public:
  texture_cl(image_size<3> const& is, bool read_only, void* host_ptr, bool is_ion = false, bool is_cached = false) :
    base_texture_cl(host_ptr, is),
    _image_size(is)
  {
    MARE_API_ASSERT(host_ptr != nullptr,
                    "Currently host_ptr == nullptr not supported. Please pre-allocate.");
    _host_ptr = host_ptr;
    try
    {
#ifdef MARE_USE_OPENCL_C_APIS_FOR_TEXTURE
      cl_image_format format;
      format.image_channel_order     = image_cl_desc::order;
      format.image_channel_data_type = image_cl_desc::type;

      cl_int error;

      cl::Context& context =
          ::mare::internal::access_cldevice::get_cl_context(
                            _device_ptr);

      MARE_GCC_IGNORE_BEGIN("-Wdeprecated-declarations");
      MARE_UNUSED(is_cached);
      if (!is_ion)
      {
        _img = ::clCreateImage3D(
                                 context(),
                                 ((_host_ptr == nullptr ? CL_MEM_ALLOC_HOST_PTR :
                                                          CL_MEM_USE_HOST_PTR) |
                                  (read_only ? CL_MEM_READ_ONLY :
                                               CL_MEM_READ_WRITE)),
                                 &format,
                                 is._width,
                                 is._height,
                                 is._depth,
                                 0,
                                 0,
                                 host_ptr,
                                 &error);
      }
      else
      {
        MARE_FATAL("mare doesn't support ion 3d texture yet.");
      }
      MARE_GCC_IGNORE_END("-Wdeprecated-declarations");

      MARE_DLOG("texture_cl(): clerror=%d\n", error);
      cl::detail::errHandler(error, "texture creation failed");
#else
      _img = cl::Image3D(::mare::internal::access_cldevice::get_cl_context(
                            _device_ptr),
                         ((_host_ptr == nullptr ? CL_MEM_ALLOC_HOST_PTR :
                                                  CL_MEM_USE_HOST_PTR) |
                          (read_only ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE)),
                         cl::img_format(image_cl_desc::order,
                                         image_cl_desc::type),
                         is._width,
                         is._height,
                         is._depth,
                         0,
                         0,
                         _host_ptr,
                         nullptr);
#endif
    }
    catch(cl::Error& err) {
      MARE_FATAL("Could not construct cl::Image3D in texture_cl()->%s for format: %d",
                 ::mare::internal::get_cl_error_string(err.err()),
                 static_cast<int>(img_format));
    }
  }

  ~texture_cl()
  {
  }
};

template<addressing_mode addr_mode, filter_mode fil_mode>
struct sampler_cl:
   public ::mare::internal::ref_counted_object<sampler_cl<addr_mode, fil_mode>>
{
  ::mare::internal::legacy::device_ptr        _device_ptr;

#ifdef MARE_USE_OPENCL_C_APIS_FOR_TEXTURE
  cl_sampler            _sampler;
#endif

public:
  explicit sampler_cl(bool normalized_coords) :
    _device_ptr(::mare::internal::s_dev_ptrs->at(0)),
    _sampler()
  {
    try
    {
#ifdef MARE_USE_OPENCL_C_APIS_FOR_TEXTURE

      cl_int error;

      cl::Context& context =
          ::mare::internal::access_cldevice::get_cl_context(
                            _device_ptr);

      MARE_GCC_IGNORE_BEGIN("-Wdeprecated-declarations");
      _sampler = ::clCreateSampler(context(),
                                   normalized_coords,
                                   translate_addressing_mode<addr_mode>::cl_val,
                                   translate_filter_mode<fil_mode>::cl_val,
                                   &error);
      MARE_GCC_IGNORE_END("-Wdeprecated-declarations");

      MARE_DLOG("sampler_cl(): clerror=%d\n", error);

      cl::detail::errHandler(error, "sampler creation failed");

#endif
    }
    catch(cl::Error& err) {
      MARE_FATAL("Could not construct cl::sampler in sampler_cl()->%s",
                 ::mare::internal::get_cl_error_string(err.err()));
    }
  }

   ~sampler_cl()
  {
    MARE_DLOG("DESTRUCTING sampler %p", _sampler);
#ifdef MARE_USE_OPENCL_C_APIS_FOR_TEXTURE
    cl_int status = ::clReleaseSampler(_sampler);
    if(status != CL_SUCCESS)
      MARE_FATAL("Failed to release sampler %p", _sampler);
#endif
  }
};

MARE_GCC_IGNORE_END("-Weffc++");

};
};
};

namespace mare {
namespace graphics {
namespace internal {

extern std::set<cl::ImageFormat, image_format_less> *p_supported_image_format_set;
};
};
};

namespace mare {
namespace graphics {
namespace internal {

cl::ImageFormat get_cl_image_format(image_format mare_image_format);

image_format    get_mare_image_format(cl::ImageFormat cl_img_fmt);

};
};
};

namespace mare {
namespace graphics {

template<image_format img_format, int dims>
using texture_ptr = ::mare::internal::mare_shared_ptr<internal::texture_cl<img_format, dims>>;

template<image_format img_format, int dims, typename T>
texture_ptr<img_format, dims>
create_texture(image_size<dims> const& is, T * host_ptr)
{
  static_assert(dims == 1 || dims == 2 || dims == 3,
                "texture_ptr(): currently only dims = 1, 2 or 3 is supported");
  using non_const_T = typename std::remove_const<T>::type;

  return texture_ptr<img_format, dims>(new internal::texture_cl<img_format,
                                                                 dims>
                                                                (is,
                                                                 std::is_const<T>::value,
                                                                 reinterpret_cast<void *>( const_cast<non_const_T*>(host_ptr) ),
                                                                 false,
                                                                 false));
}

#ifdef MARE_HAVE_QTI_HEXAGON

template<image_format img_format, int dims>
texture_ptr<img_format, dims>
create_texture(image_size<dims> const& is, ion_memregion& ion_mr)
{
  static_assert(dims == 2 || dims == 3,
                "texture_ptr(): currently only dims = 2 or 3 is supported");

  MARE_API_ASSERT(ion_mr.get_num_bytes() > 0, "Memregion has zero bytes.");
  auto int_ion_mr = ::mare::internal::memregion_base_accessor::get_internal_mr(ion_mr);
  MARE_INTERNAL_ASSERT(int_ion_mr != nullptr, "Error. Internal memregion member is null");
  MARE_INTERNAL_ASSERT(int_ion_mr->get_type() == ::mare::internal::memregion_t::ion,
                       "Error. Incorrect type of memregion");
  return texture_ptr<img_format, dims>(new internal::texture_cl<img_format,
                                                                 dims>
                                                                (is,
                                                                 false,
                                                                 ion_mr.get_ptr(),
                                                                 true,
                                                                 int_ion_mr->is_cacheable()));
}
#endif

template<image_format img_format, int dims>
void* map(texture_ptr<img_format, dims>& tp)
{
  auto internal_tp = c_ptr(tp);
  MARE_INTERNAL_ASSERT(internal_tp != nullptr,
                       "mare_shared_ptr of texture_cl is nullptr");
  return internal_tp->map();
}

template<image_format img_format, int dims>
void unmap(texture_ptr<img_format, dims>& tp)
{
  auto internal_tp = c_ptr(tp);
  MARE_INTERNAL_ASSERT(internal_tp != nullptr,
                       "mare_shared_ptr of texture_cl is nullptr");
  internal_tp->unmap();
}

template<addressing_mode addr_mode, filter_mode fil_mode>
using sampler_ptr = ::mare::internal::mare_shared_ptr<internal::sampler_cl<addr_mode, fil_mode>>;

template<addressing_mode addr_mode, filter_mode fil_mode>
sampler_ptr<addr_mode, fil_mode>
create_sampler(bool normalized_coords)
{
  return sampler_ptr<addr_mode, fil_mode>(new internal::sampler_cl<addr_mode, fil_mode>(normalized_coords));
}

template<image_format img_format>
bool is_supported()
{
  using image_cl_desc = typename mare::graphics::internal::translate_image_format_to_struct<img_format>::type;
  cl::ImageFormat format(image_cl_desc::order, image_cl_desc::type);
  return internal::p_supported_image_format_set->count(format) > 0;
}

bool is_supported(image_format img_format);

};
};

namespace mare {
namespace graphics {
namespace internal {

template<typename TexturePtr>
struct texture_ptr_info;

template<image_format ImgFormat, int Dims>
struct texture_ptr_info<texture_ptr<ImgFormat, Dims>> {
  static constexpr image_format img_format = ImgFormat;
  static constexpr int dims = Dims;
};

template<int dims_to, int dims_from>
struct image_size_converter;

template<>
struct image_size_converter<1,1> {
  static
  image_size<1> convert(image_size<1> const& is)
  {
    return is;
  }
};

template<>
struct image_size_converter<1,2> {
  static
  image_size<1> convert(image_size<2> const& is)
  {
    return image_size<1>{is._width};
  }
};

template<>
struct image_size_converter<1,3> {
  static
  image_size<1> convert(image_size<3> const& is)
  {
    return image_size<1>{is._width};
  }
};

template<>
struct image_size_converter<2,1> {
  static
  image_size<2> convert(image_size<1> const& is)
  {
    return image_size<2>{is._width, 1};
  }
};

template<>
struct image_size_converter<2,2> {
  static
  image_size<2> convert(image_size<2> const& is)
  {
    return is;
  }
};

template<>
struct image_size_converter<2,3> {
  static
  image_size<2> convert(image_size<3> const& is)
  {
    return image_size<2>{is._width, is._height};
  }
};

template<>
struct image_size_converter<3,1> {
  static
  image_size<3> convert(image_size<1> const& is)
  {
    return image_size<3>{is._width, 1, 1};
  }
};

template<>
struct image_size_converter<3,2> {
  static
  image_size<3> convert(image_size<2> const& is)
  {
    return image_size<3>{is._width, is._height, 1};
  }
};

template<>
struct image_size_converter<3,3> {
  static
  image_size<3> convert(image_size<3> const& is)
  {
    return is;
  }
};

};
};
};
#endif
