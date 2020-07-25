#pragma once

#include <string>

namespace mt::core { struct Texture; }

namespace mt {
  mt::core::Texture LoadTexture(std::string const & filename);
}
