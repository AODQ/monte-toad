#pragma once

#include <optional>
#include <vector>

// -- fwd decl
namespace bvh {
  template <typename T> struct BoundingBox;
  template <typename T, size_t N> struct Vector;
  template <typename T> struct Ray;
}

namespace mt::core { struct BvhIntersection; }

namespace mt::core {

  struct TriangleMesh {
    std::vector<glm::vec3> origins;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvCoords;
    std::vector<size_t>    meshIndices;
  };


  struct Triangle {
    TriangleMesh const * mesh = nullptr;
    size_t idx = -1ul;

    using ScalarType = float;
    using IntersectionType = BvhIntersection;

    bool Valid() const { return static_cast<bool>(mesh) && idx != -1ul; }

    size_t MeshIdx() const { return this->mesh->meshIndices[this->idx]; }

    /* // Required by BVH if splitting is to be performed */
    /* std::pair<bvh::BoundingBox<float>, bvh::BoundingBox<float>> split( */
    /*   size_t axis */
    /* , float position */
    /* ) const; */

    // Required by BVH
    bvh::BoundingBox<float> bounding_box() const;

    // Required by BVH
    bvh::Vector<float, 3> center() const;
    glm::vec3 Center() const;

    /* // Required by BVH if splitting is to be performed */
    /* float area() const; */

    std::optional<BvhIntersection> intersect(bvh::Ray<float> const & ray) const;
  };
}
