// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <chrono>
#include <mare/devicetypes.hh>
#include <mare/internal/util/debug.hh>

namespace mare {

namespace power {

bool is_supported();

namespace mode {

class efficient_t {};

const efficient_t efficient {};

class normal_t {};

const normal_t normal {};

class perf_burst_t {};

const perf_burst_t perf_burst {};

class saver_t {};

const saver_t saver {};

class window {
  public :
    explicit window(size_t min = 0, size_t max = 100): _min(min), _max(max)
    {
      if(( min > 100) || (max > 100) || (min > max)) {
        MARE_FATAL("Window power mode requires values between 0 and 100 and"
            " a minimum percent lower than the maximum");
      }
    };

    explicit window(window const& other) : _min(other._min), _max(other._max) {}

    explicit window(window&& other) : _min(other._min)
                           , _max(other._max)  {}

    window& operator= (window const& other)
    {
      this->_min = other._min;
      this->_max = other._max;
      return *this;
    }

    MARE_DELETE_METHOD(window& operator= (window&& other));

    size_t get_min() const { return _min; }

    size_t get_max() const { return _max; }
  private :

    size_t _min;
    size_t _max;
};

};

void request_mode(mode::window const& win,
                  std::chrono::milliseconds const& duration = std::chrono::milliseconds(0),
                  device_set const& devices = {mare::device_type::cpu});

void request_mode(mode::efficient_t const&,
                  std::chrono::milliseconds const& duration = std::chrono::milliseconds(0),
                  device_set const& devices = {mare::device_type::cpu});

void request_mode(mode::normal_t const&, device_set const& devices = {mare::device_type::cpu});

void request_mode(mode::saver_t const&,
                  std::chrono::milliseconds const& duration = std::chrono::milliseconds(0),
                  device_set const& devices = {mare::device_type::cpu});

void request_mode(mode::perf_burst_t const&,
                  std::chrono::milliseconds const& duration = std::chrono::milliseconds(0),
                  device_set const& devices = {mare::device_type::cpu});

void set_goal(const float desired, const float tolerance = 0.0, const device_set& devices = {mare::device_type::cpu});

void clear_goal();

void regulate(const float measured);

};

namespace beta {
namespace power {

class tuner {
public:
  mare::device_set devices;
};

void set_goal(const float, const float, const tuner&);
void clear_goal();
void regulate(const float);

};
};

};
