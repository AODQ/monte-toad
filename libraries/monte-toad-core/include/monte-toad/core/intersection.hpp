#pragma once

#include <monte-toad/core/math.hpp>

#include <cstdint>

namespace mt::core {
  struct BvhIntersection {
    size_t triangleIdx;
    float length;
    glm::vec2 barycentricUv;

    float distance() const { return length; }
  };
}
