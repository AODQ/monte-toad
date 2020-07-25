#pragma once

#include <monte-toad/math.hpp>
#include <monte-toad/span.hpp>
#include <monte-toad/util/pimpl.hpp>

#include <optional>
#include <vector>

// -- fwd decl
namespace mt::core { struct Triangle; }
namespace mt::core { struct BvhIntersection; }

namespace mt::core {
  struct AccelerationStructure {
    struct Impl;
    mt::util::pimpl<Impl> impl;

    static void Construct(
      mt::core::AccelerationStructure & self
    , std::vector<mt::core::Triangle> && triangles
    );

    span<mt::core::Triangle> Triangles();
    span<mt::core::Triangle const> Triangles() const;
  };

  std::optional<mt::core::BvhIntersection> IntersectClosest(
    mt::core::AccelerationStructure const & accel
  , glm::vec3 ori, glm::vec3 dir, mt::core::Triangle const * ignoredTriangle
  );
}
