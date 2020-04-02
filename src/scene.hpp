#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct Triangle {
  Triangle(glm::vec3 a_, glm::vec3 b_, glm::vec3 c_) : v0(a_), v1(b_), v2(c_) {}
  ~Triangle() {}
  glm::vec3 v0, v1, v2;
};

struct Scene {
  Scene() = default;

  static Scene Construct(std::string const & filename);

  std::vector<Triangle> scene;

  glm::vec3 bboxMin, bboxMax;
};

Triangle const * Raycast(
  Scene const & model
, glm::vec3 ori
, glm::vec3 dir
, float & distance
, glm::vec2 & uv
);
