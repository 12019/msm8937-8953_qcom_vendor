// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once

#include <initializer_list>
#include <string>

#include <mare/internal/util/macros.hh>

namespace mare {

enum device_type {
  cpu,
  gpu,
  hexagon
};

std::string to_string(device_type d);

class device_set {
  bool _on_cpu;
  bool _on_gpu;
  bool _on_hexagon;
public:

  device_set() :
    _on_cpu(false),
    _on_gpu(false),
    _on_hexagon(false)
  {}

  device_set(std::initializer_list<device_type> device_list) :
    _on_cpu(false),
    _on_gpu(false),
    _on_hexagon(false)
  {
    for(auto d : device_list)
      add(d);
  }

  bool on_cpu() const { return _on_cpu; }

  bool on_gpu() const { return _on_gpu; }

  bool on_hexagon() const { return _on_hexagon; }

  device_set& add(device_type d) {
    switch(d) {
      default:
      case cpu: _on_cpu = true; break;
      case gpu: _on_gpu = true; break;
      case hexagon: _on_hexagon = true; break;
    }
    return *this;
  }

  device_set& add(device_set const& other) {
    if(other.on_cpu()) _on_cpu = true;
    if(other.on_gpu()) _on_gpu = true;
    if(other.on_hexagon()) _on_hexagon = true;
    return *this;
  }

  device_set& remove(device_type d) {
    switch(d) {
      default:
      case cpu: _on_cpu = false; break;
      case gpu: _on_gpu = false; break;
      case hexagon: _on_hexagon = false; break;
    }
    return *this;
  }

  device_set& remove(device_set const& other) {
    if(other.on_cpu()) _on_cpu = false;
    if(other.on_gpu()) _on_gpu = false;
    if(other.on_hexagon()) _on_hexagon = false;
    return *this;
  }

  device_set& negate() {
    _on_cpu = !_on_cpu;
    _on_gpu = !_on_gpu;
    _on_hexagon = !_on_hexagon;
    return *this;
  }

  std::string to_string() const;

  MARE_DEFAULT_METHOD(device_set(device_set const&));

  MARE_DEFAULT_METHOD(device_set& operator=(device_set const&));

  MARE_DEFAULT_METHOD(device_set(device_set&&));

  MARE_DEFAULT_METHOD(device_set& operator=(device_set&&));
};

};
