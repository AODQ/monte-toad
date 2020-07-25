#pragma once

#include <monte-toad/core/enum.hpp>
#include <monte-toad/core/math.hpp>
#include <monte-toad/core/surfaceinfo.hpp>

namespace mt::debugutil {
  struct IntegratorPathUnit {
    glm::vec3 radiance;
    glm::vec3 accumulatedIrradiance;
    mt::TransportMode transportMode;
    size_t it;
    mt::core::SurfaceInfo surface;
  };
}
