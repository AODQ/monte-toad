#pragma once

#include <string>

#include <mt-plugin/enums.hpp>

namespace mt { struct PluginInfo; }

namespace mt {
  uint32_t LoadPlugin(
    mt::PluginInfo & pluginInfo
  , mt::PluginType pluginType, std::string const & filename
  );

  void FreePlugins();

  // checks if plugins need to be reloaded
  void UpdatePlugins();

  // checks that plugin of type is valid
  bool Valid(mt::PluginInfo const & pluginInfo, mt::PluginType pluginType);

  // cleans plugin so that it can be recognized as invalid. Also frees userdata
  // of the plugin if(f) the plugin is valid
  void Clean(mt::PluginInfo & pluginInfo, mt::PluginType pluginType);
}

char const * ToString(mt::PluginType pluginType);
