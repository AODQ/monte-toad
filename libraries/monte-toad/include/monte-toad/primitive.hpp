#pragma once

#include "math.hpp"

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow"
  #include <bvh/bvh.hpp>
  #include <bvh/single_ray_traverser.hpp>
#pragma GCC diagnostic pop

#include <memory>

namespace mt {
  struct BvhIntersection {
    BvhIntersection() = default;

    size_t triangleIdx;
    float length;
    glm::vec2 barycentricUv;

    float distance() const { return length; }
  };

  struct Triangle {
    // TODO I wish i could just do Triangle() = default, but it does not construct
    // properly for some reason
    Triangle(
      uint16_t meshIdx_
    , glm::vec3 a_, glm::vec3 b_, glm::vec3 c_
    , glm::vec3 n0_, glm::vec3 n1_, glm::vec3 n2_
    , glm::vec2 uv0_, glm::vec2 uv1_, glm::vec2 uv2_
    );
    Triangle();

    ~Triangle();

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
    auto BoundingBox() const { return bounding_box(); }

    // Required by BVH if splitting is to be performed
    bvh::Vector3<float> center() const;
    glm::vec3 Center() const;

    // Required by BVH if splitting is to be performed
    float area() const;

    std::optional<BvhIntersection> intersect(bvh::Ray<float> const & ray) const;
  };

  enum class CullFace { None, Front, Back };

  std::optional<BvhIntersection> RayTriangleIntersection(
    bvh::Ray<float> const & ray
  , Triangle const & triangle
  , CullFace cullFace = CullFace::None
  , float epsilon = 0.00000001f
  );

  struct AccelerationStructure {
    AccelerationStructure() = default;

    std::vector<Triangle> triangles;
    bvh::Bvh<float> boundingVolume;
    bvh::SingleRayTraverser<decltype(boundingVolume)>
      boundingVolumeTraversal { boundingVolume };

    static std::unique_ptr<AccelerationStructure> Construct(
      std::vector<Triangle> && triangles
    );
  };

  std::optional<BvhIntersection> IntersectClosest(
    AccelerationStructure const & accel
  , glm::vec3 ori, glm::vec3 dir, Triangle const * ignoredTriangle
  );
}
