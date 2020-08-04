#pragma once

#include <array>
#include <string>
#include <vector>

namespace mt::core { struct Scene; }

namespace mt::core {
  //////////////////////////////////////////////////////////////////////////////
  struct Texture {
    uint64_t width, height;
    std::vector<glm::vec4> data;
    std::string label;

    Texture() = default;

    static Texture Construct(
      size_t width, size_t height, size_t colorChannels
    , void const * data
    );

    bool Valid() const { return data.size() > 0; }
    size_t Idx(size_t x, size_t y) const { return y*this->width + x; }
  };

  // TODO BELOW should probably go into a different header
  template <typename T> struct TextureOption {
    bool GuiApply(mt::core::Scene const & scene);
    T Get(glm::vec2 const & uv) const;

    std::string label = "N/A";
    float minRange = 0.0f, maxRange = 1.0f;
    T userValue { 0.0f };
    mt::core::Texture const * userTexture = nullptr;
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
