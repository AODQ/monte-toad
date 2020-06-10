#pragma once

#include <monte-toad/enum.hpp>
#include <monte-toad/span.hpp>

#include <glm/glm.hpp>

#include <stddef.h>

namespace mt { struct Scene; }
namespace mt { struct PluginInfo; }
namespace mt { struct RenderInfo; }

namespace mt {
  // Dispatches engine over the block region specified by [min, max).
  void DispatchEngineBlockRegion(
    mt::Scene const & scene
  , mt::RenderInfo & render
  , mt::PluginInfo const & pluginInfo

  , size_t integratorIdx
  , size_t const minX, size_t const minY
  , size_t const maxX, size_t const maxY
  );
}
