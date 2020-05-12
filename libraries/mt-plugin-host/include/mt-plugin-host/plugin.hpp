#pragma once

#include <string>

namespace mt { struct PluginInfo; }

namespace mt {
  enum class PluginType : size_t {
    Integrator
  , Kernel
  , Material
  , Camera
  , Random
  , UserInterface
  , Size
  };

  uint32_t LoadPlugin(
    mt::PluginInfo & pluginInfo
  , mt::PluginType pluginType, std::string const & filename
  );

  void FreePlugins();

  // checks if plugins need to be reloaded
  void UpdatePlugins();

  bool Valid(mt::PluginInfo & pluginInfo, mt::PluginType pluginType);
}

char const * ToString(mt::PluginType pluginType);
