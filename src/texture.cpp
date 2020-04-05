#include "texture.hpp"

#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../ext/stb_image.hpp"

////////////////////////////////////////////////////////////////////////////////
Texture Texture::Construct(std::string const & filename) {
  stbi_set_flip_vertically_on_load(false);

  spdlog::info("Loading texture {}", filename);

  Texture texture;
  texture.filename = filename;

  { // load texture
    int width, height, channels;
    uint8_t * rawByteData =
      stbi_load(
        filename.c_str(),
        &width, &height, &channels,
        STBI_rgb_alpha
      );
    texture.width = width;
    texture.height = height;

    if (!rawByteData) {
      spdlog::error("Could not load texture {}", filename);
    }

    spdlog::info("channels {}", channels);

    // -- copy stbi texture into our own texture format for fast/simple access
    texture.data.resize(texture.width*texture.height);
    for (size_t i = 0; i < texture.data.size(); ++ i) {
      size_t x = i%texture.width, y = i/texture.width;

      // Since we ask sbti to load in STBI_rgb_alpha format, the channel is set
      // as 4 even if the image itself does not support 4
      uint8_t const * byte =
        rawByteData + y*4 + x*texture.height*4;

      // Load all channels for the color, note that in cases of textures that
      // only support red channel, rgb channel, etc. we want special behavior
      // to 1.0f (which works nicely for alpha)
      for (int j = 0; j < 4; ++ j) {
        // compare zero-indexed channels to iterator to see if its channel has
        // to be set a default value
        if (channels-1 < j) {
          // only default alpha to 1.0f, that way RGB still works, and with R we
          // get only the red color
          texture.data[i][j] = j == 3 ? 1.0f : 1.0f;
          continue;
        }
        texture.data[i][j] = *(byte + j)/255.0f;
      }
    }

    // we've copied this data into vector so it's no longer necessary to keep it
    // around
    stbi_image_free(rawByteData);
  }

  spdlog::info(
    "Resize texture {}x{} components to {}",
    texture.width, texture.height
  , texture.data.size()
  );

  return texture;
}

////////////////////////////////////////////////////////////////////////////////
Texture Texture::Construct(int width, int height, void * data) {
  return Texture{};
}

////////////////////////////////////////////////////////////////////////////////
glm::vec4 Sample(Texture const & texture, glm::vec2 uvCoords) {
  uvCoords = glm::mod(uvCoords, glm::vec2(1.0f));
  /* uvCoords = glm::clamp(uvCoords, glm::vec2(0.0f), glm::vec2(1.0f)); */
  uvCoords.y = 1.0f - uvCoords.y;
  uint64_t
    x =
      glm::clamp(
        static_cast<uint64_t>(uvCoords.x*texture.width), 0ul, texture.width-1
      )
  , y =
      glm::clamp(
        static_cast<uint64_t>(uvCoords.y*texture.height), 0ul, texture.height-1
      )
  ;

  uint64_t idx = y + x*texture.height;
  return texture.data[idx];
}
