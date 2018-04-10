// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <cstddef>
#ifdef MARE_HAVE_GLES
#include <GLES3/gl31.h>
#endif

#include <mare/internal/compat/compilercompat.h>
#ifdef MARE_HAVE_OPENCL
#include <mare/internal/device/cldevice.hh>
#include <mare/internal/device/gpuopencl.hh>
#endif
#include <mare/internal/memalloc/alignedmalloc.hh>
#include <mare/internal/util/debug.hh>
#include <mare/internal/util/macros.hh>
#include <mare/internal/util/scopeguard.hh>

namespace mare{
namespace internal{

#ifdef MARE_HAVE_OPENCL
extern std::vector<legacy::device_ptr> *s_dev_ptrs;
#endif

enum class memregion_t {
  none,
  main,
  ion,
  glbuffer
};

class internal_memregion{
public:
  internal_memregion() : _num_bytes(0){}

  explicit internal_memregion(size_t sz) : _num_bytes(sz) {}

  virtual ~internal_memregion(){}

  virtual memregion_t get_type() const { return memregion_t::none; }

  virtual void* get_ptr(void ) const {
    MARE_FATAL("Calling get_ptr() method of base class internal_memregion.");
    return nullptr;
  }

#ifdef MARE_HAVE_GLES
  virtual GLuint get_id() const {
    MARE_FATAL("Calling get_id() method of base class internal_memregion.");
  }
#else
  virtual int get_id() const {
    MARE_FATAL("Calling get_id() method of base class internal_memregion.");
   }
#endif

  size_t get_num_bytes() const { return _num_bytes;}

  virtual bool is_cacheable() const {return false;}

protected:
  size_t _num_bytes;

  MARE_DELETE_METHOD(internal_memregion(internal_memregion const&));
  MARE_DELETE_METHOD(internal_memregion& operator=(internal_memregion const&));
  MARE_DELETE_METHOD(internal_memregion(internal_memregion&&));
  MARE_DELETE_METHOD(internal_memregion& operator=(internal_memregion&&));
};

MARE_GCC_IGNORE_BEGIN("-Weffc++");
class internal_main_memregion : public internal_memregion {
MARE_GCC_IGNORE_END("-Weffc++");
private:
  void* _p;
  bool _is_internally_allocated;
public:
  internal_main_memregion(size_t sz, size_t alignment) :
    internal_memregion(sz),
    _p(nullptr),
    _is_internally_allocated(true)
  {
    MARE_INTERNAL_ASSERT(sz != 0, "Error. Specified size of mem region is 0");
    _p = ::mare::internal::mare_aligned_malloc(alignment,sz);
  }

  internal_main_memregion(void* ptr, size_t sz) :
    internal_memregion(sz),
    _p(ptr),
    _is_internally_allocated(false)
  {

    MARE_API_ASSERT(ptr != nullptr, "Error. Expect non nullptr for externally allocated memregion");
  }

  ~internal_main_memregion()
  {

    if(_is_internally_allocated){
      MARE_INTERNAL_ASSERT(_p != nullptr, "Error. mem region pointer is nullptr");
      ::mare::internal::mare_aligned_free(_p);
    }
  }

  void* get_ptr(void ) const {return _p;}

  memregion_t get_type() const {return memregion_t::main;}

  MARE_DELETE_METHOD(internal_main_memregion(internal_main_memregion const&));
  MARE_DELETE_METHOD(internal_main_memregion& operator=(internal_main_memregion const&));
  MARE_DELETE_METHOD(internal_main_memregion(internal_main_memregion&&));
  MARE_DELETE_METHOD(internal_main_memregion& operator=(internal_main_memregion&&));
};

#ifdef MARE_HAVE_QTI_HEXAGON
MARE_GCC_IGNORE_BEGIN("-Weffc++");
class internal_ion_memregion : public internal_memregion {
 MARE_GCC_IGNORE_END("-Weffc++");
private:
  bool _cacheable;

  void* _p;

  bool _is_internally_allocated;

public:
  internal_ion_memregion(size_t sz, bool cacheable);

  internal_ion_memregion(void* ptr, size_t sz, bool cacheable);

  ~internal_ion_memregion();

  void* get_ptr(void ) const {return _p;}

  memregion_t get_type() const {return memregion_t::ion;}

  bool is_cacheable() const {return _cacheable;}

  MARE_DELETE_METHOD(internal_ion_memregion(internal_ion_memregion const&));
  MARE_DELETE_METHOD(internal_ion_memregion& operator=(internal_ion_memregion const&));
  MARE_DELETE_METHOD(internal_ion_memregion(internal_ion_memregion&&));
  MARE_DELETE_METHOD(internal_ion_memregion& operator=(internal_ion_memregion&&));
};
#endif

#ifdef MARE_HAVE_GLES
class internal_glbuffer_memregion : public internal_memregion {
private:
  GLuint _id;
public:

  explicit internal_glbuffer_memregion(GLuint id):
    internal_memregion(),
    _id(id)
  {

    GLint old_value;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_value);

    auto guard = make_scope_guard([old_value] {
        glBindBuffer(GL_ARRAY_BUFFER, old_value);
        GLenum error = glGetError();
        MARE_API_THROW((error == GL_NO_ERROR), "glBindBuffer(0)->%x", error);
      });

    glBindBuffer(GL_ARRAY_BUFFER, _id);
    GLenum error = glGetError();

    MARE_API_THROW((error == GL_NO_ERROR), "glBindBuffer(%u)->%x", _id,
                     error);

    MARE_API_THROW((glIsBuffer(_id) == GL_TRUE), "Invalid GL buffer");

    GLint gl_buffer_size = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &gl_buffer_size);
    error = glGetError();
    MARE_API_THROW((error == GL_NO_ERROR), "glGetBufferParameteriv()->%d", error);

    _num_bytes = gl_buffer_size;
    MARE_DLOG("GL buffer object size: %zu", _num_bytes);
    MARE_API_THROW(_num_bytes != 0, "GL buffer size can't be 0");
  }

  GLuint get_id() const {return _id;}

  memregion_t get_type() const {return memregion_t::glbuffer;}

  MARE_DELETE_METHOD(internal_glbuffer_memregion(internal_glbuffer_memregion const&));
  MARE_DELETE_METHOD(internal_glbuffer_memregion& operator=(internal_glbuffer_memregion const&));
  MARE_DELETE_METHOD(internal_glbuffer_memregion(internal_glbuffer_memregion&&));
  MARE_DELETE_METHOD(internal_glbuffer_memregion& operator=(internal_glbuffer_memregion&&));
};
#endif

};
};
