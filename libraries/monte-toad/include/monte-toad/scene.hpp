#pragma once

#include <monte-toad/core/accelerationstructure.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/texture.hpp>

#include <any>
#include <filesystem>
#include "span.hpp"
#include <string>
#include <vector>

namespace mt::core { struct Triangle; }
namespace mt { struct PluginInfoRandom; }
namespace mt { struct SurfaceInfo; }

namespace mt {
  struct Mesh {
    size_t idx;
  };

  struct EmissionSource {
    std::vector<size_t> triangles;

    mt::Texture environmentMap;

    size_t skyboxEmitterPluginIdx = -1lu;
  };

  struct Scene {
    static void Construct(
      Scene & self
    , std::string const & filename
    , std::string const & environmentMapFilename = ""
    );

    std::filesystem::path basePath;

    std::vector<Mesh> meshes;

    std::any environmentData;

    mt::core::AccelerationStructure accelStructure;
    EmissionSource emissionSource;

    glm::vec3 bboxMin, bboxMax;
  };

  mt::SurfaceInfo Raycast(
    Scene const & scene
  , glm::vec3 ori, glm::vec3 dir
  , mt::core::Triangle const * ignoredTriangle
  );

  std::tuple<mt::core::Triangle const *, glm::vec2> EmissionSourceTriangle(
    Scene const & scene
  , mt::PluginInfoRandom const & random
  );
}
