#pragma once

#include "noise.hpp"

#include <glm/glm.hpp>

struct Scene;
struct Camera;

glm::vec3 Render(
  glm::vec2 const uv
, glm::vec2 const dimensions
, GenericNoiseGenerator & noiseGenerator
, Scene const & scene
, Camera const & camera
, uint32_t samplesPerPixel
, uint32_t pathsPerSample
, bool useBvh
);
