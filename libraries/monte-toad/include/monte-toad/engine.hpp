#pragma once

#include "span.hpp"

#include <glm/glm.hpp>

#include <stddef.h>

namespace mt { struct Scene; }
namespace mt { struct PluginInfo; }
namespace mt { struct RenderInfo; }

namespace mt {
  // Dispatches engine over the block region specified by [min, max).
  void DispatchEngineBlockRegion(
    mt::Scene const & scene
  , span<glm::vec4> imageBuffer
  , mt::RenderInfo & renderInfo
  , mt::PluginInfo & pluginInfo
  , size_t const min, size_t const max
  , bool headless
  );
}
