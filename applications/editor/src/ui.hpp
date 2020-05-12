#pragma once

namespace mt { struct PluginInfo; }
namespace mt { struct RenderInfo; }

namespace ui {
  bool Initialize(mt::RenderInfo const & renderInfo);

  void Run(mt::RenderInfo & renderInfo , mt::PluginInfo & pluginInfo);
}
