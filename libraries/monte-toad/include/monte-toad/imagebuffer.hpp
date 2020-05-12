#pragma once

#include "math.hpp"
#include "span.hpp"

#include <string>

namespace mt {
  void SaveImage(
    span<glm::vec4> data
  , size_t width, size_t height
  , std::string const & filename
  , bool displayProgress
  );
}
