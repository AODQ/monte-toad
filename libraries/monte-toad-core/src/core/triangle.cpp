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

bvh::Vector3<double> ToBvh(glm::vec3 v) {
  return bvh::Vector3<double>(v.x, v.y, v.z);
}

glm::dvec3 ToGlm(bvh::Vector3<double> const & v) {
  return glm::dvec3(v[0], v[1], v[2]);
}

std::optional<mt::core::BvhIntersection> RayTriangleIntersection(
  bvh::Ray<double> const & ray
, mt::core::Triangle const & triangle
//, mt::core::CullFace /*cullFace*/
//, float /*epsilon*/
) {
  auto const ro = ::ToGlm(ray.origin), rd = ::ToGlm(ray.direction);
  glm::dvec3 const
    v1v0 = triangle.v1 - triangle.v0
  , v2v0 = triangle.v2 - triangle.v0
  , rov0 = ro          - triangle.v0
  ;

  // cramer's rule adapted from iq's code shadertoy.com/view/MlGcDz
  glm::dvec3 const
    n = glm::cross(v1v0, v2v0)
  , q = glm::cross(rov0, rd)
  ;

  double const
    d = 1.0/glm::dot(rd, n)
  , u = d*glm::dot(-q, v2v0)
  , v = d*glm::dot( q, v1v0)
  , t = d*glm::dot(-n, rov0)
  ;

  if (u < 0.0 || v < 0.0 || u+v > 1.0 || t < 0.0)
    { return std::nullopt; }

  mt::core::BvhIntersection intersection;
  intersection.length = t;
  intersection.barycentricUv = glm::vec2(u, v);
  return intersection;
}

} // -- end anon namespace

std::pair<bvh::BoundingBox<double>, bvh::BoundingBox<double>>
mt::core::Triangle::split(
  size_t axis
, float positio
) const {
  double position = positio;
  // Adapted from BVH's triangle code again
  auto left  = bvh::BoundingBox<double>::empty();
  auto right = bvh::BoundingBox<double>::empty();
  auto splitEdge =
    [=] (const glm::dvec3& a, const glm::dvec3& b) {
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

bvh::BoundingBox<double> mt::core::Triangle::bounding_box() const {
  auto bbox = bvh::BoundingBox<double>(::ToBvh(this->v0));
  bbox.extend(::ToBvh(this->v1));
  bbox.extend(::ToBvh(this->v2));
  return bbox;
}

bvh::Vector3<double> mt::core::Triangle::center() const {
  return ::ToBvh(Center());
}

glm::vec3 mt::core::Triangle::Center() const {
  return (this->v0 + this->v1 + this->v2) * (1.0/3.0);
}

float mt::core::Triangle::area() const {
  auto const edge0 = this->v0 - this->v1;
  auto const edge1 = this->v2 - this->v0;
  return glm::length(glm::cross(edge0, edge1)) * 0.5;
}

std::optional<mt::core::BvhIntersection> mt::core::Triangle::intersect(
  bvh::Ray<double> const & ray
) const {
  return ::RayTriangleIntersection(ray, *this);
}
