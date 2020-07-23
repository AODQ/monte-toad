#pragma once

#include <monte-toad/math.hpp>

#include <string>

// -- fwd decl
template <typename ElementType> struct span;

namespace mt {
  void SaveImage(
    span<glm::vec4> data
  , size_t width, size_t height
  , std::string const & filename
  , bool displayProgress
  );
}
