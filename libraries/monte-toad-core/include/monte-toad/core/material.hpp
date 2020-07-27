#pragma once

#include <monte-toad/core/any.hpp>

#include <vector>

namespace mt::core { struct BsdfSampleInfo; }
namespace mt::core { struct Scene; }
namespace mt::core { struct SurfaceInfo; }
namespace mt { struct PluginInfo; }

namespace mt::core {
  struct MaterialComponent {
    float probability;

    size_t pluginIdx;
    mt::core::Any userdata;
  };

  struct Material {
    std::vector<MaterialComponent> reflective, refractive;
    float indexOfRefraction;
  };

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

  glm::vec3 MaterialFs(
    mt::core::SurfaceInfo const & surface
  , mt::core::Scene const & scene
  , mt::PluginInfo const & plugin
  , glm::vec3 const & wo
  );
}
