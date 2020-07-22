#pragma once

#include <monte-toad/enum.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/surfaceinfo.hpp>

namespace mt::debugutil {
  struct IntegratorPathUnit {
    glm::vec3 radiance;
    glm::vec3 accumulatedIrradiance;
    mt::TransportMode transportMode;
    size_t it;
    mt::SurfaceInfo surface;
  };
}
