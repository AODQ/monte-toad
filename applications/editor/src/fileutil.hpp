#pragma once

#include <mt-plugin/enums.hpp>

#include <string>

namespace mt::core { struct RenderInfo; }
namespace mt { struct PluginInfo; }

namespace fileutil {
  void LoadEditorConfig(
    mt::core::RenderInfo & render
  , mt::PluginInfo & plugin
  );

  void SaveEditorConfig(
    mt::core::RenderInfo const & render
  , mt::PluginInfo const & plugin
  );

  // Loads plugin at file location returning true on success
  bool LoadPlugin(
    mt::PluginInfo & plugin
  , mt::core::RenderInfo & render
  , std::string const & file
  , mt::PluginType type
  );
}
