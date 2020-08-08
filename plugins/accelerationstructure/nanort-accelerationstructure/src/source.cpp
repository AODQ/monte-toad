// nanort acceleration structure

#include <monte-toad/core/any.hpp>
#include <monte-toad/core/intersection.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/renderinfo.hpp>
#include <monte-toad/core/scene.hpp>
#include <monte-toad/core/span.hpp>
#include <monte-toad/core/triangle.hpp>
#include <mt-plugin/enums.hpp>

#pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <nanort/nanort.hpp>
#pragma GCC diagnostic pop

#include <imgui/imgui.hpp>

#include <vector>
#include <optional>

namespace {

struct NanoAccelerationStructure {
  mt::core::TriangleMesh triangleMesh;
  std::vector<uint32_t> faces;

  // have to make this a unique ptr bc there is no default or assignment
  // constructor
  std::unique_ptr<nanort::TriangleIntersector<float>> triangleIntersector;

  nanort::BVHAccel<float> accel;
};

} // -- anon namespace


extern "C" {

char const * PluginLabel() { return "madmann bvh acceleration structure"; }
mt::PluginType PluginType() { return mt::PluginType::AccelerationStructure; }

mt::core::Any Construct(mt::core::TriangleMesh && triangleMeshMv) {
  ::NanoAccelerationStructure self;
  self.triangleMesh = std::move(triangleMeshMv);

  // construct faces in a way that will skip normals uv coords and meshIdx
  for (size_t i = 0; i < self.triangleMesh.origins.size(); ++ i) {
    self.faces.emplace_back(i);
  }

  nanort::BVHBuildOptions<float> buildOptions;
  buildOptions.cache_bbox = true;

  auto mesh =
    nanort::TriangleMesh<float>(
      &self.triangleMesh.origins[0].x, self.faces.data(), sizeof(glm::vec3)
    );
  auto sahPred =
    nanort::TriangleSAHPred<float>(
      &self.triangleMesh.origins[0].x, self.faces.data(), sizeof(glm::vec3)
    );

  bool success =
    self.accel.Build(self.faces.size()/3, mesh, sahPred, buildOptions);

  if (!success) {
    spdlog::error("failed to build acceleration tree");
    return mt::core::Any{};
  }

  self.triangleIntersector =
    std::make_unique<nanort::TriangleIntersector<float>>(
      &self.triangleMesh.origins[0].x, self.faces.data(), sizeof(glm::vec3)
    );

  mt::core::Any any;
  any.data = new ::NanoAccelerationStructure{std::move(self)};
  return any;
}

std::optional<mt::core::BvhIntersection> IntersectClosest(
  mt::core::Any const & selfAny
, glm::vec3 const & ori, glm::vec3 const & dir
, size_t const ignoredTriangleIdx
) {
  if (selfAny.data == nullptr) { return std::nullopt; }
  auto const & self =
    *reinterpret_cast<::NanoAccelerationStructure const *>(selfAny.data);

  nanort::Ray<float> ray;
  ray.org[0] = ori.x; ray.org[1] = ori.y; ray.org[2] = ori.z;
  ray.dir[0] = dir.x; ray.dir[1] = dir.y; ray.dir[2] = dir.z;

  nanort::BVHTraceOptions traceOptions;
  traceOptions.skip_prim_id = ignoredTriangleIdx;
  traceOptions.cull_back_face = false;

  nanort::TriangleIntersection<float> isect;
  bool hit =
    self.accel.Traverse(ray, *self.triangleIntersector, &isect, traceOptions);

  if (!hit) { return std::nullopt; }

  mt::core::BvhIntersection intersection;
  intersection.triangleIdx = isect.prim_id;
  intersection.length = isect.t;
  intersection.barycentricUv = glm::vec2(isect.u, isect.v);
  return intersection;
}

mt::core::Triangle GetTriangle(mt::core::Any & selfAny, size_t triangleIdx) {
  auto & self = *reinterpret_cast<::NanoAccelerationStructure*>(selfAny.data);
  return mt::core::Triangle{&self.triangleMesh, triangleIdx};
}

void UiUpdate(
  mt::core::Scene & /*scene*/
, mt::core::RenderInfo & /*render*/
, mt::PluginInfo const & /*plugin*/
) {
  ImGui::Begin("acceleration structure");
  ImGui::End();
}

}
