#include <monte-toad/primitive.hpp>

#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/span.hpp>
#include <monte-toad/span.hpp>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow"
  #include <bvh/bvh.hpp>
  #include <bvh/parallel_reinsertion_optimizer.hpp>
  #include <bvh/primitive_intersectors.hpp>
  #include <bvh/single_ray_traverser.hpp>
  #include <bvh/sweep_sah_builder.hpp>
  #include <bvh/utilities.hpp>
#pragma GCC diagnostic pop

namespace {
bvh::Vector3<float> ToBvh(glm::vec3 v) {
  return bvh::Vector3<float>(v.x, v.y, v.z);
}
} // -- end anon namespace

////////////////////////////////////////////////////////////////////////////////
std::optional<mt::BvhIntersection> mt::RayTriangleIntersection(
  bvh::Ray<float> const & ray
, Triangle const & triangle
, CullFace /*cullFace*/
, float /*epsilon*/
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
  BvhIntersection intersection;
  intersection.length = t * invDet;
  intersection.barycentricUv = glm::vec2(u, v) * invDet;
  return intersection;
}

////////////////////////////////////////////////////////////////////////////////
mt::Triangle::Triangle() {}

////////////////////////////////////////////////////////////////////////////////
mt::Triangle::Triangle(
    uint16_t meshIdx_
  , glm::vec3 a_, glm::vec3 b_, glm::vec3 c_
  , glm::vec3 n0_, glm::vec3 n1_, glm::vec3 n2_
  , glm::vec2 uv0_, glm::vec2 uv1_, glm::vec2 uv2_
) :
    meshIdx(meshIdx_)
  , v0(a_), v1(b_), v2(c_)
  , n0(n0_), n1(n1_), n2(n2_)
  , uv0(uv0_), uv1(uv1_), uv2(uv2_)
{}

////////////////////////////////////////////////////////////////////////////////
std::pair<bvh::BoundingBox<float>, bvh::BoundingBox<float>>
mt::Triangle::split(
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
  if (q0) { left.extend(ToBvh(this->v0)); }
  else    { right.extend(ToBvh(this->v0)); }
  if (q1) { left.extend(ToBvh(this->v1)); }
  else    { right.extend(ToBvh(this->v1)); }
  if (q2) { left.extend(ToBvh(this->v2)); }
  else    { right.extend(ToBvh(this->v2)); }
  if (q0 ^ q1) {
      auto m = splitEdge(this->v0, this->v1);
      left.extend(ToBvh(m));
      right.extend(ToBvh(m));
  }
  if (q1 ^ q2) {
      auto m = splitEdge(this->v1, this->v2);
      left.extend(ToBvh(m));
      right.extend(ToBvh(m));
  }
  if (q2 ^ q0) {
      auto m = splitEdge(this->v2, this->v0);
      left.extend(ToBvh(m));
      right.extend(ToBvh(m));
  }
  return std::make_pair(left, right);
}

////////////////////////////////////////////////////////////////////////////////
bvh::BoundingBox<float> mt::Triangle::bounding_box() const {
  auto bbox = bvh::BoundingBox<float>(ToBvh(this->v0));
  bbox.extend(ToBvh(this->v1));
  bbox.extend(ToBvh(this->v2));
  return bbox;
}

////////////////////////////////////////////////////////////////////////////////
bvh::Vector3<float> mt::Triangle::center() const {
  return ToBvh(Center());
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 mt::Triangle::Center() const {
  return (this->v0 + this->v1 + this->v2) * (1.0f/3.0f);
}

////////////////////////////////////////////////////////////////////////////////
float mt::Triangle::area() const {
  auto const edge0 = this->v0 - this->v1;
  auto const edge1 = this->v2 - this->v0;
  return glm::length(glm::cross(edge0, edge1)) * 0.5f;
}

////////////////////////////////////////////////////////////////////////////////
std::optional<mt::BvhIntersection> mt::Triangle::intersect(
  bvh::Ray<float> const & ray
) const {
  return RayTriangleIntersection(ray, *this);
}

////////////////////////////////////////////////////////////////////////////////
mt::Triangle::~Triangle() {}

////////////////////////////////////////////////////////////////////////////////
struct mt::AccelerationStructure::Impl {
  std::vector<Triangle> triangles;
  bvh::Bvh<float> boundingVolume;
  bvh::SingleRayTraverser<decltype(boundingVolume)>
    boundingVolumeTraversal { boundingVolume };
};

mt::AccelerationStructure::AccelerationStructure() {}
mt::AccelerationStructure::~AccelerationStructure() {}
mt::AccelerationStructure::AccelerationStructure(
  mt::AccelerationStructure&& other
) {
  this->impl = std::move(other.impl);
  other.impl = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
void mt::AccelerationStructure::Construct(
  mt::AccelerationStructure & self
, std::vector<Triangle> && trianglesMv
) {
  self.impl = std::make_unique<mt::AccelerationStructure::Impl>();

  self.impl->triangles = std::move(trianglesMv);

  auto & triangles = self.impl->triangles;

  auto [bboxes, centers] =
    bvh::compute_bounding_boxes_and_centers(triangles.data(), triangles.size());

  auto global_bbox =
    bvh::compute_bounding_boxes_union(bboxes.get(), triangles.size());

  auto referenceCount = triangles.size();

  { // -- build SAH
    bvh::SweepSahBuilder<bvh::Bvh<float>> builder(self.impl->boundingVolume);
    builder.build(global_bbox, bboxes.get(), centers.get(), referenceCount);
  }

  { // -- optimizer
    bvh::ParallelReinsertionOptimizer<bvh::Bvh<float>> optimizer(
      self.impl->boundingVolume
    );
    optimizer.optimize();
  }

  /* bvh::optimize_bvh_layout(self->boundingVolume, referenceCount); */

  auto shuffledTriangles =
    bvh::shuffle_primitives(
      triangles.data(),
      self.impl->boundingVolume.primitive_indices.get(),
      referenceCount
    );

  triangles.resize(referenceCount);
  std::memcpy(
    reinterpret_cast<void*>(triangles.data()),
    reinterpret_cast<void*>(shuffledTriangles.get()),
    referenceCount * sizeof(Triangle)
  );
}

span<mt::Triangle> mt::AccelerationStructure::Triangles() {
  return make_span(this->impl->triangles);
}

span<mt::Triangle> mt::AccelerationStructure::Triangles() const {
  return make_span(this->impl->triangles);
}

namespace {
  struct Intersector {
    using Result = mt::Triangle::IntersectionType;

    bvh::Bvh<float> const * boundingVolume;
    span<mt::Triangle const> triangles;
    mt::Triangle const * ignoredTriangle;

    Intersector(
      bvh::Bvh<float> const & boundingVolume_
    , span<mt::Triangle const> triangles_
    , mt::Triangle const * ignoredTriangle_
    )
      : boundingVolume(&boundingVolume_)
      , triangles(triangles_)
      , ignoredTriangle(ignoredTriangle_)
    {}

    std::optional<Result> intersect(size_t idx, bvh::Ray<float> const & ray)
      const
    {
      auto const & triangle = triangles[idx];
      if (&triangle == ignoredTriangle) return std::nullopt;
      auto result = triangle.intersect(ray);
      if (result) result->triangleIdx = idx;
      return result;
    }

    static constexpr bool any_hit = false;
  };
}

////////////////////////////////////////////////////////////////////////////////
std::optional<mt::BvhIntersection>
mt::IntersectClosest(
  AccelerationStructure const & self
, glm::vec3 ori, glm::vec3 dir, Triangle const * ignoredTriangle
) {
  auto const & impl = *self.impl;
  Intersector intersector(
    impl.boundingVolume, make_span(impl.triangles), ignoredTriangle
  );
  bvh::SingleRayTraverser<bvh::Bvh<float>> traversal(impl.boundingVolume);

  // weird hack to make sure none of the components are 0, as BVH doesn't seem
  // to work with it (or maybe how I interact with BVH)
  if (dir.x == 0.0f)
    { dir = glm::normalize(dir + glm::vec3(0.00001f, 0.0f, 0.0f)); }
  if (dir.z == 0.0f)
    { dir = glm::normalize(dir + glm::vec3(0.0, 0.0f, 0.00001f)); }
  if (dir.y == 0.0f)
    { dir = glm::normalize(dir + glm::vec3(0.0, 0.0f, 0.00001f)); }

  return traversal.traverse(bvh::Ray(ToBvh(ori), ToBvh(dir)), intersector);
}
