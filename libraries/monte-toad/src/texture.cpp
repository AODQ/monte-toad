#include <monte-toad/texture.hpp>

#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.hpp>

#include <filesystem>

////////////////////////////////////////////////////////////////////////////////
mt::Texture mt::Texture::Construct(std::string const & filename) {
  stbi_set_flip_vertically_on_load(false);

  spdlog::debug("Loading texture {}", filename);

  mt::Texture texture;
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
      texture.width = 0;
      texture.height = 0;
      texture.data = {};
      return texture;
    }

    // -- copy stbi texture into our own texture format for fast/simple access
    texture.data.resize(texture.width*texture.height);
    for (size_t i = 0; i < texture.data.size(); ++ i) {
      size_t x = i%texture.width, y = i/texture.width;

      // Since we ask sbti to load in STBI_rgb_alpha format, the channel is set
      // as 4 even if the image itself does not support 4
      uint8_t const * byte = rawByteData + texture.Idx(x, y)*4;

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

  spdlog::debug("\ttexture size {}x{}", texture.width, texture.height);

  return texture;
}

////////////////////////////////////////////////////////////////////////////////
mt::Texture mt::Texture::Construct(int width, int height, void * data) {
  return mt::Texture{};
}

////////////////////////////////////////////////////////////////////////////////
mt::CubemapTexture mt::CubemapTexture::Construct(
  std::string const & baseFilename
) {
  auto basePath = std::filesystem::path{baseFilename};
  auto baseExtension = basePath.extension();

  mt::CubemapTexture texture;

  // remove extension to load files with -0.ext, ..., -5.ext
  basePath.replace_extension(""); //
  for (size_t i = 0; i < 6; ++ i) {
    texture.textures[i] =
      mt::Texture::Construct(
        (basePath / "-" / std::to_string(i) / baseExtension).string()
      );
  }

  return texture;
}

////////////////////////////////////////////////////////////////////////////////
glm::vec4 mt::Sample(mt::Texture const & texture, glm::vec2 uvCoords) {
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

  return texture.data[texture.Idx(x, y)];
}

////////////////////////////////////////////////////////////////////////////////
glm::vec4 mt::SampleBilinear(mt::Texture const & texture, glm::vec2 uvCoords) {
  glm::vec2 res = glm::vec2(texture.width, texture.height);

  glm::vec2 st = uvCoords*res - glm::vec2(0.5f);

  glm::vec2 iuv = glm::floor(st);
  glm::vec2 fuv = glm::fract(st);

  glm::vec4
    a = Sample(texture, (iuv + glm::vec2(0.5f,0.5f))/res)
  , b = Sample(texture, (iuv + glm::vec2(1.5f,0.5f))/res)
  , c = Sample(texture, (iuv + glm::vec2(0.5f,1.5f))/res)
  , d = Sample(texture, (iuv + glm::vec2(1.5f,1.5f))/res)
  ;

  return
    mix(
      mix(a, b, fuv.x)
    , mix( c, d, fuv.x)
    , fuv.y
    );
}

////////////////////////////////////////////////////////////////////////////////
glm::vec4 mt::Sample(mt::CubemapTexture const & texture, glm::vec3 dir) {
  // TODO
  return glm::vec4(1.0f);
}

////////////////////////////////////////////////////////////////////////////////
glm::vec4 mt::Sample(mt::Texture const & texture, glm::vec3 dir) {
  return
    Sample(
      texture,
      glm::vec2(
          0.5f + (glm::atan(dir.x, dir.z) / 6.283185307f)
        , 0.5f - (glm::asin(-dir.y)/3.14159f)
      )
    );
}
