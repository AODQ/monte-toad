#pragma once

#include <monte-toad/core/any.hpp>
#include <monte-toad/core/span.hpp>
#include <monte-toad/core/texture.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace mt::core { struct SurfaceInfo; }
namespace mt::core { struct Texture; }
namespace mt::core { struct Triangle; }
namespace mt { struct PluginInfo; }

namespace mt::core {
  struct Mesh {
    mt::core::Any material;
    size_t idx;
  };

  struct EmissionSource {
    std::vector<size_t> triangles;

    mt::core::Texture environmentMap;

    size_t skyboxEmitterPluginIdx = -1lu;
  };

  struct Scene {
    static void Construct(
      Scene & self
    , mt::PluginInfo const & plugin
    , std::string const & filename
    );

    std::filesystem::path basePath;

    std::vector<mt::core::Mesh> meshes;

    std::vector<mt::core::Texture> textures;

    mt::core::Any accelStructure;
    EmissionSource emissionSource;

    glm::vec3 bboxMin, bboxMax;
  };

  mt::core::SurfaceInfo Raycast(
    Scene const & scene
  , mt::PluginInfo const & plugin
  , glm::vec3 ori, glm::vec3 dir
  , size_t const ignoredTriangle
  );

  std::tuple<mt::core::Triangle, glm::vec2> EmissionSourceTriangle(
    Scene const & scene
  , mt::PluginInfo const & plugin
  );
}
