#include "primitive.hpp"

#include "math.hpp"
#include "span.hpp"

#include <bvh/binned_sah_builder.hpp>
#include <bvh/heuristic_primitive_splitter.hpp>
#include <bvh/locally_ordered_clustering_builder.hpp>
#include <bvh/parallel_reinsertion_optimization.hpp>
#include <bvh/sweep_sah_builder.hpp>
#include <bvh/utilities.hpp>

#include <spdlog/spdlog.h>

#include "span.hpp"

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
Triangle::~Triangle() {}

////////////////////////////////////////////////////////////////////////////////
// TODO support min/max distance
std::optional<Intersection> RayTriangleIntersection(
  glm::vec3 ori
, glm::vec3 dir
, Triangle const & triangle
, CullFace cullFace
, float epsilon
) {
  glm::vec3 ab = triangle.v1 - triangle.v0;
  glm::vec3 ac = triangle.v2 - triangle.v0;
  glm::vec3 pvec = glm::cross(dir, ac);
  float determinant = glm::dot(ab, pvec);

  if ((cullFace == CullFace::None  && glm::abs(determinant) <  epsilon)
   || (cullFace == CullFace::Back  &&          determinant  <  epsilon)
   || (cullFace == CullFace::Front &&          determinant  > -epsilon)
  ) {
    return std::nullopt;
  }

  float determinantInv = 1.0f / determinant;

  glm::vec2 uv;

  glm::vec3 tvec = ori - triangle.v0;
  uv.x = determinantInv * glm::dot(tvec, pvec);
  if (uv.x < 0.0f || uv.x > 1.0f) { return std::nullopt; }

  glm::vec3 qvec = glm::cross(tvec, ab);
  uv.y = determinantInv * glm::dot(dir, qvec);
  if (uv.y < 0.0f || uv.x + uv.y > 1.0f) { return std::nullopt; }

  float distance = determinantInv * glm::dot(ac, qvec);
  if (distance <= 0.0f) { return std::nullopt; }

  Intersection intersection;
  intersection.length = distance;
  intersection.barycentricUv = uv;
  return std::make_optional(intersection);
}

////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<AccelerationStructure> AccelerationStructure::Construct(
  std::vector<Triangle> && trianglesMv
, bool optimize
) {
  auto self = std::make_unique<AccelerationStructure>();

  self->triangles = std::move(trianglesMv);

  auto boundingBoxes = std::vector<bvh::BoundingBox<float>>();
  boundingBoxes.resize(self->triangles.size());
  auto centers = std::vector<bvh::Vector3<float>>();
  centers.resize(self->triangles.size());

  // -- calculate bounding boxes
  #pragma omp parallel for
  for (size_t i = 0; i < self->triangles.size(); ++ i) {
    auto & triangle = self->triangles[i];

    auto bbox = bvh::BoundingBox<float>(ToBvh(triangle.v0));
    bbox.extend(ToBvh(triangle.v1));
    bbox.extend(ToBvh(triangle.v2));

    boundingBoxes[i] = bbox;
    centers[i] = ToBvh((triangle.v0 + triangle.v1 + triangle.v2) * (1.0f/3.0f));
  }

  // -- calculate global bounding box (not thread-safe w/o omp reduction)
  auto globalBbox =
    bvh::compute_bounding_boxes_union(
      boundingBoxes.data()
    , self->triangles.size()
    );

  { // -- build tree
    bvh::SweepSahBuilder<decltype(self->boundingVolume)>
      boundingVolumeBuilder { self->boundingVolume };

    boundingVolumeBuilder.build(
      globalBbox, boundingBoxes.data(), centers.data(), self->triangles.size()
    );
  }

  { // -- optimize
    bvh::ParallelReinsertionOptimization<decltype(self->boundingVolume)>
      boundingVolumeOptimizer { self->boundingVolume };
    boundingVolumeOptimizer.optimize();
  }

  // Reorder primitives to BVH's primitive indices, ei before you would have to
  // access the triangles as
  //   accel.triangles[accel.boundingVolume.primitive_indices[i]]
  // now you can just do
  //   accel.triangles[i]
  decltype(self->triangles) primitivesCopy;
  primitivesCopy.resize(self->triangles.size());
  #pragma omp parallel for
  for (size_t i = 0; i < self->triangles.size(); ++ i) {
    primitivesCopy[i] =
      self->triangles[self->boundingVolume.primitive_indices[i]];
  }
  std::swap(self->triangles, primitivesCopy);

  return self;
}

////////////////////////////////////////////////////////////////////////////////
std::optional<Intersection>
IntersectClosest(
  AccelerationStructure const & accel
, glm::vec3 ori, glm::vec3 dir
) {
  struct ClosestIntersector {
    ClosestIntersector() = default;

    span<Triangle const> triangles;

    using Result = Intersection;
    bool const any_hit = false;

    static ClosestIntersector Construct(
      AccelerationStructure const & accel
    ) {
      ClosestIntersector intersector;
      intersector.triangles = make_span(accel.triangles);
      return intersector;
    }

    std::optional<Intersection> operator()(
      size_t idx
    , bvh::Ray<float> const & ray
    ) const {
      auto intersection =
        RayTriangleIntersection(
          ToGlm(ray.origin)
        , ToGlm(ray.direction)
        , triangles[idx]
        );

      if (intersection.has_value()) {
        intersection->triangleIndex = idx;
      }
      return intersection;
    }
  };

  auto intersector = ClosestIntersector::Construct(accel);

  return
    accel
      .boundingVolumeTraversal
      .intersect(bvh::Ray(ToBvh(ori), ToBvh(dir)), intersector);
}
