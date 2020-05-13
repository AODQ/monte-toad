#pragma once

namespace mt {
  enum class PluginType : size_t {
    Integrator
  , Kernel
  , Material
  , Camera
  , Random
  , UserInterface
  , Size
  , Invalid
  };

  enum class InputKey : size_t {
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
