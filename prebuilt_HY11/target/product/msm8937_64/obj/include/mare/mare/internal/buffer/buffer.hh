// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <string>

#include <mare/devicetypes.hh>
#include <mare/memregion.hh>

#include <mare/internal/buffer/bufferstate.hh>
#include <mare/internal/buffer/memregionaccessor.hh>
#include <mare/internal/compat/compilercompat.h>
#include <mare/internal/graphics/texture.hh>
#include <mare/internal/util/macros.hh>
#include <mare/internal/util/mareptrs.hh>

namespace mare {

template<typename T> class buffer_ptr;

};

namespace mare {
namespace internal {

class buffer_accessor;

void* get_or_create_host_accessible_data_ptr(bufferstate* p_bufstate);

void invalidate_non_host_arenas(bufferstate* p_bufstate);

void setup_preallocated_buffer_data(bufferstate* p_bufstate,
                                    void* preallocated_ptr,
                                    size_t size_in_bytes);

void setup_memregion_buffer_data(bufferstate* p_bufstate,
                                 internal_memregion* int_mr,
                                 size_t size_in_bytes);

using bufferstate_ptr = mare::internal::mare_shared_ptr<bufferstate>;

struct buffer_as_texture_info {
#ifdef MARE_HAVE_OPENCL
private:
  bool                          _used_as_texture;

  int                           _dims;
  mare::graphics::image_format  _img_format;
  mare::graphics::image_size<3> _img_size;

  mare::graphics::internal::base_texture_cl* _tex_cl;

public:

  buffer_as_texture_info(bool used_as_texture                     = false,
                         int  dims                                = 0,
                         mare::graphics::image_format  img_format = mare::graphics::image_format::first,
                         mare::graphics::image_size<3> img_size   = {0,0,0},
                         mare::graphics::internal::base_texture_cl* tex_cl = nullptr) :
    _used_as_texture(used_as_texture),
    _dims(dims),
    _img_format(img_format),
    _img_size(img_size),
    _tex_cl(tex_cl)
  {}

  bool get_used_as_texture() const { return _used_as_texture; }
  int get_dims() const { return _dims; }
  mare::graphics::image_format get_img_format() const { return _img_format; }
  mare::graphics::image_size<3> get_img_size() const { return _img_size; }

  mare::graphics::internal::base_texture_cl*& access_tex_cl() { return _tex_cl; }

#else
  buffer_as_texture_info() {}

  bool get_used_as_texture() const { return false; }
#endif
};

class buffer_ptr_base {
  bool _exclusive;
  bool _do_sync;

  bufferstate_ptr _bufstate;

  void* _host_accessible_data;

  buffer_as_texture_info _tex_info;

  friend class buffer_accessor;

protected:

  buffer_ptr_base() :
    _exclusive(true),
    _do_sync(true),
    _bufstate(nullptr),
    _host_accessible_data(nullptr),
    _tex_info()
  {}

  buffer_ptr_base(size_t size_in_bytes,
                  bool conservative_host_sync,
                  device_set const& device_hints) :
    _exclusive(true),
    _do_sync(true),
    _bufstate(new bufferstate(size_in_bytes, conservative_host_sync, device_hints)),
    _host_accessible_data(nullptr),
    _tex_info()
  {
    if(conservative_host_sync) {
      allocate_host_accessible_data();
      invalidate_non_host_arenas();
    }
  }

  buffer_ptr_base(void* preallocated_ptr,
                  size_t size_in_bytes,
                  bool conservative_host_sync,
                  device_set const& device_hints) :
    _exclusive(true),
    _do_sync(true),
    _bufstate(new bufferstate(size_in_bytes, conservative_host_sync, device_hints)),
    _host_accessible_data(nullptr),
    _tex_info()
  {
    MARE_API_ASSERT(preallocated_ptr != nullptr, "null preallocated region");
    setup_preallocated_buffer_data(::mare::internal::c_ptr(_bufstate),
                                   preallocated_ptr,
                                   size_in_bytes);
    allocate_host_accessible_data();
    if(conservative_host_sync)
      invalidate_non_host_arenas();
    MARE_INTERNAL_ASSERT(_host_accessible_data == preallocated_ptr,
                         "allocate_host_accessible_data() was expected to get back preallocated_ptr");
  }

  buffer_ptr_base(memregion const& mr,
                  size_t size_in_bytes,
                  bool conservative_host_sync,
                  device_set const& device_hints) :
    _exclusive(true),
    _do_sync(true),
    _bufstate(new bufferstate(size_in_bytes, conservative_host_sync, device_hints)),
    _host_accessible_data(nullptr),
    _tex_info()
  {
    auto int_mr = ::mare::internal::memregion_base_accessor::get_internal_mr(mr);
    MARE_INTERNAL_ASSERT(int_mr != nullptr, "Error.Internal memregion object is null");
    MARE_INTERNAL_ASSERT(int_mr->get_type() != ::mare::internal::memregion_t::none,
                         "Error. Invalid type of memregion");
    setup_memregion_buffer_data(::mare::internal::c_ptr(_bufstate),
                                int_mr,
                                size_in_bytes);
    if(conservative_host_sync) {
      allocate_host_accessible_data();
      invalidate_non_host_arenas();
    }
  }

  MARE_DEFAULT_METHOD(buffer_ptr_base(buffer_ptr_base const&));
  MARE_DEFAULT_METHOD(buffer_ptr_base& operator=(buffer_ptr_base const&));
  MARE_DEFAULT_METHOD(buffer_ptr_base(buffer_ptr_base&&));
  MARE_DEFAULT_METHOD(buffer_ptr_base& operator=(buffer_ptr_base&&));

public:
  inline
  void* get_host_accessible_data() const { return _host_accessible_data; }

  inline
  bool syncs_on_task_finish() const { return _do_sync; }

  inline
  bool is_exclusive_to_task() const { return _exclusive; }

  void allocate_host_accessible_data() {
    if(_host_accessible_data != nullptr)
      return;

    auto p_bufstate = ::mare::internal::c_ptr(_bufstate);
    if(p_bufstate == nullptr)
      return;

    _host_accessible_data = get_or_create_host_accessible_data_ptr( p_bufstate );
    MARE_INTERNAL_ASSERT(_host_accessible_data != nullptr, "allocate_host_accessible_data failed");
  }

  void invalidate_non_host_arenas() {
    auto p_bufstate = ::mare::internal::c_ptr(_bufstate);
    if(p_bufstate == nullptr)
      return;

    ::mare::internal::invalidate_non_host_arenas( p_bufstate );
  }

  inline
  void sync() {
    auto p_bufstate = ::mare::internal::c_ptr(_bufstate);
    if(p_bufstate == nullptr)
      return;

    p_bufstate->lock();
    if(get_host_accessible_data() == nullptr)
      allocate_host_accessible_data();
    p_bufstate->host_code_sync();
    p_bufstate->unlock();
  }

  std::string to_string() const {
    std::string s = ::mare::internal::strprintf("host_ptr=%p syncs=%s exclusive=%s bufstate=%p",
                                                _host_accessible_data,
                                                _do_sync ? "Y" : "N",
                                                _exclusive ? "Y" : "N",
                                                ::mare::internal::c_ptr(_bufstate));
    return s;
  }

#ifdef MARE_HAVE_OPENCL
  template<mare::graphics::image_format img_format, int dims>
  void treat_as_texture(mare::graphics::image_size<dims> const& is) {
    _tex_info = buffer_as_texture_info(true,
                                       dims,
                                       img_format,
                                       mare::graphics::internal::image_size_converter<3, dims>::convert(is));
  }
#endif

  inline
  bool is_null() const { return ::mare::internal::c_ptr(_bufstate) == nullptr; }

  inline
  void const* get_buffer() const { return ::mare::internal::c_ptr(_bufstate); }

};

class buffer_accessor {
public:
  static void override_access_permissions(buffer_ptr_base& bb, bool exclusive, bool do_sync) {
    bb._exclusive = exclusive;
    bb._do_sync   = do_sync;
  }

  static bufferstate_ptr get_bufstate(buffer_ptr_base& bb) {
    return bb._bufstate;
  }

  static bufferstate_ptr const get_bufstate(buffer_ptr_base const& bb) {
    return bb._bufstate;
  }

  static buffer_as_texture_info get_buffer_as_texture_info(buffer_ptr_base const& bb) {
    return bb._tex_info;
  }

  MARE_DELETE_METHOD(buffer_accessor());
  MARE_DELETE_METHOD(buffer_accessor(buffer_accessor const&));
  MARE_DELETE_METHOD(buffer_accessor& operator=(buffer_accessor const&));
  MARE_DELETE_METHOD(buffer_accessor(buffer_accessor&&));
  MARE_DELETE_METHOD(buffer_accessor& operator=(buffer_accessor&&));
};

template<typename T>
void override_access_permissions(buffer_ptr<T>& b, bool exclusive, bool do_sync) {
  buffer_ptr_base& bb = reinterpret_cast<buffer_ptr_base&>(b);
  buffer_accessor::override_access_permissions(bb, exclusive, do_sync);
}

};
};

namespace mare {

template<typename T>
buffer_ptr<T> create_buffer(size_t num_elems,
                            bool conservative_host_sync,
                            device_set const& likely_devices)
{
  MARE_API_ASSERT(num_elems > 0, "create_buffer(): cannot create empty buffer.");
  buffer_ptr<T> b(num_elems,
                  conservative_host_sync,
                  likely_devices);
  return b;
}

template<typename T>
buffer_ptr<T> create_buffer(T* preallocated_ptr,
                            size_t num_elems,
                            bool conservative_host_sync,
                            device_set const& likely_devices)
{
  MARE_API_ASSERT(num_elems > 0, "create_buffer(): cannot create empty buffer.");
  buffer_ptr<T> b(preallocated_ptr,
                  num_elems,
                  conservative_host_sync,
                  likely_devices);
  return b;
}

template<typename T>
buffer_ptr<T> create_buffer(memregion const& mr,
                            size_t num_elems,
                            bool conservative_host_sync,
                            device_set const& likely_devices)
{
  MARE_API_ASSERT(mr.get_num_bytes() > 0 || num_elems > 0, "create_buffer(): cannot create empty buffer.");
  buffer_ptr<T> b(mr,
                  num_elems,
                  conservative_host_sync,
                  likely_devices);
  return b;
}

};

namespace mare {
namespace mem_access {

template<typename T>
buffer_ptr<T> exclusive_nosync(buffer_ptr<T> const& b)
{
  buffer_ptr<T> new_b(b);
  ::mare::internal::override_access_permissions(new_b, true, false);
  return new_b;
}

template<typename T>
buffer_ptr<T> exclusive_sync(buffer_ptr<T> const& b)
{
  buffer_ptr<T> new_b(b);
  ::mare::internal::override_access_permissions(new_b, true, true);
  return new_b;
}

template<typename T>
buffer_ptr<T> shared_nosync(buffer_ptr<T> const& b)
{
  buffer_ptr<T> new_b(b);
  ::mare::internal::override_access_permissions(new_b, false, false);
  return new_b;
}

template<typename T>
buffer_ptr<T> shared_sync(buffer_ptr<T> const& b)
{
  buffer_ptr<T> new_b(b);
  ::mare::internal::override_access_permissions(new_b, false, false);
  return new_b;
}

};
};

namespace mare {

template<typename BufferPtr>
struct in {
  using buffer_type = BufferPtr;
};

template<typename BufferPtr>
struct out {
  using buffer_type = BufferPtr;
};

template<typename BufferPtr>
struct inout {
  using buffer_type = BufferPtr;
};

};
