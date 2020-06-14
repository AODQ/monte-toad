#pragma once

namespace mt { struct PluginInfo; }
namespace mt { struct RenderInfo; }

namespace ui {
  void InitializeLogger();

  bool Initialize(mt::RenderInfo & render, mt::PluginInfo & plugin);

  void Run(mt::RenderInfo & renderInfo, mt::PluginInfo & pluginInfo);
}