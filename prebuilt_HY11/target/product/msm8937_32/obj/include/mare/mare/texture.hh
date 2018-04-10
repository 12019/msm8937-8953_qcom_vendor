// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef MARE_HAVE_OPENCL

#include <mare/texturetype.hh>

#include <mare/internal/graphics/texture.hh>

namespace mare {
namespace graphics {

template<image_format img_format, int dims>
using texture_ptr = ::mare::internal::mare_shared_ptr<
                              internal::texture_cl<img_format, dims> >;

template<image_format img_format, int dims, typename T>
texture_ptr<img_format, dims>
create_texture(image_size<dims> const& is, T* host_ptr);

#ifdef MARE_HAVE_QTI_HEXAGON

template<image_format img_format, int dims>
texture_ptr<img_format, dims>
create_texture(image_size<dims> const& is, ion_memregion const& ion_mr);
#endif

template<image_format img_format, int dims>
void* map(texture_ptr<img_format, dims>& tp);

template<image_format img_format, int dims>
void unmap(texture_ptr<img_format, dims>& tp);

template<addressing_mode addr_mode, filter_mode fil_mode>
using sampler_ptr = ::mare::internal::mare_shared_ptr<
                              internal::sampler_cl<addr_mode, fil_mode>>;

template<addressing_mode addr_mode, filter_mode fil_mode>
sampler_ptr<addr_mode, fil_mode>
create_sampler(bool normalized_coords);

bool is_supported(image_format img_format);

};
};

#endif
