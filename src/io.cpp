#include "io.hpp"

#include "log.hpp"

#include <fstream>

////////////////////////////////////////////////////////////////////////////////
Buffer Buffer::Construct(size_t width, size_t height) {
  Buffer self;
  self.width = width; self.height = height;
  self.data.resize(width*height);
  return self;
}

////////////////////////////////////////////////////////////////////////////////
void PrintProgress(float progress) {
  printf("[");
  for (int i = 0; i < 40; ++ i) {
    if (i <  static_cast<int>(40.0f*progress)) printf("=");
    if (i == static_cast<int>(40.0f*progress)) printf(">");
    if (i >  static_cast<int>(40.0f*progress)) printf(" ");
  }
  // leading spaces in case of terminal/text corruption
  printf("] %0.1f%%   \r", progress*100.0f);
  fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
void SaveImage(
  Buffer const & data
, std::string const & filename
, bool displayProgress
) {
  auto file = std::ofstream{filename, std::ios::binary};
  spdlog::info("Saving image {}", filename);
  file << "P6\n" << data.Width() << " " << data.Height() << "\n255\n";
  for (size_t i = 0u; i < data.Height()*data.Width(); ++ i) {
    // flip Y
    glm::vec3 pixel = data.At(i%data.Width(), data.Height()-i/data.Width()-1);
    pixel = 255.0f * glm::clamp(pixel, glm::vec3(0.0f), glm::vec3(1.0f));
    file
      << static_cast<uint8_t>(pixel.x)
      << static_cast<uint8_t>(pixel.y)
      << static_cast<uint8_t>(pixel.z)
    ;

    if (displayProgress && i%data.Width() == 0)
      PrintProgress(i/static_cast<float>(data.Height()*data.Width()));
  }

  if (displayProgress) {
    PrintProgress(1.0f);
    printf("\n"); // new line for progress bar
  }

  spdlog::info("Finished saving image {}", filename);
}
