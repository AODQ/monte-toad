#pragma once

#include <cstdint>
#include <cstddef>

namespace mt {
  enum struct PluginType : size_t {
    Integrator
  , Kernel
  , Material
  , Camera
  , Random
  , UserInterface
  , Emitter
  , Size
  , Invalid
  };

  enum struct InputKey : size_t {
    eA
  , eW
  , eS
  , eD
  , eQ
  , eE
  , eSpace
  , eShift
  , size
  };
}