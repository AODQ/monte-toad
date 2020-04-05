#pragma once

#include "primitive.hpp"
#include "texture.hpp"

#include "../ext/bvh.hpp"
#include <glm/glm.hpp>

#include "span.hpp"
#include <string>
#include <vector>

struct Mesh {
  Mesh() = default;
  size_t diffuseTextureIdx = static_cast<size_t>(-1);
};

struct Scene {
  Scene() = default;

  static Scene Construct(
    std::string const & filename
  , std::string const & environmentMapFilename = ""
  , bool upAxisZ = false
  , bool optimizeBvh = false
  );

  std::vector<Texture> textures;
  std::vector<Mesh> meshes;

  Texture environmentTexture;

  AccelerationStructure accelStructure;

  glm::vec3 bboxMin, bboxMax;
};

Triangle const * Raycast(
  Scene const & scene
, glm::vec3 ori, glm::vec3 dir
, Intersection & intersection
, bool useBvh
);
