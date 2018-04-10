// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/buffer/memregion.hh>
#include <mare/internal/util/debug.hh>
#include <mare/internal/util/macros.hh>

namespace mare {
namespace internal {

class memregion_base_accessor;
};

class memregion {

  friend
  class ::mare::internal::memregion_base_accessor;

protected:
  explicit memregion(internal::internal_memregion* int_mr) : _int_mr(int_mr){}

  internal::internal_memregion* _int_mr;

public:

  ~memregion(){
    MARE_INTERNAL_ASSERT(_int_mr != nullptr, "Error. Internal memregion is null");
    delete _int_mr;
  }

  size_t get_num_bytes() const { return _int_mr->get_num_bytes(); }

  MARE_DELETE_METHOD(memregion());
  MARE_DELETE_METHOD(memregion(memregion const&));
  MARE_DELETE_METHOD(memregion& operator= (memregion const&));
  MARE_DELETE_METHOD(memregion (memregion &&));
  MARE_DELETE_METHOD(memregion& operator= (memregion &&));

};

MARE_GCC_IGNORE_BEGIN("-Weffc++");

class main_memregion : public memregion{
public:

  static constexpr size_t s_default_alignment = 4096;

  explicit main_memregion(size_t sz, size_t alignment=s_default_alignment):
    memregion(new internal::internal_main_memregion(sz, alignment)){}

  main_memregion(void* ptr, size_t sz):
    memregion(new internal::internal_main_memregion(ptr, sz)){}

  MARE_DELETE_METHOD(main_memregion());
  MARE_DELETE_METHOD(main_memregion(main_memregion const&));
  MARE_DELETE_METHOD(main_memregion& operator= (main_memregion const&));
  MARE_DELETE_METHOD(main_memregion (main_memregion &&));
  MARE_DELETE_METHOD(main_memregion& operator= (main_memregion &&));

  void* get_ptr() const {
    return _int_mr->get_ptr();
  }

};

#ifdef MARE_HAVE_QTI_HEXAGON

class ion_memregion : public memregion{

public:

  explicit ion_memregion(size_t sz, bool cacheable=true):
    memregion(new internal::internal_ion_memregion(sz, cacheable)){}

  ion_memregion(void* ptr, size_t sz, bool cacheable):
    memregion(new internal::internal_ion_memregion(ptr, sz, cacheable)){}

  MARE_DELETE_METHOD(ion_memregion());
  MARE_DELETE_METHOD(ion_memregion(ion_memregion const&));
  MARE_DELETE_METHOD(ion_memregion& operator= (ion_memregion const&));
  MARE_DELETE_METHOD(ion_memregion (ion_memregion &&));
  MARE_DELETE_METHOD(ion_memregion& operator= (ion_memregion &&));

  void* get_ptr() const {return _int_mr->get_ptr();}

  bool is_cacheable() const {return _int_mr->is_cacheable();}

};
#endif

#ifdef MARE_HAVE_GLES

class glbuffer_memregion : public memregion{
public:

  explicit glbuffer_memregion(GLuint id):
    memregion(new internal::internal_glbuffer_memregion(id)){}

  MARE_DELETE_METHOD(glbuffer_memregion());
  MARE_DELETE_METHOD(glbuffer_memregion(glbuffer_memregion const&));
  MARE_DELETE_METHOD(glbuffer_memregion& operator= (glbuffer_memregion const&));
  MARE_DELETE_METHOD(glbuffer_memregion (glbuffer_memregion &&));
  MARE_DELETE_METHOD(glbuffer_memregion& operator= (glbuffer_memregion &&));

  GLuint get_id() const {return _int_mr->get_id();}
};
#endif

MARE_GCC_IGNORE_END("-Weffc++");

};
