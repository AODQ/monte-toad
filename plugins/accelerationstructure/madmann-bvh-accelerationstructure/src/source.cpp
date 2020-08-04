// madmann bvh acceleration structure

#include <monte-toad/core/any.hpp>
#include <monte-toad/core/intersection.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/span.hpp>
#include <monte-toad/core/triangle.hpp>
#include <mt-plugin/enums.hpp>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wshadow"
  #include <bvh/binned_sah_builder.hpp>
  #include <bvh/bvh.hpp>
  #include <bvh/heuristic_primitive_splitter.hpp>
  #include <bvh/hierarchy_refitter.hpp>
  #include <bvh/leaf_collapser.hpp>
  #include <bvh/linear_bvh_builder.hpp>
  #include <bvh/locally_ordered_clustering_builder.hpp>
  #include <bvh/node_layout_optimizer.hpp>
  #include <bvh/parallel_reinsertion_optimizer.hpp>
  #include <bvh/single_ray_traverser.hpp>
  #include <bvh/spatial_split_bvh_builder.hpp>
  #include <bvh/sweep_sah_builder.hpp>
#pragma GCC diagnostic pop

#include <vector>
#include <optional>

namespace {

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

struct BvhAccelerationStructure {
  std::vector<mt::core::Triangle> triangles;
  bvh::Bvh<float> boundingVolume;
  bvh::SingleRayTraverser<decltype(boundingVolume)>
    boundingVolumeTraversal { boundingVolume };
};

} // -- anon namespace


extern "C" {

char const * PluginLabel() { return "madmann bvh acceleration structure"; }
mt::PluginType PluginType() { return mt::PluginType::AccelerationStructure; }

mt::core::Any Construct(std::vector<mt::core::Triangle> && trianglesMv) {
  ::BvhAccelerationStructure self;
  self.triangles = std::move(trianglesMv);

  auto [bboxes, centers] =
    bvh::compute_bounding_boxes_and_centers(
      self.triangles.data(), self.triangles.size()
    );

  auto const globalBbox =
    bvh::compute_bounding_boxes_union(bboxes.get(), self.triangles.size());

  auto referenceCount = self.triangles.size();

  {
    bvh::LocallyOrderedClusteringBuilder<bvh::Bvh<float>, uint32_t>
      builder { self.boundingVolume };
    builder.build(globalBbox, bboxes.get(), centers.get(), referenceCount);
  }

  {
    bvh::LeafCollapser leafCollapser(self.boundingVolume);
    leafCollapser.collapse();
  }

  auto shuffledTriangles =
    bvh::shuffle_primitives(
      self.triangles.data()
    , self.boundingVolume.primitive_indices.get()
    , referenceCount
    );

  self.triangles.resize(referenceCount);
  std::memcpy(
    reinterpret_cast<void *>(self.triangles.data())
  , reinterpret_cast<void *>(shuffledTriangles.get())
  , referenceCount * sizeof(mt::core::Triangle)
  );

  mt::core::Any any;
  any.data = new ::BvhAccelerationStructure{std::move(self)};
  return any;
}

std::optional<mt::core::BvhIntersection> IntersectClosest(
  mt::core::Any const & selfAny
, glm::vec3 const & ori, glm::vec3 const & dir
, mt::core::Triangle const * const ignoredTriangle
) {
  auto const & self =
    *reinterpret_cast<::BvhAccelerationStructure const *>(selfAny.data);

  auto intersector =
    ::Intersector(
      self.boundingVolume, make_span(self.triangles), ignoredTriangle
    );

  auto traversal =
    bvh::SingleRayTraverser<bvh::Bvh<float>> {self.boundingVolume};

  // weird hack to make sure none of the components are 0, as BVH doesn't seem
  // to work with it (or maybe how I interact with BVH)
  glm::vec3 fixDir = dir;
  if (dir.x == 0.0f)
    { fixDir = glm::normalize(dir + glm::vec3(0.00001f, 0.0f, 0.0f)); }
  if (dir.y == 0.0f)
    { fixDir = glm::normalize(dir + glm::vec3(0.0, 0.00001f, 0.0f)); }
  if (dir.z == 0.0f)
    { fixDir = glm::normalize(dir + glm::vec3(0.0, 0.0f, 0.00001f)); }

  return
    traversal
      .traverse(bvh::Ray(::ToBvh(ori), ::ToBvh(fixDir)), intersector);
}

span<mt::core::Triangle> GetTriangles(mt::core::Any & selfAny) {
  auto & self = *reinterpret_cast<::BvhAccelerationStructure*>(selfAny.data);
  return make_span(self.triangles);
}

void UiUpdate(
  mt::core::Scene & /*scene*/
, mt::core::RenderInfo & /*render*/
, mt::PluginInfo const & /*plugin*/
) {
}

}
