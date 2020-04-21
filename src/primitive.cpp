#include "primitive.hpp"

#include "log.hpp"
#include "math.hpp"
#include "span.hpp"

#include <bvh/binned_sah_builder.hpp>
#include <bvh/heuristic_primitive_splitter.hpp>
#include <bvh/intersectors.hpp>
#include <bvh/locally_ordered_clustering_builder.hpp>
#include <bvh/parallel_reinsertion_optimization.hpp>
#include <bvh/spatial_split_bvh_builder.hpp>
#include <bvh/sweep_sah_builder.hpp>
#include <bvh/utilities.hpp>

#include "span.hpp"

////////////////////////////////////////////////////////////////////////////////
std::optional<Intersection> RayTriangleIntersection(
  bvh::Ray<float> const & ray
, Triangle const & triangle
, CullFace cullFace
, float epsilon
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
  Intersection intersection;
  intersection.length = t * invDet;
  intersection.barycentricUv = glm::vec2(u, v) * invDet;
  return intersection;
}

////////////////////////////////////////////////////////////////////////////////
Triangle::Triangle() {}

////////////////////////////////////////////////////////////////////////////////
Triangle::Triangle(
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
Triangle::split(
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
bvh::BoundingBox<float> Triangle::bounding_box() const {
  auto bbox = bvh::BoundingBox<float>(ToBvh(this->v0));
  bbox.extend(ToBvh(this->v1));
  bbox.extend(ToBvh(this->v2));
  return bbox;
}

////////////////////////////////////////////////////////////////////////////////
bvh::Vector3<float> Triangle::center() const {
  return ToBvh(Center());
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 Triangle::Center() const {
  return (this->v0 + this->v1 + this->v2) * (1.0f/3.0f);
}

////////////////////////////////////////////////////////////////////////////////
float Triangle::area() const {
  auto const edge0 = this->v0 - this->v1;
  auto const edge1 = this->v2 - this->v0;
  return glm::length(glm::cross(edge0, edge1)) * 0.5f;
}

////////////////////////////////////////////////////////////////////////////////
std::optional<Intersection> Triangle::intersect(
  bvh::Ray<float> const & ray
) const {
  return RayTriangleIntersection(ray, *this);
}

////////////////////////////////////////////////////////////////////////////////
Triangle::~Triangle() {}

////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<AccelerationStructure> AccelerationStructure::Construct(
  std::vector<Triangle> && trianglesMv
, bool optimize
) {
  auto self = std::make_unique<AccelerationStructure>();

  self->triangles = std::move(trianglesMv);

  auto & triangles = self->triangles;

  auto [bboxes, centers] =
    bvh::compute_bounding_boxes_and_centers(triangles.data(), triangles.size());

  auto global_bbox =
    bvh::compute_bounding_boxes_union(bboxes.get(), triangles.size());

  auto referenceCount = triangles.size();

  { // -- build SAH
    bvh::SweepSahBuilder<bvh::Bvh<float>> builder(self->boundingVolume);
    builder.build(global_bbox, bboxes.get(), centers.get(), referenceCount);
  }

  { // -- optimizer
    bvh::ParallelReinsertionOptimization<bvh::Bvh<float>> optimization(
      self->boundingVolume
    );
    optimization.optimize();
  }

  bvh::optimize_bvh_layout(self->boundingVolume, referenceCount);

  auto shuffledTriangles =
    bvh::shuffle_primitives(
      triangles.data(),
      self->boundingVolume.primitive_indices.get(),
      referenceCount
    );

  triangles.resize(referenceCount);
  std::memcpy(
    reinterpret_cast<void*>(triangles.data()),
    reinterpret_cast<void*>(shuffledTriangles.get()),
    referenceCount * sizeof(Triangle)
  );

  return self;
}

////////////////////////////////////////////////////////////////////////////////
std::optional<Intersection>
IntersectClosest(
  AccelerationStructure const & self
, glm::vec3 ori, glm::vec3 dir
) {
  bvh::ClosestIntersector<true, bvh::Bvh<float>, Triangle>
    intersector(self.boundingVolume, self.triangles.data());
  bvh::SingleRayTraversal<bvh::Bvh<float>> traversal(self.boundingVolume);

  // weird hack to make sure none of the components are 0, as BVH doesn't seem
  // to work with it (or maybe how I interact with BVH)
  if (dir.x == 0.0f)
    { dir = glm::normalize(dir + glm::vec3(0.00001f, 0.0f, 0.0f)); }
  if (dir.z == 0.0f)
    { dir = glm::normalize(dir + glm::vec3(0.0, 0.0f, 0.00001f)); }
  if (dir.y == 0.0f)
    { dir = glm::normalize(dir + glm::vec3(0.0, 0.0f, 0.00001f)); }

  auto result =
    traversal.intersect(bvh::Ray(ToBvh(ori), ToBvh(dir)), intersector);
  if (!result) { return std::nullopt; }

  // have to copy over triangle idx as that's not done in intersection code
  result->intersection.triangleIdx = result->primitive_index;
  return result->intersection;
}
