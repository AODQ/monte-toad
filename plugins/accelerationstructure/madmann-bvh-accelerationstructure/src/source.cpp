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

#include <vector>
#include <optional>

namespace {

enum struct Builder : uint8_t {
  BinnedSah, SweepSah, SpatialSplit, LocallyOrderedClustering, Linear
};
Builder builder = Builder::LocallyOrderedClustering;
bool optimizeLayout = false;
bool collapseLeaves = true;
bool parallelReinsertion = false;
float preSplitPercent = false;

bvh::Vector3<float> ToBvh(glm::vec3 v) {
  return bvh::Vector3<float>(v.x, v.y, v.z);
}

struct Intersector {
  using Result = mt::core::Triangle::IntersectionType;

  span<mt::core::Triangle const> triangles;
  mt::core::Triangle const * ignoredTriangle;

  Intersector(
    span<mt::core::Triangle const> triangles_
  , mt::core::Triangle const * ignoredTriangle_
  )
    : triangles(triangles_)
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

  bvh::HeuristicPrimitiveSplitter<mt::core::Triangle> splitter;

  // -- presplit
  if (::preSplitPercent > 0.0f) {
    std::tie(referenceCount, bboxes, centers) =
      splitter.split(
          globalBbox
        , self.triangles.data(), self.triangles.size()
        , ::preSplitPercent
      );
  }

  // -- build bvh/accel tree
  switch (::builder) {
    case ::Builder::BinnedSah: {
      auto builder =
        bvh::BinnedSahBuilder<bvh::Bvh<float>, 16ul>(self.boundingVolume);
      builder.build(globalBbox, bboxes.get(), centers.get(), referenceCount);
    } break;
    case ::Builder::SweepSah: {
      auto builder = bvh::SweepSahBuilder<bvh::Bvh<float>>(self.boundingVolume);
      builder.build(globalBbox, bboxes.get(), centers.get(), referenceCount);
    } break;
    case ::Builder::SpatialSplit: {
      auto builder =
        bvh::SpatialSplitBvhBuilder<bvh::Bvh<float>, mt::core::Triangle, 64ul>(
          self.boundingVolume
        );
      builder.build(
        globalBbox, self.triangles.data(), bboxes.get(), centers.get()
      , referenceCount
      );
    } break;
    case ::Builder::LocallyOrderedClustering: {
      auto builder =
        bvh::LocallyOrderedClusteringBuilder<bvh::Bvh<float>, uint32_t>(
          self.boundingVolume
        );
      builder.build(globalBbox, bboxes.get(), centers.get(), referenceCount);
    } break;
    case ::Builder::Linear: {
      auto builder =
        bvh::LinearBvhBuilder<bvh::Bvh<float>, uint32_t>(self.boundingVolume);
      builder.build(globalBbox, bboxes.get(), centers.get(), referenceCount);
    } break;
  }

  // -- presplit repair leaves
  if (::preSplitPercent > 0.0f) {
    splitter.repair_bvh_leaves(self.boundingVolume);
  }

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
  }

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

  auto intersector = ::Intersector(make_span(self.triangles), ignoredTriangle);

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
  ImGui::Begin("acceleration structure");

  auto buildButton = [&](char const * label, ::Builder b) {
    if (ImGui::RadioButton(label, ::builder == b))
      { ::builder = b; }
  };

  buildButton("binned sah", ::Builder::BinnedSah);
  buildButton("sweep sah", ::Builder::SweepSah);
  buildButton("spatial split", ::Builder::SpatialSplit);
  buildButton("local ordered clustering", ::Builder::LocallyOrderedClustering);
  buildButton("linear", ::Builder::Linear);

  ImGui::Separator();

  ImGui::Checkbox("optimize layout", &::optimizeLayout);
  ImGui::Checkbox("collapse leaves", &::collapseLeaves);
  ImGui::Checkbox("parallel reinsertion", &::parallelReinsertion);
  ImGui::SliderFloat("presplit %", &::preSplitPercent, 0.0f, 1.0f);

  ImGui::End();
}

}
