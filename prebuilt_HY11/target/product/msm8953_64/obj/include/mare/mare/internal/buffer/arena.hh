// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <cstring>
#include <memory>

#ifdef MARE_HAVE_GLES
#include <GLES3/gl31.h>
#endif

#include <mare/internal/buffer/executordevice.hh>
#include <mare/internal/buffer/memregion.hh>
#include <mare/internal/compat/compilercompat.h>
#ifdef MARE_HAVE_OPENCL
#include <mare/internal/device/cldevice.hh>
#include <mare/internal/device/gpuopencl.hh>
#include <mare/internal/graphics/texture.hh>
#endif
#include <mare/internal/util/debug.hh>
#include <mare/internal/util/macros.hh>

namespace mare {
namespace internal {

enum arena_t : size_t {
  MAINMEM_ARENA = 0,
#ifdef MARE_HAVE_OPENCL
  CL_ARENA      = 1,
#endif
#ifdef MARE_HAVE_QTI_HEXAGON
  ION_ARENA     = 2,
#endif
#ifdef MARE_HAVE_GLES
  GL_ARENA      = 3,
#endif
#ifdef MARE_HAVE_OPENCL
  TEXTURE_ARENA = 4,
#endif
  NO_ARENA      = 5
};

constexpr size_t NUM_ARENA_TYPES = arena_t::NO_ARENA;

enum arena_alloc_t{
  UNALLOCATED,
  INTERNAL,
  EXTERNAL,
  BOUND
};

#ifdef MARE_HAVE_GLES

#define MARE_GL_CALL(GL_FUNC_NAME, ...) \
  {\
    GL_FUNC_NAME(__VA_ARGS__); \
    GLenum error = glGetError(); \
    MARE_API_THROW((error == GL_NO_ERROR), "glBindBuffer(0)->%x", error);\
  }

#define MARE_GL_CALL_WITH_RETURN_VALUE(GL_FUNC_NAME, ret, ...) \
  {\
    ret = GL_FUNC_NAME(__VA_ARGS__); \
    GLenum error = glGetError(); \
    MARE_API_THROW((error == GL_NO_ERROR), "glBindBuffer(0)->%x", error); \
  }

class gl_ssbo_binding_restorer{
public:
  gl_ssbo_binding_restorer() : _value_to_be_restored(0) {

    MARE_GL_CALL(glGetIntegerv, GL_SHADER_STORAGE_BUFFER_BINDING, &_value_to_be_restored);
  }

  MARE_DELETE_METHOD(gl_ssbo_binding_restorer(gl_ssbo_binding_restorer&));
  MARE_DELETE_METHOD(gl_ssbo_binding_restorer(gl_ssbo_binding_restorer&&));
  MARE_DELETE_METHOD(gl_ssbo_binding_restorer& operator=(gl_ssbo_binding_restorer&));
  MARE_DELETE_METHOD(gl_ssbo_binding_restorer& operator=(gl_ssbo_binding_restorer&&));

  ~gl_ssbo_binding_restorer(){
    MARE_GL_CALL(glBindBuffer, GL_SHADER_STORAGE_BUFFER, _value_to_be_restored);
  }
private:
  GLint _value_to_be_restored;
};
#endif

class arena
{

public:

  virtual void allocate(internal_memregion *, size_t){
    MARE_FATAL("Calling allocate method of base class arena");
  }

  virtual void allocate(size_t s) = 0;

  virtual void bind_allocate(arena *){
    MARE_FATAL("Calling bind_allocate method of base class arena");
  }

  virtual void deallocate() = 0;

  virtual void acquire(executor_device){

  }

  virtual void release(executor_device){

  }

  virtual void copy_in  (arena &src) { copy_in_sync(src); }

  virtual void copy_out (arena &dst) { copy_out_sync(dst); }

  virtual void * get_ptr() {
    MARE_FATAL("Calling get_ptr() method of base class arena");
  }

#ifdef MARE_HAVE_GLES
  virtual GLuint get_id(){
    MARE_FATAL("Calling get_id() method of base class arena");
  }
#endif

  size_t  get_size() const { return _size; }

  virtual arena_t get_type() const {
    return _type;
  }

  virtual arena_alloc_t get_alloc_type() const {
    return _alloc_type;
  }

#ifdef MARE_HAVE_OPENCL
  virtual cl::Buffer& get_ocl_buffer(){
    MARE_FATAL("Calling get_ocl_buffer() method of base class");
  }
#endif

  virtual void invalidate(){

  }

  virtual bool is_cacheable() const{
    MARE_FATAL("is_cacheable() is currently only supported for ion_arena");
  }

  virtual ~arena() {
  }

  arena * get_bound_to_arena() const {return _bound_to_arena;}

private:

  virtual void copy_in_sync  (arena &src) = 0;

  virtual void copy_out_sync (arena &dst) = 0;

  friend void copy_arenas(arena* src, arena* dest);

protected:
  explicit arena(arena_t type) : _size(0),
                                 _type(type),
                                 _alloc_type(UNALLOCATED),
                                 _bound_to_arena(nullptr){}

  MARE_DELETE_METHOD(arena(arena&));
  MARE_DELETE_METHOD(arena(arena&&));
  MARE_DELETE_METHOD(arena& operator=(arena&));
  MARE_DELETE_METHOD(arena& operator=(arena&&));

  size_t _size;
  arena_t const _type;
  arena_alloc_t _alloc_type;
  arena * _bound_to_arena;

};

inline void copy_arenas(arena* src, arena* dest){
  MARE_INTERNAL_ASSERT(src != nullptr, "Source Arena is null");
  MARE_INTERNAL_ASSERT(src->get_alloc_type() != UNALLOCATED,
                       "Source Arena must be allocated");
  MARE_INTERNAL_ASSERT(dest != nullptr, "Destination Arena is null");
  switch(src->get_type()){
    case MAINMEM_ARENA:
      switch(dest->get_type()){
        case MAINMEM_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            dest->copy_in(*src);
          }
          else{

            dest->allocate(src->get_size());
            dest->copy_in(*src);
          }
        break;
#ifdef MARE_HAVE_OPENCL
        case CL_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            dest->copy_in(*src);
          }
          else{

            dest->bind_allocate(src);
            dest->copy_in(*src);
          }
        break;
#endif
#ifdef MARE_HAVE_QTI_HEXAGON
        case ION_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            if(src->get_bound_to_arena() != dest){
              dest->copy_in(*src);
            }
          }
          else{

            dest->allocate(src->get_size());
            dest->copy_in(*src);
          }
        break;
#endif
#ifdef MARE_HAVE_GLES
        case GL_ARENA:

          if(dest->get_alloc_type() == UNALLOCATED){

            dest->bind_allocate(src);
          }
          dest->copy_in(*src);
        break;
#endif
#ifdef MARE_HAVE_OPENCL
        case TEXTURE_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            dest->copy_in(*src);
          }
          else{

            MARE_FATAL("Error. Texture arena should be allocated before being copied into");
          }
        break;
#endif
        case NO_ARENA:
        default:
          MARE_FATAL("Destination Arena is of unsupported type");
        break;
      }
    break;
#ifdef MARE_HAVE_OPENCL
    case CL_ARENA:
      switch(dest->get_type()){
        case MAINMEM_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            src->copy_out(*dest);
          }
          else{

#ifdef MARE_HAVE_QTI_HEXAGON
            if((src->get_alloc_type() == BOUND) && (src->get_bound_to_arena()->get_type() == ION_ARENA)){

              dest->bind_allocate(src->get_bound_to_arena());
            }
            else{
#endif

              dest->allocate(src->get_size());
              src->copy_out(*dest);
#ifdef MARE_HAVE_QTI_HEXAGON
            }
#endif
          }
        break;
        case CL_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){
            src->copy_out(*dest);
          }
          else{
            dest->allocate(src->get_size());
            src->copy_out(*dest);
          }
        break;
#ifdef MARE_HAVE_QTI_HEXAGON
        case ION_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            src->copy_out(*dest);
          }
          else{

            dest->allocate(src->get_size());
            src->copy_out(*dest);
          }
        break;
#endif
#ifdef MARE_HAVE_GLES
        case GL_ARENA:

          if(dest->get_alloc_type() == UNALLOCATED){
            dest->allocate(src->get_size());
          }
          dest->copy_in(*src);
        break;
#endif
#ifdef MARE_HAVE_OPENCL
        case TEXTURE_ARENA:

          MARE_INTERNAL_ASSERT(dest->get_alloc_type() != UNALLOCATED,
                               "Error. Texture arena must be allocated");
          src->copy_out(*dest);
        break;
#endif
        case NO_ARENA:
        default:
          MARE_FATAL("Destination Arena is of unsupported type");
        break;
      }
    break;
#endif
#ifdef MARE_HAVE_GLES
    case GL_ARENA:
      switch(dest->get_type()){
        case MAINMEM_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){
            src->copy_out(*dest);
          }
          else{
            dest->allocate(src->get_size());
            src->copy_out(*dest);
          }
        break;
#ifdef MARE_HAVE_OPENCL
        case CL_ARENA:

          if(dest->get_alloc_type() == UNALLOCATED){

            dest->bind_allocate(src);
          }
          src->copy_out(*dest);
        break;
#endif
#ifdef MARE_HAVE_QTI_HEXAGON
        case ION_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){
            src->copy_out(*dest);
          }
          else{
            dest->allocate(src->get_size());
            src->copy_out(*dest);
          }
        break;
#endif
        case GL_ARENA:

          if(dest->get_alloc_type() == UNALLOCATED){
            dest->allocate(src->get_size());
          }
          src->copy_out(*dest);
        break;
#ifdef MARE_HAVE_OPENCL
        case TEXTURE_ARENA:
          MARE_FATAL("Copying between GL and Texture arenas is currently unsupported");
        break;
#endif
        case NO_ARENA:
        default:
          MARE_FATAL("Destination Arena is of unsupported type");
        break;
      }
    break;
#endif
#ifdef MARE_HAVE_QTI_HEXAGON
    case ION_ARENA:
      switch(dest->get_type()){
        case MAINMEM_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            dest->copy_in(*src);
          }
          else{

            dest->bind_allocate(src);
            dest->copy_in(*src);
          }
        break;
#ifdef MARE_HAVE_OPENCL
        case CL_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            dest->copy_in(*src);
          }
          else{

            dest->bind_allocate(src);
            dest->copy_in(*src);
          }
        break;
#endif
        case ION_ARENA:

          if(dest->get_alloc_type() == UNALLOCATED){
            dest->allocate(src->get_size());
          }
          dest->copy_in(*src);
        break;
#ifdef MARE_HAVE_GLES
        case GL_ARENA:

          if(dest->get_alloc_type() == UNALLOCATED){

            dest->bind_allocate(src);
          }
          dest->copy_in(*src);
        break;
#endif
#ifdef MARE_HAVE_OPENCL
        case TEXTURE_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            dest->copy_in(*src);
          }
          else{

            MARE_FATAL("Texture arena should be allocated before it is copied into");
          }
          break;
#endif
        case NO_ARENA:
        default:
          MARE_FATAL("Destination Arena is of unsupported type");
        break;
      }
    break;
#endif
#ifdef MARE_HAVE_OPENCL
    case TEXTURE_ARENA:
      switch(dest->get_type()){
        case MAINMEM_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            src->copy_out(*dest);
          }
          else{

#ifdef MARE_HAVE_QTI_HEXAGON
            if((src->get_alloc_type() == BOUND) && (src->get_bound_to_arena()->get_type() == ION_ARENA)){

              dest->bind_allocate(src->get_bound_to_arena());
            }
            else{
#endif

              dest->allocate(src->get_size());
              src->copy_out(*dest);
#ifdef MARE_HAVE_QTI_HEXAGON
            }
#endif
          }
        break;
        case CL_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){
            src->copy_out(*dest);
          }
          else{
            dest->allocate(src->get_size());
            src->copy_out(*dest);
          }
        break;
#ifdef MARE_HAVE_QTI_HEXAGON
        case ION_ARENA:

          if(dest->get_alloc_type() != UNALLOCATED){

            src->copy_out(*dest);
          }
          else{

            dest->allocate(src->get_size());
            src->copy_out(*dest);
          }
        break;
#endif
#ifdef MARE_HAVE_GLES
        case GL_ARENA:
          MARE_FATAL("Copying between GL and Texture arenas is currently unsupported");
        break;
#endif
#ifdef MARE_HAVE_OPENCL
        case TEXTURE_ARENA:

          MARE_INTERNAL_ASSERT(dest->get_alloc_type() != UNALLOCATED,
                               "Error. Texture arena must be allocated");
          src->copy_out(*dest);
        break;
#endif
        case NO_ARENA:
        default:
          MARE_FATAL("Destination Arena is of unsupported type");
        break;
      }
    break;
#endif
    case NO_ARENA:
    default:
      MARE_FATAL("Source Arena is of unsupported type");
    break;
  }
}

MARE_GCC_IGNORE_BEGIN("-Weffc++");
class mainmem_arena : public arena {
MARE_GCC_IGNORE_END("-Weffc++");
public:
  mainmem_arena() : arena(MAINMEM_ARENA),
                   _ptr(nullptr){}

  ~mainmem_arena() {
    deallocate();
  }

  void bind_allocate(arena *p) {
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");
    _ptr = p->get_ptr();
    _size = p->get_size();
    _alloc_type = BOUND;
    auto arena_p_is_bound_to = p->get_bound_to_arena();
    _bound_to_arena = arena_p_is_bound_to == nullptr? p: arena_p_is_bound_to;
  }

  virtual void allocate(internal_memregion* mr, size_t size_in_bytes) {
    MARE_INTERNAL_ASSERT((mr->get_type() == memregion_t::main),
                         "Creating CPU arena from unsupported memregion");
    MARE_INTERNAL_ASSERT(mr->get_ptr() != nullptr, "p should not be null");

    MARE_INTERNAL_ASSERT(mr->get_num_bytes() >= size_in_bytes, "Requested arena size is greater than memregion size");
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");
    _ptr = mr->get_ptr();
    _size = size_in_bytes;
    _alloc_type = EXTERNAL;
  }

  virtual void allocate(size_t s) {
    MARE_INTERNAL_ASSERT(s != 0, "Allocating an arena of size 0");
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");
    _ptr = malloc(s);
    if(_ptr == nullptr)
      throw std::bad_alloc();
    _size = s;
    _alloc_type = INTERNAL;
  }

  void allocate(void* p, size_t s){
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");
    _ptr = p;
    MARE_INTERNAL_ASSERT(_ptr != nullptr, "Nullptr passed as input");
    _size = s;
    _alloc_type = EXTERNAL;
  }

  virtual void deallocate() {
    MARE_INTERNAL_ASSERT(get_alloc_type() == UNALLOCATED || _ptr != nullptr,
                         "Error: allocated mainmem_arena has _ptr == nullptr");
    if(get_alloc_type() == INTERNAL)
      free(_ptr);

    if(get_alloc_type() == BOUND){
      _bound_to_arena = nullptr;
    }
    _ptr = nullptr;
    _alloc_type = UNALLOCATED;
  }

  void * get_ptr() {

    if(get_alloc_type() == BOUND){
      return get_bound_to_arena()->get_ptr();
    }

    if(_ptr == nullptr){
      MARE_INTERNAL_ASSERT(get_size() != 0, "Allocating an arena of size 0");
      allocate(get_size());
    }
    return _ptr;
  }

private:
  void *_ptr;

  virtual void copy_in_sync(arena &src) {
    if(get_bound_to_arena() == &src){
      MARE_INTERNAL_ASSERT(get_alloc_type() == BOUND, "Error. Arena is bound but its type is not updated");
    }
    else{
      auto ptr = get_ptr();
      auto src_ptr = src.get_ptr();
      MARE_INTERNAL_ASSERT(ptr != nullptr,"Error. Host accessible destination pointer is nullptr");
      MARE_INTERNAL_ASSERT(src_ptr!= nullptr,"Error. Host accessible source pointer is nullptr");
      MARE_INTERNAL_ASSERT(src.get_size() >= get_size(), "Source arena is larger than dest");

      if(ptr != src_ptr)
        memcpy(get_ptr(), src.get_ptr(), get_size());
    }
  }

  virtual void copy_out_sync (arena &dst) {

    dst.copy_in(*this);
  }
};

#ifdef MARE_HAVE_OPENCL

MARE_GCC_IGNORE_BEGIN("-Weffc++");
class cl_arena : public arena {
MARE_GCC_IGNORE_END("-Weffc++");
private:
  ::mare::internal::cldevice* _device;
  cl::Buffer _ocl_buffer;
  cl:: Event* _event;
  void* _mapped_ptr;

  bool _bound_with_host_accessible_pointer;

  enum cl_arena_state{
    INIT,
    MAPPED,
    UNMAPPED
  };

  cl_arena_state _cl_state;

  bool is_acquired_by_device(executor_device ed) const{
    if(ed == executor_device::cpu)
      return (get_state() == MAPPED);
    else if(ed == executor_device::gpucl)
      return (get_state() != MAPPED);
    else{
      MARE_FATAL("Checking if CL arena is accessible by unsupported device");
    }
  }

  void map(){
    MARE_INTERNAL_ASSERT(get_alloc_type() != UNALLOCATED, "Error. Mapping an unallocated cl arena");
    MARE_INTERNAL_ASSERT(get_state() != MAPPED, "Error. Attempting to map an already mapped arena");
    _mapped_ptr = _device->map_async(_ocl_buffer, get_size(), *_event);
    _event->wait();
    _cl_state = MAPPED;
  }

  void unmap(){
    MARE_INTERNAL_ASSERT(get_state() == MAPPED, "Error. Arena is not mapped");
    _device->unmap_async(_ocl_buffer, _mapped_ptr, *_event);
    _event->wait();
    _cl_state = UNMAPPED;

    if(!_bound_with_host_accessible_pointer)
      _mapped_ptr = nullptr;
  }

  virtual void copy_in_sync (arena &src) {

    MARE_INTERNAL_ASSERT(src.get_ptr() != nullptr, "invalid source pointer");

    if(get_bound_to_arena() != nullptr &&
       (get_bound_to_arena() == &src || get_bound_to_arena() == src.get_bound_to_arena())) {
      MARE_INTERNAL_ASSERT(src.get_size() <= get_size(),
                           "destination arena too small dst(%zu) > src(%zu)",
                           get_size(), src.get_size());

      if(get_state() != MAPPED)
        _device->copy_to_device_blocking(_ocl_buffer, src.get_ptr(), src.get_size());

    } else{
      acquire(executor_device::cpu);
      MARE_INTERNAL_ASSERT(get_state() == MAPPED,"Error. Expect CL arena state to be mapped.");
      memcpy(get_ptr(), src.get_ptr(), get_size());
    }
  }

  virtual void copy_out_sync (arena &dst) {

    if(get_bound_to_arena() != nullptr &&
       (get_bound_to_arena() == &dst || get_bound_to_arena() == dst.get_bound_to_arena())){
      MARE_INTERNAL_ASSERT(get_alloc_type() == BOUND, "Error Arena is bound, but alloc_type has not been updated");
      MARE_INTERNAL_ASSERT(dst.get_size() >= get_size(),
                           "destination arena too small dst(%zu) < src(%zu)",
                           dst.get_size(), get_size());

      if(get_state() != MAPPED)
        _device->copy_from_device_blocking(_ocl_buffer, get_ptr(), get_size());
    } else{
      acquire(executor_device::cpu);
      MARE_INTERNAL_ASSERT(get_state() == MAPPED,"Error. Expect CL arena state to be mapped.");

      dst.copy_in(*this);
    }
  }

public:
  explicit cl_arena() : arena(CL_ARENA),
                        _device(nullptr),
                        _ocl_buffer(),
                        _event(new cl::Event()),
                        _mapped_ptr(nullptr),
                        _bound_with_host_accessible_pointer(false),
                        _cl_state(INIT)
  {
    _device = ::mare::internal::c_ptr( ::mare::internal::get_default_cldevice() );
  }

  ~cl_arena() {
    deallocate();
    delete _event;
  }

  cl::Buffer& get_ocl_buffer() { return _ocl_buffer; }

  cl_arena_state get_state() const {return _cl_state;}

  void bind_allocate(arena *p) {
    MARE_INTERNAL_ASSERT(p != nullptr, "p should not be null");
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");
    _size = p->get_size();
    _alloc_type = BOUND;
    auto arena_p_is_bound_to = p->get_bound_to_arena();
    _bound_to_arena = arena_p_is_bound_to == nullptr? p: arena_p_is_bound_to;
    cl_int status;

    if(_bound_to_arena->get_type() == MAINMEM_ARENA){
      _mapped_ptr = _bound_to_arena->get_ptr();
      _bound_with_host_accessible_pointer = true;
      unsigned long long flags = CL_MEM_READ_WRITE |
                                 CL_MEM_USE_HOST_PTR;
      try {
        _ocl_buffer =  cl::Buffer(_device->get_context(),
                                  flags,
                                  get_size(),
                                  _mapped_ptr,
                                  &status);
      }
      catch (cl::Error& err) {
        MARE_FATAL("cl::Buffer(%llu, %zu, %p)->%s,",
                   flags, get_size(), _mapped_ptr,
                   ::mare::internal::get_cl_error_string(err.err()));
      }

    }
#ifdef MARE_HAVE_GLES
    else if(_bound_to_arena->get_type() == GL_ARENA){
      GLuint gl_buffer = p->get_id();
      unsigned long long flags = CL_MEM_READ_WRITE;
      try {
        _ocl_buffer =  cl::BufferGL(_device->get_context(),
                                    flags,
                                    gl_buffer,
                                    &status);
      }
      catch (cl::Error& err) {
        MARE_FATAL("cl::BufferGL(%llu, %u)->%s",
                    flags, gl_buffer, get_cl_error_string(err.err()));
      }
      catch (...) {
        MARE_FATAL("Unknown error in cl::BufferGL(%llu, %u)",
                    flags, gl_buffer);
      }
    }
#endif
#if MARE_HAVE_QTI_HEXAGON
    else if(_bound_to_arena->get_type() == ION_ARENA){
      _mapped_ptr = _bound_to_arena->get_ptr();
      _bound_with_host_accessible_pointer = true;

      cl_mem_ion_host_ptr ionmem_host_ptr;

      ionmem_host_ptr.ext_host_ptr.allocation_type = CL_MEM_ION_HOST_PTR_QCOM;

      if(_bound_to_arena->is_cacheable())
       ionmem_host_ptr.ext_host_ptr.host_cache_policy = CL_MEM_HOST_WRITEBACK_QCOM;
      else
        ionmem_host_ptr.ext_host_ptr.host_cache_policy = CL_MEM_HOST_UNCACHED_QCOM;

      ionmem_host_ptr.ion_hostptr = _mapped_ptr;
      ionmem_host_ptr.ion_filedesc = rpcmem_to_fd(ionmem_host_ptr.ion_hostptr);
      unsigned long long flags = CL_MEM_READ_WRITE |
                                CL_MEM_USE_HOST_PTR | CL_MEM_EXT_HOST_PTR_QCOM;
      try {
        _ocl_buffer =  cl::Buffer(_device->get_context(),
                                  flags,
                                  get_size(),
                                  &ionmem_host_ptr,
                                  &status);
      }
      catch (cl::Error& err) {
      MARE_FATAL("cl::Buffer(%llu, %zu, %p)->%d",
                 flags, get_size(), _mapped_ptr, err.err());
      }
      catch (...) {
        MARE_FATAL("Unknown error in cl::Buffer(%llu, %zu,%p)",
                   flags, get_size(), nullptr);
      }
    }
#endif
    else{
      MARE_FATAL("Calling bind allocate with unsupported arena type");
    }
  }

  virtual void allocate(internal_memregion*, size_t){
    MARE_FATAL("Trying to allocate CL_ARENA from memregion");
  }

  virtual void allocate(size_t s) {
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");
    _size = s;
    _alloc_type = INTERNAL;

    cl_int status;
    unsigned long long flags = CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR;
    try {
      _ocl_buffer =  cl::Buffer(_device->get_context(),
                                flags,
                                get_size(),
                                nullptr,
                                &status);
    }
    catch (cl::Error& err) {
      MARE_FATAL("cl::Buffer(%llu, %zu, %p)->%s",
                 flags, get_size(), nullptr,
                 ::mare::internal::get_cl_error_string(err.err()));
    }
    catch (...) {
      MARE_FATAL("Unknown error in cl::Buffer(%llu, %zu,%p)",
                 flags, get_size(), nullptr);
    }
  }

  virtual void deallocate() {

    if(get_alloc_type() == BOUND){
      _bound_to_arena = nullptr;
    }
    _mapped_ptr = nullptr;
    _alloc_type = UNALLOCATED;
  }

  virtual void acquire(executor_device ed){
    if(ed == executor_device::cpu){
      if(!is_acquired_by_device(ed))
        map();
    }
    else if(ed == executor_device::gpucl){
      if(!is_acquired_by_device(ed))
        unmap();
    }
    else{
      MARE_FATAL("Trying to make CL arena accessible to unsupported device");
    }
  }

  virtual void release(executor_device ed){
    if(ed == executor_device::gpucl){
#ifdef MARE_HAVE_QTI_HEXAGON
      if(_alloc_type == BOUND && _bound_to_arena->get_type() == ION_ARENA){
        map();
      }
#endif
    }
  }

  void * get_ptr() {
    return _mapped_ptr;
  }

};
#endif

#ifdef MARE_HAVE_GLES

MARE_GCC_IGNORE_BEGIN("-Weffc++");
class gl_arena : public arena {
MARE_GCC_IGNORE_END("-Weffc++");
public:
  explicit gl_arena() : arena(GL_ARENA),
#ifdef MARE_HAVE_OPENCL
                        _device(nullptr),
                        _ocl_buffer(),
                        _event(nullptr),
#endif
                        _gl_buffer(0),
                        _ptr(nullptr)
  {
#ifdef MARE_HAVE_OPENCL
    _device = ::mare::internal::c_ptr( ::mare::internal::get_default_cldevice() );
#endif
  }

  virtual void bind_allocate(arena *p) {

    MARE_INTERNAL_ASSERT(p != nullptr, "p should not be null");
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");
    _size = p->get_size();
    _alloc_type = BOUND;
    auto arena_p_is_bound_to = p->get_bound_to_arena();
    _bound_to_arena = arena_p_is_bound_to == nullptr? p: arena_p_is_bound_to;

    if(_bound_to_arena->get_type() == MAINMEM_ARENA
#ifdef MARE_HAVE_QTI_HEXAGON
        || _bound_to_arena->get_type() == ION_ARENA
#endif
      ){
      gl_ssbo_binding_restorer restorer;
      MARE_GL_CALL(glGenBuffers, 1, &_gl_buffer);
      MARE_GL_CALL(glBindBuffer, GL_SHADER_STORAGE_BUFFER, _gl_buffer);
      MARE_GL_CALL(glBufferData, GL_SHADER_STORAGE_BUFFER, _size,
                   static_cast<const void*>(_bound_to_arena->get_ptr()), GL_DYNAMIC_COPY);
    }
    else{
      MARE_FATAL("Binding GL arena to an arena of unsupported type");
    }
  }

  virtual void allocate(internal_memregion *mr, size_t size_in_bytes){
    MARE_INTERNAL_ASSERT((mr->get_type() == memregion_t::glbuffer),
                         "Allocating ion arena from unknown memregion type.");
    MARE_INTERNAL_ASSERT(mr->get_num_bytes() >= size_in_bytes, "Requested arena size is greater than memregion size");
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");
    _gl_buffer = mr->get_id();
    _size = size_in_bytes;
    _alloc_type = EXTERNAL;
  }

  virtual void allocate(size_t sz) {
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");

    gl_ssbo_binding_restorer restorer;
    MARE_GL_CALL(glGenBuffers, 1, &_gl_buffer);
    MARE_GL_CALL(glBindBuffer, GL_SHADER_STORAGE_BUFFER, _gl_buffer);
    MARE_GL_CALL(glBufferData, GL_SHADER_STORAGE_BUFFER, sz, nullptr, GL_DYNAMIC_COPY);
    _alloc_type = INTERNAL;
    _size = sz;
  }

  virtual void deallocate() {

    _ptr = nullptr;

    if(_alloc_type == INTERNAL){
      MARE_GL_CALL(glDeleteBuffers, 1, &_gl_buffer);
    }

    _alloc_type = UNALLOCATED;
  }

  void * get_ptr() {return _ptr;}

  GLuint get_id() {return _gl_buffer;}

  virtual void acquire(executor_device ed){
    if(ed == executor_device::cpu){

      if(_ptr == nullptr){

        gl_ssbo_binding_restorer restorer;
        MARE_GL_CALL(glBindBuffer, GL_SHADER_STORAGE_BUFFER, _gl_buffer);
        MARE_GL_CALL_WITH_RETURN_VALUE(glMapBufferRange, _ptr, GL_SHADER_STORAGE_BUFFER, 0, _size, GL_MAP_WRITE_BIT);
        if(_ptr == nullptr) {
          MARE_FATAL("glMapBufferRange returned NULL");
        }
      }
      MARE_INTERNAL_ASSERT(_ptr != nullptr, "GL Buffer is still unmapped");
    }
    else if(ed == executor_device::gpucl){

      if(_ptr != nullptr){

        gl_ssbo_binding_restorer restorer;

        MARE_GL_CALL(glBindBuffer, GL_SHADER_STORAGE_BUFFER, _gl_buffer);
        GLuint ret;
        MARE_GL_CALL_WITH_RETURN_VALUE(glUnmapBuffer, ret, GL_SHADER_STORAGE_BUFFER);
        if(ret != GL_TRUE) {
          MARE_FATAL("glUnmapBuffer()->%u failed", ret);
        }
        _ptr = nullptr;
      }
    }
  }

  virtual void release(executor_device){

  }

private:
#ifdef MARE_HAVE_OPENCL
  ::mare::internal::cldevice* _device;
  cl::Buffer _ocl_buffer;
  cl::Event* _event;
#endif
  GLuint _gl_buffer;
  void* _ptr;

#ifdef MARE_HAVE_OPENCL

  void copy_to_bound_cl_arena(cl::Buffer& dst_ocl_buffer){

    glFinish();
    clEnqueueAcquireGLObjects(_device->get_cmd_queue()(),
                              1,
                              &dst_ocl_buffer(),
                              0,
                              nullptr,
                              nullptr);

  }

  void copy_from_bound_cl_arena(cl::Buffer& dst_ocl_buffer){
    clEnqueueReleaseGLObjects(_device->get_cmd_queue()(),
                              1,
                              &dst_ocl_buffer(),
                              0,
                              nullptr,
                              &(*_event)());
    clFinish(_device->get_cmd_queue()());

  }
#endif

  virtual void copy_in_sync (arena & src) {
#ifdef MARE_HAVE_OPENCL

    if(src.get_type() == CL_ARENA && src.get_bound_to_arena() == this){
      copy_from_bound_cl_arena(src.get_ocl_buffer());
      return;
    }
#endif
    MARE_INTERNAL_ASSERT(src.get_ptr() != nullptr, "invalid source pointer");

    if(_ptr == nullptr){

      gl_ssbo_binding_restorer restorer;

      MARE_GL_CALL(glBindBuffer, GL_SHADER_STORAGE_BUFFER, _gl_buffer);
      MARE_GL_CALL_WITH_RETURN_VALUE(glMapBufferRange, _ptr, GL_SHADER_STORAGE_BUFFER, 0, _size, GL_MAP_WRITE_BIT);
      if(_ptr == nullptr) {
        MARE_FATAL("glMapBufferRange returned NULL");
      }
    }
    MARE_INTERNAL_ASSERT(_ptr != nullptr, "GL Buffer is still unmapped");

    memcpy(get_ptr(), src.get_ptr(), get_size());

    gl_ssbo_binding_restorer restorer;

    MARE_GL_CALL(glBindBuffer, GL_SHADER_STORAGE_BUFFER, _gl_buffer);
    GLuint ret;
    MARE_GL_CALL_WITH_RETURN_VALUE(glUnmapBuffer, ret, GL_SHADER_STORAGE_BUFFER);
    if(ret != GL_TRUE) {
      MARE_FATAL("glUnmapBuffer()->%u failed", ret);
    }
    _ptr = nullptr;
  }

  virtual void copy_out_sync (arena & dst) {
#ifdef MARE_HAVE_OPENCL

    if(dst.get_type() == CL_ARENA && dst.get_bound_to_arena() == this){
      copy_to_bound_cl_arena(dst.get_ocl_buffer());
      return;
    }
#endif

    MARE_INTERNAL_ASSERT(get_size() <= dst.get_size(),"destination not large enough");

    if(_ptr == nullptr){

      gl_ssbo_binding_restorer restorer;
      MARE_GL_CALL(glBindBuffer, GL_SHADER_STORAGE_BUFFER, _gl_buffer);
      MARE_GL_CALL_WITH_RETURN_VALUE(glMapBufferRange, _ptr, GL_SHADER_STORAGE_BUFFER, 0, _size, GL_MAP_WRITE_BIT);
      if(_ptr == nullptr) {
        MARE_FATAL("glMapBufferRange returned NULL");
      }
    }
    dst.copy_in(*this);
  }
};
#endif

#ifdef MARE_HAVE_QTI_HEXAGON

MARE_GCC_IGNORE_BEGIN("-Weffc++");
class ion_arena : public arena {
MARE_GCC_IGNORE_END("-Weffc++");

public:
  ion_arena() : arena(ION_ARENA),
                _ptr(nullptr),
                _cacheable(false){}

  ~ion_arena() { deallocate(); }

  void bind_allocate(arena*) {
    MARE_FATAL("Cannot currently bind an ion_arena to another arena");
  }

  virtual void allocate(internal_memregion *mr, size_t size_in_bytes){
    MARE_INTERNAL_ASSERT((mr->get_type() == memregion_t::ion),
                         "Allocating ion arena from unknown memregion type.");
    MARE_INTERNAL_ASSERT(mr->get_ptr() != nullptr, "p should not be null");

    MARE_INTERNAL_ASSERT(mr->get_num_bytes() >= size_in_bytes,
                         "Requested arena size is greater than memregion size");
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");
    _ptr = mr->get_ptr();
    MARE_INTERNAL_ASSERT(_ptr != nullptr, "failed to allocate hexagon Arena");
    _cacheable = mr->is_cacheable();
    _size = size_in_bytes;
    _alloc_type = EXTERNAL;
  }

  virtual void allocate(size_t s);

  virtual void deallocate();

  virtual void * get_ptr() {

    MARE_INTERNAL_ASSERT(_ptr != nullptr, "invalid host pointer");
    return _ptr;
  }

  bool is_cacheable() const {return _cacheable;}

private:
  void* _ptr;

  bool _cacheable;

  virtual void copy_in_sync(arena &src) {

    MARE_INTERNAL_ASSERT(src.get_ptr() != get_ptr(),
                         "Arenas map to the same memory region. Should be bound");
    MARE_INTERNAL_ASSERT(get_ptr() != nullptr && src.get_ptr() != nullptr,
                         "invalid source or destination pointers");
    MARE_INTERNAL_ASSERT(src.get_size() >= get_size(), "not enough data in src");
    memcpy(get_ptr(), src.get_ptr(), get_size());

  }

  virtual void copy_out_sync (arena &dst) {

    dst.copy_in(*this);
  }
};

#endif

#ifdef MARE_HAVE_OPENCL

MARE_GCC_IGNORE_BEGIN("-Weffc++");
class texture_arena : public arena {
MARE_GCC_IGNORE_END("-Weffc++");
private:
  ::mare::graphics::internal::base_texture_cl* _tex_cl;
  void*                                        _mapped_ptr;

  enum tex_arena_state{
    INIT,
    MAPPED,
    UNMAPPED
  };

  tex_arena_state _tex_state;

  bool is_acquired_by_device(executor_device ed) const{
    if(ed == executor_device::cpu)
      return (_tex_state == MAPPED);
    else if(ed == executor_device::gputexture)
      return !(_tex_state == MAPPED);
    else{
      MARE_FATAL("Checking if texture arena is accessible by unsupported device");
    }
  }

  void map(){
    MARE_INTERNAL_ASSERT(_tex_cl != nullptr, "Null _tex_cl");
    if(!is_mapped()) {
      _mapped_ptr = _tex_cl->map();
      _tex_state = MAPPED;
    }
  }

  void unmap(){
    MARE_INTERNAL_ASSERT(_tex_cl != nullptr, "Null _tex_cl");
    if(is_mapped()) {
      _tex_cl->unmap();
      _tex_state = UNMAPPED;
    }
  }

public:
  explicit texture_arena() :
    arena(TEXTURE_ARENA),
    _tex_cl(nullptr),
    _mapped_ptr(nullptr),
    _tex_state(INIT)
  {}

  ~texture_arena() {
    deallocate();
  }

  mare::graphics::internal::base_texture_cl* get_texture() const { return _tex_cl; }

  bool has_format() { return get_texture() != nullptr; }

  void set_texture(mare::graphics::internal::base_texture_cl* tex_cl) {
    MARE_INTERNAL_ASSERT(tex_cl != nullptr, "Null tetxure");
    _tex_cl = tex_cl;
  }

  bool is_mapped() const { return (_tex_state == MAPPED); }

  virtual void allocate(internal_memregion*, size_t){
    MARE_FATAL("Only bind_allocate() supported for texture arena");
  }

  virtual void allocate(size_t ) {
    MARE_FATAL("Only bind_allocate() supported for texture arena");
  }

  virtual void bind_allocate(arena *p) {
    MARE_INTERNAL_ASSERT(p != nullptr, "p should not be null");
    MARE_INTERNAL_ASSERT(_alloc_type == UNALLOCATED, "Error. Reallocating arena");
    _size = p->get_size();
    _alloc_type = BOUND;
    auto arena_p_is_bound_to = p->get_bound_to_arena();
    _bound_to_arena = arena_p_is_bound_to == nullptr? p: arena_p_is_bound_to;

    MARE_INTERNAL_ASSERT(_bound_to_arena->get_type() == MAINMEM_ARENA
#ifdef MARE_HAVE_QTI_HEXAGON
                         || _bound_to_arena->get_type() == ION_ARENA
#endif
                         ,"Calling bind allocate with unsupported base arena type");
  }

  virtual void deallocate() {
    delete _tex_cl;
    _tex_cl = nullptr;
    _alloc_type = UNALLOCATED;
  }

  virtual void acquire(executor_device ed){
    MARE_INTERNAL_ASSERT(_tex_cl != nullptr,
                         "texture has not yet been setup");
    if(ed == executor_device::cpu){
      if(!is_acquired_by_device(ed))
        map();
    }
    else if(ed == executor_device::gputexture){
      if(!is_acquired_by_device(ed))
        unmap();
    }
    else{
      MARE_FATAL("Trying to make texture arena accessible to unsupported device");
    }
  }

  virtual void release(executor_device ed){
    if(ed == executor_device::gputexture && is_acquired_by_device(ed)){
      if(_alloc_type == BOUND){

        map();
      }
    }
  }

  void * get_ptr() {
    return _mapped_ptr;
  }

private:

  virtual void copy_in_sync (arena &src) {
    MARE_INTERNAL_ASSERT(src.get_ptr() != nullptr, "invalid source pointer");
    MARE_INTERNAL_ASSERT(src.get_size() <= get_size(),
                        "destination arena too small dst(%zu) > src(%zu)",
                        get_size(), src.get_size());

    if(get_bound_to_arena() != nullptr &&
       (get_bound_to_arena() == &src ||
        get_bound_to_arena() == src.get_bound_to_arena())){
      if(_tex_state == UNMAPPED)
        MARE_FATAL( "Error. Texture arena must be in mapped or INIT state to copy_in");

    }
    else{

      acquire(executor_device::cpu);
      MARE_INTERNAL_ASSERT(is_mapped() ,"Error. Expect Texture arena state to be mapped.");
      memcpy(get_ptr(), src.get_ptr(), get_size());
    }
  }

  virtual void copy_out_sync (arena &dst) {
    MARE_INTERNAL_ASSERT(dst.get_size() >= get_size(),
                         "destination arena too small dst(%zu) < src(%zu)",
                         dst.get_size(), get_size());

    acquire(executor_device::cpu);
    MARE_INTERNAL_ASSERT(is_mapped(), "Error. Expect Texture arena state to be mapped.");

    if(get_bound_to_arena() != nullptr &&
       (get_bound_to_arena() == &dst ||
        get_bound_to_arena() == dst.get_bound_to_arena())){

    }
    else{

      dst.copy_in(*this);
    }
  }

};
#endif

};
};
