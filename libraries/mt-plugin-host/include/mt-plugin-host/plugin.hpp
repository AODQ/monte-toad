#pragma once

#include <string>

#include <mt-plugin/enums.hpp>

namespace mt { struct PluginInfo; }

namespace mt {
  // loads plugin of specific type from the filename into pluginInfo. In cases
  // where the type is a vector, the memory must be allocated beforehand and
  // its index passed in
  bool LoadPlugin(
    mt::PluginInfo & plugin
  , mt::PluginType pluginType, std::string const & filename
  , size_t idx = 0
  );

  void FreePlugin(size_t idx);

  void FreePlugins();

  // checks if plugins need to be reloaded
  void UpdatePlugins(mt::PluginInfo & plugin);

  // checks that plugin of type is valid
  bool Valid(
    mt::PluginInfo const & plugin
  , mt::PluginType pluginType
  , size_t idx = 0
  );

  // cleans plugin so that it can be recognized as invalid. Also frees userdata
  // of the plugin if(f) the plugin is valid
  void Clean(
    mt::PluginInfo & plugin
  , mt::PluginType pluginType
  , size_t idx
  );
}

char const * ToString(mt::PluginType pluginType);
