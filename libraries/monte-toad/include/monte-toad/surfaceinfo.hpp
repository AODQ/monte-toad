#pragma once

#include <glm/glm.hpp>

namespace mt { struct Triangle; }

namespace mt {
  struct SurfaceInfo {
    mt::Triangle const * triangle;
    float distance;
    glm::vec2 barycentricUv;

    glm::vec3 normal;
  };
}
