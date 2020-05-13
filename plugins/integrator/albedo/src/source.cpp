#include <cr/cr.h>

#include <monte-toad/log.hpp>
#include <monte-toad/math.hpp>
#include <monte-toad/scene.hpp>
#include <monte-toad/texture.hpp>

#include <mt-plugin/plugin.hpp>

namespace {
glm::vec3 Dispatch(
  glm::vec2 const & uv, glm::vec2 const & resolution
, mt::Scene const & scene
, mt::RenderInfo const & renderInfo
, mt::PluginInfo const & pluginInfo
) {
  auto [origin, wi] =
    pluginInfo.camera.Dispatch(pluginInfo.random, renderInfo, uv);

  auto results = mt::Raycast(scene, origin, wi, true, nullptr);
  auto const * triangle = std::get<0>(results);
  if (!triangle) { return glm::vec3(0.0f); }

  auto uvCoord =
    BarycentricInterpolation(
      triangle->uv0, triangle->uv1, triangle->uv2
    , std::get<1>(results).barycentricUv
    );

  auto const & material = scene.meshes[triangle->meshIdx].material;
  auto diffuse = material.colorDiffuse;
  if (material.baseColorTextureIdx != static_cast<size_t>(-1)) {
    diffuse =
      SampleBilinear(scene.textures[material.baseColorTextureIdx], uvCoord);
  }

  // do depth just for some visual characteristics
  float distance = std::get<1>(results).length;
  distance /= glm::length(scene.bboxMax - scene.bboxMin);

  return glm::vec3(glm::exp(-distance * 2.0f)) * diffuse;
}
}

CR_EXPORT int cr_main(struct cr_plugin * ctx, enum cr_op operation) {
  // return immediately if an update, this won't do anything
  if (operation == CR_STEP || operation == CR_UNLOAD) { return 0; }
  if (!ctx || !ctx->userdata) { return 0; }

  auto & integrator =
    *reinterpret_cast<mt::PluginInfoIntegrator*>(ctx->userdata);

  switch (operation) {
    case CR_LOAD:
      integrator.realtime = true;
      integrator.useGpu = false;
      integrator.Dispatch = &Dispatch;
      integrator.pluginType = mt::PluginType::Integrator;
    break;
    case CR_UNLOAD: break;
    case CR_STEP: break;
    case CR_CLOSE: break;
  }

  spdlog::info("albedo integrator plugin successfully loaded");

  return 0;
}
