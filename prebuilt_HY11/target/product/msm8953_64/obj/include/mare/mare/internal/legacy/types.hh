// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/util/mareptrs.hh>

namespace mare {
namespace internal {

class group;
class hexagonbuffer;

#ifdef MARE_HAVE_OPENCL
class cldevice;
#endif

namespace legacy {

#ifdef MARE_HAVE_GPU

template<typename T>
class buffer;

template<typename...Kargs>
class gpukernel;

#ifdef MARE_HAVE_OPENCL

typedef internal::mare_shared_ptr<internal::cldevice> device_ptr;
#else

typedef struct dummy_device_ptr {} device_ptr;
#endif

template<typename... Kargs>
using kernel_ptr = internal::mare_shared_ptr< ::mare::internal::legacy::gpukernel<Kargs...> >;

#endif

#ifdef MARE_HAVE_QTI_HEXAGON
typedef internal::mare_shared_ptr<internal::hexagonbuffer> hexagonbuffer_ptr;
#endif

};
};
};
