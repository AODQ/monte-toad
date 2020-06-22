#pragma once

#include <monte-toad/primitive.hpp>
#include <monte-toad/surfaceinfo.hpp>
#include <monte-toad/texture.hpp>

#include <glm/glm.hpp>

#include <any>
#include <filesystem>
#include "span.hpp"
#include <string>
#include <vector>
#include <variant>

namespace mt { struct Triangle; }
namespace mt { struct PluginInfoRandom; }

namespace mt {
  struct Scene;

  struct Mesh {
    size_t idx;
  };

  struct EmissionSource {
    std::vector<size_t> triangles;

    mt::Texture environmentMap;

    int32_t skyboxEmitterPluginIdx = -1;
  };

  struct Scene {
    Scene() = default;

    static Scene Construct(
      std::string const & filename
    , std::string const & environmentMapFilename = ""
    );

    std::filesystem::path basePath;

    std::vector<Mesh> meshes;

    std::any environmentData;

    std::unique_ptr<mt::AccelerationStructure> accelStructure;
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
