// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/internal/compat/compiler_check.h>

#include <mare/affinity.hh>
#include <mare/buffer.hh>
#include <mare/index.hh>
#include <mare/range.hh>
#include <mare/runtime.hh>

#include <mare/group.hh>
#include <mare/groupptr.hh>
#include <mare/patterns.hh>
#include <mare/power.hh>

#include <mare/schedulerstorage.hh>
#include <mare/scopedstorage.hh>
#include <mare/taskstorage.hh>
#include <mare/threadstorage.hh>

#include <mare/taskfactory.hh>
#include <mare/texture.hh>
#include <mare/internal/compat/compat.h>

#include <mare/internal/task/task.cc.hh>

#ifdef __cplusplus
extern "C" {
#endif
const char *mare_version();
const char *mare_scm_revision();
int         mare_scm_changed();
const char *mare_scm_version();
const char *mare_scm_description();

#ifdef __cplusplus
}
#endif
