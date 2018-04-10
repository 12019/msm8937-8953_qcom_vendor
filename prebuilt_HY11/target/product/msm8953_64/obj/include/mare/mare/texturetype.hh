// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef MARE_HAVE_OPENCL

#include <mare/internal/compat/compilercompat.h>

namespace mare {
namespace graphics {

enum class image_format : int {
  first,

  RGBAsnorm_int8 = first,
  RGBAunorm_int8,
  RGBAsigned_int8,
  RGBAunsigned_int8,
  RGBAunorm_int16,
  RGBA_float,
  RGBA_half,

  ARGBsnorm_int8,
  ARGBunorm_int8,
  ARGBsigned_int8,
  ARGBunsigned_int8,

  BGRAsnorm_int8,
  BGRAunorm_int8,
  BGRAsigned_int8,
  BGRAunsigned_int8,

  INTENSITYsnorm_int8,
  INTENSITYsnorm_int16,
  INTENSITYunorm_int8,
  INTENSITYunorm_int16,
  INTENSITY_float,

  LUMINANCEsnorm_int8,
  LUMINANCEsnorm_int16,
  LUMINANCEunorm_int8,
  LUMINANCEunorm_int16,
  LUMINANCE_float,

  last = LUMINANCE_float
};

enum class filter_mode
{
  FILTER_NEAREST,
  FILTER_LINEAR
};

enum class addressing_mode
{
  ADDRESS_NONE,
  ADDRESS_CLAMP_TO_EDGE,
  ADDRESS_CLAMP,
  ADDRESS_REPEAT
};

MARE_GCC_IGNORE_BEGIN("-Weffc++");

template<int dims>
struct image_size
{};

template<>
struct image_size<1>
{
  size_t _width;
};

template<>
struct image_size<2>
{
  size_t _width;
  size_t _height;
};

template<>
struct image_size<3>
{
  size_t _width;
  size_t _height;
  size_t _depth;
};

MARE_GCC_IGNORE_END("-Weffc++");
};
};

#endif
