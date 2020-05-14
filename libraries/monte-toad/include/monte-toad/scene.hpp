#pragma once

#include "primitive.hpp"
#include "texture.hpp"

#include <glm/glm.hpp>

#include <any>
#include <filesystem>
#include "span.hpp"
#include <string>
#include <vector>
#include <variant>

namespace mt {
  struct Scene;

  struct Mesh {
    Mesh() = default;
    std::any userdata;
  };

  struct EmissionSource {
    std::vector<size_t> triangles;
  };

  struct Scene {
    Scene() = default;

    static Scene Construct(
      std::string const & filename
    , std::string const & environmentMapFilename = ""
    , bool optimizeBvh = false
    // TODO useBvh
    );

    std::filesystem::path basePath;

    std::vector<Texture> textures;
    std::vector<Mesh> meshes;

    Texture environmentTexture;

    std::unique_ptr<AccelerationStructure> accelStructure;
    EmissionSource emissionSource;

    glm::vec3 bboxMin, bboxMax;
  };

  std::tuple<Triangle const *, BvhIntersection> Raycast(
    Scene const & scene
  , glm::vec3 ori, glm::vec3 dir
  , bool useBvh
  , Triangle const * ignoredTriangle
  );

  std::tuple<Triangle const *, glm::vec2> EmissionSourceTriangle(
    Scene const & scene
  /* , GenericNoiseGenerator & noise */
  , size_t idx
  );
}
