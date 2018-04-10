// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef MARE_HAVE_OPENCL

#include <mare/internal/util/debug.hh>

#define __CL_ENABLE_EXCEPTIONS

MARE_GCC_IGNORE_BEGIN("-Wshadow");
MARE_GCC_IGNORE_BEGIN("-Wdeprecated-declarations");
MARE_GCC_IGNORE_BEGIN("-Weffc++");
MARE_GCC_IGNORE_BEGIN("-Wold-style-cast");
MARE_GCC_IGNORE_BEGIN("-Wunused-parameter");

#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.hpp>

#elif defined(__ANDROID__)
#include <CL/cl.hpp>

#else
#include <CL/cl.hpp>
#endif

MARE_GCC_IGNORE_END("-Wunused-parameter");
MARE_GCC_IGNORE_END("-Wold-style-cast");
MARE_GCC_IGNORE_END("-Weffc++");
MARE_GCC_IGNORE_END("-Wdeprecated-declarations");
MARE_GCC_IGNORE_END("-Wshadow");
namespace mare {

namespace internal {

const char *get_cl_error_string(cl_int error);

};
};

#endif

