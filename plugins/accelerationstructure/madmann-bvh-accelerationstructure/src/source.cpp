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

#include <imgui/imgui.hpp>

#include <optional>
#include <vector>

namespace {

enum struct Builder : uint8_t {
  BinnedSah, SweepSah, SpatialSplit, LocallyOrderedClustering, Linear
};
Builder builder = Builder::LocallyOrderedClustering;
bool optimizeLayout = false;
bool collapseLeaves = true;
bool parallelReinsertion = false;
/* float preSplitPercent = false; */

bvh::Vector3<float> ToBvh(glm::vec3 v) {
  return bvh::Vector3<float>(v.x, v.y, v.z);
}

struct Intersector {
  using Result = mt::core::Triangle::IntersectionType;

  mt::core::TriangleMesh const * triangleMesh;
  size_t ignoredTriangleIdx;

  Intersector(
    mt::core::TriangleMesh const * triangleMesh_
  , size_t const ignoredTriangleIdx_
  )
    : triangleMesh(triangleMesh_)
    , ignoredTriangleIdx(ignoredTriangleIdx_)
  {}

  std::optional<Result> intersect(size_t idx, bvh::Ray<float> const & ray)
    const
  {
    if (idx == this->ignoredTriangleIdx) return std::nullopt;
    auto triangle = mt::core::Triangle{this->triangleMesh, idx};
    auto result = triangle.intersect(ray);
    if (result) result->triangleIdx = idx;
    return result;
  }

  static constexpr bool any_hit = false;
};

struct BvhAccelerationStructure {
  mt::core::TriangleMesh triangleMesh;
  bvh::Bvh<float> boundingVolume;
};

} // -- anon namespace


extern "C" {

char const * PluginLabel() { return "madmann bvh acceleration structure"; }
mt::PluginType PluginType() { return mt::PluginType::AccelerationStructure; }

mt::core::Any Construct(mt::core::TriangleMesh && triangleMesh) {
  ::BvhAccelerationStructure self;
  self.triangleMesh = std::move(triangleMesh);

  std::vector<bvh::BoundingBox<float>> bboxes;
  std::vector<bvh::Vector3<float>> centers;

  // -- comopute bounding box and center of primitives
  bboxes.reserve(self.triangleMesh.meshIndices.size());
  centers.reserve(self.triangleMesh.meshIndices.size());
  for (size_t i = 0ul; i < self.triangleMesh.meshIndices.size(); ++ i) {
    auto triangle = mt::core::Triangle{&self.triangleMesh, i};
    bboxes.emplace_back(triangle.bounding_box());
    centers.emplace_back(triangle.center());
  }

  auto const globalBbox =
    bvh::compute_bounding_boxes_union(bboxes.data(), bboxes.size());

  auto referenceCount = bboxes.size();

  /* bvh::HeuristicPrimitiveSplitter<mt::core::Triangle> splitter; */

  /* // -- presplit */
  /* if (::preSplitPercent > 0.0f) { */
  /*   std::tie(referenceCount, bboxes, centers) = */
  /*     splitter.split( */
  /*         globalBbox */
  /*       , self.triangles.data(), self.triangles.size() */
  /*       , ::preSplitPercent */
  /*     ); */
  /* } */

  // -- build bvh/accel tree
  switch (::builder) {
    case ::Builder::BinnedSah: {
      auto builder =
        bvh::BinnedSahBuilder<bvh::Bvh<float>, 16ul>(self.boundingVolume);
      builder.build(globalBbox, bboxes.data(), centers.data(), referenceCount);
    } break;
    case ::Builder::SweepSah: {
      auto builder = bvh::SweepSahBuilder<bvh::Bvh<float>>(self.boundingVolume);
      builder.build(globalBbox, bboxes.data(), centers.data(), referenceCount);
    } break;
    case ::Builder::SpatialSplit: {
      /* auto builder = */
      /*   bvh::SpatialSplitBvhBuilder<bvh::Bvh<float>, mt::core::Triangle>( */
      /*     self.boundingVolume */
      /*   ); */
      /* builder.build( */
      /*   globalBbox, self.triangles.data(), bboxes.data(), centers.data() */
      /* , referenceCount */
      /* ); */
    } break;
    case ::Builder::LocallyOrderedClustering: {
      auto builder =
        bvh::LocallyOrderedClusteringBuilder<bvh::Bvh<float>, uint32_t>(
          self.boundingVolume
        );
      builder.build(globalBbox, bboxes.data(), centers.data(), referenceCount);
    } break;
    case ::Builder::Linear: {
      auto builder =
        bvh::LinearBvhBuilder<bvh::Bvh<float>, uint32_t>(self.boundingVolume);
      builder.build(globalBbox, bboxes.data(), centers.data(), referenceCount);
    } break;
  }

  /* // -- presplit repair leaves */
  /* if (::preSplitPercent > 0.0f) { */
  /*   splitter.repair_bvh_leaves(self.boundingVolume); */
  /* } */

  // -- parallel reinsertion optimizer
  if (::parallelReinsertion) {
    auto reinsertionOptimizer =
      bvh::ParallelReinsertionOptimizer<bvh::Bvh<float>>(self.boundingVolume);
    reinsertionOptimizer.optimize();
  }

  // -- optimize node layout
  if (::optimizeLayout) {
    auto layoutOptimizer = bvh::NodeLayoutOptimizer(self.boundingVolume);
    layoutOptimizer.optimize();
  }

  // -- collapse leaves
  if (::collapseLeaves) {
    auto leafCollapser = bvh::LeafCollapser(self.boundingVolume);
    leafCollapser.collapse();
  }

  { // -- preshuffle triangles
    mt::core::TriangleMesh triangleMeshCopy;
    triangleMeshCopy.origins.reserve(referenceCount*3);
    triangleMeshCopy.normals.reserve(referenceCount*3);
    triangleMeshCopy.uvCoords.reserve(referenceCount*3);
    triangleMeshCopy.meshIndices.reserve(referenceCount);
    auto const & indices = self.boundingVolume.primitive_indices.get();
    for (size_t i = 0; i < referenceCount; ++ i) {
      for (size_t k = 0; k < 3; ++ k) {
        triangleMeshCopy.origins.emplace_back(
          self.triangleMesh.origins[indices[i]*3+k]
        );

        triangleMeshCopy.normals.emplace_back(
          self.triangleMesh.normals[indices[i]*3+k]
        );

        triangleMeshCopy.uvCoords.emplace_back(
          self.triangleMesh.uvCoords[indices[i]*3+k]
        );
      }

      self.triangleMesh.meshIndices.emplace_back(
        self.triangleMesh.meshIndices[indices[i]]
      );
    }

    self.triangleMesh = std::move(triangleMeshCopy);
  }

  mt::core::Any any;
  any.data = new ::BvhAccelerationStructure{std::move(self)};
  return any;
}

std::optional<mt::core::BvhIntersection> IntersectClosest(
  mt::core::Any const & selfAny
, glm::vec3 const & ori, glm::vec3 const & dir
, size_t const ignoredTriangleIdx
) {
  auto const & self =
    *reinterpret_cast<::BvhAccelerationStructure const *>(selfAny.data);

  auto intersector = ::Intersector(&self.triangleMesh, ignoredTriangleIdx);

  auto traversal =
    bvh::SingleRayTraverser<
      bvh::Bvh<float>, 128, bvh::RobustNodeIntersector<bvh::Bvh<float>>
    > {self.boundingVolume};

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

mt::core::Triangle GetTriangle(mt::core::Any & selfAny, size_t triangleIdx) {
  auto & self = *reinterpret_cast<::BvhAccelerationStructure*>(selfAny.data);
  return mt::core::Triangle{&self.triangleMesh, triangleIdx};
}

void UiUpdate(
  mt::core::Scene & /*scene*/
, mt::core::RenderInfo & /*render*/
, mt::PluginInfo const & /*plugin*/
) {
  ImGui::Begin("acceleration structure");

  auto buildButton = [&](char const * label, ::Builder b) {
    if (ImGui::RadioButton(label, ::builder == b))
      { ::builder = b; }
  };

  buildButton("binned sah", ::Builder::BinnedSah);
  buildButton("sweep sah", ::Builder::SweepSah);
  /* buildButton("spatial split", ::Builder::SpatialSplit); */
  buildButton("local ordered clustering", ::Builder::LocallyOrderedClustering);
  buildButton("linear", ::Builder::Linear);

  ImGui::Separator();

  ImGui::Checkbox("optimize layout", &::optimizeLayout);
  ImGui::Checkbox("collapse leaves", &::collapseLeaves);
  ImGui::Checkbox("parallel reinsertion", &::parallelReinsertion);
  /* ImGui::SliderFloat("presplit %", &::preSplitPercent, 0.0f, 1.0f); */

  ImGui::End();
}

}
