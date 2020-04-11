#pragma once

#include <glm/glm.hpp>

struct Scene;
struct Camera;

glm::vec3 Render(
  glm::vec2 const uv
, Scene const & scene
, Camera const & camera
, uint32_t samplesPerPixel
, bool useBvh
);
