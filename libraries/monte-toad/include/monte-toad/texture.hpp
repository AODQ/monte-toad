#pragma once

#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

namespace mt {
  ////////////////////////////////////////////////////////////////////////////////
  struct Texture {
    uint64_t width, height;
    std::vector<glm::vec4> data;
    std::string filename;

    Texture() = default;

    static Texture Construct(std::string const & filename);
    static Texture Construct(int width, int height, void * data);

    bool Valid() const { return data.size() > 0; }
    size_t Idx(size_t x, size_t y) const { return y*this->width + x; }
  };

  struct CubemapTexture {
    std::array<Texture, 6> textures;
    CubemapTexture() = default;

    static CubemapTexture Construct(std::string const & baseFilename);
  };

  // TODO namespace
  glm::vec4 Sample(Texture const & texture, glm::vec2 uvCoords);
  glm::vec4 SampleBilinear(Texture const & texture, glm::vec2 uvCoords);
  glm::vec4 Sample(CubemapTexture const & texture, glm::vec3 dir);
  glm::vec4 Sample(Texture const & texture, glm::vec3 dir); // spherical
}
