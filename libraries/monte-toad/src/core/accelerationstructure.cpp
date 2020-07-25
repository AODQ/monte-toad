#include <monte-toad/core/accelerationstructure.hpp>

#include <monte-toad/core/triangle.hpp>
#include <monte-toad/core/intersection.hpp>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow"
  #include <bvh/bvh.hpp>
  #include <bvh/single_ray_traverser.hpp>
  #include <bvh/sweep_sah_builder.hpp>
  #include <bvh/parallel_reinsertion_optimizer.hpp>
#pragma GCC diagnostic pop

namespace
{

bvh::Vector3<float> ToBvh(glm::vec3 v) {
  return bvh::Vector3<float>(v.x, v.y, v.z);
}

struct Intersector {
  using Result = mt::core::Triangle::IntersectionType;

  bvh::Bvh<float> const * boundingVolume;
  span<mt::core::Triangle const> triangles;
  mt::core::Triangle const * ignoredTriangle;

  Intersector(
    bvh::Bvh<float> const & boundingVolume_
  , span<mt::core::Triangle const> triangles_
  , mt::core::Triangle const * ignoredTriangle_
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

} // -- anon namespace

// -- mt::core::AccelerationStructure::impl implementation ---------------------
struct mt::core::AccelerationStructure::Impl {
  std::vector<Triangle> triangles;
  bvh::Bvh<float> boundingVolume;
  bvh::SingleRayTraverser<decltype(boundingVolume)>
    boundingVolumeTraversal { boundingVolume };
};

// can now specialize the pimpl type
#define PIMPL_SPECIALIZE mt::core::AccelerationStructure::Impl
#include <monte-toad/util/pimpl_impl.inl>

// -- mt::core::AccelerationStructure implementation ---------------------------
void mt::core::AccelerationStructure::Construct(
  mt::core::AccelerationStructure & self
, std::vector<Triangle> && trianglesMv
) {
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

span<mt::core::Triangle> mt::core::AccelerationStructure::Triangles() {
  return make_span(this->impl->triangles);
}

span<mt::core::Triangle const>
mt::core::AccelerationStructure::Triangles() const {
  return make_span(this->impl->triangles);
}

std::optional<mt::core::BvhIntersection>
mt::core::IntersectClosest(
  mt::core::AccelerationStructure const & self
, glm::vec3 ori, glm::vec3 dir, mt::core::Triangle const * ignoredTriangle
) {
  auto const & impl = *self.impl;
  ::Intersector intersector(
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
