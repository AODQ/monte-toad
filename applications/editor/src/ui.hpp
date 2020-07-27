#pragma once

namespace mt { struct PluginInfo; }
namespace mt::core { struct RenderInfo; }

namespace ui {
  void InitializeLogger();

  bool Initialize(mt::core::RenderInfo & render, mt::PluginInfo & plugin);

  void Run(mt::core::RenderInfo & render, mt::PluginInfo & plugin);
}
