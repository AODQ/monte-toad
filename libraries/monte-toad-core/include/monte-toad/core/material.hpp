#pragma once

#include <monte-toad/core/any.hpp>

#include <vector>

namespace mt::core { struct BsdfSampleInfo; }
namespace mt::core { struct Scene; }
namespace mt::core { struct SurfaceInfo; }
namespace mt { struct PluginInfo; }

namespace mt::core {
  struct MaterialComponent {
    float probability = 0.0f;

    size_t pluginIdx = -1lu;
    mt::core::Any userdata;
  };

  struct Material {
    MaterialComponent emitter;
    std::vector<MaterialComponent> reflective, refractive;
    float indexOfRefraction;
  };

  bool MaterialIsEmitter(
    mt::core::SurfaceInfo const & surface
  , mt::core::Scene const & scene
  , mt::PluginInfo const & plugin
  );

  mt::core::BsdfSampleInfo MaterialSample(
    mt::core::SurfaceInfo const & surface
  , mt::core::Scene const & scene
  , mt::PluginInfo const & plugin
  );

  float MaterialPdf(
    mt::core::SurfaceInfo const & surface
  , mt::core::Scene const & scene
  , mt::PluginInfo const & plugin
  , glm::vec3 const & wo
  , bool const reflection
  , size_t const componentIdx
  );

  float MaterialIndirectPdf(
    mt::core::SurfaceInfo const & surface
  , mt::core::Scene const & scene
  , mt::PluginInfo const & plugin
  , glm::vec3 const & wo
  );

  glm::vec3 MaterialEmitterFs(
    mt::core::SurfaceInfo const & surface
  , mt::core::Scene const & scene
  , mt::PluginInfo const & plugin
  );

  glm::vec3 MaterialFs(
    mt::core::SurfaceInfo const & surface
  , mt::core::Scene const & scene
  , mt::PluginInfo const & plugin
  , glm::vec3 const & wo
  );
}
