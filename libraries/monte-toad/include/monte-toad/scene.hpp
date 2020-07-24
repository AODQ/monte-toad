#pragma once

#include <monte-toad/math.hpp>
#include <monte-toad/primitive.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <monte-toad/texture.hpp>

#include <any>
#include <filesystem>
#include "span.hpp"
#include <string>
#include <vector>

namespace mt { struct Triangle; }
namespace mt { struct PluginInfoRandom; }

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

    mt::AccelerationStructure accelStructure;
    EmissionSource emissionSource;

    glm::vec3 bboxMin, bboxMax;
  };

  mt::SurfaceInfo Raycast(
    Scene const & scene
  , glm::vec3 ori, glm::vec3 dir
  , Triangle const * ignoredTriangle
  );

  std::tuple<Triangle const *, glm::vec2> EmissionSourceTriangle(
    Scene const & scene
  , mt::PluginInfoRandom const & random
  );
}
