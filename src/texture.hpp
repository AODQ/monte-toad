#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct Texture {
  uint64_t width, height;
  std::vector<glm::vec4> data;
  std::string filename;

  Texture() = default;

  static Texture Construct(std::string const & filename);
  static Texture Construct(int width, int height, void * data);
};

glm::vec4 Sample(Texture const & texture, glm::vec2 uvCoords);
