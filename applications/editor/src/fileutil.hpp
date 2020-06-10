#pragma once

#include <mt-plugin/enums.hpp>

#include <string>

namespace mt { struct PluginInfo; }
namespace mt { struct RenderInfo; }

namespace fileutil {
  void LoadEditorConfig(mt::RenderInfo & render, mt::PluginInfo & plugin);

  void SaveEditorConfig(
    mt::RenderInfo const & render
  , mt::PluginInfo const & plugin
  );

  // Loads plugin at file location returning true on success
  bool LoadPlugin(
    mt::PluginInfo & plugin
  , mt::RenderInfo & render
  , std::string const & file
  , mt::PluginType type
  );

  std::string FilePicker(std::string const & flags);
}
