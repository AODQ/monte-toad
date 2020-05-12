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
}
