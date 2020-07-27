#pragma once

#include <string>

namespace mt::core { struct Texture; }

namespace mt::util {
  mt::core::Texture LoadTexture(std::string const & filename);
}
