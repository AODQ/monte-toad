#pragma once

#include <monte-toad/core/math.hpp>

#include <array>
#include <vector>

namespace mt::core {
  ////////////////////////////////////////////////////////////////////////////////
  struct Texture {
    uint64_t width, height;
    std::vector<glm::vec4> data;

    Texture() = default;

    static Texture Construct(
      size_t width, size_t height, size_t colorChannels
    , void const * data
    );

    bool Valid() const { return data.size() > 0; }
    size_t Idx(size_t x, size_t y) const { return y*this->width + x; }
  };

  /* struct CubemapTexture { */
  /*   std::array<Texture, 6> textures; */
  /*   CubemapTexture() = default; */

  /*   static CubemapTexture Construct(std::string const & baseFilename); */
  /* }; */

  // TODO namespace
  glm::vec4 Sample(Texture const & texture, glm::vec2 uvCoords);
  glm::vec4 SampleBilinear(Texture const & texture, glm::vec2 uvCoords);
  /* glm::vec4 Sample(CubemapTexture const & texture, glm::vec3 dir); */
  glm::vec4 Sample(Texture const & texture, glm::vec3 dir); // spherical
}
