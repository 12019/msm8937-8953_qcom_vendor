// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#ifdef MARE_HAVE_GLES

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <mare/internal/util/macros.hh>

namespace mare {

namespace internal {

namespace testing {
class buffer_tests;
};

class egl
{
private:
  EGLDisplay _display;
  EGLConfig _config;
  EGLContext _context;
  EGLSurface _surface;
  EGLint _channel_r_bits;
  EGLint _channel_g_bits;
  EGLint _channel_b_bits;
  EGLint _channel_a_bits;
  EGLint _depth_buffer_bits;
  EGLint _stencil_buffer_bits;
  egl();
  ~egl();
public:
  static egl& get_instance() {
    static egl egl_instance;
    return egl_instance;
  }

  EGLContext get_context() { return _context; }
  EGLDisplay get_display() { return _display; }
  friend class internal::testing::buffer_tests;

private:
  MARE_DELETE_METHOD(egl(egl&));
  MARE_DELETE_METHOD(egl(egl&&));
  MARE_DELETE_METHOD(egl& operator=(egl&));
  MARE_DELETE_METHOD(egl& operator=(egl&&));
};

};
};

#endif
