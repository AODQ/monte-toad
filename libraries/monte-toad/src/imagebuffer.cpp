#include <monte-toad/imagebuffer.hpp>

#include <monte-toad/log.hpp>
#include <monte-toad/span.hpp>

#include <fstream>

////////////////////////////////////////////////////////////////////////////////
void mt::SaveImage(
  span<glm::vec4> data
, size_t width, size_t height
, std::string const & filename
, bool displayProgress
) {
  auto file = std::ofstream{filename, std::ios::binary};
  spdlog::info("Saving image {}", filename);
  file << "P6\n" << width << " " << height << "\n255\n";
  for (size_t i = 0u; i < width*height; ++ i) {
    // flip Y
    glm::vec4 pixel = data[i];
    pixel = 255.0f * glm::clamp(pixel, glm::vec4(0.0f), glm::vec4(1.0f));
    file
      << static_cast<uint8_t>(pixel.x)
      << static_cast<uint8_t>(pixel.y)
      << static_cast<uint8_t>(pixel.z)
    ;

    if (displayProgress && i%width == 0)
      PrintProgress(i/static_cast<float>(width*height));
  }

  if (displayProgress) {
    PrintProgress(1.0f);
    printf("\n"); // new line for progress bar
  }

  spdlog::info("Finished saving image {}", filename);
}
