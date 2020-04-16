#pragma once

#include "primitive.hpp"
#include "texture.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include "span.hpp"
#include <string>
#include <vector>

struct aiMaterial;

struct Scene;

struct Material {
  Material() = default;
  std::string name = "";
  glm::vec3
    colorDiffuse     = glm::vec3(0.0f)
  , colorSpecular    = glm::vec3(0.0f)
  , colorAmbient     = glm::vec3(0.0f)
  , colorEmissive    = glm::vec3(0.0f)
  , colorTransparent = glm::vec3(0.0f);

  bool emissive = false;

  float opacity = 1.0f;

  float shininess = 0.0f;
  float shininessStrength = 1.0f;

  float indexOfRefraction = 1.0f;

  size_t
    baseColorTextureIdx        = static_cast<size_t>(-1)
  , normalTextureIdx           = static_cast<size_t>(-1)
  , emissionTextureIdx         = static_cast<size_t>(-1)
  , metallicTextureIdx         = static_cast<size_t>(-1)
  , diffuseRoughnessTextureIdx = static_cast<size_t>(-1)
  ;

  float textureMultiplier = 1.0f;

  // TODO uv mapping mode

  bool noCulling = false;

  static Material Construct(aiMaterial const &, Scene & scene);
};

struct Mesh {
  Mesh() = default;

  Material material;
};

struct Scene {
  Scene() = default;

  static Scene Construct(
    std::string const & filename
  , std::string const & environmentMapFilename = ""
  , bool upAxisZ = false
  , bool optimizeBvh = false
  );

  std::filesystem::path basePath;

  std::vector<Texture> textures;
  std::vector<Mesh> meshes;

  Texture environmentTexture;

  std::unique_ptr<AccelerationStructure> accelStructure;

  glm::vec3 bboxMin, bboxMax;
};

std::pair<Triangle const *, Intersection> Raycast(
  Scene const & scene
, glm::vec3 ori, glm::vec3 dir
, bool useBvh
);
