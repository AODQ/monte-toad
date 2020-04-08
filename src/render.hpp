#pragma once

#include <glm/glm.hpp>

struct Scene;
struct Camera;

struct RenderResults {
  RenderResults() = default;

  glm::vec3 color;
  bool valid;
};

RenderResults Render(
  glm::vec2 const uv
, Scene const & scene
, Camera const & camera
, bool useBvh
);
