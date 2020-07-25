#include <monte-toad/textureloader.hpp>

#include <monte-toad/core/texture.hpp>

#include <spdlog/spdlog.h>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdouble-promotion"
  #define STB_IMAGE_IMPLEMENTATION
  #include <stb_image.hpp>
#pragma GCC diagnostic pop

#include <filesystem>

mt::core::Texture mt::LoadTexture(std::string const & filename) {
  stbi_set_flip_vertically_on_load(false);

  int width, height, channels;
  uint8_t * rawByteData =
    stbi_load(
      filename.c_str(),
      &width, &height, &channels,
      STBI_rgb_alpha
    );

  auto texture =
    mt::core::Texture::Construct(width, height, channels, rawByteData);

  stbi_image_free(rawByteData);

  return texture;
}

/* //////////////////////////////////////////////////////////////////////////////// */
/* mt::CubemapTexture mt::CubemapTexture::Construct( */
/*   std::string const & baseFilename */
/* ) { */
/*   auto basePath = std::filesystem::path{baseFilename}; */
/*   auto baseExtension = basePath.extension(); */

/*   mt::CubemapTexture texture; */

/*   // remove extension to load files with -0.ext, ..., -5.ext */
/*   basePath.replace_extension(""); // */
/*   for (size_t i = 0; i < 6; ++ i) { */
/*     texture.textures[i] = */
/*       mt::Texture::Construct( */
/*         (basePath / "-" / std::to_string(i) / baseExtension).string() */
/*       ); */
/*   } */

/*   return texture; */
/* } */

