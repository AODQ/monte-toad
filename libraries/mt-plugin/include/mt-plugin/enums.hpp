#pragma once

#include <cstdint>
#include <cstddef>

namespace mt {
  enum struct PluginType : size_t {
    Integrator
  , Kernel
  , AccelerationStructure
  , Bsdf
  , Material
  , Camera
  , Random
  , UserInterface
  , Emitter
  , Dispatcher
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
