#include <monte-toad/core/triangle.hpp>

#include <monte-toad/core/enum.hpp>
#include <monte-toad/core/intersection.hpp>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow"
  #include <bvh/bounding_box.hpp>
  #include <bvh/ray.hpp>
  #include <bvh/utilities.hpp>
  #include <bvh/vector.hpp>
#pragma GCC diagnostic pop

namespace
{

bvh::Vector3<float> ToBvh(glm::vec3 v) {
  return bvh::Vector3<float>(v.x, v.y, v.z);
}

std::optional<mt::core::BvhIntersection> RayTriangleIntersection(
  bvh::Ray<float> const & ray
, mt::core::Triangle const & triangle
//, mt::core::CullFace /*cullFace*/
//, float /*epsilon*/
) {
  // Adapted from BVH triangle intersection code, as my personal intersection
  // code doesn't seem to work with the library (neither does replacing these
  // bvh::* operations with glm::* , maybe should investigate as to why later?)
  auto const p0 = ToBvh(triangle.v0);
  auto const e1 = ToBvh(triangle.v0 - triangle.v1);
  auto const e2 = ToBvh(triangle.v2 - triangle.v0);
  auto const n = bvh::cross(e1, e2);

  static constexpr float tolerance = 1e-5f;

  auto const c = p0 - ray.origin;
  auto const r = bvh::cross(ray.direction, c);
  auto const det = bvh::dot(n, ray.direction);
  auto const absDet = std::fabs(det);

  auto const u = bvh::product_sign(bvh::dot(r, e2), det);
  auto const v = bvh::product_sign(bvh::dot(r, e1), det);
  auto const w = absDet - u - v;

  if (u < -tolerance || v < -tolerance || w < -tolerance) return std::nullopt;
  auto t = bvh::product_sign(bvh::dot(n, c), det);
  if (t < absDet * ray.tmin || absDet * ray.tmax <= t) return std::nullopt;

  auto invDet = 1.0f / absDet;
  mt::core::BvhIntersection intersection;
  intersection.length = t * invDet;
  intersection.barycentricUv = glm::vec2(u, v) * invDet;
  return intersection;
}

} // -- end anon namespace

std::pair<bvh::BoundingBox<float>, bvh::BoundingBox<float>>
mt::core::Triangle::split(
  size_t axis
, float position
) const {
  // Adapted from BVH's triangle code again
  auto left  = bvh::BoundingBox<float>::empty();
  auto right = bvh::BoundingBox<float>::empty();
  auto splitEdge =
    [=] (const glm::vec3& a, const glm::vec3& b) {
      auto t = (position - a[axis]) / (b[axis] - a[axis]);
      return a + t * (b - a);
    };
  auto q0 = this->v0[axis] <= position;
  auto q1 = this->v1[axis] <= position;
  auto q2 = this->v2[axis] <= position;
  if (q0) { left.extend(::ToBvh(this->v0)); }
  else    { right.extend(::ToBvh(this->v0)); }
  if (q1) { left.extend(::ToBvh(this->v1)); }
  else    { right.extend(::ToBvh(this->v1)); }
  if (q2) { left.extend(::ToBvh(this->v2)); }
  else    { right.extend(::ToBvh(this->v2)); }
  if (q0 ^ q1) {
      auto m = splitEdge(this->v0, this->v1);
      left.extend(::ToBvh(m));
      right.extend(::ToBvh(m));
  }
  if (q1 ^ q2) {
      auto m = splitEdge(this->v1, this->v2);
      left.extend(::ToBvh(m));
      right.extend(::ToBvh(m));
  }
  if (q2 ^ q0) {
      auto m = splitEdge(this->v2, this->v0);
      left.extend(::ToBvh(m));
      right.extend(::ToBvh(m));
  }
  return std::make_pair(left, right);
}

bvh::BoundingBox<float> mt::core::Triangle::bounding_box() const {
  auto bbox = bvh::BoundingBox<float>(::ToBvh(this->v0));
  bbox.extend(::ToBvh(this->v1));
  bbox.extend(::ToBvh(this->v2));
  return bbox;
}

bvh::Vector3<float> mt::core::Triangle::center() const {
  return ::ToBvh(Center());
}

glm::vec3 mt::core::Triangle::Center() const {
  return (this->v0 + this->v1 + this->v2) * (1.0f/3.0f);
}

float mt::core::Triangle::area() const {
  auto const edge0 = this->v0 - this->v1;
  auto const edge1 = this->v2 - this->v0;
  return glm::length(glm::cross(edge0, edge1)) * 0.5f;
}

std::optional<mt::core::BvhIntersection> mt::core::Triangle::intersect(
  bvh::Ray<float> const & ray
) const {
  return ::RayTriangleIntersection(ray, *this);
}
