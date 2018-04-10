// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <mare/taskfactory.hh>

namespace mare {
namespace affinity {

enum class cores {
  all,
  big,
  little
};

enum class mode {
  allow_local_setting,
  override_local_setting,
};

class settings {

public:

  explicit settings(cores cores_attribute, bool pin_threads, mode md = ::mare::affinity::mode::allow_local_setting) :
    _cores(cores_attribute)
    , _pin_threads(pin_threads)
    , _mode(md)
  {}

  ~settings() {}

  cores get_cores() const { return _cores; }

  void set_cores(cores cores_attribute) { _cores = cores_attribute; }

  bool get_pin_threads() const { return _pin_threads; }

  void set_pin_threads() { _pin_threads = true; }

  void reset_pin_threads() { _pin_threads = false; }

  mode get_mode() const { return _mode; }

  void set_mode(mode md) { _mode = md; }

  bool operator==(const settings& rhs) const {
    return (_cores == rhs._cores) && (_pin_threads == rhs._pin_threads) && (_mode == rhs._mode);
  }

  bool operator!=(const settings& rhs) const { return !(*this == rhs); }

private:
  cores _cores;
  bool _pin_threads;
  mode _mode;
};

void set(const settings as);

void reset();

settings get();

};
};

namespace mare {
namespace internal {

extern bool g_is_big_little_soc;

inline void
set_big_little_soc() {
  g_is_big_little_soc = true;
}

inline bool
is_big_little_soc() {
  return g_is_big_little_soc;
}

namespace affinity {
mare::affinity::settings get_non_local_affinity_settings();
};

namespace power {

bool is_this_big_core();

};
};
};

namespace mare {
namespace affinity {

template <typename Function, typename... Args>
void execute(mare::affinity::settings desired_aff, Function&& f, Args... args)
{
  if (mare::internal::is_big_little_soc()) {
    auto desired_cores = desired_aff.get_cores();
    auto non_local_settings = mare::internal::affinity::get_non_local_affinity_settings();

    if (non_local_settings.get_mode() == mare::affinity::mode::override_local_setting) {
      if (non_local_settings.get_cores() == mare::affinity::cores::all) {

        f(args...);
        return;
      } else {
        desired_cores = non_local_settings.get_cores();
      }
    } else {

    }

    auto is_caller_executing_on_big = mare::internal::power::is_this_big_core();

    auto diff_aff = (((desired_cores == mare::affinity::cores::big) && !is_caller_executing_on_big)
        || ((desired_cores == mare::affinity::cores::little) && is_caller_executing_on_big));
    if (diff_aff) {
      auto ck = mare::create_cpu_kernel(f);
      if (desired_cores == mare::affinity::cores::big) {
        ck.set_big();
      } else if (desired_cores == mare::affinity::cores::little) {
        ck.set_little();
      }
      auto t = mare::launch(ck, args...);
      t->wait_for();
      return;
    }
  }

  f(args...);
  return;
}

};

};
