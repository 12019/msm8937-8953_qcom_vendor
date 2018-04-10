// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#if defined(__ANDROID__)
  #if defined(__clang__)

    #if (__clang_major__ < 3) || (__clang_major == 3 && __clang_minor__ < 4)
      #error "MARE requires Android LLVM 3.4 or higher"
    #endif
  #endif
#else
  #error "Platform not supported!"
#endif
