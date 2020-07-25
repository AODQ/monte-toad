#pragma once

#include <optional>

// -- fwd decl
namespace bvh {
  template <typename T> struct BoundingBox;
  template <typename T, size_t N> struct Vector;
  template <typename T> struct Ray;
}

namespace mt::core { struct BvhIntersection; }

namespace mt::core {
  struct Triangle {
    uint16_t meshIdx;

    using ScalarType = float;
    using IntersectionType = BvhIntersection;

    glm::vec3 v0, v1, v2;
    glm::vec3 n0, n1, n2;
    glm::vec2 uv0, uv1, uv2;

    // Required by BVH if splitting is to be performed
    std::pair<bvh::BoundingBox<float>, bvh::BoundingBox<float>> split(
      size_t axis
    , float position
    ) const;

    // Required by BVH if splitting is to be performed
    bvh::BoundingBox<float> bounding_box() const;

    // Required by BVH if splitting is to be performed
    bvh::Vector<float, 3> center() const;
    glm::vec3 Center() const;

    // Required by BVH if splitting is to be performed
    float area() const;

    std::optional<BvhIntersection> intersect(bvh::Ray<float> const & ray) const;
  };
}